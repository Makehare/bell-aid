#include "detector.h"
#include <math.h>
#include <string.h>

/* Tunables. Defaults are the values chosen on the evaluation set (test/eval, README "Detector").
 * The host tools may override them; the firmware uses the defaults. */
det_params_t det_params = {
    .margin_db   = 6.0f,    /* band excess counts only above band floor + this */
    .act         = 10.0f,   /* hop is "sound" if its strongest band is this far above margin */
    .range_db    = 30.0f,   /* matching keeps bands within this range of the hop's strongest band */
    .pk_w        = 0.35f,   /* weight of bands that are not local spectral peaks */
    .w_pause     = 0.2f,    /* DTW weight of template pause hops */
    .pause_qa    = 0.30f,   /* similarity: template pause vs. live sound */
    .prefix_s    = 2.0f,    /* a match may fire once this much of the template has matched */
    .def_thr     = 0.74f,   /* detection threshold (console `thr` overrides) */
    .floor_up    = 0.002f,  /* per hop: band floor rise while the band is >10 dB above it */
    .floor_near  = 0.02f,   /* per hop: band floor tracking near the floor (~1.5 s) */
    .trig_db     = 12.0f,   /* training: ring onset above frame floor */
    .end_db      = 6.0f,    /* training: hop is "still sounding" above frame floor + this */
    .end_rel_db  = 25.0f,   /* training: ... and less than this below the ring peak */
    .end_quiet_s = 2.0f,    /* training: ring ends after this much quiet */
};

#define REFRACTORY_S   1.0f
#define INF            1e30f

static float s_win[DET_WIN];
static float s_cos[DET_WIN / 2], s_sin[DET_WIN / 2];
static int   s_edge[DET_BANDS + 1];
static int   s_lo_bin, s_hi_bin;
static bool  s_tables;

static void tables(void)
{
    if (s_tables) return;
    for (int i = 0; i < DET_WIN; i++)
        s_win[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / (DET_WIN - 1));
    for (int i = 0; i < DET_WIN / 2; i++) {
        s_cos[i] = cosf(2.0f * (float)M_PI * i / DET_WIN);
        s_sin[i] = -sinf(2.0f * (float)M_PI * i / DET_WIN);
    }
    const float binhz = (float)DET_FS / DET_WIN;
    for (int k = 0; k <= DET_BANDS; k++) {
        float hz = DET_LO_HZ * powf(2.0f, ((float)k - 0.5f) / 12.0f);   /* band k centred on 300*2^(k/12) */
        s_edge[k] = (int)lrintf(hz / binhz);
        if (k > 0 && s_edge[k] <= s_edge[k - 1]) s_edge[k] = s_edge[k - 1] + 1;
        if (s_edge[k] > DET_WIN / 2) s_edge[k] = DET_WIN / 2;
    }
    s_lo_bin = s_edge[0];
    s_hi_bin = s_edge[DET_BANDS];
    s_tables = true;
}

static void fft(float *re, float *im)
{
    const int n = DET_WIN;
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        int step = n / len;
        for (int i = 0; i < n; i += len) {
            for (int k = 0; k < len / 2; k++) {
                float wr = s_cos[k * step], wi = s_sin[k * step];
                int a = i + k, b = i + k + len / 2;
                float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr;        im[a] += ti;
            }
        }
    }
}

