"""Inventory embedded script cards and record evidence for a content review.

The default mode is read-only.  Apply accepts an explicit, reviewed proposal
file; a heuristic finding alone never changes a card.
"""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
import hashlib
import html
import json
import os
import shutil
from pathlib import Path
import re
import runpy
import subprocess
import sys
from urllib.parse import quote


ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "runtime" / "Scripts"
CARD_TOOL = runpy.run_path(str(ROOT / "tools" / "script-card-tool.py"))
read_document = CARD_TOOL["read_document"]
validate_schema = CARD_TOOL["validate_schema"]
serialize = CARD_TOOL["serialize"]

GENERIC_USAGE = (
    "Откройте нужную книгу в FBE Next",
    "Проверьте перечисленные параметры",
    "Запустите команду и ответьте на её диалоги",
    "Просмотрите результат до сохранения",
    "Запустите команду из меню или дерева скриптов",
)
GENERIC_WARNING = (
    "Перед массовой обработкой сохраните отдельную копию",
    "Перед массовой обработкой сохраните отдельную копию книги",
)
EXTERNAL_WARNING = "Есть запуск внешнего приложения или открытие сетевого адреса"
GENERIC_UNDO = re.compile(
    r"полн(?:ый|ая|ую|ого|ой|ые).{0,30}(?:откат|отмен)|полнота отката.{0,30}не проверен|"
    r"(?:откат|отмена).{0,40}(?:не проверен|не подтвержд[её]н)|"
    r"не доказывает корректный откат|гарантия отмены не установлена|"
    r"явная группировка Undo|прямых Undo-вызовов нет", re.I)
INTERNAL_SETTINGS = {"ProcessingEnded", "rememberedPoem", "InsideP", "InsideSelection",
                     "help", "errMsg", "ScriptName", "NumerusVersion", "NazvanieSkripta",
                     "ShablonName", "report", "ss"}
API_PATTERNS = {
    "undo_begin": r"\bBeginUndoUnit\s*\(",
    "undo_end": r"\bEndUndoUnit\s*\(",
    "dialog": r"\b(?:prompt|InputBox|MessageBox|ShowDialog|showModalDialog)\s*\(",
    "selection": r"\b(?:createRange|createTextRange|selection|GetSelection)\b",
    "write": r"\b(?:insertAdjacentHTML|replaceChild|appendChild|removeChild|createElement|innerHTML|outerHTML|SetBinary|SetText|put_data)\b",
    "external": r"\b(?:ShellExecute(?:Ex)?|WScript\.Shell|XMLHTTP|WinHttpRequest|fetch|Navigate)\b|\.\s*(?:Run|Exec)\s*\(",
    "network": r"\b(?:XMLHTTP|WinHttpRequest|fetch|InternetOpen|URLDownloadToFile)\b",
    "process": r"\b(?:ShellExecute(?:Ex)?|WScript\.Shell)\b|\.\s*(?:Run|Exec)\s*\(",
    "local_window": r"\bwindow\s*\.\s*open\s*\(",
    "file": r"\b(?:FileSystemObject|ADODB\.Stream|SaveToFile|CreateTextFile|OpenTextFile|CopyFile|DeleteFile)\b",
    "return": r"\breturn\b",
}


@dataclass(frozen=True)
class Source:
    lines: list[str]
    executable: str
    masked: str
    line_offset: int


def mask_non_code(source: str) -> str:
    """Keep line offsets while hiding comments and string contents.

    This is a lexical aid, not a JavaScript parser. Escapes and old JScript
    regular expressions remain candidates for human review.
    """
    out: list[str] = []
    state = "code"
    quote = ""
    index = 0
    regex_class = False
    while index < len(source):
        ch = source[index]
        nxt = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if ch in "\"'`":
                state, quote = "string", ch
                out.append(" ")
            elif ch == "/" and nxt == "/":
                state = "line"
                out.extend("  ")
                index += 1
            elif ch == "/" and nxt == "*":
                state = "block"
                out.extend("  ")
                index += 1
            elif ch == "/" and (not "".join(out[-40:]).rstrip() or
                                  "".join(out[-40:]).rstrip()[-1] in "=(:,[!&|?;{}" or
                                  re.search(r"\b(?:return|case|throw)\s*$", "".join(out[-40:]))):
                state, regex_class = "regex", False
                out.append(" ")
            else:
                out.append(ch)
        elif state == "string":
            if ch == "\\" and nxt:
                out.extend("  ")
                index += 1
            elif ch == quote:
                state = "code"
                out.append(" ")
            else:
                out.append("\n" if ch == "\n" else " ")
        elif state == "regex":
            if ch == "\\" and nxt:
                out.extend("  ")
                index += 1
            else:
                if ch == "[": regex_class = True
                elif ch == "]": regex_class = False
                elif ch == "/" and not regex_class: state = "code"
                out.append("\n" if ch == "\n" else " ")
        elif state == "line":
            if ch == "\n":
                state = "code"
                out.append("\n")
            else:
                out.append(" ")
        else:
            if ch == "*" and nxt == "/":
                state = "code"
                out.extend("  ")
                index += 1
            else:
                out.append("\n" if ch == "\n" else " ")
        index += 1
    return "".join(out)


