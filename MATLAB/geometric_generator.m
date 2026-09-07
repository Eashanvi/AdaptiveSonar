function [t, signal, inst_freq] = geometric_generator(params, fs)

% ==========================================================
% PHASE 1 - GEOMETRIC FREQUENCY SWEEP
% MATLAB 2016 compatible
% ==========================================================

f_start = params.f_start;
f_end = params.f_end;
T = params.pulse_duration;
A = params.amplitude;

% Number of samples
N = round(T * fs);

% Time vector
t = (0:N-1)/fs;

% Normalized time
tau = t/T;

% Geometric frequency trajectory
inst_freq = f_start * (f_end/f_start).^tau;

% Phase calculation
phase = 2*pi*f_start*T/log(f_end/f_start) .* ...
        ((f_end/f_start).^tau - 1);

% Generate waveform
signal = A*cos(phase);

end