/* Band levels (dB, strongest bin per band) and frame level of the 1024-sample window prev+pcm. */
static void band_levels(det_t *d, const int16_t *pcm, float lv[DET_BANDS], float *frame_db)
{
    static float re[DET_WIN], im[DET_WIN];
    tables();
    float mean = 0;
    for (int i = 0; i < DET_FRAME; i++) mean += (float)d->prev[i] + (float)pcm[i];
    mean /= DET_WIN;
    for (int i = 0; i < DET_FRAME; i++) {
        re[i] = ((float)d->prev[i] - mean) * s_win[i];
        re[i + DET_FRAME] = ((float)pcm[i] - mean) * s_win[i + DET_FRAME];
    }
    memset(im, 0, sizeof(im));
    memcpy(d->prev, pcm, sizeof(d->prev));
    fft(re, im);
    float tot = 0;
    for (int b = 0; b < DET_BANDS; b++) {
        float mx = 0;
        for (int k = s_edge[b]; k < s_edge[b + 1]; k++) {
            float p = re[k] * re[k] + im[k] * im[k];
            tot += p;
            if (p > mx) mx = p;
        }
        lv[b] = 10.0f * log10f(mx + 1.0f);
    }
    *frame_db = 10.0f * log10f(tot / (float)(s_hi_bin - s_lo_bin) + 1.0f);
}

/* Feature = band level above the band floor; updates the floors. */
void det_features(det_t *d, const int16_t *pcm, float feat[DET_BANDS], float *frame_db)
{
    float lv[DET_BANDS];
    band_levels(d, pcm, lv, frame_db);
    const det_params_t *P = &det_params;
    d->nframes++;
    if (d->nframes <= 8) {                                  /* start-up: learn floors quickly */
        for (int b = 0; b < DET_BANDS; b++)
            d->bfloor[b] = d->nframes == 1 ? lv[b] : 0.7f * d->bfloor[b] + 0.3f * lv[b];
    }
    for (int b = 0; b < DET_BANDS; b++) {
        float x = lv[b] - d->bfloor[b] - P->margin_db;
        feat[b] = x < 0 ? 0 : x > 120 ? 120 : x;
    }
    if (d->nframes > 8) {
        /* floor = running mean of the band level while it is near the floor (tracks the
         * background), very slow rise while clearly above it, frozen while recording a ring */
        bool rec = d->train_state == 2;
        for (int b = 0; b < DET_BANDS && !rec; b++) {
            float e = lv[b] - d->bfloor[b];
            d->bfloor[b] += (e < 10.0f ? P->floor_near : P->floor_up) * e;
        }
    }
    /* frame-level floor (training onset / end and the monitor printout) */
    float db = *frame_db;
    if (d->nframes <= 8) d->floor_db = (d->nframes == 1) ? db : 0.7f * d->floor_db + 0.3f * db;
    else if (db < d->floor_db) d->floor_db = 0.8f * d->floor_db + 0.2f * db;
    else if (db < d->floor_db + 10.0f) d->floor_db += 0.01f * (db - d->floor_db);
    else if (d->train_state != 2) d->floor_db += 0.0005f * (db - d->floor_db);
}

/* ---- matching ---- */

/* Shape a feature (dB above the band floors) for matching: keep only bands within range_db of the
 * hop's strongest band (tonal partials stay sharp peaks, broadband sound becomes a flat vector),
 * bands that are not local spectral peaks count pk_w. Returns the norm; *act = strongest band
 * above `act` dB (else the hop is a pause). */
static float shape(const float *x, float *y, bool *act)
{
    const det_params_t *P = &det_params;
    float mx = 0;
    for (int b = 0; b < DET_BANDS; b++) if (x[b] > mx) mx = x[b];
    *act = mx > P->act;
    float base = mx - P->range_db, n = 0;
    for (int b = 0; b < DET_BANDS; b++) {
        float v = x[b] - base;
        if (v <= 0 || x[b] <= 0) { y[b] = 0; continue; }
        bool peak = (b == 0 || x[b] >= x[b - 1]) && (b == DET_BANDS - 1 || x[b] >= x[b + 1]);
        if (!peak) v *= P->pk_w;
        y[b] = v;
        n += v * v;
    }
    return sqrtf(n);
}

