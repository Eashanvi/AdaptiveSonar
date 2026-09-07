clc;
clear;
close all;

% ==========================================================
% ADAPTIVE SONAR - PHASE 1 FINAL POC
% MATLAB 2016 COMPATIBLE
% ==========================================================

% ----------------------------------------------------------
% 1. SELECT TEST ENVIRONMENT
% ----------------------------------------------------------

depth = 150;          % meters
turbidity = 90;       % percent
temperature = 20;     % Celsius
salinity = 35;        % PSU

fs = 5e6;             % Sampling frequency = 5 MHz


% ----------------------------------------------------------
% 2. ENVIRONMENT CLASSIFICATION
% ----------------------------------------------------------

env = environment_model(depth, turbidity, ...
                        temperature, salinity);


% ----------------------------------------------------------
% 3. PARAMETER ADAPTATION
% ----------------------------------------------------------

params = adaptation(env);


% ----------------------------------------------------------
% 4. WAVEFORM SELECTION
% ----------------------------------------------------------

selected_waveform = waveform_selector(env);

params.waveform = selected_waveform;


% ----------------------------------------------------------
% 5. DISPLAY DECISION
% ----------------------------------------------------------

fprintf('\n');
fprintf('====================================================\n');
fprintf('          ADAPTIVE SONAR - PHASE 1 POC\n');
fprintf('====================================================\n');

fprintf('\nENVIRONMENT\n');
fprintf('Depth        : %.1f m\n', depth);
fprintf('Turbidity    : %.1f %%\n', turbidity);
fprintf('Temperature  : %.1f C\n', temperature);
fprintf('Salinity     : %.1f PSU\n', salinity);

fprintf('\nCLASSIFICATION\n');
fprintf('Depth State  : %s\n', env.depth_state);
fprintf('Turbidity    : %s\n', env.turbidity_state);

fprintf('\nADAPTIVE PARAMETERS\n');
fprintf('Start Freq   : %.1f kHz\n', params.f_start/1000);
fprintf('End Freq     : %.1f kHz\n', params.f_end/1000);
fprintf('Pulse        : %.2f ms\n', params.pulse_duration*1000);
fprintf('Amplitude    : %.2f\n', params.amplitude);

fprintf('\nDECISION\n');
fprintf('Selected Waveform : %s\n', params.waveform);

fprintf('====================================================\n');


% ----------------------------------------------------------
% 6. GENERATE SELECTED WAVEFORM
% ----------------------------------------------------------

if strcmp(params.waveform, 'LFM')

    [t, signal, inst_freq] = ...
        lfm_generator(params, fs);

    waveform_type = 'LFM';

elseif strcmp(params.waveform, 'BARKER')

    [t, signal, code] = ...
        barker_generator(params, fs);

    waveform_type = 'BARKER';

elseif strcmp(params.waveform, 'GEOMETRIC')

    [t, signal, inst_freq] = ...
        geometric_generator(params, fs);

    waveform_type = 'GEOMETRIC';

else

    error('Unknown waveform selected.');

end


% ----------------------------------------------------------
% 7. BASIC SIGNAL INFORMATION
% ----------------------------------------------------------

num_samples = length(signal);
signal_duration = num_samples / fs;

peak_amplitude = max(abs(signal));


fprintf('\nSIGNAL OUTPUT\n');
fprintf('Waveform       : %s\n', waveform_type);
fprintf('Samples        : %d\n', num_samples);
fprintf('Duration       : %.3f ms\n', signal_duration*1000);
fprintf('Peak Amplitude : %.3f\n', peak_amplitude);
fprintf('Sampling Rate  : %.1f MHz\n', fs/1e6);

fprintf('====================================================\n');


% ----------------------------------------------------------
% 8. PLOT GENERATED WAVEFORM
% ----------------------------------------------------------

figure;

plot(t*1000, signal);

xlabel('Time (ms)');
ylabel('Amplitude');

title(['Selected Adaptive Waveform - ' waveform_type]);

grid on;


% ----------------------------------------------------------
% 9. FREQUENCY ANALYSIS
% ----------------------------------------------------------

[f, magnitude] = fft_analysis(signal, fs);


figure;

plot(f/1000, magnitude);

xlabel('Frequency (kHz)');
ylabel('Magnitude');

title(['Frequency Spectrum - ' waveform_type]);

grid on;


% ----------------------------------------------------------
% 10. INSTANTANEOUS FREQUENCY
% ----------------------------------------------------------

if strcmp(waveform_type, 'LFM') || ...
        strcmp(waveform_type, 'GEOMETRIC')

    figure;

    plot(t*1000, inst_freq/1000);

    xlabel('Time (ms)');
    ylabel('Frequency (kHz)');

    title(['Instantaneous Frequency - ' waveform_type]);

    grid on;

end


% ----------------------------------------------------------
% 11. BARKER AUTOCORRELATION
% ----------------------------------------------------------

if strcmp(waveform_type, 'BARKER')

    [acor, lag] = xcorr(signal);

    figure;

    plot(lag/fs*1000, acor);

    xlabel('Lag (ms)');
    ylabel('Correlation');

    title('Barker-13 Autocorrelation');

    grid on;

end


% ----------------------------------------------------------
% 12. SPECTROGRAM
% ----------------------------------------------------------

figure;

window_length = 1024;
overlap = 768;
nfft = 2048;

spectrogram(signal, window_length, ...
            overlap, nfft, fs, 'yaxis');

title(['Sonar Spectrogram - ' waveform_type]);

ylim([0 0.5]);


% ----------------------------------------------------------
% 13. FINAL RESULT
% ----------------------------------------------------------

fprintf('\n');
fprintf('====================================================\n');
fprintf('              PHASE 1 POC COMPLETE\n');
fprintf('====================================================\n');

fprintf('Environment       : %s / %s\n', ...
        env.depth_state, env.turbidity_state);

fprintf('Waveform Selected : %s\n', params.waveform);

fprintf('Frequency Range   : %.1f - %.1f kHz\n', ...
        params.f_start/1000, params.f_end/1000);

fprintf('Pulse Duration    : %.2f ms\n', ...
        params.pulse_duration*1000);

fprintf('Peak Amplitude    : %.3f\n', peak_amplitude);

fprintf('Samples           : %d\n', num_samples);

fprintf('====================================================\n');