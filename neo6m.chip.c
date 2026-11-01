#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
  uart_dev_t uart;
  timer_t timer;
} chip_state_t;

// Wrap an NMEA body in $...*CS\r\n with a correctly computed checksum
static void send_nmea(chip_state_t *chip, const char *body) {
  uint8_t cs = 0;
  for (const char *p = body; *p; p++) cs ^= (uint8_t)*p;
  char line[128];
  int len = snprintf(line, sizeof(line), "$%s*%02X\r\n", body, cs);
  uart_write(chip->uart, (uint8_t *)line, (uint32_t)len);
}

static void send_gps_data(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  // Fixed position in Pune (18.5204 N, 73.8567 E)
  send_nmea(chip, "GPGGA,123519,1831.224,N,07351.402,E,1,08,0.9,560.0,M,0.0,M,,");
  send_nmea(chip, "GPRMC,123519,A,1831.224,N,07351.402,E,000.0,000.0,311026,,,A");
}

void chip_init() {
  chip_state_t *chip = malloc(sizeof(chip_state_t));

  uart_config_t uart_cfg = {
    .tx = pin_init("TX", OUTPUT),
    .rx = pin_init("RX", INPUT),
    .baud_rate = 9600
  };
  chip->uart = uart_init(&uart_cfg);

  timer_config_t timer_cfg = {
    .callback = send_gps_data,
    .user_data = chip
  };
  chip->timer = timer_init(&timer_cfg);
  timer_start(chip->timer, 1000000, true);  // every 1 s, repeating
}
