# FreeRTOS Firmware Architecture

## Phase 3 — Initial integration of the ESP32-Wrover firmware

### Integration objective

In Phase 3, the objective was to move from a purely simulated model in Simulink (Phase 2) to a fully integrated system with real hardware based on an ESP32-Wrover. To achieve this, firmware was developed that:

- Implements binary serial communication with Simulink through the `PcToEsp` and `EspToPc` structures.
- Manages sensors, actuators, and communication concurrently using FreeRTOS tasks.
- Exposes a basic user interface through potentiometers (setpoints), buttons (mode and HVAC enable), and LEDs/fan as physical actuators.

The firmware is broken down below into functional blocks, relating them to the integration, final testing, and optimization requirements specified by the instructor.

### Module integration in the ESP32 firmware

The code for integrating the ESP32-Simulink modules is located at:

```text
firmware/3_B2_FreeRTOS_Comunicacion/
B2_FreeRTOS_TX_RX_Integracion_v1/B2_FreeRTOS_TX_RX_Integracion_v1.ino
```

### Communication protocol: `PcToEsp` and `EspToPc` structures

The foundation of the Simulink integration is the exchange of two binary structures, defined in the code as follows (simplified):

```c
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
```

| Structure | Direction | Fields | Size |
|---|---|---:|---:|
| `PcToEsp` | Simulink/PC → ESP32 | 6 `float` | 24 bytes |
| `EspToPc` | ESP32 → Simulink/PC | 8 `float` | 32 bytes |

Compile-time checks verify that the sizes are exactly 24 and 32 bytes, guaranteeing bit-for-bit compatibility with the Byte Pack/Unpack blocks defined in Simulink during Phase 2.

The `PcToEsp` structure contains the simulated signals coming from the `MiniHVAC_Top` model (temperature, humidity, symbolic pressure, and fan and heater commands, in addition to `alarm_flag`), while `EspToPc` contains the real or user-interface signals: sensor readings (`T_real`, `RH_real`), actuator states, setpoints (`T_set`, `RH_set`), and mode flags (`mode_auto_manual`, `hvac_enable`).

### Shared variables and synchronization with FreeRTOS

The firmware keeps a global copy of the received commands (`pcCmd`) and of the state sent to the PC (`espState`). To avoid race conditions between the FreeRTOS tasks that read and write these structures, a mutual exclusion mechanism based on `portMUX_TYPE` is used.

Safe access functions are defined on top of this mutex (`safeUpdatePcCmd`/`safeReadPcCmd` and `safeUpdateEspState`/`safeReadEspState`), allowing the transmission (`TaskTX`) and reception (`TaskRX`) tasks to run in parallel without corrupting data. This meets the compatibility and robustness requirement between concurrent modules.

### Hardware integration from the firmware

The next firmware block defines all physical pins in the system, including actuators (PWM fan, heater/LED, alarm LED), the temperature and humidity sensor (DHT22), potentiometers for `T_set` and `RH_set`, and buttons for AUTO/MANUAL mode and HVAC enable.

In addition, a PWM channel is configured to control the fan at **25 kHz** with **8-bit** resolution, meeting the requirement to integrate physical actuator control in parallel with serial communication.

In the `setup()` function, the serial port (**115200 baud**), actuator pins, DHT sensor, digital inputs with `INPUT_PULLUP`, and fan PWM are initialized. The communication structures are also initialized to zero, and a coherent initial state is set (for example, MANUAL mode and HVAC enabled).

> Extraction note: in the text extracted from the F3/F4 documents, the symbolic pin identifiers (`PIN_FAN_PWM`, `PIN_DHT`, etc.) are preserved where they appear. The documents do not include a complete textual table with explicit GPIO numbers; therefore, no pin numbers not present in the source have been invented.

### Actuator abstraction functions

To decouple the logic from the rest of the code, abstraction functions such as `setFanPwm()`, `setHeaterOutput()`, and `setAlarmOutputs()` are introduced.

| Function | Responsibility |
|---|---|
| `setFanPwm(duty)` | Clamps the duty cycle to the `[0, 1]` range and converts it into an integer value according to the PWM resolution. |
| `setHeaterOutput(level)` | Enables or disables the heater pin based on a threshold. |
| `setAlarmOutputs(alarm)` | Turns the alarm LED/buzzer on or off according to the value of `alarm_flag`. |

This design makes future improvements easier (for example, changing the fan or heater hardware) without modifying the Simulink integration logic.

### Sensor and user-input acquisition

The `updateSensorsAndInputs(espState)` function brings together the temperature and humidity sensor reading, potentiometer reading for setpoints, and button reading for mode and global enable in one place:

