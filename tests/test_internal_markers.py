from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

import pytest


ATOMIQ110_TREE = Path("modules") / "nsx-ambiqsuite" / "sdk" / "mcu" / "atomiq110"
ATOMIQ110_PREFIX = ATOMIQ110_TREE.as_posix()

# Ratchet constant. The atomiq110 payload was scrubbed of vendor
# `#### INTERNAL ####` engineering content at intake (issue #52). It does not go
# to zero: a block that still holds live declarations keeps its body *and* its
# fence, because a live declaration fenced inside an INTERNAL block is part of
# the ABI the drop's prebuilt archives were compiled against, and an unfenced
# remnant would drop out of `scan` and out of this ratchet forever. This is the
# exact number of fence lines the scrub retained; it may go down (a future
# intake removes the declaration upstream), never up.
ATOMIQ110_RETAINED_MARKERS = 66

INTERNAL_BLOCK_SAMPLE = """\
#ifndef SAMPLE_H
#define SAMPLE_H

typedef enum
{
    AM_SAMPLE_CONTROL_LIVE,
// #### INTERNAL BEGIN ####
    AM_SAMPLE_CONTROL_FENCED_BUT_LIVE,
    // AM_SAMPLE_CONTROL_COMMENTED_OUT,
// #### INTERNAL END ####
} am_sample_control_e;

// #### INTERNAL BEGIN ####
//
// TODO: ticket chatter about an unreleased block.
//
/*typedef enum
{
    AM_SAMPLE_DEAD_ENTRY,
} am_sample_dead_e;*/
// #### INTERNAL END ####

// #### INTERNAL BEGIN ####
#if 0 // never compiled, so never part of the prebuilt archives
extern uint32_t am_sample_disabled(void);
#endif
extern uint32_t am_sample_fenced_prototype(void);
// #### INTERNAL END ####

#endif // SAMPLE_H
"""

# A miniature stand-in for the real vendor payload, in the shapes the atomiq110
# headers actually use. `test_scrub_reproduces_its_output_byte_for_byte` pins
# the tool's output over PRE to exactly POST, which is the same property the
# committed atomiq110 tree has against `origin/main`: the payload is not
# hand-edited, it is a function of the vendor drop and this tool. Encoding it on
# a sample rather than the 30-file vendor tree keeps the suite fast and keeps
# the pre-scrub tree out of the repo.
VENDOR_SAMPLE_PRE = """\
//*****************************************************************************
//
//! @file am_hal_sample.h
//
//*****************************************************************************
#ifndef AM_HAL_SAMPLE_H
#define AM_HAL_SAMPLE_H

// #### INTERNAL BEGIN ####
// JIRA FALCSW-0000: this whole note is engineering-internal.
// #### INTERNAL END ####

//! @brief Sample control modes.
typedef enum
{
    AM_HAL_SAMPLE_MODE_A = 0,
    AM_HAL_SAMPLE_MODE_B = 1,
// #### INTERNAL BEGIN ####
    AM_HAL_SAMPLE_MODE_UNRELEASED = 2,
// #### INTERNAL END ####
} am_hal_sample_mode_e;

// #### INTERNAL BEGIN ####
#if 0
extern uint32_t am_hal_sample_experiment(void);
#endif
// #### INTERNAL END ####

extern uint32_t am_hal_sample_init(void);

#endif // AM_HAL_SAMPLE_H
"""

VENDOR_SAMPLE_POST = """\
//*****************************************************************************
//
//! @file am_hal_sample.h
//
//*****************************************************************************
#ifndef AM_HAL_SAMPLE_H
#define AM_HAL_SAMPLE_H

//! @brief Sample control modes.
typedef enum
{
    AM_HAL_SAMPLE_MODE_A = 0,
    AM_HAL_SAMPLE_MODE_B = 1,
// #### INTERNAL BEGIN ####
    AM_HAL_SAMPLE_MODE_UNRELEASED = 2,
// #### INTERNAL END ####
} am_hal_sample_mode_e;

extern uint32_t am_hal_sample_init(void);

#endif // AM_HAL_SAMPLE_H
"""


