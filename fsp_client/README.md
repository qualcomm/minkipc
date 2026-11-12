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

## Factory Safe Provisioning

Factory Safe Provisioning (FSP) is how a device gets the per-device secrets
it needs for DRM and content-protection features — Widevine, HDCP, Keymaster,
and OEM "UIE" keys — injected securely at manufacturing time, without the
key material ever being exposed outside of QTEE.

Each of these use-cases is backed by a public/private key pair bound to the
device:

- The **private key** never leaves secure storage. It's provisioned once,
  at the factory, and used from then on only inside QTEE to answer
  challenges (e.g. decrypt DRM content, respond to an HDCP handshake).
- The **public key** can be read back out and handed to whichever backend
  needs to verify or provision the device (e.g. a CA issuing a certificate
  for that device's key, or a license server registering it).

The private key material itself is delivered to the factory line as a
**keybox**: a vendor-supplied blob tied to a specific feature (Widevine,
HDCP1/2, Keymaster, UIE key 0/1 — see `idl/IFSPApp.idl`), optionally
encrypted for transport, and provisioned onto the device exactly once per
unit via this client.

`fsp_client` gives you command-line access to both halves of that
lifecycle: read the public key of an already-provisioned feature to hand
upstream, or provision a feature from a keybox file during manufacturing/
bring-up. See [Usage](#usage) above for the CLI surface, and the client
source for implementation details.

## Build

Gated by the top-level `BUILD_FSP_CLIENT` CMake option (default `ON`). The TA
directory can be changed at build time with `-DTA_DIR=<path>`.

```
cmake .. -DBUILD_FSP_CLIENT=ON
cmake --build . --target fsp_client
```
