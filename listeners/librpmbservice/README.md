# librpmbservice — RPMB Listener for MinkIPC

## Overview

`librpmbservice` is a shared-library listener that bridges QTEE (Qualcomm
Trusted Execution Environment) applications to the hardware RPMB (Replay
Protected Memory Block) partition on eMMC and UFS storage devices.

It registers with the QTEE supplicant as a CBO listener under service ID
`0x2000`.  When a QTEE application issues an RPMB command via MinkIPC, the
supplicant dispatches it to this library, which performs the corresponding
authenticated ioctl sequence against the kernel RPMB device node and returns
the result frame to the secure world.

## Architecture

```
QTEE Application
      │  MinkIPC (SMCInvoke)
      ▼
QTEE Supplicant (qtee_supplicant)
      │  CBO listener dispatch
      ▼
librpmbservice.so          ← this library
  ├── rpmb_service.c       MinkIPC registration + command dispatch
  ├── rpmb.c               Device detection, driver table, read/write API
  ├── rpmb_emmc.c          eMMC RPMB ioctl implementation
  └── rpmb_ufs.c           UFS RPMB BSG implementation
      │
      ▼
Kernel RPMB device node
  /dev/mmcblk0rpmb  (eMMC)
  /dev/bsg/ufs-bsg  (UFS)
```

### Device detection

At service startup `rpmb_init()` walks a static driver table in priority
order (UFS first, eMMC second).  Each driver's `probe()` function checks
whether its device node is present.  The first driver that succeeds has its
`init()` called; its ops pointer is stored as the single active device for
the lifetime of the service.

If no device is found the service continues running but returns an error
status to the secure world on every read/write request — it does not crash
or fall back to a wrong device type.

Adding support for a new storage type requires only a new row in
`rpmb_driver_table[]` in `rpmb.c`; no other file needs to change.

### eMMC driver (`rpmb_emmc.c`)

Device parameters are read from sysfs at init time rather than hardcoded:

| sysfs attribute | Purpose |
|---|---|
| `/sys/block/mmcblk0/device/raw_rpmb_size_mult` | RPMB partition size |
| `/sys/block/mmcblk0/device/rel_sectors` | Reliable-write frame count |
| `/sys/block/mmcblk0/device/enhanced_rpmb_supported` | eMMC 5.1 enhanced RPMB |

The ioctl sequences follow the JEDEC eMMC 4.5 specification and the
Qualcomm reference implementation:

- **Read** — 2-command `MMC_IOC_MULTI_CMD`: `CMD25` (send request frame) +
  `CMD18` (read response frames).
- **Write** — 3-command `MMC_IOC_MULTI_CMD` per MAC batch: `CMD25` with
  `SECURE_WRITE` flag (data frames) + `CMD25` (result-read request) +
  `CMD18` (result frame).  The result frame is checked after each
  transaction; the loop stops on any non-zero result code.

### UFS driver (`rpmb_ufs.c`)

Uses the UFS BSG interface (`/dev/bsg/ufs-bsg` or `/dev/bsg/ufs-bsg0`) with
SCSI Security Protocol Out/In commands.

## Source files

| File | Role |
|---|---|
| `rpmb_service.c` | MinkIPC lifecycle (`init`/`deinit`) and `smci_dispatch` |
| `rpmb.c` | Driver table, `rpmb_init/read/write`, wakelock |
| `rpmb_emmc.c` | eMMC probe / init / read / write / exit |
| `rpmb_ufs.c` | UFS probe / init / read / write / exit |
| `rpmb_logging.c` | Syslog-based logging with optional console output |
| `rpmb.h` | Public API: `rpmb_dev_ops_t`, `rpmb_frame`, `rpmb_init_info_t` |
| `rpmb_private.h` | Internal: `struct rpmb_stats`, driver function prototypes |
| `rpmb_logging.h` | `RPMB_LOG_{ERROR,WARN,INFO,DEBUG}` macros |
| `rpmb_ufs.h` | UFS BSG constants and structures |

