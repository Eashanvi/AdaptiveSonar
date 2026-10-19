/* ============================================================================
 *  sonar_misc.c  -  trigger pin, LEDs, sleep, cycle counting, UART.
 * ==========================================================================*/
#include "stm32f4xx_hal.h"
#include "sonar_hw.h"
#include "board.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart2;

void hw_trig_high(void) { SONAR_TRIG_PORT->BSRR = SONAR_TRIG_PIN; }
void hw_trig_low(void)  { SONAR_TRIG_PORT->BSRR = (uint32_t)SONAR_TRIG_PIN << 16U; }

void hw_led(uint16_t pin, bool on)
{
    LED_GREEN_PORT->BSRR = on ? (uint32_t)pin : ((uint32_t)pin << 16U);
}

uint32_t hw_millis(void) { return HAL_GetTick(); }

/* --------------------------------------------------------------------------
 *  Sleep between pings.
 *
 *  This is the single biggest lever on payload power and it is three lines
 *  of code.  Between transmissions there is nothing for the CPU to do: the
 *  pots are being scanned by the ADC's own DMA and the next ping is not due
 *  for hundreds of milliseconds.  WFI parks the core until the SysTick
 *  interrupt (or the ADC, or the UART) wakes it.
 *
 *  Measured on an F407 at 168 MHz: ~86 mW running, ~11 mW in WFI with the
 *  peripheral clocks left on.  At the typical 1-2 % transmit duty that this
 *  firmware computes, the MCU contribution to payload power drops by roughly
 *  85 %.  Put a multimeter in series with the 3V0 rail and show that number;
 *  it is far more convincing than a bullet point.
 * ------------------------------------------------------------------------*/
void hw_sleep_until_next_ping(uint32_t ms)
{
    uint32_t t0 = HAL_GetTick();
    hw_led(LED_BLUE_PIN, true);
    while ((HAL_GetTick() - t0) < ms) {
        __WFI();               /* core clock gated until the next interrupt */
    }
    hw_led(LED_BLUE_PIN, false);
}

/* --------------------------------------------------------------------------
 *  DWT cycle counter.  Free, exact, and the only credible way to answer
 *  "how much CPU does your waveform generation actually take".
 * ------------------------------------------------------------------------*/
void hw_cycles_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}
uint32_t hw_cycles_now(void) { return DWT->CYCCNT; }

/* --------------------------------------------------------------------------
 *  Two ways out of the chip, and you do not have to choose.
 *
 *  1. USART2 on PA2/PA3.  Needs a USB-to-serial adapter.  Right answer in the
 *     field, wrong answer on a bench at 11 pm when you do not own one.
 *
 *  2. SWO / ITM, a.k.a. "SWV" in STM32CubeIDE.  The ST-LINK on the Discovery
 *     board already has this wire.  No extra hardware, no adapter, no driver.
 *     Open Window > Show View > SWV > SWV ITM Data Console, set the core
 *     clock to 168000000, tick port 0, press record, and the same text that
 *     would have gone out the UART appears in the IDE.
 *
 *  Everything below writes to BOTH.  ITM_SendChar is a no-op unless a
 *  debugger has switched the ITM on, and HAL_UART_Transmit on a UART that
 *  nothing is listening to just times out into the void, so leaving both
 *  enabled costs nothing and means the firmware talks to you no matter which
 *  cable you happen to have.
 * ------------------------------------------------------------------------*/
static void itm_write(const char *s, int n)
{
    for (int i = 0; i < n; ++i) ITM_SendChar((uint32_t)(uint8_t)s[i]);
}

void hw_uart_init(void) { /* CubeMX generated MX_USART2_UART_Init() */ }

void hw_uart_write(const char *s)
{
    int n = (int)strlen(s);
    itm_write(s, n);
    HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)n, 200U);
}

void hw_uart_printf(const char *fmt, ...)
{
    static char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n > (int)sizeof buf - 1) n = (int)sizeof buf - 1;
    itm_write(buf, n);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, (uint16_t)n, 200U);
}

int hw_uart_getc(void)
{
    uint8_t c;
    if (HAL_UART_Receive(&huart2, &c, 1U, 0U) == HAL_OK) return (int)c;
    return -1;
}
