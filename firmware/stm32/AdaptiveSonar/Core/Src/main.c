/* ============================================================================
 *  main.c  -  Makara sonar transmitter, STM32F407G-DISC1.
 *  Bring-up build (v3, 25 Sept 2026).  REPLACES the CubeMX-generated main.c.
 *
 *  WHY THIS FILE WAS REWRITTEN
 *  ---------------------------
 *  Until now every peripheral setting came from the .ioc file.  One wrong
 *  number there (clock source, timer period, DMA mode) and the board either
 *  lights the red LED or freezes before any LED comes on.  This file sets up
 *  every peripheral itself, with exactly the values the sonar needs, so the
 *  .ioc no longer matters.
 *
 *  IMPORTANT: do NOT open the .ioc and press "Generate Code" after copying
 *  this in.  That would overwrite this file with the old settings.  If
 *  CubeIDE ever asks "Do you want to generate code?", answer NO.
 *
 *  THE CLOCK
 *  ---------
 *  The old setup took its 8 MHz from the ST-LINK chip on the board (HSE
 *  bypass).  If that signal is missing - external ST-LINK dongle, board
 *  powered from the dongle, different board revision - the clock setup fails
 *  and the program stops dead before any LED lights.  This version uses the
 *  chip's own internal 16 MHz oscillator (HSI), which is always there, and
 *  multiplies it to the same 168 MHz.  Pitch accuracy is 1 %, which no
 *  oscilloscope at the judging table will notice.
 *
 *  WHAT YOU SEE AT POWER-UP
 *  ------------------------
 *    1. All four LEDs (green, orange, red, blue) on together for 1 second.
 *       -> the new program is running.  If you never see this, it is not.
 *    2. They go off.  Then blue is on most of the time (sleeping between
 *       pings) and green comes on during every transmission.
 *    3. Red steady      = a self-check failed (see g_selfcheck_msg).
 *       Red fast blink  = an initialisation step failed (see g_boot_stage).
 *       All four blink  = the program crashed (HardFault).
 * ==========================================================================*/
#include "main.h"
#include "board.h"
#include "sonar_config.h"

/* ---- peripheral handles (names match what the sonar_*.c files expect) --- */
DAC_HandleTypeDef  hdac;
DMA_HandleTypeDef  hdma_dac1;
TIM_HandleTypeDef  htim6;
ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
UART_HandleTypeDef huart2;

volatile uint32_t g_boot_stage = 0U;

void sonar_app_run(void);

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM6_Init(void);
static void MX_ADC1_Init(void);
static void MX_USART2_UART_Init(void);
static void lamp_test(void);

#define ALL_LEDS  (LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN)

/* ========================================================================== */
int main(void)
{
    HAL_Init();                 g_boot_stage = 1U;
    SystemClock_Config();       g_boot_stage = 2U;
    MX_GPIO_Init();             g_boot_stage = 3U;
    lamp_test();                                    /* 1 s, all four LEDs on */
    MX_DMA_Init();              g_boot_stage = 4U;
    MX_DAC_Init();              g_boot_stage = 5U;
    MX_TIM6_Init();             g_boot_stage = 6U;
    MX_ADC1_Init();             g_boot_stage = 7U;
    MX_USART2_UART_Init();      g_boot_stage = 8U;

    g_boot_stage = 9U;
    sonar_app_run();            /* never returns */

    while (1) { }
}

/* --------------------------------------------------------------------------
 *  168 MHz from the internal 16 MHz oscillator.
 *    16 MHz / M16 = 1 MHz  ->  x N336 = 336 MHz  ->  / P2 = 168 MHz core
 *    APB1 = 168/4 = 42 MHz, and the APB1 TIMER clock is 2 x 42 = 84 MHz
 *    (this is the 84 MHz that board.h divides down to the 200 kHz sample rate)
 *    APB2 = 168/2 = 84 MHz
 * ------------------------------------------------------------------------*/
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 16U;
    osc.PLL.PLLN            = 336U;
    osc.PLL.PLLP            = RCC_PLLP_DIV2;
    osc.PLL.PLLQ            = 7U;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
}

/* --------------------------------------------------------------------------
 *  LEDs PD12-15, scope trigger PD2 (outputs), blue USER button PA0 (input).
 * ------------------------------------------------------------------------*/
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOD, ALL_LEDS | SONAR_TRIG_PIN, GPIO_PIN_RESET);

    g.Pin   = ALL_LEDS | SONAR_TRIG_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOD, &g);

    /* The board already has a pull-down on PA0.  No internal pull here. */
    g.Pin  = SONAR_BTN_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SONAR_BTN_PORT, &g);
}

