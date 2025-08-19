#include <RF24.h>

RF24 radio(9, 10);

const byte ADDRESS[6] = "BTN2"; // Unique pipe address for this node

const int BUTTON_PIN = 2;

enum Command : uint8_t {
  CMD_DISABLE = 0,
  CMD_ENABLE = 1
};

bool enabled = false;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(ADDRESS);
  radio.openReadingPipe(1, ADDRESS);
  radio.startListening();
}

void loop() {
  if (radio.available()) {
    uint8_t cmd;
    radio.read(&cmd, sizeof(cmd));
    enabled = (cmd == CMD_ENABLE);
  }

  if (enabled && digitalRead(BUTTON_PIN) == LOW) {
    radio.stopListening();
    uint8_t msg = 2; // This node's ID
    radio.write(&msg, sizeof(msg));
    radio.startListening();
    enabled = false;
    delay(50);
  }
}

