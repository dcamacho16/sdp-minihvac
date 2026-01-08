#include <Arduino.h>
#include <DHTesp.h>

// ─────────────────────────────────────────────────────────────
// 1. Definición de estructuras de comunicación
// ─────────────────────────────────────────────────────────────

struct PcToEsp {
  float T_sim;         // 1
  float RH_sim;        // 2
  float P_sim;         // 3
  float u_fan_sim;     // 4
  float u_heater_sim;  // 5
  float alarm_flag;    // 6 (0.0 ó 1.0)
};                     // 24 bytes

struct EspToPc {
  float T_real;            // 1
  float RH_real;           // 2
  float u_fan_real;        // 3
  float u_heater_real;     // 4
  float T_set;             // 5
  float RH_set;            // 6
  float mode_auto_manual;  // 7 (0.0=MANUAL, 1.0=AUTO)
  float hvac_enable;       // 8 (0.0=OFF,    1.0=ON)
};                         // 32 bytes

static_assert(sizeof(PcToEsp) == 24, "PcToEsp debe medir 24 bytes");
static_assert(sizeof(EspToPc) == 32, "EspToPc debe medir 32 bytes");

// ─────────────────────────────────────────────────────────────
// 2. Variables globales compartidas y sincronización
// ─────────────────────────────────────────────────────────────

// Comandos recibidos del PC (Simulink)
PcToEsp pcCmd;

// Estado que enviamos al PC
EspToPc espState;

// Mutex para secciones críticas
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void safeUpdatePcCmd(const PcToEsp &newCmd) {
  portENTER_CRITICAL(&mux);
  pcCmd = newCmd;
  portEXIT_CRITICAL(&mux);
}

void safeReadPcCmd(PcToEsp &dest) {
  portENTER_CRITICAL(&mux);
  dest = pcCmd;
  portEXIT_CRITICAL(&mux);
}

void safeUpdateEspState(const EspToPc &newState) {
  portENTER_CRITICAL(&mux);
  espState = newState;
  portEXIT_CRITICAL(&mux);
}

void safeReadEspState(EspToPc &dest) {
  portENTER_CRITICAL(&mux);
  dest = espState;
  portEXIT_CRITICAL(&mux);
}

// ─────────────────────────────────────────────────────────────
// 3. Definición de pines y parámetros de hardware
// ─────────────────────────────────────────────────────────────

// Actuadores
const int PIN_FAN_PWM = 5;    // PWM para ventilador / motor DC
const int PIN_HEATER = 2;     // LED que simula calefactor
const int PIN_ALARM_LED = 4;  // LED/Buzzer de alarma

// Sensor T/RH (DHT)
const int PIN_DHT = 21;

// Potenciómetros para consignas
const int PIN_POT_TSET = 34;   // T_set (ADC1)
const int PIN_POT_RHSET = 35;  // RH_set (ADC1)

// Botones (con INPUT_PULLUP)
const int PIN_BTN_MODE = 25;  // AUTO/MANUAL
const int PIN_BTN_HVAC = 26;  // HVAC enable

// Configuración PWM ventilador
const int FAN_PWM_CHANNEL = 0;
const int FAN_PWM_FREQUENCY = 25000;  // 25 kHz
const int FAN_PWM_RESOLUTION = 8;     // 8 bits (0..255)

// Periodo de envío de estado al PC (Simulink)
const uint32_t TX_PERIOD_MS = 1000;  // 1 s o 2 s (DHT11 es lento)

// Objeto sensor DHT
DHTesp dht;

// ─────────────────────────────────────────────────────────────
// 4. Prototipos de funciones y tareas
// ─────────────────────────────────────────────────────────────

void setFanPwm(float duty);
void setHeaterOutput(float level);
void setAlarmOutputs(bool alarm);
void updateSensorsAndInputs(EspToPc &state);
void applyPCCommands(const PcToEsp &cmd);

