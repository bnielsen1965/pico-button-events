# pico-button-events

Debounced push-button event detection for Raspberry Pi Pico, distributed as a CMake INTERFACE library.

A C port of the JavaScript [button-events](https://github.com/bnielsen1965/button-events) module: GPIO edge interrupts feed a small state machine that produces debounced `clicked`, `double_clicked`, `triple_clicked`, `quadruple_clicked`, `pressed` (long press) and `released` events.

## Features

- Click, double/triple/quadruple-click, long-press, and release events
- Configurable debounce / long-press / multi-click-chaining windows (default 30 / 150 / 150 ms)
- ISR only records edges; all state-machine work happens in the tick, so the IRQ path stays tiny
- Self-healing level resync: a missed edge re-syncs within one tick, so the machine can never get stuck out of step with the physical button
- Up to 8 buttons per build (`BUTTON_MAX`)
- OS-agnostic and app-agnostic: works bare-metal (single or dual core) and under FreeRTOS

## Supported hardware

- Raspberry Pi Pico (RP2040) and Pico 2 (RP2350)
- Cortex-M or Hazard3 RISC-V cores (RP2350)
- Buttons wired **active-low**: one side to GND, the other to a GPIO. The module enables the internal pull-up, so no external resistors are needed.

## Requirements

- Pico SDK installed and `PICO_SDK_PATH` set
- CMake 3.12+
- An ARM or RISC-V GCC toolchain (the Pico SDK handles whichever platform you select)

## Usage in Your CMake Project

### 1. Include the library

```cmake
add_subdirectory(/path/to/pico-button-events build_button_events)
```

### 2. Link your target

```cmake
target_link_libraries(your_target pico_stdlib pico-button-events)
```

### 3. Use in code

```c
#include "pico-button-events.h"

static void onEvent(uint button_index, button_event_t event)
{
    printf("Button %d: %s\n", button_index + 1, buttonEventName(event));
}

int main(void)
{
    stdio_init_all();

    buttonInit(0, 12, onEvent);  // GPIO 12 to GND

    while (true)
    {
        buttonTickAll(get_absolute_time());
        sleep_ms(20);
    }
}
```

## API

| Function | Description |
| --- | --- |
| `buttonInit(index, gpio, callback)` | Configure a button: input with internal pull-up, both-edge IRQ. Must run on the core that will call `buttonTickAll()`. |
| `buttonTickAll(now)` | Run the debounce + state machine and emit pending events. Call from **one** core, roughly every 20 ms. |
| `buttonGetTiming(t)` / `buttonSetTiming(t)` | Get/set the timing windows (`debounce_ms`, `pressed_ms`, `clicked_ms`) for all buttons. Settable at runtime; call from the ticking core. |
| `buttonEventName(event)` | Human-readable name of an event, for logging. |

### Event semantics (mirrors the JS module)

- Tap under `pressed_ms` → `clicked`. A new tap within `clicked_ms` of the release chains: `double_clicked`, `triple_clicked`; the fourth press emits `quadruple_clicked` immediately.
- Hold past `pressed_ms` → `pressed` (long press); letting go emits `released`.
- A click followed by a re-press while held emits the `*_PRESSED` variants.

## Multi-core contract

The core that calls `buttonInit()` owns the GPIO IRQs (SIO GPIO IRQs are per-core) and **must be the same core that calls `buttonTickAll()`**. With the ISR and the tick on one core, the ISR merely preempts the tick loop, so the module needs no locks.

Other cores/tasks that want button events should receive them out of the event callback (app-level IPC — a queue or semaphore).

If the ticking core is busy (e.g. running a long command), events coalesce: a mash collapses to one settled transition. This matches the JS module's semantics, and the self-healing resync keeps the machine correct afterward.

## Example

See [`example/`](example/README.md) — four buttons on GPIO 10–13 printing every event.

## Roadmap

- Polarity option (buttons wired to 3V3, active-high)
- Per-button timing overrides (e.g. a 2 s power button next to a 100 ms escape button)
- FreeRTOS SMP notes / demo (atomic edge handoff)

## License

MIT — see [LICENSE](LICENSE).
