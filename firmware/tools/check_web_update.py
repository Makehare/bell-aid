#!/usr/bin/env python3
"""Host check for docs/update/: the web updater must write only the app image at 0x20000.
Fails if the manifest or the served binary deviates from firmware/dist."""
import hashlib, json, re, sys
from pathlib import Path

FW = Path(__file__).resolve().parent.parent
UP = FW.parent / "docs" / "update"
APP_OFFSET = 0x20000


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def check(manifest_path=UP / "manifest.json", dist=FW / "dist" / "doorbell-app.bin", up_dir=UP):
    err = []
    m = json.loads(manifest_path.read_text())
    # ESP Web Tools 10.5.2 (no improv): with prompt_erase false/missing the install
    # button ERASES THE WHOLE FLASH. true shows an "Erase device" checkbox, unticked.
    if m.get("new_install_prompt_erase") is not True:
        err.append("new_install_prompt_erase must be true (false/missing makes ESP Web Tools erase the device)")
    if "improv" in json.dumps(m).lower():
        err.append("no improv allowed")
    builds = m.get("builds", [])
    if len(builds) != 1:
        err.append(f"need exactly 1 build, got {len(builds)}")
    for b in builds:
        if b.get("chipFamily") != "ESP32-S3":
            err.append("chipFamily must be ESP32-S3")
        parts = b.get("parts", [])
        if len(parts) != 1:
            err.append(f"need exactly 1 part, got {len(parts)}")
            continue
        p = parts[0]
        if p.get("offset") != APP_OFFSET:
            err.append(f"offset {p.get('offset')} != {APP_OFFSET} (0x20000)")
        if p.get("path") != "doorbell-app.bin":
            err.append(f"unexpected part path {p.get('path')}")
        f = up_dir / str(p.get("path"))
        if not f.is_file():
            err.append(f"{f} missing")
        elif not dist.is_file() or sha(f) != sha(dist):
            err.append("sha256 of served bin differs from firmware/dist/doorbell-app.bin")
    if not re.fullmatch(r"\d+\.\d+(\.\d+)?", str(m.get("version", ""))):
        err.append("bad version")
    idx = (up_dir / "index.html").read_text()
    mm = re.search(r"<!--ver-->([^<]*)<!--/ver-->", idx)
    if not mm or mm.group(1) != m.get("version"):
        err.append("index.html version label differs from manifest")
    mv = re.search(r'"(esp-web-tools-(\d+\.\d+\.\d+))/install-button\.js"', idx)
    if not mv:
        err.append("esp-web-tools must be vendored and pinned (esp-web-tools-X.Y.Z/install-button.js)")
    else:
        lib = up_dir / mv.group(1)
        for f in ("install-button.js", "LICENSE"):
            if not (lib / f).is_file():
                err.append(f"{lib.name}/{f} missing")
    if re.search(r"https?://[^\"' >]*\.js", idx):
        err.append("no external script allowed (vendored only)")
    for f in ("Inter-OFL.txt", "SpaceGrotesk-OFL.txt"):
        if not (up_dir / "fonts" / f).is_file():
            err.append(f"fonts/{f} missing (OFL)")
    sums = FW / "dist" / "SHA256SUMS"
    if sums.is_file() and (up_dir / "doorbell-app.bin").is_file():
        want = [l.split()[0] for l in sums.read_text().splitlines() if l.endswith("doorbell-app.bin")]
        if want and want[0] != sha(up_dir / "doorbell-app.bin"):
            err.append("sha256 differs from dist/SHA256SUMS")
    return err


if __name__ == "__main__":
    errors = check()
    for e in errors:
        print("FAIL:", e, file=sys.stderr)
    if errors:
        sys.exit(1)
    print("web update check ok (1 part, 0x20000, erase prompt on (unticked), vendored lib, sha matches dist)")