/* shaped template hops (quantised), norms, activity, DTW weights */
static void shape_rows(const uint8_t (*f)[DET_BANDS], int M, uint8_t (*q)[DET_BANDS], float *nrm, float *w)
{
    float x[DET_BANDS], y[DET_BANDS];
    for (int j = 0; j < M; j++) {
        bool a;
        for (int b = 0; b < DET_BANDS; b++) x[b] = f[j][b] * DET_QSTEP;
        shape(x, y, &a);
        float n = 0;
        for (int b = 0; b < DET_BANDS; b++) {
            float v = y[b] / DET_QSTEP + 0.5f;
            q[j][b] = (uint8_t)(v > 255 ? 255 : v);
            n += (q[j][b] * DET_QSTEP) * (q[j][b] * DET_QSTEP);
        }
        nrm[j] = a ? sqrtf(n) : 0;                         /* 0 = pause */
        w[j] = a ? 1.0f : det_params.w_pause;
    }
}

/* frame cost in [0,1]: shaped template hop t (norm tn, 0 = pause) vs shaped live hop y (norm yn, 0 = pause) */
static float cost(const uint8_t *t, float tn, const float *y, float yn)
{
    if (tn > 0 && yn > 0) {
        float s = 0;
        for (int b = 0; b < DET_BANDS; b++) s += (float)t[b] * y[b];
        s = s * DET_QSTEP / (tn * yn);
        return 1.0f - (s > 1 ? 1 : s);
    }
    if (tn <= 0 && yn <= 0) return 0.0f;                  /* pause matches pause */
    return tn > 0 ? 1.0f : 1.0f - det_params.pause_qa;    /* sound expected / pause expected */
}

/* One streaming subsequence-DTW step (weighted). c[j] = cost of live hop t vs template hop j,
 * w[j] = template hop weight. D/L/C1 hold the columns for t-1 ([0]) and t-2 ([1]). Returns the
 * best score 1 - (weighted mean cost) over template ends j >= from. */
static void dtw_reset(float D[2][DET_MAX_LEN], float L[2][DET_MAX_LEN], float *C1)
{
    for (int j = 0; j < DET_MAX_LEN; j++) { D[0][j] = D[1][j] = INF; L[0][j] = L[1][j] = 1; C1[j] = 1; }
}

static float dtw_step(float D[2][DET_MAX_LEN], float L[2][DET_MAX_LEN], float *C1, const float *c, const float *w, int M, int from, int *at)
{
    float Dn[DET_MAX_LEN], Ln[DET_MAX_LEN];
    float best = -1;
    Dn[0] = w[0] * c[0]; Ln[0] = w[0];
    for (int j = 1; j < M; j++) {
        float bd = INF, bl = 1, ba = INF;
        float wc = w[j] * c[j];
        if (D[0][j - 1] < INF) {                               /* diagonal */
            float v = D[0][j - 1] + wc, l = L[0][j - 1] + w[j];
            if (v < ba * l) { bd = v; bl = l; ba = v / l; }
        }
        if (j >= 2 && D[0][j - 2] < INF) {                     /* live 2x faster: skip a template hop */
            float v = D[0][j - 2] + w[j - 1] * c[j - 1] + wc, l = L[0][j - 2] + w[j - 1] + w[j];
            if (v < ba * l) { bd = v; bl = l; ba = v / l; }
        }
        if (D[1][j - 1] < INF) {                               /* live 2x slower: hold a template hop */
            float v = D[1][j - 1] + w[j] * C1[j] + wc, l = L[1][j - 1] + 2 * w[j];
            if (v < ba * l) { bd = v; bl = l; ba = v / l; }
        }
        Dn[j] = bd; Ln[j] = bl;
    }
    for (int j = from; j < M; j++) {
        if (Dn[j] >= INF) continue;
        float s = 1.0f - Dn[j] / Ln[j];
        if (s > best) { best = s; if (at) *at = j; }
    }
    memcpy(D[1], D[0], sizeof(float) * M); memcpy(L[1], L[0], sizeof(float) * M);
    memcpy(D[0], Dn, sizeof(float) * M);   memcpy(L[0], Ln, sizeof(float) * M);
    memcpy(C1, c, sizeof(float) * M);
    return best;
}

