# Appendix X. MATLAB TX/RX Encoding/Decoding Tests  
## Scripts `TEST_RX_From_ESP32_bytes.m` and `TEST_TX_To_ESP32_bytes.m`

### X.1. Purpose of this appendix

The purpose of this appendix is to document the tests performed in MATLAB to validate, in isolation, the encoding and decoding of the data packets exchanged between Simulink and the ESP32 through serial communication.

For this purpose, two scripts were developed:

- `TEST_RX_From_ESP32_bytes.m`  
- `TEST_TX_To_ESP32_bytes.m`

These tests are part of **Phase 2** of the project and are aligned with the following objectives:

- **Module-based implementation:** separately validate the data packing and unpacking logic of the `TX_To_ESP32` and `RX_From_ESP32` subsystems.  
- **Module-level testing:** verify that the bytes generated in MATLAB are equivalent to the bytes expected by the ESP32 firmware running FreeRTOS, before connecting the hardware.

---

### X.2. Context: `PcToEsp` and `EspToPc` data structures

Communication between Simulink and the ESP32 is based on two fixed-size C structures, composed only of `float` types (4 bytes each):

- **`PcToEsp`** (24 bytes, 6 fields): data sent by Simulink to the ESP32.  
- **`EspToPc`** (32 bytes, 8 fields): data sent by the ESP32 back to Simulink.

At the binary level, the `TX_To_ESP32` and `RX_From_ESP32` subsystems must generate and reconstruct exactly the same 24/32 bytes used by the FreeRTOS tasks in the firmware. The MATLAB tests focus precisely on verifying this *byte-by-byte* equivalence.

---

### X.3. Script `TEST_RX_From_ESP32_bytes.m`

#### X.3.1. Purpose

The script `TEST_RX_From_ESP32_bytes.m` makes it possible to **simulate the `EspToPc` packet** that the ESP32 would send to Simulink (32 bytes) without requiring the hardware to be connected. 

Its main purpose is to:

- Generate a `uint8[32]` vector in MATLAB that represents a valid `EspToPc` structure message.  
- Verify that the `RX_From_ESP32` subsystem can correctly decode that 32-byte vector and reconstruct the temperature, humidity, setpoint, and control-state values without loss of information.

#### X.3.2. Operation

The basic steps implemented in the script are:

1. **Definition of test values**  
   Numerical values are assigned to each field of the `EspToPc` structure (for example, `T_real`, `RH_real`, `u_fan_real`, `T_set`, etc.). These values are chosen so that they are easy to recognize (for example, 25.0 °C, 50.0 %RH, etc.) in order to simplify the subsequent verification.

2. **Construction of the `single` vector**  
   The values are grouped into a MATLAB vector of type `single`, respecting the **exact field order** of the C structure:

   ```matlab
   data_single = single([T_real, RH_real, u_fan_real, ... , hvac_enable]);
   ```

3. **Conversion to bytes (`uint8`) with `typecast`**  
   The `typecast` function is used to reinterpret the memory block of the `single` vector as a byte vector (`uint8`):

   ```matlab
   data_bytes = typecast(data_single, 'uint8');  % Result: uint8[32]
   ```

   This produces exactly the binary representation that would be sent through the serial port.

4. **Printing the vector for use in Simulink**  
   The script displays the vector in an easily copyable format:

   ```matlab
   uint8([0 0 200 65  0 0 72 66  ...  0 0 128 63])
   ```

   This vector can be pasted directly into a **Constant** block in the Simulink model and connected to the `RX_data` input of the `RX_From_ESP32` subsystem.

5. **Reconstruction of the values (internal verification)**  
   To ensure that the process is reversible, the script can perform the inverse step:

   ```matlab
   data_single_rec = typecast(data_bytes, 'single');
   ```

   and compare `data_single_rec` with the original values, verifying that no information has been lost and that the field order is correct.

#### X.3.3. Expected result

Running `TEST_RX_From_ESP32_bytes.m` produces:

- A `uint8[32]` vector representing a valid packet of the `EspToPc` structure.  
- Confirmation that, when this vector is used as the input of `RX_From_ESP32`, the output signals in Simulink match the values defined in MATLAB (real temperature, real humidity, setpoints, actuator states, etc.).

