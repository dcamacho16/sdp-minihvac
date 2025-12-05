# `Alarms_Logic` Module

## 1. Module purpose

The `Alarms_Logic` module is part of the **MiniHVAC_Top** project for the course *Sistemas Digitales Programables* (Programmable Digital Systems). Its main function is to:

- **Monitor** the critical variables of the mini-HVAC climate-control system (temperature, humidity, control errors, and sensor status).
- **Detect abnormal or potentially hazardous conditions** using different alarm logic paths.
- **Generate individual alarm signals and a global alarm (`alarm_flag`)** that can be used both in simulation and in the hardware (ESP32: LED, buzzer, LCD, etc.).

This module was developed in **Phase 2** with the following objectives:

- **Modular implementation**: design `Alarms_Logic` as an independent subsystem in Simulink, decoupled from the control logic and the physical model.
- **Module-level testing**: validate the block specifically through controlled scenarios in Simulink before integrating it with the remaining blocks and with the ESP32.
- **Technical documentation**: provide a clear description of the module functionality, interface, and role within the MiniHVAC system.

### 1.1. Context within the MiniHVAC system

The top-level model `MiniHVAC_Top` includes the following main subsystems:

1. `MiniHVAC_Zone`  
   - Physical model of the HVAC zone (simulated temperature `T_sim`, simulated humidity `RH_sim`, pressure, etc.).

2. `Thermostat_Logic`  
   - Management of setpoints (`T_set`, `RH_set`), error calculation (`error_T`, `error_RH`), AUTO/MANUAL mode, and the enable signal `hvac_enable`.

3. `Control_Simulink`  
   - Implementation of the controllers (PI/PID or others) when the system is operating in AUTO mode.

4. `ESP32_Interface`  
   - Data exchange with the ESP32 microcontroller (real sensors `T_real`, `RH_real`, physical actuators, buttons, potentiometers, etc.).

5. `Alarms_Logic` (this module)  
   - Monitoring of the previous variables to generate **logical** safety and diagnostic alarms.

The design assumes **reasonable physical limits** for the variables:

- Temperature within the approximate working range of the prototype:  
  `T_phys_min` ≈ −20 °C, `T_phys_max` ≈ 60 °C.
- Relative humidity within the physical range [0 %, 100 %].

Based on these physical limits, the following thresholds are defined:

- Comfort/safety thresholds for temperature (`T_low_threshold`, `T_high_threshold`).
- Humidity thresholds (`RH_low_threshold`, `RH_high_threshold`), for example to represent:
  - **Dry winter scenario**: humidity that is too low (`alarm_RH_low`).
  - **Very humid environment**: risk of condensation or mold (`alarm_RH_high`).

The ESP32 will use the alarm signals, especially `alarm_flag`, to activate physical indicators:

- Red alarm LED.
- Audible buzzer.
- Specific messages on the LCD screen.

---

## 2. Module interface

### 2.1. Inputs (Inports / input signals)

| Signal             | Type   | Unit | Description                                                                                 |
|-------------------|--------|------|---------------------------------------------------------------------------------------------|
| `T_sim`           | double | °C   | Simulated zone temperature, provided by `MiniHVAC_Zone`.                                    |
| `RH_sim`          | double | %    | Simulated relative humidity of the zone, provided by `MiniHVAC_Zone`.                       |
| `T_set`           | double | °C   | Temperature setpoint defined by the user/ESP32, received from `Thermostat_Logic`.           |
| `RH_set`          | double | %    | Humidity setpoint defined by the user/ESP32.                                                |
| `error_T`         | double | °C   | Temperature error, calculated as `T_set − T_sim` in `Thermostat_Logic`.                     |
| `error_RH`        | double | %    | Humidity error, calculated as `RH_set − RH_sim` in `Thermostat_Logic`.                      |
| `T_real`          | double | °C   | Real temperature measured by the sensor connected to the ESP32, received via `ESP32_Interface`. |
| `RH_real`         | double | %    | Real humidity measured by the physical sensor (ESP32).                                      |
| `hvac_enable`     | bool   | –    | Global enable signal for the HVAC system.                                                   |
| `mode_auto_manual`| bool   | –    | Operating mode (for example, 0 = MANUAL, 1 = AUTO), generated in `Thermostat_Logic`.        |

> Note: In the current Phase 2 version, `T_set` and `RH_set` are used indirectly through `error_T` and `error_RH`. They are reserved for future extensions, such as out-of-range setpoint alarms.

