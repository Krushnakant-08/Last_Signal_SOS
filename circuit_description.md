# Trekker Mesh: Circuit Description

Transmitter (trekker node), receiver (base camp gateway) and a Tinkercad simulation of the whole project.

**Project:** Edge AI-Driven Adaptive LoRa Mesh Tracking and Automated Emergency Alert System for Off-Grid Trekkers
**Source:** your "Circuit Wiring and Pinout Diagram" text file. It covers the trekker node only.

> **Receiver wiring is not in your file.** Section 3 derives it from the same pins and the project's workflow (a second ESP32 + Ra-02, USB powered, Wi-Fi to MQTT). Treat it as a proposal and change it freely.

---

## 1. System at a glance

```
 TREKKER NODE A ))) LoRa 433 MHz ))) TREKKER NODE B ))) LoRa ))) BASE CAMP GATEWAY --Wi-Fi--> MQTT --> web dashboard
 (origin)                            (origin + relay)              (receiver)
```

Every trekker node is identical hardware. A node sends its own packets and forwards packets from other nodes that it hears. The gateway has no IMU or GPS. It receives, publishes to MQTT and raises a local alarm.

---

## 2. Transmitter: trekker node

### 2.1 Block diagram

```
                 USB-C 5 V
                     |
 18650 --- TP4056 (charge + protect) --- power switch --- 3.3 V supply path (see issue 1)
 3.7 V                                                        |
                                                              v
   MPU6050 --I2C (21,22)------+                    +-------------------+
   NEO-6M  --UART2 (16,17)----+------------------->|      ESP32        |---SPI (18,19,23,5)--> Ra-02 SX1278 --> antenna
   SOS button  (27) ----------+                    |  TinyML + logic   |---RST 14, DIO0 26 --->
                                                   +-------------------+
                                                     |      |      |
                                                  buzzer   LEDs   (recommended) battery sense
                                                  GPIO25   32/33/2   GPIO35
```

### 2.2 Parts

| Qty | Part | Role |
|---|---|---|
| 1 | ESP32 dev board (30 or 38 pin, WROOM-32) | Compute, TinyML inference, payload creation |
| 1 | MPU6050 on GY-521 breakout | 6-axis IMU for activity and fall detection |
| 1 | NEO-6M GPS module with antenna | Position |
| 1 | SX1278 Ra-02 (433 MHz) on a breakout, plus antenna | LoRa radio |
| 1 | 18650 Li-ion cell and holder | Battery |
| 1 | TP4056 charger module (version with protection chip) | Charge and cell protection |
| 1 | Push button | Manual SOS |
| 1 | Active buzzer | Local alarm |
| 3 | LEDs (red, green, blue) with 220 to 330 Ω resistors | Status |
| Recommended | 2 × 100 kΩ, 1 × 100 nF | Battery voltage sensing |
| Recommended | 100 µF + 100 nF | Decoupling at the Ra-02 supply pin |
| Recommended | Boost module to 5 V, or a 3.3 V LDO, plus a slide switch | Power path (see issue 1) |

### 2.3 Power

| From | To |
|---|---|
| Battery (+) | TP4056 B+ |
| Battery (−) | TP4056 B− |
| TP4056 OUT+ | ESP32 VIN (5 V pin) or a 3.3 V regulator (your file allows either) |
| TP4056 OUT− | ESP32 GND |

All 3.3 V loads (MPU6050, NEO-6M, Ra-02) take power from the ESP32 3V3 pin. All grounds join at one point. A single-cell Li-ion voltage runs from 3.0 V to 4.2 V, which matters for the VIN option: see issue 1 in section 4.

### 2.4 Pin map

