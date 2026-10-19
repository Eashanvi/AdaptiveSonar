/* ============================================================================
 *  sonar_adc.c  -  four potentiometers, scanned by hardware, filtered by DMA.
 *
 *  The CPU never polls a pot.  ADC1 is configured in scan mode over four
 *  channels, triggered by its own continuous-conversion mode, with DMA in
 *  circular mode writing straight into a ring.  The application just reads
 *  the latest values whenever it wants them.
 *
 *  ENV_ADC_OVERSAMPLE frames are averaged before the IIR filter in
 *  environment.c ever sees them.  The F407's ADC has no hardware
 *  oversampling block (that arrived on the L4/G4 families), so we do it in
 *  the DMA ring: allocate N frames, let DMA fill them round-robin, and
 *  average on read.  Costs nothing at 20 Hz and buys about two extra bits.
 * ==========================================================================*/
#include "stm32f4xx_hal.h"
#include "sonar_hw.h"
#include "board.h"

extern ADC_HandleTypeDef hadc1;

static volatile uint16_t s_ring[ENV_ADC_OVERSAMPLE][ENV_ADC_CHANNELS];
static uint16_t s_avg[ENV_ADC_CHANNELS];
static volatile bool s_ready = false;

void hw_adc_init(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)s_ring,
                      ENV_ADC_OVERSAMPLE * ENV_ADC_CHANNELS);
}

const uint16_t *hw_adc_raw(void)
{
    for (unsigned c = 0; c < ENV_ADC_CHANNELS; ++c) {
        uint32_t acc = 0;
        for (unsigned f = 0; f < ENV_ADC_OVERSAMPLE; ++f) acc += s_ring[f][c];
        s_avg[c] = (uint16_t)(acc / ENV_ADC_OVERSAMPLE);
    }
    return s_avg;
}

bool hw_adc_ready(void) { return s_ready; }

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h) { (void)h; s_ready = true; }
