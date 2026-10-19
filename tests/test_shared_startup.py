"""Link initialized shared/resource data and execute the actual copy loops."""
from __future__ import annotations

import ast
from pathlib import Path
import re
import shutil
import socket
import subprocess

import pytest

from test_small_m55_memory import compiler_for_test


SOCS = ("apollo4l", "apollo4p", "apollo510", "apollo510b", "apollo5b", "atomiq110", "apollo330P", "apollo510L")
SCRIPTS = tuple(path.relative_to(Path(__file__).resolve().parents[1]).as_posix()
    for path in sorted((Path(__file__).resolve().parents[1] / "modules/nsx-core/src").glob("*/gcc/linker*.ld")))
SCRIPTS = tuple(path for path in SCRIPTS if Path(path).parts[3] in SOCS)


@pytest.mark.parametrize("script", SCRIPTS)
@pytest.mark.parametrize("contents", ("both", "resource", "shared", "subsection", "empty"))
@pytest.mark.parametrize("family", ("gcc", "atfe"))
def test_shared_load_copy_window(repo_root: Path, tmp_path: Path, script: str, contents: str, family: str) -> None:
    compiler = compiler_for_test(family)
    target = ["--target=arm-none-eabi"] if family == "atfe" else []
    tools = compiler.parent
    nm = tools / ("llvm-nm" if family == "atfe" else "arm-none-eabi-nm")
    objcopy = tools / ("llvm-objcopy" if family == "atfe" else "arm-none-eabi-objcopy")
    body = '.syntax unified\n.thumb\n.section .text,"ax"\n.global Reset_Handler\n.thumb_func\nReset_Handler: b .\n'
    expected = []
    if contents in ("both", "resource"):
        body += '.section .resource_table,"aw"\n.global resource_marker\nresource_marker: .word 0xAABBCCDD\n'
        expected.append(("resource_marker", 0xAABBCCDD))
    if contents in ("both", "shared", "subsection"):
        section = ".shared.sample" if contents == "subsection" else ".shared"
        body += f'.section {section},"aw"\n.global shared_value\nshared_value: .word 0x11223344\n'
        expected.append(("shared_value", 0x11223344))
    source = tmp_path / "probe.s"
    source.write_text(body)
    obj, elf = tmp_path / "probe.o", tmp_path / "probe.elf"
    flags = [*target, "-mcpu=cortex-m55", "-mthumb", "-mfloat-abi=hard"]
    subprocess.run([str(compiler), *flags, "-c", str(source), "-o", str(obj)], check=True, capture_output=True)
    result = subprocess.run([str(compiler), *flags, "-nostdlib", f"-T{repo_root / script}", str(obj), "-o", str(elf)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    symbols = subprocess.check_output([str(nm), "-n", str(elf)], text=True)
    addresses = {name: int(address, 16) for address, name in re.findall(r"^([0-9a-fA-F]+)\s+\S\s+(\S+)$", symbols, re.M)}
    assert addresses["_sedata"] - addresses["_ssdata"] == len(expected) * 4
    if not expected:
        return
    image = tmp_path / "shared.bin"
    subprocess.run([str(objcopy), "--dump-section", f".shared={image}", str(elf)], check=True, capture_output=True)
    data = image.read_bytes()
    assert len(data) == len(expected) * 4
    for name, value in expected:
        # Startup copies from LOADADDR(.shared) directly to _ssdata.
        offset = addresses[name] - addresses["_ssdata"]
        assert 0 <= offset < len(data)
        assert int.from_bytes(data[offset:offset + 4], "little") == value


@pytest.mark.parametrize("soc", SOCS)
@pytest.mark.parametrize("empty", (False, True))
def test_actual_shared_copy_loop(repo_root: Path, tmp_path: Path, soc: str, empty: bool) -> None:
    qemu = shutil.which("qemu-system-arm")
    compiler = compiler_for_test("gcc")
    gdb = compiler.parent / "arm-none-eabi-gdb"
    if not qemu or not gdb.is_file():
        pytest.skip("QEMU and Arm GDB are required for copy-loop execution")
    text = (repo_root / f"modules/nsx-core/src/{soc}/gcc/startup_gcc.c").read_text()
    blocks = re.findall(r"__asm\((.*?)\);", text, re.S)
    selected = [block for block in blocks if "ldr     r0, =_init_data_sram" in block]
    assert len(selected) == 1
    assembly = "".join(ast.literal_eval(token) for token in re.findall(r'"(?:[^"\\]|\\.)*"', selected[0]))
    end = "destination" if empty else "destination + 8"
    probe = tmp_path / "loop.s"
    probe.write_text('.syntax unified\n.thumb\n.section .text,"ax"\n.global Reset_Handler,done\n.thumb_func\nReset_Handler:\n' + assembly + '''
.thumb_func
done: b done
.section .rodata,"a"
.global initial
initial: .word 0xAABBCCDD,0x11223344
.section .data,"aw"
.global destination
destination: .word 0xDEADBEEF,0xDEADBEEF,0xDEADBEEF
''' + f'.set _init_data_sram,initial\n.set _ssdata,destination\n.set _sedata,{end}\n')
    linker = tmp_path / "loop.ld"
    linker.write_text('SECTIONS { .text 0x01000000 : { *(.text*) *(.rodata*) } .data 0x21000000 : { *(.data*) } }')
    elf = tmp_path / "loop.elf"
    subprocess.run([str(compiler), "-mcpu=cortex-m55", "-mthumb", "-nostdlib", f"-T{linker}", str(probe), "-o", str(elf)], check=True, capture_output=True)
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    # Scratch addresses and a direct entry isolate startup copy instructions;
    # this machine is not an Apollo peripheral or memory-aperture model.
    with (tmp_path / "qemu.log").open("w") as log:
        process = subprocess.Popen([qemu, "-M", "mps3-an547", "-nographic", "-S", "-gdb", f"tcp:127.0.0.1:{port}", "-kernel", str(elf)], stdout=log, stderr=log)
        try:
            commands = ["set pagination off", "set confirm off", f"target remote 127.0.0.1:{port}", "set $xpsr=0x01000000", "set $pc=Reset_Handler", "break done", "continue", "x/3wx &destination", "detach", "quit"]
            command = [str(gdb), "-q", "-nx", "-batch", str(elf)]
            for item in commands:
                command += ["-ex", item]
            result = subprocess.run(command, capture_output=True, text=True, timeout=15)
            (tmp_path / "gdb.log").write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stdout + result.stderr
            expected = "0xdeadbeef\t0xdeadbeef\t0xdeadbeef" if empty else "0xaabbccdd\t0x11223344\t0xdeadbeef"
            assert expected in result.stdout.lower(), result.stdout + result.stderr
        finally:
            process.terminate()
            process.wait(timeout=5)
