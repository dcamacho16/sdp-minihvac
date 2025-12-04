# 0\_Readme\_Firmware – ESP32 code organization

This document describes the proposed folder structure for the ESP32 firmware within the **MiniHVAC – Sistemas Digitales Programables (Programmable Digital Systems)** project.  
The goal is to clearly separate:

- Basic hardware tests.
- Simulink communication prototypes.
- FreeRTOS-based versions.
- Integrated versions of the complete Mini-HVAC system.
- Stable builds and the final prototype (in other project folders).

---

## 1. Location within the project

Recommended path (inside MATLAB-Drive or another project root folder):

```text
ESP32_Project_Final/
  1_Desarrollo/
    Firmware_ESP32/
      0_Readme_Firmware.md   ← este archivo
      ...
```

This `0_Readme_Firmware.md` file lives in the `Firmware_ESP32` folder and acts as a local index for all ESP32-related code.

---

## 2. Structure of `Firmware_ESP32`

The proposed structure is as follows:

```text
Firmware_ESP32/
  0_Readme_Firmware.md

  1_Basicos/
    Blink_Test/
      Blink_Test.ino
    Serial_Echo_Test/
      Serial_Echo_Test.ino
    (otras pruebas simples de pines, PWM, sensores, etc.)

  2_B1_Comunicacion_Serial/
    B1_Serial_Simulink_Minimo/
      B1_Serial_Simulink_Minimo.ino

  3_B2_FreeRTOS_Comunicacion/
    B2_FreeRTOS_TX_RX_Minimo/
      B2_FreeRTOS_TX_RX_Minimo.ino
    B2_FreeRTOS_Sensores_LCD/
      B2_FreeRTOS_Sensores_LCD.ino
      (u otros ficheros .h/.cpp asociados)

  4_MiniHVAC_Integrado/
    MiniHVAC_ESP32_v1/
      MiniHVAC_ESP32_v1.ino
    MiniHVAC_ESP32_v2/
      MiniHVAC_ESP32_v2.ino
    (iteraciones posteriores con mejoras)
```

### 2.1. `1_Basicos/`

This folder contains very simple sketches intended to:

- Check that the ESP32 board works correctly (Blink).
- Verify the serial port and communication with the PC (Serial Echo).
- Test PWM, analog input reading, individual sensors, etc.

These sketches are **not** used directly in the Simulink integration, but they are useful as quick references and for hardware debugging.

### 2.2. `2_B1_Comunicacion_Serial/`

This folder stores the sketches corresponding to **Part B1 – Serial communication in Simulink**, typically:

- Sending and receiving fixed binary frames without FreeRTOS.
- Using `Serial.readBytes()` and `Serial.write()` to validate that Simulink `Byte Pack/Unpack` matches the C structures (`struct`).

Example folder:

- `B1_Serial_Simulink_Minimo/`  
  Reference sketch that checks PC ↔ ESP32 communication using a single `loop()` (without tasks).

### 2.3. `3_B2_FreeRTOS_Comunicacion/`

Folder dedicated to **Part B2 – ESP32 code with FreeRTOS**.  

This is where the sketches are located that:

- Define the `PcToEsp` (24 bytes) and `EspToPc` (32 bytes) structures.
- Create FreeRTOS tasks for:
  - `TaskTX`: periodic transmission of the `EspToPc` state to the PC.
  - `TaskRX`: reception of `PcToEsp` commands from Simulink.
  - Later on, `TaskSensors`, `TaskLCD`, etc.

Recommended subfolders:

- `B2_FreeRTOS_TX_RX_Minimo/`  
  **Minimal test sketch**.  
  Expected functionality:
  - `EspToPc.T_real` is simulated as a counter that increases by 1 ºC every second.
  - Simulink receives that value through the `RX_From_ESP32` block and plots it.
  - Simulink sends a `PcToEsp` structure where only `u_fan_sim` changes.
  - The ESP32 receives `u_fan_sim` and adjusts a PWM output (`setFanPwm()`) on an output pin.
  - The same value is printed to the serial monitor for debugging.

- `B2_FreeRTOS_Sensores_LCD/` (suggested name)  
  Extended version in which the following are added:
  - Real sensor reading (for example DHT22 or BME280).
  - Updating `T_real`, `RH_real`, etc. in `espState`.
  - Displaying key variables on an I²C LCD (optional).
  - More complete use of `mode_auto_manual` and `hvac_enable`.