| Module | Module pin | ESP32 pin | Direction and notes |
|---|---|---|---|
| MPU6050 | VCC | 3V3 | Power |
| | GND | GND | |
| | SDA | GPIO21 | I²C data. Address 0x68 when AD0 is low or open |
| | SCL | GPIO22 | I²C clock. The GY-521 has its own pull-ups |
| NEO-6M | VCC | 3V3 | Power |
| | GND | GND | |
| | TX | GPIO16 (RX2) | GPS to ESP32. Serial2 at 9600 baud |
| | RX | GPIO17 (TX2) | ESP32 to GPS. Only needed to configure the module |
| Ra-02 | VCC | 3V3 | 3.3 V only |
| | GND | GND | Connect every GND pad |
| | SCK | GPIO18 | SPI clock |
| | MISO | GPIO19 | |
| | MOSI | GPIO23 | |
| | NSS | GPIO5 | Chip select |
| | RESET | GPIO14 | |
| | DIO0 | GPIO26 | Interrupt for TX done / RX done |
| | ANT | Antenna | Never transmit without it |
| SOS button | Terminal 1 | GPIO27 | `INPUT_PULLUP`, reads LOW when pressed |
| | Terminal 2 | GND | |
| Active buzzer | + | GPIO25 | Digital output |
| | − | GND | |
| Red LED | Anode via resistor | GPIO32 | Emergency / SOS active |
| Green LED | Anode via resistor | GPIO33 | Power / system OK |
| Blue LED | Anode via resistor | GPIO2 | LoRa transmitting or receiving |
| All LEDs | Cathode | GND | |
| Battery sense (recommended) | Divider midpoint | GPIO35 | ADC1, input only |

### 2.5 Netlist

| Net | Connected pins |
|---|---|
| GND | TP4056 OUT−, ESP32 GND, MPU6050 GND, NEO-6M GND, Ra-02 GND, button terminal 2, buzzer −, three LED cathodes, divider bottom resistor |
| 3V3 | ESP32 3V3, MPU6050 VCC, NEO-6M VCC, Ra-02 VCC, 100 µF and 100 nF to GND |
| VIN | TP4056 OUT+ (through the power path chosen in issue 1) to ESP32 VIN |
| I2C_SDA | ESP32 GPIO21, MPU6050 SDA |
| I2C_SCL | ESP32 GPIO22, MPU6050 SCL |
| GPS_TX | NEO-6M TX, ESP32 GPIO16 |
| GPS_RX | ESP32 GPIO17, NEO-6M RX |
| LORA_SCK / MISO / MOSI / NSS | GPIO18 / 19 / 23 / 5 to the same-named Ra-02 pins |
| LORA_RST | GPIO14, Ra-02 RESET |
| LORA_DIO0 | GPIO26, Ra-02 DIO0 |
| SOS_N | GPIO27, button terminal 1 |
| BUZ | GPIO25, buzzer + |
| LED_R / LED_G / LED_B | GPIO32 / 33 / 2, each through a resistor to its LED anode |
| BATSENSE | VBAT, 100 kΩ, node, 100 kΩ, GND. Node goes to GPIO35 and a 100 nF capacitor to GND |

### 2.6 How each part works

**IMU (MPU6050).** The ESP32 reads acceleration at about 50 Hz into a 2 s window. TinyML classifies the window as walking, resting or abnormal. A free-fall followed by a spike of more than about 2.6 g marks an impact. The interrupt pin is not wired in your file and is not needed if the ESP32 polls.

**GPS (NEO-6M).** The module streams NMEA sentences at 9600 baud. A parser (for example TinyGPSPlus) extracts latitude and longitude. It needs a clear view of the sky and can take a minute or more for a cold start. Indoors it will not get a fix.

**LoRa (Ra-02).** The ESP32 talks SPI to the radio. The packet carries node ID, GPS, battery, activity state and an SOS flag. Add a sequence number, a hop count and a TTL so relays can drop duplicates and stop loops. A typical starting configuration is 433 MHz, 125 kHz bandwidth and spreading factor 7 to 9, with the same sync word on every node.

**SOS button.** Pressed pulls GPIO27 to ground. The firmware sends an immediate priority packet and repeats it until the user clears the alert.

**Buzzer and LEDs.** Red shows an emergency, green shows the system is alive, blue pulses on every radio packet. The buzzer sounds during a fall or inactivity countdown and during an SOS.

