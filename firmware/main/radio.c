#include "app.h"
#include "board.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "radio";
static const uint8_t BCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static QueueHandle_t s_q;
static bool s_lr;

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (!s_q) return;
    rx_item_t it;
    if (!dbn_pkt_parse(data, (size_t)len, &it.p)) return;
    memcpy(it.src, info->src_addr, 6);
    it.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
    xQueueSend(s_q, &it, 0);
}

esp_err_t radio_init(bool lr, bool want_rx)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    s_lr = false;
    if (lr) {
        /* BGN + LR: can receive both normal and LR frames. */
        esp_err_t e = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
        s_lr = (e == ESP_OK);
        if (!s_lr) ESP_LOGW(TAG, "LR protocol not accepted: %s", esp_err_to_name(e));
    }

    ESP_ERROR_CHECK(esp_now_init());
    if (want_rx) {
        s_q = xQueueCreate(16, sizeof(rx_item_t));
        ESP_ERROR_CHECK(esp_now_register_recv_cb(on_recv));
    }
    esp_now_peer_info_t peer = { .channel = 0, .ifidx = WIFI_IF_STA, .encrypt = false };
    memcpy(peer.peer_addr, BCAST, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    if (s_lr) {
        esp_now_rate_config_t rc = { .phymode = WIFI_PHY_MODE_LR, .rate = WIFI_PHY_RATE_LORA_250K, .ersu = false, .dcm = false };
        esp_err_t e = esp_now_set_peer_rate_config(BCAST, &rc);
        if (e != ESP_OK) { ESP_LOGW(TAG, "LR rate config failed: %s", esp_err_to_name(e)); s_lr = false; }
    }
    uint8_t mac[6]; radio_mac(mac);
    ESP_LOGI(TAG, "ESP-NOW up, ch %d, LR %s, MAC %02x:%02x:%02x:%02x:%02x:%02x", ESPNOW_CHANNEL, s_lr ? "on" : "off",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return ESP_OK;
}

void radio_mac(uint8_t mac[6]) { esp_read_mac(mac, ESP_MAC_WIFI_STA); }
bool radio_lr_active(void) { return s_lr; }
esp_err_t radio_send(const dbn_pkt_t *p) { return esp_now_send(BCAST, (const uint8_t *)p, sizeof(*p)); }

int radio_rx(rx_item_t *it, uint32_t timeout_ms)
{
    if (!s_q) return 0;
    return xQueueReceive(s_q, it, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}
