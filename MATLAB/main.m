clc;
clear;
close all;

% ==========================================================
% PHASE 1 - ADAPTIVE SONAR
% COMPLETE MATLAB SIMULATION
% MATLAB 2016 compatible
% ==========================================================

% ----------------------------------------------------------
% Sampling frequency
% ----------------------------------------------------------

fs = 5e6;       % 5 MHz


% ==========================================================
% ENVIRONMENTAL CONDITIONS
% ==========================================================

depth = 20;
turbidity = 10;
temperature = 20;
salinity = 35;


% ==========================================================
% ENVIRONMENT MODEL
% ==========================================================

env = environment_model(depth, turbidity, temperature, salinity);


% ==========================================================
% ADAPTATION ENGINE
% ==========================================================

params = adaptation(env);


% ----------------------------------------------------------
% Adaptive waveform selection
% ----------------------------------------------------------

selected_waveform = waveform_selector(env);
params.waveform = selected_waveform;

% ==========================================================
% LFM WAVEFORM GENERATION
% ==========================================================

[t, signal, inst_freq] = lfm_generator(params, fs);


% ==========================================================
% GEOMETRIC FREQUENCY SWEEP
% ==========================================================

[t_geo, geo_signal, geo_freq] = ...
    geometric_generator(params, fs);


% ==========================================================
% BARKER-13 PHASE-CODED WAVEFORM
% ==========================================================

[t_barker, barker_signal, code] = ...
    barker_generator(params, fs);


% ==========================================================
% STEP 1 - LFM WAVEFORM
% ==========================================================

figure;

plot(t*1000, signal);

xlabel('Time (ms)');
ylabel('Amplitude');

title('Adaptive LFM Sonar Waveform');

grid on;


% ==========================================================
% STEP 2 - LFM INSTANTANEOUS FREQUENCY
% ==========================================================

figure;

plot(t*1000, inst_freq/1000);

xlabel('Time (ms)');
ylabel('Frequency (kHz)');

title('LFM Instantaneous Frequency');

grid on;


% ==========================================================
% STEP 3 - LFM FFT
% ==========================================================

[f, magnitude] = fft_analysis(signal, fs);

figure;

plot(f/1000, magnitude);

xlabel('Frequency (kHz)');
ylabel('Magnitude');

title('LFM Frequency Spectrum');

grid on;

xlim([0 600]);


% ==========================================================
% STEP 4 - LFM SPECTROGRAM
% ==========================================================

figure;

window_length = 1024;
overlap = 768;
nfft = 2048;

spectrogram(signal, window_length, overlap, nfft, fs, 'yaxis');

title('LFM Sonar Spectrogram');

ylim([0 0.5]);


% ==========================================================
% STEP 5 - WINDOWING
% ==========================================================

[signal_hamming, signal_hann, signal_blackman] = ...
    windowing(signal);


% ==========================================================
% STEP 6 - FFT OF WINDOWED SIGNALS
% ==========================================================

[f, mag_original] = fft_analysis(signal, fs);

[~, mag_hamming] = fft_analysis(signal_hamming, fs);

[~, mag_hann] = fft_analysis(signal_hann, fs);

[~, mag_blackman] = fft_analysis(signal_blackman, fs);


% ==========================================================
% STEP 7 - WINDOW COMPARISON
% ==========================================================

figure;

plot(f/1000, mag_original);

hold on;

plot(f/1000, mag_hamming);
plot(f/1000, mag_hann);
plot(f/1000, mag_blackman);

xlabel('Frequency (kHz)');
ylabel('Magnitude');

title('LFM Spectrum - Window Comparison');

legend('Original', 'Hamming', 'Hann', 'Blackman');

grid on;

xlim([50 300]);

hold off;


% ==========================================================
% STEP 8 - GEOMETRIC SWEEP WAVEFORM
% ==========================================================

figure;

plot(t_geo*1000, geo_signal);

xlabel('Time (ms)');
ylabel('Amplitude');

title('Geometric Frequency Sweep');

grid on;


% ==========================================================
% STEP 9 - GEOMETRIC SWEEP FREQUENCY
% ==========================================================

figure;

