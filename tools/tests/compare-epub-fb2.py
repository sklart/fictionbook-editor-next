"""Read-only semantic inventory for a user-supplied EPUB -> FB2 pair.

This is a triage report, not proof of equivalent rendering or correct import.
Reports contain hashes/counts, never book text or input paths.
"""
import argparse
import base64
from bisect import bisect_left
from collections import Counter
import hashlib
import html.entities
import itertools
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


def significant_characters(text):
    """Count punctuation and selected spaces without folding them together."""
    return Counter(f'U+{ord(char):04X}' for char in unicodedata.normalize('NFC', text)
                   if unicodedata.category(char).startswith('P') or char in '\u00a0\u202f')


def nonspace_characters(text):
    return Counter(char for char in unicodedata.normalize('NFC', text) if not char.isspace())


def metadata_hashes(values):
    return [digest(normalized(value).encode('utf-8')) for value in values if normalized(value)]


def epub_metadata(package):
    metadata = next((node for node in package if local(node.tag) == 'metadata'), None)
    result = {key: [] for key in ('title', 'author', 'language', 'description', 'identifier', 'publisher', 'date')}
    if metadata is None: return result
    mapping = {'title': 'title', 'creator': 'author', 'language': 'language', 'description': 'description',
               'identifier': 'identifier', 'publisher': 'publisher', 'date': 'date'}
    for node in metadata:
        name = local(node.tag)
        if name in mapping:
            result[mapping[name]].append(''.join(node.itertext()))
    return {key: metadata_hashes(value) for key, value in result.items()}


def fb2_metadata(root):
    result = {key: [] for key in ('title', 'author', 'language', 'description', 'identifier', 'publisher', 'date')}
    description = next((node for node in root if local(node.tag) == 'description'), None)
    if description is None: return result
    sections = {local(node.tag): node for node in description}
    title = sections.get('title-info')
    if title is not None:
        for node in title:
            name = local(node.tag)
            if name == 'book-title': result['title'].append(''.join(node.itertext()))
            elif name == 'lang': result['language'].append(''.join(node.itertext()))
            elif name == 'annotation': result['description'].append(visible(node))
            elif name == 'author':
                result['author'].append(' '.join(''.join(child.itertext()) for child in node
                                                 if local(child.tag) in ('first-name', 'middle-name', 'last-name', 'nickname')))
    document = sections.get('document-info')
    if document is not None:
        result['identifier'].extend(''.join(node.itertext()) for node in document if local(node.tag) == 'id')
    publish = sections.get('publish-info')
    if publish is not None:
        for node in publish:
            if local(node.tag) == 'publisher': result['publisher'].append(''.join(node.itertext()))
            elif local(node.tag) == 'year': result['date'].append(''.join(node.itertext()))
    return {key: metadata_hashes(value) for key, value in result.items()}


def elements(root, name):
    return [node for node in root.iter() if local(node.tag) == name]


def visible(root):
    if local(root.tag) in ('script', 'style'):
        return ''
    text = (root.text or '') + ''.join(visible(child) + (child.tail or '') for child in root)
    return text + (' ' if local(root.tag) in ('br', 'p', 'v', 'li', 'td', 'th', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'subtitle', 'text-author') else '')


def word_visible(root):
    """Separate block boundaries without splitting words across inline markup."""
    tag = local(root.tag).lower()
    if tag in ('script', 'style'):
        return ''
    blocks = {'body', 'section', 'div', 'article', 'aside', 'p', 'pre', 'blockquote',
              'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'ul', 'ol', 'li', 'table',
              'thead', 'tbody', 'tfoot', 'tr', 'td', 'th', 'br', 'title',
              'subtitle', 'text-author', 'poem', 'stanza', 'v', 'cite', 'epigraph'}
    content = (root.text or '') + ''.join(word_visible(child) + (child.tail or '') for child in root)
    return ' ' + content + ' ' if tag in blocks else content


