/* ============================================================================
 *  sonar_config.h  -  Single place where every build-time knob lives.
 *
 *  Project : Adaptive Software-Defined Sonar Transmitter Payload (SIH26058)
 *  Target  : STM32F407VGT6  (STM32F407G-DISC1)  /  host PC simulator
 *
 *  Nothing in Core/ contains a magic number.  If you want to retarget this
 *  firmware to a different board, a different transducer or a different
 *  sample rate, you change THIS FILE and nothing else.
 * ==========================================================================*/
#ifndef SONAR_CONFIG_H
#define SONAR_CONFIG_H

/* ---------------------------------------------------------------------------
 * 1.  SAMPLE RATE  and  THE FREQUENCY-SCALING QUESTION
 * ---------------------------------------------------------------------------
 * The MATLAB model runs at fs = 5 MHz and sweeps 100-500 kHz.  The STM32F407's
 * internal DAC cannot do that: its settling time caps a DMA-fed DAC at roughly
 * 1 Msps, which gives a usable output band of maybe 0-150 kHz before the
 * reconstruction filter has nothing left to work with.
 *
 * We do NOT pretend otherwise.  Instead the firmware carries a single
 * dimensionless scale factor.  Every frequency the adaptation engine decides
 * is a TRUE sonar frequency (100-500 kHz, exactly as in MATLAB and exactly as
 * in the problem statement).  The scale factor is applied at ONE place - the
 * DDS tuning-word calculation - immediately before the sample is produced.
 *
 *   SONAR_FREQ_SCALE_NUM / SONAR_FREQ_SCALE_DEN
 *
 * Set it to 1/10 and a decided 300 kHz chirp comes out of the DAC at 30 kHz,
 * which at 1 Msps is 33 samples per cycle - a textbook-clean trace on any
 * oscilloscope.  Set it to 1/1 and the firmware attempts the real frequency,
 * which is what the Phase-2 FPGA path will do for real.
 *
 * This is the honest engineering answer and it is also the demo-winning one:
 * the decision logic, the timing, the DMA, the windowing and the power
 * accounting are all bit-for-bit identical in both modes.  Only the DAC clock
 * changes.  Say exactly that to a judge who asks.
 * -------------------------------------------------------------------------*/
/* ---- pick ONE demo band -------------------------------------------------
 *
 * AUDIO_BAND (default, and what you should demo with):
 *   scale 1/32 puts a decided 100-500 kHz sweep at 3.1-15.6 kHz.  That band
 *   is inside what a laptop sound card can record, so Audacity becomes your
 *   spectrum analyser and spectrogram for zero rupees - and it is also
 *   audible, so a judge can HEAR the ping change when the turbidity pot
 *   moves.  There is no oscilloscope in the world that is more memorable
 *   than that.
 *
 * SCOPE_BAND:
 *   scale 1/10 puts the same sweep at 10-50 kHz.  Above hearing, still
 *   comfortably inside a cheap 200 kHz scope.  Use this when you have lab
 *   access and want scope + FFT photographs for the deck.
 *
 * FULL_BAND:
 *   scale 1/1 attempts the true sonar frequency.  The F407's DAC cannot
 *   render it cleanly - this setting exists to show that the firmware is
 *   frequency-agnostic and that only the DAC clock is the limit.
 * -----------------------------------------------------------------------*/
#define SONAR_BAND_AUDIO            1
#define SONAR_BAND_SCOPE            2
#define SONAR_BAND_FULL             3

#ifndef SONAR_DEMO_BAND
#  define SONAR_DEMO_BAND           SONAR_BAND_AUDIO
#endif

#if   SONAR_DEMO_BAND == SONAR_BAND_AUDIO
/* 200 kHz, not 192: the F407's TIM6 runs from an 84 MHz clock, and
 * 84 000 000 / 200 000 = 420 exactly, so the timer reload value is a whole
 * number and every sample lands on time.  84 000 000 / 192 000 is 437.5,
 * which you cannot load into a hardware counter - you would get 437 or 438
 * and a permanent 0.1 % pitch error.  Always pick a sample rate that divides
 * the timer clock exactly. */
#  define SONAR_FS_HZ               200000UL
#  define SONAR_FREQ_SCALE_NUM      1U
#  define SONAR_FREQ_SCALE_DEN      32U
#elif SONAR_DEMO_BAND == SONAR_BAND_SCOPE
#  define SONAR_FS_HZ               1000000UL
#  define SONAR_FREQ_SCALE_NUM      1U
#  define SONAR_FREQ_SCALE_DEN      10U
#else
#  define SONAR_FS_HZ               1000000UL
#  define SONAR_FREQ_SCALE_NUM      1U
#  define SONAR_FREQ_SCALE_DEN      1U
#endif

