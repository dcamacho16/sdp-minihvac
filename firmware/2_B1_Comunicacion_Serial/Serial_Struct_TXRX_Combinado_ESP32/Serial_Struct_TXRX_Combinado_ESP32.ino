/*
 * Proyecto: MiniHVAC - Pruebas B1 Comunicación Serie
 * Sketch:   B1_Serial_Struct_TXRX_ESP32.ino
 * Carpeta:  1_Desarrollo/Firmware_ESP32/2_B1_Comunicacion_Serial/Serial_Struct_TXRX_ESP32/
 *
 * Objetivo:
 *   - Combinar en un único sketch la recepción de PcToEsp (24 bytes)
 *     y el envío periódico de EspToPc (32 bytes) sin FreeRTOS.
 *   - Servir como "banco de pruebas" de la comunicación full-duplex ESP32 ↔ Simulink.
 */

#include <Arduino.h>

// ────────────────────── Definición de estructuras ──────────────────────

struct PcToEsp {
  float T_sim;         // 1
  float RH_sim;        // 2
  float P_sim;         // 3
  float u_fan_sim;     // 4
  float u_heater_sim;  // 5
  float alarm_flag;    // 6 (0.0 ó 1.0)
}; // 24 bytes

struct EspToPc {
  float T_real;           // 1
  float RH_real;          // 2
  float u_fan_real;       // 3
  float u_heater_real;    // 4
  float T_set;            // 5
  float RH_set;           // 6
  float mode_auto_manual; // 7
  float hvac_enable;      // 8
}; // 32 bytes

static_assert(sizeof(PcToEsp) == 24, "PcToEsp debe ocupar 24 bytes");
static_assert(sizeof(EspToPc) == 32, "EspToPc debe ocupar 32 bytes");

// ────────────────────── Variables globales ──────────────────────

PcToEsp pcCmd;
EspToPc espState;

uint32_t lastTxMillis = 0;
const uint32_t TX_PERIOD_MS = 1000;

float simTemperature = 20.0f;

// ────────────────────── Funciones auxiliares ──────────────────────

void updateEspState(EspToPc &state, const PcToEsp &cmd) {
  // Ejemplo: usar la T_sim como referencia y sumar un pequeño offset
  simTemperature += 1.0f;
  if (simTemperature > 40.0f) {
    simTemperature = 20.0f;
  }

  state.T_real        = simTemperature;
  state.RH_real       = 50.0f;
  // Podemos "copiar" los comandos de Simulink a las variables reales para depurar
  state.u_fan_real    = cmd.u_fan_sim;
  state.u_heater_real = cmd.u_heater_sim;

  state.T_set         = 25.0f;
  state.RH_set        = 45.0f;
  state.mode_auto_manual = 1.0f;
  state.hvac_enable      = 1.0f;
}

void printPcCmd(const PcToEsp &cmd) {
  Serial.println(F("===== PcToEsp recibido ====="));
  Serial.print(F("T_sim        = ")); Serial.println(cmd.T_sim);
  Serial.print(F("RH_sim       = ")); Serial.println(cmd.RH_sim);
  Serial.print(F("P_sim        = ")); Serial.println(cmd.P_sim);
  Serial.print(F("u_fan_sim    = ")); Serial.println(cmd.u_fan_sim);
  Serial.print(F("u_heater_sim = ")); Serial.println(cmd.u_heater_sim);
  Serial.print(F("alarm_flag   = ")); Serial.println(cmd.alarm_flag);
  Serial.println(F("============================="));
}

// ────────────────────── Setup y Loop ──────────────────────

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println(F("B1_Serial_Struct_TXRX_ESP32 - Inicio de la prueba combinada"));
  Serial.print(F("Tamaño PcToEsp = "));
  Serial.println(sizeof(PcToEsp));
  Serial.print(F("Tamaño EspToPc = "));
  Serial.println(sizeof(EspToPc));
}

void loop() {
  // ── 1) Recepción: si hay un paquete de 24 bytes, lo leemos ──
  if (Serial.available() >= (int)sizeof(PcToEsp)) {
    size_t readBytes = Serial.readBytes(
      reinterpret_cast<uint8_t*>(&pcCmd),
      sizeof(PcToEsp)
    );
    if (readBytes == sizeof(PcToEsp)) {
      printPcCmd(pcCmd);
    } else {
      Serial.print(F("Error RX: bytes leídos = "));
      Serial.println(readBytes);
    }
  }

  // ── 2) Transmisión periódica de EspToPc ──
  uint32_t now = millis();
  if (now - lastTxMillis >= TX_PERIOD_MS) {
    lastTxMillis = now;

    updateEspState(espState, pcCmd);

    size_t sent = Serial.write(
      reinterpret_cast<uint8_t*>(&espState),
      sizeof(EspToPc)
    );

    Serial.print(F("[TX] Enviando EspToPc (bytes enviados = "));
    Serial.print(sent);
    Serial.println(F(")"));
    Serial.print(F("  T_real = ")); Serial.println(espState.T_real);
    Serial.print(F("  RH_real = ")); Serial.println(espState.RH_real);
    Serial.print(F("  u_fan_real = ")); Serial.println(espState.u_fan_real);
    Serial.print(F("  u_heater_real = ")); Serial.println(espState.u_heater_real);
    Serial.println();
  }

  delay(5);
}