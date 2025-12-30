#include <Arduino.h>

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

// Comandos recibidos del PC
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

const int PIN_FAN_PWM = 5;    // PWM para ventilador / motor DC
const int PIN_HEATER = 2;     // LED que simula calefactor
const int PIN_ALARM_LED = 4;  // LED/Buzzer de alarma

const int FAN_PWM_CHANNEL = 0;
const int FAN_PWM_FREQUENCY = 25000;  // 25 kHz
const int FAN_PWM_RESOLUTION = 8;     // 8 bits (0..255)

const uint32_t TX_PERIOD_MS = 1000;  // Enviar estado cada 1 s

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

// ─────────────────────────────────────────────────────────────
// 5. setup() y loop()
// ─────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(2000);  // Esperamos a que el puerto serie esté listo

  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_ALARM_LED, OUTPUT);

  digitalWrite(PIN_HEATER, LOW);
  digitalWrite(PIN_ALARM_LED, LOW);

  // Configuración del canal PWM para el ventilador
  ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQUENCY, FAN_PWM_RESOLUTION);
  ledcAttachPin(PIN_FAN_PWM, FAN_PWM_CHANNEL);
  ledcWrite(FAN_PWM_CHANNEL, 0);  // ventilador inicialmente apagado

  // Inicializamos las estructuras a cero
  memset(&pcCmd, 0, sizeof(pcCmd));
  memset(&espState, 0, sizeof(espState));

  // Estado inicial: MANUAL y HVAC habilitado
  espState.mode_auto_manual = 0.0f;  // 0 = MANUAL
  espState.hvac_enable = 1.0f;       // 1 = ON

  // Creamos tarea de transmisión de estado al PC
  xTaskCreatePinnedToCore(
    TaskTX,
    "TaskTX",
    4096,
    NULL,
    1,  // prioridad normal
    NULL,
    1);

  // Creamos tarea de recepción de comandos del PC
  xTaskCreatePinnedToCore(
    TaskRX,
    "TaskRX",
    4096,
    NULL,
    2,  // prioridad un poco más alta
    NULL,
    1);

  // Agregamos los siguientes prints para depuración del código
  // Serial.print("sizeof(PcToEsp) = ");
  // Serial.println(sizeof(PcToEsp));
  // Serial.print("sizeof(EspToPc) = ");
  // Serial.println(sizeof(EspToPc));
}

void loop() {
  // No usamos loop(), todas las funciones periódicas están en tareas FreeRTOS.
  vTaskDelay(pdMS_TO_TICKS(1000));
}

// ─────────────────────────────────────────────────────────────
// 6. Implementación de funciones auxiliares
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
  // Ejemplo simple: ON si level > 0.5
  digitalWrite(PIN_HEATER, (level > 0.5f) ? HIGH : LOW);
}

void setAlarmOutputs(bool alarm) {
  digitalWrite(PIN_ALARM_LED, alarm ? HIGH : LOW);
}

// Versión mínima de actualización de sensores:
//   - T_real: contador de 20 a 40 ºC que se incrementa 1 ºC por ciclo
//   - RH_real: valor fijo 50 %
//   - u_fan_real / u_heater_real: copiamos de los comandos actuales
void updateSensorsAndInputs(EspToPc &state) {
  static float fakeTemp = 20.0f;

  fakeTemp += 1.0f;
  if (fakeTemp > 40.0f) {
    fakeTemp = 20.0f;
  }

  state.T_real = fakeTemp;
  state.RH_real = 50.0f;

  PcToEsp cmdCopy;
  safeReadPcCmd(cmdCopy);

  state.u_fan_real = cmdCopy.u_fan_sim;
  state.u_heater_real = cmdCopy.u_heater_sim;
  state.T_set = cmdCopy.T_sim;
  state.RH_set = cmdCopy.RH_sim;
  // mode_auto_manual y hvac_enable ya estaban configurados
}

// Por ahora sólo usamos u_fan_sim para controlar el PWM del ventilador.
// Más adelante aquí meterás lógica AUTO/MANUAL, hvac_enable, alarm_flag, etc.
void applyPCCommands(const PcToEsp &cmd) {
  float duty = cmd.u_fan_sim; // Comentar para prueba minima sin Simulink conectado
  //float duty = 0.2f;  // Prueba sin Simulink
  setFanPwm(duty);

  // Opcional (para cuando extiendas la lógica):
  // setHeaterOutput(cmd.u_heater_sim);
  // setAlarmOutputs(cmd.alarm_flag > 0.5f);
}

// ─────────────────────────────────────────────────────────────
// 7. Implementación de tareas FreeRTOS
// ─────────────────────────────────────────────────────────────

// Tarea TaskTX: actualiza espState y lo envía al PC cada TX_PERIOD_MS
void TaskTX(void *pvParameters) {
  (void)pvParameters;

  EspToPc localState;

  for (;;) {
    // 1) Tomamos la última versión de espState
    safeReadEspState(localState);

    // 2) Actualizamos sensores / variables simuladas
    updateSensorsAndInputs(localState);

    // 3) Guardamos la nueva versión en la variable global
    safeUpdateEspState(localState);

    // 4) Hacemos una copia "congelada" para enviar
    EspToPc toSend;
    safeReadEspState(toSend);

    // 5) Enviamos los 32 bytes por el puerto serie
    Serial.write((uint8_t *)&toSend, sizeof(toSend));
    Serial.flush();  // opcional

    // DEBUG: mostrar T_real en texto usando Serial Monitor 
    // (Comentar las dos lineas al utilizar Simulink para imprimir con lineas limpias, sin basura)
    // Serial.print("T_real = ");
    // Serial.println(localState.T_real);

    // 6) Esperamos hasta el siguiente envío
    vTaskDelay(pdMS_TO_TICKS(TX_PERIOD_MS));
  }
}

// Tarea TaskRX: recibe estructuras PcToEsp (24 bytes) desde el PC
void TaskRX(void *pvParameters) {
  (void)pvParameters;

  PcToEsp localCmd;

  for (;;) {
    if (Serial.available() >= (int)sizeof(PcToEsp)) {
      size_t n = Serial.readBytes(
        (uint8_t *)&localCmd,
        sizeof(PcToEsp));

      if (n == sizeof(PcToEsp)) {
        // Actualizamos los comandos globales
        safeUpdatePcCmd(localCmd);

        // Aplicamos los comandos (por ahora: u_fan_sim → PWM)
        applyPCCommands(localCmd);

        // Mensaje de debug
        // Serial.print("Recibido u_fan_sim = ");
        // Serial.println(localCmd.u_fan_sim, 3);
      } else {
        // Serial.println("ERROR: lectura incompleta de PcToEsp");
      }
    }

    // Pequeño delay para evitar busy-wait
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}