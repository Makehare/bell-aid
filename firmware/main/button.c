#include "app.h"
#include "board.h"
#include "driver/gpio.h"

#define DEBOUNCE_MS 30

static bool     s_raw_prev, s_stable;       /* true = pressed */
static uint32_t s_change_ms, s_press_ms;
static dbn_click_state_t s_clk;
static volatile bool s_immediate;

void button_init(void)
{
    gpio_config_t c = {
        .pin_bit_mask = 1ULL << PIN_BTN_USER,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,       /* external 10k also fitted */
    };
    gpio_config(&c);
}

bool button_down(void) { return gpio_get_level(PIN_BTN_USER) == 0; }

void button_set_immediate(bool on) { s_immediate = on; }

btn_event_t button_poll(uint32_t t)
{
    bool raw = button_down();
    if (raw != s_raw_prev) { s_raw_prev = raw; s_change_ms = t; }
    if (raw != s_stable && (t - s_change_ms) >= DEBOUNCE_MS) {
        s_stable = raw;
        if (s_stable) s_press_ms = t;
    }
    switch (dbn_click_update(&s_clk, s_stable, t, BTN_LONG_MS, BTN_VLONG_MS, s_immediate)) {
    case DBN_CLK_SHORT:  return BTN_SHORT;
    case DBN_CLK_DOUBLE: return BTN_DOUBLE;
    case DBN_CLK_TRIPLE: return BTN_TRIPLE;
    case DBN_CLK_LONG:   return BTN_LONG;
    case DBN_CLK_VLONG:  return BTN_VLONG;
    default:             return BTN_NONE;
    }
}

uint32_t button_held_ms(uint32_t t) { return s_stable ? t - s_press_ms : 0; }