- `T_real` and `RH_real` are read from the DHT22, checking that the values are not `NaN`.
- The potentiometer values are converted into approximate physical setpoints for `T_set` (for example, **18–28 ºC**) and `RH_set` (for example, **30–70 %RH**).
- The buttons are read to set `mode_auto_manual` and `hvac_enable`.
- `u_fan_real` and `u_heater_real` are updated with the latest commands received from Simulink.

With this, the ESP32 becomes the source of truth for the real variables sent to Simulink to close the loop with the modules designed in Phase 2.

### Applying commands received from Simulink

The `applyPCCommands(cmd)` function is responsible for closing the integration between the Simulink commands and the physical actuators. From the `PcToEsp` structure:

- `setFanPwm(cmd.u_fan_sim)` makes the fan follow the simulated command.
- `setHeaterOutput(cmd.u_heater_sim)` turns the heater/LED on or off.
- `setAlarmOutputs(cmd.alarm_flag > 0.5f)` activates the alarm LED/buzzer when Simulink detects an alarm condition.

This block implements the requirement to integrate serial communication with LED and actuator control in a single firmware application.

### Concurrent organization with FreeRTOS

#### Tasks and responsibility split

The firmware delegates almost all system work to two FreeRTOS tasks:

| Task | Relative priority | Responsibility |
|---|---|---|
| `TaskTX` | Low-medium | Periodically updates `espState` with sensor, potentiometer, and button readings, and sends the `EspToPc` structure to the PC every `TX_PERIOD_MS` milliseconds. |
| `TaskRX` | Slightly higher | Waits to receive 24 bytes (the size of `PcToEsp`) over the serial port and, once a complete packet has been received, updates the global commands (`safeUpdatePcCmd`) and calls `applyPCCommands()` to update the actuators. |

The `loop()` function is left practically empty, with a `vTaskDelay`, because all logic has been moved into the tasks. This satisfies the requirement for parallel communication and actuator handling based on FreeRTOS.

#### Timing management and compatibility with slow sensors

The code uses a `TX_PERIOD_MS` transmission period that is consistent with the speed of the DHT sensor and with a reasonable update frequency for the user interface. The typical latency of the DHT has been taken into account to:

- Avoid saturating the UART with unnecessary traffic.
- Give the sensor enough time to stabilize readings between samples.

This configuration addresses the requirement to optimize response time without sacrificing system stability.

### Phase 3 improvements and optimization

Throughout Phase 3, several design decisions were introduced to improve system efficiency and clarity:

- Use of compact structures (`PcToEsp` at 24 bytes and `EspToPc` at 32 bytes), reducing the required bandwidth and making debugging easier.
- Separation of responsibilities by function (`setFanPwm`, `setHeaterOutput`, `setAlarmOutputs`, `updateSensorsAndInputs`, `applyPCCommands`), making the firmware more maintainable.
- Protection of shared data through mutual exclusion mechanisms, avoiding race conditions between tasks.
- Assignment of reasonable task priorities (`TaskRX` with slightly higher priority than `TaskTX`), prioritizing command reception and application to improve perceived response time.

As potential future work, memory usage could be optimized further (by reducing temporary copies), filtering could be added to the DHT readings to improve measurement stability, and a third task could be added to manage an LCD screen without overloading the current tasks.

### Phase 3 summary and conclusions

In this Phase 3, the hardware-software integration of the MiniHVAC project was completed:

- The ESP32-Wrover was robustly connected to the `MiniHVAC_Top` model through a well-defined binary protocol.
- Serial communication, sensor reading, user input handling, and actuator control were successfully executed in parallel thanks to FreeRTOS.
- Full-operation tests were performed in different scenarios (AUTO mode with Simulink, local MANUAL mode), identifying and correcting synchronization, initial-state, and sampling-time issues.
- Improvement and optimization criteria were applied both at the code-organization level and in the management of timing and microcontroller resources.

As part of the final delivery, two videos were added showing the system operating in real time, where the interaction between Simulink, the ESP32, the sensors, and the physical actuators can be seen. These videos provide a visual demonstration of the integration achieved in this phase.

## Phase 4 — Final firmware evolution and TaskLCD

### Final integration objective

In Phase 3, the project moved from a purely simulated model in Simulink (Phase 2) to a system integrated with real hardware based on an ESP32-Wrover, through firmware capable of communicating with the PC over the serial port, reading sensors, and actuating a fan and a “heater” (LED).

In Phase 4, this integration was completed and refined by correcting the handling of the `hvac_enable` and `mode_auto_manual` buttons and adding a new `TaskLCD` task to display the system state in real time on an I2C LCD screen.

The specific objectives of this block were:

- Consolidate the firmware as the control core of the MiniHVAC prototype.
- Ensure robust and stable behavior of the user buttons, with reliable switching between modes and states.
- Provide a local LCD interface that complements monitoring in Simulink.
- Validate the complete system through tests and recorded video demonstrations.