This test validates the **receive path** (ESP32 → Simulink) at the binary packing/decoding level.

---

### X.4. Script `TEST_TX_To_ESP32_bytes.m`

#### X.4.1. Purpose

The script `TEST_TX_To_ESP32_bytes.m` focuses on the opposite direction of communication: **validating the `PcToEsp` packet** that Simulink must send to the ESP32. 

Its main objectives are to:

- Generate in MATLAB a `uint8[24]` vector representing the expected content of the `PcToEsp` structure.  
- Compare this reference vector with the `tx_bytes` output of the `TX_To_ESP32` subsystem after a Simulink simulation, checking for a *byte-by-byte* match.

#### X.4.2. Operation

The basic steps implemented in the script are:

1. **Definition of simulated values**  
   Values are assigned to the fields of the `PcToEsp` structure:

   ```matlab
   T_sim, RH_sim, P_sim, u_fan_sim, u_heater_sim, alarm_flag
   ```

   These values represent the state and control signals that Simulink would send to the ESP32.

2. **Construction of the `single` vector**  
   A vector of type `single` is built with the fields in the same order as the C structure:

   ```matlab
   data_single = single([T_sim, RH_sim, P_sim, u_fan_sim, u_heater_sim, alarm_flag]);
   ```

3. **Conversion to bytes (`uint8`)**  
   `typecast` is used to convert this vector into 24 bytes:

   ```matlab
   expected_bytes_row = typecast(data_single, 'uint8');  % Result: uint8[24]
   ```

   This vector represents the packet that the ESP32 expects to receive through the serial port.

4. **Printing the vector for documentation / use in Simulink**  
   The script prints the vector in the following format:

   ```matlab
   uint8([0 0 200 65  0 0 72 66  ... ])
   ```

   which makes it easier to include in the documentation or use in other test blocks.

5. **Comparison with the Simulink model output (optional but recommended)**  
   If the `ESP32_Interface.slx` model has previously been simulated with the **Single simulation output** option enabled and a **To Workspace** block storing the `tx_bytes` signal, then the following is available:

   ```matlab
   out.tx_bytes
   ```

   The script can verify the match using:

   ```matlab
   isequal(expected_bytes_row, out.tx_bytes)
   ```

   - If the result is `1` (TRUE), this confirms that the `TX_To_ESP32` subsystem is generating exactly the same 24 bytes as those calculated in MATLAB.  
   - If there is a mismatch, the script can use index-by-index comparisons to identify the byte (or bytes) where the difference occurs, making debugging easier.

#### X.4.3. Expected result

Running `TEST_TX_To_ESP32_bytes.m` produces:

- A `uint8[24]` vector that acts as the “correct” reference for the `PcToEsp` structure.  
- Confirmation that the `tx_bytes` signal produced by `TX_To_ESP32` matches this reference vector, validating the **transmit path** (Simulink → ESP32) at the binary level.

---

### X.5. Integration with the Phase 2 workflow

These MATLAB tests are integrated into the Phase 2 workflow as follows:

1. **Preliminary test without hardware**  
   Before connecting the ESP32, the scripts are used to:
   - Generate and verify the `EspToPc` (RX) and `PcToEsp` (TX) packets.  
   - Adjust the field order, data type (`single`/`float`), and vector size in the `TX_To_ESP32` and `RX_From_ESP32` subsystems.

2. **Modular validation**  
   Each script makes it possible to independently validate:
   - The receive module (`RX_From_ESP32`) using simulated data.  
   - The transmit module (`TX_To_ESP32`) by comparing it against a reference calculated in MATLAB.

3. **Reduction of errors in the hardware phase**  
   When moving to the integration with the real ESP32, the likelihood of errors is significantly reduced because:
   - The byte encoding/decoding has already been verified.  
   - Any remaining failure is usually related to serial-port configuration, FreeRTOS task synchronization, or wiring, rather than the data packing stage.

---

### X.6. Conclusions of this appendix

The scripts `TEST_RX_From_ESP32_bytes.m` and `TEST_TX_To_ESP32_bytes.m` are key tools for meeting the **module-based implementation** and **module-level testing** objectives in Phase 2 of the project:

