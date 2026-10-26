"""Recompute C4 diagnostic tables from raw evidence; no application launches."""
import argparse
import hashlib
import json
import math
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MIB = 2**20


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def stats(values):
    values = sorted(values)
    return dict(n=len(values), median=statistics.median(values),
                p90=values[math.ceil(.9 * len(values)) - 1],
                min=values[0], max=values[-1])


def analyze(directory):
    rows = [r for r in load(directory / "raw.json") if not r["warmup"]]
    keys = ("fixture", "case", "count", "bytes", "mode", "variant", "kind", "metric")
    groups = {}
    for row in rows:
        key = tuple((k, row[k]) for k in keys if k in row)
        groups.setdefault(key, []).append(row["value"])
    return [dict(key, **stats(values)) for key, values in groups.items()]


def fit(points):
    xs, ys = zip(*points)
    xm, ym = statistics.mean(xs), statistics.mean(ys)
    slope = sum((x-xm)*(y-ym) for x, y in points) / sum((x-xm)**2 for x in xs)
    intercept = ym - slope*xm
    total = sum((y-ym)**2 for y in ys)
    residual = sum((y-intercept-slope*x)**2 for x, y in points)
    return dict(intercept_bytes=intercept, bytes_per_entry=slope,
                r_squared=1-residual/total if total else None)