**Battery sense (not in your file).** The packet includes battery status, so the ESP32 needs a way to measure the cell. Two equal resistors halve the cell voltage (4.2 V reads as 2.1 V). Read it on an ADC1 pin: GPIO32 to 39 are ADC1, but GPIO32 and 33 are used for LEDs, so GPIO35 is the natural choice.

### 2.7 Firmware settings that match the wiring

```cpp
// Pin definitions
#define PIN_SDA 21
#define PIN_SCL 22
#define GPS_RX_PIN 16   // ESP32 RX2 <- NEO-6M TX
#define GPS_TX_PIN 17   // ESP32 TX2 -> NEO-6M RX
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_NSS 5
#define LORA_RST 14
#define LORA_DIO0 26
#define PIN_SOS 27
#define PIN_BUZZER 25
#define PIN_LED_R 32
#define PIN_LED_G 33
#define PIN_LED_B 2
#define PIN_VBAT 35

// Setup calls
Wire.begin(PIN_SDA, PIN_SCL);
Serial2.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);   // "LoRa" library by Sandeep Mistry
LoRa.begin(433E6);
pinMode(PIN_SOS, INPUT_PULLUP);
```

### 2.8 Peak current (approximate, from typical datasheet values)

| Load | Peak |
|---|---|
| ESP32 running, Wi-Fi off | about 80 mA |
| Ra-02 transmitting at +20 dBm | about 120 mA |
| NEO-6M tracking | about 45 mA |
| MPU6050 | about 4 mA |
| Active buzzer | about 30 mA |
| LEDs | about 10 mA |
| Everything at once | about 290 mA |

Measure the real prototype before you choose the regulator and quote battery life.

### 2.9 Bring-up order

1. Check continuity between all GND points before connecting power.
2. Power from USB only. Confirm 3.3 V at the ESP32 3V3 pin and at every module's VCC.
3. Run an I²C scanner. Expect 0x68.
4. Print raw NMEA from Serial2. Expect text even without a fix.
5. Run a LoRa sender on one node and a receiver on another with antennas attached.
6. Add the button, buzzer and LEDs.
7. Only then connect the battery and test the power path.

---

## 3. Receiver: base camp gateway

### 3.1 What is derived

This section reuses the trekker pins, so one LoRa driver works on both boards. The MPU6050, NEO-6M and battery sense are left out.

### 3.2 Block diagram

```
 USB 5 V supply (1 A or more)
        |
        v
 +--------------+  SPI (18,19,23,5), RST 14, DIO0 26   +----------------+
 |    ESP32     |<------------------------------------>| Ra-02 SX1278   |--> antenna (high, clear view)
 | LoRa RX +    |                                      +----------------+
 | Wi-Fi + MQTT |---> ACK button (27), buzzer (25), LEDs (32, 33, 2)
 +--------------+
        | Wi-Fi 2.4 GHz
        v
   MQTT broker --> cloud database --> web dashboard
```

### 3.3 Pin map

| Module | Module pin | ESP32 pin | Notes |
|---|---|---|---|
| Ra-02 | VCC / GND | 3V3 / GND | 3.3 V only. Add 100 µF + 100 nF |
| | SCK / MISO / MOSI | GPIO18 / 19 / 23 | |
| | NSS | GPIO5 | |
| | RESET | GPIO14 | |
| | DIO0 | GPIO26 | RX done interrupt |
| Acknowledge button | Terminal 1 / 2 | GPIO27 / GND | `INPUT_PULLUP`. Silences the alarm |
| Active buzzer | + / − | GPIO25 / GND | Sounds on SOS |
| Red LED | via resistor | GPIO32 | SOS or fall received (latched) |
| Green LED | via resistor | GPIO33 | Wi-Fi and MQTT connected |
| Blue LED | via resistor | GPIO2 | LoRa packet received |
| Power | USB 5 V | USB port | The board's regulator makes 3.3 V |
| Optional OLED (SSD1306) | SDA / SCL | GPIO21 / 22 | Last node, hops, signal strength |

### 3.4 Netlist