def source_for(document) -> Source:
    text = document.code_bytes[document.bom_size:].decode(document.encoding, errors="strict")
    text = re.sub(r"\r+\n|\r", "\n", text)
    offset = document.data[:document.span[1]].count(b"\n") if document.span else 0
    return Source(text.splitlines(), text, mask_non_code(text), offset)


def evidence(source: Source, pattern: str, *, masked: bool = True, limit: int = 4) -> list[dict]:
    haystack = source.masked if masked else source.executable
    matches = []
    for match in re.finditer(pattern, haystack, re.I | re.M):
        source_line = haystack.count("\n", 0, match.start()) + 1
        matches.append({"line": source_line + source.line_offset,
                        "text": source.lines[source_line - 1].strip()[:180]})
        if len(matches) >= limit:
            break
    return matches


def finding(category: str, field: str, previous, proposed, reason: str,
            confidence: str, proof: list[dict] | None = None) -> dict:
    return {"category": category, "field": field, "previous": previous,
            "proposed": proposed, "reason": reason, "confidence": confidence,
            "evidence": proof or []}


def linked_resource_candidates(source: Source) -> list[dict]:
    """Literal input resources, excluding generated report filenames.

    A call in a comment is ignored. Dynamic paths and transitive loads still
    require review; this deliberately does not claim dependency completeness.
    """
    result = []
    # Conservative direct-call graph for named legacy functions. Escaping
    # references, dynamic code and duplicate bindings remain unknown. This is
    # evidence about reachability, never automatic proof of a required input.
    braces, stack = {}, []
    for position, character in enumerate(source.masked):
        if character == "{": stack.append(position)
        elif character == "}" and stack: braces[stack.pop()] = position
    functions = []
    for declaration in re.finditer(r"\bfunction\s+([\w$]+)\s*\(", source.masked):
        opening = source.masked.find("{", declaration.end())
        if opening in braces:
            functions.append({"name": declaration[1], "nameStart": declaration.start(1),
                              "begin": opening, "end": braces[opening]})
    def owner(position):
        enclosing = [f for f in functions if f["begin"] < position < f["end"]]
        return max(enclosing, key=lambda f: f["begin"])["name"] if enclosing else "<top>"
    names = {f["name"] for f in functions}
    graph = {name: set() for name in names | {"<top>"}}
    uncertain = bool(stack) or len(names) != len(functions) or bool(re.search(
        r"\b(?:eval|Function|setTimeout|setInterval)\s*\(|\bfunction\s*\(", source.masked))
    for function in functions:
        name = function["name"]
        for reference in re.finditer(r"\b" + re.escape(name) + r"\b", source.masked):
            if reference.start() == function["nameStart"]: continue
            tail = source.masked[reference.end():]
            prefix = source.masked[:reference.start()].rstrip()
            if re.match(r"\s*\(", tail) and not prefix.endswith("."):
                graph[owner(reference.start())].add(name)
            else:
                uncertain = True
    reachable = {"<top>"} | ({"Run"} if "Run" in names else set())
    pending = list(reachable)
    while pending:
        for called in graph[pending.pop()] - reachable:
            reachable.add(called)
            pending.append(called)
    pattern = r'''\b(?:window\s*\.\s*open|showModalDialog|showModelessDialog|ShowDialog)\s*\(\s*(["'])(HTML/[^"']+\.(?:html?|js))\1'''
    for match in re.finditer(pattern, source.executable, re.I):
        if not source.masked[match.start():match.start()+6].strip():
            continue
        line = source.executable.count("\n", 0, match.start()) + 1
        enclosing = owner(match.start())
        reachability = "unknown" if uncertain else "reachable" if enclosing in reachable else "not_referenced"
        # A top-level exported helper can also be called by linked HTML/JS.
        # Only a local unreferenced function can be ruled out in this file.
        declaration = next((f for f in functions if f["name"] == enclosing), None)
        if reachability == "not_referenced" and declaration and owner(declaration["nameStart"]) == "<top>":
            reachability = "unknown"
        result.append({"path": match[2], "line": line + source.line_offset,
                       "text": source.lines[line-1].strip()[:180],
                       "ownerFunction": enclosing, "reachability": reachability,
                       "required": None})
    return result


