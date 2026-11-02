#include "app.h"
#include "board.h"
#include "led_strip.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "leds";
static led_strip_handle_t s_strip;
static uint8_t s_pix[NUM_LEDS * 3];          /* RGB */
static float s_scale = 1.0f;

void leds_init(void)
{
    led_strip_config_t cfg = {
        .strip_gpio_num = PIN_LED_DIN,
        .max_leds = NUM_LEDS,
        .led_model = LED_MODEL_WS2812,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .flags = { .invert_out = false },
    };
    led_strip_rmt_config_t rmt = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags = { .with_dma = false },
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&cfg, &rmt, &s_strip));
    leds_clear();
    leds_show();
    ESP_LOGI(TAG, "WS2812 on GPIO%d, %d px, cap %d mA", PIN_LED_DIN, NUM_LEDS, LED_CAP_MA);
}

void leds_clear(void) { memset(s_pix, 0, sizeof(s_pix)); }

void leds_set(int i, uint8_t r, uint8_t g, uint8_t b)
{
    if (i < 0 || i >= NUM_LEDS) return;
    s_pix[i * 3] = r; s_pix[i * 3 + 1] = g; s_pix[i * 3 + 2] = b;
}

void leds_fill(uint8_t r, uint8_t g, uint8_t b) { for (int i = 0; i < NUM_LEDS; i++) leds_set(i, r, g, b); }

void leds_show(void)
{
    uint8_t out[NUM_LEDS * 3];
    memcpy(out, s_pix, sizeof(out));
    s_scale = dbn_led_limit(out, NUM_LEDS, (float)LED_CAP_MA);
    for (int i = 0; i < NUM_LEDS; i++) led_strip_set_pixel(s_strip, i, out[i * 3], out[i * 3 + 1], out[i * 3 + 2]);
    led_strip_refresh(s_strip);
}

float leds_last_scale(void) { return s_scale; }