| Net | Connected pins |
|---|---|
| GND | USB GND, ESP32 GND, Ra-02 GND, button terminal 2, buzzer −, LED cathodes, OLED GND |
| 3V3 | ESP32 3V3, Ra-02 VCC (with 100 µF and 100 nF to GND), OLED VCC |
| LORA_* | Same as the trekker node (section 2.5) |
| ACK_N | GPIO27, button terminal 1 |
| BUZ | GPIO25, buzzer + |
| LED_R / LED_G / LED_B | GPIO32 / 33 / 2 through resistors |

### 3.5 Behaviour

1. The ESP32 sits in LoRa receive mode. DIO0 fires when a packet arrives.
2. It parses the payload and adds receive signal strength and the hop count.
3. It publishes to MQTT: `trek/base/<node>/telemetry` normally, `trek/base/<node>/alert` when the SOS or fall flag is set.
4. On an alert it latches the red LED and buzzer until the ACK button is pressed.
5. The dashboard subscribes to both topics and updates the live map.

Wi-Fi bursts draw about 250 mA on top of the radio, so the 5 V supply should be rated for at least 1 A. Mount the antenna high, with a clear view down the trail.

---

## 4. Issues found in the given wiring

| # | Severity | Issue | Recommended fix |
|---|---|---|---|
| 1 | High | **TP4056 OUT+ to ESP32 VIN.** A cell is 3.0 to 4.2 V. The dev board's onboard regulator (typically AMS1117) needs roughly 4.5 V or more to hold 3.3 V, so the 3.3 V rail sags below spec as the cell drains and will brown out during LoRa transmit bursts. | Use a boost module set to 5.0 V into VIN. Or feed a 3.3 V LDO directly into the 3V3 pin, with a switch that is open whenever USB is plugged in. |
| 2 | High | **No battery-voltage path.** The payload includes battery status but nothing measures the cell. | Add the 100 kΩ / 100 kΩ divider to GPIO35 (section 2.5). |
| 3 | Medium | **Buzzer straight from a GPIO.** Active buzzers often draw 20 to 30 mA, above what an ESP32 pin should supply continuously. | Drive it with an NPN transistor (for example BC547) and a 1 kΩ base resistor. |
| 4 | Medium | **TP4056 module variant.** Boards without the protection chip leave the cell unprotected. The module also does not share load and charge. | Buy the version with DW01 and FS8205A protection. Do not rely on powering the load while charging. |
| 5 | Medium | **Blue and green LEDs at 3.3 V.** Blue LEDs drop about 3 V, so 330 Ω leaves under 1 mA and they look dim. | Use about 100 Ω for blue and bright-green LEDs, or check the forward voltage. |
| 6 | Medium | **Ra-02 supply.** Transmit bursts dip the 3.3 V rail. | Place 100 µF and 100 nF at the module's VCC pin. Keep the antenna attached. |
| 7 | Low | **GPIO2 is a boot strapping pin.** It must not be pulled high at reset, and many dev boards already have an onboard LED there. | An LED to ground with a series resistor is fine. Do not add a pull-up. |
| 8 | Low | **GPIO16 and 17 are used by PSRAM on WROVER modules.** | Use a WROOM-32 board, or move the GPS UART. |
| 9 | Info | **GPIO5 and GPIO14 toggle at boot.** | Harmless here. Avoid heavy loads on them. |
| 10 | Info | **Receiver wiring is missing.** | Section 3 proposes one. |

The earlier circuit page I drew for you used different pins for the SOS button (GPIO13) and added an IMU interrupt and a transistor-driven buzzer. It does not match your file. I can redraw it to match your pinout plus the fixes above.

---

## 5. Simulation in Tinkercad

### 5.1 What Tinkercad can and cannot do

- **No ESP32.** A third-party guide states that Tinkercad Circuits cannot add an ESP32 board. I did not find an official statement either way.
- **LoRa, GPS, MPU6050.** I could not confirm that Tinkercad has an SX1278, a NEO-6M or an MPU6050. Plan on none of them.
- **What it does have:** the Arduino Uno, potentiometers, push buttons, LEDs, a piezo buzzer and multi-board circuits.