def inspect(path: Path) -> dict:
    relative = path.relative_to(SCRIPTS).as_posix()
    document = read_document(path.read_bytes())
    if document.card is None:
        return {"path": relative, "result": "invalid", "findings": [
            finding("requires_review", "card", None, None, "Карточка отсутствует", "high")]}
    card = document.card
    source = source_for(document)
    found: list[dict] = []
    schema_errors = validate_schema(card)
    hash_current = card.get("sourceRevision", {}).get("sha256") == document.sha256
    for error in schema_errors:
        found.append(finding("requires_review", "schema", error, None, "Ошибка схемы", "high"))
    digest = card.get("sourceRevision", {}).get("sha256")
    if digest != document.sha256:
        found.append(finding("requires_review", "sourceRevision.sha256", digest, document.sha256,
                             "Код отличается от описанной редакции", "high"))
    facts = {key: evidence(source, pattern, limit=1000 if key == "return" else 4)
             for key, pattern in API_PATTERNS.items()}
    if re.search(r"window\.external\.inflateBlock\s*\([^;\n]+\)\s*=", source.executable):
        for field in ("description", "warnings", "usage"):
            values = card.get(field, [])
            if isinstance(values, str): values = [values]
            for value in values:
                if re.search(r"результату вызова|ошибочн[^.]*присваив", value, re.I):
                    found.append(finding("incorrect_jscript_claim", field, value, None,
                                         "inflateBlock — индексируемое COM-свойство с propput; отказ обычного JS-парсера не доказывает ошибку JScript",
                                         "medium"))
    # String contents are kept only for this diagnostic. A namespace URL alone
    # must never be interpreted as an executed network operation.
    namespace_urls = evidence(source, r"https?://[^\s'\"]+", masked=False)
    if card.get("summary", "").strip(" .").lower() == card.get("title", "").strip(" .").lower():
        found.append(finding("inaccurate_description", "summary", card["summary"], None,
                             "Краткое описание повторяет название без объяснения действия", "medium"))
    for item in card.get("usage", []):
        if any(item.startswith(prefix) for prefix in GENERIC_USAGE):
            found.append(finding("generic_usage", "usage", item, None,
                                 "Типовая инструкция требует проверки применимости к этому скрипту", "high"))
            if item.startswith("Запустите команду и ответьте") and not facts["dialog"]:
                found.append(finding("generic_usage", "usage", item, None,
                                     "В исполняемом JS не найден прямой вызов диалога; возможна зависимость или косвенный вызов",
                                     "medium"))
    for item in card.get("warnings", []):
        if any(part in item for part in GENERIC_WARNING):
            found.append(finding("requires_review", "warnings", item, None,
                                 "Общее предупреждение о массовой обработке требует проверки области действия", "medium"))
        if EXTERNAL_WARNING in item:
            found.append(finding("requires_review", "warnings", item, None,
                                 "Проверить достижимые внешние вызовы и связанные файлы; URL литерал не является сетевой операцией",
                                 "medium", facts["external"] or namespace_urls))
    for field in ("description", "usage", "warnings"):
        values = card.get(field, [])
        if isinstance(values, str): values = [values]
        for value in values:
            if GENERIC_UNDO.search(value):
                found.append(finding("generic_undo", field, value, None,
                                     "Общее предупреждение не называет условие или конкретный дефект", "medium"))
    participants = Counter(str(author.get("name", "")).strip().casefold()
                           for author in card.get("authors", []) if author.get("name"))
    for name, count in participants.items():
        if count > 1:
            found.append(finding("duplicate_author", "authors", name, None,
                                 "Имя повторяется; при показе объединить роли без потери атрибуции", "high"))
    undo = card.get("undo") or {}
    description = undo.get("description", "")
    if re.search(r"BeginUndoUnit|EndUndoUnit", description, re.I) or GENERIC_UNDO.search(description):
        found.append(finding("incorrect_undo", "undo.description", description, None,
                             "Техническое имя API или необоснованное обещание Ctrl+Z в пользовательском тексте", "high",
                             facts["undo_begin"] + facts["undo_end"]))
    if card.get("requiresSelection") is True and re.search(
            r"(?:выделени[ея] не требу|без выделения)", " ".join(card.get("usage", [])), re.I):
        found.append(finding("selection_conflict", "usage", card.get("usage"), None,
                             "Указано обязательное выделение, но инструкция допускает его отсутствие", "medium"))
    if card.get("requiresSelection") is False and not re.search(
            r"или слово|под курсором", card.get("scope", ""), re.I) and re.search(
            r"только (?:в |по )?выделенн|необходимо выделить", card.get("scope", ""), re.I):
        found.append(finding("selection_conflict", "scope", card.get("scope"), None,
                             "Область ограничена выделением, которое объявлено необязательным", "medium"))
    if facts["undo_begin"] and facts["undo_end"]:
        begins = list(re.finditer(API_PATTERNS["undo_begin"], source.masked, re.I | re.M))
        ends = list(re.finditer(API_PATTERNS["undo_end"], source.masked, re.I | re.M))
        returns = []
        if begins and ends:
            for match in re.finditer(API_PATTERNS["return"], source.masked, re.I | re.M):
                if begins[0].start() < match.start() < ends[-1].start():
                    line = source.masked.count("\n", 0, match.start()) + 1
                    returns.append({"line": line + source.line_offset,
                                    "text": source.lines[line - 1].strip()[:180]})
                if len(returns) >= 4:
                    break
        if returns:
            found.append(finding("requires_review", "undo", undo, None,
                                 "Есть return между первым открытием и последним закрытием группы; проверить ветви и вложенность",
                                 "medium", facts["undo_begin"][:1] + returns + facts["undo_end"][-1:]))
    settings = card.get("settings") or []
    setting_evidence = []
    linked_text = []
    for dep in card.get("dependencies", []):
        target = (ROOT / "runtime" / str(dep.get("path", ""))).resolve()
        if target.is_relative_to((ROOT / "runtime").resolve()) and target.is_file() and target.suffix.lower() in (".js", ".htm", ".html"):
            linked = read_document(target.read_bytes())
            linked_text.append(linked.code_bytes[linked.bom_size:].decode(linked.encoding))
    for index, item in enumerate(settings):
        if not isinstance(item, dict):
            continue
        name = item.get("name", "")
        if not isinstance(name, str) or not re.fullmatch(r"[\w$]+", name):
            continue
        declarations = evidence(source, rf"\b(?:var|let|const)\s+{re.escape(name)}\s*=", limit=3)
        uses = evidence(source, rf"\b{re.escape(name)}\b", limit=8)
        linked_uses = sum(len(re.findall(rf"\b{re.escape(name)}\b", text)) for text in linked_text)
        setting_evidence.append({"index": index, "name": name, "declarations": declarations,
                                 "uses": uses, "linkedOccurrences": linked_uses,
                                 "classification": "requires_review"})
        if not isinstance(item.get("title"), str) or not re.search(r"[А-Яа-яЁё]", item["title"]):
            found.append(finding("setting_title", f"settings[{index}].title", item.get("title"), None,
                                 "Нужно явное человекочитаемое русское название", "high"))
        # A single lexical occurrence is only a candidate: indirect access,
        # HTML event handlers and legacy JScript constructs need source review.
        if len(uses) <= max(1, len(declarations)) and not linked_uses:
            found.append(finding("unused_setting", f"settings[{index}]", item, None,
                                 "Не найдено чтение после объявления; проверить косвенный доступ и зависимости",
                                 "medium", uses))
        if name in INTERNAL_SETTINGS:
            found.append(finding("internal_setting", f"settings[{index}]", item, None,
                                 "Служебное состояние или текст сообщения вместо пользовательского параметра",
                                 "medium", uses))
        descriptions = str(item.get("description", ""))
        if (re.search(r"^(?:Inside|cur|tmp|temp|cnt|count|idx|index|i$|j$)", name, re.I)
                or re.search(r"если находимся|сч[её]тчик|внутренн|промежуточн", descriptions, re.I)):
            found.append(finding("requires_review", f"settings[{index}]", item, None,
                                 "Похоже на служебный флаг, счётчик или промежуточную переменную; проверить использование",
                                 "medium", declarations))
        elif not declarations and not linked_uses:
            found.append(finding("requires_review", f"settings[{index}]", item, None,
                                 "Объявление параметра в исполняемом коде не найдено; проверить связанный файл или динамическую настройку",
                                 "low"))
    resource_candidates = linked_resource_candidates(source)
    declared_paths = {dep.get("path") for dep in card.get("dependencies", [])}
    for candidate in resource_candidates:
        if candidate["reachability"] == "not_referenced":
            if any(dep.get("path") == candidate["path"] and not dep.get("optional")
                   for dep in card.get("dependencies", [])):
                found.append(finding("unreachable_dependency", "dependencies", candidate["path"], None,
                                     "Обязательный путь найден лишь в невызванной функции; проверить и исправить карточку",
                                     "medium", [candidate]))
        elif candidate["path"] not in declared_paths:
            found.append(finding("unlisted_dependency", "dependencies", candidate["path"], None,
                                 "Вызов открывает ресурс, не перечисленный в карточке; проверить назначение",
                                 "medium", [candidate]))
    dependency_evidence = []
    for dependency in card.get("dependencies", []):
        relative_dependency = dependency.get("path", "")
        target = (ROOT / "runtime" / Path(relative_dependency)).resolve() if isinstance(relative_dependency, str) else ROOT
        valid_path = target.is_relative_to((ROOT / "runtime").resolve())
        exists = valid_path and target.is_file()
        actual_digest = (read_document(target.read_bytes()).sha256 if target.suffix.lower() == ".js"
                         else hashlib.sha256(target.read_bytes()).hexdigest()) if exists else None
        dependency_evidence.append({"path": relative_dependency, "exists": exists,
                                    "declaredSha256": dependency.get("sha256"),
                                    "actualSha256": actual_digest,
                                    "digestMatches": actual_digest == dependency.get("sha256") if actual_digest else None,
                                    "contentReviewed": False})
        if not exists and not dependency.get("optional"):
            found.append(finding("incorrect_dependency", "dependencies", dependency, None,
                                 "Обязательный файл зависимости отсутствует", "high"))
        elif exists and dependency.get("sha256") and actual_digest != dependency["sha256"]:
            found.append(finding("incorrect_dependency", "dependencies", dependency, actual_digest,
                                 "Хеш связанного ресурса отличается от карточки", "high"))
    # A static scan is not a proof of correctness for any unflagged field.
    unresolved_fields = ["title", "summary", "description", "authors", "version", "scope",
                         "modifiesDocument", "requiresSelection", "usage", "warnings",
                         "settings", "dependencies", "undo", "examples", "tags"]
    return {"path": relative, "result": "requires_review", "codeSha256": document.sha256,
            "structurallyValid": not schema_errors, "sourceHashCurrent": hash_current,
            "contentReview": "requires_review",
            "encoding": document.encoding, "bomBytes": document.bom_size,
            "codeBytes": len(document.code_bytes), "lineEnding": "CRLF" if b"\r\n" in document.code_bytes else "LF",
            "claims": {key: card.get(key) for key in unresolved_fields +
                       ["authorshipStatus", "dependenciesComplete", "sourceRevision"]},
            "evidence": facts, "namespaceUrls": namespace_urls,
            "settingEvidence": setting_evidence, "dependencyEvidence": dependency_evidence,
            "resourceCandidates": resource_candidates,
            "authorshipEvidence": evidence(source, r"(?:автор|написал|author|version|верси)", masked=False),
            "warningEvidence": [{"text": warning, "verification": "requires_review"}
                                for warning in card.get("warnings", [])],
            "unverifiedFields": unresolved_fields, "findings": found}


