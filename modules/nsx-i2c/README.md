# nsx-i2c

`nsx-i2c` provides the NSX wrapper for Ambiq I2C and related
register-access helpers.

Purpose:
- expose the `nsx_i2c_*` API surface
- keep basic register-access helpers available for existing device code
- define the bus contract for sensor-specific modules

The module boundary is the bus API and register-helper layer.

## Combined write/read with repeated START

Use `nsx_i2c_write_read_combined` when a device requires its command/register
prefix and read to share one I2C transaction. The prefix is sent in byte order,
followed by repeated START and the read; STOP is sent only at the end. The
existing `nsx_i2c_write_read` remains a STOP-separated pair for compatibility.

```c
uint8_t command[] = {0xd0};
uint32_t rx_word = 0; /* Receive storage must be uint32_t-aligned. */
uint32_t status = nsx_i2c_write_read_combined(&bus, 0x76, command, sizeof(command),
                                           &rx_word, 1);
if (status == AM_HAL_STATUS_SUCCESS) {
    uint8_t chip_id = ((uint8_t *)&rx_word)[0];
    /* Use chip_id. */
}
```

Both lengths must be nonzero. `NSX_I2C_COMBINED_MAX_WRITE_SIZE` and
`NSX_I2C_COMBINED_MAX_READ_SIZE` expose the selected HAL's limits:

| IOM family | Maximum command prefix | Maximum read |
| --- | --- | --- |
| Apollo3 / Apollo3 Plus | 3 bytes | 512 bytes |
| Apollo4 Lite / Plus, Apollo330 Plus, Apollo510 / B / Lite | 5 bytes | 4095 bytes |

Longer commands are rejected before I/O; no implicit splitting or STOP-separated
fallback occurs. The address is unshifted and seven-bit. The receive buffer must
be word-aligned and writable for the requested length; the prefix needs only
byte alignment. The call is synchronous and preserves native HAL failure codes,
unlike the legacy helpers' success/error mapping.

The application must serialize all access to the same IOM, including queued
operations. This function owns no mutex and is not ISR-safe. HAL polling limits
and recovery govern completion; there is no per-call deadline. Failure may leave
partial receive data or device side effects. Do not automatically replay a
failed operation. See [implementation and validation notes](../../docs/i2c-combined-transfers.md).