/* earliest template hop at which a match may fire: prefix_s into the template, and at least
 * MIN_ACTIVE sounding hops matched (so a template with long pauses cannot fire on a pause) */
#define MIN_ACTIVE 25
static int prefix_for(const float *w, int M)
{
    int p = (int)(det_params.prefix_s * DET_FS / DET_FRAME + 0.5f), na = 0, j = 0;
    for (; j < M && na < MIN_ACTIVE; j++) na += w[j] > det_params.w_pause;
    if (j - 1 > p) p = j - 1;
    return M - 1 < p ? M - 1 : p;
}

static void derive(det_t *d)
{
    int M = d->tmpl.h.len;
    shape_rows((const uint8_t (*)[DET_BANDS])d->tmpl.f, M, d->tq, d->tnorm, d->tw);
    d->prefix = prefix_for(d->tw, M);
    dtw_reset(d->D, d->L, d->C1);
}

void det_init(det_t *d)
{
    memset(d, 0, sizeof(*d));
    d->thr = det_params.def_thr;
    d->refractory_frames = (uint32_t)(REFRACTORY_S * DET_FS / DET_FRAME);
    d->floor_db = 30.0f;
}

float det_default_threshold(void) { return det_params.def_thr; }

bool det_set_template(det_t *d, const void *blob, size_t bytes)
{
    const det_tmpl_hdr_t *h = (const det_tmpl_hdr_t *)blob;
    d->have_tmpl = false;
    if (bytes < sizeof(*h) || h->magic != DET_TMPL_MAGIC || h->version != DET_TMPL_VERSION ||
        h->bands != DET_BANDS || h->len < 4 || h->len > DET_MAX_LEN || bytes != DET_TMPL_BYTES(h->len))
        return false;
    memcpy(&d->tmpl, blob, bytes);
    d->thr = h->thr_pct ? h->thr_pct / 100.0f : det_params.def_thr;
    derive(d);
    d->have_tmpl = true;
    return true;
}

size_t det_template_bytes(const det_t *d) { return DET_TMPL_BYTES(d->tmpl.h.len); }

void det_set_threshold(det_t *d, float thr)
{
    if (thr < 0.3f) thr = 0.3f;
    if (thr > 0.99f) thr = 0.99f;
    d->thr = thr;
}

void det_train_begin(det_t *d)
{
    d->train_state = 1;
    d->rounds = 0;
    d->rec_pos = 0;
    memset(d->pre, 0, sizeof(d->pre));
}

void det_train_abort(det_t *d) { d->train_state = 0; dtw_reset(d->D, d->L, d->C1); }
int  det_train_rounds(const det_t *d) { return d->rounds; }
bool det_train_recording(const det_t *d) { return d->train_state == 2; }

static void quant(const float *x, uint8_t *q)
{
    for (int b = 0; b < DET_BANDS; b++) {
        float v = x[b] / DET_QSTEP + 0.5f;
        q[b] = (uint8_t)(v > 255 ? 255 : v);
    }
}

/* A recorded "ring" counts only if it sounded for >= 0.4 s, peaked >= 20 dB above the background
 * and has >= 10 hops with a clear tonal/band peak (else it was a click or background noise). */
static bool ring_valid(const det_t *d)
{
    if (d->last_loud < (int)(0.4f * DET_FS / DET_FRAME) + 2 || d->ring_peak_db < d->floor_db + 20.0f) return false;
    int n = d->rec_pos < DET_MAX_LEN ? d->rec_pos : DET_MAX_LEN, na = 0;
    for (int i = 0; i < n; i++) {
        int mx = 0;
        for (int b = 0; b < DET_BANDS; b++) if (d->rec[d->rounds][i][b] > mx) mx = d->rec[d->rounds][i][b];
        na += mx * DET_QSTEP > det_params.act;
    }
    return na >= 10;
}

