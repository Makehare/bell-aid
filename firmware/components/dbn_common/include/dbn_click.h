#pragma once
/* Button click classifier (pure C, host-tested in test/host/test_click.c).
 * Input: debounced button level + time, polled every ~10-20 ms.
 * Output: one event per gesture.
 *  - release after >= VLONG_MS -> VLONG, >= LONG_MS -> LONG (any pending clicks are dropped)
 *  - shorter presses are clicks; they are counted and decided when no new press starts within
 *    DBN_CLICK_GAP_MS after the last release (gap = release -> next press, <= 400 ms counts):
 *      1 click -> SHORT, 2 clicks -> DOUBLE (ignored by the roles), 3 clicks -> TRIPLE,
 *      4 or more -> DOUBLE (= nothing, so a fumbled series never fires the single action)
 *  - immediate mode (alert flash, training, pairing): a click is reported as SHORT on release,
 *    without waiting for the window, so dismissing/ending stays instant. */
#include <stdint.h>
#include <stdbool.h>

#define DBN_CLICK_GAP_MS 400

typedef enum { DBN_CLK_NONE = 0, DBN_CLK_SHORT, DBN_CLK_DOUBLE, DBN_CLK_TRIPLE, DBN_CLK_LONG, DBN_CLK_VLONG } dbn_click_t;

typedef struct {
    bool     down;          /* debounced level */
    uint32_t press_ms, release_ms;
    uint8_t  clicks;
} dbn_click_state_t;

void        dbn_click_init(dbn_click_state_t *s);
dbn_click_t dbn_click_update(dbn_click_state_t *s, bool pressed, uint32_t now_ms,
                             uint32_t long_ms, uint32_t vlong_ms, bool immediate);
