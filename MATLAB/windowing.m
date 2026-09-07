function [signal_hamming, signal_hann, signal_blackman] = windowing(signal)

% ==========================================================
% PHASE 1 - WINDOWING
% MATLAB 2016 compatible
% ==========================================================

N = length(signal);

% Generate windows
w_hamming = hamming(N)';
w_hann = hann(N)';
w_blackman = blackman(N)';

% Apply windows
signal_hamming = signal .* w_hamming;
signal_hann = signal .* w_hann;
signal_blackman = signal .* w_blackman;

end