# nsx-gpio

`nsx-gpio` provides GPIO configuration, level I/O, and edge-triggered pin interrupts for AmbiqSuite-backed NSX targets.

Pins can be configured as disabled, input, output, or output-with-read, with an initial level for outputs. Input pins may register a rising, falling, or both-edge callback; pin interrupts are dispatched through `nsx-interrupt`, which owns the GPIO bank vectors. Each fired pin is fanned out to its registered callback with its pin number and an opaque context.

The API offers `init`, `write`, `read`, and `toggle`, plus per-pin interrupt `enable`, `disable`, and `clear`.

Public interfaces live in `includes-api/`.
