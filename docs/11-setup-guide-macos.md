# Technical log — ESP32 WROVER with **MATLAB R2025b + Simulink Support Package for Arduino Hardware**
> *readthedocs*-style document used to record the **installation**, **issues**, **diagnosis**, and **solutions** required to successfully compile and run **Deploy & Start** for a Simulink model on an **ESP32‑WROVER**.

---
## 1) Context and objective

- **Objective:** use Simulink to generate and deploy standalone code on an **ESP32 WROVER** (test: LED on GPIO 2) and create a **reproducible manual**.
- **Environment:**
  - macOS (MacBook).
  - **MATLAB/Simulink R2025b**.
  - Add-on: **Simulink Support Package for Arduino Hardware**.
  - Board: **ESP32‑WROVER Module** (ESP32‑D0WD‑V3 chip).
  - Typical serial port: `/dev/cu.usbserial-210`.

---
## 2) Initial add-on installation and “Hardware Setup”

1. Install the **Simulink Support Package for Arduino Hardware** add-on.
2. Open **Add‑On Manager → Setup** and follow the wizard:
   - The wizard installs third-party dependencies (ESP32 Core 2.0.11) **inside MATLAB’s internal arduino‑cli**, at the following path:

```
~/Documents/MATLAB/SupportPackages/R2025b/3P.instrset/arduinoide.instrset/aCLI
```

3. Select the port: `/dev/cu.usbserial-210`.

> **Key note:** MATLAB uses **its own** `arduino-cli` (aCLI) and its **own** “ESP32 core” at the path above. It **does not** depend on the external Arduino IDE.

---
## 3) Observed issues

### 3.1 FQBN error / unrecognized option
**Symptom:**
```
Error during Upload: Unknown FQBN: getting build properties for board esp32:esp32:esp32wrover: invalid option 'FlashSize'
```
**Cause:** the Simulink model generates a command with:
```
--board-options "FlashSize=4M,FlashMode=dio,FlashFreq=80,UploadSpeed=115200"
```
If the **core** being used by the aCLI **does not define** the `FlashSize` menu, the CLI fails while resolving the FQBN.

### 3.2 Connection/baud-rate issues (serial noise)
**Typical symptom from Setup or the uploader:**
```
Changing baud rate to 921600
A fatal error occurred: Unable to verify flash chip connection (Invalid head of packet ...)
```
**Cause:** **Setup** attempts to validate at **921600 baud** by default, which is often unstable on macOS (cables/hubs).

---
## 4) Verify and isolate MATLAB’s internal `arduino-cli`

In **MATLAB Command Window**:
```matlab
cliPath = codertarget.arduinobase.internal.getArduinoIDERoot
system([fullfile(cliPath,'arduino-cli') ' core list'])
```

In **Terminal** (aCLI folder):
```bash
cd ~/Documents/MATLAB/SupportPackages/R2025b/3P.instrset/arduinoide.instrset/aCLI
./arduino-cli core list
./arduino-cli core uninstall esp32:esp32 || true
rm -rf ./data/packages/esp32
./arduino-cli core update-index
./arduino-cli core install esp32:esp32@2.0.11
./arduino-cli board details -b esp32:esp32:esp32wrover | egrep -i "Board|Options|Flash|Upload"
```

---
## 5) Applied solutions

### 5.1 Keep **ESP32 core 2.0.11** in MATLAB’s aCLI
- This version is the one expected by the R2025b **Setup** and aligns the build/upload recipes.

### 5.2 Force a stable upload speed (115200) for **Setup**
Setup ignores the model’s `--board-options` and uses the core’s **default** values. To avoid 921600, create **local overrides** in the aCLI core:

Core path:
```
~/Documents/MATLAB/SupportPackages/R2025b/3P.instrset/arduinoide.instrset/aCLI/data/packages/esp32/hardware/esp32/2.0.11/
```

#### (A) `platform.local.txt` (override for esptool patterns)
Create the **`platform.local.txt`** file with:
```
# Fuerza 115200 en todas las rutas de subida/erase/program de esptool.py
tools.esptool_py.upload.pattern_args=--chip {build.mcu} --port "{serial.port}" --baud 115200 {upload.flags} --before default_reset --after hard_reset write_flash {upload.erase_cmd} -z --flash_mode {build.flash_mode} --flash_freq {build.flash_freq} --flash_size {build.flash_size} {build.bootloader_addr} "{build.path}/{build.project_name}.bootloader.bin" 0x8000 "{build.path}/{build.project_name}.partitions.bin" 0xe000 "{runtime.platform.path}/tools/partitions/boot_app0.bin" 0x10000 "{build.path}/{build.project_name}.bin" {upload.extra_flags}

tools.esptool_py.erase.pattern_args=--chip {build.mcu} --port "{serial.port}" --baud 115200 {upload.flags} --before default_reset --after hard_reset erase_flash

tools.esptool_py.program.pattern_args=--chip {build.mcu} --port "{serial.port}" --baud 115200 {upload.flags} --before default_reset --after hard_reset write_flash -z --flash_mode {build.flash_mode} --flash_freq {build.flash_freq} --flash_size {build.flash_size} 0x10000 "{build.path}/{build.project_name}.bin"

# Red de seguridad
upload.speed=115200
esptool_py.speed=115200
```

> With this, Setup stops trying 921600 and validates at 115200.

