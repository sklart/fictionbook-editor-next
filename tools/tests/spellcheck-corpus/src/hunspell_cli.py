from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


class HunspellError(RuntimeError):
    pass


def _dictionary_base(path: Path) -> str:
    if path.suffix.lower() in {".aff", ".dic"}:
        path = path.with_suffix("")
    aff = path.with_suffix(".aff")
    dic = path.with_suffix(".dic")
    if not aff.is_file() or not dic.is_file():
        raise FileNotFoundError(f"Dictionary pair not found: {aff} / {dic}")
    return str(path)


def list_misspelled(hunspell_exe: Path, dictionary: Path, words: list[str]) -> set[str]:
    base = _dictionary_base(dictionary)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", newline="\n", delete=False) as handle:
        for word in words:
            handle.write(word)
            handle.write("\n")
        input_path = Path(handle.name)
    try:
        command = [str(hunspell_exe), "-i", "UTF-8", "-d", base, "-l", str(input_path)]
        result = subprocess.run(command, capture_output=True, check=False)
        if result.returncode not in (0, 1):
            stderr = result.stderr.decode("utf-8", errors="replace")
            raise HunspellError(f"Hunspell failed ({result.returncode}): {stderr.strip()}")
        output = result.stdout.decode("utf-8", errors="replace")
        return {line.strip() for line in output.splitlines() if line.strip()}
    finally:
        input_path.unlink(missing_ok=True)


def suggestions(hunspell_exe: Path, dictionary: Path, words: list[str]) -> dict[str, list[str]]:
    """Get suggestions through Hunspell pipe mode (-a)."""
    if not words:
        return {}
    base = _dictionary_base(dictionary)
    payload = "\n".join(words) + "\n"
    command = [str(hunspell_exe), "-i", "UTF-8", "-d", base, "-a"]
    result = subprocess.run(command, input=payload.encode("utf-8"), capture_output=True, check=False)
    if result.returncode not in (0, 1):
        stderr = result.stderr.decode("utf-8", errors="replace")
        raise HunspellError(f"Hunspell suggestions failed ({result.returncode}): {stderr.strip()}")
    lines = result.stdout.decode("utf-8", errors="replace").splitlines()
    if lines and lines[0].startswith("@(#)"):
        lines = lines[1:]
    responses = [line for line in lines if line.strip()]
    parsed: dict[str, list[str]] = {}
    for word, line in zip(words, responses, strict=False):
        if line.startswith("&") and ":" in line:
            tail = line.split(":", 1)[1]
            parsed[word] = [item.strip() for item in tail.split(",") if item.strip()]
        else:
            parsed[word] = []
    for word in words:
        parsed.setdefault(word, [])
    return parsed
