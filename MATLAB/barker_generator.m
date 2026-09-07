function [t, signal, code] = barker_generator(params, fs)

% ==========================================================
% PHASE 1 - BARKER / PHASE-CODED WAVEFORM
% MATLAB 2016 compatible
% ==========================================================

% Barker-13 code
code = [1 1 1 1 1 -1 -1 1 1 -1 1 -1 1];

% Total pulse duration
T = params.pulse_duration;

% Number of chips
num_chips = length(code);

% Chip duration
Tc = T / num_chips;

% Samples per chip
samples_per_chip = round(Tc * fs);

% Create waveform
signal = [];

for i = 1:num_chips
    
    chip = code(i) * ones(1, samples_per_chip);
    
    signal = [signal chip];
    
end

% Time vector
N = length(signal);
t = (0:N-1)/fs;

% Apply amplitude
signal = params.amplitude * signal;

end