## MinkIPC command protocol

The service handles four command IDs dispatched by the secure world:

| Command ID | Value | Description |
|---|---|---|
| `TZ_CM_CMD_RPMB_INIT` | `0x101` | Device init — returns size, `rel_wr_count`, and (version 2+) `dev_type` |
| `TZ_CM_CMD_RPMB_READ` | `0x102` | Authenticated read |
| `TZ_CM_CMD_RPMB_WRITE` | `0x103` | Authenticated write |
| `TZ_CM_CMD_RPMB_PARTITION` | `0x104` | Partition table query |

All commands share a single 20 KB shared-memory buffer
(`TZ_MAX_BUF_LEN = 20040` bytes).  The request and response are packed
into the same buffer; the response header (`tz_rpmb_rw_res_t`) is written
at offset 0 and the RPMB frame payload follows immediately after.

### Key wire structures

```c
typedef struct {
    uint32_t cmd_id;
    uint32_t version;
    int32_t  status;
    uint32_t num_sectors;    /* RPMB partition size in 512-byte sectors */
    uint32_t rel_wr_count;   /* Frames per authenticated write operation */
} tz_sd_device_init_res_t;

typedef struct {
    uint32_t cmd_id;
    uint32_t version;
    int32_t  status;
    uint32_t num_sectors;
    uint32_t rel_wr_count;
    uint32_t dev_type;       /* device_id_type of the detected device (eMMC/UFS) */
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t reserved4;
} tz_sd_device_init_res_v02_t;

/* Read / write request */
typedef struct {
    uint32_t cmd_id;
    uint32_t num_sectors;       /* Total frames to transfer */
    uint32_t req_buff_len;      /* Length of RPMB frame data */
    uint32_t req_buff_offset;   /* Offset of RPMB frames within buffer */
    uint32_t version;
    uint32_t rel_wr_count;      /* MAC batch size (write only) */
} tz_rpmb_rw_req_t;
```

`rel_wr_count` in the write request is the number of frames over which the
secure world computed a single HMAC.  The eMMC driver sends exactly that
many frames per `MMC_IOC_MULTI_CMD` call so the write counter increments
only once per batch.

## Build

```bash
cd <workspace>/minkipc_code
mkdir -p build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../CMakeToolchain.txt
make rpmbservice
```

To enable console logging during development (prints to stderr in addition
to syslog), uncomment in `CMakeLists.txt`:

```cmake
add_definitions(-DRPMB_DEBUG_CONSOLE_OUTPUT)
```

The installed artefact is `librpmbservice.so.1.0.0` with the symlinks
`librpmbservice.so.1` and `librpmbservice.so`.

## Runtime requirements

| Resource | Path | Notes |
|---|---|---|
| eMMC RPMB device | `/dev/mmcblk0rpmb` | char device (kernel ≥ 4.19) |
| eMMC RPMB device (legacy) | `/dev/block/mmcblk0rpmb` | block device (kernel < 4.19) |
| UFS BSG device | `/dev/bsg/ufs-bsg` or `/dev/bsg/ufs-bsg0` | |
| Wakelock | `/sys/power/wake_lock` | optional; non-fatal if absent |
| eMMC sysfs | `/sys/block/mmcblk0/device/` | `raw_rpmb_size_mult`, `rel_sectors` |

## Logging

Logs are written to syslog under the tag `[RPMB]`.  The log level can be
controlled at runtime via the `RPMB_LOG_LEVEL` environment variable
(integer, `3`=ERROR … `7`=DEBUG).  Set `RPMB_LOG_CONSOLE=1` to mirror
output to stderr without rebuilding.

## References

- JEDEC eMMC 4.5 / 5.1 specification (JESD84-B45 / JESD84-B51)
- JEDEC UFS 2.0 specification (JESD220)
- Linux kernel `Documentation/driver-api/mmc/`
- Qualcomm reference: `securemsm/tzservices/rpmb/`
