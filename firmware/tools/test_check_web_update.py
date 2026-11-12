#!/usr/bin/env python3
"""Negative tests for check_web_update: each bad manifest must be rejected."""
import json, shutil, sys, tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import check_web_update as c

good = json.loads((c.UP / "manifest.json").read_text())


def run(mut, bin_mut=False):
    with tempfile.TemporaryDirectory() as d:
        d = Path(d)
        shutil.copy(c.UP / "index.html", d)
        shutil.copytree(c.UP / "fonts", d / "fonts")
        shutil.copytree(c.UP / "esp-web-tools-10.5.2", d / "esp-web-tools-10.5.2")
        shutil.copy(c.UP / "doorbell-app.bin", d)
        m = json.loads(json.dumps(good))
        mut(m)
        (d / "manifest.json").write_text(json.dumps(m))
        if bin_mut:
            (d / "doorbell-app.bin").write_bytes(b"x" + (d / "doorbell-app.bin").read_bytes())
        return c.check(d / "manifest.json", up_dir=d)


assert run(lambda m: None) == [], "good manifest rejected"
cases = {
    "offset 0": lambda m: m["builds"][0]["parts"][0].update(offset=0),
    "offset 0x10000": lambda m: m["builds"][0]["parts"][0].update(offset=0x10000),
    "no erase prompt (would erase device)": lambda m: m.update(new_install_prompt_erase=False),
    "erase prompt missing": lambda m: m.pop("new_install_prompt_erase"),
    "extra part": lambda m: m["builds"][0]["parts"].append({"path": "doorbell-app.bin", "offset": 0x9000}),
    "two builds": lambda m: m["builds"].append(m["builds"][0]),
    "wrong chip": lambda m: m["builds"][0].update(chipFamily="ESP32"),
    "version mismatch": lambda m: m.update(version="9.9"),
}
for n, f in cases.items():
    assert run(f), f"not rejected: {n}"
assert run(lambda m: None, bin_mut=True), "sha mismatch not rejected"
print("check_web_update negative tests ok (%d cases + sha)" % len(cases))
