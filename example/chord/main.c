// pico-button-events chord example
//
// Demonstrates chord detection: four push buttons on GPIO 10-13 (each wired
// between its GPIO and GND, so the internal pull-up makes them active-low).
//
// Instead of one click event per button, buttons that are held down together
// form a single chord. Each button is one bit of the chord:
//
//   button 0 = bit 0, button 1 = bit 1, button 2 = bit 2, button 3 = bit 3
//
// so a chord is a 4-bit value printed as a hex digit from 1 to F:
//
//   one button  -> 0x1 / 0x2 / 0x4 / 0x8
//   two buttons -> 0x3 / 0x5 / 0x6 / 0x9 / 0xA / 0xC
//   three       -> 0x7 / 0xB / 0xD / 0xE
//   all four    -> 0xF
//
// When the hex digit is emitted is set by CHORD_MODE:
//   BUTTON_CHORD_MODE_RELEASE - when the last button is let go, so buttons
//     that overlap in time form one chord while a quick separate tap is its
//     own chord.
//   BUTTON_CHORD_MODE_PRESS   - as soon as the pressed buttons have been
//     stable for the settle window (chord_ms); the value tracks the buttons
//     currently down.

#include <stdio.h>

#include "pico/stdlib.h"

#include "pico-button-events.h"

#define NUM_BUTTONS 4
#define FIRST_BUTTON_GPIO 10 // buttons on GPIO 10, 11, 12, 13
#define TICK_MS 20           // tick rate for the state machine

// When the chord fires: release (last button let go) or press (once the
// pressed buttons have been stable for the settle window, chord_ms).
// #define CHORD_MODE BUTTON_CHORD_MODE_RELEASE
#define CHORD_MODE BUTTON_CHORD_MODE_PRESS

// Chord handler, called from buttonTickAll() with the bitmask of the buttons
// that were held together (bit i = button i).
static void onChord(uint mask)
{
    printf("Chord: 0x%X\n", mask);
}

int main()
{
    stdio_init_all();

    // Configure the buttons. A NULL per-button callback: we only want chord
    // events, not individual click/press/release events.
    for (uint i = 0; i < NUM_BUTTONS; i++)
        buttonInit(i, FIRST_BUTTON_GPIO + i, (button_event_cb)NULL);

    // Treat all four buttons as one chord group: bit i = button i.
    buttonChordInit(0, NUM_BUTTONS, onChord, CHORD_MODE);

    printf("pico-button-events chord example: %d buttons on GPIO %d-%d\n",
           NUM_BUTTONS, FIRST_BUTTON_GPIO, FIRST_BUTTON_GPIO + NUM_BUTTONS - 1);

    while (true)
    {
        // drive the debounce + state machine at a fixed ~20 ms rate
        buttonTickAll(get_absolute_time());
        sleep_ms(TICK_MS);
    }
}
