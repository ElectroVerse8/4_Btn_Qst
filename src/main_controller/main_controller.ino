// =============================
// MAIN_CONTROLLER_ESP_NOW_3X.ino
// =============================
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

// ---------- CONFIG ----------
#define WIFI_CHANNEL 1
const int LED_PINS[4] = { 2, 4, 16, 17 };  // change to your pins
const int RESET_PIN   = 23;                // active LOW

// Put your 4 remote MACs here (copy from Serial prints on remotes)
uint8_t REMOTE_MACS[4][6] = {
  {0x3C,0x71,0xBF,0xAA,0xF2,0x50},
  {0x24,0x6F,0x28,0x44,0x55,0x66},
  {0x24,0x6F,0x28,0x77,0x88,0x99},
  {0x24,0x6F,0x28,0xAA,0xBB,0xCC}
};

// ---------- PROTOCOL ----------
enum MsgType : uint8_t { HELLO=1, HELLO_ACK, ARM, BUZZ, WIN, DISABLE, ENABLE, PING, DONE };

#pragma pack(push,1)
struct Msg {
  uint8_t  type;
  uint8_t  node_id;
  uint16_t seq;
  uint32_t t_us;
  uint8_t  payload8;
};
#pragma pack(pop)

const char* msgTypeStr(uint8_t type){
  switch(type){
    case HELLO:     return "HELLO";
    case HELLO_ACK: return "HELLO_ACK";
    case ARM:       return "ARM";
    case BUZZ:      return "BUZZ";
    case WIN:       return "WIN";
    case DISABLE:   return "DISABLE";
    case ENABLE:    return "ENABLE";
    case PING:      return "PING";
    case DONE:      return "DONE";
    default:        return "?";
  }
}

// ---------- STATE ----------
struct RemoteState {
  bool discovered = false;
  uint16_t lastSeq = 0;
} R[4];

volatile bool armed  = false;
volatile bool locked = false;
volatile uint8_t winnerId = 0;

uint16_t txSeq = 1;

static inline bool newer(uint16_t a, uint16_t b){ return (uint16_t)(a - b) != 0; }

void addPeer(const uint8_t mac[6]) {
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, mac, 6);
  p.channel = WIFI_CHANNEL;
  p.ifidx = WIFI_IF_STA;
  p.encrypt = false;
  esp_now_add_peer(&p);
}

void ledAllOff() { for (int i=0; i<4; i++) digitalWrite(LED_PINS[i], LOW); }

void ledBlinkIdx(int idx, int times, int onMs=60, int offMs=60){
  for (int k=0;k<times;k++){
    digitalWrite(LED_PINS[idx], HIGH); delay(onMs);
    digitalWrite(LED_PINS[idx], LOW);  delay(offMs);
  }
}

void sendAll(uint8_t type, uint8_t payload8=0){
  uint16_t seq = txSeq++;
  Msg m{type, 0, seq, (uint32_t)micros(), payload8};
  for (int i=0;i<4;i++) esp_now_send(REMOTE_MACS[i], (uint8_t*)&m, sizeof(m));
  Serial.printf("Main->All %s seq=%u payload=%u\n", msgTypeStr(type), seq, payload8);
}

void sendTo(uint8_t node_id_1to4, uint8_t type){
  if (node_id_1to4 < 1 || node_id_1to4 > 4) return;
  uint16_t seq = txSeq++;
  Msg m{type, node_id_1to4, seq, (uint32_t)micros(), 0};
  esp_now_send(REMOTE_MACS[node_id_1to4-1], (uint8_t*)&m, sizeof(m));
  Serial.printf("Main->%u %s seq=%u\n", node_id_1to4, msgTypeStr(type), seq);
}

// NEW 3.x RX callback signature
void onRecv(const esp_now_recv_info *info, const uint8_t *data, int len){
  if (len != sizeof(Msg) || !info) return;

  const uint8_t* mac = info->src_addr;
  Msg m; memcpy(&m, data, sizeof(Msg));

  // map sender MAC to index
  int idx = -1;
  for (int i=0;i<4;i++){
    if (memcmp(mac, REMOTE_MACS[i], 6) == 0) { idx = i; break; }
  }
  if (idx < 0) return;

  if (!newer(m.seq, R[idx].lastSeq)) return;
  R[idx].lastSeq = m.seq;

  Serial.printf("Main<- %u %s seq=%u\n", idx+1, msgTypeStr(m.type), m.seq);

  switch (m.type) {
    case HELLO:
    case PING: {
      R[idx].discovered = true;
      // LED tick confirm
      digitalWrite(LED_PINS[idx], HIGH); delay(50); digitalWrite(LED_PINS[idx], LOW);
      // Ack
      Msg ack{HELLO_ACK, (uint8_t)(idx+1), txSeq++, (uint32_t)micros(), 0};
      esp_now_send(REMOTE_MACS[idx], (uint8_t*)&ack, sizeof(ack));
      break;
    }
    case BUZZ: {
      if (locked) return;
      if (!winnerId) {
        // First BUZZ wins; optional tie-break could compare m.t_us, but ISR arrival is good enough
        winnerId = m.node_id ? m.node_id : (uint8_t)(idx+1);
        locked = true; armed = false;

        // Light winner LED solid
        for (int i=0;i<4;i++) digitalWrite(LED_PINS[i], i==(winnerId-1) ? HIGH : LOW);

        // Announce and freeze the round
        sendAll(WIN, winnerId);
        sendAll(DISABLE);
      }
      break;
    }
    default: break;
  }
}

void discoverSequence(){
  Serial.println("Discovering remotes...");
  uint32_t tStart = millis();
  while (true) {
    bool all = true;
    for (int i=0;i<4;i++){
      if (!R[i].discovered){
        all = false;
        // Your "check" behavior: flash the LED while pinging
        digitalWrite(LED_PINS[i], HIGH);
        sendTo(i+1, HELLO);
        delay(80);
        digitalWrite(LED_PINS[i], LOW);
        delay(80);
      }
    }
    if (all) break;
    if (millis() - tStart > 8000) break; // safety cap
  }
  Serial.println("Discovery complete");
}

void armGame(){
  winnerId = 0;
  locked   = false;
  armed    = true;
  ledAllOff();
  sendAll(ENABLE);
  delay(50);
  sendAll(ARM);
  Serial.println("Game armed");
}

void setup(){
  for (int i=0;i<4;i++){ pinMode(LED_PINS[i], OUTPUT); digitalWrite(LED_PINS[i], LOW); }
  pinMode(RESET_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    // Fatal: sweep forever
    while(true){
      for (int i=0;i<4;i++){ ledBlinkIdx(i,1,150,100); }
    }
  }

  // Add peers (remotes)
  for (int i=0;i<4;i++) addPeer(REMOTE_MACS[i]);

  // Register 3.x callback
  esp_now_register_recv_cb(onRecv);

  // (Optional) print my MAC to help fill MAIN_MAC on remotes
   Serial.begin(115200);
   Serial.print("Main MAC: "); Serial.println(WiFi.macAddress());
   Serial.println("Main controller setup complete");

  // Startup discovery and ready-up
  discoverSequence();
  armGame();
}

void loop(){
  // Reset for next round
  if (digitalRead(RESET_PIN) == LOW) {
    // quick visual sweep
    for (int k=0;k<2;k++){
      for (int i=0;i<4;i++){ digitalWrite(LED_PINS[i], HIGH); delay(60); digitalWrite(LED_PINS[i], LOW); }
    }
    armGame();
    delay(250);
    Serial.println("Game reset");
  }
}
