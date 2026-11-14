"""Local configuration and preflight; acceptance policy remains in the Plan."""
from dataclasses import asdict, dataclass
import math
import os
from pathlib import Path
import shutil
import sys

from tools.hardware.api import BoardRequest
from tools.hardware.provenance import checkout
from tools.hardware.process import run_worker
from .io import read_json, sha256
from .plan import Plan
from .policy import BOARDS, CORE_GATES

HARNESS_ROOT = Path(__file__).resolve().parents[2]


def execution_path(gcc: Path) -> str:
    return (str(gcc / 'bin') + os.pathsep + str(Path(sys.executable).parent)
            + os.pathsep + os.environ.get('PATH', ''))


@dataclass(frozen=True)
class Binding:
    board: str
    probe: str
    device: str

    def __post_init__(self):
        if not all(isinstance(value, str) for value in (self.board, self.probe, self.device)):
            raise ValueError('Inventory board/probe/device must be strings')
        BoardRequest(self.probe, self.device)
        if not self.device.strip() or self.board not in BOARDS or int(self.probe) <= 0:
            raise ValueError('Inventory requires a supported board and expected device')


def inventory(path: Path) -> tuple[Binding, ...]:
    value = read_json(path)
    if set(value) != {'schema_version', 'boards'} or type(value['schema_version']) is not int or value['schema_version'] != 1:
        raise ValueError('Inventory requires schema_version 1 and boards')
    if not isinstance(value['boards'], list):
        raise ValueError('Inventory boards must be an array')
    entries = []
    for entry in value['boards']:
        if not isinstance(entry, dict) or set(entry) != {'board', 'probe', 'device'}:
            raise ValueError('Inventory entry requires board/probe/device')
        entries.append(Binding(**entry))
    if len({entry.board for entry in entries}) != len(entries):
        raise ValueError('Duplicate inventory board')
    if len({int(entry.probe) for entry in entries}) != len(entries):
        raise ValueError('One probe cannot represent multiple boards')
    return tuple(entries)


@dataclass(frozen=True)
class Config:
    baseline: Path
    candidate: Path
    gcc_root: Path
    output: Path
    host_timeout: float = 1200
    hardware_timeout: float = 40

    def __post_init__(self):
        for name in ('baseline', 'candidate', 'gcc_root', 'output'):
            object.__setattr__(self, name, getattr(self, name).resolve())
        for value in (self.host_timeout, self.hardware_timeout):
            if not math.isfinite(value) or value <= 0:
                raise ValueError('Timeouts must be finite and positive')

    def roots(self, plan: Plan) -> dict:
        result = {}
        for role, root in (('baseline', self.baseline), ('candidate', self.candidate), ('harness', HARNESS_ROOT)):
            value = checkout(root)
            if value['dirty'] or value['revision'] != getattr(plan.revisions, role):
                raise ValueError(f'{role} must match its clean planned commit: {root}')
            result[role] = value
        return result

    def prepare(self, plan: Plan) -> dict:
        if sys.platform != 'linux':
            raise ValueError('Local qualification execution currently supports Linux')
        roots = self.roots(plan)
        if any(self.output.is_relative_to(Path(value['root'])) for value in roots.values()):
            raise ValueError('Evidence output must be outside all input checkouts')
        if self.output.exists():
            raise ValueError('Evidence output must be a fresh directory')
        if plan.gates != tuple(sorted(CORE_GATES, key=lambda gate: gate.id)):
            raise ValueError('Additional host/prerequisite handlers are not implemented; coverage stays incomplete')
        if any(item.toolchain != 'gcc' for item in plan.compilers):
            raise ValueError('Only GCC execution is implemented; declared licensed compiler coverage is incomplete')
        self.output.mkdir(parents=True)
        return roots


@dataclass(frozen=True)
class Binary:
    name: str
    path: str
    sha256: str
    version: str


@dataclass(frozen=True)
class Tools:
    gcc_root: str
    cmake_root: str
    binaries: tuple[Binary, ...]

    @classmethod
    def capture(cls, gcc: Path, output: Path):
        output.mkdir()
        env = dict(os.environ, PATH=execution_path(gcc))
        def capture(command, name):
            path = output / f'{name}.log'
            with path.open('w') as log:
                run_worker(command, log=log, cwd=output, timeout=15, env=env)
            return path.read_text()
        paths = {name: gcc / 'bin' / f'arm-none-eabi-{name}' for name in ('gcc', 'g++', 'ld', 'nm', 'readelf')}
        for name in ('cmake', 'ninja'):
            found = shutil.which(name, path=env['PATH'])
            if not found:
                raise ValueError(f'Missing tool: {name}')
            paths[name] = Path(found)
        paths['python'] = Path(sys.executable)
        binaries = []
        for name, path in paths.items():
            # Preserve the venv invocation path; resolving its Python symlink
            # would silently execute without the installed harness dependencies.
            path = path.absolute()
            if not path.is_file() or not os.access(path, os.X_OK):
                raise ValueError(f'Missing executable: {path}')
            text = capture([str(path), '--version'], name)
            binaries.append(Binary(name, str(path), sha256(path), text.splitlines()[0]))
        script = output / 'cmake-root.cmake'
        script.write_text('message("${CMAKE_ROOT}")\n')
        root = capture([str(paths['cmake']), '-P', str(script)], 'cmake-root').strip()
        if not Path(root).is_dir():
            raise ValueError('CMake did not report its module directory')
        return cls(str(gcc), root, tuple(binaries))

    def path(self, name: str) -> Path:
        return Path(next(item.path for item in self.binaries if item.name == name))

    def to_dict(self) -> dict:
        return asdict(self)

    def fingerprint(self) -> dict:
        return {item.name: (item.sha256, item.version) for item in self.binaries}

    def environment(self, scratch: Path) -> dict[str, str]:
        scratch.mkdir(exist_ok=True)
        env = os.environ.copy()
        for key in ('PYTHONPATH', 'PYTEST_ADDOPTS', 'SKIP'):
            env.pop(key, None)
        env.update(PATH=execution_path(Path(self.gcc_root)), GCC_ROOT=self.gcc_root,
                   NSX_REQUIRE_ARM_GCC='1', PYTEST_DISABLE_PLUGIN_AUTOLOAD='1', LC_ALL='C', TMPDIR=str(scratch))
        return env
