#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import os
import re
import shutil
import subprocess
import sys
import zipfile
from dataclasses import dataclass, replace
from pathlib import Path


# Clang-family (ATfE/ACfE) FPU spellings. GCC derives CPU/FPU/float-abi from the
# AmbiqSuite-generated per-part makefile, so these only apply to clang toolchains.
DEFAULT_M55_FPU = "fp-armv8-fullfp16-sp-d16"   # Cortex-M55 (Apollo5/510 family)
DEFAULT_M4F_FPU = "fpv4-sp-d16"                 # Cortex-M4F (Apollo2-4 families)


@dataclass(frozen=True)
class PartBuild:
    name: str
    clang_fpu: str = DEFAULT_M55_FPU
    # CMSIS startup source basename to promote. Defaults to system_<name>.c.
    # Override when a part's system source is named differently upstream.
    system_source: str | None = None
    # When the upstream CMSIS startup source for this part is absent, synthesize
    # it by copying this sibling source and applying system_synth_subs. Used for
    # parts (e.g. apollo4l) that historically shipped a startup file only in ZIP
    # drops; the body is identical to the donor save for a couple of includes.
    system_synth_from: str | None = None
    system_synth_subs: tuple[tuple[str, str], ...] = ()
    # Toolchains this part is built for. Defaults to the full promoted set. Narrow
    # it for parts that a toolchain cannot target (e.g. Apollo2 predates ACfE, so
    # it ships gcc+atfe only). The provider train's toolchains form the union; this
    # filters which (part, toolchain) pairs are actually built and published.
    toolchains: tuple[str, ...] = ()


@dataclass(frozen=True)
class BoardBuild:
    name: str
    part: str
    # Strip the unconditional `am_devices_display_generic.h` include from the
    # worktree am_bsp.h before building. Required for boards whose part lacks the
    # NemaGFX display controller (e.g. Apollo4 "blue" boards) where that header
    # transitively pulls the apollo5-only nema_dc.h. The display code it guards is
    # behind DISP_CTRL_IP (undefined by default), so removing the include is safe.
    strip_display_include: bool = False
    # Inject a compatible `am_hal_gpio_pincfg_t` typedef into the worktree am_bsp.h
    # before building. Required for legacy parts (Apollo2) whose HAL predates that
    # Apollo3+ GPIO type while stable's shared device headers (am_devices_button.h)
    # now reference it. The BSP only instantiates LED/button tables, never the
    # pin-cfg button APIs that use the type, so the alias is build-only.
    inject_gpio_pincfg_shim: bool = False


@dataclass(frozen=True)
class TrainSpec:
    train_id: str
    provider_id: str
    module_dir: str
    # Human-facing version label. None means derive a snapshot tag at build time
    # from the resolved source ("{source_ref}-{YYYY.MM.DD}"), used for trains built
    # off a rolling branch where a fixed SDK release number would be misleading.
    version: str | None
    parts: tuple[PartBuild, ...]
    boards: tuple[BoardBuild, ...]
    toolchains: tuple[str, ...]
    omitted_device_headers: tuple[str, ...]
    omitted_part_headers: tuple[str, ...]
    display_bsp_headers: tuple[str, ...]


@dataclass(frozen=True)
class ToolchainProfile:
    name: str
    config: str
    compiler: str
    archive_tool: str
    make_vars: tuple[tuple[str, str], ...]
    required_tools: tuple[str, ...] = ()

PROMOTED_TOOLCHAINS = ("gcc", "atfe", "acfe")
PROMOTED_FILE_SUFFIXES = (".h", ".hpp", ".inc")
BANNER_TEXT_SUFFIXES = (".h", ".hpp", ".inc", ".c")
# Device drivers ship both headers and source so consumers can compile the
# peripheral support they need; HAL/BSP remain precompiled.
PROMOTED_DEVICE_SUFFIXES = (".h", ".hpp", ".inc", ".c")
PROMOTED_UTILITY_SOURCES = (
    "am_util_stdio.c",
    "am_util_delay.c",
    "am_util_pmu.c",
)

OMITTED_DEVICE_HEADERS = (
    "am_devices_510L_radio.h",
    "am_devices_display_generic.h",
    "am_devices_dc_dbi_novatek.h",
    "am_devices_dc_dpi_japandisplayinc.h",
    "am_devices_dc_dsi_forcelead.h",
    "am_devices_dc_dsi_novatek.h",
    "am_devices_dc_dsi_raydium.h",
    "am_devices_dc_jdi_sharp.h",
    "am_devices_dc_xspi_raydium.h",
)

DISPLAY_BSP_HEADERS = (
    "apollo510_evb",
    "apollo510b_evb",
    "apollo510dL_evb",
)

# CMSIS device register-map headers for parts that are not publicly released.
# The upstream SDK drop ships register maps for early-silicon and unannounced
# parts; `promote_vendor_headers` copies the whole CMSIS/Include directory, so
# these are pruned here to keep the public provider payload limited to released
# parts. apollo5a/apollo5b are pre-production Apollo5 silicon superseded by the
# released Apollo510 family; bronco is an unannounced part. atomiq110 (AT110) is
# promoted: it is staged as the FPGA part with its own HAL/BSP archives (upstream
# mcu/atomiq110/am_mcu_apollo.h hardcodes ATOMIQ11X_FPGA, so the HAL builds in
# FPGA mode by default).
OMITTED_PART_HEADERS = (
    "apollo5a.h",
    "apollo5a_generic.h",
    "system_apollo5a.h",
    "bronco.h",
    "bronco_generic.h",
    "system_bronco.h",
)

