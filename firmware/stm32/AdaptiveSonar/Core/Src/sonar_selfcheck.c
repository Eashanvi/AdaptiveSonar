/* ============================================================================
 *  sonar_selfcheck.c  -  does the chip agree with what we think we told it?
 *
 *  WHY THIS FILE EXISTS
 *  --------------------
 *  Almost every "my STM32 project does not work" story is not a bug in the
 *  code.  It is one wrong number in a settings dialog three days earlier.
 *  The prescaler was left at 1 instead of 0.  The DMA was left in Normal mode
 *  instead of Circular, so the sound plays once and never again.  The clock
 *  was never switched to the 8 MHz input on the board, so the whole chip is
 *  running ten times too slow and every chirp comes out at the wrong pitch.
 *
 *  None of those announce themselves.  The board still boots, the LED still
 *  blinks, and you lose an evening.
 *
 *  So before the firmware does anything else, it reads the settings back OUT
 *  of the silicon and compares them against what this project needs.  If
 *  something is wrong it says so in one line of plain English, names the
 *  dialog box you have to go fix, and lights the red LED.
 *
 *  Run it once at boot.  It costs about a microsecond and it will save you
 *  an evening at least once.
 * ==========================================================================*/
#include "stm32f4xx_hal.h"
#include "sonar_hw.h"
#include "board.h"

extern DAC_HandleTypeDef  hdac;
extern TIM_HandleTypeDef  htim6;
extern ADC_HandleTypeDef  hadc1;

static int s_fails;

/* --------------------------------------------------------------------------
 *  Bring-up build (v3): the result is ALSO kept in two global variables, so
 *  you can read it in Live Expressions with no serial cable and no
 *  breakpoint:
 *      g_selfcheck_fails   0 = all 15 checks passed
 *      g_selfcheck_msg     the first check that failed, in plain English
 * ------------------------------------------------------------------------*/
volatile int  g_selfcheck_fails = -1;          /* -1 = has not run yet */
volatile char g_selfcheck_msg[64] = "not run yet";

static void set_msg(const char *m)
{
    unsigned i = 0;
    for (; m[i] && i < sizeof g_selfcheck_msg - 1U; ++i) g_selfcheck_msg[i] = m[i];
    g_selfcheck_msg[i] = '\0';
}

/* Print one line per check.  ok == true prints a tick and moves on; ok ==
 * false prints the measured value, what it should have been, and where to go
 * and change it. */
static void check(bool ok, const char *what, const char *fix)
{
    if (ok) {
        hw_uart_printf("  [ ok ] %s\r\n", what);
    } else {
        if (s_fails == 0) set_msg(what);
        ++s_fails;
        hw_uart_printf("  [FAIL] %s\r\n", what);
        hw_uart_printf("         fix: %s\r\n", fix);
    }
}

/* The APB1 *timer* clock is not the same number as the APB1 peripheral
 * clock.  When the APB1 prescaler is anything other than 1, the timers get
 * fed twice the bus clock.  That factor of two is the single most common
 * reason a timer runs at half or double the intended rate. */