static void trim_lead(det_t *d);
static void trim_bursts(det_t *d);
static void close_ring(det_t *d)
{
    trim_lead(d);
    trim_bursts(d);
    int len = d->last_loud + 2;                       /* keep one quiet hop after the last sound */
    if (len > d->rec_pos) len = d->rec_pos;
    if (len > DET_MAX_LEN) len = DET_MAX_LEN;
    d->rec_len[d->rounds] = (uint16_t)len;
    d->rounds++;
}

/* Drop leading hops far (> end_rel_db) below the ring peak: the recording was started by
 * background noise before the real ring. Keeps 2 hops of lead-in. Only hops still in the buffer
 * (rec_pos <= DET_MAX_LEN) can be moved. */
static void trim_lead(det_t *d)
{
    if (d->rec_pos > DET_MAX_LEN) return;
    int n = d->rec_pos, k = 0;
    while (k < n && d->rec_dbq[k] < d->ring_peak_db - det_params.end_rel_db) k++;
    int sh = k - 2;
    if (sh <= 0) return;
    memmove(d->rec[d->rounds][0], d->rec[d->rounds][sh], (size_t)(n - sh) * DET_BANDS);
    memmove(d->rec_dbq, d->rec_dbq + sh, (size_t)(n - sh));
    d->rec_pos -= sh;
    d->last_loud -= sh;
}

/* Cut short bursts off the start or end of the recorded ring. Typical case: the bell push is near
 * the sender. Its click starts the recording and the chime follows 0.2-1.2 s later. Kept in the
 * template, the click and the gap are different on every press, and they weigh as much as the
 * chime in matching, so the template no longer matches the real chime (it can score 0.6-0.7
 * against every later ring although training succeeded).
 * The ring is split into parts at >= BURST_GAP quiet hops or at a new attack (level back up by
 * BURST_RISE_DB after falling that much below the part's peak: a loud click's echo can bridge the
 * gap). A first or last part is a burst (click, knock, door) if it sounds for <= BURST_MAX hops,
 * is loud (within BURST_RISE_DB of its peak) for <= BURST_STRONG hops and is <= 1/3 of the longest
 * part. Bell notes ring on for longer, and notes of a melody are not that much shorter than its
 * longest note, so they stay. Nothing is cut if less than 2 x BURST_MAX sounding hops would remain. */
