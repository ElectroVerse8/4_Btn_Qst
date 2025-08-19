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
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(ADDRESS);
  radio.openReadingPipe(1, ADDRESS);
  radio.startListening();
  Serial.println("Remote button 2 ready");
}

void loop() {
  if (radio.available()) {
    uint8_t cmd;
    radio.read(&cmd, sizeof(cmd));
    enabled = (cmd == CMD_ENABLE);
    Serial.print("Received command: ");
    Serial.println(enabled ? "ENABLE" : "DISABLE");
  }

  if (enabled && digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed");
    radio.stopListening();
    uint8_t msg = 2; // This node's ID
    Serial.println("Sending ID 2");
    bool ok = radio.write(&msg, sizeof(msg));
    Serial.println(ok ? "Send success" : "Send failed");
    radio.startListening();
    enabled = false;
    delay(50);
  }
}

