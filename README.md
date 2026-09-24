Lab Report - 5

FINITE STATE MACHINE EXTENDED TO A STOPWATCH USING SYSTICK TIMER INTERRUPT, UART CONSOLE, AND 4X4 KEYPAD CONTROL

Sowmya Balaji, SR no. 28488

Electronic Systems Engineering, IISc

1. Objective

The objective of this assignment is to extend the finite state machine developed in Lab 4 to add a Stopwatch mode alongside the existing RGB LED colour/blink-speed control. The SysTick timer interrupt is used to keep track of elapsed time, and the stopwatch can be operated either from the UART console or from a 4x4 matrix keypad. The 4-digit multiplexed 7-segment display is time-shared between the two modes: it shows the stopwatch reading when Stopwatch mode is enabled, and reverts to displaying the onboard RGB LED status (colour and blink speed) from Lab 4 when disabled. Interrupt-driven handling is also added for the onboard SW1 and SW2 switches so that no input source relies on polling.

2. Hardware & GPIO Configuration

The experiment continues to use the TM4C123GH6PM LaunchPad together with the EduARM4 addon board. The onboard switches SW1 (PF4) and SW2 (PF0) are reconfigured to trigger GPIO edge-interrupts instead of being polled, so that a press is captured and debounced entirely within the interrupt handler. A 4x4 matrix keypad is interfaced through GPIO rows and columns and scanned to detect key presses. UART0 continues to communicate with the PC over the USB-to-serial connection for console commands. The SysTick timer is configured to generate a periodic interrupt (once every millisecond) that increments a global millisecond counter used to derive the stopwatch's elapsed time. The 4-digit multiplexed 7-segment display and the RGB LED on the addon board remain the shared outputs of the system.

3. Stopwatch Control via UART Console and 4x4 Keypad

A Stopwatch state, comprising an Enabled/Disabled flag, a Running/Stopped flag, and a Paused/Resumed flag, is added to the global state machine from Lab 4. Each of the three controls can be driven identically from the UART console or from the 4x4 keypad, and both input paths update the same shared state:

●  Enable/Disable: Toggles whether the 7-segment display shows the stopwatch or the Lab 4 RGB status. Triggered by a dedicated UART command, or, alternately, by pressing the first key (row 1, column 1) on the 4x4 keypad.

●  Start/Stop: Starts the SysTick-driven millisecond counter from zero and begins timing, or stops it and resets the elapsed time. Triggered by a UART command, or, alternately, by pressing the second key (row 1, column 2) on the keypad.

●  Pause/Resume: Freezes the millisecond counter without resetting it, holding the last displayed value, and resumes counting from that value on the next toggle. Triggered by a UART command, or, alternately, by pressing the third key (row 1, column 3) on the keypad.

Internally, the SysTick ISR only advances the millisecond counter when the stopwatch is Enabled, Running, and not Paused, so the same counter update logic serves both the switch/keypad and UART input paths without duplication. UART command parsing and keypad scanning are both debounced so that a single physical or typed action produces exactly one state transition.

4. 7-Segment Display Output

The program continuously multiplexes the 4-digit 7-segment display to reflect whichever mode is currently active, with refresh timing kept flicker-free regardless of which interrupt source last updated the state. When Stopwatch mode is enabled, the elapsed time is displayed across the four digits in the format S:S:ms:ms, with the two seconds digits and the two most significant milliseconds digits updated every SysTick tick. When Stopwatch mode is disabled, the display reverts to the Lab 4 layout, showing the onboard RGB LED's status: overall Status ('r'/'P'), colour code, and blink speed level.

5. Interrupt Support for Onboard Switches

SW1 and SW2 are configured with GPIO edge-triggered interrupts rather than being polled in the main loop, so that a press is serviced promptly regardless of what else the program is doing. Their interrupt service routines apply debouncing and then drive the same Lab 4 colour/blink-speed transitions as before, coexisting with the SysTick ISR that maintains the stopwatch counter and with UART reception, so that switch presses, keypad input, and console commands can all update the shared global state without conflict.

6. Conclusion

This assignment successfully extended the Lab 4 finite state machine with a Stopwatch mode timed by the SysTick interrupt and controllable from both the UART console and a 4x4 keypad, while retaining interrupt-driven (rather than polled) handling of the onboard SW1 and SW2 switches. The 4-digit multiplexed 7-segment display was shared cleanly between the Stopwatch reading and the original RGB status view, producing a flicker-free, glitch-free display across all input sources and both modes.
