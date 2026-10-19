/*
  Trekker mesh: Tinkercad stand-in for one trekker node (Arduino Uno).

  Flash the same sketch on both trekker Unos and change NODE_ID:
    NODE_ID 1 = Trekker A (origin only)
    NODE_ID 2 = Trekker B (origin and relay for Trekker A)

  What is replaced in the simulation:
    MPU6050   -> pot on A0 (acceleration in g) + WALK and FALL buttons that inject a signal
    NEO-6M    -> simulated coordinates that drift while walking
    LoRa      -> SoftwareSerial 9600 baud (D3 = RX, D4 = TX)
    18650     -> pot on A1 (0..100 %)

  Time is compressed about 6x so a demo fits in a few minutes. Real values are in comments.
*/
#include <SoftwareSerial.h>

#define NODE_ID 1

const uint8_t PIN_ACC = A0, PIN_BAT = A1;
const uint8_t PIN_SOS = 2, PIN_RX = 3, PIN_TX = 4, PIN_WALK = 5, PIN_FALL = 6;
const uint8_t PIN_BUZ = 8, PIN_LR = 9, PIN_LG = 10, PIN_LB = 11;

const unsigned long WALK_INT = 5000;    // real 30 s
const unsigned long REST_INT = 20000;   // real 120 s
const unsigned long ABN_INT  = 2000;    // real 10 s
const unsigned long FALL_INT = 1000;    // real 6 s
const unsigned long SOS_INT  = 2000;    // real 10 s
const unsigned long CONFIRM  = 8000;    // real 30 s
const unsigned long INACT    = 30000;   // real 180 s
const unsigned long HOLD_OK  = 3000;    // hold SOS this long to clear an alert
const float IMPACT_G = 2.6;
const uint8_t N = 40;                   // 2 s window at 20 Hz

enum Act { RESTING, WALKING, ABNORMAL };
const char *ACT_NAME[] = { "RESTING", "WALKING", "ABNORMAL" };

SoftwareSerial radio(PIN_RX, PIN_TX);

float win[N];
uint8_t wi = 0, wn = 0;
float lat = 19.6040 - 0.0003 * NODE_ID;
float lon = 73.7000 + 0.0006 * NODE_ID;
Act act = RESTING;
uint8_t level = 0;                      // 0 normal, 1 fall suspected, 2 SOS
bool alerting = false, impact = false, fallOn = false;
unsigned long alertEnd = 0, restSince = 0, nextTx = 2000, blueUntil = 0, fallT = 0;
unsigned int seq = 0;
unsigned long seen[8];
uint8_t seenN = 0;

float readAccG(unsigned long now) {
  if (fallOn) {
    unsigned long t = now - fallT;
    if (t < 250) return 0.15;           // free fall
    if (t < 350) return 3.4;            // impact
    fallOn = false;
  }
  float g = analogRead(PIN_ACC) * 4.0 / 1023.0;
  if (digitalRead(PIN_WALK) == LOW) g += 0.5 * sin(2 * PI * 1.9 * now / 1000.0);
  return g < 0 ? 0 : g;
}

Act classify(float sd, float mx) {
  if (mx > 2.0 || sd > 0.9) return ABNORMAL;
  if (sd < 0.06) return RESTING;
  return WALKING;
}

unsigned long interval() {
  if (level == 2) return SOS_INT;
  if (level == 1) return FALL_INT;
  if (act == RESTING) return REST_INT;
  if (act == ABNORMAL) return ABN_INT;
  return WALK_INT;
}

void sendPacket() {
  char la[14], lo[14], buf[80];
  int bat = map(analogRead(PIN_BAT), 0, 1023, 0, 100);
  seq++;
  dtostrf(lat, 1, 5, la);
  dtostrf(lon, 1, 5, lo);
  snprintf(buf, sizeof(buf), "$TRK,%d,%u,%d,%s,%s,%d,%d,%d*",
           NODE_ID, seq, 1, la, lo, bat, (int)act, (int)level);
  radio.println(buf);
  Serial.print(F("TX    ")); Serial.println(buf);
  blueUntil = millis() + 80;
}

void handleRx(char *line) {
  char copy[80];
  strncpy(copy, line, sizeof(copy) - 1);
  copy[sizeof(copy) - 1] = 0;
  char *t = strtok(copy, ",");
  char *a = strtok(NULL, ",");
  char *b = strtok(NULL, ",");
  char *c = strtok(NULL, ",");
  if (!t || !a || !b || !c || strcmp(t, "$TRK") != 0) return;
  int id = atoi(a);
  unsigned int s = atoi(b);
  int hop = atoi(c);
  blueUntil = millis() + 80;
  if (id == NODE_ID) return;
  unsigned long key = ((unsigned long)id << 16) | s;
  for (uint8_t i = 0; i < seenN; i++) if (seen[i] == key) return;
  seen[seenN % 8] = key;
  if (seenN < 8) seenN++;
  char *p = line;
  for (uint8_t k = 0; k < 4; k++) { p = strchr(p, ','); if (!p) return; p++; }
  char out[80];
  snprintf(out, sizeof(out), "$TRK,%d,%u,%d,%s", id, s, hop + 1, p);
  radio.println(out);
  Serial.print(F("RELAY ")); Serial.println(out);
}

