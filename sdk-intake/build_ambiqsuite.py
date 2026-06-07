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
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class PartBuild:
    name: str


@dataclass(frozen=True)
class BoardBuild:
    name: str
    part: str


@dataclass(frozen=True)
class ToolchainProfile:
    name: str
    config: str
    compiler: str
    archive_tool: str
    make_vars: tuple[tuple[str, str], ...]
    required_tools: tuple[str, ...] = ()


PARTS = (
    PartBuild("apollo330P"),
    PartBuild("apollo510"),
    PartBuild("apollo510L"),
)

BOARDS = (
    BoardBuild("apollo330mP_evb", "apollo330P"),
    BoardBuild("apollo510_evb", "apollo510"),
    BoardBuild("apollo510b_evb", "apollo510"),
    BoardBuild("apollo510dL_evb", "apollo510L"),
)

PROMOTED_TOOLCHAINS = ("gcc", "atfe", "acfe")
PROMOTED_FILE_SUFFIXES = (".h", ".hpp", ".inc")
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
# released Apollo510 family; bronco and atomiq110 are unannounced parts.
OMITTED_PART_HEADERS = (
    "apollo5a.h",
    "apollo5a_generic.h",
    "system_apollo5a.h",
    "bronco.h",
    "bronco_generic.h",
    "system_bronco.h",
    "atomiq110.h",
    "atomiq110_generic.h",
    "system_atomiq110.h",
)

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

RELEASE_CFLAGS = (
    "-g0",
)

DEFAULT_ATFE_ROOT = Path.home() / ".local" / "ATfE" / "ATfE-22.1.0"
DEFAULT_ACFE_ROOT = Path.home() / "ACfE"
DEFAULT_ATFE_TARGET = "thumbv8.1m.main-unknown-none-eabihf"
DEFAULT_ACFE_TARGET = "arm-arm-none-eabi"
DEFAULT_M55_FPU = "fp-armv8-fullfp16-sp-d16"
DEFAULT_AMBIQSUITE_REPO = Path.home() / "Ambiq" / "sws" / "ambiqsuite"
DEFAULT_SOURCE_REF = "release_sdk5p2p0_rc4"
REVISION_PATTERN = re.compile(r"revision\s+([^\s]+)\s+of the AmbiqSuite")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def intake_local_root() -> Path:
    return repo_root() / "sdk-intake" / "local"


def default_zip_path(version: str) -> Path:
    return intake_local_root() / "drops" / f"AmbiqSuite_{version}.zip"


def default_extract_dir(version: str) -> Path:
    return intake_local_root() / "work" / f"AmbiqSuite_{version}"


def default_git_worktree_dir(source_ref: str) -> Path:
    safe_ref = re.sub(r"[^A-Za-z0-9_.-]+", "-", source_ref).strip("-") or "ambiqsuite"
    return intake_local_root() / "work" / f"ambiqsuite-{safe_ref}"


def artifact_root(version: str) -> Path:
    return repo_root() / "artifacts" / "ambiqsuite" / version


def provider_sdk_root() -> Path:
    return repo_root() / "modules" / "nsx-ambiqsuite-r5" / "sdk"


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
    return find_sdk_root(worktree_dir)


def resolve_source_root(args: argparse.Namespace) -> tuple[Path, str, str | None]:
    if args.ambiqsuite_repo is not None or args.source_ref is not None:
        source_ref = args.source_ref or DEFAULT_SOURCE_REF
        worktree_dir = args.source_worktree_dir or default_git_worktree_dir(source_ref)
        sdk_root = materialize_git_ref(args.ambiqsuite_repo or DEFAULT_AMBIQSUITE_REPO, source_ref, worktree_dir, force=args.force_source_ref)
        return sdk_root, "git_ref", source_ref
    if args.source_root is not None:
        return find_sdk_root(args.source_root.expanduser().resolve()), "source_root", None
    zip_path = (args.zip_path or default_zip_path(args.version)).expanduser().resolve()
    require_file(zip_path)
    extract_dir = (args.extract_dir or default_extract_dir(args.version)).expanduser().resolve()
    return extract_sdk(zip_path, extract_dir, force=args.force_extract), "zip", str(zip_path)


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
    subprocess.run([sys.executable, str(find_buildproj_script(generator_dir))], cwd=generator_dir, check=True)
    require_file(generated_makefile)


