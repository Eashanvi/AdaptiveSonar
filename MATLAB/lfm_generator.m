function [t, signal, inst_freq] = lfm_generator(params, fs)

% ==========================================================
% PHASE 1 - ADAPTIVE SONAR
% LFM Waveform Generator
% MATLAB 2016 compatible
% ==========================================================

% Extract parameters
f_start = params.f_start;
f_end = params.f_end;
T = params.pulse_duration;
A = params.amplitude;

% Number of samples
N = round(T * fs);

% Time vector
t = (0:N-1) / fs;

% Chirp rate
k = (f_end - f_start) / T;

% Instantaneous phase
phase = 2*pi*(f_start*t + 0.5*k*t.^2);

% Generate LFM signal
signal = A*cos(phase);

% Instantaneous frequency
inst_freq = f_start + k*t;

end