static void lamp_test(void)
{
    HAL_GPIO_WritePin(GPIOD, ALL_LEDS, GPIO_PIN_SET);
    HAL_Delay(1000);
    HAL_GPIO_WritePin(GPIOD, ALL_LEDS, GPIO_PIN_RESET);
    HAL_Delay(200);
}

/* DMA controller clocks and interrupts.  The streams themselves are set up
 * in stm32f4xx_hal_msp.c when the DAC and ADC are initialised. */
static void MX_DMA_Init(void)
{
    __HAL_RCC_DMA1_CLK_ENABLE();                  /* DAC  -> DMA1 Stream5 */
    __HAL_RCC_DMA2_CLK_ENABLE();                  /* ADC1 -> DMA2 Stream0 */

    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/* DAC channel 1 on PA4, output buffer on, triggered by TIM6. */
static void MX_DAC_Init(void)
{
    DAC_ChannelConfTypeDef ch = {0};

    hdac.Instance = DAC;
    if (HAL_DAC_Init(&hdac) != HAL_OK) Error_Handler();

    ch.DAC_Trigger      = DAC_TRIGGER_T6_TRGO;
    ch.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
    if (HAL_DAC_ConfigChannel(&hdac, &ch, DAC_CHANNEL_1) != HAL_OK) Error_Handler();
}

/* TIM6: the sample clock.  84 MHz / (419 + 1) = 200 kHz.  Its only job is
 * to send an "update" pulse to the DAC 200 000 times a second. */
static void MX_TIM6_Init(void)
{
    TIM_MasterConfigTypeDef m = {0};

    htim6.Instance               = TIM6;
    htim6.Init.Prescaler         = 0U;
    htim6.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim6.Init.Period            = TIM6_ARR_FOR_FS;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK) Error_Handler();

    m.MasterOutputTrigger = TIM_TRGO_UPDATE;
    m.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &m) != HAL_OK) Error_Handler();
}

/* ADC1: scans the four pots continuously, DMA writes them into memory.
 *   Rank 1 PC1 depth, Rank 2 PC2 turbidity, Rank 3 PC4 temp, Rank 4 PC5 sal */
static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef ch = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV8;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = ENABLE;
    hadc1.Init.ContinuousConvMode    = ENABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = ENV_ADC_CHANNELS;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SEQ_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK) Error_Handler();

    const uint32_t chans[4] = { SONAR_ADC_CH_DEPTH, SONAR_ADC_CH_TURB,
                                SONAR_ADC_CH_TEMP,  SONAR_ADC_CH_SAL };
    for (uint32_t i = 0; i < 4U; ++i) {
        ch.Channel      = chans[i];
        ch.Rank         = i + 1U;
        ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;
        if (HAL_ADC_ConfigChannel(&hadc1, &ch) != HAL_OK) Error_Handler();
    }
}

/* USART2 on PA2/PA3.  Only useful if you have a USB-serial adapter; it is
 * harmless if nothing is connected. */
static void MX_USART2_UART_Init(void)
{
    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = 921600U;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_8;
    if (HAL_UART_Init(&huart2) != HAL_OK) Error_Handler();
}

/* --------------------------------------------------------------------------
 *  Failure indicators.  Plain busy-wait loops, because if we got here the
 *  timers and interrupts cannot be trusted.
 * ------------------------------------------------------------------------*/
static void spin(volatile uint32_t n) { while (n--) { __NOP(); } }

static void leds_raw(uint16_t on_mask)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIOD->MODER = (GPIOD->MODER & ~(0xFFUL << 24)) | (0x55UL << 24); /* PD12-15 out */
    GPIOD->BSRR  = ((uint32_t)ALL_LEDS << 16);
    GPIOD->BSRR  = on_mask;
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) {                                   /* red, fast blink */
        leds_raw(LED_RED_PIN);  spin(1500000UL);
        leds_raw(0U);           spin(1500000UL);
    }
}

void board_crash_blink(void)
{
    __disable_irq();
    for (;;) {                                   /* all four, slow blink */
        leds_raw(ALL_LEDS);     spin(6000000UL);
        leds_raw(0U);           spin(6000000UL);
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file; (void)line;
    Error_Handler();
}
#endif
