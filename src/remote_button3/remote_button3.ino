#include <esp_now.h>
#include <WiFi.h>

// Replace with your main controller's MAC address
uint8_t MAIN_MAC[6] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};

const uint8_t NODE_ID = 3;
const int BUTTON_PIN = 2;

enum Command : uint8_t {
  CMD_DISABLE = 0,
  CMD_ENABLE = 1
};

bool enabled = false;

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len == 1) {
    enabled = (incomingData[0] == CMD_ENABLE);
    Serial.print("Received command: ");
    Serial.println(enabled ? "ENABLE" : "DISABLE");
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

  Serial.println("Remote button 3 ready");
}

void loop() {
  if (enabled && digitalRead(BUTTON_PIN) == LOW) {
    uint8_t msg = NODE_ID;
    esp_err_t result = esp_now_send(MAIN_MAC, &msg, sizeof(msg));
    Serial.println(result == ESP_OK ? "Send success" : "Send failed");
    enabled = false;
    delay(50);
  }
}

