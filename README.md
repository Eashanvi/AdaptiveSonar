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
              +--------+--------+
              |        |        |
              v        v        v
             LFM     BARKER   GEOMETRIC
              |        |        |
              +--------+--------+
                       |
                       v
              +-------------------+
              | Signal Analysis   |
              +-------------------+
                       |
              +--------+--------+
              |        |        |
              v        v        v
             FFT  Autocorrelation Spectrogram
                       |
                       v
              +-------------------+
              | Python Dashboard  |
              +-------------------+             +-------------------+

# 3. Environment Classification

Phase 1 currently evaluates three representative underwater environments.

## Shallow / Low Turbidity

Example:

```text
Depth       : 20 m
Turbidity   : 10 %
Temperature : 20 °C
Salinity    : 35 PSU
```

Classification:

```text
Depth State     : SHALLOW
Turbidity State : LOW
```

Adaptive parameters:

```text
Start Frequency : 300 kHz
End Frequency   : 500 kHz
Pulse Duration  : 1.60 ms
Amplitude       : 0.60
Waveform        : BARKER
```

---

## Medium / Medium Turbidity

Example:

```text
Depth       : 80 m
Turbidity   : 60 %
Temperature : 20 °C
Salinity    : 35 PSU
```

Classification:

```text
Depth State     : MEDIUM
Turbidity State : MEDIUM
```

Adaptive parameters:

```text
Start Frequency : 200 kHz
End Frequency   : 400 kHz
Pulse Duration  : 3.00 ms
Amplitude       : 0.70
Waveform        : LFM
```

---

## Deep / High Turbidity

Example:

```text
Depth       : 150 m
Turbidity   : 90 %
Temperature : 20 °C
Salinity    : 35 PSU
```

Classification:

```text
Depth State     : DEEP
Turbidity State : HIGH
```

Adaptive parameters:

```text
Start Frequency : 100 kHz
End Frequency   : 250 kHz
Pulse Duration  : 7.50 ms
Amplitude       : 1.00
Waveform        : GEOMETRIC
```

---

# 4. Adaptive Waveform Selection

The current Phase 1 decision logic uses environmental conditions to select the waveform.

| Environment     | Waveform        |
| --------------- | --------------- |
| Shallow / Low   | Barker-13       |
| Medium / Medium | LFM             |
| Deep / High     | Geometric Sweep |

The waveform selection demonstrates the core concept of **environment-aware sonar transmission**.

Future phases can replace the rule-based selection with a more advanced optimization or machine-learning-based decision engine.

---

# 5. Waveforms Implemented

## LFM

Linear Frequency Modulation sweeps the carrier frequency linearly between the configured start and end frequencies.

Example:

```text
Start Frequency : 200 kHz
End Frequency   : 400 kHz
Pulse Duration  : 3 ms
```

The LFM waveform is analyzed using:

* Time-domain waveform
* Instantaneous frequency
* FFT
* Spectrogram
* Autocorrelation

---

## Barker-13

A 13-bit Barker phase-coded waveform is implemented for pulse-compression analysis.

The Barker sequence provides good autocorrelation characteristics with a strong central peak and relatively low sidelobes.

Example:

```text
Barker Code Length : 13
```

The autocorrelation response is evaluated as part of Phase 1 performance analysis.

---

## Geometric Frequency Sweep

The geometric waveform performs a nonlinear frequency sweep.

It is currently used for the deep/high-turbidity operating condition.

Example:

```text
Start Frequency : 100 kHz
End Frequency   : 250 kHz
Pulse Duration  : 7.5 ms
```

---

# 6. Signal Analysis

Phase 1 performs several signal-processing operations.

## FFT Analysis

The frequency spectrum of the generated signal is calculated to examine its occupied frequency range.

## Window Comparison

The LFM spectrum is compared using:

* Original signal
* Hamming window
* Hann window
* Blackman window

This allows investigation of spectral leakage and sidelobe behavior.

## Autocorrelation

Autocorrelation is calculated to evaluate pulse-compression and correlation characteristics.

## Spectrogram

A time-frequency spectrogram is generated to visualize the frequency evolution of the sonar waveform.

---

# 7. Phase 1B Performance Evaluation

The system evaluates the following performance parameters:

* Bandwidth
* Pulse duration
* Peak amplitude
* Signal energy
* Autocorrelation characteristics

Example results:

| Environment | Waveform | Bandwidth | Duration | Peak Amp | Energy |
| ----------- | -------- | --------: | -------: | -------: | -----: |
| 20 m / 10%  | Barker-13 |   200 kHz |  1.60 ms |     0.60 |   1440 |
| 80 m / 60%  | LFM      |   200 kHz |  3.00 ms |     0.70 |   3675 |
| 150 m / 90% | Geometric |   150 kHz |  7.50 ms |     1.00 |  18750 |

The evaluation demonstrates that the adaptive system modifies transmission characteristics as the environmental conditions change.

---

# 8. MATLAB Signal Engine

MATLAB is responsible for the core signal-processing pipeline.

The current implementation is compatible with:

```text
MATLAB 2016+
```

The MATLAB engine performs:

```text
Environment Input
        ↓
Environment Classification
        ↓
Parameter Adaptation
        ↓
Waveform Selection
        ↓
Waveform Generation
        ↓
Signal Analysis
        ↓
Performance Evaluation
        ↓
Results Export
```

---

# 9. Python Visualization Dashboard

A Python dashboard is included for visualizing the results generated by MATLAB.

