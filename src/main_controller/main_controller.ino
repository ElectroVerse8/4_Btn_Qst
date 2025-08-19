#include <RF24.h>

// nRF24L01 pins: CE on 9, CSN on 10
RF24 radio(9, 10);

const byte ADDRESSES[4][6] = {"BTN1", "BTN2", "BTN3", "BTN4"};

const int LED_PINS[4] = {2, 3, 4, 5};
const int RESET_PIN = 6;

enum Command : uint8_t {
  CMD_DISABLE = 0,
  CMD_ENABLE = 1
};

int winner = -1;

void broadcastCommand(Command cmd) {
  radio.stopListening();
  for (int i = 0; i < 4; i++) {
    radio.openWritingPipe(ADDRESSES[i]);
    radio.write(&cmd, sizeof(cmd));
  }
  radio.startListening();
}

void resetGame() {
  for (int i = 0; i < 4; i++) {
    digitalWrite(LED_PINS[i], LOW);
  }
  winner = -1;
  broadcastCommand(CMD_ENABLE);
}

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }
  pinMode(RESET_PIN, INPUT_PULLUP);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);

  // Open reading pipes for each remote
  for (int i = 0; i < 4; i++) {
    radio.openReadingPipe(i + 1, ADDRESSES[i]);
  }
  radio.startListening();

  resetGame(); // Send initial enable and clear LEDs
}

void loop() {
  uint8_t pipe;
  if (radio.available(&pipe)) {
    uint8_t msg;
    radio.read(&msg, sizeof(msg));
    int idx = pipe - 1;
    if (winner < 0 && idx >= 0 && idx < 4) {
      winner = idx;
      digitalWrite(LED_PINS[idx], HIGH);
      broadcastCommand(CMD_DISABLE);
    }
  }

  // Reset button logic
  if (digitalRead(RESET_PIN) == LOW) {
    resetGame();
    delay(200); // simple debounce
  }
}

