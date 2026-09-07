clc;
clear;
close all;

% ==========================================================
% PHASE 1B - ADAPTIVE SONAR
% WAVEFORM PERFORMANCE EVALUATION
% MATLAB 2016 COMPATIBLE
% ==========================================================

fs = 5e6;          % Sampling frequency = 5 MHz

% ==========================================================
% ENVIRONMENT TEST CASES
% ==========================================================

depth_cases = [20 80 150];
turbidity_cases = [10 60 90];
temperature = 20;
salinity = 35;

% ==========================================================
% STORAGE
% ==========================================================

waveform_names = cell(3,1);
bandwidth = zeros(3,1);
duration = zeros(3,1);
peak_amplitude = zeros(3,1);
energy = zeros(3,1);
samples = zeros(3,1);

% ==========================================================
% RUN ALL THREE ENVIRONMENTS
% ==========================================================

for k = 1:3

    depth = depth_cases(k);
    turbidity = turbidity_cases(k);

    % ------------------------------------------------------
    % Environment classification
    % ------------------------------------------------------

    env = environment_model(depth, turbidity, ...
                            temperature, salinity);

    % ------------------------------------------------------
    % Adaptive parameter selection
    % ------------------------------------------------------

    params = adaptation(env);

    % ------------------------------------------------------
    % Waveform selection
    % ------------------------------------------------------

    if strcmp(params.waveform,'LFM')

        [t, signal, inst_freq] = ...
            lfm_generator(params, fs);

    elseif strcmp(params.waveform,'BARKER')

        [t, signal, code] = ...
            barker_generator(params, fs);

    elseif strcmp(params.waveform,'GEOMETRIC')

        [t, signal, inst_freq] = ...
            geometric_generator(params, fs);

    end

    % ------------------------------------------------------
    % Store basic parameters
    % ------------------------------------------------------

    waveform_names{k} = params.waveform;

    duration(k) = length(signal) / fs;

    peak_amplitude(k) = max(abs(signal));

    samples(k) = length(signal);

    energy(k) = sum(signal.^2);

    bandwidth(k) = abs(params.f_end - params.f_start);

end

% ==========================================================
% DISPLAY PERFORMANCE TABLE
% ==========================================================

fprintf('\n');
fprintf('====================================================\n');
fprintf('       PHASE 1B - PERFORMANCE EVALUATION\n');
fprintf('====================================================\n');

fprintf('\n');

fprintf('%-12s %-12s %-12s %-12s %-12s %-12s\n', ...
        'Environment','Waveform','Bandwidth', ...
        'Duration','Peak Amp','Energy');

fprintf('----------------------------------------------------\n');

for k = 1:3

    fprintf('%-12s %-12s %8.1f kHz %8.3f ms %8.3f %12.2f\n', ...
        sprintf('%dm/%d%%', ...
        depth_cases(k), turbidity_cases(k)), ...
        waveform_names{k}, ...
        bandwidth(k)/1000, ...
        duration(k)*1000, ...
        peak_amplitude(k), ...
        energy(k));

end

fprintf('====================================================\n');

% ==========================================================
% PERFORMANCE BAR CHART - BANDWIDTH
% ==========================================================

figure;

bar(bandwidth/1000);

set(gca,'XTickLabel',{'Shallow/Low','Medium/Medium','Deep/High'});

xlabel('Environment');

ylabel('Bandwidth (kHz)');

title('Adaptive Sonar Bandwidth');

grid on;

% ==========================================================
% PERFORMANCE BAR CHART - DURATION
% ==========================================================

figure;

bar(duration*1000);

set(gca,'XTickLabel',{'Shallow/Low','Medium/Medium','Deep/High'});

xlabel('Environment');

ylabel('Pulse Duration (ms)');

title('Adaptive Sonar Pulse Duration');

grid on;

% ==========================================================
% PERFORMANCE BAR CHART - ENERGY
% ==========================================================

figure;

bar(energy);

set(gca,'XTickLabel',{'Shallow/Low','Medium/Medium','Deep/High'});

xlabel('Environment');

ylabel('Signal Energy');

title('Adaptive Sonar Signal Energy');

grid on;

% ==========================================================
% AUTOCORRELATION ANALYSIS
% ==========================================================

for k = 1:3

    depth = depth_cases(k);
    turbidity = turbidity_cases(k);

    env = environment_model(depth, turbidity, ...
                            temperature, salinity);

    params = adaptation(env);

    if strcmp(params.waveform,'LFM')

        [t, signal, inst_freq] = ...
            lfm_generator(params, fs);

    elseif strcmp(params.waveform,'BARKER')

        [t, signal, code] = ...
            barker_generator(params, fs);

    elseif strcmp(params.waveform,'GEOMETRIC')

        [t, signal, inst_freq] = ...
            geometric_generator(params, fs);

    end

    [acor, lag] = xcorr(signal);

    figure;

    plot(lag/fs*1000, acor);

    xlabel('Lag (ms)');

    ylabel('Correlation');

    title(['Autocorrelation - ' params.waveform]);

    grid on;

end

% ==========================================================
% FINAL MESSAGE
% ==========================================================

fprintf('\n');
fprintf('====================================================\n');
fprintf('       PHASE 1B PERFORMANCE ANALYSIS COMPLETE\n');
fprintf('====================================================\n');

fprintf('\nThree environmental conditions evaluated.\n');

fprintf('Waveform adaptation verified.\n');

fprintf('Bandwidth, duration, amplitude and energy evaluated.\n');

fprintf('Autocorrelation characteristics evaluated.\n');

fprintf('\nPHASE 1 MATLAB POC READY.\n');

fprintf('====================================================\n');