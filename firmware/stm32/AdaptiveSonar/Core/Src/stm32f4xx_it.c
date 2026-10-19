/* ============================================================================
 *  stm32f4xx_it.c  -  interrupt handlers.
 *  Bring-up build (v3, 25 Sept 2026).  REPLACES the CubeMX-generated file.
 *
 *  Only four interrupts matter to the sonar:
 *    SysTick        - the 1 ms tick (HAL_Delay, HAL_GetTick, the sleep loop)
 *    DMA1 Stream5   - DAC buffer half-played / fully-played -> refill
 *    DMA2 Stream0   - ADC pot readings arrived
 *  and the crash handlers, which blink all four LEDs so a crash is obvious.
 *  v3.1: SysTick also samples the blue button every 1 ms (never misses a press).
 * ==========================================================================*/
#include "main.h"
#include "stm32f4xx_it.h"

extern DMA_HandleTypeDef hdma_dac1;
extern DMA_HandleTypeDef hdma_adc1;

void NMI_Handler(void)        { while (1) { } }
void HardFault_Handler(void)  { board_crash_blink(); }
void MemManage_Handler(void)  { board_crash_blink(); }
void BusFault_Handler(void)   { board_crash_blink(); }
void UsageFault_Handler(void) { board_crash_blink(); }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }

void sonar_button_tick(void);   /* sonar_app.c - reads the blue button every 1 ms */

void SysTick_Handler(void)
{
    HAL_IncTick();
    sonar_button_tick();
}

void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_dac1);
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}