### 2.2. Outputs (Outports / output signals)

| Signal                  | Type | Description                                                                                         |
|------------------------|------|-----------------------------------------------------------------------------------------------------|
| `alarm_flag`           | bool | Global alarm, produced by the logical OR of all internal alarms.                                    |
| `alarm_T_high`         | bool | Simulated temperature above the upper threshold `T_high_threshold`.                                 |
| `alarm_T_low`          | bool | Simulated temperature below the lower threshold `T_low_threshold`.                                  |
| `alarm_RH_high`        | bool | Simulated humidity above the threshold `RH_high_threshold`.                                         |
| `alarm_RH_low`         | bool | Simulated humidity below the threshold `RH_low_threshold`.                                          |
| `alarm_sensor_fault`   | bool | Sensor fault detected from `T_real`/`RH_real` outside the physical range or invalid values.         |
| *(optional)* `alarm_error_T_persist` | bool | Persistent temperature-error alarm, which may be kept as an internal signal or exposed as an output. |

---

## 3. Internal model and implementation

### 3.1. General module structure

`Alarms_Logic` is implemented as a **Simulink Subsystem** that encapsulates several internal subsystems:

- `Temp_Alarms`  
  **Temperature** alarm logic.

- `RH_Alarms`  
  **Relative humidity** alarm logic.

- `Error_Alarms`  
  Detection of **persistent control errors**, mainly on `error_T`.

- `Sensor_Alarms`  
  Detection of **sensor faults** based on `T_real` and `RH_real`.

- `Global_Alarm`  
  Combination of all internal alarms to generate the global signal `alarm_flag`.

Each subsystem uses only basic Simulink blocks: `Inport`, `Outport`, `Relational Operator`, `Compare to Constant`, `Logical Operator`, `Abs`, `Integrator`, `Data Type Conversion`, etc.

### 3.2. Temp_Alarms (temperature alarms)

**Internal inputs:**

- `T_sim`
- `hvac_enable`
- `T_high_threshold` (Constant)
- `T_low_threshold`  (Constant)

**Internal outputs:**

- `alarm_T_high`
- `alarm_T_low`

**Logic:**

- **High-temperature alarm**:

  - A `Relational Operator` block configured as `>` is used:
    - Input 1: `T_sim`
    - Input 2: `T_high_threshold`
    - Output: `T_above_high` (bool).
  - A `Logical Operator` block (AND) combines:
    - `T_above_high`
    - `hvac_enable`
  - The output of this AND block is `alarm_T_high`.

- **Low-temperature alarm**:

  - Another `Relational Operator` configured as `<` is used:
    - Input 1: `T_sim`
    - Input 2: `T_low_threshold`
    - Output: `T_below_low` (bool).
  - Again, a logical AND with `hvac_enable` produces `alarm_T_low`.

This ensures that temperature alarms are only activated when the system is enabled (`hvac_enable = 1`).

### 3.3. RH_Alarms (humidity alarms)

**Internal inputs:**

- `RH_sim`
- `hvac_enable`
- `RH_high_threshold` (Constant)
- `RH_low_threshold`  (Constant)

**Internal outputs:**

- `alarm_RH_high`
- `alarm_RH_low`

**Logic:**

- `alarm_RH_high` is activated when:
  - `RH_sim > RH_high_threshold` AND `hvac_enable = 1`.

- `alarm_RH_low` is activated when:
  - `RH_sim < RH_low_threshold` AND `hvac_enable = 1`.

This makes it possible to represent different use cases:

- **Winter, very dry environment**: `RH_sim` below `RH_low_threshold` will generate `alarm_RH_low`, anticipating the need for humidification.
- **Very humid environment**: `RH_sim` above `RH_high_threshold` will generate `alarm_RH_high`.

### 3.4. Error_Alarms (persistent control error)

**Internal inputs:**

- `error_T`
- `error_RH` (reserved for future extensions)
- `error_T_max` (Constant, acceptable margin for |error_T|)
- `error_T_persist_time` (Constant, minimum persistence time)
- `hvac_enable`
- `mode_auto_manual` (typically using `1` = AUTO)

**Relevant output:**

- `alarm_error_T_persist` (can be exposed externally or used only for the global OR).

**Logic:**