def ensure_hal_generated_sources(sdk_root: Path, part: PartBuild) -> None:
    hal_dir = sdk_root / "mcu" / part.name / "hal"
    if (hal_dir / "am_hal_status.h").is_file():
        return
    makefile = hal_dir / "mcu" / "Makefile"
    require_file(makefile)
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
        root = Path(args.atfe_root).expanduser().resolve()
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
                ("FPU", args.fpu),
                ("EXTRA_CFLAGS", extra_cflags(ATFE_WARNING_SUPPRESSIONS, debug_symbols=args.debug_symbols)),
            ),
        )

    if toolchain == "acfe":
        root = Path(args.acfe_root).expanduser().resolve()
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
                ("FPU", args.fpu),
                ("EXTRA_CFLAGS", extra_cflags(ACFE_WARNING_SUPPRESSIONS, debug_symbols=args.debug_symbols)),
            ),
            required_tools=(str(armlink), str(fromelf)),
        )

    raise ValueError(toolchain)


def run_make(build_dir: Path, profile: ToolchainProfile, *, verbose: bool) -> None:
    makefile = build_dir / "Makefile"
    require_file(makefile)
    shutil.rmtree(build_dir / profile.config, ignore_errors=True)
    shutil.rmtree(build_dir / "gcc" / profile.config, ignore_errors=True)
    command = ["make"]
    if verbose:
        command.append("VERBOSE=1")
    command.extend(f"{key}={value}" for key, value in profile.make_vars)
    subprocess.run(command, cwd=build_dir, check=True)


