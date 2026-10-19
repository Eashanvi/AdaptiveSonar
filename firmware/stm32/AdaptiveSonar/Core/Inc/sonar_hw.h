/* ============================================================================
 *  sonar_hw.h  -  the thin layer between the portable core and the F407.
 * ==========================================================================*/
#ifndef SONAR_HW_H
#define SONAR_HW_H

#include "sonar_types.h"
#include "sonar_config.h"

/* ---------------------------------------------------------------- DAC path */
void     hw_dac_init(void);

/* Start streaming one pulse.  Returns immediately: the DMA and TIM6 do the
 * work and the CPU is free (or asleep) until the pulse is over. */
void     hw_dac_start_pulse(const sonar_params_t *p);

/* Called from the DMA half-transfer and transfer-complete interrupts.
 * This is the ONLY code that runs during a transmission. */
void     hw_dac_on_half(void);
void     hw_dac_on_full(void);

bool     hw_dac_busy(void);
/* Emergency stop if a pulse never finished (should never be needed). */
void     hw_dac_force_stop(void);
uint32_t hw_dac_samples_sent(void);

/* ---------------------------------------------------------------- ADC path */
void     hw_adc_init(void);
/* Latest filtered raw counts, oldest-to-newest order matching board.h. */
const uint16_t *hw_adc_raw(void);
bool     hw_adc_ready(void);

/* ------------------------------------------------------------------- misc  */
void     hw_trig_high(void);
void     hw_trig_low(void);
void     hw_led(uint16_t pin, bool on);
void     hw_sleep_until_next_ping(uint32_t ms);
uint32_t hw_millis(void);

/* DWT cycle counter - the honest way to measure how much CPU a ping costs. */
void     hw_cycles_init(void);
uint32_t hw_cycles_now(void);

/* ------------------------------------------------------------- telemetry   */
void     hw_uart_init(void);
void     hw_uart_write(const char *s);
void     hw_uart_printf(const char *fmt, ...);
/* Non-blocking single-character read; returns -1 if nothing waiting. */
int      hw_uart_getc(void);

#endif /* SONAR_HW_H */
