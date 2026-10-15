# pico-button-events Examples

Two small demos. Each is a standalone CMake project — build either one from its own folder.

## Setup (both)

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_BOARD=pico2
# or: source ./setup.sh   (from within the example folder)
```

Build from inside an example folder:

```bash
mkdir -p build && cd build
cmake ..
make
```

## Examples

- **[`four-buttons/`](four-buttons/README.md)** — the original demo: four buttons on GPIO 10–13 printing every event the state machine emits (single/double/triple/quadruple clicks and long-press/release).
- **[`chord/`](chord/README.md)** — hold buttons together to emit a single **chord**; each button is one bit, so a chord is a hex digit from 1 to F.