# Single unified provider descriptor. Mirrors upstream `stable`: one
# `nsx-ambiqsuite` provider module covers every released Apollo-class part/board.
# Built from a single AmbiqSuite ref, so every part is regenerated at the same SDK
# version rather than pinned to a historical release. Per-part toolchain narrowing
# (e.g. Apollo2 ships gcc+atfe only) is expressed on each PartBuild; the train
# toolchains below are the union across all parts.
TRAINS: dict[str, TrainSpec] = {
    "stable": TrainSpec(
        train_id="stable",
        provider_id="ambiqsuite",
        module_dir="nsx-ambiqsuite",
        version=None,  # derived snapshot tag, e.g. stable-2026.06.17
        parts=(
            PartBuild("apollo330P"),
            PartBuild("apollo510"),
            PartBuild("apollo510L"),
            PartBuild(
                "apollo4l",
                clang_fpu=DEFAULT_M4F_FPU,
                system_synth_from="system_apollo4.c",
                system_synth_subs=(
                    ("//! @file system_apollo4.c", "//! @file system_apollo4l.c"),
                    (
                        "//! @brief Ambiq Micro Apollo4 MCU specific functions.",
                        "//! @brief Ambiq Micro Apollo4 Lite MCU specific functions.",
                    ),
                    ('#include "system_apollo4.h"', '#include "system_apollo4l.h"'),
                    ('#include "apollo4.h"', '#include "apollo4l.h"'),
                ),
            ),
            PartBuild("apollo4p", clang_fpu=DEFAULT_M4F_FPU),
            PartBuild("apollo3", clang_fpu=DEFAULT_M4F_FPU),
            PartBuild("apollo3p", clang_fpu=DEFAULT_M4F_FPU),
            # Apollo2 predates ACfE; build it for gcc+atfe only.
            PartBuild("apollo2", clang_fpu=DEFAULT_M4F_FPU, toolchains=("gcc", "atfe")),
            # AT110 (Atomiq), Cortex-M55. Realized today only as an FPGA; upstream
            # mcu/atomiq110/am_mcu_apollo.h hardcodes ATOMIQ11X_FPGA, so the HAL is
            # built in FPGA mode with no extra flags.
            PartBuild("atomiq110"),
        ),
        boards=(
            BoardBuild("apollo330mP_evb", "apollo330P"),
            BoardBuild("apollo510_evb", "apollo510"),
            BoardBuild("apollo510b_evb", "apollo510"),
            BoardBuild("apollo510dL_evb", "apollo510L"),
            BoardBuild("apollo4l_evb", "apollo4l"),
            BoardBuild("apollo4l_blue_evb", "apollo4l", strip_display_include=True),
            BoardBuild("apollo4p_evb", "apollo4p"),
            BoardBuild("apollo4p_blue_kbr_evb", "apollo4p", strip_display_include=True),
            BoardBuild("apollo4p_blue_kxr_evb", "apollo4p", strip_display_include=True),
            BoardBuild("apollo3_evb", "apollo3"),
            BoardBuild("apollo3_evb_cygnus", "apollo3"),
            BoardBuild("apollo3p_evb", "apollo3p"),
            BoardBuild("apollo3p_evb_cygnus", "apollo3p"),
            BoardBuild("apollo2_evb", "apollo2", inject_gpio_pincfg_shim=True),
            # AT110 ships only as the FPGA turbo board today.
            BoardBuild("atomiq110_fpga_turbo", "atomiq110"),
        ),
        toolchains=PROMOTED_TOOLCHAINS,
        omitted_device_headers=OMITTED_DEVICE_HEADERS,
        omitted_part_headers=OMITTED_PART_HEADERS,
        display_bsp_headers=DISPLAY_BSP_HEADERS,
    ),
}

ATFE_WARNING_SUPPRESSIONS = (
    "-Wno-unused-command-line-argument",
    "-Wno-c11-extensions",
    "-Wno-c23-extensions",
    "-Wno-gnu-zero-variadic-macro-arguments",
    "-Wno-strict-prototypes",
)

ACFE_WARNING_SUPPRESSIONS = (
    "-Wno-c11-extensions",
    "-Wno-c23-extensions",
    "-Wno-pedantic",
)

ACFE_COMPAT_DEFINES = (
    # Modern ARMClang defines __ARMCC_VERSION but not the legacy armcc __MODULE__
    # builtin that older HALs (Apollo2) reference for debug filenames. Map it to
    # __FILE__ so those parts compile; inert for parts that never use the token.
    "-D__MODULE__=__FILE__",
)

RELEASE_CFLAGS = (
    "-g0",
)

DEFAULT_ATFE_TARGET = "thumbv8.1m.main-unknown-none-eabihf"
DEFAULT_ACFE_TARGET = "arm-arm-none-eabi"
DEFAULT_SOURCE_REF = "stable"
# Submodules the HAL/BSP build compiles against. `git worktree add` does not
# populate submodules, so these are initialized explicitly in the worktree.
# NemaGFX provides nema_dc.h, required by the apollo510L DSI HAL (am_hal_dsi.c).
REQUIRED_SUBMODULES = ("third_party/ThinkSi/NemaGFX_SDK",)
REVISION_PATTERN = re.compile(r"revision\s+([^\s]+)\s+of the AmbiqSuite")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def intake_local_root() -> Path:
    return repo_root() / "sdk-intake" / "local"


def default_zip_path(version: str) -> Path:
    return intake_local_root() / "drops" / f"AmbiqSuite_{version}.zip"


def default_extract_dir(zip_path: Path) -> Path:
    # Derive the extraction directory from the drop's filename so it is stable and
    # unique per zip. The resolved build version is not known at this point (trains
    # default to a snapshot tag derived later), so it cannot name this directory.
    return intake_local_root() / "work" / zip_path.stem


def default_git_worktree_dir(source_ref: str) -> Path:
    safe_ref = re.sub(r"[^A-Za-z0-9_.-]+", "-", source_ref).strip("-") or "ambiqsuite"
    return intake_local_root() / "work" / f"ambiqsuite-{safe_ref}"


def artifact_root(train: TrainSpec, version: str) -> Path:
    return repo_root() / "artifacts" / "ambiqsuite" / train.train_id / version


def provider_sdk_root(train: TrainSpec) -> Path:
    return repo_root() / "modules" / train.module_dir / "sdk"


def part_toolchains(train: TrainSpec, part: PartBuild) -> tuple[str, ...]:
    """Toolchains a part is built for: its own narrowing if set, else the train
    union. Lets a single unified train carry parts a toolchain cannot target
    (e.g. Apollo2 ships gcc+atfe only) without dropping it from the others."""
    return part.toolchains or train.toolchains


def board_toolchains(train: TrainSpec, board: BoardBuild) -> tuple[str, ...]:
    """Toolchains a board's BSP is built for, inherited from its part."""
    for part in train.parts:
        if part.name == board.part:
            return part_toolchains(train, part)
    return train.toolchains


def display_path(path: Path) -> str:
    try:
        return path.relative_to(repo_root()).as_posix()
    except ValueError:
        return path.as_posix()


def find_sdk_root(extract_dir: Path) -> Path:
    candidates = [extract_dir]
    candidates.extend(path for path in extract_dir.rglob("*") if path.is_dir())
    for candidate in candidates:
        if (candidate / "mcu").is_dir() and (candidate / "boards").is_dir():
            return candidate
    raise FileNotFoundError(f"could not find AmbiqSuite root under {extract_dir}")


def extract_sdk(zip_path: Path, extract_dir: Path, *, force: bool) -> Path:
    if force and extract_dir.exists():
        shutil.rmtree(extract_dir)
    if not extract_dir.exists():
        extract_dir.mkdir(parents=True)
        with zipfile.ZipFile(zip_path) as archive:
            archive.extractall(extract_dir)
    return find_sdk_root(extract_dir)


def git_commit(repo: Path, ref: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), "rev-parse", "--verify", f"{ref}^{{commit}}"],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return result.stdout.strip()


def git_commit_date(repo: Path, commit: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), "show", "-s", "--date=format:%Y.%m.%d", "--format=%cd", commit],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return result.stdout.strip()


