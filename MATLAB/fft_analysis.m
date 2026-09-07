function [f, magnitude] = fft_analysis(signal, fs)

% ==========================================================
% PHASE 1 - FFT ANALYSIS
% MATLAB 2016 compatible
% ==========================================================

N = length(signal);

% FFT
Y = fft(signal);

% Two-sided magnitude
P2 = abs(Y/N);

% Single-sided spectrum
P1 = P2(1:floor(N/2)+1);

P1(2:end-1) = 2*P1(2:end-1);

% Frequency axis
f = fs*(0:floor(N/2))/N;

% Return magnitude
magnitude = P1;

end