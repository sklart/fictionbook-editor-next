from __future__ import annotations

import argparse
import csv
import html
from pathlib import Path


def paragraph(text: str) -> str:
    return f"<p>{html.escape(text, quote=False)}</p>"


def main() -> int:
    parser = argparse.ArgumentParser(description="Create an FB2 document for manual review in FBE Next")
    parser.add_argument("--disagreements", type=Path, required=True)
    parser.add_argument("--typo-benchmark", type=Path, required=True)
    parser.add_argument("--annotated-disagreements", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--limit", type=int, default=500)
    args = parser.parse_args()

    sections: list[tuple[str, list[str]]] = []

    disagreement_rows: list[dict[str, str]] = []
    with args.disagreements.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        disagreement_rows = list(reader)[: args.limit]
    disagreement_paragraphs = []
    for row in disagreement_rows:
        statuses = ", ".join(
            f"{key.removeprefix('accepted_')}={'да' if value == '1' else 'нет'}"
            for key, value in row.items()
            if key.startswith("accepted_")
        )
        disagreement_paragraphs.append(
            f"{row.get('word')} — частота {row.get('frequency')}; {statuses}. Контекст: {row.get('contexts', '')}"
        )
    sections.append(("Расхождения между словарями", disagreement_paragraphs))

    if args.annotated_disagreements and args.annotated_disagreements.is_file():
        annotated_rows: list[dict[str, str]] = []
        with args.annotated_disagreements.open("r", encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            annotated_rows = list(reader)[: args.limit]
        annotated_paragraphs = []
        for row in annotated_rows:
            statuses = ", ".join(
                f"{key.removeprefix('accepted_')}={'да' if value == '1' else 'нет'}"
                for key, value in row.items()
                if key.startswith("accepted_")
            )
            annotated_paragraphs.append(
                f"[{row.get('corpus')}] {row.get('word')} — частота {row.get('frequency')}; "
                f"классы: {row.get('class_counts', '')}; части речи: {row.get('pos_counts', '')}; "
                f"{statuses}. Контекст: {row.get('contexts', '')}"
            )
        sections.append(("Расхождения на размеченных корпусах НКРЯ", annotated_paragraphs))

    typo_rows: list[dict[str, str]] = []
    with args.typo_benchmark.open("r", encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            if row.get("wrong_rejected") == "False" or row.get("correct_accepted") == "False":
                typo_rows.append(row)
            if len(typo_rows) >= args.limit:
                break
    typo_paragraphs = []
    for row in typo_rows:
        typo_paragraphs.append(
            f"[{row.get('dictionary')}] {row.get('wrong')} → {row.get('correct')}; "
            f"ошибка обнаружена: {row.get('wrong_rejected')}; правильное слово принято: {row.get('correct_accepted')}. "
            f"Исходная фраза: {row.get('source', '')}"
        )
    sections.append(("Пропущенные ошибки и отклонённые исправления", typo_paragraphs))

    body_parts = []
    for title, paragraphs in sections:
        body_parts.append("<section>")
        body_parts.append(f"<title>{paragraph(title)}</title>")
        body_parts.extend(paragraph(item) for item in paragraphs)
        body_parts.append("</section>")

    content = f'''<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <description>
    <title-info>
      <genre>reference</genre>
      <author><first-name>FBE</first-name><last-name>Next</last-name></author>
      <book-title>Проверка нового русского словаря</book-title>
      <lang>ru</lang>
    </title-info>
    <document-info>
      <author><nickname>spellcheck-corpus</nickname></author>
      <program-used>make_fb2_review.py</program-used>
      <date value="2026-08-04">4 августа 2026</date>
      <id>fbe-next-spellcheck-corpus-review</id>
      <version>1.0</version>
    </document-info>
  </description>
  <body>
    <title>{paragraph('Проверка нового русского словаря')}</title>
    {''.join(body_parts)}
  </body>
</FictionBook>
'''
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(content, encoding="utf-8", newline="\n")
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
