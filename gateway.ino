/*
  Trekker mesh: Tinkercad stand-in for the base camp gateway (Arduino Uno).

  The Serial Monitor plays the part of the MQTT broker and web dashboard.
  Radio input: SoftwareSerial 9600 baud on D3 (connect to the last trekker's D4).
  Button on D2 (to GND) acknowledges an alarm and silences it for 30 s.
*/
#include <SoftwareSerial.h>

const uint8_t PIN_ACK = 2, PIN_RX = 3, PIN_TX = 4;
const uint8_t PIN_BUZ = 8, PIN_LR = 9, PIN_LG = 10, PIN_LB = 11;
const char *ACT_NAME[] = { "RESTING", "WALKING", "ABNORMAL" };

SoftwareSerial radio(PIN_RX, PIN_TX);
bool alarm = false;
int alarmNode = 0, alarmLevel = 0;
unsigned long ackedUntil = 0, blueUntil = 0;

void handlePacket(char *line, unsigned long now) {
  char *t   = strtok(line, ",*");
  char *id  = strtok(NULL, ",*");
  char *sq  = strtok(NULL, ",*");
  char *hop = strtok(NULL, ",*");
  char *la  = strtok(NULL, ",*");
  char *lo  = strtok(NULL, ",*");
  char *bat = strtok(NULL, ",*");
  char *ac  = strtok(NULL, ",*");
  char *lv  = strtok(NULL, ",*");
  if (!t || !id || !sq || !hop || !la || !lo || !bat || !ac || !lv || strcmp(t, "$TRK") != 0) return;
  int a = constrain(atoi(ac), 0, 2);
  int level = atoi(lv);
  blueUntil = now + 80;

  Serial.print(F("trek/base/t")); Serial.print(id);
  Serial.print(level ? F("/alert ") : F("/telemetry "));
  Serial.print(F("{\"seq\":")); Serial.print(sq);
  Serial.print(F(",\"hops\":")); Serial.print(hop);
  Serial.print(F(",\"lat\":")); Serial.print(la);
  Serial.print(F(",\"lon\":")); Serial.print(lo);
  Serial.print(F(",\"bat\":")); Serial.print(bat);
  Serial.print(F(",\"act\":\"")); Serial.print(ACT_NAME[a]);
  Serial.print(F("\",\"level\":")); Serial.print(level);
  Serial.println(F("}"));

  if (level >= 1 && now > ackedUntil) {
    if (!alarm || level > alarmLevel) {
      Serial.print(level == 2 ? F("*** SOS from T") : F("*** possible fall from T"));
      Serial.print(id); Serial.print(F(" at ")); Serial.print(la); Serial.print(F(", ")); Serial.println(lo);
    }
    alarm = true; alarmNode = atoi(id); alarmLevel = level;
  }
}

void radioPoll(unsigned long now) {
  static char buf[90];
  static uint8_t n = 0;
  while (radio.available()) {
    char c = radio.read();
    if (c == '\n' || c == '\r') {
      if (n) { buf[n] = 0; handlePacket(buf, now); n = 0; }
    } else if (n < sizeof(buf) - 1) buf[n++] = c;
  }
}

void setup() {
  Serial.begin(9600);
  radio.begin(9600);
  pinMode(PIN_ACK, INPUT_PULLUP);
  pinMode(PIN_BUZ, OUTPUT); pinMode(PIN_LR, OUTPUT); pinMode(PIN_LG, OUTPUT); pinMode(PIN_LB, OUTPUT);
  Serial.println(F("Base camp gateway online. Listening for trekker packets."));
}

void loop() {
  unsigned long now = millis();
  radioPoll(now);
  if (digitalRead(PIN_ACK) == LOW && alarm) {
    alarm = false; alarmLevel = 0; ackedUntil = now + 30000UL;
    Serial.print(F("alarm for T")); Serial.print(alarmNode); Serial.println(F(" acknowledged"));
  }
  bool blink = (now / 250) % 2;
  if (alarm && blink) tone(PIN_BUZ, 2200); else noTone(PIN_BUZ);
  digitalWrite(PIN_LR, alarm && blink);
  digitalWrite(PIN_LG, HIGH);
  digitalWrite(PIN_LB, now < blueUntil);
}