- They validate that Simulink and the ESP32 share the **same binary data format**, ensuring that the C structures (`PcToEsp` and `EspToPc`) are correctly aligned with the `uint8` vectors used in the communication subsystems.  
- They make it possible to detect early errors in field order, data types, or packet sizes without requiring the hardware.  
- They improve development traceability and reproducibility by documenting a clear method for checking the integrity of the serial communication before moving on to physical tests with the ESP32.

This appendix formally records the tests performed and their role within the overall Phase 2 validation strategy.


---


# Appendix B – Simulink–ESP32 Serial Communication Tests (Phase 2)

## B.0. Purpose of the tests

The purpose of this appendix is to document the tests performed to validate the **serial communication** between the mini-HVAC model in Simulink and the **ESP32** microcontroller, both on the Simulink side (Part B1) and on the firmware side with **FreeRTOS** (Part B2).

The tests were organized into two stages:

- **B1. Serial communication in Simulink**  
  Verify that the `TX_To_ESP32` and `RX_From_ESP32` subsystems correctly encode and decode the 24- and 32-byte data structures that will be exchanged with the ESP32.

- **B2. ESP32 firmware with FreeRTOS + PC–ESP32 serial communication**  
  Implement the same data structures on the ESP32, manage communication through the USB serial port, and verify real-time information exchange with Simulink, including control of a physical actuator (DC fan/motor using PWM).

---

## B1. Serial communication in Simulink

### B1.1. Reference data structures

Two C structures are defined to represent the packets exchanged between the PC (Simulink) and the ESP32:

```c
// Datos enviados desde Simulink al ESP32 (24 bytes)
struct PcToEsp {
  float T_sim;         // 1
  float RH_sim;        // 2
  float P_sim;         // 3
  float u_fan_sim;     // 4
  float u_heater_sim;  // 5
  float alarm_flag;    // 6 (0.0 ó 1.0)
}; // 6 * 4 = 24 bytes

// Datos enviados desde el ESP32 a Simulink (32 bytes)
struct EspToPc {
  float T_real;           // 1
  float RH_real;          // 2
  float u_fan_real;       // 3
  float u_heater_real;    // 4
  float T_set;            // 5
  float RH_set;           // 6
  float mode_auto_manual; // 7 (0.0=MANUAL, 1.0=AUTO)
  float hvac_enable;      // 8 (0.0=OFF,    1.0=ON)
}; // 8 * 4 = 32 bytes
```

In Simulink, these structures are represented as vectors of type `single`, which are then converted to `uint8` using byte packing/unpacking blocks.

---

### B1.2. `TX_To_ESP32` subsystem (Simulink → uint8[24] vector)

**Purpose:** pack the 6 `float` values of the `PcToEsp` structure into a `uint8[24]` vector with the same order and alignment as in C.

**Elements used:**

- Input blocks:
  - `T_sim`, `RH_sim`, `P_sim`, `u_fan_sim`, `u_heater_sim`, `alarm_flag` (type `single`).
- **Byte Pack** block (Simulink / Simulink Real-Time / Utility Blocks):
  - **Data type**: `single`.
  - **Number of inputs**: 6.
  - **Output data type**: `uint8`.
  - The signal connection order exactly matches the field order of the `PcToEsp` structure.
- Subsystem output: `uint8` vector of length 24 named `TX_data`.

**Test in B1:**

- Test vectors were defined in MATLAB:

  ```matlab
  pcValues = single([T_sim, RH_sim, P_sim, u_fan_sim, u_heater_sim, alarm_flag]);
  pcBytes_ref = typecast(pcValues, 'uint8');  % Referencia en MATLAB
  ```

- `pcBytes_ref` was compared with the output of the `TX_To_ESP32` subsystem using a Scope and the signal inspection tool, verifying that **all 24 bytes matched bit by bit**.

---

### B1.3. `RX_From_ESP32` subsystem (uint8[32] vector → Simulink)

**Purpose:** reconstruct the 8 `float` values of the `EspToPc` structure from a `uint8[32]` vector.

**Elements used:**

- Input: constant `uint8[32]` vector named `RX_data` (for the B1 test).
- **Byte Unpack** block:
  - **Output data type**: `single`.
  - **Number of outputs**: 8.
  - **Input data type**: `uint8`.
- Outputs: `T_real`, `RH_real`, `u_fan_real`, `u_heater_real`, `T_set`, `RH_set`, `mode_auto_manual`, `hvac_enable`.

