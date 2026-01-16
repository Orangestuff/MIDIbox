# ESP32-S3 Wireless MIDI Foot Controller

A fully customizable, battery-powered MIDI foot controller built on the ESP32-S3. It features 8 footswitches, an expression pedal input, RGB LED status indicators, and a modern Web Interface for configuration—no recompiling required to change banks or messages!

## 🌟 Features

* **Dual Connectivity:** Works simultaneously as a **Bluetooth LE MIDI** device and a **USB MIDI** device.
* **Web Configuration Portal:** Host-based configuration page (access via browser) to map switches, calibrate the expression pedal, and manage WiFi settings.
* **4 Programmable Banks:** Cycle through 4 distinct banks, each with its own color coding.
* **Smart LED Feedback:**
* **Per-Switch LEDs:** Show bank color when active/toggled.
* **Battery Monitor:** Onboard LED changes color (Green/Orange/Red) based on voltage.
* **Visual Feedback:** Flashes white on press, blue/white on mode changes.


* **Advanced Switch Modes:**
* **Momentary & Toggle:** Configurable per switch.
* **Exclusive Groups:** "Radio button" style logic (only one switch in a group can be active at a time).
* **Bank Cycling:** Assign dedicated switches to cycle banks forward or reverse.


* **Expression Pedal:** Integrated smoothing and hysteresis filtering with easy min/max calibration via the Web UI.
* **Deep Sleep Ready:** Optimized for battery operation.

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

## 🔌 Wiring
The project uses a **2x4 Switch Matrix** to save pins.

**Pin Definitions (Default in `main.c`):**

* **Matrix Rows:** GPIO 12, 13
* **Matrix Columns:** GPIO 4, 5, 6, 8
* **LED Data:** GPIO 48
* **Battery Sense (ADC):** GPIO 7 (via Voltage Divider)
* **Expression Pedal (ADC):** GPIO 3

> **Note:** The LEDs require a "Snake" wiring pattern for the code's default mapping:
> `Sw1 -> Sw2 -> Sw3 -> Sw4 -> Sw8 -> Sw7 -> Sw6 -> Sw5`

## ⚡ Optional Hardware Mod: Offline Charging

By default, the ESP32 draws power and boots up whenever a USB cable is connected, even if the intention is only to charge the battery.

To enable **Offline Charging** (charging the battery via USB while the main unit remains powered down), you can remove the VBUS blocking diode (typically labeled **D1** or similar on the PCB).

* **Action:** Desolder and remove the Schottky diode on the 5V rail.
* **Result:** The USB connection will power the battery management circuit exclusively. The ESP32 will not boot until the main power switch is toggled ON.

## 🖨️ 3D Printed Case

This repository includes STL files for a custom enclosure designed to fit the electronics and switches perfectly.

* **pedalCaseV2.STL:** Holds all electronics.
* **baseplateV2.STL:** mount with M3 screws and heated inserts.

## 🚀 Installation & Setup

This project is built using the **ESP-IDF** framework.

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



## ⚙️ Configuration (Web UI)

You do not need to edit code to change MIDI messages!

1. **Enter Config Mode:**
* If no WiFi is configured, the pedal creates an Access Point named **`MidiPedal_Config`**.
* Connect to this network with password "12345678".


2. **Open the Interface:**
* Navigate to `http://192.168.4.1` in your web browser.


3. **Customize:**
* **Switches:** Set Type (PC, CC, Note), Channel, and Value.
* **Modes:** Enable Toggle, Momentary, or assign Exclusive Groups.
* **Pedal:** Calibrate Heel/Toe positions visually.
* **WiFi:** Connect to your home router to access config without disconnecting your internet.



## 🎮 Usage

* **Wireless Toggle:** Hold **Switch 5 + Switch 8** for 2 seconds to toggle between BLE MIDI (Blue Flash) and WiFi Config Mode (White Flash).
* **Bank Cycle:** Assign "Bank Cycle Fwd" or "Rev" to any switch in the Web UI to navigate banks.
* **Battery Check:** The onboard LED (Index 0) indicates health:
* 🟢 **Green:** > 3.9V
* 🟠 **Orange:** > 3.6V
* 🔴 **Red:** < 3.6V (Charge Soon)



## 📄 License

This project is open source. Feel free to modify and build your own!

---
