"""Device-pack provisioning and session selection need no SEGGER installation."""
import hashlib
import json
import zipfile

import pytest

from tools.hardware import device_pack


@pytest.fixture
def pack(tmp_path, monkeypatch):
    archive = tmp_path / 'vendor.zip'
    files = {'JLinkDevices.xml': b'<Database/>', 'Devices/vendor.elf': b'loader'}
    with zipfile.ZipFile(archive, 'w') as z:
        for name, content in files.items():
            z.writestr('vendor/' + name, content)
        z.writestr('vendor/../../unwanted', b'not extracted')
    manifest = {'name': 'test', 'archive_prefix': 'vendor/', 'minimum_jlink': '8.40',
                'archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
                'devices': ['TEST'],
                'files': {n: hashlib.sha256(v).hexdigest() for n, v in files.items()}}
    path = tmp_path / 'manifest.json'
    path.write_text(json.dumps(manifest))
    monkeypatch.setattr(device_pack, 'MANIFEST', path)
    return archive, tmp_path / 'cache with spaces'


class Probe:
    version = '9.50'

    def __init__(self):
        self.commands = []

    def exec_command(self, command):
        self.commands.append(command)
        return 0

    def get_device_index(self, device):
        assert self.commands
        return 1


def test_install_and_select_verified_pack(pack):
    archive, root = pack
    installed = device_pack.install(archive, root)
    assert device_pack.install(archive, root) == installed
    assert sorted(str(p.relative_to(installed)) for p in installed.rglob('*') if p.is_file()) == [
        'Devices/vendor.elf', 'JLinkDevices.xml']
    probe = Probe()
    evidence = device_pack.configure(probe, 'TEST', root)
    assert evidence['archive_sha256'] == installed.name
    assert probe.commands == [f'JLinkDevicesXMLPath = {installed}/']


def test_wrong_archive_rejected(pack):
    archive, root = pack
    archive.write_bytes(b'wrong archive')
    with pytest.raises(ValueError, match='does not match'):
        device_pack.install(archive, root)
    assert not root.exists()


def test_changed_loader_rejected_before_jlink(pack):
    archive, root = pack
    installed = device_pack.install(archive, root)
    (installed / 'Devices/vendor.elf').write_bytes(b'changed')
    probe = Probe()
    with pytest.raises(RuntimeError, match='Missing or changed'):
        device_pack.configure(probe, 'TEST', root)
    assert not probe.commands


def test_missing_pack_has_provisioning_hint(pack):
    _, root = pack
    with pytest.raises(RuntimeError, match='python -m tools.hardware.device_pack'):
        device_pack.configure(Probe(), 'TEST', root)


def test_unaffected_device_needs_no_pack(pack):
    _, root = pack
    probe = Probe()
    assert device_pack.configure(probe, 'OTHER', root) is None
    assert not probe.commands


def test_old_jlink_rejected(pack):
    archive, root = pack
    device_pack.install(archive, root)
    probe = Probe()
    probe.version = '8.12'
    with pytest.raises(RuntimeError, match='8.40 or later'):
        device_pack.configure(probe, 'TEST', root)
    assert not probe.commands
