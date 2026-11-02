#!/usr/bin/env python3
"""Run the detector evaluation on data/set (see build_set.py) and print the results.

    make -C test/host wavtool
    python3 test/eval/run_eval.py [--md results.md] [--json results.json] [-t THR] [-P k=v ...]

For every experiment (bell x condition x held-out ring) the wavtool trains on train.wav exactly as
the device would (ring segmentation, medoid + averaging) and is then run on the held-out test
clips and on all negatives. A held-out ring counts as detected if the detector fires inside the
ring window; every other firing is a false trigger. Pure Python stdlib.

Negative groups: core = speech, TV/radio/conversation, household (Freesound) and the ESC-50
"interior/domestic" and "human non-speech" classes; outdoor = ESC-50 animals, nature, urban
(sirens, car horns, ...); hard = ring tones, other bells and chimes, alarm clocks, church bells.
ESC-50 files hold 12 clips of 5 s; each false trigger is attributed to the clip it fired in.
"""
import argparse, csv, json, os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.environ.get("BELL_EVAL_DATA", os.path.join(HERE, "data"))
ESC_GROUP = {0: "outdoor", 1: "outdoor", 2: "core", 3: "core", 4: "outdoor"}   # ESC-50 target // 10
CAT_GROUP = {"speech": "core", "tv": "core", "household": "core", "hard_ringtone": "hard",
             "hard_otherbell": "hard", "hard_alarm_bells": "hard", "transient": "transient"}
GROUPS = ("core", "transient", "outdoor", "hard")
# ESC-50 classes (inside "core") that are short transients; their false triggers are also listed under "transient"
ESC_TRANSIENT = ("door_wood_knock", "mouse_click", "keyboard_typing", "clapping", "footsteps", "can_opening", "door_wood_creaks", "glass_breaking")


