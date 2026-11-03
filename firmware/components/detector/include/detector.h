// Trainable doorbell detector, v2. Pure C, no ESP-IDF dependency (host-testable).
//
// Audio: 16 kHz mono int16, fed in hops of DET_FRAME (512) samples = 32 ms; analysis window
// 1024 samples (64 ms, Hann), so tones a semitone apart are resolved above ~300 Hz.
//
// Feature per hop: DET_BANDS semitone bands (300 Hz .. ~6.6 kHz). Each band holds the strongest
// FFT bin in that band, in dB ABOVE that band's own adaptive background floor (0 = background).
// Tonal sounds (bell partials) give a few sharp, stable peaks; speech and noise give broad or
// moving patterns. Level above the background is kept, so pauses and decays are information.
//
// Template: the whole ring, from onset until the sound has been quiet for DET_END_QUIET_S
// (pauses inside a melody shorter than that stay inside one ring), up to DET_MAX_LEN hops (8 s).
// Training records up to 3 rings and stores the most typical one (medoid).
//
// Matching: streaming subsequence DTW (template must be matched from its first hop, live audio
// may start anywhere), local slope 1/2..2, frame similarity = cosine of the band vectors, pause
// matches pause. Score = 1 - mean path cost. Detection when the score at the template end (or,
// for templates longer than DET_PREFIX_S, at any point after that) reaches the threshold.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DET_FS            16000
#define DET_FRAME         512                  /* hop: 32 ms */
#define DET_WIN           1024                 /* analysis window: 64 ms */
#define DET_BANDS         54                   /* semitone bands from DET_LO_HZ */
#define DET_LO_HZ         300.0f
#define DET_MAX_LEN       250                  /* max template hops (8.0 s) */
#define DET_MAX_ROUNDS    3
#define DET_TMPL_VERSION  2
#define DET_TMPL_MAGIC    0x7442u
#define DET_QSTEP         0.5f                 /* dB per step in stored (uint8) features */

/* Stored template (NVS blob = header + len * DET_BANDS bytes). */
typedef struct {
    uint16_t magic;                            /* DET_TMPL_MAGIC */
    uint8_t  version;                          /* DET_TMPL_VERSION */
    uint8_t  bands;                            /* DET_BANDS */
    uint16_t len;                              /* hops in template */
    uint8_t  thr_pct;                          /* threshold in percent at training time */
    uint8_t  rounds;                           /* rings used for training */
} det_tmpl_hdr_t;

typedef struct {
    det_tmpl_hdr_t h;
    uint8_t f[DET_MAX_LEN][DET_BANDS];         /* dB above band floor, DET_QSTEP steps */
} det_template_t;

#define DET_TMPL_BYTES(len) (sizeof(det_tmpl_hdr_t) + (size_t)(len) * DET_BANDS)

/* Tunables (defaults in detector.c; host tools may override them for evaluation). */
typedef struct {
    float margin_db, act, range_db, pk_w, w_pause, pause_qa, prefix_s, def_thr;
    float floor_up, floor_near, trig_db, end_db, end_rel_db, end_quiet_s;
} det_params_t;
extern det_params_t det_params;

typedef enum { DET_IDLE = 0, DET_MATCH, DET_TRAIN_WAIT, DET_TRAIN_REC, DET_TRAIN_ROUND_DONE } det_event_t;

typedef struct {
    det_template_t tmpl;
    bool     have_tmpl;
    float    thr;
    /* derived from the template */
    float    tnorm[DET_MAX_LEN];
    uint8_t  tq[DET_MAX_LEN][DET_BANDS];       /* shaped template (see detector.c shape()) */
    float    tw[DET_MAX_LEN];                  /* DTW weight per template hop (pause hops weigh less) */
    int      prefix;                           /* earliest template index where a match may fire */
    /* live state */
    int16_t  prev[DET_FRAME];                  /* previous hop (window = prev + current) */
    float    bfloor[DET_BANDS];                /* per-band background floor, dB */
    float    floor_db;                         /* frame-level floor, dB */
    float    last_db, last_score;
    uint32_t nframes;
    uint32_t refractory, refractory_frames;
    /* streaming DTW: [0] = live hop t-1, [1] = t-2 */
    float    D[2][DET_MAX_LEN];
    float    L[2][DET_MAX_LEN];
    float    C1[DET_MAX_LEN];
    /* training */
    int      train_state;                      /* 0 off, 1 waiting, 2 recording, 3 cooldown */
    int      rounds, rec_pos, last_loud, quiet;
    float    ring_peak_db;
    float    dbhist[16];                       /* recent frame levels (training onset = a rise) */
    uint8_t  pre[2][DET_BANDS];
    uint8_t  rec_dbq[DET_MAX_LEN];             /* frame level (dB) of the ring being recorded */
    uint8_t  rec[DET_MAX_ROUNDS][DET_MAX_LEN][DET_BANDS];
    uint16_t rec_len[DET_MAX_ROUNDS];
    float    train_scores[DET_MAX_ROUNDS];     /* other rings vs. chosen template */
} det_t;

void  det_init(det_t *d);
/* Load a stored template blob of `bytes` bytes. Returns false (no template loaded) for another
 * version/format: the caller should erase it and ask the user to train again. */
bool  det_set_template(det_t *d, const void *blob, size_t bytes);
void  det_set_threshold(det_t *d, float thr);
/* Feed exactly DET_FRAME samples. Returns DET_MATCH on detection, DET_TRAIN_* during training. */
det_event_t det_process(det_t *d, const int16_t *pcm);
/* Training: call begin, feed audio; each ring gives DET_TRAIN_ROUND_DONE once it has ended.
 * Call det_train_finish() (after >= 1 ring) to build the template (threshold = default). */
void  det_train_begin(det_t *d);
void  det_train_abort(det_t *d);
int   det_train_rounds(const det_t *d);
bool  det_train_recording(const det_t *d);
bool  det_train_finish(det_t *d);
/* Template blob for NVS: &d->tmpl, det_template_bytes(d) bytes. */
size_t det_template_bytes(const det_t *d);
float det_default_threshold(void);
/* exposed for tests and tools */
void  det_features(det_t *d, const int16_t *pcm, float feat[DET_BANDS], float *frame_db);
