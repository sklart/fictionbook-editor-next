"""Matched-process C5 comparison; positive delta means an increase."""
import argparse
import csv
import json
import statistics
from pathlib import Path
import math

def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def stats(values):
    values = sorted(values)
    rank = (len(values) - 1) * .9
    lo, hi = math.floor(rank), math.ceil(rank)
    return dict(median=statistics.median(values),
                p90=values[lo] + (values[hi] - values[lo]) * (rank - lo),
                min=min(values), max=max(values), N=len(values))


def values(directory, release=False, implementation="old"):
    data = load(directory / "measurements.json")
    stages = {s["stage"]: s for s in data["stages"]}
    result = {("time", name): value for name, value in data["timings"].items()}
    for name, row in stages.items():
        if name.startswith("internal_"):
            continue
        for metric, value in row.items():
            if metric not in ("stage", "elapsed_ms", "qpc"):
                result[(name, metric)] = value
    if release:
        name = "body_layout_released" if implementation == "new" else "body_idle"
        for metric, value in stages[name].items():
            if metric not in ("stage", "elapsed_ms", "qpc"):
                result[("body_after_release", metric)] = value
        cost = stages["body_layout_released"]["elapsed_ms"] - stages["body_idle"]["elapsed_ms"] if implementation == "new" else 0
        result[("time", "source_to_body_plus_release_upper_bound")] = data["timings"]["source_to_body"] + cost
        result[("time", "layout_release_upper_bound")] = cost
    return result


def analyze(root, release=False):
    runs = {}
    for path in root.glob("*/manifest.json"):
        receipt = load(path)
        if receipt["status"] != "PASS" or receipt["warmup"]:
            continue
        key = receipt["fixture"], receipt["implementation"], receipt["repeat"]
        assert key not in runs, key
        runs[key] = values(path.parent, release, receipt["implementation"])
    rows = []
    for fixture in sorted({f for f, _, _ in runs}):
        repeats = sorted(r for f, impl, r in runs if f == fixture and impl == "old" and (fixture, "new", r) in runs)
        if not repeats:
            continue
        keys = set.intersection(*(set(runs[(fixture, impl, repeat)]) for repeat in repeats for impl in ("old", "new")))
        for stage, metric in sorted(keys):
            old = [runs[(fixture, "old", repeat)][(stage, metric)] for repeat in repeats]
            new = [runs[(fixture, "new", repeat)][(stage, metric)] for repeat in repeats]
            row = dict(fixture=fixture, stage=stage, metric=metric, old=stats(old), new=stats(new), N=len(repeats))
            ratios = [(b / a - 1) * 100 for a, b in zip(old, new) if a != 0]
            row["delta_pct"] = (row["new"]["median"] / row["old"]["median"] - 1) * 100 if row["old"]["median"] else (0 if row["new"]["median"] == 0 else None)
            row["paired_delta_pct"] = statistics.median(ratios) if ratios else None
            row["pairs_above_5pct"] = sum(value > 5 for value in ratios)
            row["paired_N"] = len(ratios)
            row["stable_regression"] = row["delta_pct"] is not None and row["delta_pct"] > 5 and len(ratios) == 7 and row["pairs_above_5pct"] >= 6
            rows.append(row)
    return rows


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("--release-layout", action="store_true")
    args = parser.parse_args()
    rows = analyze(args.root, args.release_layout)
    (args.root / "statistics.json").write_text(json.dumps(rows, indent=2), encoding="utf-8")
    with (args.root / "statistics.csv").open("w", encoding="utf-8", newline="") as output:
        flat = [{**{k: v for k, v in row.items() if k not in ("old", "new")}, **{impl + "_" + key: value for impl in ("old", "new") for key, value in row[impl].items()}} for row in rows]
        if flat:
            writer = csv.DictWriter(output, flat[0].keys())
            writer.writeheader()
            writer.writerows(flat)
    for row in rows:
        if (row["stage"] == "body_after_release" and row["metric"] == "private_bytes") or (row["stage"] == "time" and row["metric"] in ("second_body_to_source", "source_to_body_plus_release_upper_bound")):
            scale = 1048576 if row["metric"] == "private_bytes" else 1
            print(row["fixture"], row["stage"], row["metric"], "N", row["N"], "old", round(row["old"]["median"] / scale, 3), "new", round(row["new"]["median"] / scale, 3), "delta", round(row["delta_pct"], 2), "bad pairs", row["pairs_above_5pct"])
    for row in rows:
        if row["stage"] == "time" and row["stable_regression"]:
            print("STABLE TIMING REGRESSION", row["fixture"], row["metric"], round(row["delta_pct"], 2), row["pairs_above_5pct"])
