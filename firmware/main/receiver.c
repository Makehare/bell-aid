#include "app.h"
#include "board.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "receiver";

#define FLASH_RING_MS   DBN_ALERT_RING_MS
#define FLASH_TEST_MS   DBN_ALERT_TEST_MS
#define PAIR_WINDOW_MS  60000

static uint8_t s_mac[6], s_peer[6], s_key[8];
static bool s_paired, s_muted;
static uint32_t s_events;
static int s_last_rssi;
static dbn_dedupe_t s_dd;

static bool load_pairing(void)
{
    return cfg_blob_get("peer", s_peer, 6) && cfg_blob_get("key", s_key, 8);
}

void receiver_status_print(void)
{
    printf("role=receiver mac=%02x:%02x:%02x:%02x:%02x:%02x paired=%s peer=%02x:%02x:%02x:%02x:%02x:%02x muted=%d heartbeat=%s rebroadcast=%u lr=%s events=%u last_rssi=%d\n",
           s_mac[0], s_mac[1], s_mac[2], s_mac[3], s_mac[4], s_mac[5], s_paired ? "yes" : "NO",
           s_peer[0], s_peer[1], s_peer[2], s_peer[3], s_peer[4], s_peer[5], s_muted, hb_enabled() ? "on" : "off", cfg_u8("rebc", 1),
           radio_lr_active() ? "on" : "off", (unsigned)s_events, s_last_rssi);
}

static void status_pixel(uint32_t now, uint32_t pair_until, bool flashing)
{
    if (flashing) return;                                          /* D1 is part of the ring flash */
    uint32_t held = button_held_ms(now);
    if (held >= BTN_VLONG_MS) leds_set(0, 40, 0, 40);
    else if (held >= BTN_LONG_MS) leds_set(0, 40, 30, 0);
    else if (now < pair_until) { bool on = (now / 150) % 2; leds_set(0, 0, 0, on ? 50 : 0); }
    else if (!s_paired) { if (now % 1500 < 120) leds_set(0, 40, 15, 0); }          /* orange blink: unpaired */
    else if (s_muted) leds_set(0, 25, 0, 0);                                       /* steady red: muted */
    else if (hb_enabled() && now % 3000 < 60) leds_set(0, 0, 8, 0);               /* green heartbeat (off with triple press) */
}

void receiver_run(void)
{
    radio_mac(s_mac);
    s_paired = load_pairing();
    uint32_t pair_until = s_paired ? 0 : now_ms() + PAIR_WINDOW_MS;       /* unpaired devices listen for a sender at boot */
    uint32_t flash_start = 0, flash_dur = 0;
    uint32_t rebc_at = 0; dbn_pkt_t rebc_pkt;
    ESP_LOGI(TAG, "%s", s_paired ? "paired" : "not paired: listening for sender beacon (start pairing on the sender: hold button >5 s)");

    for (;;) {
        uint32_t now = now_ms();
        bool flashing = now - flash_start < flash_dur && flash_dur;
        button_set_immediate(flashing);                            /* dismissing a flash must not wait for the multi-click window */
        btn_event_t ev = button_poll(now);

        if (ev == BTN_TRIPLE) {                                    /* heartbeat on/off (NVS) */
            hb_set(!hb_enabled());
            ESP_LOGI(TAG, "heartbeat=%d", hb_enabled());
            leds_clear(); leds_show();
            leds_hb_confirm(hb_enabled());
        } else if (ev == BTN_SHORT) {
            if (flashing) flash_dur = 0;                           /* dismiss */
            else { s_muted = !s_muted; ESP_LOGI(TAG, "muted=%d", s_muted); }
        } else if (ev == BTN_LONG) { flash_start = now; flash_dur = FLASH_TEST_MS; }       /* local test */
        else if (ev == BTN_VLONG) { pair_until = now + PAIR_WINDOW_MS; ESP_LOGI(TAG, "pairing mode for %d s", PAIR_WINDOW_MS / 1000); }

        rx_item_t it;
        while (radio_rx(&it, 10)) {
            const dbn_pkt_t *p = &it.p;
            s_last_rssi = it.rssi;
            if (p->type == DBN_PAIR_BEACON) {
                if (now < pair_until) {
                    memcpy(s_peer, p->sender, 6); memcpy(s_key, p->key, 8);
                    cfg_blob_set("peer", s_peer, 6); cfg_blob_set("key", s_key, 8);
                    s_paired = true; pair_until = 0;
                    ESP_LOGI(TAG, "paired to %02x:%02x:%02x:%02x:%02x:%02x", s_peer[0], s_peer[1], s_peer[2], s_peer[3], s_peer[4], s_peer[5]);
                    for (int i = 0; i < 3; i++) { leds_clear(); leds_set(0, 0, 60, 0); leds_show(); vTaskDelay(pdMS_TO_TICKS(150)); leds_clear(); leds_show(); vTaskDelay(pdMS_TO_TICKS(150)); }
                }
                continue;
            }
            if (!s_paired || memcmp(p->sender, s_peer, 6) || memcmp(p->key, s_key, 8)) continue;   /* not our sender */
            if (dbn_dedupe_seen(&s_dd, p->seq)) continue;
            s_events++;
            ESP_LOGI(TAG, "%s seq=%u hop=%u rssi=%d", p->type == DBN_TEST ? "TEST" : "RING", p->seq, p->hop, it.rssi);
            if (p->type == DBN_TEST) { flash_start = now; flash_dur = FLASH_TEST_MS; }
            else if (!s_muted) { flash_start = now; flash_dur = FLASH_RING_MS; }
            if (p->hop == 0 && cfg_u8("rebc", 1) && !rebc_at) {          /* one re-broadcast per event */
                dbn_pkt_build(&rebc_pkt, (dbn_type_t)p->type, p->seq, p->copy, 1, p->sender, p->key);
                rebc_at = now + 15 + (esp_random() % 45);
                if (!rebc_at) rebc_at = 1;
            }
        }
        if (rebc_at && (int32_t)(now - rebc_at) >= 0) { radio_send(&rebc_pkt); rebc_at = 0; }

        flashing = now - flash_start < flash_dur && flash_dur;
        leds_clear();
        if (flashing) leds_alert(now - flash_start);
        status_pixel(now, pair_until, flashing);
        leds_show();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