def load_module(repo_root: Path, file_name: str, module_name: str):
    module_path = repo_root / "sdk-intake" / file_name
    spec = importlib.util.spec_from_file_location(module_name, module_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


@pytest.fixture()
def markers(repo_root: Path):
    return load_module(repo_root, "internal_markers.py", "test_internal_markers_module")


@pytest.fixture()
def workflow(repo_root: Path):
    return load_module(repo_root, "intake_workflow.py", "test_internal_markers_workflow")


# --------------------------------------------------------------------------
# Repo-wide ratchet
# --------------------------------------------------------------------------
def test_atomiq110_markers_do_not_exceed_the_retained_fence_count(repo_root: Path, markers) -> None:
    hits = markers.scan_tree(repo_root / ATOMIQ110_TREE, path_prefix=ATOMIQ110_PREFIX)
    assert len(hits) <= ATOMIQ110_RETAINED_MARKERS, "\n".join(hit.render() for hit in hits)


def test_every_atomiq110_marker_is_a_retained_kept_block_fence(repo_root: Path, markers) -> None:
    """The scrub removed every dead block outright, so every marker still in the
    tree must belong to a balanced block that the scrub deliberately kept --
    never an orphan, never a block that is dead but was missed."""
    retained = 0
    for path in markers.iter_scannable_files(repo_root / ATOMIQ110_TREE):
        result = markers.scrub_text(markers.read_source(path), label=path.as_posix())
        assert result.removed_blocks == (), f"{path}: dead INTERNAL block survived the scrub"
        retained += 2 * len(result.kept_blocks)
    assert retained == ATOMIQ110_RETAINED_MARKERS


def test_repo_wide_markers_do_not_exceed_recorded_baseline(repo_root: Path, markers) -> None:
    counts = markers.marker_counts(markers.scan_tree(repo_root))
    comparison = markers.compare_to_baseline(counts, markers.load_baseline())
    assert comparison.ok, (
        "new INTERNAL engineering content relative to "
        f"{markers.BASELINE_RELATIVE.as_posix()}:\n{comparison.render()}"
    )


def test_baseline_never_over_allows(repo_root: Path, markers) -> None:
    """A stale entry would silently re-open room for internal content to come
    back in a later drop, so the ratchet is regenerated after every scrub."""
    counts = markers.marker_counts(markers.scan_tree(repo_root))
    comparison = markers.compare_to_baseline(counts, markers.load_baseline())
    assert comparison.stale == ()


def test_baseline_pins_the_scrubbed_tree_to_its_retained_fences(repo_root: Path, markers) -> None:
    baseline = markers.load_baseline()
    scrubbed = {
        path: count for path, count in baseline.items()
        if any(path.startswith(prefix) for prefix in markers.SCRUBBED_TREE_PREFIXES)
    }
    assert sum(scrubbed.values()) == ATOMIQ110_RETAINED_MARKERS
    # Fences come in pairs; an odd entry would mean an orphan got baselined.
    assert [path for path, count in scrubbed.items() if count % 2] == []


def test_unparseable_fence_exemptions_are_exact(repo_root: Path, markers) -> None:
    """The fence-shape ratchet is regenerated alongside the counts: a stale
    exemption would let a genuinely broken fence in under an old file's name."""
    found = {path for path, _message in markers.find_unparseable_fences(repo_root)}
    assert found == set(markers.load_fence_exemptions())


# --------------------------------------------------------------------------
# Reproduction 1: three-hash fences (`// #### INTERNAL END ###`)
# --------------------------------------------------------------------------
def test_marker_regex_accepts_three_hash_fences(markers) -> None:
    """Real drops are not typographically consistent. The old four-hash-only
    pattern silently skipped these, so the fenced content was invisible to the
    ratchet and to the scrub."""
    assert markers.MARKER_RE.search("// #### INTERNAL END ###")
    assert markers.MARKER_RE.search("// ### INTERNAL BEGIN ###")
    assert markers.MARKER_RE.search("// ##### INTERNAL BEGIN #####")
    assert markers.MARKER_RE.search("// #### internal begin ####")
    assert not markers.MARKER_RE.search("// ## INTERNAL BEGIN ##")


@pytest.mark.parametrize("soc", ["apollo3", "apollo3p"])
def test_three_hash_fences_in_the_real_tree_are_counted(repo_root: Path, markers, soc: str) -> None:
    header = repo_root / "modules" / "nsx-ambiqsuite" / "sdk" / "mcu" / soc / "hal" / "am_hal_uart.h"
    hits = markers.scan_text(header.read_text(encoding="utf-8"))
    assert any(line.rstrip().endswith("###") and not line.rstrip().endswith("####") for _n, _k, line in hits), (
        f"{header}: three-hash INTERNAL fence not detected"
    )


# --------------------------------------------------------------------------
# Reproduction 2: preprocessor directives that only exist inside a comment
# --------------------------------------------------------------------------
def test_scrub_ignores_preprocessor_directives_inside_block_comments(markers) -> None:
    """A commented-out `#endif` used to terminate the `#if 0` walk early, which
    deleted the opening `/*` while leaving its `*/`, and left the real `#endif`
    with nothing to close. Matching comment-stripped lines fixes the tracking."""
    text = (
        "// #### INTERNAL BEGIN ####\n"
        "#if 0\n"
        "extern uint32_t am_sample_dead(void);\n"
        "/*\n"
        "#endif\n"
        "*/\n"
        "#endif\n"
        "extern uint32_t am_sample_live(void);\n"
        "// #### INTERNAL END ####\n"
    )
    scrubbed = markers.scrub_text(text, label="sample.h").text
    assert "am_sample_live" in scrubbed
    assert "am_sample_dead" not in scrubbed
    # The whole `#if 0 ... #endif` region went, comment and all: no orphan
    # `#endif` and no orphan comment terminator left behind.
    assert "#endif" not in scrubbed
    assert "*/" not in scrubbed
    assert "/*" not in scrubbed


# --------------------------------------------------------------------------
# Reproduction 3: a bare `,` is live code, not vestigial punctuation
# --------------------------------------------------------------------------
def test_scrub_keeps_a_block_whose_only_survivor_is_a_separator_comma(markers) -> None:
    """`line_is_live` used to strip `;,`, so a fenced line holding only the
    comma that separates two live initializers read as dead and the block was
    deleted -- silently fusing the two entries into a syntax error."""
    text = (
        "static const uint32_t g_table[] =\n"
        "{\n"
        "    0x00000001\n"
        "// #### INTERNAL BEGIN ####\n"
        "    ,   // separator, kept with the internal note below\n"
        "// #### INTERNAL END ####\n"
        "    0x00000002\n"
        "};\n"
    )
    scrubbed = markers.scrub_text(text, label="sample.h").text
    assert scrubbed == text, "the separator comma is live code; the block must be kept intact"
    assert markers.line_is_live(",")
    assert not markers.line_is_live(";")
    assert not markers.line_is_live("   ")


# --------------------------------------------------------------------------
# Reproduction 4: a marker line that opens or closes a block comment
# --------------------------------------------------------------------------
def test_scrub_refuses_a_marker_line_that_straddles_a_block_comment(markers) -> None:
    """Deleting `// #### INTERNAL BEGIN #### */` would take the `*/` with it and
    leave the preceding `/*` swallowing every live declaration after it."""
    text = (
        "extern uint32_t am_sample_live(void);\n"
        "/* engineering note\n"
        "// #### INTERNAL BEGIN #### */\n"
        "extern uint32_t am_sample_fenced(void);\n"
        "// #### INTERNAL END ####\n"
    )
    with pytest.raises(markers.InternalMarkerError, match="block comment"):
        markers.scrub_text(text, label="sample.h")

    # The mirror case: a marker line that *opens* a comment it does not close.
    opening = (
        "/* #### INTERNAL BEGIN ####\n"
        "extern uint32_t am_sample_fenced(void);\n"
        "*/\n"
        "// #### INTERNAL END ####\n"
    )
    with pytest.raises(markers.InternalMarkerError, match="block comment"):
        markers.scrub_text(opening, label="sample.h")


# --------------------------------------------------------------------------
# Reproduction 5: an orphan fence the count-based ratchet cannot see
# --------------------------------------------------------------------------
def test_staging_gate_rejects_an_orphan_fence_the_count_ratchet_allows(tmp_path: Path, markers, workflow) -> None:
    """Marker counts are blind to fence *shape*: an unmatched BEGIN makes the
    count go down, which the ratchet reads as an improvement, while the
    unterminated block hides everything after it. The gate parses the fences."""
    staged = tmp_path / "sdk"
    header_dir = staged / "mcu" / "atomiq110"
    header_dir.mkdir(parents=True)
    (header_dir / "am_hal_orphan.h").write_text(
        "// #### INTERNAL BEGIN ####\n"
        "// first note\n"
        "// #### INTERNAL BEGIN ####\n"
        "// second note\n"
        "// #### INTERNAL END ####\n",
        encoding="utf-8",
    )

    # A baseline that *allows* this file's three marker lines, so the count
    # ratchet passes and only the fence-shape check can fail the stage.
    baseline = tmp_path / "baseline.yaml"
    baseline.write_text(
        "version: 1\n"
        "files:\n"
        "  modules/nsx-ambiqsuite/sdk/mcu/atomiq110/am_hal_orphan.h: 3\n"
        "unparseable_fences: []\n",
        encoding="utf-8",
    )
    counts = markers.marker_counts(markers.scan_tree(staged, path_prefix="modules/nsx-ambiqsuite/sdk"))
    assert markers.compare_to_baseline(counts, markers.load_baseline(baseline)).ok

    with pytest.raises(workflow.IntakeVerificationError) as excinfo:
        workflow.assert_no_new_internal_markers(
            staged, path_prefix="modules/nsx-ambiqsuite/sdk", baseline=baseline
        )
    assert "cannot be parsed" in str(excinfo.value)
    assert "am_hal_orphan.h" in str(excinfo.value)


# --------------------------------------------------------------------------
# Reproduction 6: line endings and page breaks survive a deletion-only pass
# --------------------------------------------------------------------------
def test_scrub_preserves_line_endings_and_form_feeds(markers) -> None:
    """Splitting on `str.splitlines()` and re-joining with "\\n" rewrote every
    CRLF and turned a form-feed page break into a newline -- turning a
    deletion-only sanitation into a whole-file reformat."""
    text = (
        "extern uint32_t am_sample_live(void);\r\n"
        "\r\n"
        "// #### INTERNAL BEGIN ####\r\n"
        "// ticket chatter\r\n"
        "// #### INTERNAL END ####\r\n"
        "\r\n"
        "\fextern uint32_t am_sample_after_page_break(void);\r\n"
    )
    scrubbed = markers.scrub_text(text, label="sample.h").text
    assert "ticket chatter" not in scrubbed
    assert "\n" not in scrubbed.replace("\r\n", "")
    assert scrubbed.count("\r\n") == scrubbed.count("\n")
    assert "\f" in scrubbed
    assert scrubbed == (
        "extern uint32_t am_sample_live(void);\r\n"
        "\r\n"
        "\fextern uint32_t am_sample_after_page_break(void);\r\n"
    )


def test_scrub_preserves_a_missing_final_newline(markers) -> None:
    text = "// #### INTERNAL BEGIN ####\n// note\n// #### INTERNAL END ####\nextern int am_x(void);"
    assert markers.scrub_text(text, label="sample.h").text == "extern int am_x(void);"


# --------------------------------------------------------------------------
# Scrub semantics
# --------------------------------------------------------------------------
def test_scrub_removes_dead_blocks_and_keeps_live_declarations(markers) -> None:
    result = markers.scrub_text(INTERNAL_BLOCK_SAMPLE, label="sample.h")
    scrubbed = result.text

    # Live declarations survive, fenced or not -- and so do the fences around
    # them, so the retained content stays visible to `scan`.
    assert "AM_SAMPLE_CONTROL_LIVE," in scrubbed
    assert "AM_SAMPLE_CONTROL_FENCED_BUT_LIVE," in scrubbed
    assert "am_sample_fenced_prototype" in scrubbed
    assert len(markers.scan_text(scrubbed)) == 2 * len(result.kept_blocks) == 4
    # Commented-out and preprocessor-dead content does not.
    assert "AM_SAMPLE_DEAD_ENTRY" not in scrubbed
    assert "am_sample_disabled" not in scrubbed
    assert "#if 0" not in scrubbed
    assert "ticket chatter" not in scrubbed
    # A block with live code keeps its body verbatim, comments included.
    assert "// AM_SAMPLE_CONTROL_COMMENTED_OUT," in scrubbed
    # Deletion only: every removed line came from the original.
    original_lines = INTERNAL_BLOCK_SAMPLE.splitlines()
    assert all(text == original_lines[number - 1] for number, text in result.removed)
    assert set(scrubbed.splitlines()) <= set(original_lines)


def test_scrub_is_idempotent_on_its_own_output(markers) -> None:
    """Retaining kept-block fences is what makes this true: a second pass sees
    the same balanced blocks, finds live code in them, and changes nothing."""
    once = markers.scrub_text(INTERNAL_BLOCK_SAMPLE, label="sample.h").text
    twice = markers.scrub_text(once, label="sample.h")
    assert not twice.changed
    assert twice.text == once


def test_scrub_only_deletes_comment_marker_or_preprocessor_dead_lines(markers) -> None:
    lines = INTERNAL_BLOCK_SAMPLE.splitlines()
    code = markers.code_only_lines(lines)
    dead = {
        index
        for start, stop in markers._find_if_zero_regions(code, 0, len(lines), label="sample.h")
        for index in range(start, stop + 1)
    }
    for number, text in markers.scrub_text(INTERNAL_BLOCK_SAMPLE, label="sample.h").removed:
        index = number - 1
        assert (
            markers.MARKER_RE.search(text)
            or not text.strip()
            or not markers.line_is_live(code[index])
            or index in dead
        ), f"scrub removed a live line: {text!r}"


def test_scrub_leaves_an_if_zero_region_with_a_live_else_branch(markers) -> None:
    text = (
        "// #### INTERNAL BEGIN ####\n"
        "#if 0\n"
        "extern uint32_t am_sample_dead(void);\n"
        "#else\n"
        "extern uint32_t am_sample_live(void);\n"
        "#endif\n"
        "// #### INTERNAL END ####\n"
    )
    scrubbed = markers.scrub_text(text, label="sample.h").text
    assert scrubbed == text


def test_scrub_refuses_unbalanced_markers(markers) -> None:
    with pytest.raises(markers.InternalMarkerError):
        markers.scrub_text("// #### INTERNAL BEGIN ####\n// note\n", label="sample.h")
    with pytest.raises(markers.InternalMarkerError):
        markers.scrub_text("// #### INTERNAL END ####\n", label="sample.h")


def test_scrub_tree_writes_nothing_when_any_file_refuses(tmp_path: Path, markers) -> None:
    """Two-phase apply. A half-scrubbed tree is neither the vendor drop nor a
    sanitized payload, and nothing records how far the pass got."""
    good = tmp_path / "am_hal_good.h"
    good.write_text(
        "// #### INTERNAL BEGIN ####\n// ticket chatter\n// #### INTERNAL END ####\nextern int am_x(void);\n",
        encoding="utf-8",
    )
    bad = tmp_path / "am_hal_zz_broken.h"
    bad.write_text("// #### INTERNAL END ####\n", encoding="utf-8")
    good_before, bad_before = good.read_bytes(), bad.read_bytes()

    with pytest.raises(markers.InternalMarkerError):
        markers.scrub_tree(tmp_path, write=True)

    assert good.read_bytes() == good_before, "a refusal must abort before anything is written"
    assert bad.read_bytes() == bad_before


def test_scrub_reproduces_its_output_byte_for_byte(tmp_path: Path, markers) -> None:
    """Tool-reproducibility: the sanitized payload is a pure function of the
    vendor drop and this tool, never a hand edit. Running the scrub over PRE
    must yield POST exactly, and re-running it must change nothing."""
    tree = tmp_path / "sdk"
    tree.mkdir()
    (tree / "am_hal_sample.h").write_text(VENDOR_SAMPLE_PRE, encoding="utf-8")

    markers.scrub_tree(tree, write=True)
    assert (tree / "am_hal_sample.h").read_text(encoding="utf-8") == VENDOR_SAMPLE_POST
    assert markers.scrub_tree(tree, write=False) == {}


def test_scrub_of_the_promoted_atomiq110_tree_is_a_no_op(repo_root: Path, markers) -> None:
    """The committed tree is the scrubber's own fixed point, so a future intake
    re-running the sanitizer cannot churn the payload."""
    assert markers.scrub_tree(repo_root / ATOMIQ110_TREE, write=False) == {}


# --------------------------------------------------------------------------
# Scan and intake gate
# --------------------------------------------------------------------------
def test_scan_flags_a_reinjected_marker(tmp_path: Path, markers) -> None:
    header = tmp_path / "mcu" / "atomiq110" / "hal" / "am_hal_reinjected.h"
    header.parent.mkdir(parents=True)
    header.write_text("// #### INTERNAL BEGIN ####\n// note\n// #### INTERNAL END ####\n", encoding="utf-8")

    hits = markers.scan_tree(tmp_path, path_prefix="modules/nsx-ambiqsuite/sdk")
    assert [hit.line for hit in hits] == [1, 3]
    assert hits[0].path == "modules/nsx-ambiqsuite/sdk/mcu/atomiq110/hal/am_hal_reinjected.h"

    comparison = markers.compare_to_baseline(markers.marker_counts(hits), markers.load_baseline())
    assert not comparison.ok
    assert comparison.regressions[0][1:] == (2, 0)


def test_scan_flags_growth_in_an_already_baselined_file(tmp_path: Path, markers) -> None:
    """The retained atomiq110 fences are pinned, not merely tolerated: adding a
    block back to a scrubbed file is still a regression."""
    header = tmp_path / "hal" / "am_hal_dcu.h"
    header.parent.mkdir(parents=True)
    header.write_text("// #### INTERNAL BEGIN ####\n// note\n// #### INTERNAL END ####\n" * 9, encoding="utf-8")
    counts = markers.marker_counts(markers.scan_tree(tmp_path, path_prefix=ATOMIQ110_PREFIX))
    comparison = markers.compare_to_baseline(counts, markers.load_baseline())
    assert not comparison.ok
    assert comparison.regressions[0] == (f"{ATOMIQ110_PREFIX}/hal/am_hal_dcu.h", 18, 8)


def test_staging_gate_rejects_new_internal_content(tmp_path: Path, workflow) -> None:
    staged = tmp_path / "sdk"
    (staged / "mcu" / "atomiq110").mkdir(parents=True)
    (staged / "mcu" / "atomiq110" / "am_hal_new.h").write_text(
        "// #### INTERNAL BEGIN ####\n// unreleased silicon note\n// #### INTERNAL END ####\n",
        encoding="utf-8",
    )
    with pytest.raises(workflow.IntakeVerificationError) as excinfo:
        workflow.assert_no_new_internal_markers(staged, path_prefix="modules/nsx-ambiqsuite/sdk")
    message = str(excinfo.value)
    assert "mcu/atomiq110/am_hal_new.h" in message
    assert "scrub" in message


def test_staging_gate_accepts_the_promoted_tree(repo_root: Path, workflow) -> None:
    promoted = repo_root / "modules" / "nsx-ambiqsuite" / "sdk"
    assert workflow.assert_no_new_internal_markers(promoted, path_prefix="modules/nsx-ambiqsuite/sdk") > 0
