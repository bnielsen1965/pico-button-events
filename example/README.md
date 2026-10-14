# pico-button-events Example

A minimal demo: four push buttons on GPIO 10–13 (each wired between its GPIO and GND, internal pull-ups) printing every event the state machine emits — single/double/triple/quadruple clicks and long-press/release.

## Requirements

- Raspberry Pi Pico (or Pico 2 / any RP2040/RP2350 board)
- Four push buttons: one side to GND, the other to GPIO 10, 11, 12, 13
- Pico SDK installed

## Build

### 1. Set environment variables

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_BOARD=pico
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

The compiled firmware (`example.uf2`) will be in the `build` directory.

## Flash

Copy `example.uf2` to the Pico while holding the BOOTSEL button, or use:

```bash
picotool load build/example.uf2
```

## Configuration

Edit `main.c` to adjust:

- `NUM_BUTTONS` — how many buttons to initialize (default: 4)
- `FIRST_BUTTON_GPIO` — GPIO of the first button; the rest follow (default: 10)
- `TICK_MS` — state-machine tick rate (default: 20)