1. **Large-error detection:**

   - An `Abs` block is applied to `error_T` to obtain `abs_error_T`.
   - A `Relational Operator` (`>`) does the following:
     - Compares `abs_error_T` with `error_T_max`.
     - Produces `cond_error_grande` (bool).
   - For the condition to be relevant, the following are also required:
     - `mode_auto_manual == AUTO` (for example, comparison to 1 using `Compare to Constant`).
     - `hvac_enable = 1`.
   - These conditions are combined using `Logical Operator` blocks (AND) to obtain `cond_error_valid`.

2. **Error-time integration:**

   - `cond_error_valid` is converted to type `double` (0.0/1.0) using `Data Type Conversion`.
   - The resulting signal feeds an `Integrator` block, whose output is interpreted as:
     - `t_error_accum` = time during which the error has been large and the system has been enabled in AUTO mode.
   - The integrator is configured with an external reset:
     - When `cond_error_valid` returns to 0, a reset signal restarts `t_error_accum`.

3. **Persistent-alarm triggering:**

   - A `Relational Operator` (`>`) compares:
     - `t_error_accum` with `error_T_persist_time`.
   - If the accumulated time exceeds the threshold, `alarm_error_T_persist` is activated.

This logic avoids generating alarms for short or transient errors and focuses attention on **sustained control failures**, which are typical of incorrect configurations, actuator faults, or unreachable setpoints.

### 3.5. Sensor_Alarms (sensor fault)

**Internal inputs:**

- `T_real`
- `RH_real`
- `T_phys_min`, `T_phys_max` (temperature physical-limit constants)
- `RH_phys_min`, `RH_phys_max` (humidity physical-limit constants)

**Output:**

- `alarm_sensor_fault`

**Logic:**

- The module checks that `T_real` and `RH_real` are within their physical ranges:

  - Temperature:
    - `T_real < T_phys_min` → `T_real_low`
    - `T_real > T_phys_max` → `T_real_high`
    - OR of both → `T_real_out_of_range`

  - Humidity:
    - `RH_real < RH_phys_min` → `RH_real_low`
    - `RH_real > RH_phys_max` → `RH_real_high`
    - OR of both → `RH_real_out_of_range`

- In addition, non-numeric values (`NaN`) can be checked using specific blocks or comparisons such as `T_real ~= T_real`.

- Finally, a global OR between:
  - `T_real_out_of_range`, `RH_real_out_of_range`, and possible `NaN` detections produces `alarm_sensor_fault`.

This alarm **is not conditioned by `hvac_enable`**, because a sensor fault is relevant even if the HVAC system is temporarily off.

### 3.6. Global_Alarm (alarm combination)

The `Global_Alarm` subsystem is responsible for generating the global alarm `alarm_flag`.

- Inputs:
  - `alarm_T_high`
  - `alarm_T_low`
  - `alarm_RH_high`
  - `alarm_RH_low`
  - `alarm_error_T_persist`
  - `alarm_sensor_fault`
  - *(optional)* other future alarms, such as communication failure.

- Logic:
  - A `Logical Operator` block configured as OR with multiple inputs computes:

    ```text
    alarm_flag = OR(alarm_T_high,
                    alarm_T_low,
                    alarm_RH_high,
                    alarm_RH_low,
                    alarm_error_T_persist,
                    alarm_sensor_fault, ...)
    ```

- Depending on design decisions, `alarm_flag` may:
  - Directly take the output of the previous OR, or
  - Be masked with `hvac_enable` through an additional AND to ignore operating alarms when the system is disabled.

In Phase 2, `Global_Alarm` was configured so that the behavior of `alarm_flag` is **consistent with the temperature and humidity tests**, preventing unrelated alarms, for example humidity or sensor alarms, from interfering with the unit tests.

### 3.7. Main parameters

The most relevant parameters, implemented as `Constant` blocks, are:

- `T_high_threshold` : Upper threshold for simulated temperature.  
- `T_low_threshold`  : Lower threshold for simulated temperature.
- `RH_high_threshold`: Upper threshold for simulated relative humidity.
- `RH_low_threshold` : Lower threshold for simulated relative humidity.
- `error_T_max`      : Maximum allowed margin for `|error_T|` before considering the error large.
- `error_T_persist_time` : Minimum persistence time for the large error before triggering `alarm_error_T_persist`.
- `T_phys_min`, `T_phys_max` : Physical temperature limits used to validate `T_real`.
- `RH_phys_min`, `RH_phys_max` : Physical humidity limits used to validate `RH_real`.