def ensure_worktree_submodules(worktree_dir: Path) -> None:
    # `git worktree add` does not populate submodules; the HAL/BSP build compiles
    # against headers from REQUIRED_SUBMODULES (e.g. NemaGFX's nema_dc.h for the
    # apollo510L DSI HAL). `submodule update --init` is idempotent, so this is a
    # no-op on a worktree where the submodule is already checked out.
    for submodule in REQUIRED_SUBMODULES:
        subprocess.run(
            ["git", "-C", str(worktree_dir), "submodule", "update", "--init", submodule],
            check=True,
        )


def materialize_git_ref(repo: Path, source_ref: str, worktree_dir: Path, *, force: bool) -> Path:
    repo = repo.expanduser().resolve()
    commit = git_commit(repo, source_ref)
    worktree_dir = worktree_dir.expanduser().resolve()
    if force and worktree_dir.exists():
        subprocess.run(["git", "-C", str(repo), "worktree", "remove", "--force", str(worktree_dir)], check=False)
        shutil.rmtree(worktree_dir, ignore_errors=True)
    if not worktree_dir.exists():
        worktree_dir.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "-C", str(repo), "worktree", "add", "--detach", str(worktree_dir), commit], check=True)
    ensure_worktree_submodules(worktree_dir)
    return find_sdk_root(worktree_dir)


def resolve_source_root(args: argparse.Namespace) -> tuple[Path, str, str | None, str | None]:
    if args.source_root is not None:
        return find_sdk_root(args.source_root.expanduser().resolve()), "source_root", None, None
    if args.zip_path is not None:
        zip_path = args.zip_path.expanduser().resolve()
        require_file(zip_path)
        extract_dir = (args.extract_dir or default_extract_dir(zip_path)).expanduser().resolve()
        return extract_sdk(zip_path, extract_dir, force=args.force_extract), "zip", str(zip_path), None
    # Default source: materialize a git ref (rolling stable branch by default).
    source_ref = args.source_ref or DEFAULT_SOURCE_REF
    repo_value = args.ambiqsuite_repo or os.environ.get("AMBIQSUITE_REPO")
    if repo_value is None:
        raise ValueError(
            "missing AmbiqSuite source: pass --source-root, --zip, or --ambiqsuite-repo "
            "(or set AMBIQSUITE_REPO for the git-ref workflow)"
        )
    repo = Path(repo_value).expanduser().resolve()
    source_commit = git_commit(repo, source_ref)
    worktree_dir = args.source_worktree_dir or default_git_worktree_dir(source_ref)
    sdk_root = materialize_git_ref(repo, source_ref, worktree_dir, force=args.force_source_ref)
    return sdk_root, "git_ref", source_ref, source_commit


def required_toolchain_root(value: str | None, *, cli_name: str, env_name: str, toolchain_name: str) -> Path:
    if value is None:
        raise ValueError(
            f"missing {toolchain_name} root: pass {cli_name} or set {env_name}"
        )
    return Path(value).expanduser().resolve()


def find_make_build_dir(source_root: Path, *relative_candidates: Path) -> Path:
    for relative in relative_candidates:
        candidate = source_root / relative
        if (candidate / "Makefile").is_file():
            return candidate
    raise FileNotFoundError("could not find Makefile in any of: " + ", ".join(path.as_posix() for path in relative_candidates))


def find_buildproj_script(generator_dir: Path) -> Path:
    for root in (generator_dir, *generator_dir.parents):
        script = root / "scripts" / "buildproj" / "buildproj.py"
        if script.is_file():
            return script
    raise FileNotFoundError(f"could not find scripts/buildproj/buildproj.py above {generator_dir}")


def ensure_generated_makefile(generated_makefile: Path, generator_dir: Path) -> None:
    if generated_makefile.is_file():
        return
    # Generate only the gcc Makefile (-g): it is the sole artifact we consume, and
    # older parts (apollo2) lack segger templates that buildproj would otherwise try
    # to emit, crashing the run.
    subprocess.run(
        [sys.executable, str(find_buildproj_script(generator_dir)), "-g"],
        cwd=generator_dir,
        check=True,
    )
    require_file(generated_makefile)


def hal_build_base(sdk_root: Path, part: PartBuild) -> Path:
    # Newer parts (apollo4/5) split the HAL build under hal/mcu (alongside hal/dsp);
    # older parts (apollo2/3) build directly in hal/. Both drive buildproj from a
    # config.ini in that directory, so detect which layout this part uses.
    hal = sdk_root / "mcu" / part.name / "hal"
    mcu = hal / "mcu"
    return mcu if (mcu / "config.ini").is_file() else hal


def ensure_hal_pin_header(hal_base: Path, sdk_root: Path) -> None:
    # Older parts (apollo2/3) generate hal/am_hal_pin.h from apollo-pinout.csv via a
    # Makefile target (am_mcu_apollo.h includes it). Running buildproj.py directly
    # does not trigger that target, so invoke it explicitly when the csv is present.
    if not (hal_base / "apollo-pinout.csv").is_file():
        return
    if (hal_base / "am_hal_pin.h").is_file():
        return
    subprocess.run(["make", "am_hal_pin.h", f"SWROOT={sdk_root}"], cwd=hal_base, check=True)
    require_file(hal_base / "am_hal_pin.h")


def ensure_hal_generated_sources(sdk_root: Path, part: PartBuild) -> None:
    hal_dir = sdk_root / "mcu" / part.name / "hal"
    if (hal_dir / "am_hal_status.h").is_file():
        return
    makefile = hal_dir / "mcu" / "Makefile"
    if not makefile.is_file():
        # Older parts (apollo2/3) build directly under hal/ and have no copyhal
        # step or generated am_hal_status.h; nothing to pre-generate.
        return
    subprocess.run(["make", "copyhal", f"SWROOT={sdk_root}"], cwd=hal_dir / "mcu", check=True)
    require_file(hal_dir / "am_hal_status.h")


def ensure_bsp_pins(sdk_root: Path, bsp_dir: Path) -> None:
    source = bsp_dir / "bsp_pins.src"
    if not source.is_file():
        return
    generator = sdk_root / "tools" / "bsp_generator" / "pinconfig.py"
    require_file(generator)
    if not (bsp_dir / "am_bsp_pins.c").is_file():
        with (bsp_dir / "am_bsp_pins.c").open("w", encoding="utf-8") as output:
            subprocess.run([sys.executable, str(generator), "bsp_pins.src", "C"], cwd=bsp_dir, stdout=output, check=True)
    if not (bsp_dir / "am_bsp_pins.h").is_file():
        with (bsp_dir / "am_bsp_pins.h").open("w", encoding="utf-8") as output:
            subprocess.run([sys.executable, str(generator), "bsp_pins.src", "H"], cwd=bsp_dir, stdout=output, check=True)