def block_signature(node):
    return digest(normalized(visible(node)).encode('utf-8'))


def target_signature(node):
    """Hash a bounded target subtree, leaving large/empty targets ambiguous."""
    if sum(1 for _ in itertools.islice(node.iter(), 1001)) > 1000:
        return None
    text = normalized(visible(node))
    return digest(text.encode('utf-8')) if text else None


def inline_semantic_signature(node):
    """Hash text and directly declared emphasis; external CSS is not resolved."""
    runs = []
    def append(text, italic, bold):
        if text:
            runs.extend((char, italic, bold) for char in unicodedata.normalize('NFC', text) if not char.isspace())
    def walk(element, italic, bold):
        tag = local(element.tag)
        if tag in ('em', 'i', 'emphasis'): italic = True
        if tag in ('strong', 'b'): bold = True
        declarations = {}
        for declaration in element.get('style', '').split(';'):
            if ':' not in declaration: continue
            key, value = declaration.split(':', 1)
            key, value = key.strip().lower(), value.strip().lower()
            if key not in ('font-style', 'font-weight'): continue
            important = value.endswith('!important')
            if important: value = value[:-10].strip()
            if key not in declarations or important or not declarations[key][0]:
                declarations[key] = (important, value)
        if 'font-style' in declarations:
            value = declarations['font-style'][1]
            if value in ('italic', 'oblique'): italic = True
            elif value == 'normal': italic = False
        if 'font-weight' in declarations:
            value = declarations['font-weight'][1]
            if value in ('bold', 'bolder') or (value.isdigit() and int(value) >= 600): bold = True
            elif value in ('normal', 'lighter') or (value.isdigit() and int(value) < 600): bold = False
        append(element.text, italic, bold)
        for child in element:
            walk(child, italic, bold)
            append(child.tail, italic, bold)
    walk(node, False, False)
    encoded = ''.join(char + ('I' if italic else 'i') + ('B' if bold else 'b') for char, italic, bold in runs)
    return digest(encoded.encode('utf-8'))


def block_details(node, **origin):
    text = visible(node).rstrip('\r\n')
    semantic = Counter()
    for child in node.iter():
        name = local(child.tag)
        if name in ('em', 'i', 'emphasis', 'strong', 'b', 'sup', 'sub', 'strike', 'strikethrough', 's', 'code'):
            semantic[name] += 1
    return {**origin, 'element': local(node.tag), 'id': node.get('id'),
            'normalized_sha256': block_signature(node),
            'strict_nfc_sha256': digest(unicodedata.normalize('NFC', text).encode('utf-8')),
            'direct_inline_semantics_sha256': inline_semantic_signature(node),
            'inline_elements': dict(semantic)}


def source_provenance(root):
    """Record structural paths and explicitly hidden ancestors for triage.

    This intentionally does not pretend to evaluate external CSS or infer
    whether aria-hidden is visually hidden.
    """
    positions = {}
    def walk(node, path, hidden):
        style = re.sub(r'\s+', '', node.get('style', '').lower())
        hidden = hidden or 'hidden' in node.attrib or bool(re.search(r'(?:^|;)(?:display:none|visibility:hidden)(?:!important)?(?:;|$)', style))
        positions[node] = (path, hidden)
        siblings = Counter()
        for child in node:
            name = local(child.tag)
            siblings[name] += 1
            walk(child, f'{path}/{name}[{siblings[name]}]', hidden)
    walk(root, f'/{local(root.tag)}[1]', False)
    return positions


