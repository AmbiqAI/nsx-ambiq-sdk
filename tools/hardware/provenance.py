"""Record the separate harness, production SDK and resolved build inputs."""
import json
from pathlib import Path
import subprocess


def checkout(root: Path) -> dict:
    def git(*args):
        return subprocess.check_output(['git', '-C', str(root), *args], text=True, timeout=15).strip()
    if Path(git('rev-parse', '--show-toplevel')).resolve() != root.resolve():
        raise ValueError(f'Expected a repository root: {root}')
    return {'root': str(root.resolve()), 'revision': git('rev-parse', 'HEAD'),
            'dirty': bool(git('status', '--porcelain', '--untracked-files=all'))}


def dependency_inputs(text: str, output: Path) -> list[Path]:
    paths = []
    for line in text.splitlines():
        if line.startswith('    '):
            path = Path(line.strip())
            if not path.is_absolute():
                path = output / path
            if path.is_file():
                paths.append(path.resolve())
    return paths


def cmake_inputs(output: Path) -> list[Path]:
    files = list((output / '.cmake/api/v1/reply').glob('cmakeFiles-v1-*.json'))
    if len(files) != 1:
        raise RuntimeError('Expected one CMake input provenance reply')
    reply = json.loads(files[0].read_text())
    paths = []
    for item in reply['inputs']:
        path = Path(item['path'])
        if not path.is_absolute():
            path = Path(reply['paths']['build' if item.get('isGenerated') else 'source']) / path
        if path.is_file():
            paths.append(path.resolve())
    return paths


def classify_inputs(paths: list[Path], harness: Path, sdk: Path, output: Path) -> dict[str, list[str]]:
    groups: dict[str, list[str]] = {name: [] for name in ('harness', 'sdk', 'generated', 'environment')}
    for path in sorted(set(p.resolve() for p in paths)):
        if path.is_relative_to(output):
            role = 'generated'
        elif path.is_relative_to(harness) and (harness != sdk or path.relative_to(harness).parts[0] in ('tests', 'tools')):
            role = 'harness'
        elif path.is_relative_to(sdk):
            role = 'sdk'
        else:
            role = 'environment'
        groups[role].append(str(path))
    return groups