**Test in B1:**

- A reference floating-point vector was defined in MATLAB:

  ```matlab
  espValues = single([T_real, RH_real, u_fan_real, u_heater_real, ...
                      T_set, RH_set, mode_auto_manual, hvac_enable]);
  espBytes = typecast(espValues, 'uint8');
  ```

- These 32 bytes were entered into Simulink through a **Constant (uint8 vector)** block connected to the `RX_From_ESP32` subsystem.
- A Scope and `Display` blocks were used to verify that the `Byte Unpack` outputs matched the original values, proving that the field order and the *little-endian* configuration were correct.

---

## B2. Integration with ESP32 and FreeRTOS

In Part B2, the ESP32 side is implemented while keeping the same `PcToEsp` and `EspToPc` structures and managing serial communication with **FreeRTOS** and several concurrent tasks.

### B2.1. General firmware description

The ESP32 firmware was developed in the **Arduino IDE** environment (board: *ESP32 Wrover Module*) and consists of:

- Definition of the `PcToEsp` and `EspToPc` structures, identical to those in B1.
- Global variables:
  - `PcToEsp pcCmd;` – commands received from Simulink.
  - `EspToPc espState;` – state periodically sent to Simulink.
- Synchronization mechanisms between tasks:
  - `portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;`
  - Helper functions `safeUpdatePcCmd`, `safeReadPcCmd`, `safeUpdateEspState`, `safeReadEspState`, which encapsulate critical sections using `portENTER_CRITICAL` and `portEXIT_CRITICAL`.
- Hardware configuration:
  - USB serial port: `Serial.begin(115200);`
  - PWM for fan/motor:
    - `PIN_FAN_PWM = 5`
    - `FAN_PWM_CHANNEL = 0`, frequency 25 kHz, 8-bit resolution (`ledcSetup` and `ledcAttachPin`).
  - Digital outputs:
    - `PIN_HEATER = 2` (heater LED).
    - `PIN_ALARM_LED = 4` (alarm LED/buzzer).

#### FreeRTOS tasks

Two main tasks were created and pinned to core 1 of the ESP32:

```cpp
xTaskCreatePinnedToCore(TaskTX, "TaskTX", 4096, NULL, 1, NULL, 1);
xTaskCreatePinnedToCore(TaskRX, "TaskRX", 4096, NULL, 2, NULL, 1);
```

- **`TaskTX`** (priority 1)  
  Periodically sends the `EspToPc` structure to Simulink.
- **`TaskRX`** (priority 2)  
  Receives `PcToEsp` structures from Simulink and applies the commands to the actuators (`u_fan_sim` → PWM).

These tasks replace the traditional Arduino `loop()` usage; the main loop remains empty (`vTaskDelay(1000);`).

---

### B2.2. Auxiliary control functions

#### Fan PWM control

```cpp
void setFanPwm(float duty) {
  if (duty < 0.0f) duty = 0.0f;
  if (duty > 1.0f) duty = 1.0f;

  int maxValue = (1 << FAN_PWM_RESOLUTION) - 1; // 255
  int pwmValue = (int)(duty * maxValue);
  ledcWrite(FAN_PWM_CHANNEL, pwmValue);
}
```

#### Heater and alarm control (minimal version)

```cpp
void setHeaterOutput(float level) {
  digitalWrite(PIN_HEATER, (level > 0.5f) ? HIGH : LOW);
}

void setAlarmOutputs(bool alarm) {
  digitalWrite(PIN_ALARM_LED, alarm ? HIGH : LOW);
}
```

#### Simulated sensor update

For the minimal test, an increasing temperature is simulated:

```cpp
void updateSensorsAndInputs(EspToPc &state) {
  static float fakeTemp = 20.0f;
  fakeTemp += 1.0f;
  if (fakeTemp > 40.0f) fakeTemp = 20.0f;

  state.T_real  = fakeTemp;
  state.RH_real = 50.0f;

  PcToEsp cmdCopy;
  safeReadPcCmd(cmdCopy);
  state.u_fan_real    = cmdCopy.u_fan_sim;
  state.u_heater_real = cmdCopy.u_heater_sim;
  state.T_set         = cmdCopy.T_sim;
  state.RH_set        = cmdCopy.RH_sim;
}
```

