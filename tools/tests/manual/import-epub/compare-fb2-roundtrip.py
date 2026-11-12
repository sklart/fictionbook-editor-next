"""Read-only diagnostic comparison of FB2 before and after editor Save/Reopen."""
import argparse
import base64
from collections import Counter
import hashlib
import json
from pathlib import Path
import unicodedata
import xml.etree.ElementTree as ET
from lxml import etree

HREF = '{http://www.w3.org/1999/xlink}href'


def local(tag):
    return tag.rsplit('}', 1)[-1]


def metadata(node):
    return (local(node.tag), tuple(sorted(node.attrib.items())), (node.text or '').strip(),
            tuple(metadata(child) for child in node))


def capture(path):
    root = ET.parse(path).getroot()
    bodies = [node for node in root if local(node.tag) == 'body']
    blocks, structure, inline = [], [], []
    for body in bodies:
        for node in body.iter():
            name = local(node.tag)
            if name in ('p', 'v', 'subtitle', 'text-author'):
                blocks.append((name, ''.join(node.itertext())))
                inline.append(tuple((local(child.tag), ''.join(child.itertext())) for child in node.iter()
                                    if local(child.tag) in ('emphasis', 'strong', 'sup', 'sub', 'strikethrough', 'code', 'style')))
            if name == 'section' or node.get('id') or node.get(HREF):
                structure.append((name, node.get('id'), node.get(HREF), node.get('type')))
    binaries = Counter(hashlib.sha256(base64.b64decode(''.join((node.text or '').split()), validate=True)).hexdigest()
                       for node in root if local(node.tag) == 'binary')
    descriptions = [metadata(node) for node in root if local(node.tag) == 'description']
    return blocks, structure, binaries, descriptions, inline


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--after', type=Path, required=True)
    p.add_argument('--schema', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    args = p.parse_args()
    report = args.report.resolve()
    if report.exists() or report in (args.before.resolve(), args.after.resolve()):
        p.error('report must be a new file distinct from both inputs')
    schema = etree.XMLSchema(etree.parse(str(args.schema)))
    def validate(path):
        return schema.validate(etree.parse(str(path), etree.XMLParser(resolve_entities=False, no_network=True, huge_tree=True)))
    before, after = capture(args.before), capture(args.after)
    def normalized_blocks(blocks):
        return [(tag, ' '.join(unicodedata.normalize('NFC', text).split())) for tag, text in blocks]
    result = {'status': 'REVIEW', 'before_sha256': hashlib.sha256(args.before.read_bytes()).hexdigest(),
              'after_sha256': hashlib.sha256(args.after.read_bytes()).hexdigest(),
              'xsd_before': validate(args.before), 'xsd_after': validate(args.after),
              'blocks_exact': before[0] == after[0],
              'blocks_normalized': normalized_blocks(before[0]) == normalized_blocks(after[0]),
              'structure_ids_links': before[1] == after[1], 'binaries_sha256': before[2] == after[2],
              'metadata_tree': before[3] == after[3], 'inline_formatting_and_text': before[4] == after[4],
              'limitations': ['Whitespace normalization can hide NBSP differences.',
                              'Actual link semantics and visual appearance require separate review.']}
    report.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))


if __name__ == '__main__':
    main()
