# nsx-i2c

`nsx-i2c` provides the NSX wrapper for Ambiq I2C and related
register-access helpers.

Purpose:
- expose the `nsx_i2c_*` API surface
- keep basic register-access helpers available for existing device code
- define the bus contract for sensor-specific modules

The module boundary is the bus API and register-helper layer.
