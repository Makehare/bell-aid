// Host-side unit tests for the detector on synthetic audio. Build+run: make -C test/host test
// (Real-audio evaluation: test/eval, see test/eval/README.md.)
#include "detector.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static uint32_t rs = 12345;
static float rnd(void) { rs = rs * 1664525u + 1013904223u; return ((rs >> 8) & 0xFFFF) / 32768.0f - 1.0f; }
static float gauss(void) { float s = 0; for (int i = 0; i < 6; i++) s += rnd(); return s / 1.7f; }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

#define MAXN (16000 * 60)
static int16_t buf[MAXN];
static float   fbuf[MAXN];

static void clr(int n) { memset(fbuf, 0, sizeof(float) * n); }
static void add_noise(int n, float amp) { for (int i = 0; i < n; i++) fbuf[i] += amp * gauss(); }
/* decaying multi-harmonic note */
static void add_note(int start, int len, float hz, float amp, float decay_s)
{
    for (int i = 0; i < len && start + i < MAXN; i++) {
        float t = (float)i / DET_FS, env = expf(-t / decay_s) * (i < 80 ? i / 80.0f : 1.0f);
        float v = sinf(2 * (float)M_PI * hz * t) + 0.5f * sinf(2 * (float)M_PI * 2.01f * hz * t) + 0.25f * sinf(2 * (float)M_PI * 3.02f * hz * t);
        fbuf[start + i] += amp * env * v;
    }
}
static void add_ding_dong(int start, float amp, float f1, float f2, float gap_s)
{
    add_note(start, 6400 + (int)(gap_s * DET_FS), f1, amp, 0.25f);            /* "ding" */
    add_note(start + 6400 + (int)(gap_s * DET_FS), 9600, f2, amp, 0.35f);     /* "dong" */
}
/* 8-note melody with a 1 s pause in the middle, ~5.5 s */
static const float MEL[8] = {784, 659, 698, 523, 523, 698, 784, 659};
static void add_melody(int start, float amp, const float *notes)
{
    for (int k = 0; k < 8; k++) {
        float t0 = 0.5f * k + (k >= 4 ? 1.0f : 0.0f);
        add_note(start + (int)(t0 * DET_FS), DET_FS, notes[k], amp, 0.3f);
    }
}
static void add_speech(int start, int len, float amp)
{
    float ph = 0;
    for (int i = 0; i < len; i++) {
        float t = (float)i / DET_FS;
        float f0 = 120 + 20 * sinf(2 * (float)M_PI * 3 * t);
        ph += 2 * (float)M_PI * f0 / DET_FS;
        float f1 = 500 + 300 * sinf(2 * (float)M_PI * 2.3f * t), f2 = 1500 + 800 * sinf(2 * (float)M_PI * 1.7f * t);
        float v = 0;
        for (int h = 1; h < 40; h++) {
            float fh = h * f0;
            float g = expf(-powf((fh - f1) / 200, 2)) + 0.6f * expf(-powf((fh - f2) / 300, 2));
            v += g * sinf(h * ph) / h;
        }
        float env = 0.5f + 0.5f * sinf(2 * (float)M_PI * 4 * t);
        fbuf[start + i] += amp * env * v;
    }
}
static void add_knock(int start, float amp) { for (int k = 0; k < 3; k++) add_note(start + k * 5000, 3000, 180, amp, 0.03f); }
static void add_noise_burst(int start, int len, float amp) { for (int i = 0; i < len; i++) fbuf[start + i] += amp * gauss() * (1.0f - (float)i / len); }
static void add_beeps(int start, float amp) { for (int k = 0; k < 4; k++) add_note(start + k * 4000, 2000, 1000, amp, 10.0f); }
/* bell-push click: press + release ~0.2 s later, 8 ms decay, broadband + 3 kHz resonance */
static void add_click(int start, float amp)
{
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < 800 && start + c * 3200 + i < MAXN; i++) {
            float t = (float)i / DET_FS, env = expf(-t / 0.008f) * (c ? 0.5f : 1.0f);
            fbuf[start + c * 3200 + i] += amp * env * (0.6f * gauss() + sinf(2 * (float)M_PI * 3000 * t));
        }
}

