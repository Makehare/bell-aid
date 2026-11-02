// Wire protocol + LED current limiter. Pure C, host-testable.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DBN_MAGIC0 'D'
#define DBN_MAGIC1 'B'
#define DBN_VER    1

typedef enum {
    DBN_RING = 1,        /* doorbell detected */
    DBN_TEST = 2,        /* test ring (flashes, flagged as test) */
    DBN_PAIR_BEACON = 3, /* sender in pairing mode: carries network key */
} dbn_type_t;

typedef struct __attribute__((packed)) {
    uint8_t  magic[2];
    uint8_t  ver;
    uint8_t  type;
    uint16_t seq;        /* event id: same for all copies of one ring */
    uint8_t  copy;       /* copy index 0..n-1 */
    uint8_t  hop;        /* 0 = from sender, 1 = re-broadcast by a receiver */
    uint8_t  sender[6];  /* MAC of the originating sender (receivers pair to this) */
    uint8_t  key[8];     /* network key, learned during pairing */
    uint8_t  crc;        /* CRC-8 over all previous bytes */
} dbn_pkt_t;

void dbn_pkt_build(dbn_pkt_t *p, dbn_type_t type, uint16_t seq, uint8_t copy, uint8_t hop,
                   const uint8_t sender[6], const uint8_t key[8]);
/* Returns true if length, magic, version and CRC are valid. */
bool dbn_pkt_parse(const void *data, size_t len, dbn_pkt_t *out);

/* Dedupe: returns true if this seq was already seen (and remembers it). */
typedef struct { uint16_t seq; bool valid; } dbn_dedupe_t;
bool dbn_dedupe_seen(dbn_dedupe_t *d, uint16_t seq);

/* LED current limiter. WS2812B: ~20 mA per colour channel at 255. */
#define DBN_LED_MA_PER_CH_FULL 20.0f
#define DBN_LED_CAP_MA         600.0f
float dbn_led_current_ma(const uint8_t *grb_or_rgb, int npix);
/* Scales the buffer in place so the estimated current is <= cap_ma. Returns scale applied (<=1). */
float dbn_led_limit(uint8_t *buf, int npix, float cap_ma);

/* ---- mic capture over the console (sender `record`, decoded by tools/record.py) ----
 * One line per hop: "PCM <seq> <crc32 hex8> <base64 of the raw int16 LE samples>\n". */
uint32_t dbn_crc32(const void *data, size_t n);            /* same value as zlib.crc32 */
#define DBN_REC_LINE_MAX(nbytes) (32 + ((nbytes) + 2) / 3 * 4 + 2)
size_t   dbn_rec_line(uint32_t seq, const void *pcm, size_t nbytes, char *out);   /* returns length, no NUL */