### Final ESP32 code

The final ESP32 code is located at:

```text
final/MiniHVAC_Integrado_Simulink/MiniHVAC_Integrado_Simulink.ino
```

The general firmware organization is maintained from Phase 3: the `PcToEsp` and `EspToPc` structures are defined, the global variables are protected by a mutex (`pcCmd`, `espState`), and the input/output pins and FreeRTOS tasks (`TaskTX`, `TaskRX`, and now `TaskLCD`) are declared.

### Consolidated binary protocol

The foundation of the Simulink integration remains the exchange of the `PcToEsp` and `EspToPc` binary structures:

```c
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
```

Compile-time checks verify that the sizes are exactly 24 and 32 bytes, guaranteeing bit-for-bit compatibility with the Byte Pack/Unpack blocks defined in Simulink during Phase 2.

The `PcToEsp` structure contains the simulated signals coming from the `MiniHVAC_Top` model (temperature, humidity, symbolic pressure, and fan and heater commands, in addition to `alarm_flag`), while `EspToPc` contains the real or user-interface signals: sensor readings (`T_real`, `RH_real`), actuator states, setpoints (`T_set`, `RH_set`), and mode flags (`mode_auto_manual`, `hvac_enable`).

### Shared variables, mutex, and critical sections

The `pcCmd` and `espState` structures are shared by several tasks, so they are protected through a low-level mutex (`portMUX_TYPE`) and helper functions:

- `safeUpdatePcCmd()` / `safeReadPcCmd()`
- `safeUpdateEspState()` / `safeReadEspState()`

This allows `TaskTX`, `TaskRX`, and `TaskLCD` to run in parallel without producing race conditions, while maintaining the consistency of the data sent to the PC and displayed on the LCD.

### Pins, sensors, actuators, and LCD in the final firmware

In the final firmware, the pins used are declared for:

| Category | Signals / identifiers | Function |
|---|---|---|
| Actuators | `PIN_FAN_PWM` | PWM channel for the DC fan. |
| Actuators | `PIN_HEATER` | LED that simulates the heater. |
| Actuators | `PIN_ALARM_LED` | Alarm LED/buzzer. |
| Temperature and humidity sensor | `PIN_DHT` | Connection to a DHT22 sensor. |
| User interface | `PIN_POT_TSET`, `PIN_POT_RHSET` | Potentiometers for temperature and humidity setpoints. |
| User interface | `PIN_BTN_MODE`, `PIN_BTN_HVAC` | AUTO/MANUAL and HVAC ON/OFF buttons. |
| Status LEDs | `PIN_LED_HVAC`, `PIN_LED_MODE` | Visually reflect the state of the `hvac_enable` and `mode_auto_manual` flags. |
| I2C LCD screen | `I2C_SDA_LCD`, `I2C_SCL_LCD` | I2C pins for the 16x2 LCD. |
| I2C LCD screen | `LiquidCrystal_I2C lcd(0x3F, 16, 2)` | 16x2 LCD with I2C address `0x3F`. |

A PWM channel is configured for the fan at **25 kHz** with **8-bit** resolution, allowing smooth airflow control. In `setup()`, the serial port, pins, DHT sensor, LCD (including the welcome message “MiniHVAC ESP32 – Initializing…”), and FreeRTOS tasks are initialized.

In this version, the initial state is set to **AUTO** and **HVAC ON**, which is more consistent with the automatic-control demos.

### Actuator abstraction functions

To decouple the logic from the rest of the code, abstraction functions such as `setFanPwm()`, `setHeaterOutput()`, and `setAlarmOutputs()` are introduced.

| Function | Responsibility |
|---|---|
| `setFanPwm(duty)` | Clamps the duty cycle to the `[0, 1]` range and converts it into an integer value according to the PWM resolution. |
| `setHeaterOutput(level)` | Enables or disables the heater pin based on a threshold. |
| `setAlarmOutputs(alarm)` | Turns the alarm LED/buzzer on or off according to the value of `alarm_flag`. |

This design makes future improvements easier (for example, changing the fan or heater hardware) without modifying the Simulink integration logic.

### Sensor reading and button-handling correction

The `updateSensorsAndInputs(EspToPc &state)` function was expanded to include:

#### DHT22 reading

- An internal sampling period of **2000 ms** (`DHT_PERIOD_MS`) and a time counter (`lastDhtMs`) are used to avoid exceeding the sensor’s recommended frequency.
- `T_real` and `RH_real` are only updated if the readings are not `NaN`.

#### Potentiometer reading

ADC values are converted into physical ranges:

| Variable | Approximate physical range | Associated function |
|---|---:|---|
| `T_set` | 18–28 °C | `mapFloat()` |
| `RH_set` | 30–70 %RH | `mapFloat()` |

