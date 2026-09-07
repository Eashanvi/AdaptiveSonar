function params = adaptation(env)

% ==========================================================
% PHASE 1 - ADAPTIVE SONAR
% Adaptation Engine
% MATLAB 2016 compatible
% ==========================================================

% Default parameters
params.f_start = 100e3;
params.f_end = 300e3;
params.pulse_duration = 5e-3;
params.amplitude = 0.7;
params.waveform = 'LFM';

% ----------------------------------------------------------
% Environmental adaptation
% ----------------------------------------------------------

% TURBIDITY
if strcmp(env.turbidity_state, 'LOW')
    
    params.f_start = 300e3;
    params.f_end = 500e3;
    params.pulse_duration = 2e-3;
    params.amplitude = 0.6;
    
elseif strcmp(env.turbidity_state, 'MEDIUM')
    
    params.f_start = 200e3;
    params.f_end = 400e3;
    params.pulse_duration = 3e-3;
    params.amplitude = 0.7;
    
else
    
    params.f_start = 100e3;
    params.f_end = 250e3;
    params.pulse_duration = 5e-3;
    params.amplitude = 0.9;
    
end

% ----------------------------------------------------------
% DEPTH adjustment
% ----------------------------------------------------------

if strcmp(env.depth_state, 'DEEP')
    
    params.pulse_duration = params.pulse_duration * 1.5;
    params.amplitude = min(params.amplitude + 0.1, 1.0);
    
elseif strcmp(env.depth_state, 'SHALLOW')
    
    params.pulse_duration = params.pulse_duration * 0.8;
    
end

end