The dashboard displays:

* Environment parameters
* Environment classification
* Adaptive parameters
* Selected waveform
* Number of samples
* Sampling rate
* Peak amplitude
* Sonar waveform
* Instantaneous frequency
* Frequency spectrum
* Autocorrelation

The current architecture is:

```text
MATLAB
   |
   | phase1_results.mat
   v
Python Dashboard
   |
   v
Visualization
```

The dashboard is primarily a **visualization layer**, while MATLAB remains the main signal-generation and signal-processing engine for Phase 1.

---

# 10. Repository Structure

```text
AdaptiveSonar/
│
├── Dashboard/
│   ├── dashboard.py
│   └── ...
│
├── Figures/
│   └── ...
│
├── MATLAB/
│   ├── main.m
│   ├── environment_model.m
│   ├── adaptation.m
│   ├── lfm_generator.m
│   ├── barker_generator.m
│   ├── geometric_generator.m
│   ├── fft_analysis.m
│   ├── windowing.m
│   └── ...
│
├── .gitignore
│
└── README.md
```

The exact contents may evolve as the project progresses.

---

# 11. Running the MATLAB Simulation

Open MATLAB and navigate to the project MATLAB directory.

Run:

```matlab
main
```

The program will:

1. Load the environmental parameters.
2. Classify the environment.
3. Calculate adaptive sonar parameters.
4. Select a waveform.
5. Generate the waveform.
6. Perform signal analysis.
7. Display performance results.
8. Export Phase 1 results for the dashboard.

---

# 12. Running the Dashboard

The dashboard requires Python.

Install the required packages:

```bash
pip install streamlit scipy numpy matplotlib
```

Navigate to the Dashboard directory:

```bash
cd Dashboard
```

Run:

```bash
streamlit run dashboard.py
```

The dashboard will normally be available at:

```text
http://localhost:8501
```

The dashboard reads the MATLAB-generated results file.

---

# 13. MATLAB → Python Data Pipeline

The Phase 1 integration uses a MATLAB `.mat` file as the interface between the MATLAB signal engine and the Python dashboard.

```text
MATLAB Simulation
       |
       v
phase1_results.mat
       |
       v
Python / Streamlit
       |
       v
Visualization Dashboard
```

This separation allows the signal-processing engine and visualization layer to evolve independently.

---

# 14. Important Design Principle

The project follows a modular architecture.

## MATLAB

Responsible for:

* Signal generation
* Environment modeling
* Adaptive parameter calculation
* Waveform selection
* Signal processing
* Performance analysis
* Result export

## Python

Responsible for:

* Visualization
* Dashboard
* Result presentation
* Future monitoring interfaces

This separation will make it easier to integrate hardware and FPGA components in later phases.

---

# 15. Future Development

## Phase 2 - FPGA / Hardware Integration

Planned work includes:

* FPGA-based waveform generation
* Hardware implementation of signal-processing blocks
* Fixed-point conversion
* FPGA verification
* Real-time signal generation
* Hardware/software co-design
* Interface between MATLAB/Python and FPGA
* Hardware-in-the-loop testing

Potential FPGA targets will be evaluated during Phase 2.

---

## Phase 3 - Advanced Adaptive Sonar

Future development may include:

* Real-time environmental sensing
* Automatic waveform optimization
* Target detection
* Noise estimation
* Signal-to-noise ratio estimation
* Adaptive bandwidth selection
* Adaptive pulse duration
* Real-time waveform switching
* Machine-learning-based waveform selection

---

# 16. Team Development

The repository is designed to support collaborative development.

Future phases will be developed using separate Git branches.

Recommended workflow:

```text
main
 |
 +-- phase-2
       |
       +-- teammate-feature-1
       |
       +-- teammate-feature-2
       |
       +-- teammate-feature-3
```

Team members should avoid directly modifying the `main` branch.

Recommended workflow:

```bash
git pull origin main

git checkout -b phase2-your-feature

# Make changes

git add .

git commit -m "Add Phase 2 feature"

git push origin phase2-your-feature
```

The branch can then be merged into the appropriate development branch through a Pull Request.

---

# 17. Current Phase 1 Demonstration

Phase 1 successfully demonstrates:

```text
Environmental Conditions
          ↓
Environment Classification
          ↓
Adaptive Parameter Calculation
          ↓
Waveform Selection
          ↓
Waveform Generation
          ↓
Signal Processing
          ↓
Performance Evaluation
          ↓
MATLAB Results
          ↓
Python Dashboard
```

The three tested environments demonstrate different adaptive operating conditions:

```text
SHALLOW / LOW
       ↓
    BARKER


MEDIUM / MEDIUM
       ↓
      LFM


DEEP / HIGH
       ↓
   GEOMETRIC
```

---

# 18. Phase 1 Status

## PHASE 1 MATLAB POC: COMPLETE

Implemented:

* ✅ Environment model
* ✅ Environment classification
* ✅ Adaptive parameter engine
* ✅ LFM generation
* ✅ Barker-13 generation
* ✅ Geometric frequency sweep
* ✅ FFT analysis
* ✅ Window comparison
* ✅ Autocorrelation analysis
* ✅ Spectrogram analysis
* ✅ Performance evaluation
* ✅ MATLAB result export
* ✅ Python visualization dashboard
* ✅ Three environmental test cases

---

## Author

**Eashanvi**

Adaptive Sonar Project
Phase 1 Proof of Concept

---

## License

This project is currently intended for academic and research purposes.