def compare_block_sequences(source, target):
    """Report candidate differences without treating changed boundaries as loss.

    Unique common blocks are used as order anchors; an LIS detects a lower
    bound for out-of-order anchors in O(n log n), including for long books.
    Repeated blocks are excluded from ordering because their pairing is
    ambiguous. Hashes preserve punctuation and NBSP only as far as the
    existing normalized() function does; they are not strict text equality.
    """
    left, right = Counter(source), Counter(target)
    positions = {value: index for index, value in enumerate(target) if right[value] == 1}
    anchors = [positions[value] for value in source
               if left[value] == 1 and value in positions]
    tails = []
    for position in anchors:
        index = bisect_left(tails, position)
        if index == len(tails):
            tails.append(position)
        else:
            tails[index] = position
    return {
        'source_blocks': len(source), 'target_blocks': len(target),
        'unmatched_source_occurrences': sum((left - right).values()),
        'unmatched_target_occurrences': sum((right - left).values()),
        'extra_repeated_target_occurrences': sum(max(0, count - left[key])
                                                 for key, count in right.items()
                                                 if count > 1 and left[key] > 0),
        'unique_common_anchors': len(anchors),
        'unique_anchors_outside_ordered_subsequence': len(anchors) - len(tails),
        'interpretation': 'Candidates only: changed block boundaries, navigation and notes can produce differences.'
    }


def compare_matched_block_details(source, target):
    """Compare only unambiguous text matches; CSS-derived style is not inferred."""
    left = Counter(item['normalized_sha256'] for item in source)
    right = Counter(item['normalized_sha256'] for item in target)
    lookup = {item['normalized_sha256']: item for item in target if right[item['normalized_sha256']] == 1}
    strict_changes = style_changes = semantic_changes = matched = 0
    def styles(item):
        names = item['inline_elements']
        return {'italic': names.get('i', 0) + names.get('em', 0) + names.get('emphasis', 0),
                'bold': names.get('b', 0) + names.get('strong', 0),
                'sup': names.get('sup', 0), 'sub': names.get('sub', 0),
                'strike': names.get('s', 0) + names.get('strike', 0) + names.get('strikethrough', 0),
                'code': names.get('code', 0)}
    for item in source:
        key = item['normalized_sha256']
        if left[key] != 1 or key not in lookup:
            continue
        other = lookup[key]
        matched += 1
        strict_changes += item['strict_nfc_sha256'] != other['strict_nfc_sha256']
        style_changes += styles(item) != styles(other)
        semantic_changes += item['direct_inline_semantics_sha256'] != other['direct_inline_semantics_sha256']
    return {'unique_normalized_matches': matched, 'strict_nfc_mismatches': strict_changes,
            'inline_markup_mismatches': style_changes,
            'direct_inline_semantic_mismatches': semantic_changes,
            'limitations': 'Explicit inline element counts only; CSS and split/merged blocks are not compared.'}


def compare_link_targets(source, target):
    """Match only labels occurring once in each document; never match IDs."""
    left = Counter(link['label_sha256'] for link in source)
    right = Counter(link['label_sha256'] for link in target)
    lookup = {link['label_sha256']: link for link in target if right[link['label_sha256']] == 1}
    comparable = changed = unavailable = note_changed = 0
    for link in source:
        label = link['label_sha256']
        if left[label] != 1 or label not in lookup:
            continue
        other = lookup[label]
        note_changed += link['note'] != other['note']
        if link['target_text_sha256'] and other['target_text_sha256']:
            comparable += 1
            changed += link['target_text_sha256'] != other['target_text_sha256']
        else:
            unavailable += 1
    return {'unique_labels_with_comparable_targets': comparable, 'candidate_target_text_differences': changed,
            'unique_labels_with_unavailable_targets': unavailable, 'candidate_note_classification_differences': note_changed,
            'limitations': 'Target text may legitimately change during conversion; repeated labels and large targets are ambiguous.'}


def archive_name(base, href):
    path = posixpath.normpath(posixpath.join(posixpath.dirname(base), unquote(urlsplit(href).path)))
    if path == '..' or path.startswith('../') or path.startswith('/'):
        raise ValueError('EPUB resource escapes archive root')
    return path