def load_proposals(path: Path) -> list[dict]:
    proposals = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(proposals, list):
        raise ValueError("proposal file must contain an array")
    return proposals


def apply_proposals(proposals: list[dict], records: list[dict]) -> int:
    by_path = {record["path"]: record for record in records}
    pending: list[tuple[Path, bytes, bytes]] = []
    touched: set[str] = set()
    for proposal in proposals:
        relative = proposal["path"]
        if relative in touched or relative not in by_path or by_path[relative]["result"] == "invalid":
            raise ValueError(f"duplicate or unknown proposal path: {relative}")
        if not isinstance(proposal.get("evidence"), str) or not proposal["evidence"].strip():
            raise ValueError(f"reviewed evidence required: {relative}")
        touched.add(relative)
        path = SCRIPTS / Path(relative)
        original = path.read_bytes()
        document = read_document(original)
        if document.sha256 != proposal["codeSha256"]:
            raise ValueError(f"code SHA changed: {relative}")
        card = json.loads(json.dumps(document.card, ensure_ascii=False))
        changed = False
        for change in proposal["changes"]:
            field = change["field"]
            if field not in card and change["before"] is not None:
                raise ValueError(f"missing original {relative}: {field}")
            if field in card and card[field] == change["after"]:
                continue
            if card.get(field) != change["before"]:
                raise ValueError(f"stale original {relative}: {field}")
            card[field] = change["after"]
            changed = True
        if not changed:
            continue
        if validate_schema(card):
            raise ValueError(f"invalid proposed card: {relative}")
        span_start, span_end = document.span
        newline = "\r\n" if b"\r\n" in document.data[span_start:span_end] else "\n"
        updated = original[:span_start] + serialize(card, document.encoding, newline) + original[span_end:]
        if read_document(updated).code_bytes != document.code_bytes:
            raise ValueError(f"code bytes changed: {relative}")
        if read_document(updated).sha256 != document.sha256:
            raise ValueError(f"source hash changed: {relative}")
        pending.append((path, original, updated))
    written: list[tuple[Path, bytes]] = []
    try:
        for path, original, updated in pending:
            path.write_bytes(updated)
            written.append((path, original))
    except OSError:
        for path, original in reversed(written):
            path.write_bytes(original)
        raise
    return len(pending)


