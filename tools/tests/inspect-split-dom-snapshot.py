"""Inspect saved MSHTML fragments without contacting or changing the editor.

This is an offline oracle, not a live DOM observer. It preserves text, sibling
order and all attributes; only HTML tag/attribute case and attribute order are
normalized. Callers must document any path redaction performed before input.
"""
import argparse
import hashlib
import json
from collections import Counter
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit


VOID = frozenset("area base br col embed hr img input link meta param source track wbr".split())


class Tree(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.root = {"tag": "#fragment", "attributes": [], "children": []}
        self.stack = [self.root]

    def handle_starttag(self, tag, attrs):
        node = {"tag": tag, "attributes": sorted([list(a) for a in attrs], key=lambda item: (item[0], item[1] is not None, item[1] or "")), "children": []}
        self.stack[-1]["children"].append(node)
        if tag not in VOID:
            self.stack.append(node)

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if tag not in VOID:
            self.handle_endtag(tag)

    def handle_endtag(self, tag):
        if len(self.stack) == 1 or self.stack[-1]["tag"] != tag:
            raise ValueError(f"Unbalanced HTML closing tag: {tag}")
        self.stack.pop()

    def handle_data(self, data):
        self.stack[-1]["children"].append({"text": data})

    def handle_comment(self, data):
        self.stack[-1]["children"].append({"comment": data})


def analyze(path):
    parser = Tree()
    parser.feed(path.read_text(encoding="utf-8"))
    if parser.rawdata:
        raise ValueError("Incomplete HTML token")
    parser.close()
    if len(parser.stack) != 1:
        raise ValueError("Unclosed elements in HTML fragment")
    ids, tags, links, elements, text, violations = Counter(), Counter(), [], [], [], []

    def visit(node, parent, index):
        if "text" in node:
            text.append(node["text"])
            return
        if "tag" not in node:
            return
        attrs = dict(node["attributes"])
        tag = node["tag"]
        parent_tag = parent.get("tag") if parent else None
        parent_attrs = dict(parent.get("attributes", [])) if parent else {}
        parent_classes = (parent_attrs.get("class") or "").lower().split()
        if tag == "p" and (parent_tag == "body" or (parent_tag == "div" and "body" in parent_classes)):
            violations.append({"kind": "paragraph_directly_in_body", "parent": parent_tag, "index": index})
        if tag != "#fragment":
            tags[tag] += 1
            if attrs.get("id"):
                ids[attrs["id"]] += 1
            if attrs.get("href") and "#" in attrs["href"]:
                links.append({"href": attrs["href"], "target": urlsplit(attrs["href"]).fragment})
            elements.append({"parent": parent_tag, "index": index, "tag": tag, "attributes": node["attributes"]})
        for i, child in enumerate(node["children"]):
            visit(child, node, i)

    visit(parser.root, None, 0)
    canonical = json.dumps(parser.root, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return {"input": str(path), "canonical_sha256": hashlib.sha256(canonical.encode("utf-8")).hexdigest(),
            "canonical_tree": parser.root, "elements": elements, "full_text": "".join(text),
            "tag_counts": dict(tags), "id_counts": dict(ids), "duplicate_ids": {k: v for k, v in ids.items() if v > 1},
            "fragment_links": [{**link, "matching_ids": ids[link["target"]]} for link in links],
            "internal_links": [{**link, "matching_ids": ids[link["target"]]} for link in links if link["href"].startswith("#")],
            "structure_violations": violations}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("snapshot", type=Path)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    if args.snapshot.resolve() == args.output.resolve():
        ap.error("output must not overwrite the input snapshot")
    args.output.write_text(json.dumps(analyze(args.snapshot), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
