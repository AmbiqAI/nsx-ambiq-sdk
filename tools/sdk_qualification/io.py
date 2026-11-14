"""Strict JSON records, content identities and contained evidence paths."""
import hashlib
import json
from pathlib import Path

from .plan import _unique_object


def sha256(path: Path) -> str:
    value = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def identity(value: object) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def read_json(path: Path) -> dict:
    value = json.loads(path.read_text(), object_pairs_hook=_unique_object)
    if not isinstance(value, dict):
        raise ValueError(f'Expected JSON object: {path}')
    return value


def contained(root: Path, relative: str) -> Path:
    if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
        raise ValueError('Evidence paths must be relative')
    path = (root / relative).resolve()
    if not path.is_relative_to(root.resolve()):
        raise ValueError('Evidence path escapes its record directory')
    return path


def write_record(output: Path, body: dict) -> dict:
    envelope = {'schema_version': 1, 'kind': 'sdk-qualification-execution',
                'body': body, 'sha256': identity(body)}
    with (output / 'record.json').open('x') as stream:
        json.dump(envelope, stream, indent=2)
        stream.write('\n')
    return envelope


def read_record(output: Path) -> dict:
    value = read_json(output / 'record.json')
    if (set(value) != {'schema_version', 'kind', 'body', 'sha256'}
            or type(value['schema_version']) is not int or value['schema_version'] != 1
            or value['kind'] != 'sdk-qualification-execution'
            or not isinstance(value['body'], dict) or identity(value['body']) != value['sha256']):
        raise ValueError(f'Invalid execution record: {output}')
    return value
