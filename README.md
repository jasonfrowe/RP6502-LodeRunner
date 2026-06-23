# Lode Runner for the Picocomputer 6502 (RP6502)

An optimized port of the classic arcade platformer *Lode Runner* to the Picocomputer 6502 hardware platform. This port features the original 23Hz engine logic, smooth 60Hz visual interpolations, beautiful retro graphics, and full OPL2 FM synthesis audio integration.

![Lode Runner Gameplay](ScreenShots/LodeRunner.gif)

---

## Features
- **Accurate Physics & AI**: Leverages classic *Lode Runner* movement rules, guard trapping, and level progression rules.
- **OPL2 Audio Integration**:
  - Background music (BGM) playback on the title screen utilizing raw YM3812 registers from `LODERUN.BIN`.
  - Sound effects (SFX) routed to channels 5, 6, 7, and 8 for digging, falling, guard trapping, gold acquisition, dying, and winning.
  - Active voice management preventing hanging notes during game pauses, deaths, or level loads.
- **Intuitive Inputs**: Native gamepad support and overhauled, ergonomically comfortable keyboard controls.
- **Quality-of-Life Safeguard (Stuck Guards)**: Automatically handles gold held by guards trapped in permanent pits. If a guard carrying gold remains stuck in the same tile for 10 seconds, the safeguard removes/destroys the gold rather than depositing it onto the playfield (preventing situations where players could get caught trying to retrieve it). The level's win condition adjusts dynamically, triggering the hidden exit ladders if no other gold remains. This safeguard can be disabled via the `STUCK_GUARD_GOLD_SAFEGUARD` compiler flag in `src/constants.h`.

---

## How to Play

### Keyboard Controls
| Action | Key |
| :--- | :--- |
| **Move Up / Climb** | `Up Arrow` |
| **Move Down / Fall** | `Down Arrow` |
| **Move Left** | `Left Arrow` |
| **Move Right** | `Right Arrow` |
| **Dig Left** | `Z` |
| **Dig Right** | `X` |
| **Pause / Resume / Start** | `P`, `Esc`, or `Enter` |

### Gamepad Controls
| Action | Button |
| :--- | :--- |
| **Movement** | `D-Pad` or `Left Analog Stick` |
| **Dig Left** | `X Button` (GP_BTN_X) |
| **Dig Right** | `A Button` (GP_BTN_A) |
| **Pause / Start** | `Start Button` (GP_BTN_START) |
| **Restart Level** | `Select Button` (GP_BTN_SELECT) |
Note: Restarting a level will result in a loss of one life.

---

## Building and Running

### Prerequisites
1. **LLVM-MOS SDK**: An LLVM compiler toolchain targeting MOS 6502 processors configured for the `rp6502` platform.
2. **CMake (version 3.18+)**.
3. **Python 3**: For asset processing scripts, with a virtual environment configured.

### Setting Up Python Dependencies
In the root directory, create a virtual environment and install the required dependencies (used by graphics and audio tools):
```bash
python3 -m venv .venv
source .venv/bin/activate
pip install pillow mido numpy
```

### Compiling the Project
To compile the asset files and build the target `.rp6502` executable:

1. **Configure CMake**:
   ```bash
   cmake -B build
   ```
2. **Build**:
   ```bash
   cmake --build build
   ```
This will compile all C source files and assemble the assets into `/build/LodeRunner.rp6502`.

### Running on the Picocomputer
Use the official `rp6502` loading tool to upload and run the executable on your hardware:
```bash
rp6502 build/LodeRunner.rp6502
```

---

## Development Utilities

### Graphics Asset Conversion
To regenerate the 4bpp sprite files and header palettes from standard PNG spritesheets:
```bash
python tools/convert_sprite.py --bpp 4 --mode tile --extract-palette graphics/player.png
```

### Level Map Packaging
To package and concatenate the binary levels file from the `/maps` directory:
```bash
python tools/convert_levels.py
```

### MIDI to OPL2 Synth Audio Converter
To convert standard MIDI soundtracks into the timing-accurate `.BIN` register write streams for the OPL2 chip:
```bash
./.venv/bin/python tools/midi_to_opl.py <input.mid> <output.bin>
```