def copy_file(source: Path, destination: Path) -> None:
    require_file(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def copy_matching_files(source_root: Path, destination_root: Path, suffixes: tuple[str, ...]) -> None:
    if not source_root.is_dir():
        raise FileNotFoundError(source_root)
    for source in sorted(source_root.rglob("*")):
        if not source.is_file() or source.name.startswith("."):
            continue
        if suffixes and source.suffix not in suffixes:
            continue
        destination = destination_root / source.relative_to(source_root)
        copy_file(source, destination)


def promote_part_headers(sdk_root: Path, part: PartBuild) -> None:
    ensure_hal_generated_sources(sdk_root, part)
    ensure_generated_makefile(sdk_root / "mcu" / part.name / "hal" / "mcu" / "gcc" / "Makefile", sdk_root / "mcu" / part.name / "hal" / "mcu")
    destination = provider_sdk_root() / "mcu" / part.name
    shutil.rmtree(destination, ignore_errors=True)
    copy_matching_files(sdk_root / "mcu" / part.name, destination, PROMOTED_FILE_SUFFIXES)


def promote_board_headers(sdk_root: Path, board: BoardBuild) -> None:
    bsp_dir = sdk_root / "boards" / board.name / "bsp"
    ensure_bsp_pins(sdk_root, bsp_dir)
    ensure_generated_makefile(bsp_dir / "gcc" / "Makefile", bsp_dir)
    destination = provider_sdk_root() / "boards" / board.name / "bsp"
    shutil.rmtree(destination, ignore_errors=True)
    copy_matching_files(bsp_dir, destination, PROMOTED_FILE_SUFFIXES)


def promote_vendor_headers(sdk_root: Path) -> None:
    copy_matching_files(
        sdk_root / "CMSIS" / "AmbiqMicro" / "Include",
        provider_sdk_root() / "CMSIS" / "AmbiqMicro" / "Include",
        PROMOTED_FILE_SUFFIXES,
    )
    copy_matching_files(sdk_root / "devices", provider_sdk_root() / "devices", PROMOTED_FILE_SUFFIXES)
    copy_matching_files(sdk_root / "utils", provider_sdk_root() / "utils", PROMOTED_FILE_SUFFIXES)
    am_util_id_header = provider_sdk_root() / "utils" / "am_util_id.h"
    if am_util_id_header.is_file():
        am_util_id_header.unlink()
    am_util_header = provider_sdk_root() / "utils" / "am_util.h"
    if am_util_header.is_file():
        am_util_header.write_text(
            am_util_header.read_text(encoding="utf-8").replace('#include "am_util_id.h"\n', ""),
            encoding="utf-8",
        )
    am_util_multi_boot_private = provider_sdk_root() / "utils" / "am_util_multi_boot_private.h"
    if am_util_multi_boot_private.is_file():
        am_util_multi_boot_private.write_text(
            am_util_multi_boot_private.read_text(encoding="utf-8").replace('#include "am_util_multi_boot_secure.h"\n', ""),
            encoding="utf-8",
        )
    copy_file(sdk_root / "mcu" / "am_sdk_version.h", provider_sdk_root() / "mcu" / "am_sdk_version.h")


def scrub_promoted_headers() -> None:
    include_root = provider_sdk_root() / "CMSIS" / "AmbiqMicro" / "Include"
    for name in OMITTED_PART_HEADERS:
        path = include_root / name
        if path.is_file():
            path.unlink()
    devices_root = provider_sdk_root() / "devices"
    for name in OMITTED_DEVICE_HEADERS:
        path = devices_root / name
        if path.is_file():
            path.unlink()
    for board_name in DISPLAY_BSP_HEADERS:
        board_header = provider_sdk_root() / "boards" / board_name / "bsp" / "am_bsp.h"
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


def promote_system_sources(sdk_root: Path) -> None:
    source_root = sdk_root / "CMSIS" / "AmbiqMicro" / "Source"
    destination_root = provider_sdk_root() / "CMSIS" / "AmbiqMicro" / "Source"
    destination_root.mkdir(parents=True, exist_ok=True)
    for part in PARTS:
        source = source_root / f"system_{part.name}.c"
        copy_file(source, destination_root / source.name)


def promote_utility_sources(sdk_root: Path) -> None:
    for source_name in PROMOTED_UTILITY_SOURCES:
        copy_file(sdk_root / "utils" / source_name, provider_sdk_root() / "src" / source_name)


def promote_license_docs(sdk_root: Path) -> None:
    shutil.copytree(sdk_root / "docs" / "licenses", provider_sdk_root() / "docs" / "licenses")
    license_pdf = provider_sdk_root() / "docs" / "licenses" / "LICENSE.pdf"
    license_rtf = provider_sdk_root() / "docs" / "licenses" / "LICENSE.rtf"
    if license_pdf.is_file() and license_rtf.is_file():
        license_pdf.unlink()
    filelist = provider_sdk_root() / "docs" / "licenses" / "filelist.txt"
    if filelist.is_file():
        lines = filelist.read_text(encoding="utf-8").splitlines()
        filelist.write_text("\n".join(line for line in lines if line != "LICENSE.pdf") + "\n", encoding="utf-8")


def promote_artifact_libraries(version: str) -> None:
    source_root = artifact_root(version)
    destination_root = provider_sdk_root() / "lib"
    shutil.rmtree(destination_root, ignore_errors=True)
    promoted = 0
    for toolchain in PROMOTED_TOOLCHAINS:
        for part in PARTS:
            source = source_root / toolchain / "lib" / part.name / "libam_hal.a"
            if source.is_file():
                copy_file(source, destination_root / toolchain / part.name / "libam_hal.a")
                promoted += 1
        for board in BOARDS:
            source = source_root / toolchain / "lib" / board.part / board.name / "libam_bsp.a"
            if source.is_file():
                copy_file(source, destination_root / toolchain / board.part / board.name / "libam_bsp.a")
                promoted += 1
    if promoted == 0:
        raise FileNotFoundError(f"no built artifacts found under {source_root}")


def promote_provider_payload(version: str, sdk_root: Path) -> None:
    shutil.rmtree(provider_sdk_root(), ignore_errors=True)
    provider_sdk_root().mkdir(parents=True, exist_ok=True)
    promote_vendor_headers(sdk_root)
    promote_system_sources(sdk_root)
    promote_utility_sources(sdk_root)
    promote_license_docs(sdk_root)
    for part in PARTS:
        promote_part_headers(sdk_root, part)
    for board in BOARDS:
        promote_board_headers(sdk_root, board)
    scrub_promoted_headers()
    promote_artifact_libraries(version)
    copy_file(artifact_root(version) / "manifest.yaml", provider_sdk_root() / "artifact-manifest.yaml")


def build_hal(sdk_root: Path, profile: ToolchainProfile, part: PartBuild, version: str, *, verbose: bool) -> None:
    ensure_hal_generated_sources(sdk_root, part)
    ensure_generated_makefile(sdk_root / "mcu" / part.name / "hal" / "mcu" / "gcc" / "Makefile", sdk_root / "mcu" / part.name / "hal" / "mcu")
    build_dir = find_make_build_dir(
        sdk_root,
        Path("mcu") / part.name / "hal" / "mcu" / "gcc",
        Path("mcu") / part.name / "hal" / "gcc",
        Path("mcu") / part.name / "hal" / "mcu",
        Path("mcu") / part.name / "hal",
    )
    run_make(build_dir, profile, verbose=verbose)
    source = built_artifact(build_dir, profile, "libam_hal.a")
    destination = artifact_root(version) / profile.name / "lib" / part.name / "libam_hal.a"
    copy_file(source, destination)


def build_bsp(sdk_root: Path, profile: ToolchainProfile, board: BoardBuild, version: str, *, verbose: bool) -> None:
    bsp_dir = sdk_root / "boards" / board.name / "bsp"
    ensure_bsp_pins(sdk_root, bsp_dir)
    ensure_generated_makefile(bsp_dir / "gcc" / "Makefile", bsp_dir)
    build_dir = find_make_build_dir(
        sdk_root,
        Path("boards") / board.name / "bsp" / "gcc",
        Path("boards") / board.name / "bsp",
    )
    run_make(build_dir, profile, verbose=verbose)
    source = built_artifact(build_dir, profile, "libam_bsp.a")
    destination = artifact_root(version) / profile.name / "lib" / board.part / board.name / "libam_bsp.a"
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


def write_manifest(
    version: str,
    sdk_root: Path,
    profiles: dict[str, ToolchainProfile],
    *,
    source_kind: str,
    source_ref: str | None,
    debug_symbols: bool,
) -> None:
    root = artifact_root(version)
    root.mkdir(parents=True, exist_ok=True)
    today = dt.date.today().isoformat()
    upstream_revision = find_upstream_revision(sdk_root)
    lines: list[str] = [
        "sdk:",
        "  provider: ambiqsuite-r5",
        f"  version: {version}",
    ]
    if upstream_revision:
        lines.append(f"  upstream_revision: {upstream_revision}")
    lines.append(f"  source_kind: {source_kind}")
    if source_ref:
        lines.append(f"  source_ref: {source_ref}")
    lines.extend([
        "  source_root: " + display_path(sdk_root),
        "  raw_source_committed: false",
        "",
        "build:",
        f"  generated_at: {today}",
        "  source: native AmbiqSuite Makefiles",
        "  toolchains:",
    ])
    for name in ("gcc", "atfe", "acfe"):
        profile = profiles.get(name)
        present = any((root / name / "lib").rglob("*.a")) if (root / name / "lib").is_dir() else False
        status = "built" if present else "not-built"
        lines.extend([
            f"    {name}:",
            f"      status: {status}",
        ])
        if profile is not None:
            lines.extend([
                f"      compiler: {yaml_quote(profile.compiler)}",
                f"      archive_tool: {yaml_quote(profile.archive_tool)}",
                f"      output_config: {profile.config}",
                f"      debug_symbols: {str(debug_symbols).lower()}",
            ])
            if not debug_symbols:
                lines.append("      release_cflags: '-g0'")
    lines.extend(["", "parts:"])
    for part in PARTS:
        lines.extend([
            f"  - logical_skew: {part.name}",
            f"    mcu_dir: mcu/{part.name}",
            f"    hal_source_dir: mcu/{part.name}/hal",
            "    hal_artifacts:",
        ])
        for name in ("gcc", "atfe", "acfe"):
            relative_path = Path(name) / "lib" / part.name / "libam_hal.a"
            digest = existing_artifact(root / relative_path)
            if digest:
                lines.extend([
                    f"      {name}:",
                    f"        path: {relative_path.as_posix()}",
                    f"        sha256: {digest}",
                ])
    lines.extend(["", "boards:"])
    for board in BOARDS:
        lines.extend([
            f"  - nsx_board: {board.name}",
            f"    ambiq_board_name: {board.name}",
            f"    logical_skew: {board.part}",
            f"    bsp_source_dir: boards/{board.name}/bsp",
            "    bsp_artifacts:",
        ])
        for name in ("gcc", "atfe", "acfe"):
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
    parser.add_argument("--version", default="R5.2.0", help="AmbiqSuite version label, for example R5.2.0.")
    parser.add_argument("--zip", dest="zip_path", type=Path, help="Path to AmbiqSuite zip drop.")
    parser.add_argument("--source-root", type=Path, help="Path to an AmbiqSuite source tree.")
    parser.add_argument("--ambiqsuite-repo", type=Path, help="Path to the AmbiqSuite Git checkout to use as the source registry.")
    parser.add_argument("--source-ref", help=f"AmbiqSuite Git tag, branch, or commit to materialize. Default: {DEFAULT_SOURCE_REF} when --ambiqsuite-repo is used.")
    parser.add_argument("--source-worktree-dir", type=Path, help="Directory for the materialized AmbiqSuite Git worktree.")
    parser.add_argument("--force-source-ref", action="store_true", help="Recreate the materialized AmbiqSuite Git worktree before building.")
    parser.add_argument("--extract-dir", type=Path, help="Local extraction directory.")
    parser.add_argument("--force-extract", action="store_true", help="Remove and re-extract the SDK drop before building.")
    parser.add_argument("--toolchain", action="append", choices=("gcc", "atfe", "acfe", "all"), default=[], help="Toolchain to build. May be repeated. Default: gcc.")
    parser.add_argument("--promote", action="store_true", help="Promote built artifacts and curated SDK payload into modules/nsx-ambiqsuite-r5/sdk.")
    parser.add_argument("--promote-only", action="store_true", help="Skip builds and only promote existing artifacts into modules/nsx-ambiqsuite-r5/sdk.")
    parser.add_argument("--gcc-prefix", default="arm-none-eabi", help="GCC bare-metal tool prefix.")
    parser.add_argument("--atfe-root", default=os.environ.get("ATFE_ROOT", str(DEFAULT_ATFE_ROOT)), help="ATfE installation root.")
    parser.add_argument("--acfe-root", default=os.environ.get("ACFE_ROOT", str(DEFAULT_ACFE_ROOT)), help="ACfE installation root.")
    parser.add_argument("--atfe-target", default=DEFAULT_ATFE_TARGET, help="Clang target triple for ATfE.")
    parser.add_argument("--acfe-target", default=DEFAULT_ACFE_TARGET, help="Armclang target triple for ACfE.")
    parser.add_argument("--fpu", default=DEFAULT_M55_FPU, help="Explicit M55 FPU spelling for clang-family toolchains.")
    parser.add_argument(
        "--debug-symbols",
        action="store_true",
        help="Keep compiler debug symbols. Defaults to release-style -g0 artifacts.",
    )
    parser.add_argument("--verbose", action="store_true", help="Pass VERBOSE=1 to AmbiqSuite makefiles.")
    return parser.parse_args()


def selected_toolchains(values: list[str]) -> list[str]:
    if not values:
        return ["gcc"]
    if "all" in values:
        return ["gcc", "atfe", "acfe"]
    result: list[str] = []
    for value in values:
        if value not in result:
            result.append(value)
    return result


def main() -> int:
    args = parse_args()
    sdk_root, source_kind, source_ref = resolve_source_root(args)

    profiles = {name: toolchain_profile(args, name) for name in ("gcc", "atfe", "acfe")}
    if not args.promote_only:
        for name in selected_toolchains(args.toolchain):
            profile = profiles[name]
            require_tool(profile.compiler)
            require_tool(profile.archive_tool)
            for tool in profile.required_tools:
                require_tool(tool)
            print(f"==> Building {args.version} HAL/BSP artifacts for {name}", flush=True)
            for part in PARTS:
                print(f"    HAL {part.name}", flush=True)
                build_hal(sdk_root, profile, part, args.version, verbose=args.verbose)
            for board in BOARDS:
                print(f"    BSP {board.name} ({board.part})", flush=True)
                build_bsp(sdk_root, profile, board, args.version, verbose=args.verbose)

        write_manifest(args.version, sdk_root, profiles, source_kind=source_kind, source_ref=source_ref, debug_symbols=args.debug_symbols)
        print(f"==> Wrote {artifact_root(args.version) / 'manifest.yaml'}", flush=True)

    if args.promote or args.promote_only:
        if args.promote_only:
            write_manifest(args.version, sdk_root, profiles, source_kind=source_kind, source_ref=source_ref, debug_symbols=args.debug_symbols)
            print(f"==> Wrote {artifact_root(args.version) / 'manifest.yaml'}", flush=True)
        promote_provider_payload(args.version, sdk_root)
        print(f"==> Promoted curated payload to {provider_sdk_root()}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        print(f"error: command failed with exit code {error.returncode}: {' '.join(error.cmd)}", file=sys.stderr)
        raise SystemExit(error.returncode)