#### Buttons with toggle and debounce

The behavior observed in Phase 3, where the buttons did not correctly hold their state, was corrected.

A debounce algorithm is now implemented:

- Internal variables `rawMode`, `stableMode`, `lastChangeModeMs`, and analogous variables for HVAC.
- A minimum stabilization time (`DEBOUNCE_MS = 40 ms`) before accepting a change.
- The pushbuttons act on the falling edge (when the button remains stable at `LOW`), and each press toggles the Boolean variables `modeState` and `hvacState`.
- The states are initialized from `espState.mode_auto_manual` and `espState.hvac_enable`, so the initial state selected in `setup()` is respected.

#### Status LEDs

After updating `mode_auto_manual` and `hvac_enable`, the `PIN_LED_MODE` and `PIN_LED_HVAC` LEDs are turned on or off consistently with these flags.

#### Real actuator states

A safe read of `pcCmd` is performed using `safeReadPcCmd()`, and `u_fan_real` and `u_heater_real` are copied from the current simulated commands.

Thanks to this restructuring, the button state is well-defined and stable, the LEDs indicate the actual system mode, and Simulink receives clean state-change signals, as shown in the Phase 4 demos.

### Applying commands received from Simulink

The `applyPCCommands(cmd)` function is responsible for closing the integration between the Simulink commands and the physical actuators. From the `PcToEsp` structure:

- `setFanPwm(cmd.u_fan_sim)` makes the fan follow the simulated command.
- `setHeaterOutput(cmd.u_heater_sim)` turns the heater/LED on or off.
- `setAlarmOutputs(cmd.alarm_flag > 0.5f)` activates the alarm LED/buzzer when Simulink detects an alarm condition.

This block implements the requirement to integrate serial communication with LED and actuator control in a single firmware application.

### Concurrent organization with FreeRTOS and TaskLCD

#### `TaskTX` and `TaskRX` tasks with responsibility split

The firmware delegates almost all system work to two FreeRTOS tasks:

| Task | Relative priority | Responsibility |
|---|---|---|
| `TaskTX` | Low-medium | Periodically updates `espState` with sensor, potentiometer, and button readings, and sends the `EspToPc` structure to the PC every `TX_PERIOD_MS` milliseconds. |
| `TaskRX` | Slightly higher | Waits to receive 24 bytes (the size of `PcToEsp`) over the serial port and, once a complete packet has been received, updates the global commands (`safeUpdatePcCmd`) and calls `applyPCCommands()` to update the actuators. |

The `loop()` function is left practically empty, with a `vTaskDelay`, because all logic has been moved into the tasks. This satisfies the requirement for parallel communication and actuator handling based on FreeRTOS.

#### New `TaskLCD` task

The main new feature in Phase 4 is the addition of the `TaskLCD` task, which runs with low priority and is responsible exclusively for updating the LCD screen.

Each `TaskLCD` iteration performs:

1. A safe read of `espState` and `pcCmd`.
2. LCD clearing (`lcd.clear()`).
3. Selection of screen 0 or 1 based on a `screen` counter.

| Screen | Content | LCD lines |
|---|---|---|
| Screen 0 | Physical variables and setpoints | Row 1: `Tr:<T_real> Rh:<RH_real>`<br>Row 2: `Ts:<T_set> Rs:<RH_set>` |
| Screen 1 | Control and actuator state | Row 1: `Md:AUTO/MAN  H:ON/OFF` (mode and HVAC enable).<br>Row 2: `Fan:<u_fan_real> Ht:<u_heater_real>` |

The screen changes automatically every **~3 s**, giving the user an alternating view of the MiniHVAC physical state and control state without needing to look at the PC.

### Timing management and compatibility with slow sensors

The code uses a `TX_PERIOD_MS` transmission period that is consistent with the speed of the DHT sensor and with a reasonable update frequency for the user interface. The typical latency of the DHT has been taken into account to:

- Avoid saturating the UART with unnecessary traffic.
- Give the sensor enough time to stabilize readings between samples.

This configuration addresses the requirement to optimize response time without sacrificing system stability.

### Block II summary

In summary, the ESP32 integration block is in a functional and stable version, with:

- A consolidated binary protocol between Simulink and ESP32.
- Robust reading of sensors and user inputs (buttons and potentiometers).
- Actuator control (fan, heater, alarm LED) coordinated with Simulink commands.
- A new local LCD interface with two screens that significantly improves prototype usability.

## Extracted images used in this file

No images were inserted in this file because the firmware sections extracted from F3/F4 do not contain embedded screenshots in the corresponding text area. The images related to tests, schematics, and the prototype were placed in `09-hardware-build.md` and `10-integration-testing.md`.