### 2.4. `4_MiniHVAC_Integrado/`

This folder stores the progressive firmware versions once they are already connected to the complete Simulink model:

- **MiniHVAC_ESP32_v1**  
  First functional integration:
  - Serial communication with structs.
  - Basic sensor reading.
  - Simple fan and heater control.

- **MiniHVAC_ESP32_v2**, `v3`, etc.  
  Improved versions:
  - Completed AUTO/MANUAL logic.
  - Use of alarm signals (`alarm_flag`) for buzzer/LED.
  - Possible integration with additional tasks (LCD, logging, etc.).

The version selected for the final prototype will later be copied to:

```text
3_Prototipo_Final/Firmware_ESP32/MiniHVAC_ESP32_Final/
```

---

## 3. Naming conventions

- Folder names that contain a sketch follow this pattern:

  ```text
  Fase_DescripcionCorta/
    Fase_DescripcionCorta.ino
  ```

  This makes it easier to open them directly from the **Arduino IDE** (the IDE requires the `.ino` file to have the same name as the folder).

- It is recommended to add a short comment header to each `.ino` indicating:
  - Project phase (B1, B2, integrated, etc.).
  - Objective of the sketch.
  - Date and version.

Example:

```cpp
// MiniHVAC_ESP32_v1.ino
// Fase: MiniHVAC integrado (v1)
// Descripción: Comunicación con Simulink mediante structs PcToEsp/EspToPc,
//              lectura básica de sensores y control de ventilador/calefactor.
// Autor: Daniel Camacho Nevárez
// Fecha: AAAA-MM-DD
```

---

## 4. Recommended workflow

1. **Test basic hardware**  
   Use the sketches in `1_Basicos/` to validate the board, pins, power supply, and sensors.

2. **Validate simple serial communication (B1)**  
   Work with the sketches in `2_B1_Comunicacion_Serial/` until:
   - Simulink and the ESP32 correctly send/receive the binary structures.
   - The sizes (24 bytes and 32 bytes) match the `Byte Pack/Unpack` blocks.

3. **Introduce FreeRTOS (B2)**  
   Move to `3_B2_FreeRTOS_Comunicacion/`:
   - Start with `B2_FreeRTOS_TX_RX_Minimo.ino`.
   - Add additional tasks only when the basic communication is stable.

4. **MiniHVAC integration**  
   Migrate to `4_MiniHVAC_Integrado/`:
   - Connect `MiniHVAC_Zone`, `Thermostat_Logic`, `Control_Simulink`, and `Alarms_Logic` with the firmware.
   - Tune parameters, sampling times, and task priorities.

5. **Promote a “final” version**  
   When a version has been validated:
   - Copy the complete folder to `2_Pruebas/Builds_OK/` with the date.
   - Copy the version selected for submission to  
     `3_Prototipo_Final/Firmware_ESP32/MiniHVAC_ESP32_Final/`.

---

## 5. Relationship with other project folders

- `1_Desarrollo/Modelos_Simulink/`  
  Contains the Simulink models:
  - `MiniHVAC_Zone`, `Thermostat_Logic`, `Control_Simulink`, `ESP32_Interface`, etc.
  - The firmware in `Firmware_ESP32` is designed to be compatible with the communication blocks (`TX_To_ESP32` and `RX_From_ESP32`).

- `1_Desarrollo/Scripts_MATLAB/`  
  Scripts used to:
  - Prepare model parameters.
  - Validate binary structures (`typecast(single(...), 'uint8')`).
  - Automate tests.

- `2_Pruebas/`  
  Records builds that have already been tested and their results (test logs, Scope captures, etc.).

- `3_Prototipo_Final/`  
  Contains only the final version of the Simulink model and the ESP32 firmware presented in the project report.

---

## 6. Notes for the project report

In the final documentation, this file can be referenced as:

> “The organization of the ESP32 firmware is described in the document  
> `1_Desarrollo/Firmware_ESP32/0_Readme_Firmware.md`, where the different phases (B1, B2, and MiniHVAC integration) and the correspondence between each sketch and the course requirements are detailed.”

This structure improves:

- Traceability between **requirements → modules → firmware versions**.
- Reproducibility of the tests performed.
- Clarity when explaining the evolution of the project in the written report.
