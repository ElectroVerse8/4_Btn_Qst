# 4 Button Quiz System

This repository contains Arduino sketches for a simple quiz-style button system using
nRF24L01 radios.

## Components

- Four battery-powered remote units (`remote_button1` to `remote_button4`).
  Each remote has a single push button and an nRF24L01 module.
- One main controller (`main_controller`) with four LEDs and a reset button.

## Behaviour

1. On start-up the main controller enables all remote buttons.
2. The first remote button pressed causes the corresponding LED on the main
   controller to light. All other remotes are disabled.
3. Pressing the reset button on the main controller turns off all LEDs and
   re-enables all remotes.

Remote sketches ignore button presses until an enable command is received from
the main controller, which helps conserve battery power.

Upload the appropriate sketch to each Arduino Nano. All sketches assume the
nRF24L01 is connected with CE on pin 9 and CSN on pin 10.


