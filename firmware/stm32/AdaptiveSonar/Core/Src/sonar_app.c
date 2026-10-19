/* ============================================================================
 *  sonar_app.c  -  the application.  Call sonar_app_run() from main().
 *
 *  THE WHOLE TRANSMITTER, IN ONE LOOP
 *  ----------------------------------
 *      read pots  ->  classify water  ->  decide parameters
 *                                             |
 *                                             v
 *                                   arm the DDS, kick the DMA
 *                                             |
 *                                        sleep (WFI)
 *                                             |
 *                                      report over UART
 *
 *  Note what is NOT here: there is no sample loop.  The CPU never touches an
 *  individual sample outside the two DMA interrupts, and it spends the
 *  overwhelming majority of every pulse repetition interval asleep.  That is
 *  the architectural claim this project is making, and it is visible on a
 *  current probe in about four seconds.
 *
 *  BRING-UP BUILD (v3, 25 Sept 2026) - what changed in this file:
 *   - The board now BOOTS IN TONE MODE: a clean 5 kHz sine burst, 200 ms on,
 *     100 ms off, forever.  That is the first thing to find on the scope.
 *   - One press of the blue USER button switches to the real adaptive sonar
 *     (NORMAL).  The button cycle is now:
 *         TONE -> NORMAL -> FAST-REPEAT -> NO-WINDOW -> TONE
 *   - If a pulse ever fails to finish, the loop stops it and carries on
 *     (counted in g_dac_stuck) instead of going silent forever.
 * ==========================================================================*/
#include "stm32f4xx_hal.h"
#include "sonar_hw.h"
#include "board.h"
#include "dds.h"
#include "environment.h"
#include "adaptation.h"
#include <string.h>

static sonar_env_t    g_env;
static sonar_params_t g_par;
static uint32_t       g_cycles_per_ping;
static uint32_t       g_cycles_in_isr;

/* --------------------------------------------------------------------------
 *  g_live  -  the dashboard for people who do not own a serial adapter.
 *
 *  In STM32CubeIDE, while the program is running under the debugger, open
 *  Window > Show View > Live Expressions and type  g_live  into it.  Every
 *  field below updates on screen a few times a second WITHOUT stopping the
 *  program.  Turn a knob, watch the numbers move.  That is a complete,
 *  demonstrable proof that the sensing-to-decision path works, and it needs
 *  no extra cable, no adapter and no driver.
 *
 *  Keep this struct flat and made of plain floats and ints.  Live Expressions
 *  will happily show a nested struct but it is much easier to read when a
 *  judge is standing behind you if everything is one level deep.
 * ------------------------------------------------------------------------*/
typedef struct {
    float    depth_m;             /* knob 1, in metres                       */
    float    turbidity_pct;       /* knob 2, in percent                      */
    float    temperature_c;       /* knob 3, in degrees C                    */
    float    salinity_psu;        /* knob 4, in PSU                          */
    float    sound_speed_mps;     /* derived - Mackenzie 1981                */
    float    absorption_db_km;    /* derived - Francois & Garrison 1982      */
    float    f_start_hz;          /* DECISION: where the chirp starts        */
    float    f_end_hz;            /* DECISION: where the chirp ends          */
    float    bandwidth_hz;        /* DECISION: f_end - f_start               */
    float    pulse_ms;            /* DECISION: how long we transmit          */
    float    amplitude;           /* DECISION: 0..1 of full scale            */
    float    range_resolution_m;  /* consequence: how fine the picture is    */
    float    duty_pct;            /* consequence: how hard the battery works */
    float    avg_power_mw;        /* consequence: payload power              */
    uint32_t ping_index;          /* counts up - proves it is still alive    */
    uint32_t cycles_per_decision; /* measured, not estimated                 */
    char     waveform[12];        /* "LFM", "BARKER13", ...                  */
    char     window[12];          /* "TUKEY", "HAMMING", ...                 */
    char     water[16];           /* "SHALLOW/MUDDY", ...                    */
    char     bench[14];           /* NORMAL / TONE / FAST-REPEAT / NO-WINDOW */
} live_t;

volatile live_t g_live;           /* volatile so the optimiser cannot hide it */

