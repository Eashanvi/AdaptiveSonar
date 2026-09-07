function waveform = waveform_selector(env)

% ==========================================================
% PHASE 1 - ADAPTIVE SONAR
% STEP 9 - WAVEFORM SELECTION
% MATLAB 2016 compatible
% ==========================================================

% ----------------------------------------------------------
% Default waveform
% ----------------------------------------------------------

waveform = 'LFM';


% ----------------------------------------------------------
% WAVEFORM DECISION LOGIC
% ----------------------------------------------------------

% SHALLOW + LOW TURBIDITY
if strcmp(env.depth_state, 'SHALLOW') && ...
        strcmp(env.turbidity_state, 'LOW')
    
    waveform = 'BARKER';
    
    
% DEEP + HIGH TURBIDITY
elseif strcmp(env.depth_state, 'DEEP') && ...
        strcmp(env.turbidity_state, 'HIGH')
    
    waveform = 'GEOMETRIC';
    
    
% MEDIUM / OTHER CONDITIONS
else
    
    waveform = 'LFM';
    
end

end