So the simulation replaces each real part with something Tinkercad does have. The behaviour (classification, adaptive reporting, fall and inactivity alerts, relay, gateway alarm) is the same logic as the real firmware.

### 5.2 What stands in for what

| Real project | Tinkercad stand-in |
|---|---|
| ESP32 | Arduino Uno |
| MPU6050 acceleration | Pot on A0 (0 to 4 g, about 1 g at rest) |
| Walking motion | WALK button injects a 1.9 Hz step signal on top of the pot |
| Fall | FALL button injects 0.25 s free fall, then a 3.4 g spike |
| NEO-6M GPS | Coordinates in code that drift while walking |
| LoRa Ra-02 link | SoftwareSerial at 9600 baud, one wire per hop |
| 18650 battery | Pot on A1 (0 to 100 %) |
| TinyML | Rule-based classifier on a 2 s window (spread and peak) |
| MQTT and dashboard | Serial Monitor on the gateway Uno, one JSON line per packet |
| SOS button, buzzer, LEDs | Same parts on Uno pins |

### 5.3 Circuit to build: three Unos

Parts: 3 × Arduino Uno R3, 2 × potentiometer per trekker, 3 push buttons per trekker, 1 push button for the gateway, 3 × piezo, 9 × LED (3 red, 3 green, 3 blue), 9 × 220 Ω resistor.

```
 [Trekker A, NODE_ID 1]  D4 ------> D3  [Trekker B, NODE_ID 2]  D4 ------> D3  [Gateway]
                          (radio hop 1)                          (radio hop 2)
 Join GND of all three boards.
```

**Each trekker Uno**

| Part | Connection |
|---|---|
| Acceleration pot | Outer legs to 5V and GND, wiper to A0 |
| Battery pot | Outer legs to 5V and GND, wiper to A1 |
| SOS button | One side to D2, opposite side to GND |
| WALK button | One side to D5, opposite side to GND |
| FALL button | One side to D6, opposite side to GND |
| Piezo | + to D8, − to GND |
| Red LED | D9, 220 Ω, LED, GND |
| Green LED | D10, 220 Ω, LED, GND |
| Blue LED | D11, 220 Ω, LED, GND |
| Radio out | D4 to the next board's D3 |

**Gateway Uno**

| Part | Connection |
|---|---|
| ACK button | One side to D2, opposite side to GND |
| Piezo | + to D8, − to GND |
| Red, green, blue LEDs | D9, D10, D11 through 220 Ω to GND |
| Radio in | D3 from Trekker B's D4. D4 is unused |

Tinkercad push buttons have four legs. Wire across diagonally opposite legs.

### 5.4 Loading the sketches

1. Click a board, open **Code**, switch the editor to **Text**, delete the sample and paste the sketch.
2. Trekker A: paste `trekker_node.ino` with `#define NODE_ID 1`. Trekker B: the same file with `#define NODE_ID 2`. Gateway: `gateway.ino`.
3. Start the simulation. Select the gateway board to read its Serial Monitor, and a trekker board to see that node's decisions.

LED meaning matches your hardware: red is emergency, green is alive, blue pulses on every radio packet.

### 5.5 Compressed timing

Real timings would make a demo tedious, so the simulation runs about 6 times faster.

| Behaviour | Simulation | Real design |
|---|---|---|
| Report while walking | 5 s | 30 s |
| Report while resting | 20 s | 120 s |
| Report on abnormal motion | 2 s | 10 s |
| Report while a fall is suspected | 1 s | 6 s |
| Report during SOS | 2 s | 10 s |
| Fall confirmation window | 8 s | 30 s |
| Inactivity before the buzzer | 30 s | 180 s |

Changing the constants at the top of `trekker_node.ino` restores the real values.

### 5.6 Packet format

```
$TRK,<node>,<seq>,<hops>,<lat>,<lon>,<battery %>,<activity>,<level>*
```

