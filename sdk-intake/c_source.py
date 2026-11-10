"""Conservative C lexical lines with physical provenance, not a C parser."""
from __future__ import annotations

import re
from dataclasses import dataclass


class SourceSyntaxError(ValueError):
    """A source spelling cannot be transformed without choosing a dialect."""


@dataclass(frozen=True)
class LogicalLine:
    start: int
    stop: int  # inclusive physical indices
    code: str


@dataclass(frozen=True)
class SourceLines:
    raw: tuple[str, ...]
    bodies: tuple[str, ...]
    code: tuple[str, ...]
    comment_states: tuple[tuple[bool, bool], ...]
    logical: tuple[LogicalLine, ...]


_TERMINATOR = re.compile(r"(?:\r\n|\n|\r)$")
_PHYSICAL = re.compile(r"[^\r\n]*(?:\r\n|\r|\n|$)")
_TRIGRAPH = re.compile(r"\?\?[=/\x27()!<>-]")
_NONSTANDARD_SPLICE = re.compile(r"\\[ \t\v\f]+(?:\r\n|\r|\n)")


def split_lines(text: str) -> tuple[list[str], list[str]]:
    """C newline boundaries only; form feeds and vertical tabs are whitespace."""
    raw = [line for line in _PHYSICAL.findall(text) if line]
    return raw, [_TERMINATOR.sub("", line) for line in raw]


def analyze(text: str) -> SourceLines:
    """Join phase-2 continuations, then recognize comments and literal tokens.

    Output always uses original raw lines. Analysis newlines inside block
    comments disappear with the comment, so multiline preprocessor expressions
    remain one logical line. Ambiguous dialect features are refused, not guessed.
    """
    if _TRIGRAPH.search(text):
        raise SourceSyntaxError("trigraph spelling requires explicit dialect review")
    if _NONSTANDARD_SPLICE.search(text):
        raise SourceSyntaxError("backslash-whitespace-newline requires explicit dialect review")
    raw, bodies = split_lines(text)
    pieces: list[str] = []
    owners: list[int] = []
    for number, (line, body) in enumerate(zip(raw, bodies)):
        has_newline = line != body
        if has_newline and body.endswith("\\"):
            piece = body[:-1]
        else:
            piece = body + ("\n" if has_newline else "")
        pieces.append(piece)
        owners.extend([number] * len(piece))
    source = "".join(pieces)
    physical_code: list[list[str]] = [[] for _ in raw]
    before = [False] * len(raw)
    after = [False] * len(raw)
    logical: list[LogicalLine] = []
    logical_code: list[str] = []
    logical_start = 0
    physical = -1
    block_comment = False
    line_comment = False
    index = 0

    def touch(position: int) -> int:
        nonlocal physical
        number = owners[position]
        while physical < number:
            if physical >= 0:
                after[physical] = block_comment
            physical += 1
            before[physical] = block_comment
        return number

    def emit(char: str, number: int) -> None:
        physical_code[number].append(char)
        logical_code.append(char)

    while index < len(source):
        number = touch(index)
        char = source[index]
        following = source[index:index + 2]
        if block_comment:
            if following == "*/":
                touch(index + 1)
                block_comment = False
                index += 2
            else:
                index += 1
            continue
        if char == "\n":
            logical.append(LogicalLine(logical_start, number, "".join(logical_code)))
            logical_start = number + 1
            logical_code = []
            line_comment = False
            index += 1
            continue
        if line_comment:
            index += 1
            continue
        if following in ("/*", "//"):
            emit(" ", number)  # Comments separate tokens; never concatenate them.
            touch(index + 1)
            block_comment = following == "/*"
            line_comment = following == "//"
            index += 2
            continue
        if following == 'R"':
            raise SourceSyntaxError("C++ raw string literal requires explicit dialect review")
        if char in "\"'":
            quote = char
            emit(quote, number)
            index += 1
            while index < len(source):
                number = touch(index)
                char = source[index]
                # Every physical part of a continued literal remains live.
                physical_code[number].append(quote)
                if char == "\n":
                    raise SourceSyntaxError("newline in an unterminated quoted literal")
                if char == "\\":
                    if index + 1 < len(source):
                        physical_code[touch(index + 1)].append(quote)
                    index += 2
                    continue
                index += 1
                if char == quote:
                    logical_code.append(quote)
                    break
            else:
                raise SourceSyntaxError("unterminated quoted literal")
            continue
        emit(char, number)
        index += 1

    if block_comment:
        raise SourceSyntaxError("unterminated block comment")
    while physical + 1 < len(raw):
        if physical >= 0:
            after[physical] = block_comment
        physical += 1
        before[physical] = block_comment
    if physical >= 0:
        after[physical] = block_comment
    if logical_start < len(raw):
        logical.append(LogicalLine(logical_start, len(raw) - 1, "".join(logical_code)))
    return SourceLines(tuple(raw), tuple(bodies), tuple("".join(line) for line in physical_code),
                       tuple(zip(before, after)), tuple(logical))
