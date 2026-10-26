"""Independent byte oracle for a diagnostic FB2 roundtrip; no FBE code imports."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

FB = "{http://www.gribuser.ru/xml/fictionbook/2.0}"
XLINK = "{http://www.w3.org/1999/xlink}href"


def manifest(path):
    root = ET.parse(path).getroot()
    result = {}
    for node in root.findall(FB + "binary"):
        identity = node.attrib["id"]
        assert identity not in result, identity
        data = base64.b64decode("".join((node.text or "").split()), validate=True)
        result[identity] = dict(mime=node.attrib["content-type"], size=len(data), sha256=hashlib.sha256(data).hexdigest())
    title = [n.text for n in root.findall("./" + FB + "description/" + FB + "title-info/" + FB + "book-title")]
    references = [n.attrib[XLINK] for n in root.iter() if XLINK in n.attrib]
    paragraphs = [" ".join("".join(n.itertext()).split()) for body in root.findall(FB + "body") for n in body.iter(FB + "p")]
    return dict(binaries=result, title=title, references=references, paragraphs=paragraphs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("expected", type=Path)
    ap.add_argument("actual", type=Path)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    expected, actual = manifest(args.expected), manifest(args.actual)
    assert actual == expected, "Decoded bytes/id/mime/title/references/body text differ"
    args.output.write_text(json.dumps(dict(status="PASS", binary_count=len(actual["binaries"]),
        decoded_bytes=sum(v["size"] for v in actual["binaries"].values()),
        checks="all binary ids, MIME, lengths, SHA256; title, references, normalized paragraph text",
        expected_file_sha256=hashlib.sha256(args.expected.read_bytes()).hexdigest(),
        actual_file_sha256=hashlib.sha256(args.actual.read_bytes()).hexdigest()), indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