This function is called from `TaskTX`, so that `T_real` increases by 1 °C on each transmission and is reused in Simulink for the receive tests.

#### Applying PC commands

```cpp
void applyPCCommands(const PcToEsp &cmd) {
  float duty = cmd.u_fan_sim;  // [0.0, 1.0]
  setFanPwm(duty);

  // Extensible a calefactor y alarma:
  // setHeaterOutput(cmd.u_heater_sim);
  // setAlarmOutputs(cmd.alarm_flag > 0.5f);
}
```

---

### B2.3. `TaskTX` task (ESP32 → Simulink)

**Purpose:** periodically send the `EspToPc` structure packed in binary form (32 bytes) through `Serial`.

```cpp
const uint32_t TX_PERIOD_MS = 1000; // 1 segundo

void TaskTX(void *pvParameters) {
  (void) pvParameters;
  EspToPc localState;

  for (;;) {
    // 1) Leer y actualizar sensores simulados
    safeReadEspState(localState);
    updateSensorsAndInputs(localState);
    safeUpdateEspState(localState);

    // 2) Copiar estado "congelado" y enviarlo
    EspToPc toSend;
    safeReadEspState(toSend);

    Serial.write((uint8_t *)&toSend, sizeof(toSend));
    Serial.flush(); // opcional

    vTaskDelay(pdMS_TO_TICKS(TX_PERIOD_MS));
  }
}
```

In this way, Simulink receives one `EspToPc` sample every second, which in B2 is used to display `T_real` as an increasing counter.

---

### B2.4. `TaskRX` task (Simulink → ESP32)

**Purpose:** read `PcToEsp` structures (24 bytes) sent from Simulink and apply `u_fan_sim` as the PWM control value for the fan.

```cpp
void TaskRX(void *pvParameters) {
  (void) pvParameters;
  PcToEsp localCmd;

  for (;;) {
    if (Serial.available() >= (int)sizeof(PcToEsp)) {
      size_t n = Serial.readBytes(
        (uint8_t *)&localCmd,
        sizeof(PcToEsp)
      );

      if (n == sizeof(PcToEsp)) {
        safeUpdatePcCmd(localCmd);
        applyPCCommands(localCmd);

        // Mensaje de depuración (cuando el puerto no está ocupado por Simulink)
        Serial.print("Recibido u_fan_sim = ");
        Serial.println(localCmd.u_fan_sim, 3);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
```

---

### B2.5. Test hardware

The following setup was used for the minimal Phase 2 test:

- **ESP32 DevKit** (board configured in Arduino IDE as *ESP32 Wrover Module*).
- **5 V DC fan/motor** controlled using:
  - General-purpose NPN transistor (for example, 2N2222).
  - 1 kΩ base resistor between `GPIO 5` (PWM) and the transistor base.
  - Motor connected between +5 V and the transistor collector.
  - Transistor emitter connected to GND.
  - 1N4007 diode in antiparallel with the motor as a flyback diode (protection against voltage spikes).
- **LED + resistor** in parallel with the motor as a visual indicator of the PWM state.
- 5 V power supply (either the board’s own 5 V rail or an external supply), with **common ground** between the ESP32 and the motor power supply.

This setup makes it possible to clearly observe the effect of `u_fan_sim` on the fan speed and on the brightness of the LED connected to the transistor.

---

## B2.6. Test models in Simulink

Two independent test models were built to validate each communication direction:

1. `Test_RX_ESP32.slx` – reception of `EspToPc` (ESP32 → Simulink).
2. `Test_TX_ESP32.slx` – transmission of `PcToEsp` (Simulink → ESP32).

In both cases, Instrument Control Toolbox blocks are used to manage the computer’s serial port.

---

### B2.6.1. `Test_RX_ESP32` model (ESP32 → Simulink)

**Purpose:** read the `uint8[32]` vector sent by `TaskTX`, unpack it with `RX_From_ESP32`, and visualize `T_real` in real time.

**Main blocks:**

- **Serial Configuration**
  - `Port`: `"/dev/cu.usbserial-210"` (serial port name in macOS).
  - `Baud rate`: `115200`.
  - `Data bits`: 8.
  - `Parity`: none.
  - `Stop bits`: 1.
  - `Byte order`: `little-endian`.
  - `Flow control`: none.
  - `Timeout`: 10 s.

