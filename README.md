# AdaptiveSonar

## Adaptive Sonar Signal Generation and Waveform Selection

AdaptiveSonar is a MATLAB-based Proof of Concept (POC) for an adaptive sonar system that dynamically selects sonar waveform parameters based on underwater environmental conditions.

The project currently implements **Phase 1**, including environmental classification, adaptive parameter selection, waveform generation, signal analysis, performance evaluation, and a Python-based visualization dashboard.

---

## Project Status

| Phase | Status |
|---|---|
| Phase 1 - Adaptive Sonar POC | ✅ Complete |
| Phase 1B - Performance Evaluation | ✅ Complete |
| Phase 1 Dashboard | ✅ Complete |
| Phase 2 - Hardware / FPGA Integration | 🔲 Planned |
| Phase 3 - Advanced Adaptive System | 🔲 Planned |

---

# 1. Project Objective

Conventional sonar systems often operate using fixed waveform parameters.

This project explores an adaptive approach where the sonar transmission parameters are modified according to the detected underwater environment.

The system takes environmental parameters such as:

- Water depth
- Turbidity
- Temperature
- Salinity

and classifies the environment into different operating conditions.

Based on the classification, the system automatically determines:

- Start frequency
- End frequency
- Pulse duration
- Signal amplitude
- Waveform type

The resulting waveform is then generated and analyzed.

---

# 2. Phase 1 Architecture

```text
             UNDERWATER ENVIRONMENT
                      |
                      v
             +-------------------+
             | Environment Model |
             +-------------------+
                      |
                      v
             +-------------------+
             | Classification    |
             |                   |
             | Depth             |
             | Turbidity         |
             +-------------------+
                      |
                      v
             +-------------------+
             | Adaptation Engine |
             +-------------------+
                      |
                      v
             +-------------------+
             | Waveform Decision |
             +-------------------+
                      |
          +-----------+-----------+
          |           |           |
          v           v           v
        LFM        BARKER     GEOMETRIC
          |           |           |
          +-----------+-----------+
                      |
                      v
             +-------------------+
             | Signal Analysis   |
             +-------------------+
                      |
          +-----------+-----------+
          |           |           |
          v           v           v
        FFT    Autocorrelation  Spectrogram
                      |
                      v
             +-------------------+
             | Python Dashboard  |
             +-------------------+
