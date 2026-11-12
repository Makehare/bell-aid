#include "dbn_click.h"
#include <string.h>

void dbn_click_init(dbn_click_state_t *s) { memset(s, 0, sizeof(*s)); }

dbn_click_t dbn_click_update(dbn_click_state_t *s, bool pressed, uint32_t now, uint32_t long_ms, uint32_t vlong_ms, bool immediate)
{
    if (pressed && !s->down) {                       /* press edge */
        s->down = true; s->press_ms = now;
        return DBN_CLK_NONE;
    }
    if (!pressed && s->down) {                       /* release edge */
        s->down = false; s->release_ms = now;
        uint32_t held = now - s->press_ms;
        if (held >= vlong_ms) { s->clicks = 0; return DBN_CLK_VLONG; }
        if (held >= long_ms)  { s->clicks = 0; return DBN_CLK_LONG; }
        if (immediate) { s->clicks = 0; return DBN_CLK_SHORT; }
        if (s->clicks < 255) s->clicks++;
        return DBN_CLK_NONE;
    }
    if (!pressed && s->clicks && (now - s->release_ms) > DBN_CLICK_GAP_MS) {
        uint8_t n = s->clicks;
        s->clicks = 0;
        return n == 1 ? DBN_CLK_SHORT : n == 3 ? DBN_CLK_TRIPLE : DBN_CLK_DOUBLE;
    }
    return DBN_CLK_NONE;
}
