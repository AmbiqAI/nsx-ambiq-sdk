"""Pinned vendor device support, provisioned separately from this public repo."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import zipfile


MANIFEST = Path(__file__).with_name('device-pack.json')


def cache_root() -> Path:
    return Path(os.environ.get('XDG_CACHE_HOME', Path.home() / '.cache')) / 'nsx-sdk' / 'device-packs'


def verify(directory: Path, manifest: dict) -> None:
    for name, expected in manifest['files'].items():
        path = directory / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise RuntimeError(f'Missing or changed device-pack file: {path}; provision the pinned archive again')


def install(archive: Path, root: Path) -> Path:
    manifest = json.loads(MANIFEST.read_text())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != manifest['archive_sha256']:
        raise ValueError('Archive does not match the pinned Ambiq device pack')
    root.mkdir(parents=True, exist_ok=True)
    destination = root / manifest['archive_sha256']
    if destination.exists():
        verify(destination, manifest)
        return destination
    with tempfile.TemporaryDirectory(dir=root) as temporary:
        staged = Path(temporary) / 'pack'
        staged.mkdir()
        with zipfile.ZipFile(archive) as zipped:
            # Extract only exact manifest members, never arbitrary archive paths.
            for name in manifest['files']:
                target = staged / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(zipped.read(manifest['archive_prefix'] + name))
        verify(staged, manifest)
        try:
            staged.rename(destination)
        except OSError:
            if not destination.is_dir():
                raise
            verify(destination, manifest)
    return destination


def configure(probe, device: str, root: Path | None = None) -> dict | None:
    manifest = json.loads(MANIFEST.read_text())
    if device not in manifest['devices']:
        return None
    directory = (root if root is not None else cache_root()) / manifest['archive_sha256']
    if not directory.is_dir():
        raise RuntimeError('Ambiq device pack required. Run: python -m tools.hardware.device_pack '
                           '--archive /path/to/JLink_Apollo330P_510L_20251015.zip')
    verify(directory, manifest)
    if tuple(map(int, probe.version.split('.'))) < tuple(map(int, manifest['minimum_jlink'].split('.'))):
        raise RuntimeError(f"Device pack requires J-Link {manifest['minimum_jlink']} or later")
    # SEGGER requires the trailing separator when this command names a directory.
    result = probe.exec_command(f'JLinkDevicesXMLPath = {directory.resolve()}{os.sep}')
    if result != 0:
        raise RuntimeError(f'J-Link rejected device-pack path: {result}')
    probe.get_device_index(device)
    return {'name': manifest['name'], 'archive_sha256': manifest['archive_sha256'],
            'directory': str(directory.resolve()), 'files': manifest['files']}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    args = parser.parse_args()
    print(install(args.archive, cache_root()))


if __name__ == '__main__':
    main()
