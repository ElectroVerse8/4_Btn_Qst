#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Replace with your remote units' MAC addresses
uint8_t REMOTE_MACS[4][6] = {
  {0x3C, 0x71, 0xBF, 0xAB, 0x0D, 0xBC},
  {0x24, 0x6F, 0x28, 0x44, 0x55, 0x66},
  {0x24, 0x6F, 0x28, 0x77, 0x88, 0x99},
  {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC}
};

const int LED_PINS[4] = {2, 4, 16, 17};
const int RESET_PIN = 23;

enum Command : uint8_t {
  CMD_DISABLE = 0,
  CMD_ENABLE = 1
};

int winner = -1;

void broadcastCommand(Command cmd) {
  const char *label = (cmd == CMD_ENABLE) ? "ENABLE" : "DISABLE";
  for (int i = 0; i < 4; i++) {
    esp_err_t result = esp_now_send(REMOTE_MACS[i], (uint8_t *)&cmd, sizeof(cmd));
    Serial.print("Sending ");
    Serial.print(label);
    Serial.print(" to remote ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(result == ESP_OK ? "Success" : "Fail");
  }
}

void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* incomingData, int len) {
  const uint8_t* mac = info->src_addr;
  if (len == 1 && winner < 0) {
    int idx = incomingData[0] - 1;
    if (idx >= 0 && idx < 4) {
      winner = idx;
      Serial.print("Winner is button ");
      Serial.println(idx + 1);
      digitalWrite(LED_PINS[idx], HIGH);
      broadcastCommand(CMD_DISABLE);
    }
  }
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
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }
  pinMode(RESET_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  ESP_ERROR_CHECK(esp_now_init());
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  for (int i = 0; i < 4; i++) {
    esp_now_peer_info_t peerInfo{};
    memcpy(peerInfo.peer_addr, REMOTE_MACS[i], 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  resetGame();
}

void loop() {
  if (digitalRead(RESET_PIN) == LOW) {
    resetGame();
    delay(200);
  }
}