def reviewed_revision(proposal: dict, document, executable_changes: list[dict]) -> dict:
    """Validate a review against its actual revision, preserving its history.

    A changed executable is accepted only through an unambiguous recorded
    chain for this path. The historical review remains historical; it is not
    evidence that the new executable was reviewed or runtime-tested.
    """
    revision = proposal["codeSha256"]
    current = document.sha256
    chain = []
    seen = set()
    while revision != current:
        if revision in seen:
            raise ValueError(f"cyclic executable revision chain: {proposal['path']}")
        seen.add(revision)
        candidates = [change for change in executable_changes
                      if change.get("path") == proposal["path"] and
                      change.get("previousCodeSha256") == revision]
        if len(candidates) != 1 or not candidates[0].get("basisCommit"):
            raise ValueError(f"unknown or ambiguous reviewed revision: {proposal['path']}")
        chain.append(candidates[0])
        revision = candidates[0]["codeSha256"]

    for change in proposal["changes"]:
        actual = document.card.get(change["field"])
        expected = change["after"]
        if chain and change["field"] == "sourceRevision":
            # Only the digest may advance. Preserve algorithm and review status.
            if not isinstance(expected, dict) or not isinstance(actual, dict):
                raise ValueError(f"invalid source revision: {proposal['path']}")
            if expected.get("sha256") != proposal["codeSha256"]:
                raise ValueError(f"historical source revision differs from review: {proposal['path']}")
            expected = dict(expected, sha256=current)
        if actual != expected:
            raise ValueError(f"reviewed proposal differs from current card: {proposal['path']}")

    return dict(proposal, revisionStatus="historical" if chain else "current",
                currentCodeSha256=current,
                revisionCommits=[change["basisCommit"] for change in chain])