void TaskTX(void *pvParameters);
void TaskRX(void *pvParameters);

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ─────────────────────────────────────────────────────────────
// 5. setup() y loop()
// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(2000);  // Esperamos a que el puerto serie esté listo

  // Actuadores
  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_ALARM_LED, OUTPUT);
  digitalWrite(PIN_HEATER, LOW);
  digitalWrite(PIN_ALARM_LED, LOW);

  // Sensor DHT
  dht.setup(PIN_DHT, DHTesp::DHT22);

  // Botones de usuario
  pinMode(PIN_BTN_MODE, INPUT_PULLUP);
  pinMode(PIN_BTN_HVAC, INPUT_PULLUP);
  // Potenciómetros: no necesitan pinMode

  // PWM ventilador
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQUENCY, FAN_PWM_RESOLUTION);
  ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);  // ventilador apagado

  // Inicializamos estructuras
  memset(&pcCmd, 0, sizeof(pcCmd));
  memset(&espState, 0, sizeof(espState));

  // Estado inicial: MANUAL y HVAC encendido
  espState.mode_auto_manual = 0.0f;  // 0 = MANUAL
  espState.hvac_enable = 0.0f;       // 1 = ON

  // Tareas FreeRTOS
  xTaskCreatePinnedToCore(
    TaskTX,
    "TaskTX",
    4096,
    NULL,
    1,  // prioridad normal
    NULL,
    1);  // core 1

  xTaskCreatePinnedToCore(
    TaskRX,
    "TaskRX",
    4096,
    NULL,
    2,  // prioridad un poco más alta
    NULL,
    1);  // core 1

  // Mensajes de depuración (sólo firmware, no Simulink)
  Serial.print("sizeof(PcToEsp) = ");
  Serial.println(sizeof(PcToEsp));
  Serial.print("sizeof(EspToPc) = ");
  Serial.println(sizeof(EspToPc));
}

void loop() {
  // Todo el trabajo se hace en las tareas FreeRTOS
  vTaskDelay(pdMS_TO_TICKS(1000));
}

// ─────────────────────────────────────────────────────────────
// 6. Funciones auxiliares
// ─────────────────────────────────────────────────────────────

void setFanPwm(float duty) {
  // duty en [0.0, 1.0]
  if (duty < 0.0f) duty = 0.0f;
  if (duty > 1.0f) duty = 1.0f;

  int maxValue = (1 << FAN_PWM_RESOLUTION) - 1;
  int pwmValue = (int)(duty * maxValue);
  ledcWrite(FAN_PWM_CHANNEL, pwmValue);
}

void setHeaterOutput(float level) {
  // ON si level > 0.5
  digitalWrite(PIN_HEATER, (level > 0.5f) ? HIGH : LOW);
}

void setAlarmOutputs(bool alarm) {
  digitalWrite(PIN_ALARM_LED, alarm ? HIGH : LOW);
}

// Actualiza espState a partir de:
// - Sensor DHT → T_real, RH_real
// - Potenciómetros → T_set, RH_set
// - Botones → mode_auto_manual, hvac_enable
// - Comandos actuales → u_fan_real, u_heater_real
void updateSensorsAndInputs(EspToPc &state) {
  // // 1) Lectura DHT
  // TempAndHumidity th = dht.getTempAndHumidity();
  // if (!isnan(th.temperature) && !isnan(th.humidity)) {
  //   state.T_real = th.temperature;
  //   state.RH_real = th.humidity;
  // }

  // 1) Lectura DHT (DEBUG)
  TempAndHumidity th = dht.getTempAndHumidity();

  // Serial.print("DHT status = ");
  // Serial.print(dht.getStatusString());
  // Serial.print("  rawT = ");
  // Serial.print(th.temperature);
  // Serial.print("  rawRH = ");
  // Serial.println(th.humidity);

  if (!isnan(th.temperature) && !isnan(th.humidity)) {
    state.T_real = th.temperature;
    state.RH_real = th.humidity;
  } else {
    // Si hay error, dejamos T_real y RH_real como estaban
  }

  // 2) Potenciómetros
  int rawTset = analogRead(PIN_POT_TSET);
  int rawRHset = analogRead(PIN_POT_RHSET);

  state.T_set = mapFloat((float)rawTset, 0.0f, 4095.0f, 18.0f, 28.0f);    // ºC aprox.
  state.RH_set = mapFloat((float)rawRHset, 0.0f, 4095.0f, 30.0f, 70.0f);  // %RH aprox.

  // 3) Botones
  bool autoMode = (digitalRead(PIN_BTN_MODE) == LOW);    // pulsado ⇒ AUTO
  bool hvacEnable = (digitalRead(PIN_BTN_HVAC) == LOW);  // pulsado ⇒ ON

  state.mode_auto_manual = autoMode ? 1.0f : 0.0f;
  state.hvac_enable = hvacEnable ? 1.0f : 0.0f;

  // 4) Estados "reales" de actuadores = comandos actuales
  PcToEsp cmdCopy;
  safeReadPcCmd(cmdCopy);
  state.u_fan_real = cmdCopy.u_fan_sim;
  state.u_heater_real = cmdCopy.u_heater_sim;
}

