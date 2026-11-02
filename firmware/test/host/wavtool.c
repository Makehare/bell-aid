// Run the detector on recordings (host tool).
//
//   wavtool [options] train ring_recording.wav [more.wav ...] -- test a.wav b.wav ...
//   wavtool [options] load template.bin -- test a.wav ...
//
// WAVs: 16 kHz, mono, 16-bit PCM (convert: ffmpeg -i in.m4a -ar 16000 -ac 1 -sample_fmt s16 out.wav).
// Training files are fed back to back exactly like the device hears them: put up to 3 rings in
// one recording (e.g. from `tools/record.py`), each followed by >= 2 s of quiet.
// Options:
//   -o template.bin     save the trained template (same blob format as stored in NVS)
//   -t <0..1>           override the threshold
//   -P name=value       override a tuning parameter (see det_params_t), e.g. -P act=8
//   -q                  machine-readable output only
#include "detector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int16_t *load(const char *fn, int *n)
{
    FILE *f = fopen(fn, "rb");
    if (!f) { perror(fn); exit(2); }
    uint8_t h[12];
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fprintf(stderr, "%s: not a WAV\n", fn); exit(2); }
    int ch = 0, sr = 0, bits = 0, fmt = 0;
    for (;;) {                                           /* walk chunks: fmt, data (skip others) */
        uint8_t c[8];
        if (fread(c, 1, 8, f) != 8) { fprintf(stderr, "%s: no data chunk\n", fn); exit(2); }
        uint32_t sz = c[4] | c[5] << 8 | c[6] << 16 | (uint32_t)c[7] << 24;
        if (!memcmp(c, "fmt ", 4)) {
            uint8_t b[40] = {0};
            if (sz > sizeof(b) || fread(b, 1, sz, f) != sz) { fprintf(stderr, "%s: bad fmt\n", fn); exit(2); }
            fmt = b[0] | b[1] << 8; ch = b[2] | b[3] << 8; sr = b[4] | b[5] << 8 | b[6] << 16; bits = b[14] | b[15] << 8;
        } else if (!memcmp(c, "data", 4)) {
            if (fmt != 1 || ch != 1 || sr != DET_FS || bits != 16) {
                fprintf(stderr, "%s: need 16 kHz mono s16 PCM (got fmt %d, %d ch, %d Hz, %d bit)\n", fn, fmt, ch, sr, bits); exit(2);
            }
            int16_t *buf = calloc(sz / 2 + DET_FRAME, 2);
            *n = (int)(fread(buf, 1, sz, f) / 2);
            fclose(f);
            return buf;
        } else fseek(f, (long)(sz + (sz & 1)), SEEK_CUR);
    }
}

static void set_param(const char *kv)
{
    static const struct { const char *k; float *v; } P[] = {
        {"margin_db", &det_params.margin_db}, {"act", &det_params.act}, {"range_db", &det_params.range_db},
        {"pk_w", &det_params.pk_w}, {"w_pause", &det_params.w_pause}, {"pause_qa", &det_params.pause_qa},
        {"prefix_s", &det_params.prefix_s}, {"def_thr", &det_params.def_thr},
        {"floor_up", &det_params.floor_up}, {"floor_near", &det_params.floor_near}, {"trig_db", &det_params.trig_db},
        {"end_db", &det_params.end_db}, {"end_rel_db", &det_params.end_rel_db}, {"end_quiet_s", &det_params.end_quiet_s},
    };
    const char *eq = strchr(kv, '=');
    for (size_t i = 0; eq && i < sizeof(P) / sizeof(P[0]); i++)
        if (strlen(P[i].k) == (size_t)(eq - kv) && !strncmp(kv, P[i].k, eq - kv)) { *P[i].v = strtof(eq + 1, NULL); return; }
    fprintf(stderr, "unknown parameter: %s\n", kv); exit(2);
}

