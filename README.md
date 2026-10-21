# Project Neal Defence Wing: Tactical Heartbeat Sensor MK-IV

[![Author](https://img.shields.io/badge/Author-Lian%20Mollick%20(@mevrickneal)-red?style=for-the-badge&logo=github)](https://github.com/mevrickneal)
[![Platform](https://img.shields.io/badge/Platform-Arduino%20Uno%20R3-00979D?style=for-the-badge&logo=arduino)](https://www.arduino.cc/)
[![Radar](https://img.shields.io/badge/Radar-HLK--LD2450%2024GHz%20FMCW-orange?style=for-the-badge)](https://www.hlktech.net/)
[![Display](https://img.shields.io/badge/Display-2.4%22%20/%202.8%22%20TFT%20LCD-green?style=for-the-badge)](https://github.com/prenticedavid/MCUFRIEND_kbv)

A fully functional, real-world recreation of the iconic **Call of Duty: Modern Warfare 2 Heartbeat Sensor**, mounted on a firearm tactical rail. Engineered by **Lian Mollick** for **Project Neal Defence Wing - TVC & Guidance**.

---

## 🎯 Display Screen Sample

The tactical HUD replicates the authentic 120-degree forward sector arc seen in military avionics and *Modern Warfare*:

![Project Neal Heartbeat Sensor 120° Tactical Display](assets/radar_screen_sample.jpg)

### HUD Display Breakdown:
- **120° Forward Sector Arc:** Extends upwards from the bottom origin across a $\pm 55^\circ$ field of view.
- **Range Rings:** Heavy 2-pixel concentric arcs at **2 meters**, **4 meters**, and **6 meters**.
- **Azimuth Spokes:** Polar sector markings at `-55°`, `-30°`, `000°` (centerline), `+30°`, and `+55°`.
- **Phosphor CRT Sweep Beam:** Smooth ~45 FPS animated sweep line leaving a fading phosphor persistence trail.
- **Hostile Target Acquisition:** Dual-layer cyan-blue blips with white glowing cores, target lock brackets `[ ● ]`, and live distance tags (e.g. `1.8m`). Hostiles within 2.5m trigger bright red hazard brackets.
- **Bottom-Left Telemetry Station:**
  - `TGT: [0-3] LOCK [HOSTILE]`
  - `RX: <count>` (Real-time serial byte counter)
  - `H:0xXX PKT: <count>` (Live packet decoding diagnostics)
  - Dynamic flashing warning badge: `! HOSTILE NEAR !`
- **Bottom-Right ECG Pulse Line:** Real-time electrocardiogram waveform that dynamically spikes as targets draw closer.
- **Branding Header:** Tactical red badge displaying `NEAL DEFENCE WING` and `FREQ: 24G`.

---

## 🔊 Authentic Geiger-Müller Acoustic Engine

Unlike standard toy buzzers that emit continuous loud screeching tones, this firmware features a **custom mechanical impulse synthesizer**:
- **Pure Mechanical Clicks:** Uses ultra-short $500\mu\text{s}$ electrical pulses (`geigerClick()`) to snap the piezo diaphragm once per discharge. This produces an authentic, sharp Geiger-Müller tube discharge click without any frequency resonance.
- **Proximity-Scaled Click Rate:**
  - **Hazard Proximity ($< 0.4\text{m}$):** Intense rapid Geiger chatter (~50 ms – 70 ms between clicks).
  - **Close Range ($1.0\text{m}$):** Rapid tactical alert (~180 ms).
  - **Combat Range ($2.5\text{m}$):** Moderate pacing (~430 ms).
  - **Distant Range ($5.0\text{m}$):** Calm, periodic ticking (~950 ms).
- **Stochastic Poisson Jitter ($\pm 15\%$):** Replicates natural radiation particle arrival randomness rather than a synthetic metronome.
- **Silence on Clear:** When no targets are in range, the buzzer is **completely silent**.

---

## 🔫 Tactical Rifle Mounting & 90° Flip-to-Side (FTS) Mechanism

Mounted on the rifle's left-side (9 o'clock) Picatinny or M-LOK rail, the sensor incorporates a spring-loaded **90-degree flip-to-side (FTS) locking detent hinge** so it never obstructs primary optics or peripheral vision in dynamic combat environments:

![Project Neal Heartbeat Sensor Tactical Rifle Foldable Mount](assets/rifle_folding_mount.jpg)

### Mechanical Design & Dual Operating Modes:
1. **DEPLOYED (Recon Mode - 90° Perpendicular):**
   - The display flips outward $90^\circ$ to sit directly in the operator's left-eye peripheral field of view.
   - Positioned low and offset to maintain an unobstructed primary sight picture through the red dot / holographic optic.
   - The HLK-LD2450 24GHz radar antenna points forward along the weapon's bore axis, scanning a $120^\circ$ forward cone up to 6 meters.
2. **STOWED (Combat / Sprint Mode - 0° Flush):**
   - A quick push on the locking detent folds the unit flat against the left side of the rifle's handguard.
   - Hugs the receiver tightly with zero snag points for sling transition, door breaches, and tactical sprinting.
   - Protects the LCD screen face against the rifle chassis when traversing rough terrain.

---

## ⚡ Hardware Components & Pinout

| Component | Model / Spec | Connection to Arduino Uno |
| :--- | :--- | :--- |
| **MCU** | Arduino Uno R3 (ATmega328P @ 16 MHz) | Master Controller |
| **Radar Module** | Hi-Link HLK-LD2450 24GHz FMCW mmWave | TX $\rightarrow$ **Pin 0 (RX)**<br>RX $\rightarrow$ **Pin 1 (TX)**<br>VCC $\rightarrow$ **5V**<br>GND $\rightarrow$ **GND** |
| **TFT Display** | 2.4" / 2.8" TFT LCD Shield (320x240 Landscape) | Plugs directly into standard Uno headers |
| **Acoustic Buzzer** | 5V Piezo Buzzer | Positive $\rightarrow$ **Pin 10**<br>Negative $\rightarrow$ **GND** |

---

## 🛠️ Zero-Desoldering Upload Workflow

On an Arduino Uno, Digital Pin 0 is shared between the USB communication chip and the radar sensor's TX wire. Because the radar streams packets at 256,000 baud, uploading firmware can collide with incoming sensor data.

### Automated Upload Tool:
To flash updates **without desoldering or disconnecting any wires**:
1. Run the automated flash retry loop:
   ```powershell
   .\auto_flash_loop.ps1
   ```
2. When the script prompts `Listening for Reset...`, give the red **Reset button** on the TFT shield a single steady tap (press and release).
3. The script will automatically catch the Optiboot bootloader sync window and flash the firmware in ~5 seconds!

---

## 📦 Software Architecture

- **State Machine Parser (`processRadarSerial`):**
  Strict 7-stage state machine (`SYNC_1` $\rightarrow$ `SYNC_2` $\rightarrow$ `SYNC_3` $\rightarrow$ `SYNC_4` $\rightarrow$ `PAYLOAD` $\rightarrow$ `TAIL_1` $\rightarrow$ `TAIL_2`) verified against `cfong2001/EBYTE_E54` (`E54_Radar.h`).
- **Frame Structure:**
  - Header: `0xAA 0xFF 0x03 0x00`
  - Payload: 24 bytes (3 targets $\times$ 8 bytes: X, Y, Speed, Resolution)
  - Tail: `0x55 0xCC`
- **Memory Footprint:** Fully optimized to fit within ATmega328P limits (**30,692 bytes** / 95% flash, leaving plenty of SRAM for local variables).

---

## 👤 Author & Acknowledgments

- **Lead Developer:** [Lian Mollick](https://github.com/mevrickneal) (@mevrickneal)
- **Organization:** Project Neal Defence Wing
- **Protocol Reference:** Adapted from the open-source LD2450 parser by `cfong2001/EBYTE_E54`.

---
*Project Neal Defence Wing - Tactical Avionics & Reconnaissance Division*