def write_reports(records: list[dict], output: Path, baseline: str,
                  reviewed: list[dict]) -> None:
    def write_utf8(path: Path, content: str) -> None:
        path.write_bytes(content.encode("utf-8"))

    output.mkdir(parents=True, exist_ok=True)
    counts = Counter(issue["category"] for record in records for issue in record["findings"])
    reviewed_counts = Counter(change.get("category", "other") for proposal in reviewed
                              for change in proposal["changes"])
    metrics = Counter()
    for proposal in reviewed:
        metrics.update(proposal.get("metrics", {}))
    data = {"baseCommit": baseline, "scriptCount": len(records), "counts": dict(counts),
            "reviewedChanges": reviewed, "reviewedCounts": dict(reviewed_counts),
            "reviewedMetrics": dict(metrics), "scripts": records}
    write_utf8(output / "script-card-content-audit.json",
               json.dumps(data, ensure_ascii=False, indent=2) + "\n")
    rows = []
    def source_link(path: str) -> str:
        target = quote(os.path.relpath(SCRIPTS / path, output).replace(os.sep, "/"), safe="/")
        return "<a href='" + html.escape(target, quote=True) + "'>" + html.escape(path) + "</a>"
    for record in records:
        for issue in record["findings"]:
            proof = "; ".join(f"L{e['line']}: {e['text']}" for e in issue["evidence"])
            rows.append("<tr><td>" + source_link(record["path"]) + "</td><td>" + html.escape(issue["category"])
                        + "</td><td>" + html.escape(issue["field"]) + "</td><td>"
                        + html.escape(str(issue["previous"])) + "</td><td>"
                        + html.escape(str(issue["proposed"])) + "</td><td>"
                        + html.escape(issue["reason"] + " " + proof) + "</td></tr>")
    reviewed_rows = []
    for proposal in reviewed:
        for change in proposal["changes"]:
            reviewed_rows.append("<tr><td>" + source_link(proposal["path"]) + "<br>"
                                 + html.escape(proposal["revisionStatus"] + ": " + proposal["codeSha256"]) + "</td><td>"
                                 + html.escape(change["field"]) + "</td><td>"
                                 + html.escape(str(change["before"])) + "</td><td>"
                                 + html.escape(str(change["after"])) + "</td><td>"
                                 + html.escape(proposal["evidence"]) + "</td></tr>")
    page = "<!doctype html><meta charset='utf-8'><title>Аудит карточек скриптов</title>" \
           "<style>body{font:14px system-ui;margin:2rem}table{border-collapse:collapse}" \
           "td,th{border:1px solid #aaa;padding:.4rem;vertical-align:top;max-width:30rem}" \
           "tr:nth-child(even){background:#eee}</style><h1>Аудит карточек скриптов</h1>" \
           f"<p>Исходный коммит: {html.escape(baseline)}. Скриптов: {len(records)}. " \
           "Автоматические замечания требуют подтверждения человеком.</p>" \
           "<h2>Проверенные исправления</h2><table><tr><th>Файл</th><th>Поле</th><th>До</th><th>После</th>" \
           "<th>Основание</th></tr>" + "".join(reviewed_rows) + "</table>" \
           "<h2>Кандидаты для проверки</h2><table><tr><th>Файл</th><th>Категория</th><th>Поле</th><th>Прежнее</th>" \
           "<th>Предложение</th><th>Основание</th></tr>" + "".join(rows) + "</table>"
    write_utf8(output / "script-card-content-audit.html", page)
    summary = ["# Аудит карточек скриптов", "", f"Исходный коммит: `{baseline}`.",
               f"Инвентаризовано файлов и карточек: **{len(records)}**.",
               f"Карточек без автоматических замечаний: **{sum(not record['findings'] for record in records)}**.",
               f"Карточек с незавершённой содержательной проверкой: **{sum(record['result'] == 'requires_review' for record in records)}**.", "",
               "## Состояние", "", "Выполнена автоматическая инвентаризация и поиск подозрительных утверждений.",
               "Содержательная проверка всех утверждений по алгоритму и зависимостям ещё не завершена;",
               "ни одна карточка не помечена достоверной только из-за отсутствия срабатываний эвристик.", "",
               "Подробности по каждому файлу и строкам исходника: [JSON](script-card-content-audit.json),",
               "[HTML](script-card-content-audit.html).", "", "## Проверенные исправления", "",
               f"Исторических записей ревью: **{len(reviewed)}**; из них относятся к прежней редакции кода: **{sum(item['revisionStatus'] == 'historical' for item in reviewed)}**.",
               "Ревью прежней редакции не подтверждает текущий исполняемый код.", ""]
    summary.extend(f"- `{key}`: {value}" for key, value in sorted(reviewed_counts.items()))
    summary += ["", "Подтверждённые результаты:", ""]
    summary.extend(f"- `{key}`: {value}" for key, value in sorted(metrics.items()))
    code_issues = [(proposal, issue) for proposal in reviewed
                   for issue in proposal.get("codeIssues", [])]
    summary += ["", f"Скриптов с записями анализа исполняемого кода (включая вопросы для проверки): **{len(set(proposal['path'] for proposal, _ in code_issues))}**.", ""]
    summary.extend(f"- `{proposal['path']}:{issue['line']}` ({proposal['revisionStatus']}, `{proposal['codeSha256']}`) — {issue['issue']}" for proposal, issue in code_issues)
    summary += ["", "## Замечания автоматического прохода", ""]
    summary.extend(f"- `{key}`: {value}" for key, value in sorted(counts.items()))
    summary += ["", "## Границы проверки", "", "Поиск вызовов учитывает комментарии и строки отдельно,",
                "но не доказывает достижимость функций, полноту внешних HTML/JS и все ветви Undo.",
                "Поля без находок остаются в очереди содержательной проверки.", ""]
    write_utf8(output / "script-card-content-audit.md", "\n".join(summary))
    concrete_undo = [(record["path"], record["claims"].get("undo", {}).get("description", ""))
                     for record in records if record["claims"].get("modifiesDocument") is not False]
    concrete_undo = [(path, text) for path, text in concrete_undo
                     if text and text != "Изменения внешних файлов не отменяются через редактор."]
    defect_lines = ["# Условия нарушения группировки отмены", "",
                    "Список основан на анализе исходников и связанных HTML/JS, а не на запуске всех аварийных ветвей.",
                    "Отсутствие finally само по себе сюда не включается. Эвристические return-кандидаты остаются в JSON-аудите.",
                    "Технические подробности и исходные ссылки на строки сохранены в script-card-reviewed-proposals.json.", "",
                    f"Скриптов с конкретными условиями: {len(concrete_undo)}.", ""]
    defect_lines.extend(f"- `{path}` — {text}" for path, text in concrete_undo)
    write_utf8(output / "script-card-undo-defects.md", "\n".join(defect_lines) + "\n")