def audit(evidence):
    """Read-only input/coverage audit; write only a new C4 audit result."""
    def sha(path):
        return hashlib.file_digest(path.open("rb"), "sha256").hexdigest().upper()
    baseline = load(evidence / "baseline.json")
    c3 = ROOT / "out/perf/results-2f0113b2-final64-uiwait"
    for name, expected in baseline["evidenceHashes"].items():
        assert sha(c3 / name) == expected, ("protected C3 evidence changed", name)
    manifest = load(c3 / "manifest.json")
    files = 0
    for variant in manifest["variants"].values():
        folder = Path(variant["exe"]).parent
        for name, expected in variant["hashes"].items():
            assert sha(folder / name) == expected, ("snapshot changed", name)
            files += 1
    for name, fixture in manifest["fixtures"].items():
        assert sha(c3 / (name + ".fb2")) == fixture["sha256"], name
    experiments = {"scaling-open-only": (18, 54), "scaling-unique": (18, 54),
                   "memory-cause": (9, 27), "open-recheck": (6, 42),
                   "profile-js": (6, 18), "open-recheck-legacy": (6, 42)}
    for name, (warm, measured) in experiments.items():
        folder = evidence / name
        state = load(folder / "manifest.json")
        assert state["status"] == "PASS", name
        assert (state["completedWarmups"], state["completedMeasured"]) == (warm, measured), name
        rows = load(folder / "raw.json")
        opened = [r for r in rows if r["kind"] == "time" and r["metric"] == "open"]
        assert sum(r["warmup"] for r in opened) == warm, name
        assert sum(not r["warmup"] for r in opened) == measured, name
        if "cases" in state:
            for case in state["cases"]:
                assert sha(folder / (case["name"] + ".fb2")) == case["sha256"], case["name"]
    cause = load(evidence / "memory-cause/manifest.json")
    assert len(cause["results"]) == 36
    assert all(r["status"] == "PASS" and r["equivalence"] == "PASS XSD/binary SHA256/MIME/references" for r in cause["results"])
    for fixture, count, size in (("many", 500, 32768000), ("large", 10, 31507926)):
        text = (evidence / "probes-js-binding" / (fixture + "-report.txt")).read_text(encoding="utf-8-sig")
        assert "result=PASS" in text
        stages = {r["stage"]: r for r in (dict(p.split("=", 1) for p in line.split(";")) for line in text.splitlines() if line.startswith("stage="))}
        for name in ("open_no_facade_access", "after_one_facade_read_released", "after_five_full_facade_reads_released", "after_js_one_read_returned", "after_js_five_full_reads_returned", "after_js_reads_gc", "body_after_source_apply_gc", "after_source_undo_discard_gc"):
            row = stages[name]
            assert int(row["current_document_bytes"]) == size, (fixture, name)
            assert int(row["unique_underlying_blocks"]) == count, (fixture, name)
            assert int(row["noncurrent_owner_bytes"]) == 0, (fixture, name)
        opened = stages["open_no_facade_access"]
        assert int(opened["reconcile_calls"]) == count + 2
        for key in ("dom_enumerations", "same_value_puts", "metadata_invalidations"):
            assert int(opened[key]) == 0, (fixture, key)
        assert "usable_payload_survived=0;canonical_expired=1;getter_calls=0" in text
        assert "base64data_attributes=0" in text
    result = dict(status="PASS", protected_c3_files=len(baseline["evidenceHashes"]),
                  snapshot_files=files, fixture_hashes="PASS", experiments=experiments,
                  causal_saved_equivalence=36, current_and_underlying_payload_checks="PASS")
    (evidence / "completion-audit.json").write_text(json.dumps(result, indent=2), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, default=ROOT / "out/perf/c4")
    parser.add_argument("--plot", type=Path)
    parser.add_argument("--audit", action="store_true", help="Verify protected inputs and required completed contours")
    args = parser.parse_args()
    output = {}
    original = load(ROOT / "out/perf/results-2f0113b2-final64-uiwait/raw.json")
    output["original-open-paired-slowdown"] = []
    for fixture in ("control", "many", "large"):
        lookup = {(r["variant"], r["round"]): r["value"] for r in original
                  if not r["warmup"] and r["fixture"] == fixture and r["kind"] == "time" and r["metric"] == "open"}
        changes = [100*(lookup["c3", i]-lookup["c2", i])/lookup["c2", i] for i in range(1, 8)]
        output["original-open-paired-slowdown"].append(dict(fixture=fixture, paired_percent=changes, **stats(changes)))
    for name in ("scaling-open-only", "scaling-unique", "memory-cause", "open-recheck", "profile-js", "open-recheck-legacy"):
        folder = args.evidence / name
        if (folder / "manifest.json").exists() and load(folder / "manifest.json")["status"] == "PASS":
            output[name] = analyze(folder)
    fits = []
    for experiment in ("scaling-open-only", "scaling-unique"):
        for variant in ("baseline", "c2", "c3"):
            for metric in ("after_open_private_bytes", "after_open_working_set", "after_open_peak_private_commit", "after_open_peak_working_set"):
                points = [(r["count"], r["median"]) for r in output[experiment]
                          if r["variant"] == variant and r["metric"] == metric and r["bytes"] == 32768000]
                fits.append(dict(experiment=experiment, variant=variant, metric=metric, **fit(points)))
    output["fits"] = fits
    profiles = []
    for fixture in ("control", "many", "large"):
        for variant in ("c2", "c3"):
            samples = [load(args.evidence / "profile-js" / f"{fixture}-profile-{i}-{variant}.tsv.profile.json") for i in (1, 2, 3)]
            for phase in samples[0]:
                profiles.append(dict(fixture=fixture, variant=variant, phase=phase,
                                     **stats([s[phase]["ms"] for s in samples]),
                                     calls=[s[phase]["calls"] for s in samples],
                                     entries=[s[phase]["entries"] for s in samples]))
    output["js-phases"] = profiles
    for fixture in ("many", "large"):
        report = args.evidence / "probes-js-binding" / (fixture + "-report.txt")
        stages = []
        for line in report.read_text(encoding="utf-8-sig").splitlines():
            if line.startswith("stage="):
                stages.append(dict(part.split("=", 1) for part in line.split(";")))
        output[fixture + "-stages"] = stages
    (args.evidence / "analysis.json").write_text(json.dumps(output, indent=2), encoding="utf-8")
    def table(headers, rows):
        return "| " + " | ".join(headers) + " |\n|" + "|".join("---" for _ in headers) + "|\n" + "\n".join("| " + " | ".join(str(x) for x in row) + " |" for row in rows) + "\n"
    lines = ["## Численные приложения\n", "### Open recheck: неизменённые C2/C3, N=7\n"]
    lines.append(table(["Fixture", "Вариант", "median ms", "p90 ms", "min ms", "max ms"],
                       [[r["fixture"],r["variant"],*[f'{r[k]:.2f}' for k in ("median","p90","min","max")]] for r in output["open-recheck"] if r["metric"] == "open"]))
    if "open-recheck-legacy" in output:
        lines.append("### Open recheck: синхронный legacy протокол, N=7\n")
        lines.append(table(["Fixture", "Вариант", "median ms", "p90 ms", "min ms", "max ms"],
                           [[r["fixture"],r["variant"],*[f'{r[k]:.2f}' for k in ("median","p90","min","max")]] for r in output["open-recheck-legacy"] if r["metric"] == "open"]))
    lines.append("### Scaling: уникальные payload, N=3; медианы MiB\n")
    metrics = ("after_open_private_bytes", "after_open_working_set", "after_open_peak_private_commit", "after_open_peak_working_set")
    lookup = {(r["case"],r["variant"],r["metric"]):r for r in output["scaling-unique"]}
    lines.append(table(["N binaries / decoded MiB", "Вариант", "Private", "WS", "Peak Private", "Peak WS"],
                       [[f'{case} / {next(r["bytes"] for r in output["scaling-unique"] if r["case"]==case)/MIB:.3f}',v,*[f'{lookup[case,v,m]["median"]/MIB:.2f}' for m in metrics]] for case in sorted({r["case"] for r in output["scaling-unique"]}) for v in ("baseline","c2","c3")]))
    lines.append("### OLS fixed + count: только 31.25 MiB, N=50/100/250/500\n\nОписательные process-level fits, не размер нативного объекта.\n")
    lines.append(table(["Content", "Variant", "Metric", "Intercept MiB", "B/entry", "R²"],
                       [[r["experiment"],r["variant"],r["metric"],f'{r["intercept_bytes"]/MIB:.2f}',f'{r["bytes_per_entry"]:.1f}',f'{r["r_squared"]:.3f}'] for r in fits]))
    lines.append("### Причинные вмешательства: original many, N=3; медианы MiB / ms\n")
    causal = output["memory-cause"]
    def med(v,mode,metric):
        return next(r["median"] for r in causal if r["variant"]==v and r["mode"]==mode and r["metric"]==metric)
    lines.append(table(["Variant", "Mode", "Private Open", "Peak Private Open", "Private after GC", "Open ms"],
                       [[v,mode,f'{med(v,mode,"after_open_private_bytes")/MIB:.2f}',f'{med(v,mode,"after_open_peak_private_commit")/MIB:.2f}',f'{med(v,mode,"after_gc_private_bytes")/MIB:.2f}',f'{med(v,mode,"open"):.2f}'] for v in ("baseline","c2","c3") for mode in ("default","periodic-gc","no-bstr-cache")]))
    for fixture in ("many","large"):
        lines.append(f"### Ownership/Undo/Source: {fixture}, один диагностический процесс\n\nBytes exact; PB/WS — MiB. Счётчики объектов не включают COM/MSHTML heap.\n")
        lines.append(table(["Stage", "Current bytes", "History-only bytes", "All unique blocks", "Noncurrent bytes", "PB", "WS"],
                           [[r["stage"],r["current_document_bytes"],r["history_only_bytes"],r["unique_underlying_blocks"],r["noncurrent_owner_bytes"],f'{int(r["private_bytes"])/MIB:.2f}',f'{int(r["working_set"])/MIB:.2f}'] for r in output[fixture+"-stages"]]))
    lines.append("### JScript Open phases, N=3, median ms; времена вложенные\n")
    lines.append(table(["Fixture", "Phase", "C2 ms", "C3 ms", "C2 calls", "C3 calls"],
                       [[f,p,*[f'{next(r["median"] for r in profiles if r["fixture"]==f and r["variant"]==v and r["phase"]==p):.2f}' if any(r["fixture"]==f and r["variant"]==v and r["phase"]==p for r in profiles) else '—' for v in ("c2","c3")],*[str(next(r["calls"] for r in profiles if r["fixture"]==f and r["variant"]==v and r["phase"]==p)) if any(r["fixture"]==f and r["variant"]==v and r["phase"]==p for r in profiles) else '—' for v in ("c2","c3")]] for f in ("control","many","large") for p in ("apiLoadFB2","LoadFromDOM","TransformXML","PutBinaries","apiAddBinary","BuildBinaryControls","RebuildImagesInfo","FillLists","attach_bridge","facade_put_bridge","dom_binary_enumeration")]))
    (args.evidence / "tables.md").write_text("\n".join(lines), encoding="utf-8")
    if args.audit:
        audit(args.evidence)
    if args.plot:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        fig, axes = plt.subplots(2, 2, figsize=(11, 7), constrained_layout=True)
        panels = [("scaling-unique", "after_open_private_bytes", "Unique payload: Private Bytes"),
                  ("scaling-unique", "after_open_peak_private_commit", "Unique payload: Peak Private"),
                  ("scaling-open-only", "after_open_peak_private_commit", "Repeated payload: Peak Private")]
        for ax, (experiment, metric, title) in zip(axes.flat, panels):
            for variant, color in zip(("baseline", "c2", "c3"), ("#666666", "#1976d2", "#e87500")):
                records = sorted((r for r in output[experiment] if r["variant"] == variant and r["metric"] == metric and r["bytes"] == 32768000), key=lambda r: r["count"])
                ax.errorbar([r["count"] for r in records], [r["median"]/MIB for r in records],
                            yerr=[[ (r["median"]-r["min"])/MIB for r in records], [(r["max"]-r["median"])/MIB for r in records]], marker="o", capsize=3, label=variant, color=color)
            ax.set_title(title)
            ax.set(xlabel="Binary count", ylabel="MiB")
            ax.grid(alpha=.25)
            ax.legend()
        ax = axes[1, 1]
        for variant in ("baseline", "c2", "c3"):
            def select(experiment):
                return {r["count"]: r["median"]/MIB for r in output[experiment] if r["variant"] == variant and r["metric"] == "after_open_peak_private_commit" and r["bytes"] == 32768000}
            unique, repeated = select("scaling-unique"), select("scaling-open-only")
            xs = sorted(unique)
            ax.plot(xs, [unique[x]-repeated[x] for x in xs], "o-", label=variant)
        ax.plot(xs, [32768000*8/3*(1-1/x)/MIB for x in xs], "k--", label="UTF-16 base64 size reference")
        ax.set(title="Peak Private: unique minus repeated", xlabel="Binary count", ylabel="MiB")
        ax.grid(alpha=.25)
        ax.legend(fontsize=8)
        fig.suptitle("C4 scaling: 31.25 MiB decoded payload; median and min/max, N=3")
        args.plot.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(args.plot, dpi=150)
        plt.close(fig)


if __name__ == "__main__":
    main()