static int quant(int n)
{
    int blocks = (n + DET_FRAME - 1) / DET_FRAME;
    for (int i = 0; i < blocks * DET_FRAME; i++) {
        float v = i < n ? fbuf[i] : 0;
        buf[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    return blocks;
}
/* feeds fbuf; returns number of matches; first match hop in *first */
static int run(det_t *d, int n, int *first)
{
    int blocks = quant(n), m = 0;
    if (first) *first = -1;
    for (int b = 0; b < blocks; b++)
        if (det_process(d, buf + b * DET_FRAME) == DET_MATCH) { m++; if (first && *first < 0) *first = b; }
    return m;
}
static det_t *fresh(const det_t *d)
{
    det_t *e = malloc(sizeof(det_t)); *e = *d; e->nframes = 0; e->floor_db = 30; e->refractory = 0;
    memset(e->prev, 0, sizeof(e->prev));
    return e;
}

typedef enum { DINGDONG, MELODY } kind_t;
static void add_ring(kind_t k, int start, float amp)
{
    if (k == DINGDONG) add_ding_dong(start, amp, 659, 523, 0);
    else add_melody(start, amp, MEL);
}
static float ring_s(kind_t k) { return k == DINGDONG ? 1.0f : 6.0f; }

static void train(det_t *d, kind_t k, float amp, float noise, int rounds)
{
    det_init(d);
    det_train_begin(d);
    int per = (int)((ring_s(k) + 5.0f) * DET_FS), total = per * rounds + 3 * DET_FS;
    clr(total);
    add_noise(total, noise);
    for (int r = 0; r < rounds; r++) add_ring(k, 2 * DET_FS + per * r + (int)(rnd() * 500), amp * (1 + 0.1f * rnd()));
    int ev_done = 0, blocks = quant(total);
    for (int b = 0; b < blocks; b++) if (det_process(d, buf + b * DET_FRAME) == DET_TRAIN_ROUND_DONE) ev_done++;
    CHECK(det_train_rounds(d) == rounds && ev_done == rounds, "train rounds %d (events %d) != %d", det_train_rounds(d), ev_done, rounds);
    CHECK(det_train_finish(d), "train finish");
}

static void positives(const det_t *d, kind_t k, const char *name)
{
    int pos = 0, N = 30; float lat = 0;
    for (int t = 0; t < N; t++) {
        float amp = 800.0f * powf(7.5f, (float)t / (N - 1));          /* 800 .. 6000 */
        float noise = 40 + 80 * (rnd() + 1) / 2;                       /* 40..120 */
        int total = (int)((ring_s(k) + 12) * DET_FS);
        clr(total); add_noise(total, noise);
        int on = DET_FS * 4 + (int)((rnd() + 1) * 4000);
        add_ring(k, on, amp);
        det_t *e = fresh(d);
        int first, m = run(e, total, &first);
        if (m >= 1) { pos++; lat += ((first + 1) * DET_FRAME - on) / (float)DET_FS; }
        else printf("  miss: amp=%.0f noise=%.0f peak=%.3f\n", amp, noise, e->last_score);
        CHECK(m <= 1, "%s: double trigger %d", name, m);
        free(e);
    }
    printf("%s positives: %d/%d detected, mean latency from onset %.2f s\n", name, pos, N, pos ? lat / pos : 0);
    CHECK(pos >= N * 95 / 100, "%s positive rate %d/%d", name, pos, N);
}

static void negatives(const det_t *d, const char *name)
{
    static const char *nn[] = {"noise", "other chime 880/698", "noise bursts", "speech-like", "knocks", "1 kHz beeps", "other melody"};
    static const float OTHER[8] = {523, 587, 659, 698, 784, 698, 659, 587};
    for (int c = 0; c < 7; c++) {
        int fp = 0;
        for (int rep = 0; rep < 4; rep++) {
            int total = DET_FS * 30;
            clr(total); add_noise(total, 40 + rep * 20);
            switch (c) {
            case 0: break;
            case 1: for (int k = 0; k < 4; k++) add_ding_dong(DET_FS * (3 + 6 * k), 3000, 880, 698, 0); break;
            case 2: for (int k = 0; k < 6; k++) add_noise_burst(DET_FS * (2 + 4 * k), 12000, 4000); break;
            case 3: add_speech(DET_FS * 2, DET_FS * 25, 3000); break;
            case 4: for (int k = 0; k < 5; k++) add_knock(DET_FS * (2 + 5 * k), 5000); break;
            case 5: for (int k = 0; k < 5; k++) add_beeps(DET_FS * (2 + 5 * k), 3000); break;
            case 6: for (int k = 0; k < 3; k++) add_melody(DET_FS * (2 + 9 * k), 3000, OTHER); break;
            }
            det_t *e = fresh(d);
            fp += run(e, total, NULL);
            free(e);
        }
        printf("%s negative [%s]: %d false triggers\n", name, nn[c], fp);
        CHECK(fp == 0, "%s: false triggers on %s: %d", name, nn[c], fp);
    }
}

int main(void)
{
    det_t *d = malloc(sizeof(det_t));

    /* ---- ding-dong, 3 rings ---- */
    train(d, DINGDONG, 3000, 60, 3);
    printf("ding-dong trained: len=%u (%.2f s) thr=%.3f\n", d->tmpl.h.len, d->tmpl.h.len * 0.032f, d->thr);
    CHECK(d->tmpl.h.len >= 20 && d->tmpl.h.len <= 80, "ding-dong template length %u", d->tmpl.h.len);
    positives(d, DINGDONG, "ding-dong");
    negatives(d, "ding-dong");

    /* ding-dong with the button held longer (gap 0.5 s between ding and dong) */
    {
        int total = DET_FS * 12; clr(total); add_noise(total, 60); add_ding_dong(DET_FS * 4, 3000, 659, 523, 0.5f);
        det_t *e = fresh(d); int m = run(e, total, NULL); free(e);
        printf("ding-dong with longer hold: %d match(es)\n", m);
        CHECK(m == 1, "held ding-dong: %d matches", m);
    }

    /* ---- bell push next to the sender: a loud click 0.3-1.0 s before every training ring must
     *      not end up in the template (it made templates that never fire on the chime) ---- */
    {
        uint16_t plain = d->tmpl.h.len;
        det_t *c = malloc(sizeof(det_t));
        det_init(c);
        det_train_begin(c);
        int per = 7 * DET_FS, total = per * 3 + 3 * DET_FS;
        clr(total); add_noise(total, 60);
        for (int r = 0; r < 3; r++) {
            int on = 2 * DET_FS + per * r;
            add_click(on, 9000);
            add_ding_dong(on + (int)((0.3f + 0.35f * r) * DET_FS), 3000, 659, 523, 0);
        }
        int blocks = quant(total);
        for (int b = 0; b < blocks; b++) det_process(c, buf + b * DET_FRAME);
        CHECK(det_train_rounds(c) == 3 && det_train_finish(c), "train with clicks");
        printf("ding-dong trained with a push click before each ring: len=%u (without clicks %u)\n", c->tmpl.h.len, plain);
        CHECK(c->tmpl.h.len <= plain + 3, "click kept in the template: len %u vs %u", c->tmpl.h.len, plain);
        positives(c, DINGDONG, "ding-dong (trained with clicks)");
        /* the click alone never fires */
        int fp = 0;
        for (int rep = 0; rep < 4; rep++) {
            int t2 = DET_FS * 20; clr(t2); add_noise(t2, 60);
            for (int k = 0; k < 8; k++) add_click(DET_FS * (1 + 2 * k), 9000);
            det_t *e = fresh(c); fp += run(e, t2, NULL); free(e);
        }
        printf("ding-dong (trained with clicks) negative [push clicks]: %d false triggers\n", fp);
        CHECK(fp == 0, "false triggers on push clicks: %d", fp);
        free(c);
    }

    /* ---- threshold tuning: above the peak score rejects, 0.3 accepts (noisy ring) ---- */
    {
        int total = DET_FS * 10; clr(total); add_noise(total, 500); add_ding_dong(DET_FS * 3, 2500, 659, 523, 0.2f);
        det_t *e = fresh(d); det_set_threshold(e, 0.99f); int top = 0; float peak = 0;
        int blocks = quant(total);
        for (int b = 0; b < blocks; b++) { top += det_process(e, buf + b * DET_FRAME) == DET_MATCH; if (e->last_score > peak) peak = e->last_score; }
        free(e);
        e = fresh(d); det_set_threshold(e, peak + 0.01f); int hi = run(e, total, NULL); free(e);
        e = fresh(d); det_set_threshold(e, 0.3f); int lo = run(e, total, NULL); free(e);
        printf("threshold tuning: peak %.3f, thr above peak -> %d, thr=0.30 -> %d matches\n", peak, hi, lo);
        CHECK(top == 0 && peak < 0.98f && hi == 0 && lo >= 1, "threshold tuning");
    }

    /* ---- template blob round trip + version check ---- */
    {
        static uint8_t blob[sizeof(det_template_t)];
        size_t n = det_template_bytes(d);
        CHECK(n == DET_TMPL_BYTES(d->tmpl.h.len) && n < 16000, "blob size %zu", n);
        memcpy(blob, &d->tmpl, n);
        det_t *e = malloc(sizeof(det_t)); det_init(e);
        CHECK(det_set_template(e, blob, n) && e->have_tmpl && fabsf(e->thr - d->thr) < 0.006f, "blob reload");
        CHECK(!det_set_template(e, blob, n - 1) && !e->have_tmpl, "wrong size rejected");
        blob[2] = 1;  /* version 1 */
        CHECK(!det_set_template(e, blob, n), "old version rejected");
        static uint8_t v1[48 * 32 * 4 + 4 + 4 + 48];  /* size of the v1 blob */
        memset(v1, 0, sizeof(v1)); v1[0] = 24;
        CHECK(!det_set_template(e, v1, sizeof(v1)), "v1 blob rejected");
        free(e);
    }

    /* ---- long melody with a pause: one ring per melody, whole melody in the template ---- */
    train(d, MELODY, 3000, 60, 3);
    printf("melody trained: len=%u (%.2f s) thr=%.3f\n", d->tmpl.h.len, d->tmpl.h.len * 0.032f, d->thr);
    CHECK(d->tmpl.h.len * 0.032f >= 5.0f, "melody template only %.2f s", d->tmpl.h.len * 0.032f);
    positives(d, MELODY, "melody");
    negatives(d, "melody");

    /* ---- single-ring training also works ---- */
    det_init(d);
    {
        det_train_begin(d);
        int total = DET_FS * 10; clr(total); add_noise(total, 80); add_ding_dong(DET_FS * 3, 2500, 784, 622, 0);
        int blocks = quant(total);
        for (int b = 0; b < blocks; b++) det_process(d, buf + b * DET_FRAME);
        CHECK(det_train_rounds(d) == 1 && det_train_finish(d), "single-ring train");
        CHECK(fabsf(d->thr - det_default_threshold()) < 0.006f, "single-ring threshold %.3f", d->thr);
        total = DET_FS * 10; clr(total); add_noise(total, 80); add_ding_dong(DET_FS * 3, 1800, 784, 622, 0);
        det_t *e = fresh(d); int m = run(e, total, NULL); free(e);
        CHECK(m == 1, "single-ring trained detector: %d matches", m);
    }

    /* ---- feature sanity: tonal peak lands in its semitone band, level kept above floor ---- */
    {
        det_t *e = malloc(sizeof(det_t)); det_init(e);
        int16_t q[DET_FRAME]; float f[DET_BANDS], db;
        for (int k = 0; k < 20; k++) { for (int i = 0; i < DET_FRAME; i++) q[i] = (int16_t)(30 * gauss()); det_features(e, q, f, &db); }
        for (int k = 0; k < 3; k++) {
            for (int i = 0; i < DET_FRAME; i++) q[i] = (int16_t)(3000 * sinf(2 * (float)M_PI * 1000 * (k * DET_FRAME + i) / DET_FS) + 30 * gauss());
            det_features(e, q, f, &db);
        }
        int arg = 0; for (int b = 1; b < DET_BANDS; b++) if (f[b] > f[arg]) arg = b;
        float hz = DET_LO_HZ * powf(2.0f, arg / 12.0f);
        printf("1 kHz tone -> band %d (%.0f Hz), %.1f dB above floor\n", arg, hz, f[arg]);
        CHECK(fabsf(hz - 1000) < 60 && f[arg] > 30, "tone feature");
        free(e);
    }

    free(d);
    printf(fails ? "DETECTOR TESTS FAILED (%d)\n" : "DETECTOR TESTS PASSED\n", fails);
    return fails != 0;
}
