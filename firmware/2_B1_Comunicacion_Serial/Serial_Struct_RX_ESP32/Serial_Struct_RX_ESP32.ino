/*
 * Proyecto: MiniHVAC - Pruebas B1 Comunicación Serie
 * Sketch:   B1_Serial_Struct_RX_ESP32.ino
 * Carpeta:  1_Desarrollo/Firmware_ESP32/2_B1_Comunicacion_Serial/Serial_Struct_RX_ESP32/
 *
 * Objetivo:
 *   - Probar la recepción de la estructura PcToEsp (24 bytes) enviada desde Simulink.
 *   - Validar que el bloque TX_To_ESP32 en Simulink codifica correctamente los 6 floats.
 *
 * Uso en Simulink:
 *   - Configurar el bloque de transmisión serie -> Byte Pack con el mismo orden de PcToEsp.
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

static_assert(sizeof(PcToEsp) == 24, "PcToEsp debe ocupar 24 bytes");

// ────────────────────── Variables globales ──────────────────────

PcToEsp pcCmd;

// ────────────────────── Funciones auxiliares ──────────────────────

void printPcCmd(const PcToEsp &cmd) {
  Serial.println(F("===== Paquete PcToEsp recibido ====="));
  Serial.print(F("T_sim        = ")); Serial.println(cmd.T_sim);
  Serial.print(F("RH_sim       = ")); Serial.println(cmd.RH_sim);
  Serial.print(F("P_sim        = ")); Serial.println(cmd.P_sim);
  Serial.print(F("u_fan_sim    = ")); Serial.println(cmd.u_fan_sim);
  Serial.print(F("u_heater_sim = ")); Serial.println(cmd.u_heater_sim);
  Serial.print(F("alarm_flag   = ")); Serial.println(cmd.alarm_flag);
  Serial.println(F("====================================="));
}

// Función opcional para descartar bytes sobrantes si nos desalineamos
void flushExtraBytes() {
  while (Serial.available() > 0) {
    (void)Serial.read();
  }
}

// ────────────────────── Setup y Loop ──────────────────────

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println(F("B1_Serial_Struct_RX_ESP32 - Inicio de la prueba RX"));
  Serial.print(F("Tamaño de PcToEsp: "));
  Serial.println(sizeof(PcToEsp));
}

void loop() {
  // Esperar a que haya suficientes bytes para leer la estructura completa
  if (Serial.available() >= (int)sizeof(PcToEsp)) {
    // Leer exactamente 24 bytes en la struct
    size_t readBytes = Serial.readBytes(
      reinterpret_cast<uint8_t*>(&pcCmd),
      sizeof(PcToEsp)
    );

    if (readBytes == sizeof(PcToEsp)) {
      printPcCmd(pcCmd);
    } else {
      Serial.print(F("Error: bytes leídos = "));
      Serial.println(readBytes);
      // En caso de error, descartamos lo que quede en el buffer
      flushExtraBytes();
    }
  }

  // Pequeño delay para no saturar el bucle
  delay(10);
}