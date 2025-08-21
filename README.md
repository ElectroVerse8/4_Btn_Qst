# 4 Button Quiz System

This repository contains Arduino sketches for a simple quiz-style button system using
ESP32 modules communicating over the ESP-NOW protocol.

## Components

- Four battery-powered remote units (`remote_button1` to `remote_button4`).
  Each remote is an ESP32 with a single push button.
- One main controller (`main_controller`) using an ESP32 with four LEDs and a reset
  button.

## Behaviour

1. On start-up the main controller checks each remote in turn. It waits for a
   button press from the current remote before moving on to the next.
2. After all remotes have confirmed, the controller enables all remote buttons.
3. The first remote button pressed causes the corresponding LED on the main
   controller to light. All other remotes are disabled.
4. Pressing the reset button on the main controller turns off all LEDs and
   re-enables all remotes.

Remote sketches ignore button presses until an enable command is received from
the main controller, which helps conserve battery power.

Each remote sketch contains a `MAIN_MAC` array that must be set to the MAC address
of the main controller. The main controller sketch contains a `REMOTE_MACS`
array that must be populated with the MAC addresses of all remotes. You can obtain
these addresses by running `WiFi.macAddress()` on each ESP32.

Upload the appropriate sketch to each ESP32 using the Arduino IDE or
`arduino-cli`.


