/* ============================================================================
 *  sonar_types.h  -  The data that flows between the three core stages.
 *
 *      sensors --> sonar_env_t --> [adaptation] --> sonar_params_t
 *                                                        |
 *                                                        v
 *                                                  [dds/waveform] --> DAC
 *
 *  Keeping these as plain PODs (no pointers, no dynamic memory) is deliberate:
 *  the whole state of the transmitter is a single struct you can dump over
 *  UART, log to a CSV, or diff against the MATLAB reference.
 * ==========================================================================*/
#ifndef SONAR_TYPES_H
#define SONAR_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------- enumerations */
typedef enum {
    TURB_LOW = 0,
    TURB_MEDIUM,
    TURB_HIGH
} turbidity_state_t;

typedef enum {
    DEPTH_SHALLOW = 0,
    DEPTH_MEDIUM,
    DEPTH_DEEP
} depth_state_t;

typedef enum {
    WF_LFM = 0,          /* linear frequency modulated chirp                 */
    WF_GEOMETRIC,        /* geometric (exponential) sweep                    */
    WF_HYPERBOLIC,       /* hyperbolic FM - Doppler tolerant                 */
    WF_BARKER13,         /* 13-chip Barker BPSK on carrier                   */
    WF_STEPPED,          /* stepped-frequency burst                          */
    WF_CW,               /* plain tone - baseline / calibration              */
    WF_COUNT
} waveform_t;

typedef enum {
    WIN_RECT = 0,
    WIN_HAMMING,
    WIN_HANN,
    WIN_BLACKMAN,
    WIN_TUKEY,           /* taper only the edges - keeps energy up           */
    WIN_COUNT
} window_t;

typedef enum {
    MODE_SEARCH = 0,     /* wide area, range priority                        */
    MODE_CLASSIFY,       /* close inspection, resolution priority            */
    MODE_TRANSIT,        /* obstacle avoidance only, minimum energy          */
    MODE_POWER_SAVE,     /* battery critical                                 */
    MODE_COUNT
} mission_mode_t;

/* ----------------------------------------------------------- environment in */
typedef struct {
    /* raw, engineering units */
    float depth_m;
    float turbidity_pct;
    float temperature_c;
    float salinity_psu;

    /* classified, with hysteresis already applied */
    depth_state_t     depth_state;
    turbidity_state_t turbidity_state;

    /* DERIVED ACOUSTICS - the MATLAB model collected temperature and salinity
     * and then never used them.  Here they do real work: they set the speed of
     * sound, which sets the range resolution and the pulse repetition
     * interval, and they set the absorption coefficient, which caps the
     * highest frequency worth transmitting at all. */
    float sound_speed_mps;      /* Mackenzie 1981                             */
    float absorption_db_km;     /* Francois-Garrison, evaluated at f_centre    */
    float max_useful_f_hz;      /* where absorption eats the link budget       */

    bool  valid;                /* false until the first full sensor sweep     */
} sonar_env_t;

/* ------------------------------------------------------ transmitter params  */
typedef struct {
    /* the three parameters the problem statement names explicitly */
    float f_start_hz;
    float f_end_hz;
    float pulse_duration_s;
    float amplitude;            /* 0..1, normalised drive into the PA         */

    /* waveform shaping */
    waveform_t waveform;
    window_t   window;
    float      tukey_alpha;     /* fraction of the pulse that is taper        */

    /* timing */
    float pri_s;                /* pulse repetition interval                  */
    float duty;                 /* pulse_duration / pri                       */

    /* derived quality metrics - computed, never guessed */
    float bandwidth_hz;
    float f_centre_hz;
    float range_resolution_m;   /* c / (2 * B)                                */
    float max_range_m;          /* c * pri / 2                                */
    float time_bandwidth;       /* B * T  - pulse compression gain            */
    float compression_gain_db;  /* 10*log10(BT)                               */
    float sidelobe_db;          /* first sidelobe, from the window choice     */

    /* energy accounting */
    float pulse_energy_mj;
    float avg_power_mw;
    float endurance_hours;

    mission_mode_t mode;
    uint32_t       ping_index;
} sonar_params_t;

/* ------------------------------------------------------------- DDS state    */
typedef struct {
    uint32_t phase;             /* Q32 phase accumulator                      */
    uint32_t phase_inc;         /* Q32 tuning word (constant-frequency modes) */
    uint64_t inc_q48;           /* chirps: tuning word with 16 extra frac bits*/
    int64_t  inc_delta_q48;     /* LFM: linear increment of the tuning word   */
    uint32_t geo_ratio_q32;     /* GEO: fractional part of the per-sample gain*/
    uint32_t n;                 /* sample index within the pulse              */
    uint32_t n_total;           /* pulse length in samples                    */
    uint32_t chip_len;          /* BPSK: samples per chip                     */
    uint32_t code_bits;         /* BPSK: packed code, LSB first               */
    uint32_t code_len;
    uint32_t step_len;          /* stepped-FM: samples per step               */
    uint32_t step_count;
    uint32_t base_inc;          /* stepped-FM / BPSK carrier                  */
    int32_t  step_inc;
    waveform_t waveform;
    window_t   window;
    uint32_t   taper_len;       /* Tukey: samples in each taper               */
    int32_t    amp_q15;         /* amplitude in Q15                           */
    bool       active;
} dds_t;

#endif /* SONAR_TYPES_H */