> The specific numerical values can be adjusted depending on the application, for example comfort thresholds versus safety thresholds. In the Phase 2 tests, reasonable values adapted to the simulation range of the `MiniHVAC_Zone` model were used.  
> *(TODO: document the final values selected in the final version of the model).*

---

## 4. Configuration and execution environment

- **Module type:**  
  Simulink subsystem inside the `MiniHVAC_Top` model.

- **Environment:**  
  - MATLAB / Simulink.
  - Version consistent with the rest of the *Sistemas Digitales Programables* practice.

- **Simulation configuration (Phase 2):**
  - Solver: configured consistently with the rest of the model, for example `Fixed-step`.
  - Integration step: typical value of **0.1 s** in the tests performed.
  - Duration of the test simulations: adjusted according to the case, on the order of 200–700 s to allow the observation of steps and persistent errors.

- **Associated hardware (planned for later phases):**
  - **ESP32** microcontroller, responsible for:
    - Providing `T_real` and `RH_real` through `ESP32_Interface`.
    - Receiving `alarm_flag` to drive an LED, buzzer, and LCD messages.
  - In Phase 2, the connection with the ESP32 is modeled at the logical level; hardware tests are considered part of the later integration work.

---

## 5. Module-level tests

### 5.1. Test objective

- Verify the behavior of `Alarms_Logic` **independently**, isolating the module from the rest of the system.
- Confirm that:
  - The temperature and humidity thresholds trigger the corresponding alarms.
  - The persistent-error alarm only activates when the error exceeds a certain duration.
  - The sensor-fault alarm detects readings outside the physical range.
  - The combination in `alarm_flag` is consistent with the individual alarms.

### 5.2. Test cases in Simulink

The following table summarizes the main test cases defined for Phase 2. In all cases, the remaining signals not involved in the test are fixed at nominal values that do not trigger other alarms, for example `RH_sim` within range, sensors within physical limits, etc.

| ID | Description                      | Main configuration                                                                                                            | Expected behavior                                                                                           | Observations                                                |
|----|----------------------------------|------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------|
| T1 | High-temperature alarm           | `T_sim` connected to a `Step` block: initial value 24 °C, final value 35 °C, `Step time = 50 s`. `T_high_threshold` ≈ 28 °C. `hvac_enable = 1`. | `alarm_T_high = 0` for `t < 50 s`. From `t ≥ 50 s`, `alarm_T_high = 1` and `alarm_flag = 1`.                | Initially, `alarm_flag` remained at 1 because of a humidity alarm; this was corrected by setting `RH_sim` to a nominal value. |
| T2 | Low-temperature alarm            | `T_sim` changes from 24 °C to a value below `T_low_threshold`, for example 10–12 °C, at a user-defined instant.       | `alarm_T_low` changes from 0 to 1 when `T_sim < T_low_threshold` with `hvac_enable = 1`. `alarm_flag` also activates.| –                                                            |
| H1 | Low-humidity alarm               | `RH_sim` configured using `Step` or `Constant` to a value below `RH_low_threshold`, with `hvac_enable = 1`.                 | `alarm_RH_low = 1` while `RH_sim < RH_low_threshold`. `alarm_flag = 1`.                                         | Relevant case for a dry winter scenario.                    |
| H2 | High-humidity alarm              | `RH_sim` forced above `RH_high_threshold`.                                                                                | `alarm_RH_high = 1` in the out-of-range region. `alarm_flag = 1`.                                                 | –                                                            |
| E1 | Persistent T-error alarm         | `error_T` forced to a constant value with `|error_T| > error_T_max`. `hvac_enable = 1`, `mode_auto_manual = AUTO`.               | After a time greater than `error_T_persist_time`, `alarm_error_T_persist = 1` and `alarm_flag = 1`.                     | Confirms correct integrator operation.                      |
| S1 | Sensor fault (temperature)       | `T_real` set to a value outside `[T_phys_min, T_phys_max]`, for example 100 °C. `RH_real` within nominal range.                   | `alarm_sensor_fault = 1` and `alarm_flag = 1`.                                                                       | Validates the defined physical limits.                      |
| S2 | Correct sensor readings          | `T_real` and `RH_real` within their physical ranges.                                                                                | `alarm_sensor_fault = 0`. `alarm_flag` depends only on other alarms, if any.                       | –                                                            |

In all cases, a **Scope** was used to visualize the following signals simultaneously:

