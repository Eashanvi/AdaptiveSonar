/* ============================================================================
 *  board.h  -  STM32F407G-DISC1 pin map for the sonar transmitter payload.
 *
 *  Every pin below is free on a stock Discovery board.  PA0 is deliberately
 *  NOT used for a potentiometer even though it is the obvious first choice:
 *  on this board PA0 is the blue USER button, wired to a pull-down and a
 *  100 nF cap, and it will fight your pot divider.
 * ==========================================================================*/
#ifndef BOARD_H
#define BOARD_H

/* ---- analog out ---------------------------------------------------------
 * DAC1 channel 1 -> PA4.  Goes to the reconstruction filter, then the PA,
 * then the transducer (or a 50 ohm dummy load on the bench).            */
#define SONAR_DAC_PORT        GPIOA
#define SONAR_DAC_PIN         GPIO_PIN_4
#define SONAR_DAC_CHANNEL     DAC_CHANNEL_1

/* ---- environment pots ---------------------------------------------------
 * Four 10k linear pots, wiper to the pin, ends to 3V0 and GND.
 * A 100 nF cap from wiper to GND keeps the ADC sample-and-hold happy.
 *
 *   PC1  ADC123_IN11   depth          0 .. 300 m
 *   PC2  ADC123_IN12   turbidity      0 .. 100 %
 *   PC4  ADC12_IN14    temperature    0 .. 35 C
 *   PC5  ADC12_IN15    salinity       0 .. 40 PSU
 */
#define SONAR_ADC_CH_DEPTH    ADC_CHANNEL_11
#define SONAR_ADC_CH_TURB     ADC_CHANNEL_12
#define SONAR_ADC_CH_TEMP     ADC_CHANNEL_14
#define SONAR_ADC_CH_SAL      ADC_CHANNEL_15

/* ---- the blue USER button ----------------------------------------------
 * PA0, already on the board, already debounced by hardware, and the ONLY
 * input you have if you are programming over an external ST-LINK V2 with no
 * serial adapter attached.  Idle low, high while pressed (the board has its
 * own pull-down resistor, so configure the pin with No pull-up/pull-down).
 *
 * This is why board.h tells you not to hang a potentiometer off PA0.       */
#define SONAR_BTN_PORT        GPIOA
#define SONAR_BTN_PIN         GPIO_PIN_0

/* ---- scope trigger ------------------------------------------------------
 * Goes high for the exact duration of the transmitted pulse.  Feed it to
 * your scope's external trigger and every capture is stable and aligned.
 * This one wire is the difference between a clean demo and five minutes of
 * fiddling with trigger level in front of a judge.                      */
#define SONAR_TRIG_PORT       GPIOD
#define SONAR_TRIG_PIN        GPIO_PIN_2

/* ---- status LEDs (already on the Discovery board) ---------------------- */
#define LED_GREEN_PORT        GPIOD
#define LED_GREEN_PIN         GPIO_PIN_12   /* transmitting               */
#define LED_ORANGE_PIN        GPIO_PIN_13   /* adaptation changed state   */
#define LED_RED_PIN           GPIO_PIN_14   /* fault / clamp hit          */
#define LED_BLUE_PIN          GPIO_PIN_15   /* sleeping between pings     */

/* ---- telemetry ----------------------------------------------------------
 * USART2, PA2 = TX, PA3 = RX.  Any 3V3 USB-serial adapter.               */
#define SONAR_UART            USART2

/* ---- timing -------------------------------------------------------------
 * TIM6 triggers the DAC.  On this board APB1 runs at 42 MHz, so the APB1
 * TIMER clock is 84 MHz.  ARR = 84e6/fs - 1.                             */
#define TIM6_INPUT_CLK_HZ     84000000UL
#define TIM6_ARR_FOR_FS       ((TIM6_INPUT_CLK_HZ / SONAR_FS_HZ) - 1UL)

#endif /* BOARD_H */