void radioPoll() {
  static char buf[80];
  static uint8_t n = 0;
  while (radio.available()) {
    char c = radio.read();
    if (c == '\n' || c == '\r') {
      if (n) { buf[n] = 0; handleRx(buf); n = 0; }
    } else if (n < sizeof(buf) - 1) buf[n++] = c;
  }
}

void clearAlert(unsigned long now) {
  level = 0; alerting = false; restSince = 0; nextTx = now;
  Serial.println(F("user OK: alert cleared"));
}

void triggerSos(unsigned long now) {
  level = 2; alerting = false; nextTx = now;
  Serial.println(F("SOS button: priority transmission"));
}

void handleButtons(unsigned long now) {
  if (digitalRead(PIN_FALL) == LOW && !fallOn) { fallOn = true; fallT = now; }
  static bool pressed = false, done = false;
  static unsigned long t0 = 0;
  bool p = digitalRead(PIN_SOS) == LOW;
  if (p) {
    if (!pressed) { pressed = true; done = false; t0 = now; }
    else if (!done && now - t0 >= HOLD_OK) { done = true; clearAlert(now); }
  } else {
    if (pressed && !done) triggerSos(now);
    pressed = false;
  }
}

void sample(unsigned long now) {
  float g = readAccG(now);
  win[wi] = g; wi = (wi + 1) % N; if (wn < N) wn++;
  if (g > IMPACT_G) {
    if (!impact) {
      impact = true;
      if (level == 0) {
        level = 1; alerting = true; alertEnd = now + CONFIRM; nextTx = now;
        Serial.print(F("FALL? impact ")); Serial.print(g, 1);
        Serial.println(F(" g: buzzer on, priority report"));
      }
    }
  } else impact = false;

  float s = 0, mx = 0, q = 0;
  for (uint8_t i = 0; i < wn; i++) { s += win[i]; if (win[i] > mx) mx = win[i]; }
  float m = s / wn;
  for (uint8_t i = 0; i < wn; i++) q += (win[i] - m) * (win[i] - m);
  Act a = classify(sqrt(q / wn), mx);
  if (a != act) {
    act = a;
    Serial.print(F("class -> ")); Serial.println(ACT_NAME[act]);
    if (act == WALKING) { lat += 0.00003; lon += 0.00002; }
  }
}

void updateAlerts(unsigned long now) {
  if (act == RESTING) { if (!restSince) restSince = now; } else restSince = 0;
  if (level == 0 && !alerting && restSince && now - restSince > INACT) {
    alerting = true; alertEnd = now + CONFIRM;
    Serial.println(F("no motion: buzzer on, SOS in 8 s unless user moves"));
  }
  if (alerting && level < 2) {
    if (act == WALKING) {
      Serial.println(level == 1 ? F("movement: false alarm cancelled") : F("movement: alert cancelled"));
      level = 0; alerting = false; restSince = 0; nextTx = now;
    } else if (now >= alertEnd) {
      level = 2; alerting = false; nextTx = now;
      Serial.println(F("no response: SOS confirmed"));
    }
  }
}

void outputs(unsigned long now) {
  bool buzz = (level > 0 || alerting) && ((now / 250) % 2);
  if (buzz) tone(PIN_BUZ, 2200); else noTone(PIN_BUZ);
  bool red = level == 2 || ((level == 1 || alerting) && ((now / 250) % 2));
  digitalWrite(PIN_LR, red);
  digitalWrite(PIN_LG, HIGH);
  digitalWrite(PIN_LB, now < blueUntil);
}

void setup() {
  Serial.begin(9600);
  radio.begin(9600);
  pinMode(PIN_SOS, INPUT_PULLUP); pinMode(PIN_WALK, INPUT_PULLUP); pinMode(PIN_FALL, INPUT_PULLUP);
  pinMode(PIN_BUZ, OUTPUT); pinMode(PIN_LR, OUTPUT); pinMode(PIN_LG, OUTPUT); pinMode(PIN_LB, OUTPUT);
  Serial.print(F("Trekker node T")); Serial.print(NODE_ID); Serial.println(F(" ready"));
}

void loop() {
  unsigned long now = millis();
  radioPoll();
  handleButtons(now);
  static unsigned long nextSample = 0;
  if (now >= nextSample) { nextSample = now + 50; sample(now); }
  updateAlerts(now);
  unsigned long iv = interval();
  if (nextTx > now + iv) nextTx = now + iv;
  if (now >= nextTx) { sendPacket(); nextTx = now + interval(); }
  outputs(now);
}
