#!/usr/bin/env python3
"""Check local links in OpenFallout's developer documentation entrypoints."""

from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
MARKDOWN_ENTRYPOINTS = (
    Path("CONTRIBUTING.md"),
    Path("docs/openfallout/architecture.md"),
)
RST_ENTRYPOINTS = (
    Path("docs/source/openfallout.rst"),
    Path("docs/source/index.rst"),
)
REPO_BLOB_PREFIX = "https://github.com/Elemperor1/OpenFallout/blob/master/"
MARKDOWN_LINK = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
REPO_BLOB_LINK = re.compile(r"<((?:https://github\.com/Elemperor1/OpenFallout/blob/)[^>]+)>_?")
DOC_ROLE = re.compile(r":doc:`(?:[^`<>]*<)?([^`<>]+)>?`")
TOCTREE = re.compile(r"^(?P<indent>\s*)\.\. toctree::\s*$")


def local_target(source: Path, target: str, root: Path) -> Path | None:
    """Resolve a relative link target, ignoring URLs and in-page anchors."""
    target = target.strip()
    if not target or target.startswith("#"):
        return None
    parsed = urlsplit(target)
    if parsed.scheme or parsed.netloc:
        return None
    path = unquote(parsed.path)
    if not path:
        return None
    return (root / source.parent / path).resolve()


def check_markdown_links(text: str, source: Path, root: Path) -> list[str]:
    errors = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        for match in MARKDOWN_LINK.finditer(line):
            target = match.group(1)
            resolved = local_target(source, target, root)
            if resolved is not None and not resolved.exists():
                errors.append(f"{source}:{line_number}: missing link target {target}")
    return errors


def check_repo_blob_links(text: str, source: Path, root: Path) -> list[str]:
    errors = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        for match in REPO_BLOB_LINK.finditer(line):
            url = match.group(1)
            if not url.startswith(REPO_BLOB_PREFIX):
                errors.append(f"{source}:{line_number}: non-canonical repository link {url}")
                continue
            target = unquote(url[len(REPO_BLOB_PREFIX):].split("#", 1)[0])
            if not target or not (root / target).exists():
                errors.append(f"{source}:{line_number}: missing repository link target {target}")
    return errors


def check_sphinx_targets(text: str, source: Path, root: Path) -> list[str]:
    errors = []
    source_file = root / source

    def check_doc(target: str, line_number: int) -> None:
        target = target.strip().split("#", 1)[0]
        target_path = Path(target)
        if target_path.suffix != ".rst":
            target_path = target_path.with_suffix(".rst")
        resolved = (source_file.parent / target_path).resolve()
        if not resolved.is_file():
            errors.append(f"{source}:{line_number}: missing Sphinx document {target}")

    lines = text.splitlines()
    for line_number, line in enumerate(lines, start=1):
        for match in DOC_ROLE.finditer(line):
            check_doc(match.group(1), line_number)

        directive = TOCTREE.match(line)
        if not directive:
            continue
        base_indent = len(directive.group("indent"))
        for offset, candidate in enumerate(lines[line_number:], start=line_number + 1):
            if candidate.strip() and len(candidate) - len(candidate.lstrip()) <= base_indent:
                break
            entry = candidate.strip()
            if not entry or entry.startswith(":"):
                continue
            check_doc(entry, offset)
    return errors


def validate(root: Path = ROOT) -> list[str]:
    errors = []
    for source in MARKDOWN_ENTRYPOINTS:
        path = root / source
        if not path.is_file():
            errors.append(f"{source}: missing documentation entrypoint")
            continue
        errors.extend(check_markdown_links(path.read_text(encoding="utf-8"), source, root))

    for source in RST_ENTRYPOINTS:
        path = root / source
        if not path.is_file():
            errors.append(f"{source}: missing documentation entrypoint")
            continue
        text = path.read_text(encoding="utf-8")
        errors.extend(check_repo_blob_links(text, source, root))
        errors.extend(check_sphinx_targets(text, source, root))
    return errors


def main() -> int:
    errors = validate()
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print("Documentation entrypoint links are valid.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
