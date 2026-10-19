/* ============================================================================
 *  host_sim.c  -  compile the EXACT firmware core on a PC and prove it.
 *
 *  This is the answer to "how do I know the embedded code does what MATLAB
 *  did, before the board even arrives".  Core/ contains no HAL call, no
 *  CMSIS header and no target-specific type, so the same .c files that get
 *  flashed onto the F407 also link into this program.
 *
 *  Build:   make host
 *  Run:     ./build/host_sim            (writes CSVs into out/)
 *           ./build/host_sim --selftest (numerical checks, exit code is the
 *                                        verdict - use it in CI)
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dds.h"
#include "environment.h"
#include "adaptation.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float g_buf[SONAR_MAX_PULSE_SAMPLES];
static float g_freq[SONAR_MAX_PULSE_SAMPLES];

/* ------------------------------------------------------------------ utils */
static void mkoutdir(void) { if (system("mkdir -p out") != 0) perror("mkdir"); }

static uint32_t generate(const sonar_params_t *p, float *dst, float *fq)
{
    dds_t d;
    dds_prepare(&d, p);
    uint32_t n = 0;
    while (d.active && n < SONAR_MAX_PULSE_SAMPLES) {
        if (fq) fq[n] = dds_inst_freq(&d);
        dds_fill_float(&d, &dst[n], 1);
        n++;
    }
    return n;
}

/* naive DFT magnitude (single-sided), matching fft_analysis.m's normalisation */
static void spectrum(const float *x, uint32_t N, uint32_t nbins,
                     float fs, float *fout, float *mout)
{
    for (uint32_t k = 0; k < nbins; ++k) {
        double re = 0, im = 0;
        double w = 2.0 * M_PI * (double)k / (double)N;
        for (uint32_t n = 0; n < N; ++n) {
            re += x[n] * cos(w * n);
            im -= x[n] * sin(w * n);
        }
        double m = sqrt(re * re + im * im) / (double)N;
        if (k > 0) m *= 2.0;
        fout[k] = (float)(fs * (double)k / (double)N);
        mout[k] = (float)m;
    }
}

/* ------------------------------------------------------------ CSV writers */
static void dump_wave(const char *name, const float *x, const float *f,
                      uint32_t N, float fs)
{
    char path[256];
    snprintf(path, sizeof path, "out/%s_wave.csv", name);
    FILE *fp = fopen(path, "w");
    fprintf(fp, "t_us,amplitude,inst_freq_hz\n");
    uint32_t step = (N > 20000U) ? (N / 20000U) : 1U;
    for (uint32_t n = 0; n < N; n += step)
        fprintf(fp, "%.4f,%.6f,%.2f\n",
                1e6f * (float)n / fs, x[n], f ? f[n] : 0.0f);
    fclose(fp);
    printf("    out/%s_wave.csv  (%u samples)\n", name, N);
}

static void dump_spec(const char *name, const float *x, uint32_t N, float fs)
{
    uint32_t nb = 1024;
    uint32_t M  = N;
    if (M > 8192U) M = 8192U;          /* keep the naive DFT tractable */
    static float fo[1024], mo[1024];
    spectrum(x, M, nb > M / 2 ? M / 2 : nb, fs, fo, mo);
    uint32_t n = nb > M / 2 ? M / 2 : nb;
    char path[256];
    snprintf(path, sizeof path, "out/%s_spec.csv", name);
    FILE *fp = fopen(path, "w");
    fprintf(fp, "freq_hz,magnitude\n");
    for (uint32_t k = 0; k < n; ++k) fprintf(fp, "%.1f,%.8f\n", fo[k], mo[k]);
    fclose(fp);
    printf("    out/%s_spec.csv\n", name);
}