#### (B) `boards.local.txt` (add the `FlashSize` menu if the model requires it)
If the model continues to pass `FlashSize=4M`, and your 2.0.11 core does not define that menu, create **`boards.local.txt`** with:
```
# Añade menú FlashSize (4MB) para WROVER y Dev Module
esp32wrover.menu.FlashSize.4M=4MB
esp32wrover.menu.FlashSize.4M.build.flash_size=4MB
esp32wrover.menu.FlashSize=4M

esp32.menu.FlashSize.4M=4MB
esp32.menu.FlashSize.4M.build.flash_size=4MB
esp32.menu.FlashSize=4M
```

> Alternative: in the model, use **Hardware board = ESP32 Dev Module (Arduino Compatible)** (it typically does not inject `FlashSize`).

### 5.3 Final settings in the **Simulink model**

- **Hardware → Hardware Settings…**
  - **Hardware board:** “ESP32 Dev Module (Arduino Compatible)” *or* “ESP32‑WROVER …” (if `FlashSize` already exists).
  - **Target hardware resources → Host-board connection**
    - **Host COM port:** `/dev/cu.usbserial-210`
    - **Baud rate:** **115200**
    - **Boot mode:** *Automatic reset via DTR/RTS*
  - **Connected I/O / External Mode**
    - **Hardware serial port:** **Serial 0**
    - (Disable during *Deploy* so the port is not opened in parallel.)
  - **Build options:** enable **Verbose output** to inspect the command if it fails.

### 5.4 Physical connection best practices

- Use a **short data cable** connected directly to the Mac (no hub).
- Close all **serial monitors** (Arduino IDE, `screen`, etc.).
- If it gets stuck at “Connecting…”, **hold BOOT** and release it when “Writing at 0x…” starts.

---
## 6) Final result (successful log)

Example of a successful upload (excerpt):
```
esptool.py v4.5.1
Serial port /dev/cu.usbserial-210
Connecting.......
Chip is ESP32-D0WD-V3 (revision v3.1)
...
Wrote 253536 bytes (137601 compressed) at 0x00010000 in 13.5 seconds
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
New upload port: /dev/cu.usbserial-210 (serial)
```

In MATLAB:
```
### Deployed code to target successfully
### Successful completion of build procedure for: arduino_gettingstarted
```

---
## 7) Lessons learned (the “why”)

1. MATLAB uses its **internal arduino‑cli**: the core must be installed and configured **inside** `.../aCLI`.
2. **Menus/FQBN**: if Simulink passes `--board-options`, the core must include those menus (`FlashSize`). Otherwise, the CLI fails.
3. **Upload speed**: 921600 often fails on macOS. Forcing 115200 with `platform.local.txt` stabilizes the overall process (including Setup).
4. **Separate the issues**: 
   - FQBN/menus → core/board/board‑options.
   - Serial connection → port, baud rate, cable/boot.

---
## 8) Quick-reference checklists

### 8.1 Environment installation/validation
- [ ] `cliPath` points to `.../aCLI`.
- [ ] `arduino-cli core list` shows **esp32:esp32 2.0.11**.
- [ ] `platform.local.txt` includes `--baud 115200` in `tools.esptool_py.*.pattern_args`.
- [ ] `boards.local.txt` adds `FlashSize=4M` (if the model uses it).
- [ ] Setup validates without “Changing baud rate to 921600”.

### 8.2 Before Deploy
- [ ] `ls /dev/cu.usb*` confirms the current port.
- [ ] **Host COM port** = `/dev/cu.usbserial-210` (or the appropriate one).
- [ ] **Upload speed = 115200**, **Boot: DTR/RTS**, **Serial 0**.
- [ ] No **External Mode/Connected I/O** during deploy.
- [ ] No serial monitors are open.

---
## 9) Useful commands (Terminal)

```
# Ruta del aCLI interno de MATLAB
cd ~/Documents/MATLAB/SupportPackages/R2025b/3P.instrset/arduinoide.instrset/aCLI

# Gestionar cores en el aCLI
./arduino-cli core list
./arduino-cli core uninstall esp32:esp32 || true
rm -rf ./data/packages/esp32
./arduino-cli core update-index
./arduino-cli core install esp32:esp32@2.0.11

# Detalles de placa (ver opciones disponibles)
./arduino-cli board details -b esp32:esp32:esp32wrover | egrep -i "Board|Options|Flash|Upload"

# Borrar flash (si quedó en estado extraño)
./data/packages/esp32/tools/esptool_py/4.5.1/esptool.py \
  --chip esp32 --port /dev/cu.usbserial-210 --baud 115200 erase_flash
```

---
## 10) Final configuration (summary)

- **aCLI core (MATLAB):** `esp32:esp32@2.0.11`
- **Local overrides:**  
  - `platform.local.txt` → `--baud 115200` in `tools.esptool_py.*.pattern_args` + base variables.  
  - `boards.local.txt` → `FlashSize=4M` menu (if the model requires it).
- **Simulink model:**  
  - Board: **ESP32 Dev Module (Arduino Compatible)** (or WROVER if `FlashSize` already exists).  
  - Port: `/dev/cu.usbserial-210`.  
  - **Upload speed:** 115200.  
  - **Hardware serial port:** Serial 0.  
  - **Boot mode:** DTR/RTS.
