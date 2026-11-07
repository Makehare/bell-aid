#include "app.h"
#include "board.h"
#include "detector.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/message_buffer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "sender";

enum { CMD_NONE = 0, CMD_TRAIN, CMD_FINISH, CMD_ABORT, CMD_CLEAR };

static det_t s_det;
static volatile int s_cmd, s_train_result, s_rounds, s_phase, s_monitor, s_train_rec;
static volatile uint32_t s_detect_count, s_frames;
static volatile float s_score, s_db;
static volatile int s_tx_type = DBN_RING;
static TaskHandle_t s_tx_task;
static uint8_t s_mac[6], s_key[8];
static uint16_t s_seq;
static volatile int s_thr_req;                 /* percent, 0 = none */
static i2s_chan_handle_t s_rx;

/* ---- capture (`record` console command) ---- */
typedef struct { uint32_t seq; int16_t pcm[DET_FRAME]; } rec_frame_t;
#define REC_BUF_FRAMES 16                      /* 0.5 s of slack for the USB host */
static MessageBufferHandle_t s_rec_mb;
static volatile int32_t s_rec_left;            /* hops still to capture */
static volatile uint32_t s_rec_drop, s_rec_seq;

static void load_key(void)
{
    if (!cfg_blob_get("key", s_key, sizeof(s_key))) {
        esp_fill_random(s_key, sizeof(s_key));
        cfg_blob_set("key", s_key, sizeof(s_key));
        ESP_LOGI(TAG, "generated new network key");
    }
}

static void i2s_start(void)
{
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    cc.dma_desc_num = 8;
    cc.dma_frame_num = DET_FRAME;           /* 8 x 32 ms of buffering rides out NVS flash writes */
    ESP_ERROR_CHECK(i2s_new_channel(&cc, NULL, &s_rx));
    i2s_std_config_t sc = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(DET_FS),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = { .mclk = I2S_GPIO_UNUSED, .bclk = PIN_MIC_SCK, .ws = PIN_MIC_WS, .dout = I2S_GPIO_UNUSED, .din = PIN_MIC_SD },
    };
    sc.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;      /* mic L/R pin tied to GND = left slot */
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &sc));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
}

/* Stored template: key "tmpl2" (v2 blob with magic + version). Templates of the v1 detector
 * ("tmpl", its threshold "thr" and length "tlen") cannot be converted: they are erased and the
 * unit shows "untrained" (orange blip) until the doorbell is trained again. */
static void load_template(void)
{
    static uint8_t blob[sizeof(det_template_t)];
    size_t n = 0;
    if (cfg_has("tmpl") || cfg_has("tlen")) {
        cfg_erase("tmpl"); cfg_erase("tlen"); cfg_erase("thr");
        ESP_LOGW(TAG, "learned sound from an older firmware removed: train the doorbell again");
    }
    if (cfg_blob_get_var("tmpl2", blob, sizeof(blob), &n)) {
        if (det_set_template(&s_det, blob, n))
            ESP_LOGI(TAG, "template loaded: %.2f s, %u ring(s), thr %u%%", s_det.tmpl.h.len * (float)DET_FRAME / DET_FS,
                     s_det.tmpl.h.rounds, s_det.tmpl.h.thr_pct);
        else { cfg_erase("tmpl2"); ESP_LOGW(TAG, "stored template has an unknown format: train the doorbell again"); }
    }
    if (!s_det.have_tmpl) ESP_LOGW(TAG, "no template: long-press the button and ring the bell to train");
    uint8_t pct = cfg_u8("thr", 0);                 /* user override from the `thr` command */
    if (pct) det_set_threshold(&s_det, pct / 100.0f);
}

