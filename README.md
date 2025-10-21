# Logic Analyzer – Firmware & Backend

Firmware and serial backend for a browser‑based logic analyzer.  
This repository contains microcontroller sketches (Arduino‑compatible) that capture digital signals and stream them over serial for visualization in the **Web GUI**.

> **Fork notice:** This project is a fork of [aster94/logic-analyzer](https://github.com/aster94/logic-analyzer), adapted and simplified to work seamlessly with the new Web GUI and modern tooling.

---

## Overview

The backend runs on supported microcontrollers and:
- Samples digital input pins
- Timestamps transitions
- Streams compact capture data over **Serial**
- Integrates with the **Web GUI** (HTML5 Canvas + Web Serial) for live viewing and analysis

**Frontend Web GUI:** `https://github.com/sancho11/logic-analyzer-computerinterface`

---

## Features

- **Multiple boards supported:** Arduino **UNO**, **MEGA**, **STM32F1**, **ESP8266**
- **Edge-based capture:** Records pin changes with timestamps for efficient streaming
- **Lightweight protocol:** Simple line‑oriented serial messages, easy to extend
- **Test sketch included:** Quickly validate wiring and visualize known patterns
- **Open-source & hackable:** Clear code paths to add boards or tweak timing

---

## Repository Structure

```
logic-analyzer/
├─ LICENSE
├─ README.md                        # (You are here)
├─ .gitignore
├─ Microcontroller_Code/
│  ├─ UNO/UNO.ino                   # Arduino Uno firmware
│  ├─ MEGA/MEGA.ino                 # Arduino Mega firmware
│  ├─ STM32F1/STM32F1.ino           # STM32F1 (Arduino_STM32 core)
│  └─ ESP8266/ESP8266.ino           # ESP8266 (3.3V logic)
└─ Extras/
   └─ tester/tester.ino             # Generates known waveforms for validation
```

---

## Supported Boards

- **Arduino UNO (ATmega328P)** – 5V logic, limited RAM  
- **Arduino MEGA (ATmega2560)** – more pins and RAM for deeper captures  
- **STM32F1** (via [Arduino_STM32 core](https://github.com/rogerclarkmelbourne/Arduino_STM32)) – faster sampling potential  
- **ESP8266** – 3.3V logic; ensure proper level shifting

> ⚠️ **Logic levels & safety**  
> • Never feed **5V** into **3.3V‑only** boards (e.g., ESP8266) without level shifting.  
> • Keep total input current within MCU specs; use series resistors if unsure.

---

## Getting Started

### Requirements
- **Arduino IDE** (or **PlatformIO**)
- Appropriate **USB drivers** for your board
- A supported browser for the GUI (Chrome/Edge for Web Serial) — used on the frontend

### 1) Flash the Firmware
1. Open the correct sketch from `Microcontroller_Code/<BOARD>/<BOARD>.ino`.  
2. In **Arduino IDE**, select the **Board** and **Port**.  
3. Click **Upload**.

### 2) (Optional) Validate with the Tester
1. Flash `Extras/tester/tester.ino` to a spare board.  
2. Wire the tester’s output pins to the analyzer board’s inputs.  
3. Use the Web GUI to verify you see the expected patterns.

### 3) Use with the Web GUI
1. Open the **Web GUI**: `https://github.com/sancho11/logic-analyzer-computerinterface`  
2. Click **Connect**, select your device’s **serial port**, and choose the correct **board profile**.  
3. Press **Start** to capture and visualize signals (or use **Simulate** in the GUI if you’re testing without hardware).

---

## High‑Level Protocol

The firmware:
- Monitors configured digital pins  
- On each transition, records **timestamp** + **pin state mask** (or pin index)  
- Streams records over **Serial** in a compact, line‑oriented format that the Web GUI parses in real time

**Notes**
- Timestamps are microsecond or tick‑based depending on the board/timer setup.  
- Keep serial baud rate and buffer sizes in sync with GUI defaults (tune in code as needed).  
- If you customize the message format, update the GUI parser accordingly.

---

## Performance & Limits

- **Depth** and **throughput** depend on board speed, baud rate, and number of active pins.  
- On smaller MCUs, prefer **edge‑based capture** (only changes) over fixed‑rate sampling to maximize effective detail.  
- For higher‑frequency signals, pick faster MCUs with more RAM, and reduce the number of monitored pins.

---

## Extending / Porting

- Start from the closest board in `Microcontroller_Code/`  
- Keep the same **message format** to remain compatible with the Web GUI  
- Document board‑specific **pin maps** or **LED indicators** in code comments  
- Submit improvements as PRs (see **Contributing**)

---

## Contributing

Contributions are welcome!

1. **Fork** the repository  
2. Create a feature branch:
   ```bash
   git checkout -b feat/stm32f1-timer-tuning
   ```
3. Make your changes with clear commits  
4. Ensure the firmware builds and streams correctly; test with the **Web GUI**  
5. Open a **Pull Request** with a concise description and any scope/limits

**Coding tips**
- Keep the **capture loop minimal**; favor post‑processing where possible  
- Avoid magic numbers; prefer `#define`/`const` with comments  
- Be mindful of ISR time and serial buffer sizes

---

## License

This project is licensed under the **GNU General Public License v3.0 (GPL‑3.0)** — see the `LICENSE` file for details.  
Original work by [aster94](https://github.com/aster94), forked and adapted for the Web GUI.

