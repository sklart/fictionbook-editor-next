"""Summarize the fixed seven-repeat paired memory contour, without rerunning it."""
import argparse
import json
import math
from pathlib import Path
import statistics

ROOT = Path(__file__).resolve().parents[2]
MIB = 2**20


def distribution(values):
    values = sorted(values)
    return dict(n=len(values), median=statistics.median(values), p90=values[math.ceil(.9 * len(values)) - 1], min=values[0], max=values[-1])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--evidence", type=Path, default=ROOT / "out/perf/c41/fix-benchmark")
    args = ap.parse_args()
    manifest = json.loads((args.evidence / "manifest.json").read_text(encoding="utf-8-sig"))
    assert manifest["status"] == "PASS" and len(manifest["results"]) == 28
    groups, paired = {}, {}
    for run in manifest["results"]:
        assert run["status"] == run["equivalence"] == "PASS"
        path = Path(run["directory"]) / (run["fixture"] + "-1-stages.json")
        stages = json.loads(path.read_text(encoding="utf-8-sig"))
        assert not any(s["stage"].startswith(("native_", "js_")) for s in stages)
        initial = stages[0]["private_bytes"]
        for state in stages:
            metrics = {k: v for k, v in state.items() if k not in ("stage", "qpc", "elapsed_ms")}
            metrics["private_delta_from_initialized"] = state["private_bytes"] - initial
            for metric, value in metrics.items():
                key = (run["fixture"], state["stage"], metric)
                groups.setdefault((*key, run["variant"]), []).append(value)
                paired.setdefault((*key, run["repeat"]), {})[run["variant"]] = value
    summary = []
    for fixture, state, metric, variant in sorted(groups):
        if variant != "old":
            continue
        old = distribution(groups[fixture, state, metric, "old"])
        new = distribution(groups[fixture, state, metric, "new"])
        assert old["n"] == new["n"] == 7
        pairs = [paired[fixture, state, metric, repeat] for repeat in range(1, 8)]
        deltas = [p["new"] - p["old"] for p in pairs]
        relative = [(p["new"] / p["old"] - 1) * 100 for p in pairs if p["old"] != 0]
        summary.append(dict(fixture=fixture, stage=state, metric=metric, old=old, new=new,
                            delta_percent=(new["median"] / old["median"] - 1) * 100 if old["median"] else None,
                            paired_delta=distribution(deltas), paired_delta_percent=distribution(relative) if relative else None))
    (args.evidence / "analysis.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    selected = {"after_open", "after_open_gc_idle", "before_source_gc", "source_visible", "source_stable",
                "body_after_readonly_source", "body_after_source_idle", "source_xml_and_text_cache_released",
                "source_scintilla_document_released", "before_close"}
    metrics = {"private_bytes", "working_set", "peak_private_bytes", "peak_working_set", "private_delta_from_initialized"}
    text = ["## Final fix before/after: targeted Open→Source→Body\n",
            "Семь отдельных old и семь new процессов на fixture; порядок вариантов и fixtures ротируется. "
            "Один и тот же диагностический Release EXE, два frozen main.js. Verification Save выполняется после всех измеряемых точек.\n",
            "MiB; p90 — nearest rank (при N=7 равен max). Δ% — отношение отдельных medians; paired Δ — median внутри repeat, в MiB. "
            "Private delta from initialized отдельно вычитает initial Private каждого процесса.\n",
            "|Fixture|State|Metric|Old median|Old p90|Old min|Old max|New median|New p90|New min|New max|N each|Δ %|Paired Δ MiB|\n"
            "|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for row in summary:
        if row["stage"] not in selected or row["metric"] not in metrics:
            continue
        values = [row["fixture"], row["stage"], row["metric"]]
        values += [f'{row[v][k]/MIB:.2f}' for v in ("old", "new") for k in ("median", "p90", "min", "max")]
        values += ["7", f'{row["delta_percent"]:+.2f}' if row["delta_percent"] is not None else "n/a", f'{row["paired_delta"]["median"]/MIB:+.2f}']
        text.append("|" + "|".join(values) + "|")
    (args.evidence / "tables.md").write_text("\n".join(text) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