static uint32_t apb1_timer_clk(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    return (((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1)
            ? pclk1 : pclk1 * 2U);
}

int sonar_selfcheck(void)
{
    s_fails = 0;

    uint32_t hclk  = HAL_RCC_GetHCLKFreq();
    uint32_t tclk  = apb1_timer_clk();
    uint32_t arr   = htim6.Instance->ARR;
    uint32_t psc   = htim6.Instance->PSC;
    uint32_t mms   = htim6.Instance->CR2 & TIM_CR2_MMS;

    hw_uart_printf("---- configuration self-check ----------------------\r\n");
    hw_uart_printf("  core clock      %lu Hz\r\n", (unsigned long)hclk);
    hw_uart_printf("  timer clock     %lu Hz\r\n", (unsigned long)tclk);
    hw_uart_printf("  TIM6 PSC/ARR    %lu / %lu\r\n",
                   (unsigned long)psc, (unsigned long)arr);

    check(hclk == 168000000UL,
          "core is running at 168 MHz",
          "Clock Configuration tab: HCLK must read 168. If it reads 16, the "
          "chip is still on its internal oscillator - set RCC High Speed "
          "Clock to 'BYPASS Clock Source' (NOT Crystal) in Pinout, then set "
          "HCLK to 168 and let CubeMX solve the tree.");

    check(tclk == TIM6_INPUT_CLK_HZ,
          "APB1 timer clock is 84 MHz",
          "Clock Configuration tab: APB1 Prescaler must be /4, which makes "
          "APB1 peripheral clock 42 MHz and APB1 TIMER clock 84 MHz.");

    check(psc == 0U,
          "TIM6 prescaler is 0",
          "TIM6 > Parameter Settings > Prescaler must be 0. Any other value "
          "divides the sample rate and drops every chirp in pitch.");

    check(arr == TIM6_ARR_FOR_FS,
          "TIM6 auto-reload matches the wanted sample rate",
          "TIM6 > Parameter Settings > Counter Period. For this build it "
          "must be the number printed as TIM6 PSC/ARR above's expected twin: "
          "timer clock / sample rate - 1.");

    check(mms == TIM_TRGO_UPDATE,
          "TIM6 sends an Update event to the DAC",
          "TIM6 > Parameter Settings > Trigger Event Selection must be "
          "'Update Event'. If this is 'Reset', the DAC never gets told to "
          "take a sample and the output sits still at mid-scale.");

    check((tclk % SONAR_FS_HZ) == 0U,
          "sample rate divides the timer clock exactly",
          "Change SONAR_FS_HZ in sonar_config.h to a value that divides the "
          "timer clock with no remainder. A hardware counter can only hold "
          "whole numbers, so 437.5 silently becomes 437 and your chirp is "
          "off pitch.");

    /* ---- DAC path ------------------------------------------------------ */
    check((hdac.Instance->CR & DAC_CR_TEN1) != 0U,
          "DAC channel 1 is triggered by hardware",
          "DAC > Parameter Settings > Trigger must NOT be 'None'.");

    check((hdac.Instance->CR & DAC_CR_TSEL1) == 0U,
          "DAC channel 1 is triggered by TIM6",
          "DAC > Parameter Settings > Trigger must be 'Timer 6 Trigger Out "
          "event'. Any other timer here and the sample rate is wrong.");

    if (hdac.DMA_Handle1 != NULL) {
        check((hdac.DMA_Handle1->Instance->CR & DMA_SxCR_CIRC) != 0U,
              "DAC DMA is in Circular mode",
              "DAC > DMA Settings > select the DAC1 request > Mode must be "
              "'Circular'. In 'Normal' mode the buffer plays exactly once and "
              "then goes silent forever - the classic first-day bug.");
        check(((hdac.DMA_Handle1->Instance->CR >> DMA_SxCR_MSIZE_Pos) & 3U) == 1U,
              "DAC DMA moves 16 bits at a time",
              "DAC > DMA Settings: both Peripheral and Memory data width must "
              "be 'Half Word'. Byte or Word here turns the sine into noise.");
    } else {
        check(false, "DAC DMA exists at all",
              "DAC > DMA Settings > Add > DAC1. Without this the CPU would "
              "have to hand-feed every sample and the whole low-power "
              "argument of this project disappears.");
    }

    /* ---- ADC path ------------------------------------------------------ */
    check(hadc1.Init.NbrOfConversion == ENV_ADC_CHANNELS,
          "ADC1 is set up to scan all four potentiometers",
          "ADC1 > Parameter Settings > Number Of Conversion must be 4, and "
          "Rank 1..4 must be IN11, IN12, IN14, IN15 in that order.");

    check(hadc1.Init.ContinuousConvMode == ENABLE,
          "ADC1 converts continuously",
          "ADC1 > Parameter Settings > Continuous Conversion Mode = Enabled.");

    check(hadc1.Init.DMAContinuousRequests == ENABLE,
          "ADC1 keeps asking the DMA for more",
          "ADC1 > Parameter Settings > DMA Continuous Requests = Enabled. "
          "Without it you get exactly four readings and then the knobs go "
          "dead.");

    if (hadc1.DMA_Handle != NULL) {
        check((hadc1.DMA_Handle->Instance->CR & DMA_SxCR_CIRC) != 0U,
              "ADC DMA is in Circular mode",
              "ADC1 > DMA Settings > Mode = Circular.");
    } else {
        check(false, "ADC DMA exists at all",
              "ADC1 > DMA Settings > Add > ADC1, Mode Circular, Half Word.");
    }

    g_selfcheck_fails = s_fails;
    if (s_fails == 0) {
        set_msg("all 15 checks passed");
        hw_uart_printf("---- all %d checks passed --------------------------\r\n\r\n", 15);
    } else {
        hw_uart_printf("---- %d CHECK(S) FAILED ----------------------------\r\n",
                       s_fails);
        hw_uart_printf("  The red LED is on. Fix the lines marked FAIL in the\r\n"
                       "  .ioc file, save it, let CubeIDE regenerate, rebuild.\r\n"
                       "  Nothing below this point can be trusted until they\r\n"
                       "  all pass.\r\n\r\n");
        hw_led(LED_RED_PIN, true);
    }
    return s_fails;
}
