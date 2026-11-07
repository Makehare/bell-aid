#include "dbn_common.h"
#include <string.h>
#include <stdio.h>

static uint8_t crc8(const uint8_t *d, size_t n)
{
    uint8_t c = 0;
    while (n--) {
        c ^= *d++;
        for (int i = 0; i < 8; i++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
    }
    return c;
}

void dbn_pkt_build(dbn_pkt_t *p, dbn_type_t type, uint16_t seq, uint8_t copy, uint8_t hop,
                   const uint8_t sender[6], const uint8_t key[8])
{
    memset(p, 0, sizeof(*p));
    p->magic[0] = DBN_MAGIC0; p->magic[1] = DBN_MAGIC1;
    p->ver = DBN_VER; p->type = type; p->seq = seq; p->copy = copy; p->hop = hop;
    memcpy(p->sender, sender, 6);
    memcpy(p->key, key, 8);
    p->crc = crc8((const uint8_t *)p, sizeof(*p) - 1);
}

bool dbn_pkt_parse(const void *data, size_t len, dbn_pkt_t *out)
{
    if (len != sizeof(dbn_pkt_t)) return false;
    memcpy(out, data, sizeof(*out));
    if (out->magic[0] != DBN_MAGIC0 || out->magic[1] != DBN_MAGIC1 || out->ver != DBN_VER) return false;
    if (out->crc != crc8((const uint8_t *)out, sizeof(*out) - 1)) return false;
    if (out->type < DBN_RING || out->type > DBN_PAIR_BEACON) return false;
    return true;
}

bool dbn_dedupe_seen(dbn_dedupe_t *d, uint16_t seq)
{
    if (d->valid && d->seq == seq) return true;
    d->valid = true;
    d->seq = seq;
    return false;
}

float dbn_led_current_ma(const uint8_t *buf, int npix)
{
    uint32_t sum = 0;
    for (int i = 0; i < npix * 3; i++) sum += buf[i];
    return (float)sum * DBN_LED_MA_PER_CH_FULL / 255.0f;
}

float dbn_led_limit(uint8_t *buf, int npix, float cap_ma)
{
    float ma = dbn_led_current_ma(buf, npix);
    if (ma <= cap_ma) return 1.0f;
    float s = cap_ma / ma;
    for (int i = 0; i < npix * 3; i++) buf[i] = (uint8_t)((float)buf[i] * s);   /* truncation only lowers */
    return s;
}

uint32_t dbn_crc32(const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

size_t dbn_rec_line(uint32_t seq, const void *pcm, size_t n, char *out)
{
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const uint8_t *in = (const uint8_t *)pcm;
    size_t o = (size_t)snprintf(out, 32, "PCM %lu %08lx ", (unsigned long)seq, (unsigned long)dbn_crc32(pcm, n));
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0u) | (i + 2 < n ? (uint32_t)in[i + 2] : 0u);
        out[o++] = T[v >> 18 & 63];
        out[o++] = T[v >> 12 & 63];
        out[o++] = i + 1 < n ? T[v >> 6 & 63] : '=';
        out[o++] = i + 2 < n ? T[v & 63] : '=';
    }
    out[o++] = '\n';
    return o;
}

void dbn_alert_pixel(uint32_t t_ms, int i, int npix, uint8_t rgb[3])
{
    uint32_t ph = t_ms % 800;
    rgb[0] = rgb[1] = rgb[2] = 0;
    if (ph < 300) { rgb[0] = rgb[1] = rgb[2] = 255; return; }
    if (ph < 400) return;
    int head = (int)((ph - 400) * (uint32_t)npix / 400);
    int d = ((i - head) % npix + npix) % npix;
    if (d < 6) { rgb[0] = 255; rgb[1] = 140; rgb[2] = 0; }
}
