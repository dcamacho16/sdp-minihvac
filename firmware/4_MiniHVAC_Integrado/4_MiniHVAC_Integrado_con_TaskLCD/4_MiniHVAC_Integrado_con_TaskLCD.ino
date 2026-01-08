/*
Esta versión integra la pantalla LCD en la lógica del programa.
	•	Creamos una TaskLCD que cada ~2 s cambia de “pantalla”.
	•	Usamos dos pantallas:

Pantalla 0 – Mundo real / consignas (para demos MANUAL/AUTO)
	•	Fila 1: Tr:24.3 Rh:46 → T_real y RH_real.
	•	Fila 2: Ts:22  Rs:50 → T_set y RH_set.

Pantalla 1 – Estado de control (para demo AUTO)
	•	Fila 1: Md:AUTO H:ON → mode_auto_manual y hvac_enable.
	•	Fila 2: Fan:1.0 Ht:0 → u_fan_real y u_heater_real (aprox).

  NOTA: 
*/

#include <Arduino.h>
#include <DHTesp.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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
const int PIN_LED_HVAC = 27;
const int PIN_LED_MODE = 14;

// I2C LCD
const int I2C_SDA_LCD = 18;
const int I2C_SCL_LCD = 19;

// Dirección típica de las 16x2 I2C (podría ser 0x27 o 0x3F)
LiquidCrystal_I2C lcd(0x3F, 16, 2);

// Configuración PWM ventilador
const int FAN_PWM_CHANNEL = 0;
const int FAN_PWM_FREQUENCY = 25000;  // 25 kHz
const int FAN_PWM_RESOLUTION = 8;     // 8 bits (0..255)

