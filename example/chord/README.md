# pico-button-events Chord Example

A demo of **chord detection**: four push buttons on GPIO 10–13 (each wired between its GPIO and GND, internal pull-ups). Instead of one click event per button, buttons held down together are combined into a single **chord**.

Each button is one bit, so a chord is a 4-bit value printed as a hex digit from **1 to F**:

| Buttons held | Bitmask | Hex |
| --- | --- | --- |
| 1 | `0x1` | `0x1` |
| 2 | `0x2` | `0x2` |
| 3 | `0x4` | `0x4` |
| 4 | `0x8` | `0x8` |
| 1 + 2 | `0x3` | `0x3` |
| 1 + 3 | `0x5` | `0x5` |
| 2 + 3 | `0x6` | `0x6` |
| 1 + 4 | `0x9` | `0x9` |
| 2 + 4 | `0xA` | `0xA` |
| 3 + 4 | `0xC` | `0xC` |
| 1 + 2 + 3 | `0x7` | `0x7` |
| … | | |
| 1 + 2 + 3 + 4 | `0xF` | `0xF` |

## Chord mode

The chord fires one of two ways, chosen by `CHORD_MODE` in `main.c` (this example ships in **press** mode):

- **`BUTTON_CHORD_MODE_PRESS`** (default here) — the hex digit is emitted as soon as the **pressed buttons have been stable** for the settle window (`chord_ms`, default 100 ms). The value tracks the buttons currently down, so it updates as you add or remove a button while holding.
- **`BUTTON_CHORD_MODE_RELEASE`** — the hex digit is emitted when the **last button is let go**. Buttons that overlap in time form one chord; a separate quick tap is its own chord.

To switch modes, edit the `CHORD_MODE` define in `main.c` and rebuild. Tune the press-mode settle window at runtime with `buttonSetTiming()` (the `chord_ms` field).

## Requirements

- Raspberry Pi Pico (or Pico 2 / any RP2040/RP2350 board)
- Four push buttons: one side to GND, the other to GPIO 10, 11, 12, 13
- Pico SDK installed

## Build

### 1. Set environment variables

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_BOARD=pico2
```

Or run the provided setup script:

```bash
source ./setup.sh
```

### 2. Build

```bash
mkdir -p build && cd build
cmake ..
make
```

The compiled firmware (`chord.uf2`) will be in the `build` directory.

## Flash

Copy `chord.uf2` to the Pico while holding the BOOTSEL button, or use:

```bash
picotool load build/chord.uf2
```

## Configuration

Edit `main.c` to adjust:

- `NUM_BUTTONS` — how many buttons to initialize (default: 4)
- `FIRST_BUTTON_GPIO` — GPIO of the first button; the rest follow (default: 10)
- `TICK_MS` — state-machine tick rate (default: 20)
