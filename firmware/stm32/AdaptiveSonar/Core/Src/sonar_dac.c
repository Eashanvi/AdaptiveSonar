/* ============================================================================
 *  sonar_dac.c  -  TIM6 -> DAC1 -> DMA1_Stream5, ping-pong, CPU-free.
 *
 *  Version 3 (25 Sept 2026, bring-up build).
 *
 *  Keeps both oscilloscope fixes from version 2:
 *   1. The DAC is switched on once at boot and never switched off, so PA4
 *      rests at mid-scale (about 1.65 V) between pings instead of 0 V.
 *   2. The half-buffer holding the end of the pulse is allowed to finish
 *      playing, so long pulses are no longer cut short.
 *  New in version 3:
 *   3. The DMA is stopped with plain register writes instead of
 *      HAL_DMA_Abort(), which is not meant to be called from inside an
 *      interrupt.
 *   4. hw_dac_force_stop() so the main loop can recover if a pulse ever
 *      fails to finish, instead of going silent forever.
 *
 *  THE IDEA
 *  --------
 *  One circular buffer, split in half.  The DMA walks the buffer continuously
 *  and raises an interrupt at the halfway point and at the end.  Each interrupt
 *  refills the half the DMA is NOT currently reading.  The CPU wakes 2 times
 *  every 1024 samples instead of 1024 times.  Between refills the core is in
 *  WFI - that is where the low-power claim comes from.
 * ==========================================================================*/
#include "stm32f4xx_hal.h"
#include "sonar_hw.h"
#include "board.h"
#include "dds.h"

extern DAC_HandleTypeDef hdac;
extern TIM_HandleTypeDef htim6;

static uint16_t s_buf[DAC_FULL_BUF];
static dds_t    s_dds;
static volatile bool     s_busy  = false;
static volatile uint32_t s_sent  = 0U;
static volatile uint32_t s_drain = 0U;

/* How many pulses have gone out.  Type g_pulses_sent into Live Expressions:
 * if it counts up, the DAC path is alive. */
volatile uint32_t g_pulses_sent = 0U;

/* --------------------------------------------------------------------------
 *  Boot: switch the DAC on, park it at mid-scale, and leave it on forever.
 * ------------------------------------------------------------------------*/
void hw_dac_init(void)
{
    dds_init();
    for (uint32_t i = 0; i < DAC_FULL_BUF; ++i) s_buf[i] = DAC_MID;

    HAL_DAC_Start(&hdac, SONAR_DAC_CHANNEL);
    HAL_DAC_SetValue(&hdac, SONAR_DAC_CHANNEL, DAC_ALIGN_12B_R, DAC_MID);

    /* The DAC is triggered by TIM6, so the mid-scale value only reaches the
     * pin on the next timer tick.  Run TIM6 for 1 ms and stop it again.
     * From here on PA4 sits at about 1.65 V whenever nothing is transmitting.*/
    HAL_TIM_Base_Start(&htim6);
    HAL_Delay(1);
    HAL_TIM_Base_Stop(&htim6);
}

/* --------------------------------------------------------------------------
 *  Start one ping.
 * ------------------------------------------------------------------------*/
void hw_dac_start_pulse(const sonar_params_t *p)
{
    if (s_busy) return;

    dds_prepare(&s_dds, p);
    s_sent  = 0U;
    s_drain = 0U;

    /* Pre-fill BOTH halves before the DMA is armed. */
    s_sent += dds_fill_dac(&s_dds, &s_buf[0], DAC_HALF_BUF);
    s_sent += dds_fill_dac(&s_dds, &s_buf[DAC_HALF_BUF], DAC_HALF_BUF);

    s_busy = true;
    hw_trig_high();                 /* PD2 goes high: scope trigger */
    hw_led(LED_GREEN_PIN, true);    /* green = transmitting         */

    if (HAL_DAC_Start_DMA(&hdac, SONAR_DAC_CHANNEL,
                          (uint32_t *)s_buf, DAC_FULL_BUF,
                          DAC_ALIGN_12B_R) != HAL_OK) {
        hw_led(LED_RED_PIN, true);
        hw_trig_low();
        hw_led(LED_GREEN_PIN, false);
        s_busy = false;
        return;
    }
    HAL_TIM_Base_Start(&htim6);
    ++g_pulses_sent;
}

/* --------------------------------------------------------------------------
 *  Stop streaming WITHOUT switching the DAC off.
 * ------------------------------------------------------------------------*/
static void stop_now(void)
{
    DMA_HandleTypeDef *d = hdac.DMA_Handle1;

    HAL_TIM_Base_Stop(&htim6);                        /* no more ticks: pin freezes */
    CLEAR_BIT(hdac.Instance->CR, DAC_CR_DMAEN1);      /* DAC stops asking for data  */
    __HAL_DAC_DISABLE_IT(&hdac, DAC_IT_DMAUDR1);

    if (d != NULL) {
        __HAL_DMA_DISABLE_IT(d, DMA_IT_TC | DMA_IT_HT | DMA_IT_TE | DMA_IT_DME);
        __HAL_DMA_DISABLE(d);
        while ((d->Instance->CR & DMA_SxCR_EN) != 0U) { }   /* a few cycles */
        /* DMA1 Stream5 flags live in HIFCR: clear all five of them. */
        DMA1->HIFCR = DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTEIF5 |
                      DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CFEIF5;
        d->State = HAL_DMA_STATE_READY;
        __HAL_UNLOCK(d);
    }
    hdac.State = HAL_DAC_STATE_READY;
    __HAL_UNLOCK(&hdac);

    hw_trig_low();
    hw_led(LED_GREEN_PIN, false);
    s_busy = false;
}

void hw_dac_force_stop(void)
{
    if (s_busy) stop_now();
}

/* --------------------------------------------------------------------------
 *  One refill step, shared by both interrupts.  `done` is the half the DMA
 *  has just finished reading; the other half is playing right now.
 *    pulse still running -> refill `done` with the next samples
 *    pulse has finished  -> fill `done` with mid-scale and count it; after
 *                           the SECOND such half the tail has fully played.
 * ------------------------------------------------------------------------*/
static void refill(uint16_t *done)
{
    if (!s_busy) return;
    if (s_dds.active) {
        s_sent += dds_fill_dac(&s_dds, done, DAC_HALF_BUF);
        return;
    }
    for (uint32_t i = 0; i < DAC_HALF_BUF; ++i) done[i] = DAC_MID;
    if (++s_drain >= 2U) stop_now();
}

void hw_dac_on_half(void) { refill(&s_buf[0]); }
void hw_dac_on_full(void) { refill(&s_buf[DAC_HALF_BUF]); }

bool     hw_dac_busy(void)         { return s_busy; }
uint32_t hw_dac_samples_sent(void) { return s_sent; }

/* HAL callbacks - these replace the empty "weak" versions inside the HAL. */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *h) { (void)h; hw_dac_on_half(); }
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *h)     { (void)h; hw_dac_on_full(); }
void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef *h)
{
    (void)h;
    hw_led(LED_RED_PIN, true);
    stop_now();
}