- **Serial Receive**
  - `Port`: same object as `Serial Configuration`.
  - `Data type`: `uint8`.
  - `Data length option`: `Length via dialog`.
  - `Data length`: 32 (size of `EspToPc`).
  - `Header` and `Terminator`: empty (not used).
  - `Sample time`: **1.0 s** (matches `TX_PERIOD_MS`).
  - Execution mode: *blocking*, waiting to receive the full number of bytes.

- **`RX_From_ESP32`** subsystem:
  - Input: `uint8[32]` vector from `Serial Receive`.
  - Internally: **Byte Unpack** block and outputs `T_real`, `RH_real`, etc.

- **Scope** for `T_real`:
  - Allows the simulated temperature evolution to be observed in real time.

**Solver configuration:**

- `Type`: `Fixed-step`.
- `Solver`: `discrete (no continuous states)`.
- `Fixed-step size`: `1.0`.
- `Stop time`: for example `60` s.

To make the simulation advance at a human-observable speed, **Simulation Pacing** was enabled at `1x`, so that 1 second of model time corresponds approximately to 1 second of real time.

**Observed result:**

- The Scope shows a `T_real` signal that:
  - Starts at 20 °C.
  - Increases by 1 °C every second until it reaches 40 °C.
  - Returns to 20 °C and repeats the cycle.
- This confirms that:
  - The ESP32 correctly sends the `EspToPc` structure (32 bytes) through `Serial.write`.
  - The `Serial Receive` block receives the complete vector.
  - The `RX_From_ESP32` subsystem and the `Byte Unpack` block correctly reconstruct the `float` values.

---

### B2.6.2. `Test_TX_ESP32` model (Simulink → ESP32)

**Purpose:** generate a `PcToEsp` structure in which only `u_fan_sim` varies through a `Step` block, send it through `Serial Send`, and verify that the ESP32 adjusts the fan PWM according to that value.

**Main blocks:**

- **Test constants**:
  - `T_sim` = 25.0
  - `RH_sim` = 50.0
  - `P_sim` = 1013.0
  - `u_heater_sim` = 0.0
  - `alarm_flag` = 0.0

- **Step for `u_fan_sim`**:
  - `Step time`: 10 s.
  - `Initial value`: 0.
  - `Final value`: 1.
  - `Sample time`: **0.1 s**.

- **`TX_To_ESP32`** subsystem:
  - Inputs: the previous signals converted to `single`.
  - Output: `uint8[24]` vector (`TX_data`).

- **Serial Configuration** (same as in the RX model):
  - `Port`: `"/dev/cu.usbserial-210"`.
  - `Baud rate`: `115200`, etc.

- **Serial Send**:
  - `Port`: the same as `Serial Configuration`.
  - `Header`: empty.
  - `Terminator`: `<none>`.
  - `Enable blocking mode`: enabled.
  - The block has no explicit sample-time parameter; it inherits the simulation rate and therefore sends one 24-byte packet **every 0.1 s**.

**Solver configuration:**

- `Type`: `Fixed-step`.
- `Solver`: `discrete (no continuous states)`.
- `Fixed-step size`: **0.1 s** (matches the Step sample time).
- `Stop time`: `30` s.
- **Simulation Pacing** enabled at `1x`, so the 30 s of simulation correspond to approximately 30 real seconds.

**Test procedure:**

1. The Arduino Serial Monitor is closed to release the `/dev/cu.usbserial-210` port.
2. `Test_TX_ESP32` is run for 30 s.
3. The ESP32 is running the `TaskTX` and `TaskRX` tasks. Specifically, `TaskRX`:
   - Waits until `Serial.available()` ≥ 24.
   - Reads 24 bytes into a `PcToEsp`.
   - Updates `pcCmd` and calls `applyPCCommands`.
4. `applyPCCommands` translates `u_fan_sim` (0 → 1) into a PWM duty cycle on pin 5 using `setFanPwm`.

**Observed results:**