def expand_xhtml_entities(data):
    """Expand known HTML entities without altering comments, CDATA or markup."""
    def replace(match):
        if match.group(1) is None:
            return match.group(0)
        name = match.group(1).decode('ascii')
        if name in ('amp', 'lt', 'gt', 'quot', 'apos'):
            return match.group(0)
        value = html.entities.html5.get(name + ';')
        if value is None:
            return match.group(0)
        return ''.join(f'&#x{ord(char):X};' for char in value).encode('ascii')
    pattern = rb'<!\[CDATA\[.*?\]\]>|<!--.*?-->|<\?.*?\?>|<!DOCTYPE(?:[^\[\]>]|\[.*?\])*>|&([A-Za-z][A-Za-z0-9]+);'
    return re.sub(pattern, replace, data, flags=re.DOTALL)


def parse_xhtml(data):
    """Read XML without fetching DTDs; expand standard HTML entities on demand."""
    try:
        return ET.fromstring(data)
    except ET.ParseError as error:
        if 'undefined entity' not in str(error):
            raise
    return ET.fromstring(expand_xhtml_entities(data))


def epub_inventory(path, include_word_roles=False, skip_malformed_xhtml=False, skip_unsafe_references=False, recover_malformed_xhtml=False):
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
        itemrefs = elements(package, 'itemref')
        invalid_itemrefs = sum(not node.get('idref') or node.get('idref') not in manifest for node in itemrefs)
        spine = [manifest[node.get('idref')] for node in itemrefs if node.get('idref') and node.get('idref') in manifest]
        # Include non-spine XHTML as a separate inventory: notes may live there.
        extra = [node for node in manifest.values() if node not in spine and node.get('media-type') == 'application/xhtml+xml']
        result = {'spine_documents': len(spine), 'invalid_spine_itemrefs': invalid_itemrefs,
                  'non_spine_documents': len(extra), 'headings': 0,
                  'paragraphs': 0, 'links': 0, 'note_references': 0, 'image_references': 0,
                  'non_xhtml_spine_items': 0, 'block_sequence': [], 'blocks': [],
                  'heading_outline': [], 'note_signatures': [], 'internal_link_targets': []}
        result['metadata_hashes'] = epub_metadata(package)
        text = []; text_by_role = {'spine': [], 'navigation': [], 'other_non_spine': []}
        image_hashes = Counter(); targets = set(); target_contents = {}; duplicate_targets = set(); references = []
        for item in spine + extra:
            try:
                name = archive_name(package_name, item.get('href', ''))
            except ValueError:
                if not skip_unsafe_references: raise
                result.setdefault('skipped_unsafe_references', []).append({'document': package_name, 'kind': 'manifest', 'href': item.get('href', '')})
                continue
            media = item.get('media-type', '').lower()
            if media not in ('application/xhtml+xml', 'text/html'):
                result['non_xhtml_spine_items'] += 1
                if media.startswith('image/'):
                    result['image_references'] += 1
                    try:
                        image_hashes[digest(read(name))] += 1
                    except KeyError:
                        result['missing_image_resources'] = result.get('missing_image_resources', 0) + 1
                continue
            raw_xhtml = read(name)
            try:
                root = parse_xhtml(raw_xhtml)
            except ET.ParseError as error:
                if recover_malformed_xhtml:
                    from lxml import etree
                    parser = etree.XMLParser(recover=True, resolve_entities=False, no_network=True, load_dtd=False)
                    recovered = etree.fromstring(expand_xhtml_entities(raw_xhtml), parser)
                    if recovered is None:
                        raise ValueError('Malformed XHTML could not be recovered: ' + name) from error
                    root = parse_xhtml(etree.tostring(recovered, encoding='utf-8'))
                    oversized_headings = sum(len(''.join(node.itertext())) > 4096 for node in root.iter()
                                             if local(node.tag) in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6'))
                    result.setdefault('recovered_malformed_documents', []).append(
                        {'path': name, 'role': 'spine' if item in spine else 'non_spine', 'error': str(error),
                         'recovery_diagnostics': len(parser.error_log), 'oversized_headings': oversized_headings})
                elif not skip_malformed_xhtml:
                    raise
                else:
                    result.setdefault('skipped_malformed_documents', []).append(
                        {'path': name, 'role': 'spine' if item in spine else 'non_spine', 'error': str(error)})
                    continue
            bodies = elements(root, 'body')
            body = bodies[0] if bodies else root
            body_text = word_visible(body)
            text.append(body_text)
            role = 'navigation' if 'nav' in item.get('properties', '').split() else 'spine' if item in spine else 'other_non_spine'
            text_by_role[role].append(body_text)
            provenance = source_provenance(body)
            for node in body.iter():
                tag = local(node.tag)
                if tag == 'p' or tag in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6'):
                    result['block_sequence'].append(block_signature(node))
                    node_path, hidden = provenance[node]
                    result['blocks'].append(block_details(node, document=name, document_role=role,
                                                          tree_path=node_path, explicitly_hidden=hidden, spine=item in spine,
                                                          ordinal=len(result['blocks'])))
                if tag in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6'):
                    result['heading_outline'].append({'level': int(tag[1]), 'text_sha256': block_signature(node)})
                if set(node.get('{http://www.idpf.org/2007/ops}type', '').split()) & {'footnote', 'endnote'} or node.get('role') in ('doc-footnote', 'doc-endnote'):
                    result['note_signatures'].append(block_signature(node))
            result['headings'] += sum(local(node.tag) in ('h1', 'h2', 'h3', 'h4', 'h5', 'h6') for node in body.iter())
            result['paragraphs'] += len(elements(body, 'p'))
            for node in body.iter():
                if node.get('id'):
                    key = (name, node.get('id'))
                    if key in targets: duplicate_targets.add(key)
                    targets.add(key)
                    if local(node.tag) in ('p', 'li', 'div', 'section', 'aside', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6'):
                        target_contents[key] = target_signature(node)
                if local(node.tag) == 'a' and node.get('href'):
                    result['links'] += 1
                    if 'noteref' in node.get('{http://www.idpf.org/2007/ops}type', '').split() or node.get('role') == 'doc-noteref':
                        result['note_references'] += 1
                    href = urlsplit(node.get('href'))
                    if not href.scheme and href.fragment:
                        try:
                            key = (archive_name(name, node.get('href')) if href.path else name, unquote(href.fragment))
                        except ValueError:
                            if not skip_unsafe_references: raise
                            result.setdefault('skipped_unsafe_references', []).append({'document': name, 'kind': 'link', 'href': node.get('href')})
                            continue
                        references.append((key, block_signature(node),
                                           'noteref' in node.get('{http://www.idpf.org/2007/ops}type', '').split() or node.get('role') == 'doc-noteref'))
                image = node.get('src') if local(node.tag) == 'img' else node.get('{http://www.w3.org/1999/xlink}href') if local(node.tag) == 'image' else None
                if image:
                    result['image_references'] += 1
                    try:
                        image_hashes[digest(read(archive_name(name, image)))] += 1
                    except KeyError:
                        result['missing_image_resources'] = result.get('missing_image_resources', 0) + 1
                    except ValueError:
                        if not skip_unsafe_references: raise
                        result.setdefault('skipped_unsafe_references', []).append({'document': name, 'kind': 'image', 'href': image})
        result['unresolved_fragment_links'] = sum(key not in targets for key, _, _ in references)
        result['internal_link_targets'] = [{'label_sha256': label, 'target_text_sha256': target_contents.get(key) if key not in duplicate_targets else None,
                                            'target_exists': key in targets, 'note': note}
                                           for key, label, note in references]
        combined = ' '.join(text)
        result['significant_characters'] = dict(significant_characters(combined))
        role_words = {key: words(' '.join(value)) for key, value in text_by_role.items()}
        result['word_occurrences_by_document_role'] = {key: sum(value.values()) for key, value in role_words.items()}
        inventory = (result, words(combined), image_hashes)
        return (*inventory, role_words, nonspace_characters(combined)) if include_word_roles else inventory


def fb2_inventory(path, include_characters=False):
    root = ET.fromstring(Path(path).read_bytes())
    bodies = [node for node in root if local(node.tag) == 'body']
    result = {'bodies': len(bodies), 'sections': 0, 'titles': 0, 'paragraphs': 0,
              'links': 0, 'note_references': 0, 'image_references': 0, 'relative_xhtml_link_candidates': 0,
              'block_sequence': [], 'blocks': [], 'heading_outline': [], 'note_signatures': [], 'internal_link_targets': []}
    result['metadata_hashes'] = fb2_metadata(root)
    id_nodes = [node for node in root.iter() if node.get('id')]
    id_counts = Counter(node.get('id') for node in id_nodes)
    targets = set(id_counts)
    target_contents = {node.get('id'): target_signature(node) for node in id_nodes
                       if id_counts[node.get('id')] == 1 and local(node.tag) in ('p', 'section', 'subtitle', 'v')}
    references = []; binaries = {}
    for node in elements(root, 'binary'):
        binaries[node.get('id')] = digest(base64.b64decode(''.join((node.text or '').split()), validate=True))
    image_hashes = Counter()
    result['cover_image_references'] = 0
    for cover in elements(root, 'coverpage'):
        for image in elements(cover, 'image'):
            result['cover_image_references'] += 1
            href = image.get('{http://www.w3.org/1999/xlink}href', '')
            if href.startswith('#') and href[1:] in binaries:
                image_hashes[binaries[href[1:]]] += 1
            else:
                result['missing_image_resources'] = result.get('missing_image_resources', 0) + 1
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
                result['blocks'].append(block_details(node, body=body.get('name') or 'main',
                                                      ordinal=len(result['blocks'])))
            if tag in ('section', 'title', 'p'):
                result[{'section': 'sections', 'title': 'titles', 'p': 'paragraphs'}[tag]] += 1
            href = node.get('{http://www.w3.org/1999/xlink}href', '')
            if tag == 'a':
                result['links'] += 1
                result['note_references'] += node.get('type') == 'note'
                if href.startswith('#'):
                    references.append((href[1:], block_signature(node), node.get('type') == 'note'))
                elif href:
                    parsed = urlsplit(href)
                    path = unquote(parsed.path).lower()
                    if not parsed.scheme and not parsed.netloc and not path.startswith(('/', '\\')) and path.endswith(('.xhtml', '.html', '.htm')):
                        result['relative_xhtml_link_candidates'] += 1
            elif tag == 'image':
                result['image_references'] += 1
                if href.startswith('#') and href[1:] in binaries:
                    image_hashes[binaries[href[1:]]] += 1
                else:
                    result['missing_image_resources'] = result.get('missing_image_resources', 0) + 1
    result['unresolved_fragment_links'] = sum(key not in targets for key, _, _ in references)
    result['internal_link_targets'] = [{'label_sha256': label, 'target_text_sha256': target_contents.get(key),
                                        'target_exists': key in targets, 'note': note}
                                       for key, label, note in references]
    combined = ' '.join(word_visible(body) for body in bodies)
    result['significant_characters'] = dict(significant_characters(combined))
    inventory = (result, words(combined), image_hashes)
    return (*inventory, nonspace_characters(combined)) if include_characters else inventory


def compare(epub, fb2, skip_malformed_xhtml=False, skip_unsafe_references=False, recover_malformed_xhtml=False):
    source, source_words, source_images, source_roles, source_characters = epub_inventory(
        epub, include_word_roles=True, skip_malformed_xhtml=skip_malformed_xhtml, skip_unsafe_references=skip_unsafe_references, recover_malformed_xhtml=recover_malformed_xhtml)
    target, target_words, target_images, target_characters = fb2_inventory(fb2, include_characters=True)
    missing = source_words - target_words
    added = target_words - source_words
    missing_images = source_images - target_images
    source_chars = Counter(source['significant_characters'])
    target_chars = Counter(target['significant_characters'])
    differences = {
        'blocks_identical_in_order': source['block_sequence'] == target['block_sequence'],
        'heading_texts_identical_in_order': [item['text_sha256'] for item in source['heading_outline']] == [item['text_sha256'] for item in target['heading_outline']],
        'missing_word_occurrences': sum(missing.values()),
        'missing_spine_word_occurrences': sum((source_roles['spine'] - target_words).values()),
        'missing_nonspace_character_occurrences': sum((source_characters - target_characters).values()),
        'added_nonspace_character_occurrences': sum((target_characters - source_characters).values()),
        'added_word_occurrences': sum(added.values()),
        'missing_original_image_occurrences': sum(missing_images.values()),
        'missing_distinct_image_sha256': sorted(set(source_images) - set(target_images)),
        'missing_epub_image_resource_occurrences': source.get('missing_image_resources', 0),
        'unmatched_image_sha256': sorted(missing_images),
        'significant_character_deficits': dict(source_chars - target_chars),
        'significant_character_excesses': dict(target_chars - source_chars),
        'block_sequence_analysis': compare_block_sequences(source['block_sequence'], target['block_sequence']),
        'matched_block_details': compare_matched_block_details(source['blocks'], target['blocks']),
        'internal_link_target_analysis': compare_link_targets(source['internal_link_targets'], target['internal_link_targets']),
        'metadata_analysis': {key: {'source_values': len(source['metadata_hashes'][key]),
                                    'target_values': len(target['metadata_hashes'][key]),
                                    'matching_hash_occurrences': sum((Counter(source['metadata_hashes'][key]) & Counter(target['metadata_hashes'][key])).values())}
                              for key in source['metadata_hashes']},
    }
    return {'schema': 1, 'status': 'PARTIAL' if source.get('skipped_malformed_documents') or source.get('skipped_unsafe_references') or source.get('recovered_malformed_documents') else 'REVIEW', 'source_sha256': digest(Path(epub).read_bytes()),
            'result_sha256': digest(Path(fb2).read_bytes()), 'epub': source, 'fb2': target,
            'differences': differences,
            'limitations': (['Malformed XHTML documents were skipped; source coverage and all comparison metrics are incomplete.'] if source.get('skipped_malformed_documents') else []) + (['Malformed XHTML was recovered heuristically; content and structure may differ from the source.'] if source.get('recovered_malformed_documents') else []) + (['Unsafe archive references were skipped; source coverage and reference metrics may be incomplete.'] if source.get('skipped_unsafe_references') else []) + ['Counts and token coverage cannot prove order, structure, link destinations or visual fidelity.',
                            'Navigation, backlinks, non-spine notes and converted images may cause legitimate differences.',
                            'Compare this report with importer warnings and inspect the actual pair before classifying a defect.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--epub', required=True, type=Path)
    parser.add_argument('--fb2', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--skip-malformed-xhtml', action='store_true', help='Skip malformed XHTML and mark the comparison PARTIAL')
    parser.add_argument('--skip-unsafe-references', action='store_true', help='Skip archive-escaping references and mark the comparison PARTIAL')
    parser.add_argument('--recover-malformed-xhtml', action='store_true', help='Heuristically recover malformed XHTML using lxml and mark the comparison PARTIAL')
    args = parser.parse_args()
    if args.report.resolve() in (args.epub.resolve(), args.fb2.resolve()):
        parser.error('Report must not overwrite either input file')
    report = compare(args.epub, args.fb2, skip_malformed_xhtml=args.skip_malformed_xhtml, skip_unsafe_references=args.skip_unsafe_references, recover_malformed_xhtml=args.recover_malformed_xhtml)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(report['status'] + ': EPUB/FB2 inventory written; this tool never declares an import defect fixed.')


if __name__ == '__main__':
    main()