#define BURST_GAP  3                                 /* hops (~0.1 s) of quiet between parts */
#define BURST_MAX  10                                /* sounding hops (~0.3 s) */
#define BURST_RISE_DB 10.0f                          /* or: a new attack out of a decay */
#define BURST_STRONG 5                               /* a click is loud for <= 5 hops (~0.16 s) */
#define MAX_PARTS  16
static void trim_bursts(det_t *d)
{
    const det_params_t *P = &det_params;
    float lim = d->floor_db + P->end_db;
    if (lim < d->ring_peak_db - P->end_rel_db) lim = d->ring_peak_db - P->end_rel_db;
    int n = d->last_loud + 1;
    if (n > DET_MAX_LEN) n = DET_MAX_LEN;
    if (n > d->rec_pos) n = d->rec_pos;
    int s0[MAX_PARTS], s1[MAX_PARTS], cnt[MAX_PARTS], np = 0, q = BURST_GAP, mx = 0;
    float pk = 0, vl = 1e9f;                          /* current part: peak level, lowest level since */
    for (int i = 2; i < n; i++) {
        float v = d->rec_dbq[i];
        if (v < vl && (np == 0 || v < pk)) vl = v;
        if (v <= lim) { q++; continue; }
        /* new part after a quiet gap, or a fresh attack >= BURST_RISE_DB out of a decay that had
         * fallen >= BURST_RISE_DB below the part's peak (a loud click's echo can bridge the gap) */
        bool fresh = np > 0 && vl <= pk - BURST_RISE_DB && v >= vl + BURST_RISE_DB;
        if ((q >= BURST_GAP || fresh) && np < MAX_PARTS) { s0[np] = i; cnt[np] = 0; np++; pk = v; vl = 1e9f; }
        if (v > pk && vl > 1e8f) pk = v;
        s1[np - 1] = i; cnt[np - 1]++; q = 0;
    }
    if (np < 2) return;
    int strong[MAX_PARTS];                            /* hops within BURST_RISE_DB of the part's peak */
    for (int k = 0; k < np; k++) {
        float p = 0;
        for (int i = s0[k]; i <= s1[k]; i++) if (d->rec_dbq[i] > p) p = d->rec_dbq[i];
        strong[k] = 0;
        for (int i = s0[k]; i <= s1[k]; i++) strong[k] += d->rec_dbq[i] >= p - BURST_RISE_DB;
    }
    for (int k = 0; k < np; k++) if (cnt[k] > mx) mx = cnt[k];
    int a = 0, b = np - 1;
#define IS_BURST(k) (cnt[k] <= BURST_MAX && strong[k] <= BURST_STRONG && 3 * cnt[k] <= mx)
    while (a < b && IS_BURST(a)) a++;
    if (d->last_loud < DET_MAX_LEN - 1)               /* (an overlong ring has no real end) */
        while (b > a && IS_BURST(b)) b--;
#undef IS_BURST
    int keep = 0;
    for (int k = a; k <= b; k++) keep += cnt[k];
    if (keep < 2 * BURST_MAX) return;                 /* too little left: a short or fragmented sound, keep it whole */
    if (b < np - 1) d->last_loud = s1[b];
    int sh = s0[a] - 2;                               /* keep 2 hops of lead-in */
    if (a == 0 || sh <= 0) return;
    int m = d->rec_pos < DET_MAX_LEN ? d->rec_pos : DET_MAX_LEN;
    memmove(d->rec[d->rounds][0], d->rec[d->rounds][sh], (size_t)(m - sh) * DET_BANDS);
    memmove(d->rec_dbq, d->rec_dbq + sh, (size_t)(m - sh));
    d->rec_pos -= sh;
    d->last_loud -= sh;
}

