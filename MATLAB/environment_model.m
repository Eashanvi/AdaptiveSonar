function env = environment_model(depth, turbidity, temperature, salinity)

% ==========================================================
% PHASE 1 - ADAPTIVE SONAR
% Environment Model
% MATLAB 2016 compatible
% ==========================================================

% Store environmental parameters
env.depth = depth;
env.turbidity = turbidity;
env.temperature = temperature;
env.salinity = salinity;

% ----------------------------------------------------------
% Classify environment
% ----------------------------------------------------------

if turbidity < 30
    turbidity_state = 'LOW';
elseif turbidity < 70
    turbidity_state = 'MEDIUM';
else
    turbidity_state = 'HIGH';
end

if depth < 50
    depth_state = 'SHALLOW';
elseif depth < 120
    depth_state = 'MEDIUM';
else
    depth_state = 'DEEP';
end

% Store classifications
env.turbidity_state = turbidity_state;
env.depth_state = depth_state;

end