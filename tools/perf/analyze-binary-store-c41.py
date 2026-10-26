"""C4.1 exact representation sizes and controlled process-memory timeline analysis."""
import argparse
import base64
import hashlib
import json
import math
import statistics
import struct
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MIB = 2**20
NS = "{http://www.gribuser.ru/xml/fictionbook/2.0}"


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def representation_sizes(path):
    raw = path.read_bytes()
    text = raw.decode("utf-8-sig")
    xml = ET.fromstring(raw)
    binaries = []
    for node in xml.findall(NS + "binary"):
        content = node.text or ""
        compact = "".join(content.split())
        decoded = base64.b64decode(compact, validate=True)
        expected = 4 * math.ceil(len(decoded) / 3)
        assert len(compact) == expected
        item = dict(id=node.attrib["id"], mime=node.attrib["content-type"],
                    decoded_bytes=len(decoded), base64_characters=len(compact),
                    base64_text_characters_with_whitespace=len(content),
                    base64_utf8_bytes=len(content.encode("utf-8")),
                    base64_utf16_bytes=len(content.encode("utf-16-le")))
        if decoded.startswith(b"\x89PNG\r\n\x1a\n"):
            width, height, depth, color = struct.unpack(">IIBB", decoded[16:26])
            item["png"] = dict(width=width, height=height, source_bit_depth=depth, source_color_type=color,
                               rgb24_dib_estimated_bytes=((width*3+3)//4)*4*height)
        binaries.append(item)
    keys = ("decoded_bytes", "base64_characters", "base64_text_characters_with_whitespace", "base64_utf8_bytes", "base64_utf16_bytes")
    return dict(path=str(path), sha256=hashlib.sha256(raw).hexdigest().upper(),
                binary_count=len(binaries), **{key: sum(x[key] for x in binaries) for key in keys},
                compact_base64_utf8_bytes=sum(x["base64_characters"] for x in binaries),
                compact_base64_utf16_bytes=sum(x["base64_characters"]*2 for x in binaries),
                full_fb2_utf8_file_bytes=len(raw), full_fixture_xml_utf16_bytes=len(text.encode("utf-16-le")),
                formulas={"base64": "sum(4*ceil(decoded_i/3))", "compact_utf16": "2*base64_characters", "full_fixture_utf16": "len(UTF8-decode(file).encode(UTF16-LE)) without a terminator", "runtime_source_utf8": "SCI_GETLENGTH; collected separately"},
                binaries=binaries)


def distribution(values):
    values = sorted(values)
    return dict(n=len(values), median=statistics.median(values), min=values[0], max=values[-1], p90=values[math.ceil(.9*len(values))-1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, default=ROOT / "out/perf/c41")
    args = parser.parse_args()
    fixtures = {name: ROOT / f"out/perf/results-2f0113b2-final64-uiwait/{name}.fb2" for name in ("control", "many", "large")}
    fixtures.update({"many-identical": ROOT / "out/perf/c4/scaling-open-only/n500-32m.fb2", "many-unique": ROOT / "out/perf/c4/scaling-unique/n500-32m.fb2"})
    sizes = {name: representation_sizes(path) for name, path in fixtures.items()}
    args.evidence.mkdir(parents=True, exist_ok=True)
    (args.evidence / "representation-sizes.json").write_text(json.dumps(sizes, indent=2), encoding="utf-8")
    groups, runs, differences = {}, [], []
    pairs = {
        "open_minus_initialized": ("process_initialized", "after_open"),
        "open_gc_minus_initialized": ("process_initialized", "after_open_gc_idle"),
        "native_one": ("before_native_one", "native_one_variant_live"),
        "native_all_retained": ("native_all_sequential_released", "native_all_variants_live"),
        "native_all_release": ("native_all_variants_live", "native_all_variants_released"),
        "js_all_retained": ("js_discard_after_gc", "js_all_retained"),
        "js_logical_clear": ("js_all_retained", "js_array_cleared"),
        "js_clear_gc": ("js_array_cleared", "js_clear_after_gc"),
        "create_dom": ("before_create_dom", "created_xml_dom_live_no_xml_getter"),
        "xml_bstr": ("created_xml_dom_live_no_xml_getter", "xml_dom_and_bstr_live"),
        "bstr_release": ("xml_dom_and_bstr_live", "xml_bstr_released_dom_live"),
        "dom_release": ("xml_bstr_released_dom_live", "xml_dom_released"),
        "xml_gc": ("xml_dom_released", "xml_release_after_gc"),
        "xml_idle": ("xml_release_after_gc", "xml_release_after_idle"),
        "source_visible": ("before_source_gc", "source_visible"),
        "return_body": ("source_stable", "body_after_readonly_source"),
        "source_cache_release": ("body_after_source_idle", "source_xml_and_text_cache_released"),
        "scintilla_doc_release": ("source_xml_and_text_cache_released", "source_scintilla_document_released"),
        "source_release_gc": ("source_scintilla_document_released", "source_release_after_gc"),
        "source_release_idle": ("source_release_after_gc", "source_release_after_idle"),
    }
    for experiment in ("timeline", "content", "control-fixed"):
        folder = args.evidence / experiment
        if not (folder / "manifest.json").exists():
            continue
        manifest = load(folder / "manifest.json")
        if manifest["status"] != "PASS":
            continue
        for result in manifest["results"]:
            fixture, repeat = result["fixture"], result["repeat"]
            stages = load(folder / f"{fixture}-{repeat}-stages.json")
            by_stage = {s["stage"]: s for s in stages}
            initial = by_stage["process_initialized"]
            for name, (before, after) in pairs.items():
                if before in by_stage and after in by_stage:
                    differences.append(dict(experiment=experiment, fixture=fixture, repeat=repeat, pair=name,
                                            private_delta=by_stage[after]["private_bytes"]-by_stage[before]["private_bytes"],
                                            working_set_delta=by_stage[after]["working_set"]-by_stage[before]["working_set"]))
            for stage in stages:
                assert stage["canonical_bytes"] == (0 if stage["stage"] == "process_initialized" else sizes[fixture]["decoded_bytes"]), (fixture, stage["stage"])
                key = (experiment, fixture, stage["stage"])
                for metric, value in stage.items():
                    if metric not in ("stage", "qpc", "elapsed_ms"):
                        groups.setdefault((*key, metric), []).append(value)
            runs.append(dict(experiment=experiment, fixture=fixture, repeat=repeat, stages=stages,
                             baseline_private_bytes=initial["private_bytes"]))
    summary = [dict(experiment=e, fixture=f, stage=s, metric=m, **distribution(values)) for (e, f, s, m), values in groups.items()]
    delta_groups = {}
    for item in differences:
        delta_groups.setdefault((item["experiment"],item["fixture"],item["pair"]), []).append(item["private_delta"])
    delta_summary = [dict(experiment=e, fixture=f, pair=p, **distribution(v)) for (e,f,p),v in delta_groups.items()]
    analysis = dict(summary=summary, runs=runs, paired_differences=differences, paired_private_delta_summary=delta_summary)
    (args.evidence / "timeline-analysis.json").write_text(json.dumps(analysis, indent=2), encoding="utf-8")
    def table(headers, rows):
        return "| " + " | ".join(headers) + " |\n|" + "|".join("---" for _ in headers) + "|\n" + "\n".join("| " + " | ".join(str(x) for x in row) + " |" for row in rows) + "\n"
    lines = ["## Численные приложения C4.1\n", "### Фактические размеры входных представлений (bytes)\n"]
    lines.append(table(["Fixture","Decoded","base64 chars","base64 UTF-16","File UTF-8","Fixture XML UTF-16"],
                       [[n,*[sizes[n][k] for k in ("decoded_bytes","base64_characters","base64_utf16_bytes","full_fb2_utf8_file_bytes","full_fixture_xml_utf16_bytes")]] for n in fixtures]))
    for fixture in ("many", "large"):
        selected = [r for r in summary if r["experiment"]=="timeline" and r["fixture"]==fixture]
        if not selected:
            continue
        lookup = {(r["stage"],r["metric"]):r for r in selected}
        order = [s["stage"] for r in runs if r["experiment"]=="timeline" and r["fixture"]==fixture and r["repeat"]==1 for s in r["stages"]]
        lines.append(f"### {fixture}: три отдельных процесса, медианы MiB\n\nPrivate/WS/peaks — process counters; logical columns не являются полным heap breakdown.\n")
        metrics = ("canonical_bytes","history_only_bytes","local_xml_bstr_live_bytes","known_point_bytes","source_cached_utf16_bytes","source_utf8_bytes","native_returned_live_bytes","js_reachable_returned_bytes","private_bytes","working_set","peak_private_bytes","peak_working_set")
        lines.append("Point bytes — локальный scope hook (BSTR или UTF-8 vector по имени состояния); это отдельное измерение, не дополнительная независимая копия.\n")
        lines.append(table(["State","Canonical","History","Local XML BSTR","Point bytes","UTF16 cache","Source UTF8","Native values","JS reachable","Private","WS","Peak Private","Peak WS"],
                           [[stage,*[f'{lookup[stage,m]["median"]/MIB:.2f}' for m in metrics]] for stage in order]))
        lines.append(f"### {fixture}: paired Private deltas, MiB (after−before), N=3\n\nКаждая разность вычислена внутри одного процесса; это не точный размер объекта.\n")
        lines.append(table(["Pair","median","p90","min","max","N"],[[r["pair"],*[f'{r[k]/MIB:.2f}' for k in ("median","p90","min","max")],r["n"]] for r in delta_summary if r["experiment"]=="timeline" and r["fixture"]==fixture]))
    content = [r for r in summary if r["experiment"]=="content" and r["metric"] in ("private_bytes","peak_private_bytes") and r["stage"] in ("after_open","after_open_gc_idle")]
    lines.append("### Identical vs unique: N=3, MiB\n")
    lines.append(table(["Fixture","State","Metric","median","min","max"],[[r["fixture"],r["stage"],r["metric"],*[f'{r[k]/MIB:.2f}' for k in ("median","min","max")]] for r in content]))
    (args.evidence / "tables.md").write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
