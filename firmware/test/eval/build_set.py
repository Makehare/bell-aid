#!/usr/bin/env python3
"""Build the detector evaluation set from data/src (see fetch.py) into data/set.

    uv run --with numpy --with scipy python test/eval/build_set.py

Deterministic (fixed seed). For every doorbell and every ring of it (leave-one-ring-out):
  * a training recording: 3 OTHER rings of the same bell, rendered in the bell's room
    (reverb, level) over quiet room noise, 4 s apart - what the device hears during training;
  * test clips: the held-out ring, rendered RENDERS times (level +-6 dB, new reverb tail,
    household/TV background at 10-30 dB SNR), 5 s of background before, 3 s after.
Two conditions: "direct" (the bell itself) and "speaker" (the same bell played through a small
phone/laptop speaker: 450 Hz-6.5 kHz band limit, resonance, soft clipping), like training from a
video. Negatives: speech, TV/radio/conversation, household noise (60 s clips, random level and
reverb), plus hard negatives (ring tones, other bells/chimes, alarm clocks) reported separately.
"""
import json, os, sys, wave
import numpy as np
from scipy import signal

FS = 16000
HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.environ.get("BELL_EVAL_DATA", os.path.join(HERE, "data"))
SRC, OUT = os.path.join(DATA, "src"), os.path.join(DATA, "set")
RENDERS = 6
SEED = 20261101


def load(p):
    with wave.open(p) as w:
        assert w.getframerate() == FS and w.getnchannels() == 1 and w.getsampwidth() == 2, p
        return np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float32)