Activity is 0 resting, 1 walking, 2 abnormal. Level is 0 normal, 1 fall suspected, 2 SOS. The origin sends hops = 1 and each relay adds one. The gateway turns a packet into the same kind of message the real MQTT feed carries:

```
trek/base/t1/telemetry {"seq":1,"hops":2,"lat":19.60373,"lon":73.70062,"bat":78,"act":"WALKING","level":0}
trek/base/t2/alert {"seq":5,"hops":1,...,"level":1}
```

### 5.7 Test script

| Step | Do this | Expect |
|---|---|---|
| 1 | Start the simulation | First report from each node after 2 s. Nodes start in the resting state, so press WALK within about 30 s or the inactivity alert (step 4) starts on its own |
| 2 | Hold WALK on Trekker A | `class -> WALKING`, reports every 5 s. Gateway shows `hops:2` for T1 (A, then B's relay, then the gateway) and `hops:1` for T2 |
| 3 | Release WALK and wait | `class -> RESTING`, reports slow to 20 s |
| 4 | Keep resting for 30 s | `no motion: buzzer on, SOS in 8 s`. Red LED blinks and the buzzer beeps |
| 5 | Hold WALK within 8 s | `movement: alert cancelled` |
| 6 | Press FALL, wait about 3 s, hold WALK | `FALL? impact 3.4 g`, level 1 reports every 1 s, then `false alarm cancelled`. The walk needs about 3 s because the 2 s window must clear the spike |
| 7 | Press FALL and do nothing | After 8 s `no response: SOS confirmed`. Level 2 packets every 2 s. The gateway prints `*** SOS from T1` with coordinates, the red LED blinks and its buzzer sounds |
| 8 | Press ACK on the gateway | Alarm silenced for 30 s |
| 9 | Hold SOS on the trekker for 3 s | `user OK: alert cleared`, back to level 0 |
| 10 | Tap SOS on the trekker | Immediate level 2 priority packet |
| 11 | Delete the wire from Trekker B D4 to gateway D3 | Packets stop reaching the gateway. This is the "no route" case |
| 12 | Move Trekker A's D4 wire directly to the gateway's D3 | Gateway shows `hops:1` for T1, showing a shorter path |

### 5.8 What I verified and what I did not

- I compiled both sketches on this machine against a stand-in Arduino library and ran the scenarios in steps 1 to 10 for a single node and the gateway. The reporting intervals, class changes, fall confirmation, false-alarm cancel, hold-to-clear, relay and duplicate drop all behaved as listed.
- I did **not** run them inside Tinkercad itself, and I did not run the three-board chain end to end. If SoftwareSerial misbehaves in Tinkercad, tell me what you see.
- The relay and gateway format were checked separately with the same packet layout.

### 5.9 Limits of the simulation

- The link is a wire. There is no range, line of sight, airtime, collision or signal-strength modelling, and nodes get no acknowledgements.
- Software serial can drop bytes while a node transmits, which is acceptable here.
- The classifier is a rule-based stand-in for TinyML. The real model needs training data from IMU recordings.
- GPS is simulated. A real NEO-6M needs open sky.
- Wi-Fi and MQTT are represented by Serial Monitor output only.

### 5.10 Option for the real ESP32 firmware: Wokwi

Wokwi's supported-hardware page lists the ESP32 and the MPU6050 as supported, along with buttons, LEDs and a buzzer. It does not list the NEO-6M or an SX1278 LoRa module. Wokwi would suit testing the real ESP32 firmware for the IMU, classifier, buttons and alerts, with the GPS and LoRa link stubbed. If you want that, I can build a Wokwi project for the trekker node.

---

## Files delivered with this document

- `trekker_node.ino`: trekker and relay sketch for Tinkercad (set `NODE_ID`).
- `gateway.ino`: gateway sketch for Tinkercad.

## Sources

- [Wokwi supported hardware](https://docs.wokwi.com/getting-started/supported-hardware)
- [How to add ESP32 in Tinkercad (third-party guide)](https://macmyths.com/how-to-add-esp32-in-tinkercad/)
