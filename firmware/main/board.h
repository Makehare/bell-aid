#pragma once
// Pin map: ARCHITECTURE.md sec. 4.4 (ESP32-S3-MINI-1-N8)
#define PIN_MIC_SCK   4     /* I2S BCLK  (sender) */
#define PIN_MIC_WS    5     /* I2S LRCLK (sender) */
#define PIN_MIC_SD    6     /* ICS-43434 data in (L/R=GND -> left slot, 24-bit in 32-bit frame); 100k pull-down on net */
#define PIN_LED_DIN   7     /* via 74AHCT1G125 to WS2812B chain, GRB */
#define PIN_BTN_USER  9     /* active LOW, external pull-up */
#define PIN_ROLE      10    /* internal pull-up; LOW = sender (R8 ROLE 0R to GND), HIGH = receiver. Prototype: R8 DNP -> all boards HIGH -> senders need NVS override */

#define NUM_LEDS      12    /* chain length on a receiver; sender uses pixel 0 only */
#define LED_CAP_MA    600   /* ring current cap, see dbn_common */

#define ESPNOW_CHANNEL 1