def run_one(args, exp, negs, root):
    files = [os.path.join(root, t["path"]) for t in exp["tests"]] + [os.path.join(root, n["path"]) for n in negs]
    cmd = [args.wavtool] + sum([["-P", p] for p in args.P], []) + (["-t", str(args.t)] if args.t else []) + \
          ["train", os.path.join(root, exp["train"]), "--"] + ([] if args.legacy else ["test"]) + files
    out = subprocess.run(cmd, capture_output=True, text=True)
    res = {"train": None, "files": {}}
    for line in out.stdout.splitlines():
        if line.startswith("TRAIN"):
            res["train"] = line
        m = re.match(r"(?:FILE )?(\S+)\s+matches=(\d+)\s+peak(?:_score)?=([\d.]+)(?:\s+times=(\S+))?(?:\s+peak_t=([\d.]+))?", line)
        if m and m.group(1) in files:
            t = [] if not m.group(4) or m.group(4) == "-" else [float(v) for v in m.group(4).split(",")]
            res["files"][m.group(1)] = dict(n=int(m.group(2)), peak=float(m.group(3)), times=t, peak_t=float(m.group(5) or 0))
    if out.returncode != 0 or not res["files"]:
        res["error"] = (out.stderr or out.stdout)[-300:]
    return exp, res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wavtool", default=os.path.join(HERE, "..", "host", "wavtool"))
    ap.add_argument("--set", default=os.path.join(DATA, "set"))
    ap.add_argument("--json", default=None, help="write per-bell / per-category results")
    ap.add_argument("--raw", default=None, help="write raw per-file results")
    ap.add_argument("--md", default=None, help="write Markdown tables")
    ap.add_argument("--legacy", action="store_true", help="old wavtool output (no match times)")
    ap.add_argument("--only", default=None, help="regex on bell name")
    ap.add_argument("--negs", default="all", help="all | none | comma list of categories")
    ap.add_argument("--sweep", action="store_true", help="with -t 0.99: detection/false-trigger clips per fixed threshold")
    ap.add_argument("-P", action="append", default=[])
    ap.add_argument("-t", type=float, default=None)
    ap.add_argument("-j", type=int, default=os.cpu_count())
    args = ap.parse_args()
    man = json.load(open(os.path.join(args.set, "manifest.json")))
    exps = [e for e in man["experiments"] if not args.only or re.search(args.only, e["bell"])]
    negs = [] if args.negs == "none" else [n for n in man["negatives"] if args.negs == "all" or n["cat"] in args.negs.split(",")]
    esc = {}
    meta = os.path.join(DATA, "dl", "ESC-50-master", "meta", "esc50.csv")
    if os.path.exists(meta):
        esc = {r["filename"]: (ESC_GROUP[int(r["target"]) // 10], r["category"]) for r in csv.DictReader(open(meta))}
    with ThreadPoolExecutor(args.j) as ex:
        results = list(ex.map(lambda e: run_one(args, e, negs, args.set), exps))
    if args.raw:
        json.dump([dict(exp=e, res=r) for e, r in results], open(args.raw, "w"))

    def neg_hits(n, r):
        """(group, label) per false trigger of negative n"""
        if "parts" in n and esc and not n["cat"].startswith("hard"):
            times = r["times"] if r["times"] else [r["peak_t"]] * r["n"]
            return [esc[n["parts"][min(len(n["parts"]) - 1, int(t // 5))]] for t in times[: r["n"]]]
        return [(CAT_GROUP.get(n["cat"], "outdoor"), n["cat"])] * r["n"]

    minutes = {g: 0.0 for g in GROUPS}
    for n in negs:
        if "parts" in n and esc and not n["cat"].startswith("hard"):
            for p in n["parts"]:
                minutes[esc[p][0]] += 5 / 60
        else:
            minutes[CAT_GROUP.get(n["cat"], "outdoor")] += n["dur"] / 60
    per, fp_detail, cond_fp = {}, {}, {}
    for exp, res in results:
        k = (exp["bell"], exp["cond"])
        s = per.setdefault(k, dict(bell=exp["bell"], cond=exp["cond"], kind=exp["kind"], source=exp["source"], license=exp["license"],
                                   identical=exp.get("identical", False), synthetic=exp["synthetic"], ring_s=exp["ring_s"],
                                   folds=0, det=0, tot=0, pos_fp=0, lat=[], tmpl_s=[], err=0, fp={g: 0 for g in GROUPS}))
        s["folds"] += 1
        if "error" in res:
            s["err"] += 1
            print("ERROR", exp["bell"], exp["cond"], exp["fold"], res["error"], file=sys.stderr)
            continue
        m = re.search(r"len=\d+ \(([\d.]+) s\)", res["train"] or "")
        if m:
            s["tmpl_s"].append(float(m.group(1)))
        for t in exp["tests"]:
            r = res["files"].get(os.path.join(args.set, t["path"]))
            if r is None:
                continue
            s["tot"] += 1
            if args.legacy:
                hit = r["n"] >= 1
                s["pos_fp"] += max(0, r["n"] - 1)
            else:
                inwin = [x for x in r["times"] if t["t0"] <= x <= t["t1"]]
                hit = bool(inwin)
                s["pos_fp"] += len(r["times"]) - len(inwin)
                if hit:
                    s["lat"].append(inwin[0] - t["t0"])
            s["det"] += hit
        for n in negs:
            r = res["files"].get(os.path.join(args.set, n["path"]))
            if not r or not r["n"]:
                continue
            for g, label in neg_hits(n, r):
                s["fp"][g] += 1
                cond_fp.setdefault((exp["cond"], g), 0)
                cond_fp[(exp["cond"], g)] += 1
                fp_detail.setdefault(g, {}).setdefault(label, 0)
                fp_detail[g][label] += 1

    rows = sorted(per.values(), key=lambda s: (s["cond"], s["bell"]))
    hdr = f"{'bell':18} {'cond':8} {'ring s':>6} {'tmpl s':>6} {'detected':>9} {'%':>6} {'lat s':>5} {'FP core':>7} {'FP trans':>8} {'FP outdoor':>10} {'FP hard':>7} {'FP in clip':>10}"
    print(hdr)
    tot_det = tot = 0
    sub = {}
    for s in rows:
        lat = sum(s["lat"]) / len(s["lat"]) if s["lat"] else 0
        tm = sum(s["tmpl_s"]) / len(s["tmpl_s"]) if s["tmpl_s"] else 0
        print(f"{s['bell']:18} {s['cond']:8} {s['ring_s']:6.1f} {tm:6.1f} {s['det']:4d}/{s['tot']:<4d} {100 * s['det'] / max(1, s['tot']):6.1f} {lat:5.2f} "
              f"{s['fp']['core']:7d} {s['fp']['transient']:8d} {s['fp']['outdoor']:10d} {s['fp']['hard']:7d} {s['pos_fp']:10d}" + (f"  ERR {s['err']}" if s["err"] else ""))
        tot_det += s["det"]; tot += s["tot"]
        keys = [f"cond {s['cond']}"] + (["local bells (direct+speaker)"] if s["source"] == "local" and s["cond"] != "click" else [])
        if s["source"] != "local" and s["cond"] in ("direct", "speaker"):
            keys.append("public set, direct+speaker (as before)")
        for k in keys:
            a = sub.setdefault(k, [0, 0, 0]); a[0] += s["det"]; a[1] += s["tot"]; a[2] += s["pos_fp"]
    n_t = len(results)
    print(f"TOTAL held-out rings detected {tot_det}/{tot} = {100 * tot_det / max(1, tot):.1f} %   ({n_t} trained templates)")
    for k, (d, t, pf) in sorted(sub.items()):
        print(f"  {k:40} {d}/{t} = {100 * d / max(1, t):.1f} %   triggers outside the ring window: {pf}")
    for g in GROUPS:
        n_fp = sum(v for (c, gg), v in cond_fp.items() if gg == g)
        top = sorted(fp_detail.get(g, {}).items(), key=lambda kv: -kv[1])[:8]
        print(f"negatives {g:9} {minutes[g]:6.1f} min each, x {n_t} templates = {minutes[g] * n_t / 60:7.1f} h: {n_fp} false triggers"
              + (f"  ({', '.join(f'{k} {v}' for k, v in top)})" if top else ""))
    esc_tr = sum(v for k, v in fp_detail.get("core", {}).items() if k in ESC_TRANSIENT)
    print(f"  of the core negatives, ESC-50 transient classes ({len(ESC_TRANSIENT)} classes, {len(ESC_TRANSIENT) * 40 * 5 / 60:.1f} min): {esc_tr} false triggers")

    if args.json:
        json.dump(dict(per=rows, minutes=minutes, templates=n_t, fp_detail=fp_detail, params=args.P, threshold=args.t),
                  open(args.json, "w"), indent=1)
    if args.md:
        with open(args.md, "w") as f:
            f.write("| Doorbell | Kind | Source (licence) | Condition | Ring s | Template s | Held-out rings detected | Mean latency s | False triggers core / transient / outdoor / hard | Triggers outside the ring in its clip |\n")
            f.write("|---|---|---|---|---|---|---|---|---|---|\n")
            for s in rows:
                lat = sum(s["lat"]) / len(s["lat"]) if s["lat"] else 0
                tm = sum(s["tmpl_s"]) / len(s["tmpl_s"]) if s["tmpl_s"] else 0
                f.write(f"| {s['bell']} | {s['kind']} | {s['source']} ({s['license']}) | {s['cond']} | {s['ring_s']:.1f} | {tm:.1f} | "
                        f"{s['det']}/{s['tot']} ({100 * s['det'] / max(1, s['tot']):.0f} %) | {lat:.2f} | {s['fp']['core']} / {s['fp']['transient']} / {s['fp']['outdoor']} / {s['fp']['hard']} | {s['pos_fp']} |\n")
            f.write(f"\nTotal: {tot_det}/{tot} = {100 * tot_det / max(1, tot):.1f} % of held-out rings detected, {n_t} trained templates.\n\n")
            f.write("| Negatives | Minutes (each template) | Template-hours | False triggers | Most frequent |\n|---|---|---|---|---|\n")
            for g in GROUPS:
                n_fp = sum(v for (c, gg), v in cond_fp.items() if gg == g)
                top = sorted(fp_detail.get(g, {}).items(), key=lambda kv: -kv[1])[:6]
                f.write(f"| {g} | {minutes[g]:.1f} | {minutes[g] * n_t / 60:.1f} | {n_fp} | {', '.join(f'{k} {v}' for k, v in top) or '-'} |\n")

    if args.sweep:
        print("\nfixed threshold T: held-out rings with peak >= T, negative clips with peak >= T (core / other)")
        for T in [x / 100 for x in range(60, 86, 2)]:
            pd = pt = nc = no = 0
            for e, r in results:
                if "error" in r:
                    continue
                for t in e["tests"]:
                    p = r["files"].get(os.path.join(args.set, t["path"]))
                    if p:
                        pt += 1; pd += p["peak"] >= T
                for n in negs:
                    p = r["files"].get(os.path.join(args.set, n["path"]))
                    if p and p["peak"] >= T:
                        g = neg_hits(n, dict(p, n=1, times=[p["peak_t"]]))[0][0]
                        if g == "core": nc += 1
                        else: no += 1
            print(f"  T={T:.2f}  {pd}/{pt} = {100 * pd / max(1, pt):5.1f} %   core {nc:5d}   other {no:5d}")


if __name__ == "__main__":
    sys.exit(main())
