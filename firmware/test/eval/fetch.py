#!/usr/bin/env python3
"""Download the detector evaluation audio into test/eval/data/ (git-ignored, never published).

    python3 test/eval/fetch.py            # needs curl + ffmpeg; ~1 GB download

Sources and licences: test/eval/sources.json and test/eval/README.md. Freesound items are fetched
as the public 128 kbps preview (no account needed). Everything is converted to 16 kHz mono s16 WAV
in data/src/.
"""
import json, os, subprocess, sys, tarfile, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.environ.get("BELL_EVAL_DATA", os.path.join(HERE, "data"))
DL, SRC = os.path.join(DATA, "dl"), os.path.join(DATA, "src")


def run(*cmd):
    subprocess.run(cmd, check=True)


def to_wav(src, dst, extra=()):
    if os.path.exists(dst):
        return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    run("ffmpeg", "-loglevel", "error", "-y", "-i", src, *extra, "-ar", "16000", "-ac", "1",
        "-sample_fmt", "s16", "-bitexact", "-map_metadata", "-1", dst)


def fetch(url, dst):
    if os.path.exists(dst):
        return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    run("curl", "-sSL", "--fail", "-A", "Mozilla/5.0", "-o", dst + ".part", url)
    os.replace(dst + ".part", dst)


def main():
    S = json.load(open(os.path.join(HERE, "sources.json")))
    for it in S["freesound"]:
        mp3 = os.path.join(DL, "fs", f"{it['id']}.mp3")
        fetch(it["preview"], mp3)
        to_wav(mp3, os.path.join(SRC, "fs", f"{it['id']}.wav"))
        print("freesound", it["id"], it["license"], it["title"])
    # LibriSpeech dev-clean (CC BY 4.0): first utterances of every speaker, ~S['librispeech']['minutes']
    tgz = os.path.join(DL, "dev-clean.tar.gz")
    fetch(S["librispeech"]["url"], tgz)
    if not os.path.isdir(os.path.join(DL, "LibriSpeech")):
        with tarfile.open(tgz) as t:
            t.extractall(DL, filter="data")
    root = os.path.join(DL, "LibriSpeech", "dev-clean")
    for spk in sorted(os.listdir(root)):
        ch = sorted(os.listdir(os.path.join(root, spk)))[0]
        fl = sorted(f for f in os.listdir(os.path.join(root, spk, ch)) if f.endswith(".flac"))[:3]
        for f in fl:
            to_wav(os.path.join(root, spk, ch, f), os.path.join(SRC, "libri", f[:-5] + ".wav"))
    print("librispeech", len(os.listdir(os.path.join(SRC, "libri"))), "utterances")
    # ESC-50 (CC BY-NC 3.0)
    z = os.path.join(DL, "esc50.zip")
    fetch(S["esc50"]["url"], z)
    if not os.path.isdir(os.path.join(DL, "ESC-50-master")):
        with zipfile.ZipFile(z) as f:
            f.extractall(DL, [n for n in f.namelist() if "/audio/" in n or "/meta/" in n or n.endswith(("LICENSE", "README.md"))])
    adir = os.path.join(DL, "ESC-50-master", "audio")
    for f in sorted(os.listdir(adir)):
        to_wav(os.path.join(adir, f), os.path.join(SRC, "esc50", f))
    print("esc50", len(os.listdir(os.path.join(SRC, "esc50"))), "clips")


if __name__ == "__main__":
    sys.exit(main())
