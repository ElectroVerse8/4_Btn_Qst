// ============================
// REMOTE_BUTTON_ESP_NOW_3X.ino
// ============================
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
//extern "C" void ets_printf(const char* fmt, ...);

// ---------- CONFIG ----------
#define WIFI_CHANNEL 1           // All devices must match
#define NODE_ID     2            // <-- set 1..4 uniquely per remote
#define LED_PIN     2            // Your LED pin
#define BUTTON_PIN  23           // Your button pin (active LOW)

// Main controller MAC (replace with your controller's MAC!)
uint8_t MAIN_MAC[6] = { 0x3C,0x71,0xBF,0xAB,0x0D,0xBC};

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
volatile bool armed   = false;
volatile bool locked  = false;
volatile bool gotWin  = false;
volatile uint8_t winnerId = 0;

uint16_t txSeq = 1;
uint16_t lastSeqFromMain = 0;

static inline bool newer(uint16_t a, uint16_t b) { return (uint16_t)(a - b) != 0; }

void blink(uint8_t times, uint16_t onMs=60, uint16_t offMs=60) {
  for (uint8_t i=0;i<times;i++){
    digitalWrite(LED_PIN, HIGH); delay(onMs);
    digitalWrite(LED_PIN, LOW);  delay(offMs);
  }
}

void addPeer(const uint8_t mac[6]) {
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, mac, 6);
  p.channel = WIFI_CHANNEL;
  p.ifidx = WIFI_IF_STA;
  p.encrypt = false;
  esp_now_add_peer(&p);
}

void sendToMain(uint8_t type, uint8_t payload8=0, uint32_t tstamp=0) {
  uint16_t seq = txSeq++;
  Msg m{type, NODE_ID, seq, tstamp ? tstamp : (uint32_t)micros(), payload8};
  esp_now_send(MAIN_MAC, (uint8_t*)&m, sizeof(m));
  Serial.printf("Remote%u->Main %s seq=%u payload=%u\n", NODE_ID, msgTypeStr(type), seq, payload8);
}

// NEW 3.x RX callback signature
void onRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(Msg) || !info) return;

  const uint8_t* mac = info->src_addr;          // sender MAC (main)
  Msg m; memcpy(&m, data, sizeof(Msg));

  // Only trust main
  if (memcmp(mac, MAIN_MAC, 6) != 0) return;
  if (!newer(m.seq, lastSeqFromMain)) return;
  lastSeqFromMain = m.seq;

  Serial.printf("Remote%u<-Main %s seq=%u\n", NODE_ID, msgTypeStr(m.type), m.seq);

  switch (m.type) {
    case HELLO:
    case PING:
      sendToMain(HELLO_ACK);
      break;
    case HELLO_ACK:
      // quick visual confirm
      digitalWrite(LED_PIN, HIGH); delay(60); digitalWrite(LED_PIN, LOW);
      break;
    case ARM:
      armed  = true;
      locked = false;
      gotWin = false;
      winnerId = 0;
      break;
    case WIN:
      gotWin = true;
      winnerId = m.payload8;
      if (winnerId == NODE_ID) {
        digitalWrite(LED_PIN, HIGH); // hold until DISABLE
      } else {
        blink(2, 50, 50);
      }
      break;
    case DISABLE:
      armed  = false;
      locked = true;
      digitalWrite(LED_PIN, LOW);
      // Optional: enter light sleep here to save battery
      // esp_sleep_enable_timer_wakeup(5ULL * 60 * 1000000ULL);
      // esp_light_sleep_start();
      break;
    case ENABLE:
      // ready for next ARM (no action required)
      break;
    default: break;
  }
}

void IRAM_ATTR onButtonISR() {
  if (!armed || locked) return;
  static uint32_t lastInt = 0;
  uint32_t now = micros();
  if ((now - lastInt) < 20000) return; // 20 ms debounce
  lastInt = now;

  // Lock immediately; send exactly once
  locked = true;
  armed  = false;
  sendToMain(BUZZ, /*payload*/0, now);
  ets_printf("Remote%u button pressed\n", NODE_ID);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), onButtonISR, FALLING);

  // Force channel and init ESP-NOW
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    // Fatal: slow blink forever
    while (true) { blink(1, 400, 400); }
  }

  addPeer(MAIN_MAC);
  esp_now_register_recv_cb(onRecv);

  // Announce presence
  sendToMain(HELLO);

   //(Optional) Print my MAC for pairing
   Serial.begin(115200);
   Serial.print("Remote MAC: "); Serial.println(WiFi.macAddress());
   Serial.println("Remote setup complete");
}

void loop() {
  // Minimal keepalive while not locked
  static uint32_t t0 = 0;
  if (!locked && millis() - t0 > 1000) {
    t0 = millis();
    sendToMain(PING);
    Serial.println("Sent PING");
  }
}
