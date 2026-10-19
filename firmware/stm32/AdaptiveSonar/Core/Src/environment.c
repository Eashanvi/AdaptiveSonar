#include "environment.h"
#include "sonar_config.h"
#include <math.h>

/* ------------------------------------------------------------ IIR filters */
static float s_f[ENV_ADC_CHANNELS];
static bool  s_primed = false;

/* Hysteresis memory: we remember the last classification so a sensor sitting
 * exactly on a threshold cannot make the transmitter flip modes every ping.
 * Without this the demo looks broken the moment someone nudges a pot. */
static depth_state_t     s_last_depth = DEPTH_MEDIUM;
static turbidity_state_t s_last_turb  = TURB_MEDIUM;

void env_init(void)
{
    s_primed = false;
    s_last_depth = DEPTH_MEDIUM;
    s_last_turb  = TURB_MEDIUM;
}

static float map_adc(uint16_t raw, float lo, float hi)
{
    if (raw > 4095U) raw = 4095U;
    return lo + (hi - lo) * ((float)raw / 4095.0f);
}

/* --------------------------------------------------------- Mackenzie 1981 */
float env_sound_speed(float T, float S, float D)
{
    float T2 = T * T, T3 = T2 * T;
    float dS = S - 35.0f;
    return 1448.96f
         + 4.591f   * T
         - 5.304e-2f * T2
         + 2.374e-4f * T3
         + 1.340f    * dS
         + 1.630e-2f * D
         + 1.675e-7f * D * D
         - 1.025e-2f * T * dS
         - 7.139e-13f * T * D * D * D;
}

/* ---------------------------------------------- Francois & Garrison 1982  */
float env_absorption_db_km(float f_hz, float T, float S, float D, float turb)
{
    const float pH = 8.0f;
    float f   = f_hz / 1000.0f;                 /* kHz - F&G works in kHz    */
    float f2  = f * f;
    float c   = env_sound_speed(T, S, D);
    float Tk  = T + 273.0f;

    /* boric acid relaxation - dominates below ~10 kHz */
    float A1 = (8.86f / c) * powf(10.0f, 0.78f * pH - 5.0f);
    float f1 = 2.8f * sqrtf(S / 35.0f) * powf(10.0f, 4.0f - 1245.0f / Tk);
    float a1 = A1 * f1 * f2 / (f1 * f1 + f2);

    /* magnesium sulphate relaxation - dominates 10-500 kHz */
    float A2 = 21.44f * S / c * (1.0f + 0.025f * T);
    float P2 = 1.0f - 1.37e-4f * D + 6.2e-9f * D * D;
    float f22 = 8.17f * powf(10.0f, 8.0f - 1990.0f / Tk)
              / (1.0f + 0.0018f * (S - 35.0f));
    float a2 = A2 * P2 * f22 * f2 / (f22 * f22 + f2);

    /* pure-water viscous absorption - dominates above ~500 kHz */
    float A3;
    if (T <= 20.0f)
        A3 = 4.937e-4f - 2.590e-5f * T + 9.11e-7f * T * T - 1.50e-8f * T * T * T;
    else
        A3 = 3.964e-4f - 1.146e-5f * T + 1.45e-7f * T * T - 6.50e-10f * T * T * T;
    float P3 = 1.0f - 3.83e-5f * D + 4.9e-10f * D * D;
    float a3 = A3 * P3 * f2;

    /* Suspended-sediment scattering.  F&G assumes clear sea water and says
     * nothing about a muddy estuary, which is precisely the case the problem
     * statement cares about.  Volume scattering from suspended particles goes
     * roughly as f^1.5 in the Rayleigh-to-geometric transition for silt-sized
     * grains.  K_SED is set so that a 90 %% turbid estuary adds roughly
     * 18 dB/km at 250 kHz on top of the clear-water figure, which is the
     * right order of magnitude for silt-laden coastal water.
     *
     * This coefficient is an ENGINEERING PLACEHOLDER, not settled physics.
     * Fit it to your own tank or field data before you quote it, and say so
     * out loud if a judge asks where the number came from.  Everything else
     * in this function is published, peer-reviewed and citable; this one
     * term is ours. */
    const float K_SED = 5.0e-5f;               /* dB/km per % per kHz^1.5    */
    float a_sed = K_SED * turb * powf(f, 1.5f);

    return a1 + a2 + a3 + a_sed;
}