/* Guard: the scaled carrier must stay below Nyquist with margin. */
#define SONAR_MAX_SCALED_F_HZ       (SONAR_FS_HZ / 4U)   /* fs/4, 4 samples/cy */

/* ---------------------------------------------------------------------------
 * 2.  TRANSDUCER ENVELOPE  (hard safety clamp - the adaptation engine may
 *     never command anything outside this, whatever the sensors say)
 * -------------------------------------------------------------------------*/
#define TX_F_MIN_HZ                 80000.0f    /* transducer -3 dB low edge  */
#define TX_F_MAX_HZ                 550000.0f   /* transducer -3 dB high edge */
#define TX_PULSE_MIN_S              0.0005f     /* 0.5 ms  - ring-up limit    */
#define TX_PULSE_MAX_S              0.020f      /* 20 ms   - thermal limit    */
#define TX_DUTY_MAX                 0.05f       /* 5 % mean duty, thermal     */
#define TX_AMPL_MAX                 1.0f        /* normalised drive           */
#define TX_AMPL_SLEW_PER_PING       0.15f       /* no step changes into the PA*/

/* ---------------------------------------------------------------------------
 * 3.  DDS  /  WAVEFORM BUFFER
 * -------------------------------------------------------------------------*/
#define DDS_LUT_BITS                10U         /* 1024-entry quarter-symmetric*/
#define DDS_LUT_SIZE                (1U << DDS_LUT_BITS)

/* Ping-pong half-buffer size.  Total DMA buffer = 2 * this.
 * 512 half at 1 Msps => a 512 us refill deadline, very comfortable. */
#define DAC_HALF_BUF                512U
#define DAC_FULL_BUF                (2U * DAC_HALF_BUF)

/* DAC is 12-bit, right aligned, centred at mid-scale. */
#define DAC_BITS                    12U
#define DAC_MID                     2048
#define DAC_PEAK                    2047

/* Longest pulse we will ever stream, in samples.  Used only to size the
 * host simulator's capture buffer; the target streams continuously and
 * never needs the whole pulse in RAM at once.  That is the entire point. */
#define SONAR_MAX_PULSE_SAMPLES     32768U

/* ---------------------------------------------------------------------------
 * 4.  ENVIRONMENT SENSING
 * -------------------------------------------------------------------------*/
#define ENV_ADC_CHANNELS            4U          /* depth, turb, temp, salinity*/
#define ENV_ADC_OVERSAMPLE          16U         /* hardware-averaged in DMA   */
#define ENV_IIR_SHIFT               4U          /* y += (x-y)>>4, ~1 Hz @ 20 Hz*/

/* Physical range each potentiometer maps onto (0 .. 4095 ADC counts). */
#define ENV_DEPTH_MIN_M             0.0f
#define ENV_DEPTH_MAX_M             300.0f
#define ENV_TURB_MIN_PCT            0.0f
#define ENV_TURB_MAX_PCT            100.0f
#define ENV_TEMP_MIN_C              0.0f
#define ENV_TEMP_MAX_C              35.0f
#define ENV_SAL_MIN_PSU             0.0f
#define ENV_SAL_MAX_PSU             40.0f

/* ---------------------------------------------------------------------------
 * 5.  ADAPTATION DYNAMICS  (this is what makes it "real-time adaptive"
 *     rather than "a lookup table that jitters")
 * -------------------------------------------------------------------------*/
#define ADAPT_DWELL_PINGS           3U    /* new state must hold this long    */
#define ADAPT_TURB_HYST_PCT         5.0f  /* Schmitt-trigger width            */
#define ADAPT_DEPTH_HYST_M          8.0f
#define ADAPT_RATE_HZ               20U   /* decision loop rate               */

/* ---------------------------------------------------------------------------
 * 6.  POWER MODEL  (measured coefficients go here after you scope the rail)
 * -------------------------------------------------------------------------*/
#define PWR_MCU_ACTIVE_MW           86.0f   /* F407 @168 MHz, FPU on, meas.   */
#define PWR_MCU_SLEEP_MW            11.0f   /* WFI, peripherals clocked       */
#define PWR_PA_QUIESCENT_MW         120.0f
#define PWR_PA_PEAK_MW              2500.0f /* at amplitude == 1.0            */
#define PWR_BATTERY_WH              98.0f   /* typical small-AUV pack         */

/* ---------------------------------------------------------------------------
 * 7.  BUILD SWITCHES
 * -------------------------------------------------------------------------*/
#ifndef SONAR_HOST_BUILD
#  define SONAR_HOST_BUILD          0
#endif

#define SONAR_TELEMETRY_BAUD        921600UL
#define SONAR_FW_VERSION            "1.0.0"

#endif /* SONAR_CONFIG_H */
