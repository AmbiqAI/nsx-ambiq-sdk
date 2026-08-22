#!/usr/bin/env python3
"""Scan and scrub vendor `#### INTERNAL ####` engineering content.

AmbiqSuite ships engineering-internal notes fenced between

    // #### INTERNAL BEGIN ####
    ...
    // #### INTERNAL END ####

markers. Upstream's own release tooling strips those fences (and the dead
content between them) before publishing; a raw engineering drop does not. When
such a drop is regenerated into `modules/<module>/sdk/`, the internal notes --
commented-out enums, `#if 0` experiments, ticket chatter -- land in the
committed provider tree.

This module is the intake-time sanitizer for that class of leak:

  scan             report every marker as `path:line`, ratcheted against
                   `sdk-intake/internal-marker-baseline.yaml`. Exit 1 when a
                   file carries more markers than its recorded baseline (or
                   carries markers with no baseline entry at all).
  scrub            conservatively rewrite a tree: drop whole INTERNAL blocks
                   that contain no live code, and drop `#if 0`/`#endif`
                   regions inside INTERNAL blocks.
  update-baseline  regenerate the ratchet baseline from a tree.

Scrub safety contract (why this never breaks a prebuilt `libam_hal.a`):
a shipped drop's headers were compiled into the archives that ship alongside
them, so removing a *live* declaration -- even one fenced inside an INTERNAL
block -- silently desynchronizes headers from the prebuilt binary. The scrub
therefore only ever deletes

  1. the marker lines of a block it removes entirely (always pure comments),
  2. lines inside an INTERNAL block whose body has no live tokens,
  3. `#if 0 ... #endif` regions inside an INTERNAL block (preprocessor-dead by
     construction, so invisible to any compiler that built the archives), and
  4. blank lines left behind at a deletion seam.

An INTERNAL block that still holds live tokens after (3) is *kept*, and keeps
its `BEGIN`/`END` fence lines along with its body, verbatim. Retaining the
fence is deliberate: the surviving content is still vendor-internal, and an
unfenced remnant would be invisible to `scan` -- the ratchet would lose sight
of it forever. Keeping the fence means the retained lines stay counted in the
baseline and stay reviewable at the next intake, and it makes the scrub
idempotent (a second run over a scrubbed tree is a no-op).

A line counts as live unless the only code left on it after comment-stripping
is a lone `;` (e.g. the stray semicolon trailing a fully commented-out
`/*typedef enum {...}*/;`), which carries no symbol. A bare `,` is *not*
treated as dead: it is an enum/initializer separator whose removal would
change live code.
"""
from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator

import yaml


# Vendor fences are not typographically consistent: the canonical form is four
# hashes on each side, but real drops also ship three (see
# `mcu/apollo3/hal/am_hal_uart.h`, `// #### INTERNAL END ###`). Accept three or
# more on either side, case-insensitively, so a stray variant can never slip
# past the ratchet.
MARKER_RE = re.compile(r"#{3,}\s*INTERNAL\s+(BEGIN|END)\s*#{3,}", re.IGNORECASE)
_IF_DIRECTIVE_RE = re.compile(r"^\s*#\s*(?:if|ifdef|ifndef)\b")
_IF_ZERO_RE = re.compile(r"^\s*#\s*if\s+0\s*(?:$|//|/\*)")
_ENDIF_RE = re.compile(r"^\s*#\s*endif\b")
_ELSE_RE = re.compile(r"^\s*#\s*(?:else|elif)\b")
# Every terminator `str.splitlines()` recognizes, anchored at end of line, so a
# keepends split can be reversed without guessing.
_LINE_TERMINATOR_RE = re.compile("(?:\r\n|[\n\r\v\f\x1c\x1d\x1e\x85\u2028\u2029])$")

# Suffixes worth scanning. Markers only ever appear in vendor C sources and
# headers today, but docs/build glue is cheap to cover and closes the obvious
# "move the note into a .md/.cmake" gap.
SCANNED_SUFFIXES = (".h", ".hpp", ".inc", ".c", ".cc", ".cpp", ".s", ".ld", ".icf", ".cmake", ".md", ".txt", ".yaml", ".yml")

EXCLUDED_DIR_NAMES = frozenset({".git", ".venv", "venv", "__pycache__", ".pytest_cache", ".cache", "node_modules", "build"})
# Scratch/staging workspaces: never committed, never gated.
EXCLUDED_RELATIVE_PREFIXES = ((("sdk-intake", "local")),)

