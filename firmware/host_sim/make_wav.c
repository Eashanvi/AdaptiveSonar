/* ============================================================================
 *  make_wav.c  -  write a .wav file containing exactly what the board emits.
 *
 *  WHY THIS EXISTS
 *  ---------------
 *  The Audacity half of this project can be set up, tested and rehearsed
 *  without any hardware at all, on a different laptop, by a different person,
 *  at the same time as someone else is fighting with STM32CubeIDE.  This tool
 *  produces the reference recording that makes that possible.
 *
 *  It is NOT a mock-up.  It calls the same dds.c and adaptation.c that are
 *  compiled onto the microcontroller, with the same parameters, so the
 *  spectrogram of this file is the spectrogram you should get off the board.
 *  If the board's recording looks like this file, the hardware is correct.
 *  If it does not, the difference is the bug, and you have something to
 *  compare against instead of a vague memory of what it ought to look like.
 *
 *  The core generates at SONAR_FS_HZ (200 kHz).  A laptop records at 48 kHz.
 *  Since the generated signal contains nothing above 15.6 kHz, dropping to
 *  48 kHz by linear interpolation loses nothing audible.
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dds.h"
#include "environment.h"
#include "adaptation.h"

#define OUT_FS      48000
#define MAXSEC      40
#define MAXOUT      (OUT_FS * MAXSEC)

static float g_hi[SONAR_FS_HZ / 2];     /* scratch at the native rate */
static float g_out[MAXOUT];
static uint32_t g_out_n;

/* Append `n` samples of g_hi (which are at SONAR_FS_HZ) to the 48 kHz output. */
static void push_resampled(uint32_t n)
{
    const double ratio = (double)SONAR_FS_HZ / (double)OUT_FS;
    uint32_t m = (uint32_t)((double)n / ratio);
    for (uint32_t i = 0; i < m && g_out_n < MAXOUT; ++i) {
        double x  = (double)i * ratio;
        uint32_t k = (uint32_t)x;
        double  fr = x - (double)k;
        float a = g_hi[k];
        float b = (k + 1U < n) ? g_hi[k + 1U] : 0.0f;
        g_out[g_out_n++] = (float)((1.0 - fr) * a + fr * b);
    }
}

static void push_silence(double seconds)
{
    uint32_t n = (uint32_t)(seconds * OUT_FS);
    for (uint32_t i = 0; i < n && g_out_n < MAXOUT; ++i) g_out[g_out_n++] = 0.0f;
}

/* Generate one pulse from a parameter set and append it. */
static void push_pulse(const sonar_params_t *p)
{
    dds_t d;
    dds_prepare(&d, p);
    uint32_t n = 0;
    uint32_t cap = (uint32_t)(sizeof g_hi / sizeof g_hi[0]);
    while (d.active && n < cap) {
        n += dds_fill_float(&d, &g_hi[n], (cap - n > 512U) ? 512U : (cap - n));
    }
    push_resampled(n);
}

static void write_wav(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    uint32_t data_bytes = g_out_n * 2U;
    uint32_t riff = 36U + data_bytes;
    uint32_t byte_rate = OUT_FS * 2U;
    uint16_t v;
    uint32_t w;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); w = 16U;      fwrite(&w, 4, 1, f);
    v = 1U;                                 fwrite(&v, 2, 1, f);  /* PCM      */
    v = 1U;                                 fwrite(&v, 2, 1, f);  /* mono     */
    w = OUT_FS;                             fwrite(&w, 4, 1, f);
    w = byte_rate;                          fwrite(&w, 4, 1, f);
    v = 2U;                                 fwrite(&v, 2, 1, f);  /* align    */
    v = 16U;                                fwrite(&v, 2, 1, f);  /* bits     */
    fwrite("data", 1, 4, f); fwrite(&data_bytes, 4, 1, f);
    for (uint32_t i = 0; i < g_out_n; ++i) {
        float s = g_out[i] * 0.70f;                 /* leave headroom        */
        if (s >  1.0f) s =  1.0f;
        if (s < -1.0f) s = -1.0f;
        int16_t q = (int16_t)lrintf(s * 32767.0f);
        fwrite(&q, 2, 1, f);
    }
    fclose(f);
    printf("  %-34s %6.2f s\n", path, (double)g_out_n / OUT_FS);
}

/* A short marker tone so whoever is recording can set the input level before
 * the interesting part starts. */
static void push_marker(double seconds, float hz)
{
    sonar_params_t p; memset(&p, 0, sizeof p);
    p.waveform = WF_CW; p.window = WIN_TUKEY; p.tukey_alpha = 0.05f;
    p.f_start_hz = hz * (float)SONAR_FREQ_SCALE_DEN / (float)SONAR_FREQ_SCALE_NUM;
    p.f_end_hz   = p.f_start_hz;
    p.pulse_duration_s = (float)seconds;
    p.amplitude = 0.8f;
    push_pulse(&p);
}

static void run_segment(double seconds, double pri,
                        float depth, float turb0, float turb1,
                        float temp, float sal, window_t forced, bool force)
{
    sonar_env_t e; sonar_params_t p;
    uint32_t pings = (uint32_t)(seconds / pri);
    for (uint32_t i = 0; i < pings; ++i) {
        float t = (pings > 1U) ? (float)i / (float)(pings - 1U) : 0.0f;
        float turb = turb0 + (turb1 - turb0) * t;
        env_update(&e, depth, turb, temp, sal);
        adapt_compute(&e, &p);
        if (force) { p.window = forced; }
        uint32_t before = g_out_n;
        push_pulse(&p);
        double used = (double)(g_out_n - before) / OUT_FS;
        if (pri > used) push_silence(pri - used);
    }
}

int main(void)
{
    dds_init(); env_init(); adapt_init();
    printf("\nReference recordings (what the board should sound like)\n");
    printf("  generated by the SAME core that runs on the STM32\n\n");

    /* ---- file 1: the adaptation demo, matching Experiment 1 ------------- */
    g_out_n = 0;
    push_marker(1.0, 5000.0f);          /* 5 kHz level-setting tone          */
    push_silence(0.5);
    env_init(); adapt_init();
    run_segment(6.0, 0.30, 30.0f,   5.0f,   5.0f, 24.0f, 34.0f, WIN_TUKEY, false);
    run_segment(8.0, 0.30, 90.0f,   5.0f,  95.0f, 22.0f, 33.0f, WIN_TUKEY, false);
    run_segment(6.0, 0.30, 220.0f, 95.0f,  95.0f, 12.0f, 35.0f, WIN_TUKEY, false);
    write_wav("out/reference_adaptive.wav");

    /* ---- file 2: window on vs window off, matching Experiment 2 --------- */
    g_out_n = 0;
    push_marker(1.0, 5000.0f);
    push_silence(0.5);
    env_init(); adapt_init();
    run_segment(6.0, 0.30, 60.0f, 40.0f, 40.0f, 20.0f, 34.0f, WIN_TUKEY, true);
    push_silence(1.0);
    run_segment(6.0, 0.30, 60.0f, 40.0f, 40.0f, 20.0f, 34.0f, WIN_RECT,  true);
    write_wav("out/reference_window_ab.wav");

    printf("\n  First 1.0 s of each file is a 5 kHz tone - use it to set the\n");
    printf("  recording level before the chirps start.\n\n");
    return 0;
}
