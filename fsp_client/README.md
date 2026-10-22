# fsp_client

CLI client for Factory Safe Provisioning (FSP). Loads the `fspapp` Trusted
Application into QTEE and invokes its `IFSPApp` methods to retrieve a
device's public key or provision a key via an encrypted/clear keybox.

This is a MinkIPC port of a downstream `fsp_client`. The TA image is pushed
to QTEE at runtime via `IAppLoader_loadFromBuffer()` (see "TA loading" below)
rather than relying on QTEE auto-loading it from disk.

## Usage

```
fsp_client -a <app_name> -c <cmd_id> [-f <file>] [-e <0|1>] [-d <feature_id>]
```

| Option | Description |
|---|---|
| `-a, --app_name <name>` | Name of the secure app (its image is `<name>.mbn`) |
| `-c, --cmd_id <id>` | Command: `1` get_public_key, `2` provision_key, `3` verify_key, `4` get_public_key_v2, `5` provision_key_v2 |
| `-f, --file <path>` | Input file path (keybox), required for `provision_key_v2` |
| `-e, --encrypted <0\|1>` | File encryption status: `0` clear, `1` encrypted (default) |
| `-d, --feature_id <id>` | Feature ID for v2 operations (see `idl/IFSPApp.idl` for the full list; `provision_key_v2` only accepts `UIE_KEY_0_FEATURE_ID`/`UIE_KEY_1_FEATURE_ID`) |
| `-h, --help` | Show usage |

Commands `1`–`3` (`get_public_key`, `provision_key`, `verify_key`) are
carried over as unimplemented stubs from the downstream client — they print
"not supported for this target" and return an error. Only the `_v2` variants
(`4`, `5`) are functional in this port.

## TA loading

`fsp_client` searches, in order, for `<app_name>.mbn` in:

```
/vendor/firmware_mnt/image, /vendor/firmware, /vendor/etc/firmware,
/firmware/image, /system/etc/firmware, /data, /tmp
```

and pushes the first one found to QTEE via the `IAppLoader`/`IAppController`
interfaces (`CAppLoader_UID`), obtaining the TA's `IFSPApp` object through
`IAppController_getAppObject()`. The TA is unloaded via
`IAppController_unload()` on exit.

## Buffers

Input keybox data for `provision_key_v2` is copied into a MinkCom memory
object (`MinkCom_getMemoryObject()`/`MinkCom_getMemoryObjectInfo()`) and
passed to the TA by reference — there is no ION/fd-based buffer sharing in
this port (unlike the downstream client).

## Build

Gated by the top-level `BUILD_FSP_CLIENT` CMake option (default `ON`):

```
cmake .. -DBUILD_FSP_CLIENT=ON
cmake --build . --target fsp_client
```

## Tests

Host-only unit tests for the pure logic (file I/O, feature-ID validation —
no QTEE calls) are gated by `BUILD_UNITTEST` (default `OFF`):

```
cmake .. -DBUILD_FSP_CLIENT=ON -DBUILD_UNITTEST=ON
cmake --build . --target fsp_client_unittest
./fsp_client/fsp_client_unittest
```

## Known gaps

- `get_public_key`/`provision_key`/`verify_key` (cmd IDs 1–3) are
  unimplemented stubs, matching the downstream client's own state for this
  target.