det_event_t det_process(det_t *d, const int16_t *pcm)
{
    const det_params_t *P = &det_params;
    float x[DET_BANDS], db;
    det_features(d, pcm, x, &db);
    d->last_db = db;

    if (d->train_state == 1) {                        /* waiting for a ring: a clear rise above the background */
        float lo = db;
        for (int i = 0; i < 16; i++) if (d->dbhist[i] < lo) lo = d->dbhist[i];
        memmove(d->dbhist, d->dbhist + 1, sizeof(float) * 15);
        d->dbhist[15] = db;
        if (db > d->floor_db + P->trig_db && db > lo + P->trig_db && d->nframes > 8 && d->rounds < DET_MAX_ROUNDS) {
            d->train_state = 2;
            d->rec_pos = 0;
            for (int i = 0; i < 2; i++) { d->rec_dbq[d->rec_pos] = 0; memcpy(d->rec[d->rounds][d->rec_pos++], d->pre[i], DET_BANDS); }
            quant(x, d->rec[d->rounds][d->rec_pos]);
            d->rec_dbq[d->rec_pos] = (uint8_t)(db < 0 ? 0 : db > 255 ? 255 : db);
            d->last_loud = d->rec_pos++;
            d->ring_peak_db = db;
            return DET_TRAIN_REC;
        }
        memcpy(d->pre[0], d->pre[1], DET_BANDS);
        quant(x, d->pre[1]);
        return DET_TRAIN_WAIT;
    }
    if (d->train_state == 2) {                        /* recording one ring until it has ended */
        if (db > d->ring_peak_db + P->trig_db && db > d->floor_db + P->trig_db) {
            /* far louder than anything so far: the start was background noise, the ring starts now */
            d->rec_pos = 0;
            for (int i = 0; i < 2; i++) { d->rec_dbq[d->rec_pos] = 0; memcpy(d->rec[d->rounds][d->rec_pos++], d->pre[i], DET_BANDS); }
            d->ring_peak_db = db;
        }
        if (db > d->ring_peak_db) d->ring_peak_db = db;
        if (d->rec_pos == DET_MAX_LEN) trim_lead(d);    /* buffer full: make room if the start was noise */
        if (d->rec_pos < DET_MAX_LEN) {
            quant(x, d->rec[d->rounds][d->rec_pos]);
            d->rec_dbq[d->rec_pos] = (uint8_t)(db < 0 ? 0 : db > 255 ? 255 : db);
        }
        memcpy(d->pre[0], d->pre[1], DET_BANDS);
        quant(x, d->pre[1]);
        /* still sounding: frame level clearly above the background and less than end_rel_db
         * below the ring's loudest hop (background activity or a long faint decay must not
         * extend the ring into the next one) */
        if (db > d->ring_peak_db) d->ring_peak_db = db;
        float lim = d->floor_db + P->end_db;
        if (lim < d->ring_peak_db - P->end_rel_db) lim = d->ring_peak_db - P->end_rel_db;
        if (db > lim) d->last_loud = d->rec_pos;
        d->rec_pos++;
        int quiet_hops = (int)(P->end_quiet_s * DET_FS / DET_FRAME + 0.5f);
        if (d->rec_pos - 1 - d->last_loud >= quiet_hops) {
            d->train_state = 1;
            memset(d->pre, 0, sizeof(d->pre));
            if (!ring_valid(d)) return DET_TRAIN_WAIT;   /* a click or background noise, not a ring */
            close_ring(d);
            return DET_TRAIN_ROUND_DONE;
        }
        if (d->rec_pos >= DET_MAX_LEN + 20 * DET_FS / DET_FRAME) {   /* never quiet: keep 8 s */
            d->last_loud = DET_MAX_LEN;
            close_ring(d);
            d->train_state = 3; d->quiet = 0;
            return DET_TRAIN_ROUND_DONE;
        }
        return DET_TRAIN_REC;
    }
    if (d->train_state == 3) {                        /* overlong ring: wait for quiet */
        d->quiet = (db < d->floor_db + P->end_db) ? d->quiet + 1 : 0;
        if (d->quiet >= 16) { d->train_state = 1; memset(d->pre, 0, sizeof(d->pre)); }
        return DET_TRAIN_WAIT;
    }

    d->last_score = 0;
    if (!d->have_tmpl) return DET_IDLE;
    if (d->refractory) { d->refractory--; return DET_IDLE; }
    const int M = d->tmpl.h.len;
    float c[DET_MAX_LEN], y[DET_BANDS];
    bool act;
    float yn = shape(x, y, &act);
    if (!act) yn = 0;
    for (int j = 0; j < M; j++) c[j] = cost(d->tq[j], d->tnorm[j], y, yn);
    int at = M - 1;
    float s = dtw_step(d->D, d->L, d->C1, c, d->tw, M, d->prefix, &at);
    d->last_score = s < 0 ? 0 : s;
    if (s >= d->thr) {
        /* lock out for the rest of the template plus REFRACTORY_S */
        d->refractory = d->refractory_frames + (uint32_t)(M - 1 - at);
        dtw_reset(d->D, d->L, d->C1);
        return DET_MATCH;
    }
    return DET_IDLE;
}

/* Score of recorded ring `b` against ring `a` used as template, with the same rule as live
 * detection (quiet padding after the ring). Uses d->tq/tnorm/tw as scratch. */
