/* ============================================================================
 *  dds.h  -  Direct Digital Synthesis engine.
 *
 *  This file is the reason the firmware meets its real-time budget.
 *
 *  The naive way to make a chirp on a microcontroller is
 *
 *      for (n = 0; n < N; n++)
 *          y[n] = A * cosf(2*PI*(f0*n/fs + 0.5*k*(n/fs)*(n/fs)));
 *
 *  which is what the MATLAB reference does, and which costs a multiply-heavy
 *  cosf() call (~90-140 cycles on Cortex-M4F) plus two float multiplies per
 *  sample.  At 1 Msps on a 168 MHz part that is well over half the CPU spent
 *  computing trigonometry, with a hard deadline and no headroom - exactly the
 *  failure mode the problem statement calls out.
 *
 *  Instead we use a phase accumulator.  Phase is an unsigned 32-bit number
 *  that wraps naturally at 2*PI, so there is no modulo and no range reduction.
 *  Advancing the phase is one add.  A chirp is produced by ALSO advancing the
 *  phase increment - a second accumulator.  The whole per-sample cost is:
 *
 *      2 adds, 1 table lookup, 1 interpolation, 2 multiplies.   ~20 cycles.
 *
 *  This is precisely the structure an FPGA DDS core uses, which is why the
 *  Phase-2 migration is a port and not a rewrite: the same tuning word, the
 *  same accumulator width, the same LUT.
 * ==========================================================================*/
#ifndef DDS_H
#define DDS_H

#include "sonar_types.h"
#include "sonar_config.h"

/* Build the Q15 sine table.  Call once at boot. */
void dds_init(void);

/* Arm the engine for one pulse described by p.  Does all the expensive
 * float maths ONCE, here, outside the sample loop. */
void dds_prepare(dds_t *d, const sonar_params_t *p);

/* Produce up to `count` samples into `dst` as DAC-ready 12-bit right-aligned
 * codes.  Returns how many were actually written; a short return means the
 * pulse ended.  The remainder of `dst` is filled with DAC_MID (silence) so the
 * DMA never has to be stopped mid-transfer. */
uint32_t dds_fill_dac(dds_t *d, uint16_t *dst, uint32_t count);

/* Same generator, but emitting normalised float in [-1, 1].  Used by the host
 * simulator so the C output can be diffed against MATLAB directly.  Bit-for-
 * bit the same code path as dds_fill_dac - only the final scaling differs. */
uint32_t dds_fill_float(dds_t *d, float *dst, uint32_t count);

/* Instantaneous frequency of the next sample, in Hz, AFTER scaling.
 * For plotting the frequency-vs-time curve on the host. */
float dds_inst_freq(const dds_t *d);

/* Exposed for the window unit tests and for the host simulator. */
int32_t dds_window_q15(const dds_t *d, uint32_t n);
int32_t dds_sin_q15(uint32_t phase);

#endif /* DDS_H */
