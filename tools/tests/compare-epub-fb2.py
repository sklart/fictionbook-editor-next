"""Read-only semantic inventory for a user-supplied EPUB -> FB2 pair.

This is a triage report, not proof of equivalent rendering or correct import.
Reports contain hashes/counts, never book text or input paths.
"""
import argparse
import base64
from collections import Counter
import hashlib
import json
from pathlib import Path
import posixpath
import re
import unicodedata
from urllib.parse import unquote, urlsplit
import xml.etree.ElementTree as ET
import zipfile


def local(tag):
    return tag.rsplit('}', 1)[-1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def normalized(text):
    return ' '.join(unicodedata.normalize('NFC', text).split())


def words(text):
    return Counter(re.findall(r'\w+', normalized(text), re.UNICODE))


def elements(root, name):
    return [node for node in root.iter() if local(node.tag) == name]


def visible(root):
    if local(root.tag) in ('script', 'style'):
        return ''
    text = (root.text or '') + ''.join(visible(child) + (child.tail or '') for child in root)
    return text + (' ' if local(root.tag) in ('p', 'v', 'li', 'td', 'th', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'subtitle', 'text-author') else '')


def block_signature(node):
    return digest(normalized(visible(node)).encode('utf-8'))


def archive_name(base, href):
    path = posixpath.normpath(posixpath.join(posixpath.dirname(base), unquote(urlsplit(href).path)))
    if path == '..' or path.startswith('../') or path.startswith('/'):
        raise ValueError('EPUB resource escapes archive root')
    return path


def epub_inventory(path):
    with zipfile.ZipFile(path) as archive:
        def read(name):
            info = archive.getinfo(name)
            if info.file_size > 64 * 1024 * 1024:
                raise ValueError('EPUB resource exceeds 64 MiB triage limit')
            return archive.read(name)

        container = ET.fromstring(read('META-INF/container.xml'))
        package_name = elements(container, 'rootfile')[0].get('full-path')
        package = ET.fromstring(read(package_name))
        manifest = {node.get('id'): node for node in elements(package, 'item')}
        spine = [manifest[node.get('idref')] for node in elements(package, 'itemref')]
        # Include non-spine XHTML as a separate inventory: notes may live there.
        extra = [node for node in manifest.values() if node not in spine and node.get('media-type') == 'application/xhtml+xml']
        result = {'spine_documents': len(spine), 'non_spine_documents': len(extra), 'headings': 0,
                  'paragraphs': 0, 'links': 0, 'note_references': 0, 'image_references': 0,
                  'block_sequence': [], 'heading_outline': [], 'note_signatures': []}
        text = []; image_hashes = Counter(); targets = set(); references = []
        for item in spine + extra:
            name = archive_name(package_name, item.get('href', ''))
            root = ET.fromstring(read(name))
            bodies = elements(root, 'body')
            body = bodies[0] if bodies else root
            text.append(visible(body))
            for node in body.iter():
                tag = local(node.tag)
                if tag == 'p' or tag in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6'):
                    result['block_sequence'].append(block_signature(node))
                if tag in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6'):
                    result['heading_outline'].append({'level': int(tag[1]), 'text_sha256': block_signature(node)})
                if set(node.get('{http://www.idpf.org/2007/ops}type', '').split()) & {'footnote', 'endnote'} or node.get('role') in ('doc-footnote', 'doc-endnote'):
                    result['note_signatures'].append(block_signature(node))
            result['headings'] += sum(local(node.tag) in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6') for node in body.iter())
            result['paragraphs'] += len(elements(body, 'p'))
            for node in body.iter():
                if node.get('id'):
                    targets.add((name, node.get('id')))
                if local(node.tag) == 'a' and node.get('href'):
                    result['links'] += 1
                    if 'noteref' in node.get('{http://www.idpf.org/2007/ops}type', '').split() or node.get('role') == 'doc-noteref':
                        result['note_references'] += 1
                    href = urlsplit(node.get('href'))
                    if not href.scheme and href.fragment:
                        references.append((archive_name(name, node.get('href')) if href.path else name, unquote(href.fragment)))
                image = node.get('src') if local(node.tag) == 'img' else node.get('{http://www.w3.org/1999/xlink}href') if local(node.tag) == 'image' else None
                if image:
                    result['image_references'] += 1
                    try:
                        image_hashes[digest(read(archive_name(name, image)))] += 1
                    except KeyError:
                        result['missing_image_resources'] = result.get('missing_image_resources', 0) + 1
        result['unresolved_fragment_links'] = sum(ref not in targets for ref in references)
        return result, words(' '.join(text)), image_hashes


def fb2_inventory(path):
    root = ET.fromstring(Path(path).read_bytes())
    bodies = [node for node in root if local(node.tag) == 'body']
    result = {'bodies': len(bodies), 'sections': 0, 'titles': 0, 'paragraphs': 0,
              'links': 0, 'note_references': 0, 'image_references': 0,
              'block_sequence': [], 'heading_outline': [], 'note_signatures': []}
    targets = {node.get('id') for node in root.iter() if node.get('id')}
    references = []; binaries = {}
    for node in elements(root, 'binary'):
        binaries[node.get('id')] = digest(base64.b64decode(''.join((node.text or '').split()), validate=True))
    image_hashes = Counter()
    def outline(node, depth):
        depth += local(node.tag) == 'section'
        if local(node.tag) == 'title':
            result['heading_outline'].append({'level': depth, 'text_sha256': block_signature(node)})
        for child in node:
            outline(child, depth)

    for body in bodies:
        outline(body, 0)
        if body.get('name') in ('notes', 'comments'):
            result['note_signatures'].extend(block_signature(section) for section in body if local(section.tag) == 'section')
        for node in body.iter():
            tag = local(node.tag)
            if tag in ('p', 'v', 'subtitle', 'text-author'):
                result['block_sequence'].append(block_signature(node))
            if tag in ('section', 'title', 'p'):
                result[{'section': 'sections', 'title': 'titles', 'p': 'paragraphs'}[tag]] += 1
            href = node.get('{http://www.w3.org/1999/xlink}href', '')
            if tag == 'a':
                result['links'] += 1
                result['note_references'] += node.get('type') == 'note'
                if href.startswith('#'):
                    references.append(href[1:])
            elif tag == 'image':
                result['image_references'] += 1
                if href.startswith('#') and href[1:] in binaries:
                    image_hashes[binaries[href[1:]]] += 1
                else:
                    result['missing_image_resources'] = result.get('missing_image_resources', 0) + 1
    result['unresolved_fragment_links'] = sum(ref not in targets for ref in references)
    return result, words(' '.join(visible(body) for body in bodies)), image_hashes


def compare(epub, fb2):
    source, source_words, source_images = epub_inventory(epub)
    target, target_words, target_images = fb2_inventory(fb2)
    missing = source_words - target_words
    added = target_words - source_words
    missing_images = source_images - target_images
    differences = {
        'blocks_identical_in_order': source['block_sequence'] == target['block_sequence'],
        'heading_texts_identical_in_order': [item['text_sha256'] for item in source['heading_outline']] == [item['text_sha256'] for item in target['heading_outline']],
        'missing_word_occurrences': sum(missing.values()),
        'added_word_occurrences': sum(added.values()),
        'missing_original_image_occurrences': sum(missing_images.values()),
        'unmatched_image_sha256': sorted(missing_images),
    }
    return {'schema': 1, 'status': 'REVIEW', 'source_sha256': digest(Path(epub).read_bytes()),
            'result_sha256': digest(Path(fb2).read_bytes()), 'epub': source, 'fb2': target,
            'differences': differences,
            'limitations': ['Counts and token coverage cannot prove order, structure, link destinations or visual fidelity.',
                            'Navigation, backlinks, non-spine notes and converted images may cause legitimate differences.',
                            'Compare this report with importer warnings and inspect the actual pair before classifying a defect.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--epub', required=True, type=Path)
    parser.add_argument('--fb2', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    if args.report.resolve() in (args.epub.resolve(), args.fb2.resolve()):
        parser.error('Report must not overwrite either input file')
    report = compare(args.epub, args.fb2)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('REVIEW: EPUB/FB2 inventory written; this tool never declares an import defect fixed.')


if __name__ == '__main__':
    main()
