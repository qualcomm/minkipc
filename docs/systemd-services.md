# systemd Services

MinkIPC provides systemd services to run the QTEE supplicant and prepare its secure file system storage.

## Qteesupplicant Service

`qteesupplicant.service` runs `qtee_supplicant`, which loads listener libraries to handle requests from QTEE. It requires a QCOM-TEE device at `/dev/tee0` and the listener libraries enabled at build time.

Start `sfsconfig.service` before the supplicant to prepare the storage directories.

## Sfsconfig Service

`sfsconfig.service` runs `sfs_config` to create the following directories and set their ownership and permissions:

- `/var/lib/tee/qtee_supplicant`
- `/var/lib/qtee_supplicant/vendor/tzstorage`

This service runs once and remains `active (exited)` after successful completion.

## Build Instructions

Follow the [MinkIPC build instructions](../README.md#build-instructions). Set `-DSYSTEMD_UNIT_DIR=/usr/lib/systemd/system` to generate and install the service files, adjusting the path for your distribution.

Use `-DDISTRO_USER=<user>` and `-DDISTRO_GROUP=<group>` to configure the supplicant account and storage directory ownership. Both default to `root`; custom accounts and groups must exist on the target system.

## Usage

After installation, enable and start the services on the target system:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now sfsconfig.service
sudo systemctl enable --now qteesupplicant.service
```

Check service status and logs:

```sh
systemctl status sfsconfig.service qteesupplicant.service
journalctl -b -u sfsconfig.service -u qteesupplicant.service
```

Open the service documentation:

```sh
systemctl help qteesupplicant.service
systemctl help sfsconfig.service
```