// Periodo de envío de estado al PC (Simulink)
const uint32_t TX_PERIOD_MS = 100;  // 0.1 s

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

  pinMode(PIN_LED_HVAC, OUTPUT);
  pinMode(PIN_LED_MODE, OUTPUT);
  digitalWrite(PIN_LED_HVAC, LOW);
  digitalWrite(PIN_LED_MODE, LOW);
  // Potenciómetros: no necesitan pinMode

  //  I2C + LCD
  Wire.begin(I2C_SDA_LCD, I2C_SCL_LCD);
  lcd.init();       // inicializa el LCD
  lcd.backlight();  // enciende la luz de fondo
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MiniHVAC ESP32");
  lcd.setCursor(0, 1);
  lcd.print("Inicializando...");
  delay(3000);

  // PWM ventilador
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQUENCY, FAN_PWM_RESOLUTION);
  ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);  // ventilador apagado

  // Inicializamos estructuras
  memset(&pcCmd, 0, sizeof(pcCmd));
  memset(&espState, 0, sizeof(espState));

  // Estado inicial: MANUAL y HVAC encendido
  espState.mode_auto_manual = 1.0f;  // 1 = AUTO; 0 = MANUAL
  espState.hvac_enable = 1.0f;       // 1 = ON; 0 = OFF

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

  // TaskLCD con prioridad baja
  xTaskCreatePinnedToCore(
    TaskLCD,
    "TaskLCD",
    4096,
    NULL,
    0,  // prioridad más baja
    NULL,
    1);

  // Mensajes de depuración (sólo firmware, no Simulink)
  // Serial.print("sizeof(PcToEsp) = ");
  // Serial.println(sizeof(PcToEsp));
  // Serial.print("sizeof(EspToPc) = ");
  // Serial.println(sizeof(EspToPc));
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
  static uint32_t lastDhtMs = 0;
  const uint32_t DHT_PERIOD_MS = 2000;  // recomendado para DHT22

  if (millis() - lastDhtMs >= DHT_PERIOD_MS) {
    lastDhtMs = millis();

    TempAndHumidity th = dht.getTempAndHumidity();
    if (!isnan(th.temperature) && !isnan(th.humidity)) {
      state.T_real = th.temperature;
      state.RH_real = th.humidity;
    }
  }

  // 2) Potenciómetros
  int rawTset = analogRead(PIN_POT_TSET);
  int rawRHset = analogRead(PIN_POT_RHSET);

  state.T_set = mapFloat((float)rawTset, 0.0f, 4095.0f, 18.0f, 28.0f);    // ºC aprox.
  state.RH_set = mapFloat((float)rawRHset, 0.0f, 4095.0f, 30.0f, 70.0f);  // %RH aprox.

  // 3) Botones con toggle + debounce (flanco de bajada)
  static bool init = false;

  static bool modeState = false;  // 0=MANUAL, 1=AUTO
  static bool hvacState = true;   // 0=OFF,   1=ON

  static int lastRawMode = HIGH;
  static int lastRawHvac = HIGH;

  static int stableMode = HIGH;
  static int stableHvac = HIGH;

  static uint32_t lastChangeModeMs = 0;
  static uint32_t lastChangeHvacMs = 0;

  const uint32_t DEBOUNCE_MS = 40;

  if (!init) {
    modeState = (state.mode_auto_manual > 0.5f);
    hvacState = (state.hvac_enable > 0.5f);

    lastRawMode = stableMode = digitalRead(PIN_BTN_MODE);
    lastRawHvac = stableHvac = digitalRead(PIN_BTN_HVAC);

    init = true;
  }

  int rawMode = digitalRead(PIN_BTN_MODE);
  if (rawMode != lastRawMode) {
    lastChangeModeMs = millis();
    lastRawMode = rawMode;
  }
  if ((millis() - lastChangeModeMs) > DEBOUNCE_MS) {
    if (rawMode != stableMode) {
      stableMode = rawMode;

      // Flanco: al quedar estable en LOW => “pulsación”
      if (stableMode == LOW) {
        modeState = !modeState;
      }
    }
  }

  int rawHvac = digitalRead(PIN_BTN_HVAC);
  if (rawHvac != lastRawHvac) {
    lastChangeHvacMs = millis();
    lastRawHvac = rawHvac;
  }
  if ((millis() - lastChangeHvacMs) > DEBOUNCE_MS) {
    if (rawHvac != stableHvac) {
      stableHvac = rawHvac;

      if (stableHvac == LOW) {
        hvacState = !hvacState;
      }
    }
  }

  state.mode_auto_manual = modeState ? 1.0f : 0.0f;
  state.hvac_enable = hvacState ? 1.0f : 0.0f;
  digitalWrite(PIN_LED_HVAC, (state.hvac_enable > 0.5f) ? HIGH : LOW);
  digitalWrite(PIN_LED_MODE, (state.mode_auto_manual > 0.5f) ? HIGH : LOW);

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
  uint32_t lastTxMs = 0;

  for (;;) {
    // 1) Tomamos la última versión de espState
    safeReadEspState(localState);

    // 2) Actualizamos sensores / inputs físicos
    updateSensorsAndInputs(localState);

    // 3) Guardamos la nueva versión
    safeUpdateEspState(localState);

    // 4) Copia "congelada" para enviar, más envío binario (32 bytes)
    // *** DESACTIVAMOS envío binario mientras probamos sin Simulink ***
    // Enviar SOLO cada TX_PERIOD_MS
    if (millis() - lastTxMs >= TX_PERIOD_MS) {
      lastTxMs = millis();

      EspToPc toSend;
      safeReadEspState(toSend);
      Serial.write((uint8_t *)&toSend, sizeof(toSend));
      //Serial.flush();
    }

    // DEBUG SOLO SIN SIMULINK:
    // Imprime/envía cada TX_PERIOD_MS (por ejemplo 1000 ms)
    // if (millis() - lastTxMs >= TX_PERIOD_MS) {
    //   lastTxMs = millis();

    //   Serial.print("T_real=");
    //   Serial.print(localState.T_real);
    //   Serial.print("  RH_real=");
    //   Serial.print(localState.RH_real);

    //   Serial.print("  T_set=");
    //   Serial.print(localState.T_set);
    //   Serial.print("  RH_set=");
    //   Serial.print(localState.RH_set);

    //   Serial.print("  mode=");
    //   Serial.print(localState.mode_auto_manual);
    //   Serial.print("  hvac=");
    //   Serial.print(localState.hvac_enable);

    //   Serial.print("  u_fan_real=");
    //   Serial.print(localState.u_fan_real);
    //   Serial.print("  u_heater_real=");
    //   Serial.println(localState.u_heater_real);
    // }

    // Poll rápido para no perder pulsaciones
    vTaskDelay(pdMS_TO_TICKS(20));
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
        //   Serial.print("Recibido u_fan_sim = ");
        //   Serial.println(localCmd.u_fan_sim, 3);
        // } else {
        //   // DEBUG de error
        //   Serial.println("ERROR: lectura incompleta de PcToEsp");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void TaskLCD(void *pvParameters) {
  (void)pvParameters;

  uint8_t screen = 0;      // 0 = real/set, 1 = estado control
  EspToPc state;
  PcToEsp cmd;

  for (;;) {
    // Leemos copias coherentes de las estructuras compartidas
    safeReadEspState(state);  // T_real, RH_real, T_set, RH_set, modo, hvac...
    safeReadPcCmd(cmd);       // T_sim, RH_sim, u_fan_sim, etc. (por si luego las necesitamos en una mejora)

    lcd.clear();

    if (screen == 0) {
      // Pantalla 0: valores reales y consignas
      // Fila 1: Tr:24.3 Rh:46
      lcd.setCursor(0, 0);
      lcd.print("Tr:");
      lcd.print(state.T_real, 1);  // 1 decimal
      lcd.print(" Rh:");
      lcd.print(state.RH_real, 0); // % sin decimales

      // Fila 2: Ts:22 Rs:50
      lcd.setCursor(0, 1);
      lcd.print("Ts:");
      lcd.print(state.T_set, 0);
      lcd.print(" Rs:");
      lcd.print(state.RH_set, 0);

    } else if (screen == 1) {
      // Pantalla 1: modo, hvac y actuadores
      // Fila 1: Md:AUTO H:ON (o Md:MAN H:OFF)
      lcd.setCursor(0, 0);
      lcd.print("Md:");
      if (state.mode_auto_manual > 0.5f) {
        lcd.print("AUTO");
      } else {
        lcd.print("MAN ");
      }

      lcd.print(" H:");
      if (state.hvac_enable > 0.5f) {
        lcd.print("ON ");
      } else {
        lcd.print("OFF");
      }

      // Fila 2: Fan:1.0 Ht:0
      lcd.setCursor(0, 1);
      lcd.print("Fan:");
      lcd.print(state.u_fan_real, 1);
      lcd.print(" Ht:");
      lcd.print(state.u_heater_real, 0);
    }

    // Pasamos a la siguiente pantalla
    screen = (screen + 1) % 2;

    // Esperamos 2 segundos antes de refrescar
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}