# ESP32_Interface

## 1. Module purpose

The **ESP32_Interface** module acts as a **bidirectional communication bridge** between the MiniHVAC simulation model implemented in Simulink and the **ESP32** microcontroller, which is responsible for interacting with the real hardware (sensors, actuators, LCD, etc.).

In **phase 2** of the project, the main goal of this module is to:

- Define and establish the **communication protocol** between Simulink and the ESP32.
- Implement and validate, **independently**, data encoding and decoding in **fixed-length binary packets**:
  - **24 bytes** for commands from Simulink → ESP32.
  - **32 bytes** for measured states from ESP32 → Simulink.
- Ensure that the structures used in Simulink are fully compatible with the C structures that will be used in the ESP32 firmware (FreeRTOS) in later phases.

In later phases (B2 and beyond), this module will be connected to:

- C++/Arduino code running on the ESP32 (with FreeRTOS).
- Real temperature and humidity sensors.
- Physical actuators (fan, heater LED, buzzer, etc.).

In this phase 2, the focus has been on **modular implementation** and **internal validation in Simulink/MATLAB**, without depending yet on execution on real hardware.

---

## 2. Module interface

The **ESP32_Interface** module is implemented as a Simulink *subsystem* inside the global `MiniHVAC_Top` model. Its purpose is to encapsulate all the logic for:

- Packing simulated and control signals to send them to the ESP32.
- Unpacking values read from the ESP32 to reintegrate them into the MiniHVAC model.

### 2.1. Inputs (Inports / input signals)

- `T_sim` (double)
- `RH_sim` (double)
- `P_sim` (double)
- `u_fan_sim` (double, 0–1)
- `u_heater_sim` (double, 0–1)
- `alarm_flag` (boolean)

### 2.2. Outputs (Outports / output signals)

- `T_real` (double)
- `RH_real` (double)
- `u_fan_real` (double)
- `u_heater_real` (double)
- `T_set` (double)
- `RH_set` (double)
- `mode_auto_manual` (double interpreted as logical)
- `hvac_enable` (double interpreted as logical)

---

## 3. Internal model and implementation

### 3.1. Subsystem `TX_To_ESP32`

Responsible for packing simulated signals into 24 bytes (`uint8[24]`) using **Byte Pack**.

Order (`PcToEsp`):

1. `T_sim`
2. `RH_sim`
3. `P_sim`
4. `u_fan_sim`
5. `u_heater_sim`
6. `alarm_flag`

All signals are first converted to `single`.

### 3.2. Subsystem `RX_From_ESP32`

Responsible for reconstructing real signals from 32 bytes (`uint8[32]`) using **Byte Unpack**.

Order (`EspToPc`):

1. `T_real`
2. `RH_real`
3. `u_fan_real`
4. `u_heater_real`
5. `T_set`
6. `RH_set`
7. `mode_auto_manual`
8. `hvac_enable`

They are converted to `double` for use in the main model.

### 3.3. Type consistency

- Simulink operates in `double`.  
- Binary communication operates in `single` (4-byte floating point compatible with ESP32).  
- Explicit conversion is performed with **Data Type Conversion**.

---

## 4. Configuration and execution environment

- Type: Simulink Subsystem  
- Key blocks: Serial Transmit, Serial Receive, Byte Pack, Byte Unpack  
- Fixed packet length: 24 and 32 bytes  
- Alignment: 1 byte  
- Serial port used in tests: Serial 0  

> TODO: Define the final baud rate and execution mode for phase 3.

---

## 5. Module-level tests

### 5.1. RX test (32 bytes)

Using `TEST_RX_From_ESP32_bytes.m`:

- Real test values were generated.
- They were packed in MATLAB → `uint8[32]`.
- They were injected into `RX_data` through a Constant block.
- Correct reconstruction was verified (`T_real`, `RH_real`, …).

### 5.2. TX test (24 bytes)

Using `TEST_TX_To_ESP32_bytes.m`:

- Simulated values were defined.
- The model was executed to obtain `out.tx_bytes`.
- The bytes were compared with the expected bytes generated in MATLAB (exact match).

---

## 6. Connection with other modules

### 6.1. Receives signals from:
- `MiniHVAC_Zone`: `T_sim`, `RH_sim`, `P_sim`
- `Thermostat_Logic` and `Control_Simulink`: `u_fan_sim`, `u_heater_sim`
- `Alarms_Logic`: `alarm_flag`

### 6.2. Sends signals to:
- `Thermostat_Logic`: `T_real`, `RH_real`
- `Control_Simulink`: `T_set`, `RH_set`, `mode_auto_manual`, `hvac_enable`

### 6.3 General role

`ESP32_Interface` is the **Hardware-in-the-Loop gateway** that:
- Allows the ESP32 to control actuators and read real sensors.
- Allows Simulink to act as the simulated plant or as the main controller depending on the demo (AUTO/MANUAL).

---

## 7. Module conclusions

- The module **has been implemented and validated independently** in this phase.  
- Exact packet encoding and decoding according to C structures have been verified.  
- It is ready for integration with **FreeRTOS on ESP32** in the next phase.

> TODO: Add the results of the tests with real hardware in phase 3, as well as the full integration of the FreeRTOS tasks (TaskRX, TaskTX, TaskSensors).
