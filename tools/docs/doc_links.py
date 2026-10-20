"""Small offline Markdown link/anchor checker. Python 3.10+, no dependencies.

Understands inline links/images, reference definitions, HTML href/src/id,
ATX and Setext headings, duplicate GitHub-style anchors, fenced/inline code.
It deliberately does not fetch remote URLs or claim full CommonMark parsing.
"""
from __future__ import annotations
from dataclasses import dataclass, asdict
from pathlib import Path, PurePosixPath
from collections import Counter
from urllib.parse import unquote, urlsplit, quote
import argparse
import html
import json
import posixpath
import re
import sys
import unicodedata
from typing import Callable, Iterable

@dataclass(frozen=True)
class Link:
    start: int
    end: int
    target: str
    full_start: int
    full_end: int
    label: str
    kind: str = 'markdown'

@dataclass(frozen=True)
class Finding:
    file: str
    line: int
    code: str
    target: str
    detail: str


def mask_code(text: str, inline: bool = True) -> str:
    # Preserve offsets/newlines so a diagnostic points at the original text.
    out = []
    fence_char = ''
    fence_len = 0
    for line in text.splitlines(keepends=True):
        m = re.match(r'^\s{0,3}(`{3,}|~{3,})', line)
        if fence_char:
            closing = re.match(r'^\s{0,3}(' + re.escape(fence_char) +
                               '{' + str(fence_len) + r',})\s*$', line.rstrip('\r\n'))
            out.append(''.join('\n' if c == '\n' else '\r' if c == '\r' else ' ' for c in line))
            if closing:
                fence_char = ''; fence_len = 0
        elif m:
            fence_char = m.group(1)[0]; fence_len = len(m.group(1))
            out.append(''.join('\n' if c == '\n' else '\r' if c == '\r' else ' ' for c in line))
        else:
            out.append(line)
    result = ''.join(out)
    result = re.sub(r'<!--[\s\S]*?-->', lambda m: re.sub(r'[^\r\n]', ' ', m[0]), result)
    if inline:
        # Match exact-length backtick runs. A failed opening delimiter remains
        # literal; it must not swallow later prose or links.
        chars = list(result)
        i = 0
        while i < len(result):
            if result[i] != '`' or (i > 0 and result[i-1] == '\\'):
                i += 1
                continue
            j = i
            while j < len(result) and result[j] == '`':
                j += 1
            run = j-i
            match = re.search(r'(?<!`)`{' + str(run) + r'}(?!`)', result[j:])
            if match is None:
                i = j
                continue
            end = j + match.end()
            for k in range(i, end):
                if chars[k] not in ('\r', '\n'):
                    chars[k] = ' '
            i = end
        result = ''.join(chars)
    return result


def balanced_end(text: str, start: int, left: str, right: str) -> int | None:
    depth = 1; i = start + 1
    while i < len(text):
        if text[i] == '\\' and i + 1 < len(text):
            i += 2; continue
        if text[i] == left:
            depth += 1
        elif text[i] == right:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return None


def destination(text: str, start: int, end: int) -> tuple[int, int]:
    while start < end and text[start].isspace(): start += 1
    while end > start and text[end-1].isspace(): end -= 1
    if start < end and text[start] == '<':
        close = text.find('>', start+1, end)
        if close >= 0: return start+1, close
    # Optional Markdown title. Do not split Windows paths merely at a space.
    m = re.search(r'\s+["\']', text[start:end])
    if m:
        end = start + m.start()
    return start, end


def links(text: str) -> list[Link]:
    masked = mask_code(text)
    out: list[Link] = []
    occupied: list[tuple[int,int]] = []
    # Reference definitions need only one destination check, regardless of uses.
    for m in re.finditer(r'^\s{0,3}\[([^\]\n]+)\]:\s*(.+)$', masked, re.M):
        a, b = destination(text, m.start(2), m.end(2))
        out.append(Link(a,b,text[a:b],m.start(),m.end(),m.group(1),'reference'))
        occupied.append((m.start(),m.end()))
    i = 0
    while i < len(masked):
        if masked[i] != '[' or (i and masked[i-1] == '\\') or any(a <= i < b for a,b in occupied):
            i += 1; continue
        j = balanced_end(masked,i,'[',']')
        if j is None: i += 1; continue
        k = j + 1
        if k >= len(masked) or masked[k] != '(':
            i = j + 1; continue
        end = balanced_end(masked,k,'(',')')
        if end is None: i = j + 1; continue
        a,b = destination(text,k+1,end)
        fs = i-1 if i and masked[i-1]=='!' else i
        out.append(Link(a,b,text[a:b],fs,end+1,text[i+1:j],
                        'image' if fs < i else 'markdown'))
        i = end + 1
    for tag in re.finditer(r'<(?:a|img|source)\b[^>]*>', masked, re.I):
        segment = text[tag.start():tag.end()]
        for m in re.finditer(r'\b(href|src)\s*=\s*(["\'])(.*?)\2', segment, re.I|re.S):
            a=tag.start()+m.start(3); b=tag.start()+m.end(3)
            out.append(Link(a,b,text[a:b],tag.start(),tag.end(),m.group(1),'html'))
    # A shared target can be seen only once; rewriting overlapping spans is unsafe.
    return sorted({(l.start,l.end):l for l in out}.values(),key=lambda l:l.start)


def slug(title: str) -> str:
    title = re.sub(r'!?\[([^\]]+)\]\([^)]*\)', r'\1', title)
    title = re.sub(r'<[^>]+>', '', title)
    title = html.unescape(title).strip().lower().replace('`','')
    title = title.replace('*','').replace('~','')
    return ''.join('-' if c.isspace() else c for c in title
                   if c.isspace() or c in '-_' or unicodedata.category(c)[0] in ('L','N','M'))