/* --------------------------------------------------------------------------
 *  Bench switches.
 *
 *  These are deliberately NOT static and deliberately volatile.  Two reasons.
 *
 *  First, a serial console needs a USB-to-serial adapter, and if you are
 *  programming through an external ST-LINK V2 you very likely do not have
 *  one.  So there has to be another way in.
 *
 *  Second, STM32CubeIDE's Live Expressions view can WRITE a variable as well
 *  as read it, while the program keeps running.  Type the name in, click the
 *  value, type 1, press Enter.  A non-static volatile global is exactly what
 *  that needs: static would hide it from the expression evaluator, and
 *  without volatile the optimiser is entitled to cache the value in a
 *  register and never notice that the debugger changed it.
 *
 *  The blue USER button does the same job with no laptop involved at all.
 * ------------------------------------------------------------------------*/
volatile bool g_fast_repeat = false;  /* ping back-to-back, for recording   */
volatile bool g_window_off  = false;  /* bypass the window, to show why     */
volatile bool g_tone_mode   = true;   /* plain tone, for the first-light test - ON at boot */
volatile uint32_t g_dac_stuck = 0U;   /* pulses that had to be force-stopped */

/* --------------------------------------------------------------------------
 *  The blue button, cycling through the four bench states.
 *
 *  at boot -> steady tone          "is ANY signal coming out?"
 *  press 1 -> normal adaptive sonar "turn the knobs, watch it adapt"
 *  press 2 -> chirps, fast repeat  "record this for the spectrogram"
 *  press 3 -> fast repeat, no window "this is what the window was for"
 *  press 4 -> back to the steady tone
 *
 *  Debounced by time, not by a delay loop: a real button bounces for a few
 *  milliseconds and we must not block the main loop waiting for it to stop.
 * ------------------------------------------------------------------------*/
static const char *bench_name(void)
{
    if (g_tone_mode)   return "TONE";
    if (g_window_off)  return "NO-WINDOW";
    if (g_fast_repeat) return "FAST-REPEAT";
    return "NORMAL";
}

/* --------------------------------------------------------------------------
 *  v3.1 (26 Sept 2026) - THE BUTTON FIX.
 *
 *  Old behaviour: the button was only looked at once per trip round the main
 *  loop.  The loop spends most of its time asleep (0.3 s in TONE, up to 2 s in
 *  NORMAL), so a normal quick press usually happened while nobody was
 *  looking and was simply missed.  That is why it took "3 or 4 presses, never
 *  the same number" - and why people then over-pressed into NO-WINDOW.
 *
 *  New behaviour: the 1 ms SysTick interrupt samples the button every
 *  millisecond (sonar_button_tick below), waits for it to be steady for
 *  30 ms, and remembers the press.  The main loop picks the press up on its
 *  next round, even if that is a second later.  Every press counts, exactly
 *  once.
 *
 *  And so you know which mode you are in without a laptop, all four LEDs
 *  flash to COUNT the mode after every press:
 *      1 flash  = TONE        (steady 5 kHz test tone)
 *      2 flashes = NORMAL      (real adaptive sonar, real timing)
 *      3 flashes = FAST-REPEAT (same pings, back to back - use for the scope)
 *      4 flashes = NO-WINDOW   (window switched off, for the comparison)
 * ------------------------------------------------------------------------*/
static volatile uint8_t s_btn_event = 0U;

void sonar_button_tick(void)            /* called from SysTick, every 1 ms */
{
    static uint8_t stable = 0U, count = 0U;
    uint8_t now = ((SONAR_BTN_PORT->IDR & SONAR_BTN_PIN) != 0U) ? 1U : 0U;
    if (now != stable) {
        if (++count >= 30U) {           /* 30 ms steady = a real change */
            stable = now;
            count  = 0U;
            if (now) s_btn_event = 1U;  /* count the press, not the release */
        }
    } else {
        count = 0U;
    }
}

static uint32_t bench_number(void)
{
    if (g_tone_mode)   return 1U;
    if (g_window_off)  return 4U;
    if (g_fast_repeat) return 3U;
    return 2U;
}

static void flash_mode(void)
{
    const uint16_t all = LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN;
    for (uint32_t i = 0; i < bench_number(); ++i) {
        HAL_GPIO_WritePin(LED_GREEN_PORT, all, GPIO_PIN_SET);   HAL_Delay(150);
        HAL_GPIO_WritePin(LED_GREEN_PORT, all, GPIO_PIN_RESET); HAL_Delay(250);
    }
    HAL_Delay(300);
}

