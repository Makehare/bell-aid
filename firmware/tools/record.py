#!/usr/bin/env python3
"""Record what the sender's microphone hears, as a WAV file (16 kHz, mono, 16-bit).

    python3 tools/record.py 30 -o mybell.wav [--port /dev/ttyACM0]

Sends `record <seconds>` to the sender over its USB console and decodes the PCM lines
("PCM <seq> <crc32> <base64>") into a WAV. The audio is exactly what the detector gets (after the
mic's 24 -> 16 bit conversion). Close any other serial monitor first. Needs pyserial
(`pip install pyserial`, it comes with esptool); on Linux/macOS it also works without it.

Then test the detector on the recording on a PC:
    make -C test/host wavtool
    test/host/wavtool train mybell.wav -- test other_recording.wav
"""
import argparse, base64, glob, os, sys, time, wave, zlib

HOP = 512


def open_port(port):
    try:
        import serial  # type: ignore
        s = serial.Serial(port, 115200, timeout=0.5)
        return s.write, lambda: s.readline(), s.close
    except ImportError:
        import termios, tty
        fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
        tty.setraw(fd)
        attrs = termios.tcgetattr(fd)
        attrs[6][termios.VMIN], attrs[6][termios.VTIME] = 0, 5
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        f = os.fdopen(fd, "rb+", buffering=0)
        buf = bytearray()

        def readline():
            while b"\n" not in buf:
                chunk = f.read(4096)
                if not chunk:
                    break
                buf.extend(chunk)
            i = buf.find(b"\n")
            if i < 0:
                out = bytes(buf); buf.clear(); return out
            out = bytes(buf[: i + 1]); del buf[: i + 1]; return out
        return f.write, readline, f.close


def guess_port():
    c = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/cu.usbmodem*"))
    return c[0] if c else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("seconds", type=int, help="1..120")
    ap.add_argument("-o", "--out", default=None, help="WAV file (default rec-<time>.wav)")
    ap.add_argument("--port", default=None, help="serial port (default: first /dev/ttyACM* or /dev/cu.usbmodem*; COMx on Windows)")
    a = ap.parse_args()
    if not 1 <= a.seconds <= 120:
        sys.exit("seconds must be 1..120")
    port = a.port or guess_port()
    if not port:
        sys.exit("no serial port found, use --port")
    out = a.out or time.strftime("rec-%Y%m%d-%H%M%S.wav")
    write, readline, close = open_port(port)
    write(b"\r\n")
    time.sleep(0.3)
    while readline():
        pass                                       # drain old output
    write(f"record {a.seconds}\r\n".encode())
    frames, bad, begun, end, expected = {}, 0, False, None, None
    deadline = time.time() + a.seconds + 15
    while time.time() < deadline:
        line = readline()
        if not line:
            continue
        line = line.decode("ascii", "replace").strip()
        if line.startswith("REC-BEGIN"):
            begun = True
            expected = int(line.split("frames=")[1].split()[0])
            print(f"recording {a.seconds} s from {port} ...", file=sys.stderr)
        elif line.startswith("PCM ") and begun:
            try:
                _, seq, crc, b64 = line.split(" ", 3)
                raw = base64.b64decode(b64, validate=True)
                if len(raw) != 2 * HOP or zlib.crc32(raw) != int(crc, 16):
                    raise ValueError
                frames[int(seq)] = raw
            except ValueError:
                bad += 1
        elif line.startswith("REC-END"):
            end = line
            break
        elif line.startswith("unknown/invalid"):
            sys.exit("the device did not accept `record` (is it a sender with the current firmware?)")
    close()
    if not begun:
        sys.exit("no REC-BEGIN from the device (wrong port, or firmware without `record`?)")
    n = expected if expected else (max(frames) + 1 if frames else 0)
    missing = [i for i in range(n) if i not in frames]
    with wave.open(out, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(16000)
        for i in range(n):
            w.writeframes(frames.get(i, b"\0" * (2 * HOP)))   # little-endian int16, as sent
    print(f"wrote {out}: {n * HOP / 16000:.1f} s, {len(missing)} hop(s) missing/zero-filled, {bad} corrupt line(s); device: {end or 'no REC-END'}",
          file=sys.stderr)
    return 1 if (missing or bad or not end) else 0


if __name__ == "__main__":
    sys.exit(main())
