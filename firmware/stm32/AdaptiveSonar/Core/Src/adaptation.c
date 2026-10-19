#include "adaptation.h"
#include "environment.h"
#include "sonar_config.h"
#include <math.h>
#include <string.h>

static mission_mode_t s_mode = MODE_SEARCH;
static float          s_soc  = 1.0f;
static float          s_last_amp = 0.0f;
static uint32_t       s_ping = 0U;

/* dwell state: a newly classified environment must hold for ADAPT_DWELL_PINGS
 * consecutive decisions before the transmitter is allowed to act on it. */
static depth_state_t     s_committed_depth = DEPTH_MEDIUM;
static turbidity_state_t s_committed_turb  = TURB_MEDIUM;
static depth_state_t     s_pending_depth   = DEPTH_MEDIUM;
static turbidity_state_t s_pending_turb    = TURB_MEDIUM;
static uint32_t          s_dwell = 0U;

void adapt_init(void)
{
    s_mode = MODE_SEARCH;
    s_soc = 1.0f;
    s_last_amp = 0.0f;
    s_ping = 0U;
    s_committed_depth = s_pending_depth = DEPTH_MEDIUM;
    s_committed_turb  = s_pending_turb  = TURB_MEDIUM;
    s_dwell = 0U;
}

void adapt_set_mode(mission_mode_t m) { if (m < MODE_COUNT) s_mode = m; }
mission_mode_t adapt_get_mode(void)   { return s_mode; }
void adapt_set_soc(float soc)         { s_soc = soc < 0.f ? 0.f : (soc > 1.f ? 1.f : soc); }

/* ==========================================================================
 *  1.  THE MATLAB PORT  -  byte-for-byte the same decisions as adaptation.m
 * ========================================================================*/
void adapt_compute_matlab_equivalent(const sonar_env_t *env, sonar_params_t *p)
{
    memset(p, 0, sizeof(*p));

    /* defaults, exactly as in adaptation.m */
    p->f_start_hz = 100e3f;
    p->f_end_hz   = 300e3f;
    p->pulse_duration_s = 5e-3f;
    p->amplitude  = 0.7f;
    p->waveform   = WF_LFM;

    switch (env->turbidity_state) {
    case TURB_LOW:
        p->f_start_hz = 300e3f; p->f_end_hz = 500e3f;
        p->pulse_duration_s = 2e-3f; p->amplitude = 0.6f; break;
    case TURB_MEDIUM:
        p->f_start_hz = 200e3f; p->f_end_hz = 400e3f;
        p->pulse_duration_s = 3e-3f; p->amplitude = 0.7f; break;
    default:
        p->f_start_hz = 100e3f; p->f_end_hz = 250e3f;
        p->pulse_duration_s = 5e-3f; p->amplitude = 0.9f; break;
    }

    if (env->depth_state == DEPTH_DEEP) {
        p->pulse_duration_s *= 1.5f;
        p->amplitude = fminf(p->amplitude + 0.1f, 1.0f);
    } else if (env->depth_state == DEPTH_SHALLOW) {
        p->pulse_duration_s *= 0.8f;
    }

    /* waveform_selector.m */
    if (env->depth_state == DEPTH_SHALLOW && env->turbidity_state == TURB_LOW)
        p->waveform = WF_BARKER13;
    else if (env->depth_state == DEPTH_DEEP && env->turbidity_state == TURB_HIGH)
        p->waveform = WF_GEOMETRIC;
    else
        p->waveform = WF_LFM;

    p->window      = WIN_RECT;
    p->bandwidth_hz = p->f_end_hz - p->f_start_hz;
    p->f_centre_hz  = 0.5f * (p->f_start_hz + p->f_end_hz);
}

/* ==========================================================================
 *  2.  WAVEFORM SELECTION, extended with mission context
 * ========================================================================*/
waveform_t adapt_select_waveform(const sonar_env_t *env, mission_mode_t mode)
{
    /* Transit is about not hitting things, not about imaging.  A short
     * constant-envelope stepped burst is the cheapest thing that still gives
     * the receiver compression gain, so it is what we send while driving. */
    if (mode == MODE_TRANSIT)    return WF_STEPPED;
    if (mode == MODE_POWER_SAVE) return WF_CW;

    /* Clear shallow water and a classification job: we want the sharpest
     * possible range cut and we do not need penetration.  A Barker-13 BPSK
     * pulse gives a 22.3 dB peak-to-sidelobe ratio with a rectangular
     * envelope the amplifier likes. */
    if (env->depth_state == DEPTH_SHALLOW && env->turbidity_state == TURB_LOW)
        return WF_BARKER13;

    /* Deep AND muddy: absorption is frequency-selective and severe, so a
     * LINEAR sweep spends equal time at frequencies that will not come back.
     * A geometric sweep spends proportionally more time low, matching the
     * transmitted spectrum to what the channel will actually pass. */
    if (env->depth_state == DEPTH_DEEP && env->turbidity_state == TURB_HIGH)
        return WF_GEOMETRIC;

    /* Deep but clear usually means we are moving over open water, where
     * platform Doppler smears a linear chirp's compressed peak.  Hyperbolic
     * FM has a Doppler-invariant ambiguity function and holds its resolution. */
    if (env->depth_state == DEPTH_DEEP && env->turbidity_state == TURB_LOW)
        return WF_HYPERBOLIC;

    return WF_LFM;
}