static void print_decision(const sonar_env_t *e, const sonar_params_t *p)
{
    printf("  ENV   depth %6.1f m (%-7s)  turbidity %5.1f %% (%-6s)"
           "  %4.1f C  %4.1f PSU\n",
           e->depth_m, env_depth_name(e->depth_state),
           e->turbidity_pct, env_turb_name(e->turbidity_state),
           e->temperature_c, e->salinity_psu);
    printf("  ACOU  c = %.1f m/s   alpha(250k) = %.1f dB/km   "
           "f_max_useful = %.0f kHz\n",
           e->sound_speed_mps, e->absorption_db_km, e->max_useful_f_hz / 1e3f);
    printf("  TX    %-10s  %6.1f - %6.1f kHz  (B = %5.1f kHz)   "
           "T = %5.2f ms   A = %.2f\n",
           adapt_waveform_name(p->waveform),
           p->f_start_hz / 1e3f, p->f_end_hz / 1e3f,
           p->bandwidth_hz / 1e3f, p->pulse_duration_s * 1e3f, p->amplitude);
    printf("  WIN   %-8s (first sidelobe %.1f dB)   mode %s\n",
           adapt_window_name(p->window), p->sidelobe_db,
           adapt_mode_name(p->mode));
    printf("  PERF  range res %.3f m   BT = %.0f  (compression gain %.1f dB)"
           "   max range %.0f m\n",
           p->range_resolution_m, p->time_bandwidth,
           p->compression_gain_db, p->max_range_m);
    printf("  PWR   PRI %.1f ms  duty %.2f %%   avg %.0f mW   "
           "endurance %.1f h\n\n",
           p->pri_s * 1e3f, p->duty * 100.0f,
           p->avg_power_mw, p->endurance_hours);
}

/* ==========================================================================
 *  SELF TEST  -  the part you can point at when someone asks "how do you
 *  know the C is right".
 * ========================================================================*/
static int fails = 0;
static void check(const char *what, int ok, const char *detail)
{
    printf("  [%s] %-52s %s\n", ok ? "PASS" : "FAIL", what, detail);
    if (!ok) fails++;
}

