/* ============================================================================
 *  adaptation.h  -  the decision engine.
 *
 *  This is a direct port of adaptation.m and waveform_selector.m, with the
 *  original thresholds preserved EXACTLY so the C build and the MATLAB build
 *  produce the same answer for the same inputs.  That equivalence is the
 *  whole point of Phase 1 and it is tested in Host/host_sim.c.
 *
 *  On top of the port sit the things a physical transmitter needs and a
 *  simulation does not:
 *
 *    - a hard transducer-envelope clamp, so no sensor reading can ever
 *      command a drive the amplifier cannot survive
 *    - amplitude slew limiting, so the PA never sees a step change
 *    - a dwell timer, so a state must persist before the transmitter acts
 *    - a duty-cycle cap derived from the pulse length, so the stage stays
 *      inside its thermal envelope
 *    - four mission modes, because "adaptive" should mean adaptive to the
 *      job as well as to the water
 *    - computed range resolution, unambiguous range, time-bandwidth product
 *      and compression gain, so every claim on the slide has a number behind it
 * ==========================================================================*/
#ifndef ADAPTATION_H
#define ADAPTATION_H

#include "sonar_types.h"

void adapt_init(void);

/* Set the mission mode.  Changes the priority between range and resolution
 * and the energy the engine is willing to spend. */
void adapt_set_mode(mission_mode_t m);
mission_mode_t adapt_get_mode(void);

/* Battery state of charge, 0..1.  Below 0.15 the engine forces POWER_SAVE. */
void adapt_set_soc(float soc);

/* The decision.  Pure function of (env, internal state) -> params.
 * Safe to call at ADAPT_RATE_HZ; the dwell timer inside decides whether the
 * newly classified state is allowed to take effect yet. */
void adapt_compute(const sonar_env_t *env, sonar_params_t *out);

/* The exact MATLAB port, with none of the extensions applied.  Kept separate
 * so the cross-check against adaptation.m tests the port and not the
 * improvements layered on top of it. */
void adapt_compute_matlab_equivalent(const sonar_env_t *env, sonar_params_t *out);

waveform_t  adapt_select_waveform(const sonar_env_t *env, mission_mode_t mode);
const char *adapt_waveform_name(waveform_t w);
const char *adapt_window_name(window_t w);
const char *adapt_mode_name(mission_mode_t m);

/* First-sidelobe level of each window, dB.  Used for the reported
 * sidelobe_db figure - these are the standard published values. */
float adapt_window_sidelobe_db(window_t w);

#endif /* ADAPTATION_H */
