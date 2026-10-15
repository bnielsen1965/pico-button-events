// pico-button-events example
//
// Minimal single-core demo: four push buttons on GPIO 10-13 (each wired
// between its GPIO and GND, so the internal pull-up makes them active-low),
// printing every event the state machine emits.
//
// Try: single tap (clicked), double/triple/quadruple tap (within ~150 ms
// between taps), and a long hold (pressed, then released on let-go).

#include <stdio.h>

#include "pico/stdlib.h"

#include "pico-button-events.h"

#define NUM_BUTTONS 4
#define FIRST_BUTTON_GPIO 10 // buttons on GPIO 10, 11, 12, 13
#define TICK_MS 20           // tick rate for the state machine

// Event handler, called from buttonTickAll() for each button event
static void onButtonEvent(uint button_index, button_event_t event)
{
    printf("Button %d: %s\n", button_index + 1, buttonEventName(event));
}

int main()
{
    stdio_init_all();

    for (uint i = 0; i < NUM_BUTTONS; i++)
        buttonInit(i, FIRST_BUTTON_GPIO + i, onButtonEvent);

    printf("pico-button-events example: %d buttons on GPIO %d-%d\n",
           NUM_BUTTONS, FIRST_BUTTON_GPIO, FIRST_BUTTON_GPIO + NUM_BUTTONS - 1);

    while (true)
    {
        // drive the debounce + state machine at a fixed ~20 ms rate
        buttonTickAll(get_absolute_time());
        sleep_ms(TICK_MS);
    }
}