- The input signal under test, for example `T_sim`, `RH_sim`, or `error_T`.
- The individual alarms (`alarm_T_high`, `alarm_T_low`, `alarm_RH_high`, `alarm_RH_low`, `alarm_error_T_persist`, `alarm_sensor_fault`).
- The global alarm `alarm_flag`.

### 5.3. Hardware tests (ESP32)

In Phase 2, the hardware test scenarios were defined, but their execution remains pending until the complete integration with the ESP32:

- Reading `T_real` and `RH_real` from the physical sensor.
- Sending `alarm_flag` to the ESP32 to:
  - Turn on the red alarm LED.
  - Activate/deactivate the buzzer.
  - Display warning messages on the LCD screen.

*(TODO: document the results of the ESP32 tests once the hardware-in-the-loop integration phase is complete.)*

---

## 6. Connection with other project modules

### 6.1. Signals received by `Alarms_Logic`

- From `MiniHVAC_Zone`:
  - `T_sim`, `RH_sim`: simulated variables of the HVAC zone.

- From `Thermostat_Logic`:
  - `T_set`, `RH_set`: setpoints established by the user or ESP32.
  - `error_T`, `error_RH`: calculated errors.
  - `hvac_enable`: global enable signal.
  - `mode_auto_manual`: operating mode (AUTO/MANUAL).

- From `ESP32_Interface`:
  - `T_real`, `RH_real`: real measurements from the physical sensors connected to the ESP32.

### 6.2. Signals delivered by `Alarms_Logic`

- To the top-level model `MiniHVAC_Top`:
  - Individual alarm signals (`alarm_T_high`, `alarm_T_low`, `alarm_RH_high`, `alarm_RH_low`, `alarm_sensor_fault`, etc.) that can be used for diagnostics or to condition other logic, for example disabling actuators.

- To the `ESP32_Interface` block, and therefore to the ESP32:
  - `alarm_flag` as a summarized global alarm signal, used to:
    - Control an alarm LED.
    - Activate a buzzer.
    - Change the message shown on the LCD screen (user alerts).

### 6.3. Role of the module within the global HVAC system

`Alarms_Logic` acts as a **safety and supervision layer** over:

- The **physical model** (`MiniHVAC_Zone`), detecting extreme temperature/humidity conditions.
- The **control logic** (`Thermostat_Logic` + `Control_Simulink`), detecting persistent tracking errors.
- The **real instrumentation** (sensors via ESP32), detecting inconsistent or failed readings.

In the planned demonstrations, for example the **dry winter environment scenario**, this module will make it possible to:

- Show how the system reacts when temperature or humidity leaves the defined range.
- Visually identify, both in Simulink and in the hardware, when a risky condition or fault has occurred.

---

## 7. Module conclusions

- The `Alarms_Logic` module has been designed and implemented as an **independent subsystem in Simulink**, meeting the Phase 2 objective of **modular implementation**.
- **Specific module-level tests** have been defined and executed in Simulink to validate:
  - Correct detection of temperature and humidity alarms.
  - Persistent control-error logic based on time integration.
  - Detection of sensor faults using physical limits.
  - Combination of all alarms into the global signal `alarm_flag`.
- During validation, initial configurations that caused constant alarms were detected and corrected, for example default values outside the range in `RH_sim`, demonstrating the iterative debugging and tuning process for the module.
- From a **technical documentation** standpoint, the module:
  - Presents a well-defined interface (inputs/outputs).
  - Clearly describes the internal logic through simple subsystems and configurable parameters.
  - Is ready to be integrated with the rest of the system (`MiniHVAC_Zone`, `Thermostat_Logic`, `Control_Simulink`, `ESP32_Interface`).

**Current module status:**

- **Simulation level:** validated for the test cases described.
- **ESP32 integration level:** pending completion in later phases (sending/receiving real alarms, tests with physical sensors and actuators).

**Relevance for future phases:**

- `Alarms_Logic` will be key to:
  - Protecting the system against undesired operating conditions.
  - Providing diagnostic information to the user, both in Simulink and in the hardware.
  - Serving as the basis for extensions such as:
    - Communication-failure alarms.
    - Specific alarms for out-of-range setpoints.
    - Alarm acknowledgment/silencing functions from the ESP32.

In this way, the module is prepared and documented for integration in the **hardware-in-the-loop with ESP32** phase and for future extensions of the MiniHVAC project.
