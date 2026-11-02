#include "app.h"
#include "board.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

static role_t resolve_role(void)
{
    uint8_t ov = cfg_u8("role", 0);
    if (ov == ROLE_SENDER || ov == ROLE_RECEIVER) return (role_t)ov;
    gpio_config_t c = { .pin_bit_mask = 1ULL << PIN_ROLE, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&c);
    vTaskDelay(pdMS_TO_TICKS(5));
    if (gpio_get_level(PIN_ROLE) == 0) return ROLE_SENDER;                     /* LOW = sender (R8 fitted) */
    ESP_LOGW(TAG, "no NVS role override, GPIO10 HIGH (R8 DNP on prototype) -> RECEIVER; "
                  "for a sender run `role sender` or hold the button 5 s at power-up");
    return ROLE_RECEIVER;
}

/* Hold the user button while powering up for 5 s to flip the NVS role override (universal prototype boards). */
static void boot_gesture(role_t current)
{
    if (!button_down()) return;
    uint32_t t0 = now_ms();
    leds_clear(); leds_set(0, 40, 30, 0); leds_show();
    while (button_down()) {
        if (now_ms() - t0 >= 5000) {
            role_t next = current == ROLE_SENDER ? ROLE_RECEIVER : ROLE_SENDER;
            cfg_set_u8("role", (uint8_t)next);
            ESP_LOGW(TAG, "role override -> %s", next == ROLE_SENDER ? "sender" : "receiver");
            for (int i = 0; i < 3; i++) {
                leds_clear(); leds_set(0, 0, 60, 0); leds_show(); vTaskDelay(pdMS_TO_TICKS(150));
                leds_clear(); leds_show(); vTaskDelay(pdMS_TO_TICKS(150));
            }
            esp_restart();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    leds_clear(); leds_show();
}

void app_main(void)
{
    ESP_ERROR_CHECK(cfg_init());
    leds_init();
    button_init();
    role_t role = resolve_role();
    boot_gesture(role);
    ESP_LOGI(TAG, "doorbell firmware, role=%s", role == ROLE_SENDER ? "SENDER" : "RECEIVER");
    ESP_ERROR_CHECK(radio_init(cfg_u8("lr", 0) != 0, role == ROLE_RECEIVER));
    console_start(role);
    if (role == ROLE_SENDER) sender_run(); else receiver_run();
}
