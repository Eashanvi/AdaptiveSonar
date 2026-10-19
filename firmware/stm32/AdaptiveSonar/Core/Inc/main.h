/* ============================================================================
 *  main.h  -  Makara sonar transmitter, STM32F407G-DISC1.
 *  Bring-up build (v3, 25 Sept 2026).  Replaces the CubeMX-generated main.h.
 * ==========================================================================*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* Called when an initialisation step fails.  Never returns.
 * The red LED blinks FAST (5 times a second) and g_boot_stage says where. */
void Error_Handler(void);

/* Called from the crash handlers in stm32f4xx_it.c.  Never returns.
 * ALL FOUR LEDs blink together, slowly. */
void board_crash_blink(void);

/* How far the boot got.  Type  g_boot_stage  into Live Expressions.
 *   1 HAL started          5 DAC ready
 *   2 clock at 168 MHz     6 TIM6 ready
 *   3 LEDs ready           7 ADC ready
 *   4 DMA ready            8 UART ready  ->  9 sonar app running          */
extern volatile uint32_t g_boot_stage;

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