def save(p, x):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    y = np.clip(np.round(x), -32768, 32767).astype(np.int16)
    with wave.open(p, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(FS); w.writeframes(y.tobytes())


def frame_db(x, n=256):
    k = len(x) // n
    f = x[:k * n].reshape(k, n)
    return 10 * np.log10(np.mean(f * f, axis=1) + 1e-3)


def active_rms(x):
    """RMS of the loudest 30 % of 32 ms frames (level of the sound itself, not of its pauses)."""
    k = len(x) // 512
    e = np.mean(x[:k * 512].reshape(k, 512) ** 2, axis=1)
    e = np.sort(e)[::-1][:max(1, int(0.3 * k))]
    return float(np.sqrt(np.mean(e)) + 1e-6)


def refine_onset(x, t):
    """Sharpest energy rise within +-0.4 s of t; returns segment start 50 ms before it."""
    db = frame_db(x)
    hop = 256 / FS
    a, b = max(1, int((t - 0.4) / hop)), min(len(db) - 1, int((t + 0.4) / hop))
    best, bi = -1e9, a
    for i in range(a, b):
        rise = db[i] - np.min(db[max(0, i - 20):i])
        if rise > best + 3:
            best, bi = rise, i
    return max(0.0, bi * hop - 0.05)


def rir(rng, rt60, drr_db):
    n = int(rt60 * FS)
    t = np.arange(n) / FS
    tail = rng.standard_normal(n) * np.exp(-6.9 * t / rt60)
    tail[: int(0.004 * FS)] = 0
    tail = signal.sosfilt(signal.butter(1, 4000, "low", fs=FS, output="sos"), tail)
    tail *= np.sqrt(10 ** (-drr_db / 10) / np.sum(tail ** 2))
    tail[0] += 1.0
    return tail


def reverb(x, h):
    return signal.fftconvolve(x, h)[: len(x) + len(h) // 2]


SPK_SOS = np.vstack([signal.butter(4, 450, "high", fs=FS, output="sos"),
                     signal.butter(4, 6500, "low", fs=FS, output="sos")])


def speaker(x):
    y = signal.sosfilt(SPK_SOS, x)
    b, a = signal.iirpeak(2500, 2.0, fs=FS)
    y = y + 0.6 * signal.lfilter(b, a, y)
    p = np.max(np.abs(y)) + 1e-6
    return np.tanh(1.8 * y / p) * p / np.tanh(1.8)


def pink(rng, n):
    w = rng.standard_normal(n)
    b, a = [0.049922035, -0.095993537, 0.050612699, -0.004408786], [1, -2.494956002, 2.017265875, -0.522189400]
    return signal.lfilter(b, a, w)


class Backgrounds:
    def __init__(self, rng, files):
        self.rng, self.clips = rng, [load(f) for f in files]

    def get(self, n):
        c = self.clips[self.rng.integers(len(self.clips))]
        if len(c) <= n:
            c = np.tile(c, n // len(c) + 1)
        s = self.rng.integers(0, len(c) - n + 1)
        return c[s:s + n].copy()


def room_tone(rng, n, level_db):
    x = pink(rng, n)
    return x / (np.sqrt(np.mean(x * x)) + 1e-9) * 10 ** (level_db / 20) * 32768


def at_level(x, db):
    return x / active_rms(x) * 10 ** (db / 20) * 32768


# ---- synthetic melodies (electronic chimes): identical every ring, like a real electronic bell ----
def chime_note(f, dur, decay):
    t = np.arange(int(dur * FS)) / FS
    env = np.exp(-t / decay) * np.minimum(1, t / 0.004)
    return env * sum(a * np.sin(2 * np.pi * f * m * t) for m, a in ((1, 1), (2.0, .45), (3.0, .25), (4.2, .12)))


def melody(notes, total):
    y = np.zeros(int(total * FS))
    for t0, f, dur, dec in notes:
        n = chime_note(f, dur, dec)
        i = int(t0 * FS)
        y[i:i + len(n)] += n[: len(y) - i]
    return y


SYNTH = {
    # 8 short notes with a 1 s pause in the middle, ~5.5 s
    "synth-tune8": melody([(0.0, 784, 0.8, .35), (0.5, 659, 0.8, .35), (1.0, 698, 0.8, .35), (1.5, 523, 1.2, .5),
                           (3.0, 523, 0.8, .35), (3.5, 698, 0.8, .35), (4.0, 784, 0.8, .35), (4.5, 659, 1.5, .6)], 6.2),
    # Westminster-like, two 4-note phrases of sustained notes with a 1.2 s pause, ~7.5 s
    "synth-westminster": melody([(0.0, 659, 1.6, 1.2), (0.7, 523, 1.6, 1.2), (1.4, 587, 1.6, 1.2), (2.1, 392, 2.0, 1.2),
                                 (4.3, 392, 1.6, 1.2), (5.0, 587, 1.6, 1.2), (5.7, 659, 1.6, 1.2), (6.4, 523, 2.0, 1.4)], 8.0),
    # three slow notes with ~1.2 s silent gaps, ~4.6 s
    "synth-slow3": melody([(0.0, 880, 0.5, .12), (1.6, 740, 0.5, .12), (3.2, 587, 1.2, .3)], 4.6),
}


def push_click(rng):
    """Bell-push click as the sender hears it when the push is near it: a press click (impulse with
    5-15 ms decay through 1-2 random resonances 1.5-6 kHz) and a weaker release click 0.08-0.4 s
    later. Different for every press."""
    def one():
        n = int(0.06 * FS)
        t = np.arange(n) / FS
        x = rng.standard_normal(n) * np.exp(-t / rng.uniform(0.005, 0.015))
        y = 0.3 * x
        for _ in range(int(rng.integers(1, 3))):
            b, a = signal.iirpeak(rng.uniform(1500, 6000), rng.uniform(4, 15), fs=FS)
            y = y + signal.lfilter(b, a, x)
        return y
    rel = rng.uniform(0.08, 0.4)
    out = np.zeros(int((rel + 0.1) * FS))
    p = one(); out[:len(p)] += p
    r = one() * 10 ** (-rng.uniform(3, 9) / 20)
    i = int(rel * FS); out[i:i + len(r)] += r
    return out


def with_click(rng, ring):
    """Condition "click": the push click 0.2-1.2 s before the chime (wireless / electronic chimes
    start late), click peak -6..+12 dB against the ring's peak. Returns (audio, chime delay s)."""
    c = push_click(rng)
    c *= np.max(np.abs(ring)) / (np.max(np.abs(c)) + 1e-9) * 10 ** (rng.uniform(-6, 12) / 20)
    d = rng.uniform(0.2, 1.2)
    y = np.zeros(max(len(c), int(d * FS) + len(ring)), np.float64)
    y[:len(c)] += c
    y[int(d * FS):int(d * FS) + len(ring)] += ring
    return y, d


def render_cond(rng, b, room, cond, manifest, bgs, quiet):
    """Leave-one-ring-out training recordings + test clips of bell b in one condition."""
    N = len(b["rings"])
    for h in range(N):
        others = [r for k, r in enumerate(b["rings"]) if k != h]
        tr = [others[k % len(others)] for k in range(3)]
        parts = []
        for r in tr:
            y = speaker(r) if cond == "speaker" else with_click(rng, r)[0] if cond == "click" else r
            y = at_level(reverb(y, rir(rng, room["rt60"], room["drr"])), room["level"] + rng.uniform(-3, 3))
            parts += [None, y]
        parts.append(None)
        gap = int(4.0 * FS)
        total = sum(gap if p is None else len(p) for p in parts)
        lvl_q = room["level"] - rng.uniform(32, 40)
        scene = room_tone(rng, total, lvl_q - 6)
        qb = quiet.get(total)
        scene += qb / (np.sqrt(np.mean(qb * qb)) + 1e-9) * 10 ** (lvl_q / 20) * 32768
        pos = 0
        for p in parts:
            if p is None:
                pos += gap
            else:
                scene[pos:pos + len(p)] += p; pos += len(p)
        base = f"{b['name']}/{cond}/fold{h}"
        trp = os.path.join(OUT, "pos", base, "train.wav")
        save(trp, scene)
        tests = []
        ring = b["rings"][h]
        for r in range(RENDERS):
            d = 0.0
            if cond == "click":
                y, d = with_click(rng, ring)
            else:
                y = ring if cond == "direct" else speaker(ring)
            y = reverb(y, rir(rng, room["rt60"] * rng.uniform(0.9, 1.1), room["drr"] + rng.uniform(-2, 2)))
            lvl = room["level"] + rng.uniform(-6, 6)
            y = at_level(y, lvl)
            snr = rng.uniform(10, 30)
            pre, post = int(5 * FS), int(3 * FS)
            n = pre + len(y) + post
            bg = bgs.get(n)
            bg = bg / (active_rms(bg) + 1e-9) * 10 ** ((lvl - snr) / 20) * 32768
            clip = bg + room_tone(rng, n, lvl - 45)
            clip[pre:pre + len(y)] += y
            p = os.path.join(OUT, "pos", base, f"test{r}.wav")
            save(p, clip)
            # "click": the ring window starts at the chime; firing on the click itself is a false trigger
            tests.append(dict(path=os.path.relpath(p, OUT), t0=5.0 + round(d, 3), t1=5.0 + len(y) / FS + 1.0, snr=round(snr, 1)))
        manifest["experiments"].append(dict(bell=b["name"], kind=b["kind"], source=b["source"], license=b["license"],
                                            synthetic=b["synthetic"], identical=b["identical"], cond=cond, fold=h, rings=N,
                                            ring_s=round(len(ring) / FS, 2),
                                            train=os.path.relpath(trp, OUT), tests=tests))


def main():
    rng = np.random.default_rng(SEED)
    S = json.load(open(os.path.join(HERE, "sources.json")))
    fs = {it["id"]: it for it in S["freesound"]}
    path = lambda i: os.path.join(SRC, "fs", f"{i}.wav")
    bg_house = [path(i) for i, it in fs.items() if it["role"] == "neg_household"]
    bg_tv = [path(i) for i, it in fs.items() if it["role"] == "neg_tv"]
    bgs = Backgrounds(rng, bg_house + bg_tv)
    quiet = Backgrounds(rng, bg_house)

    # ---- doorbell rings ----
    bells = []
    for i, it in fs.items():
        if it["role"] != "bell":
            continue
        x = load(path(i))
        on = [refine_onset(x, t) for t in it["rings"]]
        rings = []
        for k, t in enumerate(on):
            end = min(len(x) / FS, t + it.get("ring_s", 9.0), (on[k + 1] - 0.1) if k + 1 < len(on) else 1e9)
            rings.append(x[int(t * FS):int(end * FS)])
        identical = len(rings) == 1                  # electronic chime: every ring is the same audio
        if identical:
            rings = rings * 4
        bells.append(dict(name=it["name"], kind=it["kind"], source=f"freesound {i}", license=it["license"], rings=rings,
                          synthetic=False, identical=identical))
    for name, y in SYNTH.items():
        bells.append(dict(name=name, kind="synthetic melody", source="generated", license="-", rings=[y] * 4, synthetic=True, identical=True))

    manifest = dict(seed=SEED, renders=RENDERS, experiments=[], negatives=[])
    for b in bells:
        room = dict(rt60=float(rng.uniform(0.25, 0.7)), drr=float(rng.uniform(0, 10)), level=float(rng.uniform(-30, -16)))
        for cond in ("direct", "speaker"):
            render_cond(rng, b, room, cond, manifest, bgs, quiet)
        print("bell", b["name"], len(b["rings"]), "rings", [round(len(r) / FS, 1) for r in b["rings"]])

    # ---- negatives (60 s clips) ----
    def neg_clip(cat, x, name, parts=None, rng=rng):
        x = reverb(x, rir(rng, rng.uniform(0.25, 0.7), rng.uniform(0, 10)))
        lvl = rng.uniform(-34, -14)
        y = at_level(x, lvl) + room_tone(rng, len(x), lvl - 45)
        p = os.path.join(OUT, "neg", cat, name + ".wav")
        save(p, y)
        manifest["negatives"].append(dict(cat=cat, path=os.path.relpath(p, OUT), dur=round(len(y) / FS, 1), **({"parts": parts} if parts else {})))

    def chunks(x, n=60 * FS):
        for s in range(0, len(x), n):
            c = x[s:s + n]
            if len(c) >= 10 * FS:
                yield s // n, c

    for i, it in fs.items():
        cat = {"neg_tv": "tv", "neg_household": "household", "neg_ringtone": "hard_ringtone", "neg_otherbell": "hard_otherbell"}.get(it["role"])
        if not cat:
            continue
        x = load(path(i))
        if cat == "household":
            x = x[: 300 * FS]                       # at most 5 min per recording
        for k, c in chunks(x):
            neg_clip(cat, c, f"{i}_{k}")
    # speech: LibriSpeech utterances with pauses, 60 s clips
    ld = os.path.join(SRC, "libri")
    utt = [load(os.path.join(ld, f)) for f in sorted(os.listdir(ld))]
    cur, k, total = [], 0, 0
    for u in utt:
        cur += [u, np.zeros(int(rng.uniform(0.2, 1.5) * FS), np.float32)]
        if sum(len(c) for c in cur) >= 60 * FS:
            neg_clip("speech", np.concatenate(cur), f"libri_{k}"); k += 1; total += 60; cur = []
        if total >= S["librispeech"]["minutes"] * 60:
            break
    # ESC-50: 12 clips of 5 s per 60 s file; bell-like classes go to the hard set
    import csv
    meta = list(csv.DictReader(open(os.path.join(DATA, "dl", "ESC-50-master", "meta", "esc50.csv"))))
    groups = {"everyday_esc50": [], "hard_alarm_bells": []}
    for m in meta:
        g = "hard_alarm_bells" if m["category"] in ("church_bells", "clock_alarm") else "everyday_esc50"
        groups[g].append(os.path.join(SRC, "esc50", m["filename"]))
    for g, files in groups.items():
        rng.shuffle(files)
        for k in range(0, len(files), 12):
            x = np.concatenate([load(f) for f in files[k:k + 12]])
            neg_clip(g, x, f"esc50_{k // 12}", [os.path.basename(f) for f in files[k:k + 12]])

    # ---- additions (own RNG, so the files above stay bit-identical) ----
    rng2 = np.random.default_rng(SEED + 1)
    # private local recordings (not downloadable, skipped if missing): data/src/local/<file>
    local = []
    for it in S.get("local", []):
        p = os.path.join(SRC, "local", it["file"])
        if not os.path.exists(p):
            print("local bell missing, skipped:", p)
            continue
        x = load(p)
        on = [refine_onset(x, t) for t in it["rings"]]
        rings = [x[int(t * FS):int(min(len(x) / FS, t + it["ring_s"]) * FS)] for t in on]
        identical = len(rings) == 1
        local.append(dict(name=it["name"], kind=it["kind"], source="local", license=it["license"],
                          rings=rings * 4 if identical else rings, synthetic=False, identical=identical))
    for b in local:
        room = dict(rt60=float(rng2.uniform(0.25, 0.7)), drr=float(rng2.uniform(0, 10)), level=float(rng2.uniform(-30, -16)))
        for cond in ("direct", "speaker"):
            render_cond(rng2, b, room, cond, manifest, bgs, quiet)
        print("local bell", b["name"], len(b["rings"]), "rings")
    # condition "click": every bell trained and tested with a bell-push click before the chime
    for b in bells + local:
        room = dict(rt60=float(rng2.uniform(0.25, 0.7)), drr=float(rng2.uniform(0, 10)), level=float(rng2.uniform(-30, -16)))
        render_cond(rng2, b, room, "click", manifest, bgs, quiet)
    # transient negatives: keys, light switches, doors, push buttons, footsteps, knocks, claps (Freesound
    # CC0, all recordings of a kind joined, 60 s clips), plus synthetic bell-push clicks
    kinds = {}
    for i, it in fs.items():
        if it["role"] == "neg_transient":
            x = load(path(i))
            kinds.setdefault(it["kind"], []).append(x - np.mean(x))
    for kind, xs in sorted(kinds.items()):
        x = np.concatenate([np.concatenate([c, np.zeros(int(0.5 * FS), np.float32)]) for c in xs])
        for k, c in chunks(x):
            neg_clip("transient", c, f"{kind.replace(' ', '_').replace('/', '_')}_{k}", rng=rng2)
    for k in range(5):
        x = np.zeros(60 * FS)
        t = rng2.uniform(0.3, 2.0)
        while t < 59:
            c = push_click(rng2) * rng2.uniform(0.3, 1.0) * 8000
            i = int(t * FS); x[i:i + len(c)] += c[: len(x) - i]
            t += rng2.uniform(0.6, 4.0)
        neg_clip("transient", x, f"push_clicks_{k}", rng=rng2)
    tot = {}
    for n in manifest["negatives"]:
        tot[n["cat"]] = tot.get(n["cat"], 0) + n["dur"]
    print("negatives (min):", {k: round(v / 60, 1) for k, v in tot.items()})
    json.dump(manifest, open(os.path.join(OUT, "manifest.json"), "w"), indent=1)


if __name__ == "__main__":
    sys.exit(main())
