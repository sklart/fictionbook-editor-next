"""Read-only analysis of completed C5 process receipts (warmups excluded)."""
import argparse
import json
import math
import statistics
from pathlib import Path


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def stats(values):
    values = sorted(values)
    rank = (len(values) - 1) * .9
    lo, hi = math.floor(rank), math.ceil(rank)
    return dict(median=statistics.median(values),
                p90=values[lo] + (values[hi] - values[lo]) * (rank - lo),
                min=min(values), max=max(values), N=len(values))


def read_runs(root):
    result = {}
    for path in root.glob("*/manifest.json"):
        manifest = load(path)
        if manifest["status"] != "PASS" or manifest["warmup"]:
            continue
        measurements = load(path.parent / "measurements.json")
        values = {("time", name): value for name, value in measurements["timings"].items()}
        for stage in measurements["stages"]:
            if stage["stage"].startswith("internal_"):
                continue
            for name, value in stage.items():
                if name not in ("stage", "qpc", "elapsed_ms"):
                    values[(stage["stage"], name)] = value
        result[(manifest["fixture"], manifest["cache"], manifest["repeat"])] = values
    return result


def analyze(root):
    runs = read_runs(root)
    rows = []
    for fixture, cache in sorted({(f, c) for f, c, _ in runs}):
        repeats = sorted(r for f, c, r in runs if (f, c) == (fixture, cache))
        keys = set.intersection(*(set(runs[(fixture, cache, r)]) for r in repeats))
        for stage, metric in sorted(keys):
            values = [runs[(fixture, cache, r)][(stage, metric)] for r in repeats]
            row = dict(fixture=fixture, cache=cache, stage=stage, metric=metric, **stats(values))
            paired = [(runs[(fixture, "document", r)][(stage, metric)], value)
                      for r, value in zip(repeats, values)
                      if (fixture, "document", r) in runs]
            paired = [(old, new) for old, new in paired if old != 0]
            if paired:
                deltas = [(new / old - 1) * 100 for old, new in paired]
                baseline = [old for old, _ in paired]
                candidates = [new for _, new in paired]
                row.update(delta_pct=(statistics.median(candidates) / statistics.median(baseline) - 1) * 100,
                           paired_delta_pct=statistics.median(deltas), paired_N=len(paired),
                           pairs_above_5pct=sum(d > 5 for d in deltas))
                row["stable_regression"] = row["delta_pct"] > 5 and len(paired) == 7 and row["pairs_above_5pct"] >= 6
            rows.append(row)
    return rows


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    rows = analyze(args.root)
    (args.root / "statistics.json").write_text(json.dumps(rows, indent=2), encoding="utf-8")
    for row in rows:
        if row["stage"] == "source_stable" and row["metric"] == "private_bytes":
            print(row["fixture"], row["cache"], "N", row["N"], "MiB", round(row["median"] / 1048576, 2), "delta", round(row.get("delta_pct", 0), 2))
    for row in rows:
        if row.get("stable_regression") and row["stage"] == "time":
            print("STABLE TIMING REGRESSION", row["fixture"], row["cache"], row["metric"], round(row["delta_pct"], 2), row["pairs_above_5pct"])
