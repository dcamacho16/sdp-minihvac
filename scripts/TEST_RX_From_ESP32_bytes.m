%% TEST_RX_From_ESP32_bytes.m
% -------------------------------------------------------------
% Este script genera el vector de 32 bytes (uint8) que se usa
% para probar el subsystem RX_From_ESP32 en el modelo ESP32_Interface.
%
% Corresponde al struct EspToPc que envía el ESP32 → PC:
%
%   struct EspToPc {
%     float T_real;         // 1
%     float RH_real;        // 2
%     float u_fan_real;     // 3
%     float u_heater_real;  // 4
%     float T_set;          // 5
%     float RH_set;         // 6
%     float mode_auto_manual; // 7 (0.0=MANUAL, 1.0=AUTO)
%     float hvac_enable;      // 8 (0.0=OFF,    1.0=ON)
%   };
%   Total: 8 * 4 = 32 bytes
%
% La idea es:
%   1) Definir los valores "reales" que queremos simular.
%   2) Convertirlos a single (float de 4 bytes).
%   3) Usar typecast(...) para obtener el vector uint8[32].
%   4) Copiar ese vector al bloque Constant → RX_data en Simulink.
% -------------------------------------------------------------

clear; clc;

%% 1. Valores de prueba para EspToPc (ESP32 → PC)
% Estos son los valores que queremos que RX_From_ESP32 reconstruya
% cuando reciba el paquete de 32 bytes.

T_real          = 25.0;  % °C
RH_real         = 50.0;  % %
u_fan_real      = 0.30;  % duty 0–1
u_heater_real   = 0.10;  % duty 0–1
T_set           = 27.0;  % °C
RH_set          = 45.0;  % %
mode_auto_manual = 1.0;  % 1 = AUTO
hvac_enable      = 1.0;  % 1 = ON

% Vector columna en el orden EXACTO del struct EspToPc
vals = single([ ...
    T_real; ...
    RH_real; ...
    u_fan_real; ...
    u_heater_real; ...
    T_set; ...
    RH_set; ...
    mode_auto_manual; ...
    hvac_enable]);

%% 2. Convertir los 8 floats (single) a 32 bytes uint8

bytes = typecast(vals, 'uint8');  % 32x1 uint8

% Para comodidad, lo dejamos como fila 1x32
bytes_row = reshape(bytes, 1, []);

%% 3. Mostrar el vector listo para pegar en Simulink

disp('Vector uint8[32] para el Constant → RX_data (RX_From_ESP32):');
disp(bytes_row);

% También imprimimos la forma lista para copiar/pegar:
fprintf('\nCopiar en el bloque Constant como:\n\n');
fprintf('uint8([');
fprintf('%d ', bytes_row);
fprintf('])\n\n');

%% 4. Comprobación inversa (opcional)
% Reconstruimos los floats a partir de los bytes para verificar
% que el proceso es reversible y que no hemos cambiado el orden.

vals_rec = typecast(bytes, 'single');  % 8x1 single

tabla = table( ...
    [T_real; RH_real; u_fan_real; u_heater_real; ...
     T_set; RH_set; mode_auto_manual; hvac_enable], ...
    vals_rec, ...
    'VariableNames', {'Valor_Original', 'Valor_Reconstruido'}, ...
    'RowNames', { ...
        'T_real', 'RH_real', 'u_fan_real', 'u_heater_real', ...
        'T_set', 'RH_set', 'mode_auto_manual', 'hvac_enable'});

disp('Comprobación de valores originales vs reconstruidos:');
disp(tabla);