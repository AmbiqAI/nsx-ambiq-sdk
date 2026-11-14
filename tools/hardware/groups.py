"""Private coordination of nested worker groups on the same Linux host."""
import json
import os
from pathlib import Path
import tempfile
import uuid

# Internal child context, not a caller configuration knob. A nested invocation
# registers its group with its ancestors so outer cancellation can reap it too.
_CONTEXT = '_NSX_WORKER_GROUP_CONTEXT'


def birth(pid: int) -> str | None:
    try:
        return Path(f'/proc/{pid}/stat').read_text().rsplit(')', 1)[1].split()[19]
    except FileNotFoundError:
        return None


class GroupScope:
    def __init__(self):
        inherited = os.environ.get(_CONTEXT)
        self.owner = None
        self.id = uuid.uuid4().hex
        if inherited is None:
            self.owner = tempfile.TemporaryDirectory(prefix='nsx-worker-groups-')
            self.root = Path(self.owner.name)
            self.ancestors = [self.id]
        else:
            value = json.loads(inherited)
            self.root = Path(value['root'])
            if not self.root.is_dir():
                raise RuntimeError('Enclosing worker scope has already closed')
            self.ancestors = [*value['ancestors'], self.id]
        self.record = self.root / (self.id + '.json')

    def environment(self, env: dict[str, str] | None) -> dict[str, str]:
        child = (os.environ if env is None else env).copy()
        child[_CONTEXT] = json.dumps({'root': str(self.root), 'ancestors': self.ancestors})
        return child

    def register(self, pid: int):
        temporary = self.record.with_suffix('.pending')
        temporary.write_text(json.dumps({'pid': pid, 'birth': birth(pid), 'ancestors': self.ancestors}))
        temporary.replace(self.record)

    def groups(self) -> set[int]:
        result = set()
        for path in self.root.glob('*.json'):
            try:
                value = json.loads(path.read_text())
                if self.id not in value['ancestors']:
                    continue
                pid = value['pid']
                current = birth(pid)
                # A missing leader can still have a live process group. A
                # reused leader PID, however, belongs to a different command.
                if current is None or current == value['birth']:
                    result.add(pid)
            except FileNotFoundError:
                continue  # A completed nested scope removed its entry.
        return result

    def close(self):
        self.record.unlink(missing_ok=True)
        if self.owner is not None:
            self.owner.cleanup()
