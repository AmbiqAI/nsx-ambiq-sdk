# Combined I2C transactions

`nsx_i2c_write_read_combined` supplies the repeated-START operation needed by
portable sensor endpoints. It is additive: `nsx_i2c_write_read` and
`nsx_i2c_transfer` retain their existing STOP-separated semantics.

## Mapping to the IOM

The wrapper packs prefix bytes into the instruction/offset field in MSB-first
order, sets its length, selects RX, and submits one zero-initialized
`am_hal_iom_transfer_t` with `bContinue=false`. The hardware performs:

```text
START -> address+W -> prefix -> repeated START -> address+R -> data -> NACK -> STOP
```

This uses the IOM's native read-with-offset operation, also used by existing
`nsx_i2c_read_sequential_regs`. It does not require two calls with a held-bus
continuation, nor a synthetic cleanup transaction after a failed first phase.
No vendor source is modified.

Evidence for the mapping:

- Ambiq Apollo3/Blue datasheet DS-A3-1p3p0 (February 2024), section 8.14.9,
  Figure 30, page 295: read with address offset uses RESTART between offset and
  read. Section 8.14.12, page 296, explicitly says the interface is held between
  those phases independently of CONT; clearing CONT permits the final STOP.
- The staged CMSIS IOM OFFSETHI description specifies MSB-first offset bytes.
- The promoted HAL headers expose `AM_HAL_IOM_MAX_OFFSETSIZE` (3 or 5) and
  `AM_HAL_IOM_MAX_TXNSIZE_I2C` (512 or 4095). The wrapper uses these definitions
  rather than silently assuming every chip has the same capabilities.
- AmbiqSuite source revision `caaf5af86087881647f56c70646c748d40c86e23`, matching
  the non-atomiq110 provider baseline, maps `ui32Instr`/`ui64Instr` and
  `ui32InstrLen` to OFFSETLO/OFFSETHI/OFFSETCNT in its blocking RX operation.

Do not extend this API to long prefixes by splitting transactions. A future
arbitrary-length chained operation needs its own supported capability, exclusive
controller ownership, cancellation and recovery contract. Short prefixes cover
ordinary sensor register reads and permit the current single-command mapping.

## Errors, alignment and completion

Validation runs before calling HAL even when HAL API validation is disabled:
non-null initialized handle, I2C mode, seven-bit address, non-null buffers,
nonzero in-range lengths and word-aligned RX storage. Prefix storage is read
byte-wise and may be unaligned. HAL's blocking RX path copies a short final FIFO
word byte-by-byte, so RX capacity need not be rounded up beyond the requested
length. The shared sensor adapter can still use aligned padded scratch buffers.

All unused descriptor fields are zero. Native HAL statuses are returned intact;
this lets the endpoint retain timeout, NACK and arbitration diagnostics. There
is one HAL call per valid request, with no retry, split or software continuation.
Partial RX data and device side effects on failure are explicitly permitted.

The inspected HAL performs bounded pending/idle checks before issuing a command,
polls RX FIFO with `AM_HAL_IOM_MAX_BLOCKING_WAIT`, checks final completion and
runs its submodule/FIFO recovery on an in-flight error. Early HAL failures can
return before that recovery path. The wrapper does not promise that every failed
call leaves the bus usable, and does not reset other controller users itself.
Controller/device recovery after a persistent bus fault belongs to the caller.

The baseline HAL defines `AM_HAL_IOM_MAX_BLOCKING_WAIT` as 500000 microseconds.
This is a per-wait escape limit, **not a 500 ms end-to-end transaction timeout**:
a multiword read may execute several FIFO waits, and pending/idle checks,
recovery, scheduling and interrupts add time. Under serialized ownership the
finite transfer length bounds the number of FIFO iterations. There is no
caller-selected deadline or ability to preempt the synchronous HAL. A strict
wall-clock deadline and queue-backed asynchronous submission remain follow-up
controller work, not sensor-core promises.

## Validation

`python -m pytest tests/test_i2c_api.py` compiles the real wrapper and public
header against a recording HAL boundary for both instruction layouts, with
validation enabled and disabled. It checks prefix wire order, one RX descriptor,
final termination, zeroed control fields, limit boundaries, malformed inputs,
unaligned prefix/RX rules, native error passthrough, and absence of retries. It
also checks that the old helper still submits separate TX/RX operations.

The recording stub proves wrapper mapping, not electrical bus behavior or HAL
recovery. Local GCC builds compile the wrapper and C++ header consumers against
real staged HAL/BSP headers for all eight declared I2C SoCs. Full SDK contract
tests run separately. No sensor board or logic analyzer has been used.

Hardware qualification must capture START/address+W/prefix/repeated
START/address+R/data/NACK/STOP on each claimed IOM family, including multibyte
prefixes and odd read lengths, then exercise NACK, clock-stall/arbitration
failure and subsequent recovery. Build evidence does not replace those checks.
