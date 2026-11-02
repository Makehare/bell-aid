// Protocol + LED limiter tests.
#include "dbn_common.h"
#include <stdio.h>
#include <string.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)
int main(void)
{
    uint8_t mac[6] = {1, 2, 3, 4, 5, 6}, key[8] = {9, 8, 7, 6, 5, 4, 3, 2};
    dbn_pkt_t p, q;
    dbn_pkt_build(&p, DBN_RING, 513, 2, 0, mac, key);
    CHECK(sizeof(p) == 23, "packet size %zu", sizeof(p));
    CHECK(dbn_pkt_parse(&p, sizeof(p), &q), "parse ok");
    CHECK(q.seq == 513 && q.copy == 2 && q.hop == 0 && !memcmp(q.sender, mac, 6) && !memcmp(q.key, key, 8), "fields");
    CHECK(!dbn_pkt_parse(&p, sizeof(p) - 1, &q), "short rejected");
    dbn_pkt_t bad = p; bad.seq ^= 1;
    CHECK(!dbn_pkt_parse(&bad, sizeof(bad), &q), "corrupt rejected");
    bad = p; bad.magic[0] = 'X';
    CHECK(!dbn_pkt_parse(&bad, sizeof(bad), &q), "magic rejected");
    bad = p; bad.ver = 9; /* crc now wrong too */
    CHECK(!dbn_pkt_parse(&bad, sizeof(bad), &q), "version rejected");

    dbn_dedupe_t dd = {0};
    CHECK(!dbn_dedupe_seen(&dd, 7), "first seen");
    CHECK(dbn_dedupe_seen(&dd, 7), "dup");
    CHECK(dbn_dedupe_seen(&dd, 7), "dup2");
    CHECK(!dbn_dedupe_seen(&dd, 8), "new seq");
    CHECK(!dbn_dedupe_seen(&dd, 7), "older seq after newer is treated as new (seq wrap ok)");

    uint8_t leds[12 * 3];
    memset(leds, 255, sizeof(leds));
    float ma = dbn_led_current_ma(leds, 12);
    float s = dbn_led_limit(leds, 12, DBN_LED_CAP_MA);
    float after = dbn_led_current_ma(leds, 12);
    printf("full white 12 px: %.0f mA est -> scale %.3f -> %.0f mA\n", ma, s, after);
    CHECK(ma > 600 && after <= 600.0f && after > 560.0f, "cap enforced (%.0f)", after);
    memset(leds, 0, sizeof(leds)); leds[0] = 255;
    CHECK(dbn_led_limit(leds, 12, 600) == 1.0f && leds[0] == 255, "no scaling under cap");
    printf(fails ? "PROTO TESTS FAILED\n" : "PROTO TESTS PASSED\n");
    return fails != 0;
}
