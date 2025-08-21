#include <esp_now.h>
#include <WiFi.h>

// Replace with your main controller's MAC address
uint8_t MAIN_MAC[6] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};

const uint8_t NODE_ID = 2;
const int BUTTON_PIN = 2;

enum Command : uint8_t {
  CMD_DISABLE = 0,
  CMD_ENABLE = 1,
  CMD_CHECK = 2,
  CMD_DONE = 3
};

const uint8_t NODE_MSG_BASE = 10;

bool enabled = false;
bool checking = false;

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == 1) {
    uint8_t cmd = incomingData[0];
    if (cmd == CMD_ENABLE) {
      enabled = true;
      Serial.println("Received command: ENABLE");
    } else if (cmd == CMD_DISABLE) {
      enabled = false;
      Serial.println("Received command: DISABLE");
      uint8_t done = CMD_DONE;
      esp_now_send(mac, &done, sizeof(done));
    } else if (cmd == CMD_CHECK) {
      Serial.println("Received command: CHECK");
      checking = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  esp_now_peer_info_t peerInfo{};
  memcpy(peerInfo.peer_addr, MAIN_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("Remote button 2 ready");
}

void loop() {
  static unsigned long lastSend = 0;
  if (checking && digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastSend > 100) {
      uint8_t resp = CMD_CHECK;
      esp_now_send(MAIN_MAC, &resp, sizeof(resp));
      checking = false;
      lastSend = now;
    }
  }
  if (enabled && digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastSend > 100) {
      uint8_t msg = NODE_MSG_BASE + NODE_ID;
      esp_err_t result = esp_now_send(MAIN_MAC, &msg, sizeof(msg));
      Serial.println(result == ESP_OK ? "Send success" : "Send failed");
      lastSend = now;
    }
  }
}

