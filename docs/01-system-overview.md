# Global Overview of the Mini-HVAC Model

## 1. General Purpose of the System

The **Mini-HVAC** model developed for the course *Sistemas Digitales Programables* (Programmable Digital Systems) is intended to represent, in a simplified but functional way, the basic behavior of an HVAC system (heating, ventilation, and conditioning) and its integration with an **ESP32** microcontroller for Hardware-in-the-Loop (HIL) testing.

The design has been explicitly structured into **five main modules (subsystems)**, each developed and tested independently as required by **Phase 2** of the project:

1. **MiniHVAC_Zone**  
   Simulates the evolution of the system temperature, humidity, and pressure/flow.

2. **Thermostat_Logic**  
   Implements the thermostat logic, AUTO/MANUAL modes, and generation of desired control signals.

3. **Control_Simulink**  
   Contains auxiliary controllers (PI/PID) that operate in AUTO mode (Demo 2).

4. **Alarms_Logic**  
   Detects alarm conditions and generates the `alarm_flag` signal.

5. **ESP32_Interface**  
   Handles Simulink ↔ ESP32 binary communication, packs and unpacks data, and enables interaction with real hardware.

The system is designed to support different operating modes, including full simulation in a digital environment, HIL tests with the ESP32, and control demonstrations under realistic scenarios such as **dry winter**, overheating, control limited by simple actuators, and related cases.

---

## 2. General Architecture of the Mini-HVAC Model

The Mini-HVAC system architecture follows a clear modular structure that supports the progressive integration of controllers, real sensors, and communication logic. Each block has a defined role within the system’s functional flow:

```
        +--------------------+
        |   Thermostat_Logic |
        +---------+----------+
                  |
                  v
         +--------+---------+
         |  Control_Simulink|
         +--------+---------+
                  |
                  v
        +---------+-----------+
        |   MiniHVAC_Zone     |
        +---------+-----------+
                  |
                  v
        +---------+-----------+
        |   ESP32_Interface   |
        +---------+-----------+
                  |
        (Sensores/Actuadores reales)
```

Each block exchanges standardized signals, which makes it possible to test and validate each subsystem individually before full integration.

---

## 3. Description of Each Subsystem

### 3.1. MiniHVAC_Zone

**Main function:**  
Simulate the thermal and humidity dynamics of the Mini-HVAC system for a small zone (room/rack).  
It includes the following processes:

- Thermal gain/input from the heater (`u_heater_sim`)
- Natural losses to the environment (`T_amb`)
- Ventilation effect (`u_fan_sim`)
- Simplified humidity dynamics (`RH_sim`)
- Auxiliary pressure/flow variable (`P_sim`)

**Assumed physical limitations:**

- Low-complexity dynamics (first order)
- Advanced saturation, condensation, or latent heat effects are not modeled (Phase 3)
- Interactions are limited to a small set of actuators

This block is the simulated “plant” of the system.

---

### 3.2. Thermostat_Logic

**Main function:**  
Take simulated or real temperature and humidity values and produce desired control signals according to the selected mode:

- **MANUAL mode:**  
  The ESP32 acts as the generator of `u_fan_real`, `u_heater_real`, `T_set`, and related signals.

- **AUTO mode:**  
  The Simulink controller (`Control_Simulink`) drives the simulated and/or real actuators.

**Key functions:**

- Comparison between `T_sim`/`T_real` and `T_set`
- Comparison between `RH_sim`/`RH_real` and `RH_set`
- Generation of thermal/hygrometric errors
- Derivation of basic control signals
- General state management: enable signal (`hvac_enable`) and alarms

TODO: Complete with the final equations during the integration phase.

---

### 3.3. Control_Simulink

**Main function:**
Provide configurable PI/PID controllers to act on:

- `u_fan_sim`
- `u_heater_sim`
- `u_humid_sim` (optional)

This block is used especially for **Demo 2**, where Simulink is intended to directly drive the actuators while the ESP32 acts only as the physical interface.

In Phase 2, the structure and signals are documented, although the final integration of the PID controller will be carried out in Phase 3.

---

### 3.4. Alarms_Logic

**Main function:**  
Detect abnormal conditions in the Mini-HVAC system.

The monitored conditions include:

- `T_sim` much higher than `T_set`  
- Divergence between simulated and real values  
- `RH_sim` outside the safe range  
- Excessive actuator usage

The main output is:

- `alarm_flag` (boolean)

The first version implements fixed thresholds; later versions will integrate a more advanced detection system.

---

### 3.5. ESP32_Interface

(**Phase 2 completed**)  
This module implements binary communication between Simulink and the ESP32 microcontroller.  

It uses:

- **`PcToEsp` packet (24 bytes)** to send data to hardware:
  - `T_sim`, `RH_sim`, `P_sim`, `u_fan_sim`, `u_heater_sim`, `alarm_flag`

- **`EspToPc` packet (32 bytes)** to receive data from hardware:
  - `T_real`, `RH_real`, `u_fan_real`, `u_heater_real`, `T_set`, `RH_set`, `mode_auto_manual`, `hvac_enable`

This module is critical because it enables tests that are:

- Fully simulated
- Performed with real sensors/actuators (HIL)
- Hybrid interactions (Simulink controls, ESP32 physically actuates)

---

## 4. Global Function of the Mini-HVAC System

The Mini-HVAC system makes it possible to study two major experimental scenarios:

### **Demo 1 — Local Control on the ESP32 (MANUAL mode)**

- The ESP32 reads real sensors.
- It generates its own setpoints (`T_set`, `RH_set`) through potentiometers/buttons.
- It controls physical actuators:
  - DC fan (PWM)
  - Heater LED
  - Alarm buzzer
- Simulink:
  - Simulates the MiniHVAC plant
  - Visualizes signals
  - Evaluates simulation ↔ hardware differences

### **Demo 2 — Control in Simulink (AUTO mode)**

- Simulink generates control signals (`u_fan_sim`, `u_heater_sim`)
- The ESP32 physically executes those signals
- The ESP32 returns real states to Simulink
- A hybrid simulation ↔ hardware control loop is completed

### **Case Study: Dry Winter**

- Low ambient temperature  
- Low humidity (< 30%)  
- Light heating + fan control are required  
- Ideal scenario for testing:
  - Setpoint tracking  
  - AUTO/MANUAL strategies  
  - Alarm management  

---

## 5. Conclusion of the Global System Overview

The Mini-HVAC model meets the requirements of **Phase 2** of the project:

- **Modular design:**  
  Each subsystem operates independently and has well-defined responsibilities.

- **Module-level testing:**  
  Dynamics, control logic, serial communication, and alarms have been verified before global integration.

- **Clear technical documentation:**  
  This global overview describes how the subsystems relate to each other and to the ESP32, laying the groundwork for the later phases of the project.

The system is ready for:

- Integration with FreeRTOS on the ESP32  
- Implementation of real PID controllers  
- Execution of real-time physical-simulation tests  