static void usage(void)
{
    fprintf(stderr, "usage: wavtool [-o tmpl.bin] [-t thr] [-P k=v] [-q] train rec.wav [...] -- test a.wav ...\n"
                    "       wavtool [-t thr] [-P k=v] [-q] load tmpl.bin -- test a.wav ...\n");
    exit(2);
}

int main(int argc, char **argv)
{
    static det_t d;
    const char *out = NULL;
    float thr = -1;
    int quiet = 0, i = 1;
    for (; i < argc && argv[i][0] == '-'; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) thr = strtof(argv[++i], NULL);
        else if (!strcmp(argv[i], "-P") && i + 1 < argc) set_param(argv[++i]);
        else if (!strcmp(argv[i], "-q")) quiet = 1;
        else usage();
    }
    if (i + 1 >= argc) usage();
    det_init(&d);
    if (!strcmp(argv[i], "load")) {
        static uint8_t blob[sizeof(det_template_t)];
        FILE *f = fopen(argv[i + 1], "rb");
        if (!f) { perror(argv[i + 1]); return 2; }
        size_t n = fread(blob, 1, sizeof(blob), f); fclose(f);
        if (!det_set_template(&d, blob, n)) { fprintf(stderr, "%s: not a v%d template\n", argv[i + 1], DET_TMPL_VERSION); return 1; }
        i += 2;
    } else if (!strcmp(argv[i], "train")) {
        det_train_begin(&d);
        for (i++; i < argc && strcmp(argv[i], "--"); i++) {
            int n; int16_t *b = load(argv[i], &n);
            for (int k = 0; k + DET_FRAME <= n; k += DET_FRAME) det_process(&d, b + k);
            free(b);
        }
        if (!det_train_finish(&d)) { fprintf(stderr, "training failed: no ring found in the training recording(s)\n"); return 1; }
        if (out) {
            FILE *f = fopen(out, "wb");
            if (!f || fwrite(&d.tmpl, 1, det_template_bytes(&d), f) != det_template_bytes(&d)) { perror(out); return 2; }
            fclose(f);
        }
    } else usage();
    if (thr > 0) det_set_threshold(&d, thr);
    printf("TRAIN rings=%d len=%u (%.2f s) thr=%.3f scores=", d.tmpl.h.rounds, d.tmpl.h.len, d.tmpl.h.len * (float)DET_FRAME / DET_FS, d.thr);
    for (int r = 0; r < d.tmpl.h.rounds; r++) printf("%s%.3f", r ? "," : "", d.train_scores[r]);
    { int na = 0; for (int j = 0; j < d.tmpl.h.len; j++) na += d.tw[j] > det_params.w_pause; printf(" active=%d", na); }
    printf(" rings_s=");
    for (int r = 0; r < d.rounds; r++) printf("%s%.2f", r ? "," : "", d.rec_len[r] * (float)DET_FRAME / DET_FS);
    printf("\n");
    if (i < argc && !strcmp(argv[i], "--")) i++;
    if (i < argc && !strcmp(argv[i], "test")) i++;
    static det_t e;
    for (; i < argc; i++) {
        int n; int16_t *b = load(argv[i], &n);
        e = d; e.nframes = 0; e.refractory = 0; e.floor_db = 30;
        memset(e.prev, 0, sizeof(e.prev));
        int m = 0; float peak = 0, peak_t = 0;
        char times[512] = ""; size_t tl = 0;
        for (int k = 0; k + DET_FRAME <= n; k += DET_FRAME) {
            if (det_process(&e, b + k) == DET_MATCH) {
                m++;
                if (tl < sizeof(times) - 16) tl += (size_t)snprintf(times + tl, sizeof(times) - tl, "%s%.2f", tl ? "," : "", (k + DET_FRAME) / (float)DET_FS);
            }
            if (e.last_score > peak) { peak = e.last_score; peak_t = (k + DET_FRAME) / (float)DET_FS; }
        }
        printf("FILE %s matches=%d peak=%.3f times=%s peak_t=%.2f\n", argv[i], m, peak, tl ? times : "-", peak_t);
        free(b);
    }
    (void)quiet;
    return 0;
}