plot(t_geo*1000, geo_freq/1000);

xlabel('Time (ms)');
ylabel('Frequency (kHz)');

title('Geometric Sweep Instantaneous Frequency');

grid on;


% ==========================================================
% STEP 10 - LFM VS GEOMETRIC FREQUENCY
% ==========================================================

figure;

plot(t*1000, inst_freq/1000);

hold on;

plot(t_geo*1000, geo_freq/1000);

xlabel('Time (ms)');
ylabel('Frequency (kHz)');

title('LFM vs Geometric Frequency Sweep');

legend('LFM', 'Geometric');

grid on;

hold off;


% ==========================================================
% STEP 11 - BARKER-13 WAVEFORM
% ==========================================================

figure;

plot(t_barker*1000, barker_signal);

xlabel('Time (ms)');
ylabel('Amplitude');

title('Barker-13 Phase-Coded Pulse');

grid on;


% ==========================================================
% STEP 12 - BARKER AUTOCORRELATION
% ==========================================================

[acor, lag] = xcorr(barker_signal);

figure;

plot(lag/fs*1000, acor);

xlabel('Lag (ms)');
ylabel('Correlation');

title('Barker-13 Autocorrelation');

grid on;


% ==========================================================
% DISPLAY PARAMETERS
% ==========================================================

fprintf('\n');
fprintf('========================================\n');
fprintf('       ADAPTIVE SONAR PHASE 1\n');
fprintf('========================================\n');

fprintf('Depth        : %.1f m\n', env.depth);

fprintf('Turbidity    : %.1f %%\n', env.turbidity);

fprintf('Temperature  : %.1f C\n', env.temperature);

fprintf('Salinity     : %.1f PSU\n', env.salinity);


fprintf('\nEnvironment Classification\n');

fprintf('Depth State  : %s\n', env.depth_state);

fprintf('Turbidity    : %s\n', env.turbidity_state);


fprintf('\nAdapted Parameters\n');

fprintf('Start Freq   : %.1f kHz\n', ...
    params.f_start/1000);

fprintf('End Freq     : %.1f kHz\n', ...
    params.f_end/1000);

fprintf('Pulse        : %.2f ms\n', ...
    params.pulse_duration*1000);

fprintf('Amplitude    : %.2f\n', ...
    params.amplitude);

fprintf('Waveform     : %s\n', ...
    params.waveform);


fprintf('\nSimulation Parameters\n');

fprintf('Sampling Freq: %.1f MHz\n', ...
    fs/1e6);

fprintf('Samples      : %d\n', ...
    length(signal));

fprintf('\nWaveform Selection\n');
fprintf('Selected Waveform : %s\n', selected_waveform);

fprintf('========================================\n');

% ==========================================================
% EXPORT PHASE 1 DATA FOR PYTHON DASHBOARD
% ==========================================================

depth = env.depth;
turbidity = env.turbidity;
temperature = env.temperature;
salinity = env.salinity;

depth_state = env.depth_state;
turbidity_state = env.turbidity_state;

f_start = params.f_start;
f_end = params.f_end;
pulse_duration = params.pulse_duration;
amplitude = params.amplitude;
waveform = params.waveform;

% ----------------------------------------------------------
% Save Phase 1 Results
% ----------------------------------------------------------

% Create a writable results directory
base_dir = userpath;

% Remove trailing separator if present
if base_dir(end) == pathsep
    base_dir = base_dir(1:end-1);
end

results_dir = fullfile(base_dir, 'AdaptiveSonarResults');

% Create directory if it does not exist
if ~exist(results_dir, 'dir')
    mkdir(results_dir);
end

% Full output file path
results_file = fullfile(results_dir, 'phase1_results.mat');

% Save Phase 1 data
save(results_file, ...
    'env', ...
    'params', ...
    't', ...
    'signal', ...
    'inst_freq', ...
    't_geo', ...
    'geo_signal', ...
    'geo_freq', ...
    't_barker', ...
    'barker_signal', ...
    'code', ...
    'acor', ...
    'lag', ...
    'f', ...
    'magnitude');

fprintf('\nPhase 1 results saved successfully.\n');
fprintf('Results file:\n%s\n', results_file);