/* Published first-sidelobe levels. */
float adapt_window_sidelobe_db(window_t w)
{
    switch (w) {
    case WIN_RECT:     return -13.3f;
    case WIN_HAMMING:  return -42.7f;
    case WIN_HANN:     return -31.5f;
    case WIN_BLACKMAN: return -58.1f;
    case WIN_TUKEY:    return -28.0f;   /* alpha dependent; ~0.25 taper */
    default:           return -13.3f;
    }
}

/* ==========================================================================
 *  3.  THE FULL ENGINE
 * ========================================================================*/
void adapt_compute(const sonar_env_t *env, sonar_params_t *p)
{
    /* ---- dwell: refuse to act on a state that has not settled ---------- */
    if (env->depth_state != s_pending_depth ||
        env->turbidity_state != s_pending_turb) {
        s_pending_depth = env->depth_state;
        s_pending_turb  = env->turbidity_state;
        s_dwell = 0U;
    } else if (s_dwell < ADAPT_DWELL_PINGS) {
        s_dwell++;
        if (s_dwell >= ADAPT_DWELL_PINGS) {
            s_committed_depth = s_pending_depth;
            s_committed_turb  = s_pending_turb;
        }
    }

    sonar_env_t eff = *env;
    eff.depth_state     = s_committed_depth;
    eff.turbidity_state = s_committed_turb;

    /* ---- start from the MATLAB baseline so behaviour is traceable ------ */
    adapt_compute_matlab_equivalent(&eff, p);

    /* ---- battery override ---------------------------------------------- */
    mission_mode_t mode = s_mode;
    if (s_soc < 0.15f) mode = MODE_POWER_SAVE;
    p->mode = mode;

    /* ---- mission-mode shaping ------------------------------------------ */
    switch (mode) {
    case MODE_CLASSIFY:
        /* resolution priority: widen the sweep, shorten the pulse */
        p->f_end_hz  = p->f_end_hz * 1.25f;
        p->pulse_duration_s *= 0.6f;
        break;
    case MODE_TRANSIT:
        p->pulse_duration_s *= 0.5f;
        p->amplitude *= 0.6f;
        break;
    case MODE_POWER_SAVE:
        p->pulse_duration_s *= 0.4f;
        p->amplitude *= 0.45f;
        p->f_end_hz = p->f_start_hz * 1.05f;   /* near-CW, minimum energy */
        break;
    case MODE_SEARCH:
    default:
        /* range priority: lengthen the pulse, keep the bandwidth.  Pulse
         * compression means longer does NOT mean blurrier - that is the
         * whole reason we transmit chirps instead of tone bursts. */
        p->pulse_duration_s *= 1.2f;
        break;
    }

    /* ---- physics override: do not transmit above what the water passes -- */
    if (env->valid && env->max_useful_f_hz > TX_F_MIN_HZ) {
        if (p->f_end_hz > env->max_useful_f_hz) {
            float shrink = env->max_useful_f_hz / p->f_end_hz;
            p->f_end_hz = env->max_useful_f_hz;
            /* keep some bandwidth rather than collapsing to a tone */
            float min_bw = 40e3f;
            if (p->f_end_hz - p->f_start_hz < min_bw)
                p->f_start_hz = fmaxf(TX_F_MIN_HZ, p->f_end_hz - min_bw);
            (void)shrink;
        }
    }

    /* ---- hard transducer clamp ------------------------------------------ */
    p->f_start_hz = fminf(fmaxf(p->f_start_hz, TX_F_MIN_HZ), TX_F_MAX_HZ);
    p->f_end_hz   = fminf(fmaxf(p->f_end_hz,   TX_F_MIN_HZ), TX_F_MAX_HZ);
    if (p->f_end_hz < p->f_start_hz) {
        float t = p->f_end_hz; p->f_end_hz = p->f_start_hz; p->f_start_hz = t;
    }
    p->pulse_duration_s = fminf(fmaxf(p->pulse_duration_s,
                                      TX_PULSE_MIN_S), TX_PULSE_MAX_S);

    /* ---- amplitude: clamp, then slew limit ------------------------------ */
    float want = fminf(p->amplitude, TX_AMPL_MAX);
    float d = want - s_last_amp;
    if (d >  TX_AMPL_SLEW_PER_PING) want = s_last_amp + TX_AMPL_SLEW_PER_PING;
    if (d < -TX_AMPL_SLEW_PER_PING) want = s_last_amp - TX_AMPL_SLEW_PER_PING;
    p->amplitude = want;
    s_last_amp = want;

    /* ---- waveform and window -------------------------------------------- */
    p->waveform = adapt_select_waveform(&eff, mode);

    /* A phase-coded pulse is rectangular by design - windowing it destroys
     * the code.  Everything else gets an edge taper, and the choice trades
     * sidelobe suppression against transmitted energy. */
    if (p->waveform == WF_BARKER13 || p->waveform == WF_STEPPED) {
        p->window = WIN_TUKEY;
        p->tukey_alpha = 0.08f;          /* just enough to protect the PA */
    } else if (mode == MODE_CLASSIFY) {
        p->window = WIN_BLACKMAN;        /* buy sidelobes with energy      */
        p->tukey_alpha = 0.0f;
    } else if (mode == MODE_POWER_SAVE) {
        p->window = WIN_TUKEY;
        p->tukey_alpha = 0.10f;
    } else {
        p->window = WIN_TUKEY;           /* the sensible default            */
        p->tukey_alpha = 0.25f;
    }

    /* ---- derived metrics: every number on the slide, computed ------------ */
    float c = env->valid ? env->sound_speed_mps : 1500.0f;
    p->bandwidth_hz  = p->f_end_hz - p->f_start_hz;
    p->f_centre_hz   = 0.5f * (p->f_start_hz + p->f_end_hz);
    p->range_resolution_m = (p->bandwidth_hz > 1.0f)
                          ? (c / (2.0f * p->bandwidth_hz)) : 999.0f;
    p->time_bandwidth = p->bandwidth_hz * p->pulse_duration_s;
    p->compression_gain_db = (p->time_bandwidth > 1.0f)
                           ? 10.0f * log10f(p->time_bandwidth) : 0.0f;
    p->sidelobe_db = adapt_window_sidelobe_db(p->window);

    /* PRI: listen long enough for the deepest echo we care about, then add
     * the pulse itself, then respect the thermal duty cap - whichever is
     * longer wins.  This is where the duty cycle actually comes from; it is
     * not a number somebody typed in. */
    float listen_range = 2.0f * fmaxf(env->depth_m, 25.0f) + 50.0f;
    float pri_range = 2.0f * listen_range / c;
    float pri_duty  = p->pulse_duration_s / TX_DUTY_MAX;
    p->pri_s = fmaxf(pri_range + p->pulse_duration_s, pri_duty);
    p->duty  = p->pulse_duration_s / p->pri_s;
    p->max_range_m = c * (p->pri_s - p->pulse_duration_s) / 2.0f;

    /* ---- energy -------------------------------------------------------- */
    float pa_peak = PWR_PA_PEAK_MW * p->amplitude * p->amplitude;  /* P ~ V^2 */
    p->pulse_energy_mj = pa_peak * p->pulse_duration_s * 1000.0f;  /* mW*ms   */
    p->avg_power_mw = PWR_PA_QUIESCENT_MW
                    + pa_peak * p->duty
                    + (PWR_MCU_SLEEP_MW
                       + (PWR_MCU_ACTIVE_MW - PWR_MCU_SLEEP_MW) * p->duty);
    p->endurance_hours = (p->avg_power_mw > 1.0f)
                       ? (PWR_BATTERY_WH * 1000.0f / p->avg_power_mw) : 0.0f;

    p->ping_index = ++s_ping;
}

/* ------------------------------------------------------------------ names */
const char *adapt_waveform_name(waveform_t w)
{
    static const char *n[WF_COUNT] = {
        "LFM", "GEOMETRIC", "HYPERBOLIC", "BARKER13", "STEPPED", "CW"
    };
    return (w < WF_COUNT) ? n[w] : "?";
}
const char *adapt_window_name(window_t w)
{
    static const char *n[WIN_COUNT] = {
        "RECT", "HAMMING", "HANN", "BLACKMAN", "TUKEY"
    };
    return (w < WIN_COUNT) ? n[w] : "?";
}
const char *adapt_mode_name(mission_mode_t m)
{
    static const char *n[MODE_COUNT] = {
        "SEARCH", "CLASSIFY", "TRANSIT", "POWER_SAVE"
    };
    return (m < MODE_COUNT) ? n[m] : "?";
}