static float match_rings(det_t *d, int a, int b)
{
    static float D[2][DET_MAX_LEN], L[2][DET_MAX_LEN], C1[DET_MAX_LEN];
    float c[DET_MAX_LEN], x[DET_BANDS], y[DET_BANDS], best = 0;
    int M = d->rec_len[a], N = d->rec_len[b];
    shape_rows((const uint8_t (*)[DET_BANDS])d->rec[a], M, d->tq, d->tnorm, d->tw);
    dtw_reset(D, L, C1);
    int from = prefix_for(d->tw, M);
    for (int i = 0; i < N + 8; i++) {
        for (int k = 0; k < DET_BANDS; k++) x[k] = i < N ? d->rec[b][i][k] * DET_QSTEP : 0;
        bool act;
        float yn = shape(x, y, &act);
        if (!act) yn = 0;
        for (int j = 0; j < M; j++) c[j] = cost(d->tq[j], d->tnorm[j], y, yn);
        float s = dtw_step(D, L, C1, c, d->tw, M, from, NULL);
        if (s > best) best = s;
    }
    return best;
}

bool det_train_finish(det_t *d)
{
    if (d->train_state == 2 && ring_valid(d)) close_ring(d);   /* ring still sounding at the end */
    d->train_state = 0;
    if (d->rounds < 1) return false;
    const det_params_t *P = &det_params;
    int R = d->rounds, med = 0;
    float S[DET_MAX_ROUNDS][DET_MAX_ROUNDS] = {{0}};
    for (int a = 0; a < R; a++)
        for (int b = 0; b < R; b++)
            if (a != b) S[a][b] = match_rings(d, a, b);
    float bestsum = -1;
    for (int a = 0; a < R; a++) {                      /* medoid: the most typical ring */
        float s = 0;
        for (int b = 0; b < R; b++) if (a != b) s += S[a][b];
        if (s > bestsum) { bestsum = s; med = a; }
    }
    float thr = P->def_thr;
    for (int b = 0; b < R; b++) d->train_scores[b] = b == med ? 1.0f : S[med][b];
    memset(&d->tmpl.h, 0, sizeof(d->tmpl.h));
    d->tmpl.h.magic = DET_TMPL_MAGIC;
    d->tmpl.h.version = DET_TMPL_VERSION;
    d->tmpl.h.bands = DET_BANDS;
    d->tmpl.h.len = d->rec_len[med] < 4 ? 4 : d->rec_len[med];
    d->tmpl.h.thr_pct = (uint8_t)(thr * 100.0f + 0.5f);
    d->tmpl.h.rounds = (uint8_t)R;
    memcpy(d->tmpl.f, d->rec[med], (size_t)d->tmpl.h.len * DET_BANDS);
    /* Average the other rings into the template (a ring that does not resemble the medoid at
     * all, score < 0.5, was stray noise and is left out). Rings of one bell keep their tempo, so
     * a time offset of up to +-12 hops is enough to line them up. */
    static uint8_t cnt[DET_MAX_LEN];
    int M = d->tmpl.h.len;
    for (int j = 0; j < M; j++) cnt[j] = 1;
    for (int b = 0; b < R; b++) {
        if (b == med || S[med][b] < 0.5f) continue;
        int N = d->rec_len[b], bo = 0;
        float bs = -1e9f;
        for (int o = -12; o <= 12; o++) {
            float s = 0;
            for (int j = 0; j < M; j++) {
                int i = j + o;
                if (i < 0 || i >= N) continue;
                float dp = 0, na = 0, nb = 0;
                for (int k = 0; k < DET_BANDS; k++) { float a = d->tmpl.f[j][k], c = d->rec[b][i][k]; dp += a * c; na += a * a; nb += c * c; }
                if (na > 0 && nb > 0) s += dp / sqrtf(na * nb);
            }
            if (s > bs) { bs = s; bo = o; }
        }
        for (int j = 0; j < M; j++) {
            int i = j + bo;
            if (i < 0 || i >= N) continue;
            for (int k = 0; k < DET_BANDS; k++)
                d->tmpl.f[j][k] = (uint8_t)((d->tmpl.f[j][k] * cnt[j] + d->rec[b][i][k] + cnt[j] / 2) / (cnt[j] + 1));
            cnt[j]++;
        }
    }
    d->thr = d->tmpl.h.thr_pct / 100.0f;
    derive(d);
    d->have_tmpl = true;
    d->refractory = 0;
    return true;
}