float env_max_useful_freq(const sonar_env_t *e, float range_m, float budget_db)
{
    /* Two-way loss = 2*(spreading + absorption).  Spherical spreading is
     * frequency-independent, so it only shifts the budget; the frequency
     * dependence is entirely in the absorption term.  Walk the transducer
     * band from the top down and take the first frequency that fits. */
    float spread = 40.0f * log10f(range_m > 1.0f ? range_m : 1.0f);
    float avail  = budget_db - spread;
    if (avail <= 0.0f) return TX_F_MIN_HZ;

    for (float f = TX_F_MAX_HZ; f >= TX_F_MIN_HZ; f -= 5000.0f) {
        float a = env_absorption_db_km(f, e->temperature_c, e->salinity_psu,
                                       e->depth_m, e->turbidity_pct);
        float loss = 2.0f * a * (range_m / 1000.0f);
        if (loss <= avail) return f;
    }
    return TX_F_MIN_HZ;
}

/* ------------------------------------------------- classification + update */
static turbidity_state_t classify_turb(float t, turbidity_state_t prev)
{
    const float H = ADAPT_TURB_HYST_PCT;
    switch (prev) {
    case TURB_LOW:    return (t > 30.0f + H) ? TURB_MEDIUM : TURB_LOW;
    case TURB_HIGH:   return (t < 70.0f - H) ? TURB_MEDIUM : TURB_HIGH;
    default:
        if (t < 30.0f - H) return TURB_LOW;
        if (t > 70.0f + H) return TURB_HIGH;
        return TURB_MEDIUM;
    }
}

static depth_state_t classify_depth(float d, depth_state_t prev)
{
    const float H = ADAPT_DEPTH_HYST_M;
    switch (prev) {
    case DEPTH_SHALLOW: return (d > 50.0f + H)  ? DEPTH_MEDIUM : DEPTH_SHALLOW;
    case DEPTH_DEEP:    return (d < 120.0f - H) ? DEPTH_MEDIUM : DEPTH_DEEP;
    default:
        if (d < 50.0f  - H) return DEPTH_SHALLOW;
        if (d > 120.0f + H) return DEPTH_DEEP;
        return DEPTH_MEDIUM;
    }
}

void env_update(sonar_env_t *env, float depth_m, float turbidity_pct,
                float temperature_c, float salinity_psu)
{
    env->depth_m       = depth_m;
    env->turbidity_pct = turbidity_pct;
    env->temperature_c = temperature_c;
    env->salinity_psu  = salinity_psu;

    env->turbidity_state = classify_turb(turbidity_pct, s_last_turb);
    env->depth_state     = classify_depth(depth_m, s_last_depth);
    s_last_turb  = env->turbidity_state;
    s_last_depth = env->depth_state;

    env->sound_speed_mps  = env_sound_speed(temperature_c, salinity_psu, depth_m);
    env->absorption_db_km = env_absorption_db_km(250000.0f, temperature_c,
                                                 salinity_psu, depth_m,
                                                 turbidity_pct);
    /* 60 dB of two-way budget at 150 m is a reasonable small-AUV side-scan
     * link; both numbers are in sonar_config.h territory once you measure. */
    env->max_useful_f_hz  = env_max_useful_freq(env, 150.0f, 130.0f);
    env->valid = true;
}

void env_update_from_adc(sonar_env_t *env, const uint16_t raw[ENV_ADC_CHANNELS])
{
    float x[ENV_ADC_CHANNELS];
    x[0] = map_adc(raw[0], ENV_DEPTH_MIN_M, ENV_DEPTH_MAX_M);
    x[1] = map_adc(raw[1], ENV_TURB_MIN_PCT, ENV_TURB_MAX_PCT);
    x[2] = map_adc(raw[2], ENV_TEMP_MIN_C,  ENV_TEMP_MAX_C);
    x[3] = map_adc(raw[3], ENV_SAL_MIN_PSU, ENV_SAL_MAX_PSU);

    if (!s_primed) {
        for (unsigned i = 0; i < ENV_ADC_CHANNELS; ++i) s_f[i] = x[i];
        s_primed = true;
    } else {
        /* first-order IIR:  y += (x - y) / 2^ENV_IIR_SHIFT
         * A pot is a noisy, human-driven input.  Without this the classifier
         * chatters on ADC LSB noise and the telemetry is unreadable. */
        const float a = 1.0f / (float)(1U << ENV_IIR_SHIFT);
        for (unsigned i = 0; i < ENV_ADC_CHANNELS; ++i) s_f[i] += (x[i] - s_f[i]) * a;
    }
    env_update(env, s_f[0], s_f[1], s_f[2], s_f[3]);
}

const char *env_depth_name(depth_state_t s)
{
    static const char *n[] = { "SHALLOW", "MEDIUM", "DEEP" };
    return n[(unsigned)s % 3U];
}
const char *env_turb_name(turbidity_state_t s)
{
    static const char *n[] = { "LOW", "MEDIUM", "HIGH" };
    return n[(unsigned)s % 3U];
}
