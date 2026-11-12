# fsp_client

CLI client for Factory Safe Provisioning (FSP). It loads the `fspapp` Trusted
Application into QTEE and uses its `IFSPApp` interface to read a device's
public key or to provision a key from a clear or encrypted keybox.

## Usage

```
fsp_client -a <app_name> -c <cmd_id> [-d <feature_id>] [-f <file>] [-e <0|1>]
```

| Option | Description |
|---|---|
| `-a, --app_name <name>` | Name of the secure app; its image is `<name>.mbn` |
| `-c, --cmd_id <id>` | Command: `1` get_public_key, `2` provision_key |
| `-d, --feature_id <id>` | Feature ID of the key (see `idl/IFSPApp.idl`); `provision_key` accepts `UIE_KEY_0_FEATURE_ID` and `UIE_KEY_1_FEATURE_ID` |
| `-f, --file <path>` | Keybox file, required for `provision_key` |
| `-e, --encrypted <0\|1>` | Keybox encryption status: `0` clear, `1` encrypted (default) |
| `-h, --help` | Show usage |

## How it works

`fsp_client` reads `<app_name>.mbn` from the TA directory (default
`/usr/lib/qtee-tas`) and pushes it to QTEE through `IAppLoader`
(`CAppLoader_UID`). It then obtains the TA's `IFSPApp` object from
`IAppController_getAppObject()`, invokes the requested command, and unloads
the TA with `IAppController_unload()` on exit.

For `provision_key`, the keybox is copied into a MinkCom memory object
(`MinkCom_getMemoryObject()`) that is passed to the TA by reference.

## Build

Gated by the top-level `BUILD_FSP_CLIENT` CMake option (default `ON`). The TA
directory can be changed at build time with `-DTA_DIR=<path>`.

```
cmake .. -DBUILD_FSP_CLIENT=ON
cmake --build . --target fsp_client
```