def anchors(text: str) -> set[str]:
    visible = mask_code(text,inline=False)
    found = set(re.findall(r'\b(?:id|name)\s*=\s*["\']([^"\']+)["\']',
                           '\n'.join(re.findall(r'<a\b[^>]*>', visible, re.I)),re.I))
    counter: Counter[str] = Counter()
    lines_ = visible.splitlines()
    for i,line in enumerate(lines_):
        m = re.match(r'^\s{0,3}#{1,6}\s+(.+?)\s*#*\s*$',line)
        heading = m.group(1) if m else None
        if heading is None and i+1<len(lines_) and line.strip() and re.match(r'^\s{0,3}(?:=+|-+)\s*$',lines_[i+1]):
            if not line.lstrip().startswith(('|','-','>')): heading=line.strip()
        if heading is not None:
            key=slug(heading); n=counter[key];counter[key]+=1
            found.add(key if n==0 else f'{key}-{n}')
    return found


def normalize_target(origin: str, target: str) -> tuple[str,str,str]:
    """Return (kind, repo-relative path, fragment)."""
    target=html.unescape(target.strip())
    if re.match(r'^[A-Za-z]:[\\/]',target) or target.startswith('\\\\') or target.lower().startswith('file:'):
        return 'absolute',target,''
    if target.startswith('//') or re.match(r'^[A-Za-z][A-Za-z0-9+.-]*:',target):
        return 'external',target,''
    # Markdown escapes, not Windows separators (those are handled above).
    target=re.sub(r'\\([()\[\] _])',r'\1',target)
    split=urlsplit(target)
    raw=unquote(split.path).replace('\\','/')
    fragment=unquote(split.fragment)
    path=origin if not raw else posixpath.normpath(posixpath.join(posixpath.dirname(origin),raw))
    if raw.startswith('/') or path == '..' or path.startswith('../'):
        return 'outside',path,fragment
    return 'local',path,fragment


def check_documents(documents: dict[str,str], exists: Callable[[str],bool],
                    load_text: Callable[[str],str|None]) -> dict:
    findings: list[Finding]=[]; checked=0; external=0
    cache={name:anchors(text) for name,text in documents.items()}
    for name,text in sorted(documents.items()):
        if re.search(r'\{\{[A-Z][A-Z0-9_]+\}\}',text):
            findings.append(Finding(name,1,'unexpanded-template','','Unexpanded assembly marker'))
        for link in links(text):
            kind,path,fragment=normalize_target(name,link.target)
            line=text.count('\n',0,link.start)+1
            if kind=='external': external+=1;continue
            checked+=1
            if kind in ('absolute','outside'):
                findings.append(Finding(name,line,kind,link.target,'Local target is not repository-relative'));continue
            if not exists(path):
                findings.append(Finding(name,line,'missing-file',link.target,path));continue
            if fragment and path.lower().endswith(('.md','.markdown','.html','.htm')):
                if path not in cache:
                    target_text=load_text(path)
                    if target_text is not None: cache[path]=anchors(target_text)
                if path in cache and fragment not in cache[path]:
                    findings.append(Finding(name,line,'missing-anchor',link.target,path+'#'+fragment))
    return {'checked_local_links':checked,'external_links_not_fetched':external,
            'documents':len(documents),'errors':[asdict(f) for f in findings],
            'status':'PASS' if not findings else 'FAIL'}


def local_check(root: Path, folder: str | None = None) -> dict:
    root = root.resolve()
    if folder is not None:
        docroot = (root / folder).resolve()
        if not docroot.is_relative_to(root) or not docroot.is_dir():
            raise ValueError(f'No repository documentation directory: {docroot}')
        paths = list(docroot.rglob('*.md'))
    else:
        # Owned documentation only: never scan generated out/build, external
        # attachments or pinned vendor/submodule trees.
        paths = list(root.glob('*.md'))
        for directory in ('docs', 'tools', 'src', 'localization', 'packaging', 'runtime', '.github'):
            paths.extend((root / directory).rglob('*.md'))
    documents = {p.relative_to(root).as_posix(): p.read_text(encoding='utf-8-sig')
                 for p in paths if p.is_file() and p.resolve().is_relative_to(root)}
    def exists(name: str) -> bool:
        p=(root/name).resolve()
        return p.is_relative_to(root) and p.exists()
    def load(name: str) -> str|None:
        p=(root/name).resolve()
        if p.is_relative_to(root) and p.is_file():
            try:return p.read_text(encoding='utf-8-sig')
            except UnicodeError:return None
        return None
    return check_documents(documents,exists,load)


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[2])
    parser.add_argument('--folder', help='Restrict to one repository directory; default: owned documentation')
    parser.add_argument('--json',type=Path)
    args=parser.parse_args()
    result=local_check(args.root,args.folder)
    if args.json:
        args.json.parent.mkdir(parents=True,exist_ok=True)
        args.json.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f"{result['status']}: {result['documents']} Markdown files; {result['checked_local_links']} local links")
    for f in result['errors']:print(f"{f['file']}:{f['line']}: {f['code']}: {f['target']} -> {f['detail']}")
    print(f"Remote URLs not fetched: {result['external_links_not_fetched']}")
    return 0 if result['status']=='PASS' else 1

if __name__=='__main__':
    try:raise SystemExit(main())
    except (OSError,ValueError) as exc:
        print(f'Documentation check failed: {exc}',file=sys.stderr);raise SystemExit(2)
