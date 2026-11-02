#!/usr/bin/env python3
"""Host test of the capture path: firmware line format (via fake_sender, built from the same
dbn_common.c as the firmware) -> pseudo serial port -> tools/record.py -> WAV.

    make -C test/host test        (runs this too)
"""
import os, pty, subprocess, sys, tempfile, wave, struct, math, tty

HERE = os.path.dirname(os.path.abspath(__file__))
RECORD = os.path.join(HERE, "..", "..", "tools", "record.py")
fails = 0


def check(c, msg):
    global fails
    if not c:
        print("FAIL:", msg); fails += 1


def make_wav(path, secs):
    n = secs * 16000
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000)
        w.writeframes(b"".join(struct.pack("<h", int(8000 * math.sin(2 * math.pi * 440 * i / 16000)) + (i % 7) - 3) for i in range(n)))


def run(src, secs, drop=-1, corrupt=-1, use_pyserial_free=True):
    master, slave = pty.openpty()
    tty.setraw(slave)
    name = os.ttyname(slave)
    dev = subprocess.Popen([os.path.join(HERE, "fake_sender"), src, str(drop), str(corrupt)], stdin=master, stdout=master)
    out = tempfile.mktemp(suffix=".wav", dir=os.environ.get("TMPDIR"))
    env = dict(os.environ)
    r = subprocess.run([sys.executable, RECORD, str(secs), "-o", out, "--port", name], capture_output=True, text=True, timeout=60, env=env)
    dev.wait(timeout=10)
    os.close(master); os.close(slave)
    return r, out


def main():
    tmp = tempfile.mkdtemp(dir=os.environ.get("TMPDIR"))
    src = os.path.join(tmp, "src.wav")
    make_wav(src, 3)
    with wave.open(src) as w:
        ref = w.readframes(w.getnframes())
    frames = 3 * 16000 // 512
    r, out = run(src, 3)
    check(r.returncode == 0, f"clean capture exit {r.returncode}: {r.stderr.strip()}")
    with wave.open(out) as w:
        got = w.readframes(w.getnframes())
        check(w.getframerate() == 16000 and w.getnchannels() == 1 and w.getsampwidth() == 2, "wav format")
    check(len(got) == frames * 1024 and got == ref[: frames * 1024], "samples identical to the source")
    print("clean capture:", r.stderr.strip())
    r, out = run(src, 3, drop=10, corrupt=20)
    with wave.open(out) as w:
        got = w.readframes(w.getnframes())
    check(r.returncode == 1 and "2 hop(s) missing" in r.stderr and "1 corrupt" in r.stderr, f"loss reported: {r.stderr.strip()}")
    check(got[10 * 1024:11 * 1024] == b"\0" * 1024 and got[20 * 1024:21 * 1024] == b"\0" * 1024 and got[:10 * 1024] == ref[:10 * 1024], "lost hops zero-filled, rest intact")
    print("lossy capture:", r.stderr.strip())
    print("RECORD TESTS FAILED" if fails else "RECORD TESTS PASSED")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
