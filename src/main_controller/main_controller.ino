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
  CMD_ENABLE = 1,
  CMD_CHECK = 2,
  CMD_DONE = 3
};

const uint8_t NODE_MSG_BASE = 10;

bool checked[4] = {false, false, false, false};
bool done[4] = {false, false, false, false};
bool disabling = false;

int winner = -1;

void sendCommandTo(int idx, Command cmd) {
  esp_err_t result = esp_now_send(REMOTE_MACS[idx], (uint8_t *)&cmd, sizeof(cmd));
  Serial.print("Sending command ");
  Serial.print(cmd);
  Serial.print(" to remote ");
  Serial.print(idx + 1);
  Serial.print(": ");
  Serial.println(result == ESP_OK ? "Success" : "Fail");
}

void broadcastCommand(Command cmd) {
  for (int i = 0; i < 4; i++) {
    sendCommandTo(i, cmd);
  }
}

bool allDone() {
  for (int i = 0; i < 4; i++) {
    if (!done[i]) return false;
  }
  return true;
}

void checkButtons() {
  for (int i = 0; i < 4; i++) {
    checked[i] = false;
    unsigned long start = millis();
    sendCommandTo(i, CMD_CHECK);
    while (!checked[i] && millis() - start < 3000) {
      digitalWrite(LED_PINS[i], HIGH);
      delay(100);
      digitalWrite(LED_PINS[i], LOW);
      delay(100);
    }
    if (checked[i]) {
      digitalWrite(LED_PINS[i], HIGH);
      delay(1000);
      digitalWrite(LED_PINS[i], LOW);
    }
  }
}

void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* incomingData, int len) {
  const uint8_t* mac = info->src_addr;
  int srcIdx = -1;
  for (int i = 0; i < 4; i++) {
    if (memcmp(mac, REMOTE_MACS[i], 6) == 0) {
      srcIdx = i;
      break;
    }
  }
  if (len == 1) {
    uint8_t val = incomingData[0];
    if (val == CMD_CHECK && srcIdx >= 0) {
      checked[srcIdx] = true;
    } else if (val == CMD_DONE && srcIdx >= 0) {
      done[srcIdx] = true;
    } else if (winner < 0 && val >= NODE_MSG_BASE + 1 && val <= NODE_MSG_BASE + 4) {
      int idx = val - NODE_MSG_BASE - 1;
      winner = idx;
      Serial.print("Winner is button ");
      Serial.println(idx + 1);
      digitalWrite(LED_PINS[idx], HIGH);
      for (int j = 0; j < 4; j++) {
        done[j] = false;
      }
      disabling = true;
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

  checkButtons();
  resetGame();
}

void loop() {
  if (disabling) {
    if (!allDone()) {
      broadcastCommand(CMD_DISABLE);
    } else {
      disabling = false;
    }
    delay(200);
  }
  if (digitalRead(RESET_PIN) == LOW) {
    resetGame();
    delay(200);
  }
}

