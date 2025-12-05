/*
 * Proyecto: MiniHVAC - Pruebas B1 Comunicación Serie
 * Sketch:   B1_Serial_Struct_TX_ESP32.ino
 * Carpeta:  1_Desarrollo/Firmware_ESP32/2_B1_Comunicacion_Serial/Serial_Struct_TX_ESP32/
 *
 * Objetivo:
 *   - Probar el envío periódico de la estructura EspToPc (32 bytes) desde el ESP32 hacia Simulink.
 *   - Validar que el bloque RX_From_ESP32 en Simulink desempaqueta correctamente los 8 floats.
 *
 * Uso en Simulink:
 *   - Conectar el puerto serie al ESP32.
 *   - Usar el bloque de recepción + Byte Unpack con el mismo orden de campos que la struct EspToPc.
 */

#include <Arduino.h>

// ────────────────────── Definición de estructuras ──────────────────────

struct EspToPc {
  float T_real;           // 1
  float RH_real;          // 2
  float u_fan_real;       // 3
  float u_heater_real;    // 4
  float T_set;            // 5
  float RH_set;           // 6
  float mode_auto_manual; // 7 (0.0 = MANUAL, 1.0 = AUTO)
  float hvac_enable;      // 8 (0.0 = OFF,    1.0 = ON)
}; // 32 bytes

static_assert(sizeof(EspToPc) == 32, "EspToPc debe ocupar 32 bytes");

// ────────────────────── Variables globales ──────────────────────

EspToPc espState;
uint32_t lastTxMillis = 0;
const uint32_t TX_PERIOD_MS = 1000; // Enviar una vez por segundo

// Contador para simular una temperatura que sube de 20 ºC a 40 ºC
float simTemperature = 20.0f;

// ────────────────────── Funciones auxiliares ──────────────────────

void fillEspState(EspToPc &state) {
  // Simulación muy simple para prueba de laboratorio

  // Temperatura real simulada: contador de 20 ºC a 40 ºC
  simTemperature += 1.0f;
  if (simTemperature > 40.0f) {
    simTemperature = 20.0f;
  }

  state.T_real        = simTemperature;
  state.RH_real       = 50.0f;   // Humedad fijada al 50 %
  state.u_fan_real    = 0.5f;    // 50 % de duty para el ventilador (valor simbólico)
  state.u_heater_real = 0.0f;    // Calefactor apagado
  state.T_set         = 25.0f;   // Setpoint fijo para la demo
  state.RH_set        = 45.0f;   // Setpoint de humedad simbólico
  state.mode_auto_manual = 1.0f; // AUTO
  state.hvac_enable      = 1.0f; // HVAC habilitado
}

// ────────────────────── Setup y Loop ──────────────────────

void setup() {
  Serial.begin(115200);
  // Espera breve para que el puerto serie esté listo (útil con monitor serie)
  delay(2000);

  Serial.println();
  Serial.println(F("B1_Serial_Struct_TX_ESP32 - Inicio de la prueba TX"));
  Serial.print(F("Tamaño de EspToPc: "));
  Serial.println(sizeof(EspToPc));
}

void loop() {
  uint32_t now = millis();

  // Envío periódico de la estructura
  if (now - lastTxMillis >= TX_PERIOD_MS) {
    lastTxMillis = now;

    fillEspState(espState);

    // Enviar los 32 bytes crudos por el puerto serie
    size_t sent = Serial.write(reinterpret_cast<uint8_t*>(&espState), sizeof(EspToPc));

    // Mensaje de depuración (solo para monitor serie humano)
    Serial.print(F("[TX] Enviando EspToPc (bytes enviados = "));
    Serial.print(sent);
    Serial.println(F(")"));
    Serial.print(F("  T_real = "));        Serial.println(espState.T_real);
    Serial.print(F("  RH_real = "));       Serial.println(espState.RH_real);
    Serial.print(F("  u_fan_real = "));    Serial.println(espState.u_fan_real);
    Serial.print(F("  u_heater_real = ")); Serial.println(espState.u_heater_real);
    Serial.print(F("  T_set = "));         Serial.println(espState.T_set);
    Serial.print(F("  RH_set = "));        Serial.println(espState.RH_set);
    Serial.print(F("  mode_auto_manual = ")); Serial.println(espState.mode_auto_manual);
    Serial.print(F("  hvac_enable = "));      Serial.println(espState.hvac_enable);
    Serial.println();
  }
}