BASELINE_RELATIVE = Path("sdk-intake") / "internal-marker-baseline.yaml"

# Trees already sanitized by `scrub`. Whatever markers they still carry are the
# retained fences of blocks that hold live declarations (see the module
# docstring); the baseline pins that exact count, so any reappearance of
# additional internal content is a regenerated-but-unsanitized drop.
SCRUBBED_TREE_PREFIXES = ("modules/nsx-ambiqsuite/sdk/mcu/atomiq110/",)


class InternalMarkerError(Exception):
    """A scan/scrub input could not be processed safely. Always fail closed."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


# --------------------------------------------------------------------------
# Scanning
# --------------------------------------------------------------------------
@dataclass(frozen=True)
class MarkerHit:
    path: str
    line: int
    kind: str
    text: str

    def render(self) -> str:
        return f"{self.path}:{self.line}: {self.text.strip()}"


def iter_scannable_files(
    root: Path,
    *,
    suffixes: Iterable[str] = SCANNED_SUFFIXES,
    excluded_dir_names: Iterable[str] = EXCLUDED_DIR_NAMES,
    excluded_relative_prefixes: Iterable[tuple[str, ...]] = EXCLUDED_RELATIVE_PREFIXES,
) -> Iterator[Path]:
    """Yield regular files under `root` worth scanning, in stable order."""
    if not root.is_dir():
        raise InternalMarkerError(f"scan root is not a directory: {root}")
    suffix_set = {suffix.lower() for suffix in suffixes}
    excluded_names = set(excluded_dir_names)
    excluded_prefixes = tuple(excluded_relative_prefixes)
    for path in sorted(root.rglob("*")):
        if path.is_symlink() or not path.is_file():
            continue
        relative_parts = path.relative_to(root).parts
        if any(part in excluded_names for part in relative_parts[:-1]):
            continue
        if any(relative_parts[: len(prefix)] == prefix for prefix in excluded_prefixes):
            continue
        if path.suffix.lower() not in suffix_set:
            continue
        yield path


def scan_text(text: str) -> list[tuple[int, str, str]]:
    """Return `(line_number, kind, line_text)` for every marker in `text`."""
    hits: list[tuple[int, str, str]] = []
    for index, line in enumerate(text.splitlines(), start=1):
        match = MARKER_RE.search(line)
        if match:
            hits.append((index, match.group(1).lower(), line))
    return hits


def scan_tree(root: Path, *, path_prefix: str = "", **kwargs) -> list[MarkerHit]:
    """Scan `root`, reporting paths as `<path_prefix>/<relative path>`."""
    prefix = path_prefix.strip("/")
    hits: list[MarkerHit] = []
    for path in iter_scannable_files(root, **kwargs):
        relative = path.relative_to(root).as_posix()
        key = f"{prefix}/{relative}" if prefix else relative
        text = path.read_text(encoding="utf-8", errors="replace")
        for line, kind, raw in scan_text(text):
            hits.append(MarkerHit(path=key, line=line, kind=kind, text=raw))
    return hits


def marker_counts(hits: Iterable[MarkerHit]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for hit in hits:
        counts[hit.path] = counts.get(hit.path, 0) + 1
    return dict(sorted(counts.items()))


@dataclass(frozen=True)
class BaselineComparison:
    regressions: tuple[tuple[str, int, int], ...]
    stale: tuple[tuple[str, int, int], ...]

    @property
    def ok(self) -> bool:
        return not self.regressions

    def render(self) -> str:
        lines = []
        for path, found, allowed in self.regressions:
            lines.append(f"{path}: {found} marker line(s), baseline allows {allowed}")
        return "\n".join(lines)


def compare_to_baseline(counts: dict[str, int], baseline: dict[str, int]) -> BaselineComparison:
    """Ratchet comparison: more markers than baseline is a regression, fewer is
    merely stale (a scrub landed and the baseline was not regenerated)."""
    regressions = tuple(
        (path, found, baseline.get(path, 0))
        for path, found in sorted(counts.items())
        if found > baseline.get(path, 0)
    )
    stale = tuple(
        (path, counts.get(path, 0), allowed)
        for path, allowed in sorted(baseline.items())
        if counts.get(path, 0) < allowed
    )
    return BaselineComparison(regressions=regressions, stale=stale)


def default_baseline_path() -> Path:
    return repo_root() / BASELINE_RELATIVE


def load_baseline(path: Path | None = None) -> dict[str, int]:
    baseline_path = path if path is not None else default_baseline_path()
    if not baseline_path.is_file():
        raise InternalMarkerError(f"internal-marker baseline not found: {baseline_path}")
    try:
        data = yaml.safe_load(baseline_path.read_text(encoding="utf-8")) or {}
    except (UnicodeDecodeError, yaml.YAMLError) as error:
        raise InternalMarkerError(f"{baseline_path} is not readable as YAML: {error}") from error
    if not isinstance(data, dict):
        raise InternalMarkerError(f"{baseline_path} must be a mapping at its top level")
    files = data.get("files") or {}
    if not isinstance(files, dict):
        raise InternalMarkerError(f"{baseline_path} 'files' must be a mapping of path -> marker count")
    baseline: dict[str, int] = {}
    for key, value in files.items():
        if not isinstance(key, str) or not isinstance(value, int) or isinstance(value, bool) or value < 0:
            raise InternalMarkerError(f"{baseline_path} entry {key!r} must map a path string to a non-negative int")
        baseline[key] = value
    return baseline


def load_fence_exemptions(path: Path | None = None) -> frozenset[str]:
    """Paths whose INTERNAL fences are known to be unparseable.

    Historical drops contain a handful of genuinely malformed fences (an orphan
    `BEGIN`, a nested pair, a marker sitting on a line of live code). They were
    reviewed once and recorded here; the gate fails only on a *new* one, the
    same ratchet shape the marker counts use."""
    baseline_path = path if path is not None else default_baseline_path()
    if not baseline_path.is_file():
        raise InternalMarkerError(f"internal-marker baseline not found: {baseline_path}")
    try:
        data = yaml.safe_load(baseline_path.read_text(encoding="utf-8")) or {}
    except (UnicodeDecodeError, yaml.YAMLError) as error:
        raise InternalMarkerError(f"{baseline_path} is not readable as YAML: {error}") from error
    entries = data.get("unparseable_fences") or []
    if not isinstance(entries, list) or not all(isinstance(entry, str) for entry in entries):
        raise InternalMarkerError(f"{baseline_path} 'unparseable_fences' must be a list of path strings")
    return frozenset(entries)


def render_baseline(counts: dict[str, int], unparseable: Iterable[str] = ()) -> str:
    header = (
        "# Ratchet baseline for vendor `#### INTERNAL ####` marker lines.\n"
        "#\n"
        "# Generated by `python sdk-intake/internal_markers.py update-baseline`.\n"
        "# Each `files` entry records how many marker lines a historical vendor drop\n"
        "# is still allowed to carry. The intake gate and tests/test_internal_markers.py\n"
        "# fail when a file exceeds its entry, or carries markers with no entry --\n"
        "# so a freshly regenerated payload can never add new internal content.\n"
        "# Counts may only ever go down: scrub the tree, then regenerate.\n"
        "#\n"
        "# This file is review-gated, not tool-gated: `update-baseline` is deliberately\n"
        "# self-service, because the control is the diff. Raising a count here is a\n"
        "# visible, reviewable line in a pull request; the gate exists so that raising\n"
        "# it is the *only* way new internal content can land.\n"
        "#\n"
        "# `unparseable_fences` records files whose fences the block parser refuses\n"
        "# (orphan/nested BEGIN or END, a marker on a line of live code, a non-UTF-8\n"
        "# payload). Counting marker lines cannot see an orphan fence -- deleting half\n"
        "# a pair makes the count go *down* -- so these are ratcheted separately.\n"
    )
    document = yaml.safe_dump(
        {
            "version": 1,
            "files": dict(sorted(counts.items())),
            "unparseable_fences": sorted(unparseable),
        },
        sort_keys=False,
        default_flow_style=False,
        width=200,
    )
    return header + document


# --------------------------------------------------------------------------
# Scrubbing
# --------------------------------------------------------------------------
def code_only_lines_with_state(lines: list[str]) -> tuple[list[str], list[tuple[bool, bool]]]:
    """Return `(code, states)`.

    `code` is `lines` with every comment (and string body) blanked out, so a
    line's remaining text is exactly its live code. Block-comment state is
    carried across lines, which is what makes a fully commented-out `/*typedef
    enum {...}*/` recognizable as dead rather than as code.

    `states[i]` is `(in_block_comment_before, in_block_comment_after)` for line
    `i`. The scrub uses it to refuse a marker line that itself opens or closes a
    block comment: deleting such a line would leave the surrounding `/*` or `*/`
    unbalanced and silently comment out (or uncomment) live code."""
    stripped: list[str] = []
    states: list[tuple[bool, bool]] = []
    in_block_comment = False
    for line in lines:
        entry_state = in_block_comment
        out: list[str] = []
        index = 0
        length = len(line)
        while index < length:
            if in_block_comment:
                end = line.find("*/", index)
                if end < 0:
                    index = length
                else:
                    in_block_comment = False
                    index = end + 2
                continue
            char = line[index]
            if char == "/" and index + 1 < length and line[index + 1] == "/":
                break
            if char == "/" and index + 1 < length and line[index + 1] == "*":
                in_block_comment = True
                index += 2
                continue
            if char in "\"'":
                quote = char
                out.append(char)
                index += 1
                while index < length:
                    if line[index] == "\\":
                        index += 2
                        continue
                    if line[index] == quote:
                        index += 1
                        break
                    index += 1
                continue
            out.append(char)
            index += 1
        stripped.append("".join(out))
        states.append((entry_state, in_block_comment))
    return stripped, states


def code_only_lines(lines: list[str]) -> list[str]:
    """`code_only_lines_with_state` without the block-comment state."""
    return code_only_lines_with_state(lines)[0]


def line_is_live(code: str) -> bool:
    """True when a comment-stripped line still carries a symbol.

    Only a lone `;` counts as dead -- the vestigial semicolon a fully
    commented-out `/*typedef enum {...}*/;` leaves behind. A bare `,` is live:
    it is an enum or initializer separator, and deleting it rewrites the
    surrounding live declaration."""
    stripped = code.strip()
    return bool(stripped) and stripped != ";"


def _find_blocks(
    lines: list[str],
    code: list[str],
    *,
    label: str,
    comment_states: list[tuple[bool, bool]] | None = None,
) -> list[tuple[int, int]]:
    """Return `(begin_index, end_index)` pairs, 0-based and inclusive.

    Fails closed on anything the scrub cannot reason about: an unbalanced or
    nested fence, a marker sharing its line with live code, or a marker line
    that itself opens or closes a block comment."""
    blocks: list[tuple[int, int]] = []
    open_index: int | None = None
    for index, line in enumerate(lines):
        match = MARKER_RE.search(line)
        if not match:
            continue
        if code[index].strip():
            raise InternalMarkerError(
                f"{label}:{index + 1}: INTERNAL marker shares a line with live code; refusing to scrub"
            )
        if comment_states is not None and comment_states[index][0] != comment_states[index][1]:
            raise InternalMarkerError(
                f"{label}:{index + 1}: INTERNAL marker line opens or closes a block comment "
                "that spans it; refusing to scrub"
            )
        if match.group(1).upper() == "BEGIN":
            if open_index is not None:
                raise InternalMarkerError(f"{label}:{index + 1}: nested INTERNAL BEGIN (opened at line {open_index + 1})")
            open_index = index
        else:
            if open_index is None:
                raise InternalMarkerError(f"{label}:{index + 1}: INTERNAL END without a matching BEGIN")
            blocks.append((open_index, index))
            open_index = None
    if open_index is not None:
        raise InternalMarkerError(f"{label}:{open_index + 1}: INTERNAL BEGIN without a matching END")
    return blocks


def _find_if_zero_regions(code: list[str], start: int, stop: int, *, label: str) -> list[tuple[int, int]]:
    """Return `(if_index, endif_index)` pairs for `#if 0` regions fully inside
    `[start, stop)`. Regions carrying a top-level `#else`/`#elif` have a live
    branch and are left alone.

    `code` must be the *comment-stripped* lines. Matching raw lines instead
    would count a commented-out `#if`/`#else`/`#endif` -- which vendor headers
    are full of, inside the very INTERNAL blocks this walks -- as a real
    directive, mistracking depth and deleting live code or leaving an orphan
    `#endif` behind."""
    regions: list[tuple[int, int]] = []
    index = start
    while index < stop:
        if not _IF_ZERO_RE.match(code[index]):
            index += 1
            continue
        depth = 1
        has_else = False
        cursor = index + 1
        end_index: int | None = None
        while cursor < stop:
            line = code[cursor]
            if _IF_DIRECTIVE_RE.match(line):
                depth += 1
            elif _ENDIF_RE.match(line):
                depth -= 1
                if depth == 0:
                    end_index = cursor
                    break
            elif depth == 1 and _ELSE_RE.match(line):
                has_else = True
            cursor += 1
        if end_index is None:
            # `#endif` lives outside the INTERNAL block (or is missing): the
            # region is not self-contained, so removing it would unbalance the
            # file. Leave it and let the scan keep reporting it.
            index += 1
            continue
        if not has_else:
            regions.append((index, end_index))
        index = end_index + 1
    return regions


@dataclass(frozen=True)
class ScrubResult:
    text: str
    removed: tuple[tuple[int, str], ...]
    removed_blocks: tuple[tuple[int, int], ...]
    kept_blocks: tuple[tuple[int, int], ...]
    removed_if_zero: tuple[tuple[int, int], ...]

    @property
    def changed(self) -> bool:
        return bool(self.removed)


def split_lines_keepends(text: str) -> tuple[list[str], list[str]]:
    """Return `(raw, bodies)`: `raw` is `text` split with its line terminators
    intact, `bodies` the same lines with the terminator removed.

    The scrub rebuilds its output by joining surviving `raw` lines, so CRLF
    endings, a bare CR, a form feed used as a page break, and a missing final
    newline all survive byte-for-byte. Splitting on `str.splitlines()` and
    re-joining with `"\\n"` would silently rewrite every one of them, turning a
    deletion-only pass into a whole-file reformat."""
    raw = text.splitlines(keepends=True)
    return raw, [_LINE_TERMINATOR_RE.sub("", line) for line in raw]


def scrub_text(text: str, *, label: str = "<text>") -> ScrubResult:
    raw_lines, lines = split_lines_keepends(text)
    code, comment_states = code_only_lines_with_state(lines)
    blocks = _find_blocks(lines, code, label=label, comment_states=comment_states)

    doomed: set[int] = set()
    removed_blocks: list[tuple[int, int]] = []
    kept_blocks: list[tuple[int, int]] = []
    removed_if_zero: list[tuple[int, int]] = []

    for begin, end in blocks:
        if_zero_regions = _find_if_zero_regions(code, begin + 1, end, label=label)
        dead_from_if_zero = {index for start, stop in if_zero_regions for index in range(start, stop + 1)}
        survivors = [index for index in range(begin + 1, end) if index not in dead_from_if_zero]
        if any(line_is_live(code[index]) for index in survivors):
            # Live declarations remain. Keep the body (including its
            # documentation) byte-for-byte *and* keep the fence: the retained
            # content is still vendor-internal, and an unfenced remnant would
            # drop out of `scan` and out of the ratchet permanently.
            doomed.update(dead_from_if_zero)
            kept_blocks.append((begin + 1, end + 1))
        else:
            doomed.update(range(begin, end + 1))
            removed_blocks.append((begin + 1, end + 1))
        removed_if_zero.extend((start + 1, stop + 1) for start, stop in if_zero_regions)

    doomed |= _seam_blank_lines(lines, raw_lines, doomed)

    removed = tuple((index + 1, lines[index]) for index in sorted(doomed))
    scrubbed = "".join(line for index, line in enumerate(raw_lines) if index not in doomed)
    return ScrubResult(
        text=scrubbed,
        removed=removed,
        removed_blocks=tuple(removed_blocks),
        kept_blocks=tuple(kept_blocks),
        removed_if_zero=tuple(removed_if_zero),
    )


_COLLAPSIBLE_TERMINATORS = frozenset({"", "\n", "\r\n", "\r"})


def _seam_blank_lines(lines: list[str], raw_lines: list[str], doomed: set[int]) -> set[int]:
    """Collapse the run of blank lines a removal leaves behind down to a single
    blank line. Only ever returns indices of blank lines, and only for runs that
    a removal actually touched -- pre-existing blank runs elsewhere in the file
    are left exactly as the vendor shipped them.

    A "blank" line here must also end in an ordinary newline. A line terminated
    by a form feed has an empty body but is a deliberate page break, and vendor
    headers use them; collapsing one away would delete formatting the scrub was
    never asked to touch."""
    extra: set[int] = set()
    survivors = [index for index in range(len(lines)) if index not in doomed]
    run: list[int] = []
    run_touched_removal = False
    previous: int | None = None

    def is_blank(index: int) -> bool:
        return not lines[index].strip() and raw_lines[index] in _COLLAPSIBLE_TERMINATORS

    def flush() -> None:
        if run_touched_removal and len(run) > 1:
            extra.update(run[1:])

    for index in survivors:
        gap = previous is not None and index != previous + 1
        if is_blank(index):
            if gap:
                run_touched_removal = True
            run.append(index)
        else:
            if gap:
                run_touched_removal = True
            flush()
            run, run_touched_removal = [], False
        previous = index
    flush()
    return extra


def read_source(path: Path) -> str:
    """Read `path` without newline translation, so CRLF stays CRLF."""
    try:
        return path.read_bytes().decode("utf-8")
    except UnicodeDecodeError as error:
        raise InternalMarkerError(f"{path} is not valid UTF-8: {error}") from error


def scrub_file(path: Path, *, write: bool) -> ScrubResult:
    result = scrub_text(read_source(path), label=path.as_posix())
    if write and result.changed:
        # `write_bytes`, not `write_text`: the latter would translate "\n" to
        # `os.linesep` and re-break the endings `split_lines_keepends` preserved.
        path.write_bytes(result.text.encode("utf-8"))
    return result


def scrub_tree(root: Path, *, write: bool, **kwargs) -> dict[str, ScrubResult]:
    """Scrub every scannable file under `root`.

    Two-phase when writing: every file is scrubbed in memory first, so a single
    refusal (unbalanced fence, impure marker line, non-UTF-8 payload) aborts the
    run before *any* file is touched. A partially-scrubbed tree is the worst
    possible outcome -- it is neither the vendor drop nor a sanitized payload,
    and nothing records which files were reached."""
    planned: list[tuple[Path, str, ScrubResult]] = []
    for path in iter_scannable_files(root, **kwargs):
        result = scrub_file(path, write=False)
        if result.changed:
            planned.append((path, path.relative_to(root).as_posix(), result))
    if write:
        for path, _relative, result in planned:
            path.write_bytes(result.text.encode("utf-8"))
    return {relative: result for _path, relative, result in planned}


def find_unparseable_fences(root: Path, *, path_prefix: str = "", **kwargs) -> list[tuple[str, str]]:
    """Return `(path, message)` for every file whose INTERNAL fences cannot be
    parsed: an orphan/nested `BEGIN`/`END`, a marker sharing its line with live
    code, a marker straddling a block comment, or an undecodable payload.

    `scan` only counts marker lines, so it is blind to an orphan fence -- delete
    one line of a pair and the count *drops*, which the ratchet reads as an
    improvement while the block silently swallows everything after it. This
    walks the same files with the scrub's own block parser so that shape is
    visible to the gate too."""
    prefix = path_prefix.strip("/")
    problems: list[tuple[str, str]] = []
    for path in iter_scannable_files(root, **kwargs):
        relative = path.relative_to(root).as_posix()
        key = f"{prefix}/{relative}" if prefix else relative
        try:
            _raw, lines = split_lines_keepends(read_source(path))
        except InternalMarkerError as error:
            problems.append((key, f"{key}: {str(error).split(': ', 1)[-1]}"))
            continue
        if not any(MARKER_RE.search(line) for line in lines):
            continue
        try:
            code, comment_states = code_only_lines_with_state(lines)
            _find_blocks(lines, code, label=key, comment_states=comment_states)
        except InternalMarkerError as error:
            problems.append((key, str(error)))
    return problems


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------
def _cmd_scan(args: argparse.Namespace) -> int:
    hits = scan_tree(args.root, path_prefix=args.path_prefix)
    counts = marker_counts(hits)
    if args.no_baseline:
        for hit in hits:
            print(hit.render())
        print(f"==> {len(hits)} marker line(s) in {len(counts)} file(s) under {args.root}")
        return 1 if hits else 0

    comparison = compare_to_baseline(counts, load_baseline(args.baseline))
    exempt = load_fence_exemptions(args.baseline)
    new_fence_problems = [
        message for path, message in find_unparseable_fences(args.root, path_prefix=args.path_prefix)
        if path not in exempt
    ]
    offending = {path for path, _found, _allowed in comparison.regressions}
    for hit in hits:
        if hit.path in offending:
            print(hit.render())
    if new_fence_problems:
        print("error: INTERNAL fences that cannot be parsed (a count-based ratchet cannot see these):", file=sys.stderr)
        for message in new_fence_problems:
            print(f"  {message}", file=sys.stderr)
        return 1
    if comparison.regressions:
        print("error: new INTERNAL content relative to the recorded baseline:", file=sys.stderr)
        print(comparison.render(), file=sys.stderr)
        print(
            "Scrub the payload at intake (`python sdk-intake/internal_markers.py scrub --root <tree> --write`) "
            "rather than deleting content from an already-shipped drop.",
            file=sys.stderr,
        )
        return 1
    print(f"==> OK: no new INTERNAL markers under {args.root} ({len(hits)} baselined marker line(s))")
    if comparison.stale and args.report_stale:
        print(f"    note: {len(comparison.stale)} baseline entry/entries now over-allow; regenerate with update-baseline")
    return 0


def _cmd_scrub(args: argparse.Namespace) -> int:
    results = scrub_tree(args.root, write=args.write)
    total = 0
    for relative, result in sorted(results.items()):
        total += len(result.removed)
        print(f"{relative}: -{len(result.removed)} line(s) "
              f"({len(result.removed_blocks)} dead block(s) removed, "
              f"{len(result.kept_blocks)} block(s) kept with live code, "
              f"{len(result.removed_if_zero)} #if 0 region(s) removed)")
        if args.verbose:
            for line_number, text in result.removed:
                print(f"    -{line_number}: {text}")
    verb = "removed" if args.write else "would remove"
    print(f"==> {verb} {total} line(s) across {len(results)} file(s) under {args.root}")
    if not args.write:
        print("    (dry run; pass --write to apply)")
    return 0


def _cmd_update_baseline(args: argparse.Namespace) -> int:
    hits = scan_tree(args.root, path_prefix=args.path_prefix)
    counts = marker_counts(hits)
    unparseable = [path for path, _message in find_unparseable_fences(args.root, path_prefix=args.path_prefix)]
    destination = args.baseline if args.baseline is not None else default_baseline_path()
    destination.write_text(render_baseline(counts, unparseable), encoding="utf-8")
    print(f"==> Wrote {destination} ({sum(counts.values())} marker line(s) across {len(counts)} file(s), "
          f"{len(unparseable)} file(s) with unparseable fences)")
    return 0


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers(dest="command", required=True)

    scan = subparsers.add_parser("scan", help="Report INTERNAL markers as path:line, ratcheted against the baseline.")
    scan.add_argument("--root", type=Path, default=repo_root(), help="Tree to scan. Default: repository root.")
    scan.add_argument("--path-prefix", default="", help="Prefix reported paths, e.g. modules/nsx-ambiqsuite/sdk for a staged payload.")
    scan.add_argument("--baseline", type=Path, default=None, help=f"Baseline file. Default: {BASELINE_RELATIVE.as_posix()}.")
    scan.add_argument("--no-baseline", action="store_true", help="Report every marker and exit non-zero if any exist.")
    scan.add_argument("--report-stale", action="store_true", help="Note baseline entries that now over-allow.")
    scan.set_defaults(func=_cmd_scan)

    scrub = subparsers.add_parser("scrub", help="Remove INTERNAL fences, dead INTERNAL blocks, and their #if 0 regions.")
    scrub.add_argument("--root", type=Path, required=True, help="Tree to scrub, e.g. a staged payload.")
    scrub.add_argument("--write", action="store_true", help="Apply the scrub. Without it, this is a dry run.")
    scrub.add_argument("--verbose", action="store_true", help="List every removed line.")
    scrub.set_defaults(func=_cmd_scrub)

    update = subparsers.add_parser("update-baseline", help="Regenerate the ratchet baseline from a tree.")
    update.add_argument("--root", type=Path, default=repo_root(), help="Tree to scan. Default: repository root.")
    update.add_argument("--path-prefix", default="", help="Prefix recorded paths.")
    update.add_argument("--baseline", type=Path, default=None, help=f"Destination. Default: {BASELINE_RELATIVE.as_posix()}.")
    update.set_defaults(func=_cmd_update_baseline)

    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        return args.func(args)
    except InternalMarkerError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
