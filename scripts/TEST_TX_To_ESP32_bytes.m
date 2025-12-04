%% TEST_TX_To_ESP32_bytes.m
% -------------------------------------------------------------
% Este script genera el vector de 24 bytes (uint8) correspondiente
% al struct PcToEsp que se envía de PC → ESP32, y opcionalmente
% compara esos bytes con los producidos por el subsystem
% TX_To_ESP32 en Simulink.
%
% struct PcToEsp {
%   float T_sim;        // 1
%   float RH_sim;       // 2
%   float P_sim;        // 3
%   float u_fan_sim;    // 4
%   float u_heater_sim; // 5
%   float alarm_flag;   // 6
% };
% Total: 6 * 4 = 24 bytes
%
% Flujo:
%   1) Definimos los valores que queremos mandar.
%   2) Los convertimos a single y luego a uint8[24] con typecast.
%   3) (Opcional) Si existe la variable 'out.tx_bytes' procedente de
%      una simulación, comparamos expected_bytes vs out.tx_bytes.
% -------------------------------------------------------------

clear; clc;

%% 1. Valores de prueba para PcToEsp (PC → ESP32)

T_sim        = 25.0;  % °C simulada
RH_sim       = 50.0;  % % simulada
P_sim        = 0.5;   % nivel de ventilación / presión
u_fan_sim    = 0.3;   % duty 0–1
u_heater_sim = 0.1;   % duty 0–1
alarm_flag   = 1.0;   % 1 = alarma activa

% Vector columna en el orden EXACTO del struct PcToEsp
vals_pc = single([ ...
    T_sim; ...
    RH_sim; ...
    P_sim; ...
    u_fan_sim; ...
    u_heater_sim; ...
    alarm_flag]);

%% 2. Convertir a 24 bytes uint8

expected_bytes = typecast(vals_pc, 'uint8');      % 24x1 uint8
expected_bytes_row = reshape(expected_bytes, 1, []);

disp('Vector uint8[24] esperado para PcToEsp (PC → ESP32):');
disp(expected_bytes_row);

fprintf('\nEjemplo para usarlo en pruebas o documentación:\n\n');
fprintf('uint8([');
fprintf('%d ', expected_bytes_row);
fprintf('])\n\n');

%% 3. Comparación opcional con lo que genera TX_To_ESP32 en Simulink
% -----------------------------------------------------------------
% Si acabas de ejecutar una simulación del modelo ESP32_Interface
% con un bloque "To Workspace" llamado tx_bytes y con
% "Single simulation output" activado, tendrás en el workspace
% una variable 'out' tipo SimulationOutput y dentro 'out.tx_bytes'.
%
%   - out.tx_bytes debe ser de tamaño 1x24 uint8 (o similar).
%   - Aquí comparamos expected_bytes_row con esa salida.

if exist('out', 'var') && isfield(out, 'tx_bytes')
    sim_bytes = out.tx_bytes;     % 1x24 uint8 (según configuración)

    % Aseguramos que es fila:
    sim_bytes_row = reshape(sim_bytes, 1, []);

    disp('Bytes generados por TX_To_ESP32 (out.tx_bytes):');
    disp(sim_bytes_row);

    iguales = isequal(sim_bytes_row, expected_bytes_row);

    if iguales
        disp('✅ Los bytes de TX_To_ESP32 COINCIDEN con expected_bytes.');
    else
        disp('❌ Los bytes de TX_To_ESP32 NO coinciden con expected_bytes.');
        disp('Comparación fila 1: [sim_bytes; expected_bytes]:');
        disp([double(sim_bytes_row); double(expected_bytes_row)]);
    end
else
    disp('Nota: no se ha encontrado ''out.tx_bytes''.'); 
    disp('Si quieres comparar, ejecuta primero la simulación del modelo');
    disp('ESP32_Interface con un bloque "To Workspace" (tx_bytes)');
    disp('y con "Single simulation output" activado.');
end

%% 4. Comprobación inversa (opcional)
% Reconstruimos los floats a partir de expected_bytes para verificar
% que typecast es reversible.

vals_rec = typecast(expected_bytes, 'single');  % 6x1 single

tabla = table( ...
    [T_sim; RH_sim; P_sim; u_fan_sim; u_heater_sim; alarm_flag], ...
    vals_rec, ...
    'VariableNames', {'Valor_Original', 'Valor_Reconstruido'}, ...
    'RowNames', { ...
        'T_sim', 'RH_sim', 'P_sim', ...
        'u_fan_sim', 'u_heater_sim', 'alarm_flag'});

disp('Comprobación de valores originales vs reconstruidos:');
disp(tabla);