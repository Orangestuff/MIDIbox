# ESP32-S3 Wireless MIDI Pedal

A fully programmable, battery-powered MIDI Foot Controller based on the ESP32-S3. This device supports **Bluetooth LE MIDI** and **USB MIDI** simultaneously, features a responsive **Web Interface** for configuration, and includes advanced **Power Management** for long battery life.

## 🚀 Key Features

### Connectivity

* **Dual MIDI Interface:** Works over **Bluetooth LE (BLE)** and **USB** simultaneously.
* **Wireless Configuration:** Hosts a WiFi Access Point (`MidiBox_Config`) for on-the-fly editing via any smartphone or laptop.
* **Unique Identity Generation:** Hold **Switch 5 + Switch 8** on boot to generate a new BLE MAC address (useful for resolving pairing conflicts).

### Control & Logic

* **4 Programmable Banks:** Each bank stores unique settings for all 8 switches and the expression pedal.
* **Advanced Switch Actions:**
* **Triggers:** Short Press, Long Press, and Release actions.
* **Message Types:** Note On/Off, CC, PC, Bank Up/Down, Direct Bank Select.
* **Logic Master:** Switches can trigger "None" (no MIDI) while still controlling other switches via groups.
* **Toggle Mode:** Latching behavior for CC or Note messages.


* **Grouping System:**
* **Exclusive Groups (Ex):** Pressing a switch turns OFF other switches in the same group (e.g., for channel switching).
* **Master/Slave Groups (Ld):** A "Master" switch can turn ON/OFF a defined set of "Slave" switches automatically.



### Expression Pedal

* **Per-Bank Configuration:** Different CC mappings and curves for every bank.
* **Smart Calibration:** "Set to Current" buttons in the Web UI for instant Min/Max calibration.
* **Response Curves:** Linear, Logarithmic (Fast Start), and Exponential (Swell).
* **Jitter Suppression:** Oversampling, hysteresis, and smoothing filters for stable output.

### Power & Presets

* **Deep Sleep:** Automatic low-power mode after inactivity.
* **Instant Wake:** Wakes up immediately upon pressing any footswitch.
* **Configurable:** Enable/Disable and set Timeout (1-120 mins) via Web UI.


* **Preset Manager:** Save and Load up to **5 Full Device Snapshots** (Setlists) to internal flash memory.
* **Battery Monitor:** Real-time voltage reading and low-battery LED warning.

---

## 🛠️ Hardware Bill of Materials (BOM)

| Component | Quantity | Description |
| --- | --- | --- |
| **Microcontroller** | 1 | ESP32-S3 DevKit (or similar S3 board) |
| **PSU/Battery Charger** | 1 | TP4056 Charger Module (or similar) |
| **Footswitches** | 8 | Momentary SPST Footswitches |
| **Power Switch** | 1 | Any SPST toggle switch |
| **LEDs** | 8 | WS2812B (Neopixel) individual LEDs or strip segments |
| **Expression Jack** | 1 | Servo extension wire |
| **Battery** | 1 | 3.7V 18650 Battery |
| **Resistors** | 3 | 2x100kΩ (For Battery Voltage Divider) 1x10kΩ (For Expression Pedal Pulldown) |
| **Diodes** | 8 | 1N4148 (For Switch Matrix - Optional but recommended) |
| **Enclosure** | 1 | 3D Printed Case (Files included in `/stl` folder) |
| **Threaded Inserts** | 6 | M3x4x5 Threaded Inserts (For pedalCaseV2.STL) |

## 🛠 Wiring & Pinout

The code is configured for an **ESP32-S3** using a 2x4 Switch Matrix.

| Component | ESP32-S3 Pin | Notes |
| --- | --- | --- |
| **Switch Row 1** | GPIO 12 | Matrix Output |
| **Switch Row 2** | GPIO 13 | Matrix Output |
| **Switch Col 1** | GPIO 4 | Matrix Input (Pull-up) |
| **Switch Col 2** | GPIO 5 | Matrix Input (Pull-up) |
| **Switch Col 3** | GPIO 6 | Matrix Input (Pull-up) |
| **Switch Col 4** | GPIO 8 | Matrix Input (Pull-up) |
| **LED Strip** | GPIO 48 | WS2812B / NeoPixel (9 LEDs) |
| **Battery Sense** | GPIO 7 | Voltage Divider (ADC1 Ch 6) |
| **Expression** | GPIO 2 | TRS Tip (ADC1 Ch 2) |

> **LED Wiring Note:** The LEDs require a "Snake" wiring pattern for the default mapping:
> `Sw1 -> Sw2 -> Sw3 -> Sw4 -> Sw8 -> Sw7 -> Sw6 -> Sw5`

