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
  const char *label = (cmd == CMD_ENABLE) ? "ENABLE" : "DISABLE";
  for (int i = 0; i < 4; i++) {
    radio.openWritingPipe(ADDRESSES[i]);
    Serial.print("Sending ");
    Serial.print(label);
    Serial.print(" to BTN");
    Serial.println(i + 1);
    bool ok = radio.write(&cmd, sizeof(cmd));
    Serial.println(ok ? "Send success" : "Send failed");
  }
  radio.startListening();
}

void resetGame() {
  Serial.println("Resetting game");
  for (int i = 0; i < 4; i++) {
    digitalWrite(LED_PINS[i], LOW);
  }
  winner = -1;
  broadcastCommand(CMD_ENABLE);
}

void setup() {
  Serial.begin(9600);
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
  Serial.println("Main controller ready");

  resetGame(); // Send initial enable and clear LEDs
}

void loop() {
  uint8_t pipe;
  if (radio.available(&pipe)) {
    uint8_t msg;
    radio.read(&msg, sizeof(msg));
    Serial.print("Received ");
    Serial.print(msg);
    Serial.print(" on pipe ");
    Serial.println(pipe);
    int idx = pipe - 1;
    if (winner < 0 && idx >= 0 && idx < 4) {
      winner = idx;
      Serial.print("Winner is button ");
      Serial.println(idx + 1);
      digitalWrite(LED_PINS[idx], HIGH);
      broadcastCommand(CMD_DISABLE);
    }
  }

  // Reset button logic
  if (digitalRead(RESET_PIN) == LOW) {
    Serial.println("Reset button pressed");
    resetGame();
    delay(200); // simple debounce
  }
}