static void audio_task(void *arg)
{
    static int32_t raw[DET_FRAME];
    static int16_t pcm[DET_FRAME];
    static rec_frame_t rf;
    i2s_start();
    for (;;) {
        size_t got = 0;
        if (i2s_channel_read(s_rx, raw, sizeof(raw), &got, 1000) != ESP_OK || got < sizeof(raw)) continue;
        for (int i = 0; i < DET_FRAME; i++) {
            int32_t v = raw[i] >> 14;               /* 24-bit mic data -> ~16-bit with +12 dB gain */
            pcm[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        if (s_rec_left > 0) {                        /* capture: exactly what the detector gets */
            rf.seq = s_rec_seq++;
            memcpy(rf.pcm, pcm, sizeof(pcm));
            if (xMessageBufferSend(s_rec_mb, &rf, sizeof(rf), 0) != sizeof(rf)) s_rec_drop++;
            s_rec_left--;
        }
        switch (s_cmd) {
        case CMD_TRAIN: s_rounds = 0; s_train_result = 0; det_train_begin(&s_det); s_cmd = CMD_NONE; break;
        case CMD_ABORT: det_train_abort(&s_det); s_cmd = CMD_NONE; break;
        case CMD_FINISH:
            if (det_train_finish(&s_det)) {
                esp_err_t e = cfg_blob_set_checked("tmpl2", &s_det.tmpl, det_template_bytes(&s_det));
                cfg_erase("thr");                    /* a new sound starts at the default threshold */
                s_train_result = e == ESP_OK ? 1 : -1;
                ESP_LOGI(TAG, "template %s: %.2f s from %u ring(s), thr %.2f", e == ESP_OK ? "saved" : "NOT saved",
                         s_det.tmpl.h.len * (float)DET_FRAME / DET_FS, s_det.tmpl.h.rounds, s_det.thr);
            } else s_train_result = -1;
            s_cmd = CMD_NONE; break;
        case CMD_CLEAR: s_det.have_tmpl = false; cfg_erase("tmpl2"); cfg_erase("thr"); s_cmd = CMD_NONE; break;
        default: break;
        }
        if (s_thr_req) { det_set_threshold(&s_det, s_thr_req / 100.0f); cfg_set_u8("thr", (uint8_t)s_thr_req); s_thr_req = 0; }

        det_event_t ev = det_process(&s_det, pcm);
        s_frames++;
        s_score = s_det.last_score;
        s_db = s_det.last_db;
        s_train_rec = det_train_recording(&s_det);
        if (ev == DET_MATCH) {
            s_detect_count++;
            s_tx_type = DBN_RING;
            if (s_tx_task) xTaskNotifyGive(s_tx_task);
        } else if (ev == DET_TRAIN_ROUND_DONE) {
            s_rounds = det_train_rounds(&s_det);
        }
        s_phase = ev;
        if (s_monitor && (s_frames % 8) == 0)
            printf("mon: db=%.1f floor=%.1f score=%.3f thr=%.2f\n", s_det.last_db, s_det.floor_db, s_det.last_score, s_det.thr);
    }
}

static void tx_task(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        int copies = cfg_u8("copies", 4);
        if (copies < 3) copies = 3;
        if (copies > 5) copies = 5;
        s_seq++;
        ESP_LOGI(TAG, "%s seq=%u x%d", s_tx_type == DBN_TEST ? "TEST" : "RING", s_seq, copies);
        for (int c = 0; c < copies; c++) {
            dbn_pkt_t p;
            dbn_pkt_build(&p, (dbn_type_t)s_tx_type, s_seq, (uint8_t)c, 0, s_mac, s_key);
            radio_send(&p);
            vTaskDelay(pdMS_TO_TICKS(30 + (esp_random() % 25)));   /* jittered spacing */
        }
    }
}

/* Stream `seconds` of the mic signal (after the 24 -> 16 bit conversion, i.e. exactly what the
 * detector sees) as text lines on the console: "REC-BEGIN ...", one line per 32 ms hop
 * (dbn_rec_line: "PCM <seq> <crc32 hex> <base64 of 512 x int16 LE>"), then
 * "REC-END frames=N dropped=M". tools/record.py turns this into a WAV. Logging is muted
 * meanwhile; detection keeps running. */
void sender_cmd_record(int seconds)
{
    static rec_frame_t rf;
    static char line[DBN_REC_LINE_MAX(sizeof(rf.pcm))];
    if (!s_rec_mb) s_rec_mb = xMessageBufferCreate(REC_BUF_FRAMES * (sizeof(rec_frame_t) + sizeof(size_t)));
    if (!s_rec_mb) { printf("record: out of memory\n"); return; }
    int frames = seconds * DET_FS / DET_FRAME;
    int mon = s_monitor;
    s_monitor = 0;
    esp_log_level_set("*", ESP_LOG_NONE);
    xMessageBufferReset(s_rec_mb);
    s_rec_drop = 0; s_rec_seq = 0;
    printf("REC-BEGIN fs=%d bits=16 frames=%d hop=%d\n", DET_FS, frames, DET_FRAME);
    fflush(stdout);
    s_rec_left = frames;
    int sent = 0;
    while (s_rec_left > 0 || !xMessageBufferIsEmpty(s_rec_mb)) {
        if (xMessageBufferReceive(s_rec_mb, &rf, sizeof(rf), pdMS_TO_TICKS(200)) != sizeof(rf)) continue;
        size_t l = dbn_rec_line(rf.seq, rf.pcm, sizeof(rf.pcm), line);
        fwrite(line, 1, l, stdout);
        fflush(stdout);
        sent++;
    }
    printf("REC-END frames=%d dropped=%lu\n", sent, (unsigned long)s_rec_drop);
    fflush(stdout);
    esp_log_level_set("*", ESP_LOG_INFO);
    s_monitor = mon;
}

void sender_cmd_alert(int on) { cfg_set_u8("salert", on ? 1 : 0); }
void sender_cmd_ring(void) { s_tx_type = DBN_TEST; if (s_tx_task) xTaskNotifyGive(s_tx_task); }
void sender_cmd_monitor(int on) { s_monitor = on; }
void sender_cmd_cleartemplate(void) { s_cmd = CMD_CLEAR; }
void sender_cmd_threshold(int pct) { s_thr_req = pct; }
void sender_status_print(void)
{
    printf("role=sender mac=%02x:%02x:%02x:%02x:%02x:%02x template=", s_mac[0], s_mac[1], s_mac[2], s_mac[3], s_mac[4], s_mac[5]);
    if (s_det.have_tmpl) printf("%.2fs/%u-rings", s_det.tmpl.h.len * (float)DET_FRAME / DET_FS, s_det.tmpl.h.rounds);
    else printf("NO (train it)");
    printf(" thr=%.2f (trained %.2f) copies=%u alert=%s lr=%s detections=%u db=%.1f score=%.3f key=%02x%02x....\n",
           s_det.thr, s_det.tmpl.h.thr_pct / 100.0f, cfg_u8("copies", 4), cfg_u8("salert", 1) ? "on" : "off", radio_lr_active() ? "on" : "off",
           (unsigned)s_detect_count, s_db, s_score, s_key[0], s_key[1]);
}

/* ---- UI ---- */
typedef enum { UI_ARMED, UI_TRAIN, UI_PAIR } ui_t;

static void d1(uint8_t r, uint8_t g, uint8_t b) { leds_clear(); leds_set(0, r, g, b); leds_show(); }

static void blink(uint8_t r, uint8_t g, uint8_t b, int n)
{
    for (int i = 0; i < n; i++) { d1(r, g, b); vTaskDelay(pdMS_TO_TICKS(150)); d1(0, 0, 0); vTaskDelay(pdMS_TO_TICKS(150)); }
}

void sender_run(void)
{
    radio_mac(s_mac);
    load_key();
    s_seq = (uint16_t)esp_random();
    det_init(&s_det);
    load_template();

    xTaskCreate(tx_task, "tx", 3072, NULL, 6, &s_tx_task);
    xTaskCreate(audio_task, "audio", 14336, NULL, 7, NULL);

    ui_t ui = UI_ARMED;
    uint32_t t0 = 0, last_round = 0, pair_until = 0, last_beacon = 0, flash_until = 0;
    uint32_t alert_start = 0, alert_dur = 0;       /* full-ring alert on this unit (same as the receivers) */
    int seen_rounds = 0;
    uint32_t seen_detect = 0;
    for (;;) {
        uint32_t now = now_ms();
        btn_event_t ev = button_poll(now);

        if (s_detect_count != seen_detect) {
            seen_detect = s_detect_count; flash_until = now + 600;
            if (cfg_u8("salert", 1) && ui == UI_ARMED) { alert_start = now; alert_dur = DBN_ALERT_RING_MS; }
        }
        bool alerting = alert_dur && now - alert_start < alert_dur;

        switch (ui) {
        case UI_ARMED:
            if (ev == BTN_SHORT && alerting) alert_dur = 0;            /* dismiss the alert on this unit */
            else if (ev == BTN_SHORT) {
                sender_cmd_ring(); flash_until = now + 600;
                if (cfg_u8("salert", 1)) { alert_start = now; alert_dur = DBN_ALERT_TEST_MS; }
            }
            else if (ev == BTN_LONG) {
                s_rounds = 0; seen_rounds = 0; s_cmd = CMD_TRAIN;
                while (s_cmd) vTaskDelay(pdMS_TO_TICKS(5));
                ui = UI_TRAIN; t0 = now; last_round = 0;
                ESP_LOGI(TAG, "training: ring the doorbell (up to %d times, let each ring finish)", DET_MAX_ROUNDS);
            } else if (ev == BTN_VLONG) { ui = UI_PAIR; pair_until = now + 30000; ESP_LOGI(TAG, "pairing beacons for 30 s"); }
            break;
        case UI_TRAIN: {
            /* A ring is complete 2 s after its sound stops (shorter pauses inside a melody stay part
             * of the ring). Finish: 3rd ring, 8 s without a new ring, or a short press. */
            if (s_rounds != seen_rounds) { seen_rounds = s_rounds; last_round = now; flash_until = now + 600; }
            bool rec = s_train_rec;
            bool fin = seen_rounds >= DET_MAX_ROUNDS || (seen_rounds >= 1 && !rec && now - last_round > 8000) || (seen_rounds >= 1 && ev == BTN_SHORT);
            bool abort_ = seen_rounds == 0 && ((!rec && now - t0 > 25000) || ev == BTN_SHORT);
            if (fin) {
                vTaskDelay(pdMS_TO_TICKS(100));
                s_train_result = 0; s_cmd = CMD_FINISH;
                while (s_cmd) vTaskDelay(pdMS_TO_TICKS(5));
                if (s_train_result == 1) blink(0, 60, 0, 3); else blink(60, 0, 0, 3);
                ui = UI_ARMED;
            } else if (abort_) {
                s_cmd = CMD_ABORT; while (s_cmd) vTaskDelay(pdMS_TO_TICKS(5));
                blink(60, 0, 0, 3); ui = UI_ARMED;
            }
            break; }
        case UI_PAIR:
            if (now - last_beacon > 300) {
                last_beacon = now;
                dbn_pkt_t p; dbn_pkt_build(&p, DBN_PAIR_BEACON, 0, 0, 0, s_mac, s_key);
                radio_send(&p);
            }
            if (now > pair_until || ev == BTN_SHORT) ui = UI_ARMED;
            break;
        }

        alerting = alert_dur && now - alert_start < alert_dur && ui == UI_ARMED;
        if (alerting) {                                  /* full ring; idle status blips resume afterwards */
            leds_clear(); leds_alert(now - alert_start); leds_show();
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* status pixel */
        uint32_t held = button_held_ms(now);
        if (held >= BTN_VLONG_MS) d1(40, 0, 40);
        else if (held >= BTN_LONG_MS) d1(40, 30, 0);
        else if (now < flash_until && ui != UI_TRAIN) d1(60, 60, 60);
        else if (now < flash_until) d1(0, 60, 0);
        else if (ui == UI_TRAIN) { int ph = (now / 8) % 200; int br = ph < 100 ? ph : 200 - ph; if (s_train_rec) d1(0, 40, 40); else d1(0, 0, 4 + br / 4); }
        else if (ui == UI_PAIR) d1((now / 150) % 2 ? 40 : 0, 0, (now / 150) % 2 ? 40 : 0);
        else if (now % 3000 < 80) d1(s_det.have_tmpl ? 0 : 20, s_det.have_tmpl ? 10 : 8, 0);   /* green = armed, orange = untrained */
        else d1(0, 0, 0);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