static int selftest(void)
{
    char d[128];
    const float fs = (float)SONAR_FS_HZ;
    const float SC = (float)SONAR_FREQ_SCALE_NUM / (float)SONAR_FREQ_SCALE_DEN;

    printf("\n== 1. DDS sine purity ==================================\n");
    {
        /* Compare the interpolated LUT against libm over a full turn. */
        double worst = 0;
        for (uint32_t i = 0; i < 100000; ++i) {
            uint32_t ph = (uint32_t)((uint64_t)i * 4294967296ULL / 100000ULL);
            double ref = sin(2.0 * M_PI * (double)ph / 4294967296.0);
            double got = (double)dds_sin_q15(ph) / 32767.0;
            double e = fabs(ref - got);
            if (e > worst) worst = e;
        }
        snprintf(d, sizeof d, "max error %.2e  (%.1f dB below full scale)",
                 worst, 20.0 * log10(worst));
        check("interpolated LUT vs libm sin()", worst < 5e-5, d);
    }

    printf("\n== 2. LFM chirp: does it sweep where we told it to ======\n");
    {
        sonar_params_t p; memset(&p, 0, sizeof p);
        p.f_start_hz = 100e3f; p.f_end_hz = 300e3f;
        p.pulse_duration_s = 2e-3f; p.amplitude = 1.0f;
        p.waveform = WF_LFM; p.window = WIN_RECT;
        dds_t dd; dds_prepare(&dd, &p);
        float f_first = dds_inst_freq(&dd);
        uint32_t N = dd.n_total;
        (void)N;
        for (uint32_t n = 0; n < N - 1; ++n) { float t; dds_fill_float(&dd, &t, 1); }
        float f_last = dds_inst_freq(&dd);
        /* The DDS deliberately seeds half a sample ahead (midpoint rule), so
         * the expected endpoints carry a half-sample of chirp rate with them. */
        float k_hz_per_s = (300e3f - 100e3f) * SC * fs / (float)N;
        float half = 0.5f * k_hz_per_s / fs;
        float want0 = 100e3f * SC + half, want1 = 300e3f * SC - half;
        snprintf(d, sizeof d, "start %.1f Hz (want %.1f), end %.1f Hz (want %.1f)",
                 f_first, want0, f_last, want1);
        check("swept endpoints match the commanded band",
              fabsf(f_first - want0) < want0 * 0.002f &&
              fabsf(f_last  - want1) < want1 * 0.002f, d);
    }

    printf("\n== 3. C core vs the MATLAB formula, sample by sample ====\n");
    {
        /* Rebuild the reference in double precision straight out of
         * lfm_generator.m and diff it against the fixed-point engine. */
        sonar_params_t p; memset(&p, 0, sizeof p);
        p.f_start_hz = 100e3f; p.f_end_hz = 300e3f;
        p.pulse_duration_s = 1e-3f; p.amplitude = 0.9f;
        p.waveform = WF_LFM; p.window = WIN_RECT;
        uint32_t N = generate(&p, g_buf, NULL);

        double f0 = 100e3 * SC, f1 = 300e3 * SC, T = (double)N / fs;
        double k = (f1 - f0) / T, worst = 0;
        for (uint32_t n = 0; n < N; ++n) {
            double t = (double)n / fs;
            double ref = 0.9 * cos(2.0 * M_PI * (f0 * t + 0.5 * k * t * t));
            double e = fabs(ref - g_buf[n]);
            if (e > worst) worst = e;
        }
        snprintf(d, sizeof d, "max abs error %.2e over %u samples", worst, N);
        check("LFM matches lfm_generator.m sample for sample", worst < 2e-3, d);
    }

    printf("\n== 4. Geometric sweep vs geometric_generator.m ==========\n");
    {
        sonar_params_t p; memset(&p, 0, sizeof p);
        p.f_start_hz = 100e3f; p.f_end_hz = 250e3f;
        p.pulse_duration_s = 1e-3f; p.amplitude = 1.0f;
        p.waveform = WF_GEOMETRIC; p.window = WIN_RECT;
        dds_t dd; dds_prepare(&dd, &p);
        uint32_t N = dd.n_total;
        float fa = dds_inst_freq(&dd);
        for (uint32_t n = 0; n < N / 2; ++n) { float t; dds_fill_float(&dd, &t, 1); }
        float fm = dds_inst_freq(&dd);
        for (uint32_t n = N / 2; n < N - 1; ++n) { float t; dds_fill_float(&dd, &t, 1); }
        float fb = dds_inst_freq(&dd);
        /* geometric means the midpoint is the GEOMETRIC mean, not the mean */
        float want_mid = sqrtf(100e3f * SC * 250e3f * SC);
        snprintf(d, sizeof d, "f(0)=%.0f  f(T/2)=%.0f (geom. mean %.0f)  f(T)=%.0f",
                 fa, fm, want_mid, fb);
        check("midpoint is the geometric mean of the endpoints",
              fabsf(fm - want_mid) < want_mid * 0.01f, d);
    }

    printf("\n== 5. Barker-13 autocorrelation ========================\n");
    {
        sonar_params_t p; memset(&p, 0, sizeof p);
        p.f_start_hz = 200e3f; p.f_end_hz = 200e3f;
        p.pulse_duration_s = 2e-3f; p.amplitude = 1.0f;
        p.waveform = WF_BARKER13; p.window = WIN_RECT;
        uint32_t N = generate(&p, g_buf, NULL);
        /* Coherently demodulate to baseband one chip at a time, which is what
         * a real receiver's matched filter does, then take the aperiodic
         * autocorrelation of the recovered complex code.  Correlating the
         * PASSBAND signal directly would measure the carrier phase, not the
         * code, and give a meaningless answer. */
        uint32_t chip = N / 13;
        dds_t probe; dds_prepare(&probe, &p);
        double ci[13], cq[13], fc = dds_inst_freq(&probe);
        for (int k = 0; k < 13; ++k) {
            double I = 0, Q = 0;
            for (uint32_t n = k * chip; n < (uint32_t)(k + 1) * chip; ++n) {
                double w = 2.0 * M_PI * fc * (double)n / fs;
                I += g_buf[n] * cos(w);
                Q -= g_buf[n] * sin(w);
            }
            ci[k] = I / chip; cq[k] = Q / chip;
        }
        double peak = 0, side = 0;
        for (int lag = 0; lag < 13; ++lag) {
            double ar = 0, ai = 0;
            for (int k = 0; k + lag < 13; ++k) {
                ar += ci[k] * ci[k + lag] + cq[k] * cq[k + lag];
                ai += cq[k] * ci[k + lag] - ci[k] * cq[k + lag];
            }
            double m = sqrt(ar * ar + ai * ai);
            if (lag == 0) peak = m; else if (m > side) side = m;
        }
        double psl = 20.0 * log10(peak / (side > 1e-12 ? side : 1e-12));
        snprintf(d, sizeof d, "peak/sidelobe = %.1f dB (Barker-13 theory 22.3)", psl);
        check("Barker-13 gives its textbook sidelobe ratio", psl > 20.0, d);
    }

    printf("\n== 6. Barker is a MODULATED pulse, not a baseband square =\n");
    {
        /* barker_generator.m emits +-A rectangular chips with NO carrier.
         * That waveform has all of its energy at DC and cannot be radiated by
         * a band-limited transducer.  Verify the C version puts its energy at
         * the carrier instead. */
        sonar_params_t p; memset(&p, 0, sizeof p);
        p.f_start_hz = 200e3f; p.f_end_hz = 200e3f;
        p.pulse_duration_s = 2e-3f; p.amplitude = 1.0f;
        p.waveform = WF_BARKER13; p.window = WIN_RECT;
        uint32_t N = generate(&p, g_buf, NULL);
        double dc = 0;
        for (uint32_t n = 0; n < N; ++n) dc += g_buf[n];
        dc = fabs(dc) / N;
        /* Energy at the carrier bin.  Read the carrier the engine actually
         * chose rather than the one we asked for: dds_prepare snaps it so the
         * chips land on zero crossings. */
        dds_t probe; dds_prepare(&probe, &p);
        double re = 0, im = 0, fc = dds_inst_freq(&probe);
        for (uint32_t n = 0; n < N; ++n) {
            double w = 2.0 * M_PI * fc * n / fs;
            re += g_buf[n] * cos(w); im -= g_buf[n] * sin(w);
        }
        double carrier = sqrt(re * re + im * im) / N;
        snprintf(d, sizeof d, "DC %.5f vs carrier %.5f  (ratio %.0f:1)",
                 dc, carrier, carrier / (dc > 1e-9 ? dc : 1e-9));
        check("energy sits at the carrier, not at DC", carrier > 50.0 * dc, d);
    }

    printf("\n== 7. Windows ==========================================\n");
    {
        sonar_params_t p; memset(&p, 0, sizeof p);
        p.f_start_hz = 200e3f; p.f_end_hz = 200e3f;
        p.pulse_duration_s = 1e-3f; p.amplitude = 1.0f;
        p.waveform = WF_CW;
        int ok = 1; double worst = 0;
        const window_t ws[] = { WIN_HAMMING, WIN_HANN, WIN_BLACKMAN };
        const double a0[] = { 0.54, 0.50, 0.42 };
        const double a1[] = { 0.46, 0.50, 0.50 };
        const double a2[] = { 0.00, 0.00, 0.08 };
        for (int i = 0; i < 3; ++i) {
            p.window = ws[i];
            dds_t dd; dds_prepare(&dd, &p);
            for (uint32_t n = 0; n < dd.n_total; n += 37) {
                double x = 2.0 * M_PI * (double)n / (double)dd.n_total;
                double ref = a0[i] - a1[i] * cos(x) + a2[i] * cos(2 * x);
                double got = (double)dds_window_q15(&dd, n) / 32767.0;
                double e = fabs(ref - got);
                if (e > worst) worst = e;
                if (e > 2e-3) ok = 0;
            }
        }
        snprintf(d, sizeof d, "max error vs windowing.m definitions %.2e", worst);
        check("Hamming / Hann / Blackman match MATLAB", ok, d);
    }
    {
        sonar_params_t p; memset(&p, 0, sizeof p);
        p.f_start_hz = 200e3f; p.f_end_hz = 200e3f;
        p.pulse_duration_s = 1e-3f; p.amplitude = 1.0f;
        p.waveform = WF_CW; p.window = WIN_TUKEY; p.tukey_alpha = 0.25f;
        dds_t dd; dds_prepare(&dd, &p);
        int ok = dds_window_q15(&dd, 0) < 200
              && dds_window_q15(&dd, dd.n_total / 2) > 32000
              && dds_window_q15(&dd, dd.n_total - 1) < 200;
        snprintf(d, sizeof d, "edges %d/%d, centre %d (Q15 full scale 32767)",
                 dds_window_q15(&dd, 0), dds_window_q15(&dd, dd.n_total - 1),
                 dds_window_q15(&dd, dd.n_total / 2));
        check("Tukey tapers the edges and leaves the middle alone", ok, d);
    }

    printf("\n== 8. Adaptation port == MATLAB decision table =========\n");
    {
        /* Every (depth, turbidity) cell of adaptation.m + waveform_selector.m,
         * hand-transcribed from the .m files as the expected answer. */
        struct { float d, t; float f0, f1, T, A; const char *wf; } tc[] = {
            /* SHALLOW + LOW  -> 300-500k, 2ms*0.8, 0.6, BARKER */
            { 20.f, 10.f, 300e3f, 500e3f, 2e-3f * 0.8f, 0.60f, "BARKER13" },
            /* SHALLOW + MED  */
            { 20.f, 50.f, 200e3f, 400e3f, 3e-3f * 0.8f, 0.70f, "LFM" },
            /* SHALLOW + HIGH */
            { 20.f, 90.f, 100e3f, 250e3f, 5e-3f * 0.8f, 0.90f, "LFM" },
            /* MEDIUM  + LOW  */
            { 80.f, 10.f, 300e3f, 500e3f, 2e-3f,        0.60f, "LFM" },
            /* MEDIUM  + MED  */
            { 80.f, 50.f, 200e3f, 400e3f, 3e-3f,        0.70f, "LFM" },
            /* MEDIUM  + HIGH */
            { 80.f, 90.f, 100e3f, 250e3f, 5e-3f,        0.90f, "LFM" },
            /* DEEP    + LOW  */
            { 150.f, 10.f, 300e3f, 500e3f, 2e-3f * 1.5f, 0.70f, "LFM" },
            /* DEEP    + MED  */
            { 150.f, 50.f, 200e3f, 400e3f, 3e-3f * 1.5f, 0.80f, "LFM" },
            /* DEEP    + HIGH -> the phase1_final.m demo case              */
            { 150.f, 90.f, 100e3f, 250e3f, 5e-3f * 1.5f, 1.00f, "GEOMETRIC" },
        };
        int ok = 1; int worstcase = -1;
        for (int i = 0; i < 9; ++i) {
            env_init();
            sonar_env_t e; sonar_params_t p;
            /* drive the classifier straight to the cell we want to test */
            env_update(&e, tc[i].d, tc[i].t, 20.f, 35.f);
            env_update(&e, tc[i].d, tc[i].t, 20.f, 35.f);
            adapt_compute_matlab_equivalent(&e, &p);
            int cell_ok = fabsf(p.f_start_hz - tc[i].f0) < 1.f
                       && fabsf(p.f_end_hz   - tc[i].f1) < 1.f
                       && fabsf(p.pulse_duration_s - tc[i].T) < 1e-9f
                       && fabsf(p.amplitude - tc[i].A) < 1e-4f
                       && strcmp(adapt_waveform_name(p.waveform), tc[i].wf) == 0;
            if (!cell_ok) { ok = 0; if (worstcase < 0) worstcase = i; }
        }
        if (ok) snprintf(d, sizeof d, "all 9 depth x turbidity cells agree");
        else    snprintf(d, sizeof d, "first mismatch at case %d", worstcase);
        check("C port reproduces adaptation.m exactly", ok, d);
    }

    printf("\n== 9. Real-time safety invariants ======================\n");
    {
        env_init(); adapt_init();
        int ok = 1; int clamp_hits = 0;
        float prev_amp = 0.f;
        for (int i = 0; i < 400; ++i) {
            /* sweep the pots around violently, including out of range */
            float dep = 150.f + 200.f * sinf(i * 0.11f);
            float tur =  50.f + 120.f * sinf(i * 0.07f);
            sonar_env_t e; sonar_params_t p;
            env_update(&e, dep, tur, 4.f + 25.f * sinf(i * 0.03f), 35.f);
            adapt_compute(&e, &p);
            if (p.f_start_hz < TX_F_MIN_HZ - 1.f ||
                p.f_end_hz   > TX_F_MAX_HZ + 1.f) { ok = 0; }
            if (p.pulse_duration_s < TX_PULSE_MIN_S - 1e-9f ||
                p.pulse_duration_s > TX_PULSE_MAX_S + 1e-9f) ok = 0;
            if (p.duty > TX_DUTY_MAX + 1e-4f) ok = 0;
            if (fabsf(p.amplitude - prev_amp) > TX_AMPL_SLEW_PER_PING + 1e-4f)
                ok = 0;
            prev_amp = p.amplitude;
            if (p.f_end_hz >= TX_F_MAX_HZ - 1.f) clamp_hits++;
        }
        snprintf(d, sizeof d,
                 "400 adversarial pings, band/pulse/duty/slew all held (%d clamped)",
                 clamp_hits);
        check("no sensor input can violate the transducer envelope", ok, d);
    }

    printf("\n== 10. Anti-chatter dwell ==============================\n");
    {
        env_init(); adapt_init();
        sonar_env_t e; sonar_params_t p;
        for (int i = 0; i < 10; ++i) { env_update(&e, 20.f, 10.f, 20.f, 35.f);
                                       adapt_compute(&e, &p); }
        waveform_t settled = p.waveform;
        int changes = 0; waveform_t last = settled;
        for (int i = 0; i < 60; ++i) {
            /* park the turbidity pot exactly on the 30 % boundary and jitter */
            float t = 30.0f + ((i % 2) ? 1.5f : -1.5f);
            env_update(&e, 20.f, t, 20.f, 35.f);
            adapt_compute(&e, &p);
            if (p.waveform != last) { changes++; last = p.waveform; }
        }
        snprintf(d, sizeof d,
                 "waveform changed %d times across 60 boundary-jitter pings", changes);
        check("hysteresis + dwell stop threshold chatter", changes == 0, d);
    }

    printf("\n========================================================\n");
    printf("  %s   (%d failure%s)\n", fails ? "SELFTEST FAILED" : "ALL CHECKS PASSED",
           fails, fails == 1 ? "" : "s");
    printf("========================================================\n\n");
    return fails ? 1 : 0;
}

