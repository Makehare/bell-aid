#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "dbn_common.h"

typedef enum { ROLE_SENDER = 1, ROLE_RECEIVER = 2 } role_t;

/* ---- cfg (NVS namespace "dbn") ---- */
esp_err_t cfg_init(void);
uint8_t   cfg_u8(const char *key, uint8_t def);
void      cfg_set_u8(const char *key, uint8_t v);
bool      cfg_blob_get(const char *key, void *buf, size_t want_len);   /* true only if size matches */
bool      cfg_blob_get_var(const char *key, void *buf, size_t max_len, size_t *len);
void      cfg_blob_set(const char *key, const void *buf, size_t len);
esp_err_t cfg_blob_set_checked(const char *key, const void *buf, size_t len);
bool      cfg_has(const char *key);
void      cfg_erase(const char *key);
/* Keys: role(0 auto,1 sender,2 receiver) thr(percent, user override) copies(3..5) rebc(0/1) lr(0/1)
 *       salert(0/1, sender shows the full-ring alert itself, default 1)
 *       key(8B network key) peer(6B paired sender MAC, receiver) tmpl2(detector template v2 blob)
 *       Obsolete (v1 detector, erased on boot): tmpl, tlen. */

/* ---- leds (all drawing is done from the role's UI task) ---- */
void  leds_init(void);
void  leds_clear(void);
void  leds_set(int i, uint8_t r, uint8_t g, uint8_t b);
void  leds_fill(uint8_t r, uint8_t g, uint8_t b);
void  leds_alert(uint32_t t_ms);    /* full-ring alert frame, t_ms since start (same on sender and receiver) */
void  leds_show(void);              /* applies the <= LED_CAP_MA current cap */
float leds_last_scale(void);

/* ---- button ---- */
typedef enum { BTN_NONE = 0, BTN_SHORT, BTN_LONG, BTN_VLONG } btn_event_t;
#define BTN_LONG_MS   1500
#define BTN_VLONG_MS  5000
void        button_init(void);
btn_event_t button_poll(uint32_t now_ms);   /* call every ~10-20 ms; event fires on release */
uint32_t    button_held_ms(uint32_t now_ms);
bool        button_down(void);

/* ---- radio ---- */
typedef struct { dbn_pkt_t p; uint8_t src[6]; int8_t rssi; } rx_item_t;
esp_err_t radio_init(bool lr, bool want_rx);
void      radio_mac(uint8_t mac[6]);
esp_err_t radio_send(const dbn_pkt_t *p);
int       radio_rx(rx_item_t *it, uint32_t timeout_ms);   /* 1 if item received */
bool      radio_lr_active(void);

/* ---- roles ---- */
void sender_run(void);              /* never returns */
void receiver_run(void);            /* never returns */
void console_start(role_t role);
uint32_t now_ms(void);

/* sender hooks for console */
void sender_cmd_ring(void);
void sender_cmd_monitor(int on);
void sender_cmd_cleartemplate(void);
void sender_cmd_threshold(int pct);
void sender_cmd_alert(int on);
void sender_cmd_record(int seconds);  /* streams mic PCM on the console, blocks until done */
void sender_status_print(void);
void receiver_status_print(void);
