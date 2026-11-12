#include "app.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"

static nvs_handle_t s_nvs;

uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

esp_err_t cfg_init(void)
{
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    if (e != ESP_OK) return e;
    return nvs_open("dbn", NVS_READWRITE, &s_nvs);
}

uint8_t cfg_u8(const char *key, uint8_t def)
{
    uint8_t v;
    return nvs_get_u8(s_nvs, key, &v) == ESP_OK ? v : def;
}

void cfg_set_u8(const char *key, uint8_t v) { nvs_set_u8(s_nvs, key, v); nvs_commit(s_nvs); }

/* heartbeat flag: cached (read from NVS on first use), written through on change */
static volatile int s_hb = -1;
bool hb_enabled(void)
{
    if (s_hb < 0) s_hb = cfg_u8("hb", 1) ? 1 : 0;
    return s_hb != 0;
}
void hb_set(bool on)
{
    s_hb = on ? 1 : 0;
    cfg_set_u8("hb", on ? 1 : 0);
}

bool cfg_blob_get(const char *key, void *buf, size_t want_len)
{
    size_t len = 0;
    if (nvs_get_blob(s_nvs, key, NULL, &len) != ESP_OK || len != want_len) return false;
    return nvs_get_blob(s_nvs, key, buf, &len) == ESP_OK;
}

bool cfg_blob_get_var(const char *key, void *buf, size_t max_len, size_t *len)
{
    size_t n = 0;
    if (nvs_get_blob(s_nvs, key, NULL, &n) != ESP_OK || n == 0 || n > max_len) return false;
    if (nvs_get_blob(s_nvs, key, buf, &n) != ESP_OK) return false;
    *len = n;
    return true;
}

bool cfg_has(const char *key)
{
    nvs_type_t t;
    return nvs_find_key(s_nvs, key, &t) == ESP_OK;
}

esp_err_t cfg_blob_set_checked(const char *key, const void *buf, size_t len)
{
    esp_err_t e = nvs_set_blob(s_nvs, key, buf, len);
    return e == ESP_OK ? nvs_commit(s_nvs) : e;
}

void cfg_blob_set(const char *key, const void *buf, size_t len) { nvs_set_blob(s_nvs, key, buf, len); nvs_commit(s_nvs); }
void cfg_erase(const char *key) { nvs_erase_key(s_nvs, key); nvs_commit(s_nvs); }