def ensure_bsp_gpio_header(sdk_root: Path, bsp_dir: Path) -> None:
    # Legacy boards (apollo2) describe pins in apollo-pinout.csv and generate
    # am_bsp_gpio.h via a Makefile target (bsp_gen.py); buildproj.py does not emit it.
    if not (bsp_dir / "apollo-pinout.csv").is_file():
        return
    if (bsp_dir / "am_bsp_gpio.h").is_file():
        return
    subprocess.run(["make", "am_bsp_gpio.h", f"SWROOT={sdk_root}"], cwd=bsp_dir, check=True)
    require_file(bsp_dir / "am_bsp_gpio.h")


def built_artifact(build_dir: Path, profile: ToolchainProfile, filename: str) -> Path:
    candidates = (
        build_dir / profile.config / filename,
        build_dir / "gcc" / profile.config / filename,
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("could not find built artifact in any of: " + ", ".join(path.as_posix() for path in candidates))


def find_upstream_revision(sdk_root: Path) -> str | None:
    for path in (
        sdk_root / "mcu" / "apollo510" / "hal" / "mcu" / "gcc" / "Makefile",
        sdk_root / "Makefile",
    ):
        if not path.is_file():
            continue
        match = REVISION_PATTERN.search(path.read_text(encoding="utf-8", errors="ignore"))
        if match and "{" not in match.group(1) and "}" not in match.group(1):
            return match.group(1)
    return None


def require_file(path: Path) -> None:
    if not path.is_file():
        raise FileNotFoundError(path)


def require_tool(path_or_name: str) -> None:
    command = path_or_name.split()[0]
    if os.sep in command:
        if not Path(command).is_file():
            raise FileNotFoundError(command)
        return
    if shutil.which(command) is None:
        raise FileNotFoundError(command)


def extra_cflags(*groups: tuple[str, ...], debug_symbols: bool) -> str:
    flags: list[str] = []
    if not debug_symbols:
        flags.extend(RELEASE_CFLAGS)
    for group in groups:
        flags.extend(group)
    return " ".join(flags)


def ensure_acfe_armar_wrapper() -> Path:
    wrapper = intake_local_root() / "tools" / "acfe_armar_wrapper.py"
    wrapper.parent.mkdir(parents=True, exist_ok=True)
    wrapper.write_text(
        """#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys


GNU_AR_FLAG_CHARS = set("dmpqrtxabcuUsSvV")


def is_archive(path: str) -> bool:
    return path.endswith((".a", ".lib"))


def is_compact_gnu_ar_flags(arg: str) -> bool:
    return bool(arg) and set(arg) <= GNU_AR_FLAG_CHARS and any(action in arg for action in "dmpqrtx")


def normalize_args(args: list[str]) -> list[str]:
    archive = None
    members = []
    index = 0
    while index < len(args):
        arg = args[index]
        if arg == "-o" and index + 1 < len(args):
            archive = args[index + 1]
            index += 2
            continue
        if arg in {"-r", "-c", "--create", "-s"} or is_compact_gnu_ar_flags(arg.lstrip("-")):
            index += 1
            continue
        if is_archive(arg):
            archive = arg
        else:
            members.append(arg)
        index += 1
    if archive is None:
        return args
    return ["-r", "--create", archive, *members]


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: acfe_armar_wrapper.py <armar> [args...]", file=sys.stderr)
        return 2
    return subprocess.call([sys.argv[1], *normalize_args(sys.argv[2:])])


if __name__ == "__main__":
    raise SystemExit(main())
""",
        encoding="utf-8",
    )
    wrapper.chmod(0o755)
    return wrapper


def toolchain_profile(args: argparse.Namespace, toolchain: str) -> ToolchainProfile:
    if toolchain == "gcc":
        return ToolchainProfile(
            name="gcc",
            config="bin",
            compiler=f"{args.gcc_prefix}-gcc",
            archive_tool=f"{args.gcc_prefix}-ar",
            make_vars=(
                ("TOOLCHAIN", args.gcc_prefix),
                ("CONFIG", "bin"),
                ("EXTRA_CFLAGS", extra_cflags(debug_symbols=args.debug_symbols)),
            ),
        )

    if toolchain == "atfe":
        root = required_toolchain_root(
            args.atfe_root,
            cli_name="--atfe-root",
            env_name="ATFE_ROOT",
            toolchain_name="ATfE",
        )
        clang = root / "bin" / "clang"
        llvm_ar = root / "bin" / "llvm-ar"
        return ToolchainProfile(
            name="atfe",
            config="bin-atfe",
            compiler=str(clang),
            archive_tool=str(llvm_ar),
            make_vars=(
                ("CONFIG", "bin-atfe"),
                ("CC", f"{clang} --target={args.atfe_target}"),
                ("GCC", f"{clang} --target={args.atfe_target}"),
                ("AR", str(llvm_ar)),
                ("EXTRA_CFLAGS", extra_cflags(ATFE_WARNING_SUPPRESSIONS, debug_symbols=args.debug_symbols)),
            ),
        )

    if toolchain == "acfe":
        root = required_toolchain_root(
            args.acfe_root,
            cli_name="--acfe-root",
            env_name="ACFE_ROOT",
            toolchain_name="ACfE",
        )
        armclang = root / "bin" / "armclang"
        armar = root / "bin" / "armar"
        armlink = root / "bin" / "armlink"
        fromelf = root / "bin" / "fromelf"
        armar_wrapper = ensure_acfe_armar_wrapper()
        return ToolchainProfile(
            name="acfe",
            config="bin-acfe",
            compiler=str(armclang),
            archive_tool=str(armar),
            make_vars=(
                ("CONFIG", "bin-acfe"),
                ("CC", f"{armclang} --target={args.acfe_target}"),
                ("GCC", f"{armclang} --target={args.acfe_target}"),
                ("LD", str(armlink)),
                ("CP", str(fromelf)),
                ("OD", str(fromelf)),
                ("RD", str(fromelf)),
                ("AR", f"{sys.executable} {armar_wrapper} {armar}"),
                ("SIZE", str(fromelf)),
                ("EXTRA_CFLAGS", extra_cflags(ACFE_WARNING_SUPPRESSIONS, ACFE_COMPAT_DEFINES, debug_symbols=args.debug_symbols)),
            ),
            required_tools=(str(armlink), str(fromelf)),
        )

    raise ValueError(toolchain)


def optional_toolchain_profile(args: argparse.Namespace, toolchain: str) -> ToolchainProfile | None:
    try:
        return toolchain_profile(args, toolchain)
    except ValueError:
        return None


def run_make(build_dir: Path, profile: ToolchainProfile, *, verbose: bool, extra_vars: tuple[tuple[str, str], ...] = ()) -> None:
    makefile = build_dir / "Makefile"
    require_file(makefile)
    shutil.rmtree(build_dir / profile.config, ignore_errors=True)
    shutil.rmtree(build_dir / "gcc" / profile.config, ignore_errors=True)
    command = ["make"]
    if verbose:
        command.append("VERBOSE=1")
    command.extend(f"{key}={value}" for key, value in (*profile.make_vars, *extra_vars))
    subprocess.run(command, cwd=build_dir, check=True)


def placeholder_year(version: str) -> str:
    match = re.search(r"(20\d{2})", version)
    if match:
        return match.group(1)
    return str(dt.date.today().year)


def normalize_banner_placeholders(text: str, version: str) -> str:
    year = placeholder_year(version)
    text = text.replace(
        "${copyright}",
        f"Copyright (c) {year}, Ambiq Micro, Inc.\n// All rights reserved.",
    )
    return text.replace("${version}", version)


def copy_file(source: Path, destination: Path, *, banner_version: str | None = None) -> None:
    require_file(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if banner_version and source.suffix in BANNER_TEXT_SUFFIXES:
        destination.write_text(
            normalize_banner_placeholders(source.read_text(encoding="utf-8"), banner_version),
            encoding="utf-8",
        )
        return
    shutil.copy2(source, destination)


def copy_matching_files(
    source_root: Path,
    destination_root: Path,
    suffixes: tuple[str, ...],
    *,
    banner_version: str | None = None,
) -> None:
    if not source_root.is_dir():
        raise FileNotFoundError(source_root)
    for source in sorted(source_root.rglob("*")):
        if not source.is_file() or source.name.startswith("."):
            continue
        if suffixes and source.suffix not in suffixes:
            continue
        destination = destination_root / source.relative_to(source_root)
        copy_file(source, destination, banner_version=banner_version)


def promote_part_headers(train: TrainSpec, version: str, sdk_root: Path, part: PartBuild) -> None:
    ensure_hal_generated_sources(sdk_root, part)
    hal_base = hal_build_base(sdk_root, part)
    ensure_hal_pin_header(hal_base, sdk_root)
    ensure_generated_makefile(hal_base / "gcc" / "Makefile", hal_base)
    destination = provider_sdk_root(train) / "mcu" / part.name
    shutil.rmtree(destination, ignore_errors=True)
    copy_matching_files(sdk_root / "mcu" / part.name, destination, PROMOTED_FILE_SUFFIXES, banner_version=version)


def promote_board_headers(train: TrainSpec, version: str, sdk_root: Path, board: BoardBuild) -> None:
    bsp_dir = sdk_root / "boards" / board.name / "bsp"
    ensure_bsp_pins(sdk_root, bsp_dir)
    ensure_generated_makefile(bsp_dir / "gcc" / "Makefile", bsp_dir)
    destination = provider_sdk_root(train) / "boards" / board.name / "bsp"
    shutil.rmtree(destination, ignore_errors=True)
    copy_matching_files(bsp_dir, destination, PROMOTED_FILE_SUFFIXES, banner_version=version)


def promote_vendor_headers(train: TrainSpec, version: str, sdk_root: Path) -> None:
    copy_matching_files(
        sdk_root / "CMSIS" / "AmbiqMicro" / "Include",
        provider_sdk_root(train) / "CMSIS" / "AmbiqMicro" / "Include",
        PROMOTED_FILE_SUFFIXES,
        banner_version=version,
    )
    copy_matching_files(
        sdk_root / "devices",
        provider_sdk_root(train) / "devices",
        PROMOTED_DEVICE_SUFFIXES,
        banner_version=version,
    )
    copy_matching_files(
        sdk_root / "utils",
        provider_sdk_root(train) / "utils",
        PROMOTED_FILE_SUFFIXES,
        banner_version=version,
    )
    am_util_id_header = provider_sdk_root(train) / "utils" / "am_util_id.h"
    if am_util_id_header.is_file():
        am_util_id_header.unlink()
    am_util_header = provider_sdk_root(train) / "utils" / "am_util.h"
    if am_util_header.is_file():
        am_util_header.write_text(
            am_util_header.read_text(encoding="utf-8").replace('#include "am_util_id.h"\n', ""),
            encoding="utf-8",
        )
    am_util_multi_boot_private = provider_sdk_root(train) / "utils" / "am_util_multi_boot_private.h"
    if am_util_multi_boot_private.is_file():
        am_util_multi_boot_private.write_text(
            am_util_multi_boot_private.read_text(encoding="utf-8").replace('#include "am_util_multi_boot_secure.h"\n', ""),
            encoding="utf-8",
        )
    copy_file(
        sdk_root / "mcu" / "am_sdk_version.h",
        provider_sdk_root(train) / "mcu" / "am_sdk_version.h",
        banner_version=version,
    )


def scrub_promoted_headers(train: TrainSpec) -> None:
    include_root = provider_sdk_root(train) / "CMSIS" / "AmbiqMicro" / "Include"
    for name in train.omitted_part_headers:
        path = include_root / name
        if path.is_file():
            path.unlink()
    devices_root = provider_sdk_root(train) / "devices"
    for name in train.omitted_device_headers:
        path = devices_root / name
        if path.is_file():
            path.unlink()
        source_path = path.with_suffix(".c")
        if source_path.is_file():
            source_path.unlink()
    for board_name in train.display_bsp_headers:
        board_header = provider_sdk_root(train) / "boards" / board_name / "bsp" / "am_bsp.h"
        if not board_header.is_file():
            continue
        board_text = board_header.read_text(encoding="utf-8")
        board_text = board_text.replace('#include "am_devices_display_generic.h"\n', "")
        board_text = re.sub(
            r'\n#if defined \(DISP_CTRL_IP\).*?#endif // DISP_CTRL_IP\n',
            "\n",
            board_text,
            count=1,
            flags=re.S,
        )
        board_header.write_text(board_text, encoding="utf-8")


def promote_system_sources(train: TrainSpec, version: str, sdk_root: Path) -> None:
    source_root = sdk_root / "CMSIS" / "AmbiqMicro" / "Source"
    destination_root = provider_sdk_root(train) / "CMSIS" / "AmbiqMicro" / "Source"
    destination_root.mkdir(parents=True, exist_ok=True)
    for part in train.parts:
        target_name = part.system_source or f"system_{part.name}.c"
        source = source_root / target_name
        if not source.is_file() and part.system_synth_from:
            donor = source_root / part.system_synth_from
            text = donor.read_text(encoding="utf-8")
            for old, new in part.system_synth_subs:
                text = text.replace(old, new)
            (destination_root / target_name).write_text(normalize_banner_placeholders(text, version), encoding="utf-8")
        else:
            copy_file(source, destination_root / source.name, banner_version=version)


def promote_utility_sources(train: TrainSpec, version: str, sdk_root: Path) -> None:
    for source_name in PROMOTED_UTILITY_SOURCES:
        copy_file(
            sdk_root / "utils" / source_name,
            provider_sdk_root(train) / "src" / source_name,
            banner_version=version,
        )


def promote_license_docs(train: TrainSpec, sdk_root: Path) -> None:
    shutil.copytree(sdk_root / "docs" / "licenses", provider_sdk_root(train) / "docs" / "licenses")
    license_pdf = provider_sdk_root(train) / "docs" / "licenses" / "LICENSE.pdf"
    license_rtf = provider_sdk_root(train) / "docs" / "licenses" / "LICENSE.rtf"
    if license_pdf.is_file() and license_rtf.is_file():
        license_pdf.unlink()
    filelist = provider_sdk_root(train) / "docs" / "licenses" / "filelist.txt"
    if filelist.is_file():
        lines = filelist.read_text(encoding="utf-8").splitlines()
        filelist.write_text("\n".join(line for line in lines if line != "LICENSE.pdf") + "\n", encoding="utf-8")


def artifact_library_specs(train: TrainSpec, toolchain: str | None = None) -> list[tuple[Path, Path]]:
    """(source-relative, dest-relative) paths below a toolchain dir for every
    HAL/BSP archive a fully-built train publishes. Source paths keep the upstream
    `lib/` segment; dest paths drop it to match the promoted payload layout. When
    a toolchain is given, only parts/boards that support it are included, so a
    part a toolchain cannot target (e.g. Apollo2 under ACfE) is not expected."""
    specs: list[tuple[Path, Path]] = []
    for part in train.parts:
        if toolchain is not None and toolchain not in part_toolchains(train, part):
            continue
        specs.append((Path("lib") / part.name / "libam_hal.a", Path(part.name) / "libam_hal.a"))
    for board in train.boards:
        if toolchain is not None and toolchain not in board_toolchains(train, board):
            continue
        specs.append((
            Path("lib") / board.part / board.name / "libam_bsp.a",
            Path(board.part) / board.name / "libam_bsp.a",
        ))
    return specs


def built_artifact_toolchains(train: TrainSpec, version: str) -> list[str]:
    """Toolchains with a materialized artifact tree for this version. A toolchain
    with no <root>/<toolchain>/lib directory was simply not built and is skipped
    rather than treated as an incomplete build."""
    root = artifact_root(train, version)
    return [name for name in train.toolchains if (root / name / "lib").is_dir()]


def missing_artifact_libraries(train: TrainSpec, version: str) -> list[str]:
    """Expected HAL/BSP archives absent from the artifact tree, considering only
    toolchains that were built. Empty means the on-disk set is complete enough to
    publish the full train for every built toolchain."""
    root = artifact_root(train, version)
    missing: list[str] = []
    for name in built_artifact_toolchains(train, version):
        for source_rel, _ in artifact_library_specs(train, name):
            if not (root / name / source_rel).is_file():
                missing.append((Path(name) / source_rel).as_posix())
    return missing


def promote_artifact_libraries(train: TrainSpec, version: str) -> None:
    source_root = artifact_root(train, version)
    destination_root = provider_sdk_root(train) / "lib"
    toolchains = built_artifact_toolchains(train, version)
    if not toolchains:
        raise FileNotFoundError(f"no built artifacts found under {source_root}")
    # Publishing a partial set would silently ship an incomplete provider payload,
    # so refuse unless every part/board archive exists for each built toolchain.
    missing = missing_artifact_libraries(train, version)
    if missing:
        raise FileNotFoundError(
            f"incomplete artifact set under {display_path(source_root)}; missing: " + ", ".join(missing)
        )
    shutil.rmtree(destination_root, ignore_errors=True)
    for name in toolchains:
        for source_rel, dest_rel in artifact_library_specs(train, name):
            copy_file(source_root / name / source_rel, destination_root / name / dest_rel)


def promote_provider_payload(train: TrainSpec, version: str, sdk_root: Path) -> None:
    shutil.rmtree(provider_sdk_root(train), ignore_errors=True)
    provider_sdk_root(train).mkdir(parents=True, exist_ok=True)
    promote_vendor_headers(train, version, sdk_root)
    promote_system_sources(train, version, sdk_root)
    promote_utility_sources(train, version, sdk_root)
    promote_license_docs(train, sdk_root)
    for part in train.parts:
        promote_part_headers(train, version, sdk_root, part)
    for board in train.boards:
        promote_board_headers(train, version, sdk_root, board)
    scrub_promoted_headers(train)
    promote_artifact_libraries(train, version)
    copy_file(artifact_root(train, version) / "manifest.yaml", provider_sdk_root(train) / "artifact-manifest.yaml")


def build_hal(sdk_root: Path, profile: ToolchainProfile, part: PartBuild, train: TrainSpec, version: str, *, verbose: bool, clang_fpu: str) -> None:
    ensure_hal_generated_sources(sdk_root, part)
    hal_base = hal_build_base(sdk_root, part)
    ensure_hal_pin_header(hal_base, sdk_root)
    ensure_generated_makefile(hal_base / "gcc" / "Makefile", hal_base)
    hal_rel = hal_base.relative_to(sdk_root)
    build_dir = find_make_build_dir(
        sdk_root,
        hal_rel / "gcc",
        hal_rel,
    )
    extra_vars = (("FPU", clang_fpu),) if profile.name in ("atfe", "acfe") else ()
    run_make(build_dir, profile, verbose=verbose, extra_vars=extra_vars)
    source = built_artifact(build_dir, profile, "libam_hal.a")
    destination = artifact_root(train, version) / profile.name / "lib" / part.name / "libam_hal.a"
    copy_file(source, destination)


def strip_display_include(bsp_header: Path) -> None:
    """Remove the unconditional am_devices_display_generic.h include from a
    worktree am_bsp.h. Idempotent; no-op if the header or include is absent."""
    if not bsp_header.is_file():
        return
    text = bsp_header.read_text(encoding="utf-8")
    stripped = text.replace('#include "am_devices_display_generic.h"\n', "")
    if stripped != text:
        bsp_header.write_text(stripped, encoding="utf-8")


GPIO_PINCFG_SHIM = (
    "/* sdk-intake: alias the Apollo3+ GPIO pin-config type for legacy parts whose\n"
    "   HAL predates it but whose shared device headers reference it. */\n"
    "typedef uint32_t am_hal_gpio_pincfg_t;\n"
)


def inject_gpio_pincfg_shim(bsp_header: Path) -> None:
    """Insert a compatible am_hal_gpio_pincfg_t typedef ahead of the am_devices.h
    include in a worktree am_bsp.h. Idempotent; no-op if already present."""
    if not bsp_header.is_file():
        return
    text = bsp_header.read_text(encoding="utf-8")
    if "am_hal_gpio_pincfg_t" in text and GPIO_PINCFG_SHIM in text:
        return
    anchor = '#include "am_devices.h"\n'
    if anchor not in text or GPIO_PINCFG_SHIM in text:
        return
    bsp_header.write_text(text.replace(anchor, GPIO_PINCFG_SHIM + anchor, 1), encoding="utf-8")


def build_bsp(sdk_root: Path, profile: ToolchainProfile, board: BoardBuild, train: TrainSpec, version: str, *, verbose: bool, clang_fpu: str) -> None:
    bsp_dir = sdk_root / "boards" / board.name / "bsp"
    ensure_bsp_pins(sdk_root, bsp_dir)
    ensure_bsp_gpio_header(sdk_root, bsp_dir)
    if board.inject_gpio_pincfg_shim:
        inject_gpio_pincfg_shim(bsp_dir / "am_bsp.h")
    if board.strip_display_include:
        strip_display_include(bsp_dir / "am_bsp.h")
    ensure_generated_makefile(bsp_dir / "gcc" / "Makefile", bsp_dir)
    build_dir = find_make_build_dir(
        sdk_root,
        Path("boards") / board.name / "bsp" / "gcc",
        Path("boards") / board.name / "bsp",
    )
    extra_vars = (("FPU", clang_fpu),) if profile.name in ("atfe", "acfe") else ()
    run_make(build_dir, profile, verbose=verbose, extra_vars=extra_vars)
    source = built_artifact(build_dir, profile, "libam_bsp.a")
    destination = artifact_root(train, version) / profile.name / "lib" / board.part / board.name / "libam_bsp.a"
    copy_file(source, destination)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def existing_artifact(path: Path) -> str | None:
    if path.is_file():
        return sha256(path)
    return None


def yaml_quote(value: str) -> str:
    if not value or any(char in value for char in ":#{}[],&*?|-<>=!%@`\"'"):
        return repr(value)
    return value


def manifest_tool_identity(value: str) -> str:
    token = value.split()[0]
    candidate = Path(token)
    if candidate.is_absolute():
        return candidate.name
    return token


def write_manifest(
    train: TrainSpec,
    version: str,
    sdk_root: Path,
    profiles: dict[str, ToolchainProfile],
    *,
    source_kind: str,
    source_ref: str | None,
    source_commit: str | None,
    debug_symbols: bool,
) -> None:
    root = artifact_root(train, version)
    root.mkdir(parents=True, exist_ok=True)
    today = dt.date.today().isoformat()
    upstream_revision = find_upstream_revision(sdk_root)
    lines: list[str] = [
        "sdk:",
        f"  provider: {train.provider_id}",
        f"  version: {version}",
    ]
    if upstream_revision:
        lines.append(f"  upstream_revision: {upstream_revision}")
    lines.append(f"  source_kind: {source_kind}")
    if source_ref:
        lines.append(f"  source_ref: {source_ref}")
    if source_commit:
        lines.append(f"  source_commit: {source_commit}")
    lines.extend([
        "  source_root: " + display_path(sdk_root),
        "  raw_source_committed: false",
        "",
        "build:",
        f"  generated_at: {today}",
        "  source: native AmbiqSuite Makefiles",
        "  toolchains:",
    ])
    for name in train.toolchains:
        profile = profiles.get(name)
        present = any((root / name / "lib").rglob("*.a")) if (root / name / "lib").is_dir() else False
        status = "built" if present else "not-built"
        lines.extend([
            f"    {name}:",
            f"      status: {status}",
        ])
        if profile is not None:
            lines.extend([
                f"      compiler: {yaml_quote(manifest_tool_identity(profile.compiler))}",
                f"      archive_tool: {yaml_quote(manifest_tool_identity(profile.archive_tool))}",
                f"      output_config: {profile.config}",
                f"      debug_symbols: {str(debug_symbols).lower()}",
            ])
            if not debug_symbols:
                lines.append("      release_cflags: '-g0'")
    lines.extend(["", "parts:"])
    for part in train.parts:
        lines.extend([
            f"  - logical_skew: {part.name}",
            f"    mcu_dir: mcu/{part.name}",
            f"    hal_source_dir: mcu/{part.name}/hal",
            "    hal_artifacts:",
        ])
        for name in train.toolchains:
            relative_path = Path(name) / "lib" / part.name / "libam_hal.a"
            digest = existing_artifact(root / relative_path)
            if digest:
                lines.extend([
                    f"      {name}:",
                    f"        path: {relative_path.as_posix()}",
                    f"        sha256: {digest}",
                ])
    lines.extend(["", "boards:"])
    for board in train.boards:
        lines.extend([
            f"  - nsx_board: {board.name}",
            f"    ambiq_board_name: {board.name}",
            f"    logical_skew: {board.part}",
            f"    bsp_source_dir: boards/{board.name}/bsp",
            "    bsp_artifacts:",
        ])
        for name in train.toolchains:
            relative_path = Path(name) / "lib" / board.part / board.name / "libam_bsp.a"
            digest = existing_artifact(root / relative_path)
            if digest:
                lines.extend([
                    f"      {name}:",
                    f"        path: {relative_path.as_posix()}",
                    f"        sha256: {digest}",
                ])
    (root / "manifest.yaml").write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build AmbiqSuite HAL/BSP artifacts from a local SDK source tree.")
    parser.add_argument("--train", choices=tuple(TRAINS), default="stable", help="Provider train to build/promote (selects parts, boards, and target module). Default: stable.")
    parser.add_argument("--only-part", action="append", default=[], metavar="PART", help="Restrict the build/promote to the named part(s). May be repeated. Boards are filtered to the selected parts.")
    parser.add_argument("--version", default=None, help="AmbiqSuite version label, for example R5.2.0. Defaults to the train's version.")
    parser.add_argument("--zip", dest="zip_path", type=Path, help="Path to AmbiqSuite zip drop.")
    parser.add_argument("--source-root", type=Path, help="Path to an AmbiqSuite source tree.")
    parser.add_argument("--ambiqsuite-repo", type=Path, help="Path to the AmbiqSuite Git checkout to use as the source registry. Required for the default git-ref workflow unless AMBIQSUITE_REPO is set.")
    parser.add_argument("--source-ref", help=f"AmbiqSuite Git tag, branch, or commit to materialize. Default: {DEFAULT_SOURCE_REF} when --ambiqsuite-repo is used.")
    parser.add_argument("--source-worktree-dir", type=Path, help="Directory for the materialized AmbiqSuite Git worktree.")
    parser.add_argument("--force-source-ref", action="store_true", help="Recreate the materialized AmbiqSuite Git worktree before building.")
    parser.add_argument("--extract-dir", type=Path, help="Local extraction directory.")
    parser.add_argument("--force-extract", action="store_true", help="Remove and re-extract the SDK drop before building.")
    parser.add_argument("--toolchain", action="append", choices=("gcc", "atfe", "acfe", "all"), default=[], help="Toolchain to build. May be repeated. Default: gcc.")
    parser.add_argument("--promote", action="store_true", help="Promote built artifacts and curated SDK payload into the train's provider module.")
    parser.add_argument("--promote-only", action="store_true", help="Skip builds and only promote existing artifacts into the train's provider module.")
    parser.add_argument("--gcc-prefix", default="arm-none-eabi", help="GCC bare-metal tool prefix.")
    parser.add_argument("--atfe-root", default=os.environ.get("ATFE_ROOT"), help="ATfE installation root. Required for --toolchain atfe unless ATFE_ROOT is set.")
    parser.add_argument("--acfe-root", default=os.environ.get("ACFE_ROOT"), help="ACfE installation root. Required for --toolchain acfe unless ACFE_ROOT is set.")
    parser.add_argument("--atfe-target", default=DEFAULT_ATFE_TARGET, help="Clang target triple for ATfE.")
    parser.add_argument("--acfe-target", default=DEFAULT_ACFE_TARGET, help="Armclang target triple for ACfE.")
    parser.add_argument("--fpu", default=None, help="Override the clang-family FPU spelling for ALL parts. Default: per-part (M55 fp-armv8 / M4F fpv4-sp-d16).")
    parser.add_argument(
        "--debug-symbols",
        action="store_true",
        help="Keep compiler debug symbols. Defaults to release-style -g0 artifacts.",
    )
    parser.add_argument("--verbose", action="store_true", help="Pass VERBOSE=1 to AmbiqSuite makefiles.")
    return parser.parse_args()


def selected_toolchains(train: TrainSpec, values: list[str]) -> list[str]:
    if not values:
        return ["gcc"]
    if "all" in values:
        return list(train.toolchains)
    result: list[str] = []
    for value in values:
        if value not in train.toolchains:
            raise ValueError(
                f"toolchain '{value}' is not supported for train {train.train_id}; "
                f"supported toolchains: {', '.join(train.toolchains)}"
            )
        if value not in result:
            result.append(value)
    return result


def main() -> int:
    args = parse_args()
    full_train = TRAINS[args.train]
    build_train = full_train
    if args.only_part:
        selected = set(args.only_part)
        unknown = selected - {p.name for p in full_train.parts}
        if unknown:
            print(f"error: --only-part {sorted(unknown)} not in train {full_train.train_id} parts {[p.name for p in full_train.parts]}", file=sys.stderr)
            return 2
        build_train = replace(
            full_train,
            parts=tuple(p for p in full_train.parts if p.name in selected),
            boards=tuple(b for b in full_train.boards if b.part in selected),
        )
    try:
        sdk_root, source_kind, source_ref, source_commit = resolve_source_root(args)
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    version = args.version or full_train.version
    if version is None:
        if source_commit:
            version = f"{source_ref}-{git_commit_date(sdk_root, source_commit)}"
        else:
            version = f"snapshot-{dt.date.today():%Y.%m.%d}"

    def resolve_fpu(default: str) -> str:
        return args.fpu or default

    try:
        build_toolchains = selected_toolchains(full_train, args.toolchain)
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    profiles = {name: optional_toolchain_profile(args, name) for name in full_train.toolchains}
    if not args.promote_only:
        for name in build_toolchains:
            profile = profiles[name]
            if profile is None:
                try:
                    profile = toolchain_profile(args, name)
                except ValueError as error:
                    print(f"error: {error}", file=sys.stderr)
                    return 2
                profiles[name] = profile
            require_tool(profile.compiler)
            require_tool(profile.archive_tool)
            for tool in profile.required_tools:
                require_tool(tool)
            print(f"==> Building {version} HAL/BSP artifacts for {build_train.train_id}/{name}", flush=True)
            for part in build_train.parts:
                if name not in part_toolchains(build_train, part):
                    print(f"    skip HAL {part.name} (no {name} support)", flush=True)
                    continue
                print(f"    HAL {part.name}", flush=True)
                build_hal(sdk_root, profile, part, build_train, version, verbose=args.verbose, clang_fpu=resolve_fpu(part.clang_fpu))
            for board in build_train.boards:
                if name not in board_toolchains(build_train, board):
                    print(f"    skip BSP {board.name} ({board.part}, no {name} support)", flush=True)
                    continue
                part = next(p for p in build_train.parts if p.name == board.part)
                print(f"    BSP {board.name} ({board.part})", flush=True)
                build_bsp(sdk_root, profile, board, build_train, version, verbose=args.verbose, clang_fpu=resolve_fpu(part.clang_fpu))

        write_manifest(full_train, version, sdk_root, profiles, source_kind=source_kind, source_ref=source_ref, source_commit=source_commit, debug_symbols=args.debug_symbols)
        print(f"==> Wrote {artifact_root(full_train, version) / 'manifest.yaml'}", flush=True)

    if args.promote or args.promote_only:
        # Promotion always publishes the full train. Verify the on-disk artifact
        # set is complete BEFORE promote_provider_payload wipes the existing
        # payload, so a partial build (e.g. --only-part) cannot leave the provider
        # module empty or incomplete.
        incomplete = missing_artifact_libraries(full_train, version)
        if incomplete or not built_artifact_toolchains(full_train, version):
            detail = ", ".join(incomplete) if incomplete else "no built toolchains"
            print(
                f"error: refusing to promote {full_train.train_id} {version}: incomplete artifact set "
                f"({detail}); a partial --only-part build cannot be promoted unless the rest of the "
                f"train's artifacts already exist on disk for this version.",
                file=sys.stderr,
            )
            return 2
        if not args.promote_only:
            # The build branch just regenerated the manifest from `profiles`. A
            # toolchain whose artifacts exist on disk but whose root was not
            # configured this run has a None profile, so its manifest row would be
            # marked built yet stripped of compiler/archive metadata. Refuse rather
            # than publish a degraded manifest; rerun with that toolchain's root set
            # (or use --promote-only to reuse the original manifest).
            unprofiled = [
                name for name in built_artifact_toolchains(full_train, version) if profiles.get(name) is None
            ]
            if unprofiled:
                print(
                    f"error: refusing to promote {full_train.train_id} {version}: built toolchain(s) "
                    f"{', '.join(unprofiled)} have on-disk artifacts but no resolved profile this run, so the "
                    f"manifest would drop their compiler/archive metadata; set their toolchain root "
                    f"(ATFE_ROOT/ACFE_ROOT) and rebuild, or use --promote-only to reuse the existing manifest.",
                    file=sys.stderr,
                )
                return 2
        if args.promote_only:
            # Reuse the manifest written by the original full build when present: a
            # promote-only run may not have ATFE/ACFE roots configured, so its
            # resolved profiles can be empty and regenerating would drop per-
            # toolchain compiler/archive metadata from the published manifest.
            manifest_path = artifact_root(full_train, version) / "manifest.yaml"
            if manifest_path.is_file():
                print(f"==> Reusing {manifest_path}", flush=True)
            else:
                write_manifest(full_train, version, sdk_root, profiles, source_kind=source_kind, source_ref=source_ref, source_commit=source_commit, debug_symbols=args.debug_symbols)
                print(f"==> Wrote {manifest_path}", flush=True)
        promote_provider_payload(full_train, version, sdk_root)
        print(f"==> Promoted curated payload to {provider_sdk_root(full_train)}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        print(f"error: command failed with exit code {error.returncode}: {' '.join(error.cmd)}", file=sys.stderr)
        raise SystemExit(error.returncode)