**Note:** The Expression Pedal input expects a voltage divider setup (3.3V -> Pot -> GND, Wiper to GPIO 2).

## ⚡ Optional Hardware Mod: Offline Charging

By default, the ESP32 draws power and boots up whenever a USB cable is connected, even if the intention is only to charge the battery.

To enable **Offline Charging** (charging the battery via USB while the main unit remains powered down), you can remove the VBUS blocking diode (typically labeled **D1** or similar on the PCB).

* **Action:** Desolder and remove the Schottky diode on the 5V rail.
* **Result:** The USB connection will power the battery management circuit exclusively. The ESP32 will not boot until the main power switch is toggled ON.

## 🖨️ 3D Printed Case

This repository includes STL files for a custom enclosure designed to fit the electronics and switches perfectly.

* **pedalCaseV2.STL:** Holds all electronics.
* **baseplateV2.STL:** mount with M3 screws and threaded inserts.
* 
---

## 💾 Installation

1. **Environment:** Requires **ESP-IDF v5.x** (Tested with v5.5.1).
2. **Dependencies:**
* `tinyusb` (Component via IDF Component Manager)
* `driver/rtc_io` (Standard in IDF)


3. **Configuration:**
* Enable **TinyUSB** in `menuconfig`.
* Set partition table to accommodate NVS data (factory/app partitions).


4. **Flash:**
```bash
idf.py build flash monitor

```



---

## 📖 User Manual

### 1. Basic Operation

* **Power On:** The LEDs will flash the current Bank Color.
* **Change Banks:** Use configured "Bank Up/Down" switches or bind banks directly.
* *Bank 1: Red | Bank 2: Green | Bank 3: Blue | Bank 4: Purple*


* **Battery Status:** LED 1 indicates level:
* 🟢 > 3.9V (Good)
* 🟡 > 3.6V (Okay)
* 🔴 < 3.6V (Low)



### 2. Configuration Mode (WiFi)

1. If no known WiFi is found, the pedal broadcasts an Access Point:
* **SSID:** `MidiBox_Config`
* **Pass:** `12345678`


2. Connect your phone/laptop to this network.
3. Open a browser to `http://192.168.4.1`.
4. **Important:** Changes made in the UI are live in RAM. Click **"Save All Configuration"** to persist them to flash.

### 3. Preset Manager

Located in the Web UI:

* **Save:** Enter a name in the text box and click **SAVE**. This takes a snapshot of the *current* state (Bank settings, Expressions, etc.) and stores it in a slot.
* **Load:** Select a preset from the dropdown and click **LOAD**. The pedal immediately reconfigures itself.
* *Note: The browser remembers your last used preset ID for convenience.*

### 4. Deep Sleep

* The device sleeps after the configured idle time (Default: 5 mins).
* **To Wake:** simply step on **ANY** switch. The pedal wakes instantly and flashes the LEDs to confirm readiness.
* **Optimization:** The startup sequence prioritizes LED feedback before initializing WiFi/USB, making the wake-up feel instantaneous.

---

## 🧩 Advanced Logic Guide

The Web UI allows complex interactions between switches using **Masks**:

* **Exclusive Mask (Ex / 🛡️):**
* Assign switches to a "Group Number" (1-8).
* If Switch A and Switch B share Group 1, pressing A will automatically turn B **OFF**.
* *Usage: Guitar Amp Channel switching (Clean vs Distortion).*


* **Lead/Master Mask (Ld / ⚡):**
* If Switch A is the "Master" of Group 1, and Switch B "Includes" (🔗) Group 1...
* Pressing A will force B to turn **ON**.
* Releasing A (or turning it off) will force B to turn **OFF**.
* *Usage: A "Solo" switch that turns on Delay + Boost + Reverb simultaneously.*



---

## 📂 Project Structure

* `main.c` - Core logic, BLE stack, USB stack, GPIO matrix scanning, Sleep logic.
* `index.h` - HTML/CSS/JS for the Web Interface (gzipped string or raw string).
* `CMakeLists.txt` - Build configuration.

---

## ⚠️ Troubleshooting

* **Pedal won't wake up:** Ensure battery is charged (>3.0V).
* **Cannot find Bluetooth:** Hold **Switch 5 + 8** while powering on to generate a new MAC address. The LEDs will flash purple.
* **Expression Pedal Jitter:** Increase `EXP_HYSTERESIS` in `main.c` or use the Web UI to re-calibrate Min/Max values.
* **"Save Error" in Web UI:** Ensure you are not spamming the save button; writing to NVS takes ~200-500ms.
