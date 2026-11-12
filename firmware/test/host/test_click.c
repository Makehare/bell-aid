// Click classifier tests: single/double/triple/long/vlong, timing edges, immediate mode.
#include "dbn_click.h"
#include <stdio.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)
#define LONG 1500
#define VLONG 5000

typedef struct { uint32_t press, release; } pr_t;

/* count events of each type for a press script, poll every 10 ms (offset = start of ms counter) */
static int ev[8];
static uint32_t step = 10;
static uint32_t last_at;
static void run(const pr_t *p, int n, uint32_t end, bool imm, uint32_t base)
{
    dbn_click_state_t s; dbn_click_init(&s);
    for (int i = 0; i < 8; i++) ev[i] = 0;
    for (uint32_t k = 0; k <= end; k += step) {
        bool down = false;
        for (int i = 0; i < n; i++) if (k >= p[i].press && k < p[i].release) down = true;
        dbn_click_t e = dbn_click_update(&s, down, base + k, LONG, VLONG, imm);
        if (e) { ev[e]++; last_at = k; }
    }
}
static int total(void) { int t = 0; for (int i = 1; i < 8; i++) t += ev[i]; return t; }
#define ONLY(e) (ev[e] == 1 && total() == 1)

int main(void)
{
    { pr_t p[] = {{100, 200}};
      run(p, 1, 2000, false, 0);
      CHECK(ONLY(DBN_CLK_SHORT), "single");
      CHECK(last_at >= 200 + DBN_CLICK_GAP_MS && last_at <= 200 + DBN_CLICK_GAP_MS + 20, "single deferred by gap, at=%u", (unsigned)last_at);
      run(p, 1, 300, false, 0);
      CHECK(total() == 0, "single not reported before window ends");
      run(p, 1, 2000, true, 0);
      CHECK(ONLY(DBN_CLK_SHORT) && last_at == 200, "immediate single on release, at=%u", (unsigned)last_at); }
    { pr_t p[] = {{100, 180}, {400, 480}};
      run(p, 2, 3000, false, 0);
      CHECK(ONLY(DBN_CLK_DOUBLE), "double"); }
    { pr_t p[] = {{100, 180}, {400, 480}, {700, 780}};
      run(p, 3, 3000, false, 0);
      CHECK(ONLY(DBN_CLK_TRIPLE), "triple");
      CHECK(last_at >= 780 + DBN_CLICK_GAP_MS, "triple decided after window"); }
    { pr_t p[] = {{100, 180}, {400, 480}, {700, 780}, {1000, 1080}};
      run(p, 4, 4000, false, 0);
      CHECK(ONLY(DBN_CLK_DOUBLE), "4 clicks = nothing (neither single nor triple)"); }
    { pr_t p[] = {{100, 200}, {590, 650}, {1000, 1080}};            /* gaps 390 / 350 */
      run(p, 3, 4000, false, 0);
      CHECK(ONLY(DBN_CLK_TRIPLE), "gaps 390/350 -> triple"); }
    { pr_t p[] = {{100, 200}, {700, 780}, {1000, 1080}};            /* first gap 500 splits the series */
      run(p, 3, 4000, false, 0);
      CHECK(ev[DBN_CLK_SHORT] == 1 && ev[DBN_CLK_DOUBLE] == 1 && total() == 2, "gap 500 splits: short=%d double=%d", ev[DBN_CLK_SHORT], ev[DBN_CLK_DOUBLE]); }
    { pr_t p[] = {{100, 200}, {620, 700}, {1000, 1080}};            /* gap 420 */
      run(p, 3, 4000, false, 0);
      CHECK(ev[DBN_CLK_TRIPLE] == 0, "gap 420 is not a triple"); }
    /* exact window boundary: 3 clicks, gap g between each release and the next press. The decision fires at the first
     * poll with age > 400 ms, but a press seen on that same poll still wins, so the real limit is 400 ms plus up to
     * one poll period (README: "<= 0.4 s"). Pinned at poll 10 ms and at the firmware's 20 ms. */
    for (int si = 0; si < 2; si++) {
        step = si ? 20 : 10;
        uint32_t gaps[] = {390, 400, 400 + step, 400 + 2 * step};
        for (int gi = 0; gi < 4; gi++) {
            uint32_t g = gaps[gi], b = 100;
            pr_t p[] = {{b, b + 80}, {b + 80 + g, b + 160 + g}, {b + 160 + 2 * g, b + 240 + 2 * g}};
            run(p, 3, 4000, false, 0);
            if (gi < 3) CHECK(ONLY(DBN_CLK_TRIPLE), "poll %u gap %u -> triple (S%d D%d T%d)", (unsigned)step, (unsigned)g, ev[DBN_CLK_SHORT], ev[DBN_CLK_DOUBLE], ev[DBN_CLK_TRIPLE]);
            else        CHECK(ev[DBN_CLK_TRIPLE] == 0, "poll %u gap %u must not be a triple", (unsigned)step, (unsigned)g);
        }
    }
    step = 10;
    { pr_t p[] = {{100, 1590}};  run(p, 1, 3000, false, 0);  CHECK(ONLY(DBN_CLK_SHORT), "1.490 s hold = short"); }
    { pr_t p[] = {{100, 1600}};  run(p, 1, 3000, false, 0);  CHECK(ONLY(DBN_CLK_LONG) && last_at == 1600, "1.5 s hold = long, no window delay"); }
    { pr_t p[] = {{100, 4999}};  run(p, 1, 7000, false, 0);  CHECK(ONLY(DBN_CLK_LONG), "4.9 s = long"); }
    { pr_t p[] = {{100, 5100}};  run(p, 1, 7000, false, 0);  CHECK(ONLY(DBN_CLK_VLONG), "5 s hold = vlong"); }
    { pr_t p[] = {{100, 180}, {400, 2100}};  run(p, 2, 4000, false, 0);  CHECK(ONLY(DBN_CLK_LONG), "click then long hold = long only"); }
    { pr_t p[] = {{100, 180}, {400, 480}, {700, 2400}};  run(p, 3, 5000, false, 0);  CHECK(ONLY(DBN_CLK_LONG), "2 clicks then long = long only"); }
    { pr_t p[] = {{100, 180}, {400, 480}, {700, 780}};
      run(p, 3, 2000, true, 0);
      CHECK(ev[DBN_CLK_SHORT] == 3 && total() == 3, "immediate: every click is SHORT, never triple"); }
    { pr_t p[] = {{100, 180}, {400, 480}, {700, 780}};
      run(p, 3, 3000, false, 0xFFFFFF00u);
      CHECK(ONLY(DBN_CLK_TRIPLE), "triple across uint32 ms wrap"); }
    printf(fails ? "CLICK TESTS FAILED\n" : "CLICK TESTS PASSED\n");
    return fails != 0;
}