- An **LED + resistor** and a **5 V DC motor with an NPN transistor** are connected to pin 5 (PWM):
  - From t = 0 to t ≈ 10 s:
    - `u_fan_sim = 0`.
    - PWM is practically 0%.
    - The motor and LED remain off.
  - From t ≈ 10 s onward:
    - The Step block changes `u_fan_sim` to 1.
    - The model sends `PcToEsp` structures with `u_fan_sim = 1`.
    - `TaskRX` adjusts PWM to 100%.
    - The motor starts spinning and the LED turns clearly on.
- The transition occurs visibly at 10 s of simulation (thanks to Simulation Pacing), demonstrating that:
  - The Simulink → ESP32 data flow is correct.
  - The `PcToEsp` structure is received without errors.
  - `u_fan_sim` is interpreted and correctly applied to the PWM power stage.

---

### B2.7. Note on serial-port debugging

During the tests, strange characters were observed in the Arduino Serial Monitor. This occurs because:

- The firmware sends binary structures (`EspToPc`) using `Serial.write`, which generates byte sequences that cannot be interpreted as ASCII text.
- At the same time, `Serial.print` and `Serial.println` are used for debugging.

When both message types (binary and text) coexist on the same port, the serial monitor mixes printable characters with arbitrary bytes, producing lines that appear corrupted. This behavior is **normal** and disappears when:

- Binary transmission is temporarily disabled to debug with text only, or
- The serial port is used exclusively for communication with Simulink (without opening the monitor).

---

## B2.8. Conclusions from the Phase 2 tests

- Part B1 made it possible to validate in isolation that the Simulink subsystems `TX_To_ESP32` and `RX_From_ESP32` correctly encode and decode the `PcToEsp` (24 bytes) and `EspToPc` (32 bytes) structures using `Byte Pack` and `Byte Unpack` with `single` format and `little-endian` order.
- In Part B2, the ESP32 firmware with FreeRTOS was implemented using the same structures, and the following was demonstrated:
  - In the **ESP32 → Simulink** direction, the periodic transmission of `EspToPc` is received and displayed as a `T_real` ramp in Simulink.
  - In the **Simulink → ESP32** direction, sending `PcToEsp` with a step in `u_fan_sim` makes it possible to control the PWM on pin 5 of the ESP32, with motor and LED activation observed from the specified instant onward (10 s of simulation).
- The combination of:
  - FreeRTOS tasks (`TaskTX` and `TaskRX`),
  - shared-data protection mechanisms (critical sections),
  - Simulink serial communication blocks (Serial Configuration / Serial Receive / Serial Send),
  - and the appropriate solver configuration (fixed-step, sampling times, and Simulation Pacing),

  ensures **deterministic and reproducible communication** between the mini-HVAC model in Simulink and the ESP32 microcontroller, meeting the Phase 2 objectives for modular implementation and module-level testing.


---

# ✅ README — TX/RX Serial Communication Tests between Simulink and ESP32

**Folder:** `Scripts_MATLAB/`

This document describes the MATLAB scripts included in this folder and explains the requirements and steps needed to correctly run the data encoding and decoding tests between:

- Simulink → ESP32 (`PcToEsp`, 24 bytes)  
- ESP32 → Simulink (`EspToPc`, 32 bytes)

These tests ensure that the following subsystems:

- `TX_To_ESP32`  
- `RX_From_ESP32`  

are correctly configured and that the sent/received packets match *bit by bit* with the C structures defined on the ESP32 under FreeRTOS.

---

## 📌 1. Structures used

### 1.1. `PcToEsp` structure (Simulink → ESP32, 24 bytes)

```c
struct PcToEsp {
    float T_sim;         // Temperatura simulada
    float RH_sim;        // Humedad simulada
    float P_sim;         // Nivel de ventilación/flujo simulado
    float u_fan_sim;     // Señal de control del ventilador
    float u_heater_sim;  // Señal de control del calefactor
    float alarm_flag;    // Alarma (0 ó 1)
};
// Total = 6 floats × 4 bytes = 24 bytes
```

---

### 1.2. `EspToPc` structure (ESP32 → Simulink, 32 bytes)

```c
struct EspToPc {
    float T_real;           // Temperatura medida
    float RH_real;          // Humedad medida
    float u_fan_real;       // Estado real del ventilador
    float u_heater_real;    // Estado real del calefactor
    float T_set;            // Setpoint de temperatura
    float RH_set;           // Setpoint de humedad
    float mode_auto_manual; // 0 = MANUAL, 1 = AUTO
    float hvac_enable;      // 0 = OFF, 1 = ON
};
// Total = 8 floats × 4 bytes = 32 bytes
```

