// Single ESP32-C3: tracker + simulated gateway.
// Wokwi can't simulate a second MCU yet, so the "radio link" is a function call
// (radioSend -> gatewayReceive). The gateway output goes to the OLED + serial monitor.

#include <Wire.h>
#include <HardwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <TinyGPSPlus.h>

#define I2C_SDA 4           // MPU6050 + OLED share this bus
#define I2C_SCL 5
#define GPS_RX 6            // ESP receives  <- GPS TX
#define GPS_TX 7            // ESP transmits -> GPS RX
#define BUTTON_SOS 1
#define BUZZER 10
#define LED_RED 2
#define LED_GREEN 3
#define LED_BLUE 0

Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

unsigned long lastPrint = 0;
unsigned long lastTrigger = 0;
int packetCount = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("--- Booting tracker + gateway ---");

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  pinMode(BUTTON_SOS, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_GREEN, HIGH);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED not found!");
  showIdle();

  if (!mpu.begin()) {
    Serial.println("Critical Error: MPU6050 missing!");
    while (1);
  }
  Serial.println("MPU6050 found. Gateway listening.");
}

void loop() {
  while (gpsSerial.available() > 0) gps.encode(gpsSerial.read());

  if (millis() - lastPrint > 20000) {
    lastPrint = millis();
    if (gps.location.isValid()) {
      Serial.print("GPS FIX  lat: "); Serial.print(gps.location.lat(), 6);
      Serial.print("  lng: "); Serial.print(gps.location.lng(), 6);
      Serial.print("  sats: "); Serial.println(gps.satellites.value());
    } else {
      Serial.print("GPS: acquiring fix... (chars received: ");
      Serial.print(gps.charsProcessed()); Serial.println(")");
    }
  }

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float acc = sqrt(a.acceleration.x * a.acceleration.x +
                   a.acceleration.y * a.acceleration.y +
                   a.acceleration.z * a.acceleration.z);

  bool sos = (digitalRead(BUTTON_SOS) == LOW);
  bool fall = (acc > 25.0);
  if ((sos || fall) && millis() - lastTrigger > 3000) {
    lastTrigger = millis();
    triggerEmergency(fall);
  }
  delay(100);
}

// ---------------- TRACKER SIDE ----------------
void triggerEmergency(bool isFall) {
  const char *event = isFall ? "FALL" : "SOS";
  Serial.print("*** EMERGENCY: "); Serial.println(event);

  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  digitalWrite(BUZZER, HIGH);

  char buf[120];
  int fix = gps.location.isValid() ? 1 : 0;
  double lat = fix ? gps.location.lat() : 0.0;
  double lng = fix ? gps.location.lng() : 0.0;
  snprintf(buf, sizeof(buf), "TRK01,%s,%d,%.6f,%.6f,%lu",
           event, fix, lat, lng, millis() / 1000);

  Serial.print("Packet transmitted -> "); Serial.println(buf);
  radioSend(buf);

  delay(1500);
  digitalWrite(BUZZER, LOW);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_GREEN, HIGH);
}

// Simulated radio link. On real hardware this is where LoRa.print() would go,
// and gatewayReceive() would run on the other board.
void radioSend(const char *pkt) {
  delay(300);                 // fake airtime
  gatewayReceive(String(pkt));
}

// ---------------- GATEWAY SIDE ----------------
void gatewayReceive(String pkt) {
  packetCount++;
  String f[6];
  int start = 0;
  for (int i = 0; i < 6; i++) {
    int comma = pkt.indexOf(',', start);
    if (comma < 0) comma = pkt.length();
    f[i] = pkt.substring(start, comma);
    start = comma + 1;
  }

  Serial.println();
  Serial.println("==========================================");
  Serial.print("[GATEWAY] PACKET RECEIVED #"); Serial.println(packetCount);
  Serial.print("  Device : "); Serial.println(f[0]);
  Serial.print("  Event  : "); Serial.println(f[1]);
  if (f[2] == "1") {
    Serial.print("  Location: "); Serial.print(f[3]); Serial.print(", "); Serial.println(f[4]);
    Serial.print("  Map    : https://maps.google.com/?q="); Serial.print(f[3]);
    Serial.print(","); Serial.println(f[4]);
  } else {
    Serial.println("  Location: no GPS fix in packet");
  }
  Serial.print("  Uptime : "); Serial.print(f[5]); Serial.println(" s");
  Serial.println("==========================================");

  showAlert(f[1], f[2], f[3], f[4], f[5]);
}

void showIdle() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("GATEWAY READY");
  display.println();
  display.println("Waiting for tracker...");
  display.display();
}

void showAlert(String ev, String fix, String lat, String lng, String up) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(ev == "FALL" ? "FALL DETECTED!" : "SOS RECEIVED!");
  display.setCursor(0, 14);
  if (fix == "1") {
    display.print("Lat: "); display.println(lat);
    display.print("Lng: "); display.println(lng);
  } else {
    display.println("No GPS fix");
  }
  display.print("Pkt #"); display.print(packetCount);
  display.print("  t="); display.print(up); display.print("s");
  display.display();
}