static void handle_button(void)
{
    if (!s_btn_event) return;
    s_btn_event = 0U;

    if (g_tone_mode) {                                  /* TONE -> NORMAL */
        g_tone_mode = false; g_fast_repeat = false; g_window_off = false;
    } else if (!g_fast_repeat && !g_window_off) {       /* NORMAL -> FAST */
        g_fast_repeat = true;
    } else if (g_fast_repeat && !g_window_off) {        /* FAST -> NO-WINDOW */
        g_window_off = true;
    } else {                                            /* NO-WINDOW -> TONE */
        g_fast_repeat = false; g_window_off = false; g_tone_mode = true;
    }
    hw_uart_printf("> bench mode: %s\r\n", bench_name());
    flash_mode();
    s_btn_event = 0U;       /* ignore anything pressed during the flashing */
}

static void copy_name(volatile char *dst, const char *src, unsigned n)
{
    unsigned i = 0;
    for (; src[i] && i < n - 1U; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static void live_update(void)
{
    g_live.depth_m            = g_env.depth_m;
    g_live.turbidity_pct      = g_env.turbidity_pct;
    g_live.temperature_c      = g_env.temperature_c;
    g_live.salinity_psu       = g_env.salinity_psu;
    g_live.sound_speed_mps    = g_env.sound_speed_mps;
    g_live.absorption_db_km   = g_env.absorption_db_km;
    g_live.f_start_hz         = g_par.f_start_hz;
    g_live.f_end_hz           = g_par.f_end_hz;
    g_live.bandwidth_hz       = g_par.f_end_hz - g_par.f_start_hz;
    g_live.pulse_ms           = g_par.pulse_duration_s * 1000.0f;
    g_live.amplitude          = g_par.amplitude;
    g_live.range_resolution_m = g_par.range_resolution_m;
    g_live.duty_pct           = g_par.duty * 100.0f;
    g_live.avg_power_mw       = g_par.avg_power_mw;
    g_live.ping_index         = g_par.ping_index;
    g_live.cycles_per_decision= g_cycles_per_ping;
    copy_name(g_live.waveform, adapt_waveform_name(g_par.waveform), 12U);
    copy_name(g_live.window,   adapt_window_name(g_par.window),     12U);
    copy_name(g_live.water,    env_turb_name(g_env.turbidity_state), 16U);
    copy_name(g_live.bench,    bench_name(), 14U);
}

int sonar_selfcheck(void);

/* ------------------------------------------------------------------ banner */
static void banner(void)
{
    hw_uart_printf("\r\n");
    hw_uart_printf("====================================================\r\n");
    hw_uart_printf("  ADAPTIVE SOFTWARE-DEFINED SONAR TRANSMITTER  v%s\r\n",
                   SONAR_FW_VERSION);
    hw_uart_printf("  STM32F407VG @ %lu MHz   fs = %lu kHz   scale = %u/%u\r\n",
                   (unsigned long)(HAL_RCC_GetHCLKFreq() / 1000000UL),
                   (unsigned long)(SONAR_FS_HZ / 1000UL),
                   SONAR_FREQ_SCALE_NUM, SONAR_FREQ_SCALE_DEN);
    hw_uart_printf("  keys: 1 SEARCH  2 CLASSIFY  3 TRANSIT  4 POWER_SAVE\r\n");
    hw_uart_printf("  blue USER button cycles: NORMAL > TONE > FAST-REPEAT\r\n");
    hw_uart_printf("                           > NO-WINDOW > NORMAL\r\n");
    hw_uart_printf("  or edit g_tone_mode / g_fast_repeat / g_window_off in\r\n");
    hw_uart_printf("  the Live Expressions view while it runs.\r\n");
    hw_uart_printf("        5  steady tone (use this first)\r\n");
    hw_uart_printf("        c  fast repeat on/off (for recording)\r\n");
    hw_uart_printf("        w  window on/off (shows what the window is for)\r\n");
    hw_uart_printf("        b  simulate battery drop   r  reset\r\n");
    hw_uart_printf("====================================================\r\n\r\n");
}

/* --------------------------------------------------------------- telemetry
 * One line per ping, comma separated, so it can be piped straight into the
 * Python dashboard or opened in a spreadsheet.  A fixed, machine-readable
 * format beats pretty printing the moment you want to plot anything. */
static void telemetry(void)
{
    hw_uart_printf(
        "PING,%lu,%.1f,%.1f,%.1f,%.1f,%s,%s,%.1f,%.1f,%s,%s,%s,"
        "%.0f,%.0f,%.3f,%.3f,%.4f,%.1f,%.1f,%.2f,%.0f,%lu,%lu\r\n",
        (unsigned long)g_par.ping_index,
        g_env.depth_m, g_env.turbidity_pct,
        g_env.temperature_c, g_env.salinity_psu,
        env_depth_name(g_env.depth_state), env_turb_name(g_env.turbidity_state),
        g_env.sound_speed_mps, g_env.absorption_db_km,
        adapt_waveform_name(g_par.waveform),
        adapt_window_name(g_par.window),
        adapt_mode_name(g_par.mode),
        g_par.f_start_hz, g_par.f_end_hz,
        g_par.pulse_duration_s * 1000.0f, g_par.amplitude,
        g_par.range_resolution_m, g_par.compression_gain_db,
        g_par.pri_s * 1000.0f, g_par.duty * 100.0f,
        g_par.avg_power_mw,
        (unsigned long)g_cycles_per_ping,
        (unsigned long)hw_dac_samples_sent());
}

/* ------------------------------------------------------------ console keys */
static void handle_console(void)
{
    int c = hw_uart_getc();
    switch (c) {
    case '1': adapt_set_mode(MODE_SEARCH);     hw_uart_write("> SEARCH\r\n");     break;
    case '2': adapt_set_mode(MODE_CLASSIFY);   hw_uart_write("> CLASSIFY\r\n");   break;
    case '3': adapt_set_mode(MODE_TRANSIT);    hw_uart_write("> TRANSIT\r\n");    break;
    case '4': adapt_set_mode(MODE_POWER_SAVE); hw_uart_write("> POWER_SAVE\r\n"); break;
    case '5': g_tone_mode = !g_tone_mode;
              hw_uart_write(g_tone_mode ? "> steady tone ON\r\n"
                                        : "> steady tone OFF\r\n"); break;
    case 'c': g_fast_repeat = !g_fast_repeat;
              hw_uart_write(g_fast_repeat ? "> fast repeat ON\r\n"
                                          : "> fast repeat OFF\r\n"); break;
    case 'w': g_window_off = !g_window_off;
              hw_uart_write(g_window_off ? "> window BYPASSED (listen for clicks)\r\n"
                                         : "> window ON\r\n"); break;
    case 'b': adapt_set_soc(0.08f);            hw_uart_write("> battery low\r\n");break;
    case 'r': adapt_set_soc(1.0f); adapt_init(); env_init();
              hw_uart_write("> reset\r\n");    break;
    default: break;
    }
}

/* ---------------------------------------------------------- one-time proof
 * Run once at boot: generate a full pulse into a scratch buffer with the
 * cycle counter running, so the very first thing the console prints is a
 * measured, defensible number for CPU cost per sample.  When a judge asks
 * "how do you know it is low power", this is the answer, and it came off
 * the silicon, not off a slide. */
static void benchmark(void)
{
    static uint16_t scratch[DAC_HALF_BUF];
    sonar_params_t p;
    memset(&p, 0, sizeof p);
    p.f_start_hz = 100e3f; p.f_end_hz = 400e3f;
    p.pulse_duration_s = 5e-3f; p.amplitude = 0.9f;
    p.waveform = WF_LFM; p.window = WIN_TUKEY; p.tukey_alpha = 0.25f;

    dds_t d; dds_prepare(&d, &p);
    uint32_t total = 0U, t0 = hw_cycles_now();
    while (d.active) total += dds_fill_dac(&d, scratch, DAC_HALF_BUF);
    uint32_t dt = hw_cycles_now() - t0;

    uint32_t hclk = HAL_RCC_GetHCLKFreq();
    float cyc_per_sample = (float)dt / (float)total;
    float budget = (float)hclk / (float)SONAR_FS_HZ;

    hw_uart_printf("BENCH  %lu samples in %lu cycles\r\n",
                   (unsigned long)total, (unsigned long)dt);
    hw_uart_printf("BENCH  %.1f cycles/sample  (budget at fs is %.0f)\r\n",
                   cyc_per_sample, budget);
    hw_uart_printf("BENCH  waveform generation uses %.1f %% of the CPU "
                   "while transmitting\r\n\r\n",
                   100.0f * cyc_per_sample / budget);
}

/* ================================================================== run   */
void sonar_app_run(void)
{
    hw_cycles_init();
    dds_init();
    env_init();
    adapt_init();
    hw_dac_init();
    hw_adc_init();
    hw_uart_init();

    banner();
    if (sonar_selfcheck() != 0) {
        /* Carry on anyway - a half-configured board still blinks and still
         * teaches you something - but the red LED and the console are now
         * telling you not to trust any waveform that comes out. */
    }
    benchmark();

    uint32_t last_report = 0U;
    depth_state_t     prev_d = DEPTH_MEDIUM;
    turbidity_state_t prev_t = TURB_MEDIUM;

    for (;;) {
        handle_console();
        handle_button();

        /* ---- 1. sense ------------------------------------------------- */
        env_update_from_adc(&g_env, hw_adc_raw());

        /* ---- 2. decide ------------------------------------------------ */
        uint32_t c0 = hw_cycles_now();
        adapt_compute(&g_env, &g_par);
        g_cycles_per_ping = hw_cycles_now() - c0;

        /* ---- 2b. bench overrides -------------------------------------
         * These exist only so a human at a breadboard can isolate one thing
         * at a time.  They sit AFTER adapt_compute so the adaptation logic
         * itself is never modified - we are overriding the request, not the
         * decision-maker.                                                 */
        if (g_tone_mode) {
            /* One steady mid-band tone.  Nothing sweeps, nothing changes.
             * If this does not come out of the speaker, the problem is in
             * the wiring or the clock, not in the clever part. */
            g_par.waveform        = WF_CW;
            g_par.f_start_hz      = 5000.0f * (float)SONAR_FREQ_SCALE_DEN
                                           / (float)SONAR_FREQ_SCALE_NUM;
            g_par.f_end_hz        = g_par.f_start_hz;
            g_par.pulse_duration_s= 0.20f;
            g_par.window          = WIN_TUKEY;
        }
        if (g_window_off) g_par.window = WIN_RECT;

        /* blink orange whenever the water actually changed category */
        bool changed = (g_env.depth_state != prev_d) ||
                       (g_env.turbidity_state != prev_t);
        hw_led(LED_ORANGE_PIN, changed);
        prev_d = g_env.depth_state;
        prev_t = g_env.turbidity_state;

        /* ---- 3. transmit ---------------------------------------------- */
        hw_dac_start_pulse(&g_par);

        /* The pulse is now entirely in hardware's hands.  Sleep through it. */
        uint32_t pulse_ms = (uint32_t)(g_par.pulse_duration_s * 1000.0f) + 1U;
        hw_sleep_until_next_ping(pulse_ms);

        /* Safety net: the DMA needs about 6 ms after the last sample to play
         * out the silent tail.  If it is still busy long after that, stop it
         * by hand so the next ping can go out. */
        uint32_t t_wait = hw_millis();
        while (hw_dac_busy() && (hw_millis() - t_wait) < 50U) { __WFI(); }
        if (hw_dac_busy()) { hw_dac_force_stop(); ++g_dac_stuck; }

        /* ---- 4. listen / idle ----------------------------------------- */
        float idle_s = g_par.pri_s - g_par.pulse_duration_s;
        uint32_t idle_ms = (idle_s > 0.0f) ? (uint32_t)(idle_s * 1000.0f) : 0U;
        if (idle_ms > 2000U) idle_ms = 2000U;    /* keep the demo lively */
        if (idle_ms < 20U)   idle_ms = 20U;
        if (g_tone_mode)     idle_ms = 100U;     /* 200 ms on, 100 ms off */
        /* For a spectrogram you want many chirps in a row, not one chirp and
         * then half a second of silence.  'c' squeezes the gap so Audacity
         * draws a repeating pattern you can actually photograph. */
        if (g_fast_repeat && idle_ms > 80U) idle_ms = 80U;
        hw_sleep_until_next_ping(idle_ms);

        /* ---- 5. report ------------------------------------------------ */
        live_update();
        if ((hw_millis() - last_report) >= 250U) {
            telemetry();
            last_report = hw_millis();
        }
        (void)g_cycles_in_isr;
    }
}
