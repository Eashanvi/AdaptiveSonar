/* ============================================================================
 *  stm32f4xx_hal_msp.c  -  the pin and DMA wiring behind each peripheral.
 *  Bring-up build (v3, 25 Sept 2026).  REPLACES the CubeMX-generated file.
 *
 *  The HAL calls these automatically from HAL_xxx_Init().  They switch on the
 *  peripheral clock, put the pins in the right mode, and connect the DMA.
 * ==========================================================================*/
#include "main.h"

extern DMA_HandleTypeDef hdma_dac1;
extern DMA_HandleTypeDef hdma_adc1;

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

/* --------------------------------------------------------------------------
 *  DAC channel 1 -> PA4 (analog).
 *  DMA1 Stream5 Channel7, memory -> DAC, 16 bit, CIRCULAR.
 *  Circular is not optional: in Normal mode the buffer plays exactly once.
 * ------------------------------------------------------------------------*/
void HAL_DAC_MspInit(DAC_HandleTypeDef *hdac)
{
    GPIO_InitTypeDef g = {0};
    if (hdac->Instance != DAC) return;

    __HAL_RCC_DAC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    g.Pin  = GPIO_PIN_4;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    hdma_dac1.Instance                 = DMA1_Stream5;
    hdma_dac1.Init.Channel             = DMA_CHANNEL_7;
    hdma_dac1.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_dac1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_dac1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_dac1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_dac1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_dac1.Init.Mode                = DMA_CIRCULAR;
    hdma_dac1.Init.Priority            = DMA_PRIORITY_VERY_HIGH;
    hdma_dac1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_dac1) != HAL_OK) Error_Handler();

    __HAL_LINKDMA(hdac, DMA_Handle1, hdma_dac1);
}

void HAL_DAC_MspDeInit(DAC_HandleTypeDef *hdac)
{
    if (hdac->Instance != DAC) return;
    __HAL_RCC_DAC_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_4);
    HAL_DMA_DeInit(hdac->DMA_Handle1);
}

/* --------------------------------------------------------------------------
 *  ADC1 -> PC1, PC2, PC4, PC5 (analog).
 *  DMA2 Stream0 Channel0, ADC -> memory, 16 bit, CIRCULAR.
 * ------------------------------------------------------------------------*/
void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc)
{
    GPIO_InitTypeDef g = {0};
    if (hadc->Instance != ADC1) return;

    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    g.Pin  = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &g);

    hdma_adc1.Instance                 = DMA2_Stream0;
    hdma_adc1.Init.Channel             = DMA_CHANNEL_0;
    hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode                = DMA_CIRCULAR;
    hdma_adc1.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_adc1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK) Error_Handler();

    __HAL_LINKDMA(hadc, DMA_Handle, hdma_adc1);
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance != ADC1) return;
    __HAL_RCC_ADC1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_4 | GPIO_PIN_5);
    HAL_DMA_DeInit(hadc->DMA_Handle);
}

/* TIM6 has no pins.  Just its clock. */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) __HAL_RCC_TIM6_CLK_ENABLE();
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) __HAL_RCC_TIM6_CLK_DISABLE();
}

/* USART2 -> PA2 (TX), PA3 (RX). */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef g = {0};
    if (huart->Instance != USART2) return;

    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    g.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) return;
    __HAL_RCC_USART2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
}