// Aplica los comandos recibidos desde Simulink
void applyPCCommands(const PcToEsp &cmd) {
  // Ventilador: PWM según u_fan_sim
  setFanPwm(cmd.u_fan_sim);

  // Calefactor: ON/OFF según u_heater_sim
  setHeaterOutput(cmd.u_heater_sim);

  // Alarma: usa alarm_flag (>0.5) para encender LED
  setAlarmOutputs(cmd.alarm_flag > 0.5f);
}

// ─────────────────────────────────────────────────────────────
// 7. Tareas FreeRTOS
// ─────────────────────────────────────────────────────────────

// TaskTX: actualiza espState y lo envía a Simulink cada TX_PERIOD_MS
void TaskTX(void *pvParameters) {
  (void)pvParameters;

  EspToPc localState;

  for (;;) {
    // 1) Tomamos la última versión de espState
    safeReadEspState(localState);

    // 2) Actualizamos sensores / inputs físicos
    updateSensorsAndInputs(localState);

    // 3) Guardamos la nueva versión
    safeUpdateEspState(localState);

    // 4) Copia "congelada" para enviar
    EspToPc toSend;
    safeReadEspState(toSend);

    // 5) Envío binario (32 bytes)
    // *** DESACTIVAMOS envío binario mientras probamos sin Simulink ***
    // Serial.write((uint8_t *)&toSend, sizeof(toSend));
    // Serial.flush();

    // DEBUG SOLO SIN SIMULINK:
    Serial.print("T_real=");
    Serial.print(localState.T_real);
    Serial.print("  RH_real=");
    Serial.print(localState.RH_real);

    Serial.print("  T_set=");
    Serial.print(localState.T_set);
    Serial.print("  RH_set=");
    Serial.print(localState.RH_set);

    Serial.print("  mode=");
    Serial.print(localState.mode_auto_manual);
    Serial.print("  hvac=");
    Serial.print(localState.hvac_enable);

    Serial.print("  u_fan_real=");
    Serial.print(localState.u_fan_real);
    Serial.print("  u_heater_real=");
    Serial.println(localState.u_heater_real);


    vTaskDelay(pdMS_TO_TICKS(TX_PERIOD_MS));
  }
}

// TaskRX: recibe estructuras PcToEsp (24 bytes) desde Simulink
void TaskRX(void *pvParameters) {
  (void)pvParameters;

  PcToEsp localCmd;

  for (;;) {
    if (Serial.available() >= (int)sizeof(PcToEsp)) {
      size_t n = Serial.readBytes(
        (uint8_t *)&localCmd,
        sizeof(PcToEsp));

      if (n == sizeof(PcToEsp)) {
        // Actualizamos comandos globales
        safeUpdatePcCmd(localCmd);

        // Aplicamos comandos a actuadores
        applyPCCommands(localCmd);

        // DEBUG SOLO SIN SIMULINK:
        Serial.print("Recibido u_fan_sim = ");
        Serial.println(localCmd.u_fan_sim, 3);
      } else {
        // DEBUG de error
        Serial.println("ERROR: lectura incompleta de PcToEsp");
       }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}