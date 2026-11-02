# Detector evaluation set

Offline evaluation of the doorbell detector on real recordings. Results: [../EVALUATION.md](../EVALUATION.md).

## Reproduce

Needs curl, ffmpeg, python3 and (for the set builder) numpy + scipy, e.g. via `uv`.

    python3 test/eval/fetch.py                                          # ~1 GB into test/eval/data/
    uv run --with numpy --with scipy python test/eval/build_set.py      # renders data/set (deterministic)
    make -C test/host wavtool
    python3 test/eval/run_eval.py --md results.md                       # a few minutes
    python3 test/eval/run_eval.py -t 0.99 --sweep                       # detection / false triggers per fixed threshold

`BELL_EVAL_DATA=/path` puts the data elsewhere. `run_eval.py -P name=value` overrides detector
tunables (see `det_params` in `components/detector/detector.c`) for experiments.

Private recordings (`sources.json`, `local`) are not downloadable and not in the repository: put
them as 16 kHz mono s16 WAV into `data/src/local/` under the listed file name (convert with
`ffmpeg -i in.m4a -ar 16000 -ac 1 -sample_fmt s16 out.wav`). Missing ones are skipped. Everything
added after the first version of the set (local bells, the `click` condition, the transient
negatives) uses its own random generator, so the files of the first version stay bit-identical.

## What the set contains

- **Doorbells** (`sources.json`, kind `bell`): 11 recordings from Freesound (ding-dongs, single
  strikes, a chime cluster, a mechanical bell, two buzzers, two electronic Westminster melodies of
  8.4 s and 11.2 s with pauses between the notes), plus 3 generated melodies (3 to 8 notes, 4.6 to
  8 s, up to 1.2 s silence between notes). Every bell has 3 to 7 rings.
- **Local bells** (`sources.json`, `local`): private recordings of real doorbells, see above.
  `home-bell`: a real front-door chime (electronic, 3 tones played twice, 1.4 s), one phone
  recording, so its "held-out" rings are copies of that one ring with new room, level and noise.
- **Leave-one-ring-out**: for each ring, a training recording with 3 other rings of the same bell
  (room reverb, quiet room noise, 4 s apart), and 6 test clips of the held-out ring (level +-6 dB,
  new reverb, household or TV background at 10-30 dB SNR). The generated melodies, the two
  Westminster recordings and `home-bell` have one recording each: their folds reuse it.
- **Three conditions**: `direct` (the bell itself), `speaker` (played through a small phone or
  laptop speaker: 450 Hz-6.5 kHz band limit, resonance, soft clipping), like training from a video,
  and `click` (the bell with the click of the bell push 0.2-1.2 s before the chime, at -6..+12 dB
  against the chime's peak, in training and test: press + release click, random resonances). In
  `click` a trigger on the click itself, before the chime, counts as a false trigger.
- **Negatives**, each run against every trained template:
  - core: LibriSpeech speech, TV / radio / conversation and kitchen / household recordings from
    Freesound, ESC-50 domestic and human non-speech classes (~173 min);
  - transient: Freesound CC0 recordings of keys, light switches, doors, push buttons, footsteps,
    knocks and claps, plus generated bell-push clicks (~21 min); `run_eval.py` also lists the
    false triggers in the 8 ESC-50 transient classes inside core (knock, click, typing, clapping,
    footsteps, can opening, door creak, glass breaking, ~27 min);
  - outdoor: ESC-50 animals, nature, urban (sirens, horns, engines) (~97 min);
  - hard: ring tones, other bells and chimes, alarm clocks, church bells (~23 min).

## Licences

The audio is **not** in the repository and must not be published with it. `fetch.py` downloads
it into `test/eval/data/` (git-ignored). Per-item source, author, licence and URL are in
`sources.json`.

| Source | Licence |
|---|---|
| Freesound items (public previews) | CC0 (82 items), CC BY (19), CC BY-NC (4): see `sources.json` |
| LibriSpeech dev-clean (openslr.org/12) | CC BY 4.0 |
| ESC-50 (github.com/karolpiczak/ESC-50) | CC BY-NC 3.0 (clips from Freesound) |
| Local recordings | private, never published |

NonCommercial items are used only for internal testing and are never redistributed.