def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "out" / "reports" / "script-cards")
    parser.add_argument("--apply", type=Path, metavar="REVIEWED_PROPOSALS_JSON")
    parser.add_argument("--reviewed-proposals", type=Path,
                        default=ROOT / "docs" / "script-card-reviewed-proposals.json")
    parser.add_argument("--baseline", type=Path,
                        default=ROOT / "docs" / "script-card-code-baseline.json")
    args = parser.parse_args()
    git = shutil.which("git")
    if git is None:
        raise ValueError("Git was not found in PATH")
    output = args.output_dir.resolve()
    docs_root = (ROOT / "docs").resolve()
    if output == docs_root or docs_root in output.parents:
        raise ValueError("Generated audit output must not be written inside docs")
    args.output_dir = output
    baseline = subprocess.check_output([git, "rev-parse", "HEAD"],
                                       cwd=ROOT, text=True).strip()
    records = [inspect(path) for path in sorted(SCRIPTS.rglob("*.js"))]
    baseline_data = {}
    if args.baseline.is_file():
        baseline_data = json.loads(args.baseline.read_text(encoding="utf-8"))
        baseline = baseline_data["baseCommit"]
        fields = ("path", "codeSha256", "encoding", "bomBytes", "codeBytes", "lineEnding")
        expected = [{field: item[field] for field in fields} for item in baseline_data["scripts"]]
        current = [{field: item[field] for field in fields} for item in records]
        if current != expected:
            raise ValueError("current executable code, encoding or line endings differ from baseline")
    if args.apply:
        changed = apply_proposals(load_proposals(args.apply), records)
        records = [inspect(path) for path in sorted(SCRIPTS.rglob("*.js"))]
        print(f"Applied {changed} reviewed card changes; code bytes checked.")
    proposals = load_proposals(args.reviewed_proposals) if args.reviewed_proposals.is_file() else []
    reviewed = []
    for proposal in proposals:
        path = SCRIPTS / Path(proposal["path"])
        document = read_document(path.read_bytes())
        reviewed.append(reviewed_revision(proposal, document, baseline_data.get("executableChanges", [])))
    write_reports(records, args.output_dir, baseline, reviewed)
    print(f"Audited {len(records)} files; detailed report: {args.output_dir / 'script-card-content-audit.json'}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as error:
        print(f"script card audit failed: {error}", file=sys.stderr)
        sys.exit(1)