---

## 📌 2. Included scripts

### 2.1. `TEST_RX_From_ESP32_bytes.m`

This script generates a `uint8[32]` vector that simulates a message sent from the ESP32 to Simulink.

**Script functions:**

- Defines simulated real values (`T_real`, `RH_real`, etc.).  
- Packs them as `single`.  
- Uses `typecast(single, 'uint8')` to convert them into 32 bytes.  
- Displays the final vector so it can be pasted into the **Constant → RX_data** block of the `RX_From_ESP32` subsystem.  
- Reconstructs the original values to verify that there is no loss of information.

**Expected result**

A vector such as:

```matlab
uint8([0 0 200 65  0 0 72 66  ...  0 0 128 63])
```

which is inserted into Simulink to check that the reconstructed outputs match the original values.

---

### 2.2. `TEST_TX_To_ESP32_bytes.m`

This script generates a `uint8[24]` vector equivalent to the packet that Simulink must send to the ESP32.

**Script functions:**

- Defines simulated values (`T_sim`, `RH_sim`, `u_fan_sim`, etc.).  
- Converts the data to 24 bytes using `typecast`.  
- Prints the vector so it is ready to paste into Simulink or include in documentation.  
- **Optional comparison:**
  - If `out.tx_bytes` is available from a simulation of the `TX_To_ESP32` subsystem,  
  - it compares it *byte by byte* against `expected_bytes_row`.  
  - It shows whether they are identical or indicates the position where they differ.

**Expected result**

A `uint8[24]` vector such as:

```matlab
uint8([0 0 200 65  0 0 72 66  ... ])
```

---

## 📌 3. Requirements for running the scripts

For both scripts to work correctly, the following are required:

### ✔ 3.1. MATLAB installed and working

Compatible with versions **R2022b** onward.

---

### ✔ 3.2. Scripts in a folder accessible to the *workspace*

Ideally:

```text
/MiProyecto/Scripts_MATLAB/
```

---

### ✔ 3.3. Comparison with Simulink (TX script only)

To compare the result with Simulink, you need to:

- Have simulated the `ESP32_Interface.slx` model.  
- Have the **Single simulation output** option enabled:  
  - *Model Settings → Data Import/Export → Single simulation output*.  
- Include a **To Workspace** block that exports the `tx_bytes` signal.

In this way, when the simulation ends, the following will appear in MATLAB:

```matlab
out.tx_bytes   % donde out es un SimulationOutput
```

---

### ✔ 3.4. No hardware required (ESP32)

These tests are purely mathematical and validate data encoding.  
The ESP32 does not need to be connected in order to run them.

---

## 📌 4. How to use these scripts

### Step 1 — Open MATLAB

Navigate to the project folder:

```matlab
cd Scripts_MATLAB
```

---

### Step 2 — Run an RX test (32 bytes)

```matlab
run TEST_RX_From_ESP32_bytes
```

**Expected result:**

- The 32 generated bytes are displayed in the console.  
- You can copy them directly into the Constant block in Simulink.

---

### Step 3 — Run a TX test (24 bytes)

```matlab
run TEST_TX_To_ESP32_bytes
```

If you have already run the Simulink simulation and have `out.tx_bytes`, you can compare:

```matlab
isequal(expected_bytes_row, out.tx_bytes)
```

It should return:

```matlab
ans =
     1    % (TRUE)
```

to indicate that both vectors are identical *byte by byte*.

---

## 📌 5. Educational purpose of these tests

These tests make it possible to:

### ✔ Validate before using hardware

They confirm that Simulink and the ESP32 will be speaking the same “binary language.”

### ✔ Ensure that the C *structs* are correctly aligned

If the bytes do not match, the FreeRTOS `TaskRX` task will misinterpret the data and the system will not behave as expected.

### ✔ Ensure reproducibility

Any change to the data structure (adding or removing fields, changing the order, etc.) can be quickly detected by running the scripts again and checking whether the byte vectors still match.

In this way, the development workflow is better documented and communication errors between Simulink and the ESP32 are easier to debug.