/* ==========================================================================
 *  MAIN
 * ========================================================================*/
int main(int argc, char **argv)
{
    dds_init();
    env_init();
    adapt_init();

    if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
        return selftest();

    mkoutdir();
    const float fs = (float)SONAR_FS_HZ;

    printf("\n");
    printf("=====================================================================\n");
    printf("  ADAPTIVE SONAR TRANSMITTER  -  firmware core, host simulation\n");
    printf("  fs = %.1f MHz    frequency scaling = %u/%u\n",
           fs / 1e6f, SONAR_FREQ_SCALE_NUM, SONAR_FREQ_SCALE_DEN);
    printf("=====================================================================\n\n");

    struct { const char *name; float d, t, temp, sal; mission_mode_t m; } scen[] = {
        { "clear_shallow_reef",  20.f, 10.f, 27.f, 35.f, MODE_CLASSIFY },
        { "coastal_medium",      80.f, 50.f, 20.f, 34.f, MODE_SEARCH   },
        { "muddy_estuary_deep", 150.f, 90.f, 14.f, 22.f, MODE_SEARCH   },
        { "transit_leg",        100.f, 40.f, 18.f, 35.f, MODE_TRANSIT  },
        { "battery_critical",   150.f, 90.f, 14.f, 22.f, MODE_SEARCH   },
    };

    FILE *sum = fopen("out/decisions.csv", "w");
    fprintf(sum, "scenario,depth_m,turbidity_pct,temp_c,sal_psu,c_mps,"
                 "alpha_db_km,waveform,window,mode,f_start_hz,f_end_hz,"
                 "bandwidth_hz,pulse_ms,amplitude,range_res_m,BT,"
                 "gain_db,pri_ms,duty_pct,avg_power_mw,endurance_h\n");

    for (unsigned i = 0; i < sizeof scen / sizeof scen[0]; ++i) {
        env_init(); adapt_init();
        adapt_set_mode(scen[i].m);
        adapt_set_soc(strcmp(scen[i].name, "battery_critical") == 0 ? 0.08f : 1.0f);

        sonar_env_t e; sonar_params_t p;
        /* run the loop a few times so the dwell timer commits, exactly as it
         * would on the board */
        for (int k = 0; k < 8; ++k) {
            env_update(&e, scen[i].d, scen[i].t, scen[i].temp, scen[i].sal);
            adapt_compute(&e, &p);
        }

        printf("--- %s ------------------------------------------\n", scen[i].name);
        print_decision(&e, &p);

        fprintf(sum, "%s,%.1f,%.1f,%.1f,%.1f,%.2f,%.3f,%s,%s,%s,"
                     "%.1f,%.1f,%.1f,%.4f,%.4f,%.4f,%.1f,%.2f,%.3f,%.3f,%.1f,%.2f\n",
                scen[i].name, e.depth_m, e.turbidity_pct, e.temperature_c,
                e.salinity_psu, e.sound_speed_mps, e.absorption_db_km,
                adapt_waveform_name(p.waveform), adapt_window_name(p.window),
                adapt_mode_name(p.mode), p.f_start_hz, p.f_end_hz,
                p.bandwidth_hz, p.pulse_duration_s * 1e3f, p.amplitude,
                p.range_resolution_m, p.time_bandwidth, p.compression_gain_db,
                p.pri_s * 1e3f, p.duty * 100.f, p.avg_power_mw, p.endurance_hours);

        uint32_t N = generate(&p, g_buf, g_freq);
        dump_wave(scen[i].name, g_buf, g_freq, N, fs);
        dump_spec(scen[i].name, g_buf, N, fs);
        printf("\n");
    }
    fclose(sum);
    printf("  out/decisions.csv written.\n\n");

    /* one file per waveform type, same environment, for the comparison plot */
    printf("--- waveform gallery (identical band and duration) ------------\n");
    const waveform_t all[] = { WF_LFM, WF_GEOMETRIC, WF_HYPERBOLIC,
                               WF_BARKER13, WF_STEPPED, WF_CW };
    for (unsigned i = 0; i < 6; ++i) {
        sonar_params_t p; memset(&p, 0, sizeof p);
        p.f_start_hz = 100e3f; p.f_end_hz = 400e3f;
        p.pulse_duration_s = 2e-3f; p.amplitude = 0.9f;
        p.waveform = all[i]; p.window = WIN_TUKEY; p.tukey_alpha = 0.2f;
        uint32_t N = generate(&p, g_buf, g_freq);
        char nm[64]; snprintf(nm, sizeof nm, "wf_%s", adapt_waveform_name(all[i]));
        dump_wave(nm, g_buf, g_freq, N, fs);
        dump_spec(nm, g_buf, N, fs);
    }

    /* window comparison on one waveform */
    printf("\n--- window gallery -------------------------------------------\n");
    const window_t wins[] = { WIN_RECT, WIN_HAMMING, WIN_HANN,
                              WIN_BLACKMAN, WIN_TUKEY };
    for (unsigned i = 0; i < 5; ++i) {
        sonar_params_t p; memset(&p, 0, sizeof p);
        p.f_start_hz = 100e3f; p.f_end_hz = 400e3f;
        p.pulse_duration_s = 2e-3f; p.amplitude = 0.9f;
        p.waveform = WF_LFM; p.window = wins[i]; p.tukey_alpha = 0.25f;
        uint32_t N = generate(&p, g_buf, g_freq);
        char nm[64]; snprintf(nm, sizeof nm, "win_%s", adapt_window_name(wins[i]));
        dump_wave(nm, g_buf, g_freq, N, fs);
        dump_spec(nm, g_buf, N, fs);
    }

    printf("\nDone.  Now run:  python3 Tools/plot_results.py\n\n");
    return 0;
}
