Here is the updated `README.md`. I have revised the **Features** and **Configuration** sections to highlight the sophisticated "Scene" logic, Master/Slave architecture, and Global Bank interaction we just implemented.

I also added a new section called **"Advanced Grouping Logic"** because your grouping system is now powerful enough to warrant its own explanation!

---

# ESP32-S3 High-Performance Wireless MIDI Foot Controller

A professional-grade, fully customizable MIDI foot controller built on the ESP32-S3. Designed for low latency and high flexibility, it features 8 footswitches, an expression pedal input, global scene management, and a modern Web Interface for on-the-fly configuration—no recompiling required!

## 🌟 Key Features

### 🚀 **Performance Core**

* **1000Hz Polling Rate:** Optimized "Fast Loop" architecture ensures ~1ms latency for note triggers.
* **Dual-Core Processing:** MIDI tasks are pinned to specific cores to prevent watchdog crashes and ensure stability.
* **Smart LED Driver:** Custom RMT driver logic eliminates "Rainbow/Disco" glitches during rapid bank switching.
* **Deep Sleep Ready:** Optimized for battery operation with voltage monitoring.

### 🎛️ **Connectivity & Control**

* **Dual Mode:** Works simultaneously as a **Bluetooth LE MIDI** device and a **USB MIDI** device.
* **4 Programmable Banks:** Banks function as a continuous controller (32 virtual switches), allowing interactions across banks.
* **Web Configuration Portal:** Host-based configuration page (access via browser) to map switches, calibrate the expression pedal, and manage WiFi settings.

### 🧠 **Advanced Grouping Engine**

* **Exclusive Groups (XOR):** "Radio button" logic. Pressing one switch turns off others in the same group (e.g., Clean vs. Distortion).
* **Inclusive Groups (Scenes):** "Link" logic. Pressing one switch activates multiple others simultaneously (e.g., Solo Boost + Delay + Reverb).
* **Multi-Group Assignment:** A single switch can belong to multiple Exclusive and Inclusive groups simultaneously.
* **Master/Slave Logic:** Define "Leader" switches that trigger scenes, while "Slave" switches can still be toggled individually without affecting the rest of the group.
* **Global Bank Interaction:** Group logic works across all 4 banks. A switch in Bank 1 can kill a switch in Bank 2.

### 🎨 **Visual Feedback**

* **Per-Switch LEDs:** Show bank color when active/toggled.
* **Battery Monitor:** Onboard LED changes color (Green/Orange/Red) based on voltage.
* **Mode Indicators:** Visual confirmation for mode changes and connection status.

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

## 🔌 Wiring & Pinout

The project uses a **2x4 Switch Matrix** to save pins.

* **Matrix Rows:** GPIO 12, 13
* **Matrix Columns:** GPIO 4, 5, 6, 8
* **LED Data:** GPIO 48 (WS2812B Data In)
* **Battery Sense:** GPIO 7 (via 100k/100k Voltage Divider)
* **Expression Pedal:** GPIO 3 (via 10k Pull-down resistor on Ring/Wiper)

> **LED Wiring Note:** The LEDs require a "Snake" wiring pattern for the default mapping:
> `Sw1 -> Sw2 -> Sw3 -> Sw4 -> Sw8 -> Sw7 -> Sw6 -> Sw5`

---

## ⚡ Optional Hardware Mod: Offline Charging

By default, the ESP32 draws power and boots up whenever a USB cable is connected, even if the intention is only to charge the battery.

To enable **Offline Charging** (charging the battery via USB while the main unit remains powered down), you can remove the VBUS blocking diode (typically labeled **D1** or similar on the PCB).

* **Action:** Desolder and remove the Schottky diode on the 5V rail.
* **Result:** The USB connection will power the battery management circuit exclusively. The ESP32 will not boot until the main power switch is toggled ON.

## 🖨️ 3D Printed Case

This repository includes STL files for a custom enclosure designed to fit the electronics and switches perfectly.

* **pedalCaseV2.STL:** Holds all electronics.
* **baseplateV2.STL:** mount with M3 screws and threaded inserts.

---

## ⚙️ Web Configuration Guide

You do not need to edit code to change MIDI messages! Connect to the pedal's WiFi (`MidiPedal_Config` / `12345678`) and navigate to `http://192.168.4.1`.

### Switch Settings

* **Function:** Select Note, CC, PC, or Bank Cycle.
* **Toggle:** Check for Latching behavior (Click ON / Click OFF). Uncheck for Momentary.
* **EXCL (Exclusive):** Enter group IDs (e.g., `1` or `1, 2`). Switches sharing an ID will mutually exclude each other.
* **INCL (Inclusive):** Enter group IDs (e.g., `3`). Switches sharing an ID will turn ON together.
* **LEAD (Master):** Enter group IDs (e.g., `3`). This switch will ACTIVATE the group. Leave blank to make this switch a "Slave" (passive member).

### Expression Pedal

* **Live Calibration:** View raw ADC values in real-time.
* **Set Min/Max:** Click buttons to instantly set Heel and Toe positions.

---

## 🧠 Understanding the Grouping Logic

This controller uses a **Bitmask System**, allowing complex overlapping relationships.

**1. The "Scene" Setup (Inclusive)**

* **Scenario:** You want Switch 1 to turn on Distortion (Sw 2) and Delay (Sw 3).
* **Setup:**
* **Switch 1:** INCL: `1`, LEAD: `1` (Master)
* **Switch 2:** INCL: `1` (Slave)
* **Switch 3:** INCL: `1` (Slave)


* **Result:** Pressing Sw 1 turns on 1, 2, and 3. Pressing Sw 2 (Slave) only toggles Sw 2.

**2. The "Channel Select" Setup (Exclusive)**

* **Scenario:** You want Clean, Rhythm, and Lead channels. Only one can be active.
* **Setup:**
* **Switch 1 (Clean):** EXCL: `1`
* **Switch 2 (Rhythm):** EXCL: `1`
* **Switch 3 (Lead):** EXCL: `1`


* **Result:** Pressing any switch instantly turns the others OFF. This works even if the switches are on different Banks!

---

## 🚀 Installation & Build

This project is built using the **ESP-IDF** framework (v5.x).

1. **Clone the Repository:**
```bash
git clone https://github.com/yourusername/esp32-midi-pedal.git

```


2. **Build and Flash:**
```bash
idf.py set-target esp32s3
idf.py build
idf.py flash monitor

```



## 🎮 Usage Shortcuts

* **Wireless Toggle:** Hold **Switch 5 + Switch 8** for 2 seconds to toggle between BLE MIDI (Blue Flash) and WiFi Config Mode (White Flash).
* **Factory Reset:** Hold **Switch 5 + Switch 8** while powering on the device to wipe saved settings (Use this after firmware updates if memory structures change).
* **Bank Cycle:** Assign "Bank Cycle Fwd" or "Rev" to any switch via the Web UI.

## 📄 License

This project is open source. Feel free to modify and build your own!
