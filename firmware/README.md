# STM32 firmware: adaptive sonar transmitter

Board: STM32F407G-DISC1. IDE: STM32CubeIDE 1.19.0. Firmware version: v3.1 (26 Sep 2026).

## Folders

| Folder | What it is |
|---|---|
| `stm32/AdaptiveSonar/` | The CubeIDE project that runs on the board |
| `host_sim/` | The same decision and waveform C code, built for a PC, with a self-test and a WAV generator |
| `tools/` | `plot_results.py` (figures from the PC simulator) and `live_dashboard.py` (live plot of the board's UART output) |

## Build and flash

1. In CubeIDE: File → Import → Existing Projects into Workspace, and pick `firmware/stm32/AdaptiveSonar`.
2. Project → Clean, then Build. Expected result: 0 errors, 0 warnings.
3. Debug, then press **Resume (F8)**. All four LEDs light for 1 s at power-up.

**Do not regenerate code from `AdaptiveSonar.ioc`.** All peripheral setup is written by hand in `main.c` and `stm32f4xx_hal_msp.c`. Regenerating would overwrite it. The `.ioc` is kept only for reference.

## How it works

1. Four potentiometers set depth, turbidity, temperature and salinity. They are read through ADC1 on PC1, PC2, PC4 and PC5, with DMA.
2. `environment.c` turns the readings into depth and turbidity classes. It also computes sound speed (Mackenzie) and absorption (Francois–Garrison).
3. `adaptation.c` chooses the waveform (LFM, geometric, hyperbolic or Barker-13), the frequency band, the pulse length and the amplitude.
4. `dds.c` builds the windowed pulse.
5. TIM6 triggers DAC1 on PA4 at 200 kHz. DMA1 Stream5 feeds it from a ping-pong buffer, so the CPU only wakes at half and full transfer.

`sonar_config.h` sets `SONAR_BAND_AUDIO`. This scales every frequency by 1/32 at the DAC, so the 100–500 kHz plan appears on PA4 as 3.1–15.6 kHz, which a normal lab scope can show.

**Blue button modes:** each press advances the mode, and the LEDs flash to show which one is active.

| Flashes | Mode |
|---|---|
| 1 | TONE |
| 2 | NORMAL |
| 3 | FAST-REPEAT |
| 4 | NO-WINDOW |

**Telemetry:** USART2 at 921600 baud sends one `PING,...` line per pulse.

**Scope trigger:** PD2.

## PC simulator

```
cd firmware/host_sim
make test    # self-test of the decision logic
make wav     # reference WAV files in out/
make plots   # needs numpy + matplotlib
```
