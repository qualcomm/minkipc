/********************************************************************
Copyright (c) 2021-2023 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
*********************************************************************/

#pragma once

/**
 * Hardcoded VM UUIDs, which in future we should be able to obtain
 * just by querying sockets.
 * */

#define VMUUID_MAX_SIZE 16


#define CLIENT_VMUID_HLOS 0x3c, 0xd4, 0x7b, 0x20, \
                          0xf1, 0x66, 0x5f, 0xe2, \
                          0x95, 0x21, 0x02, 0xee, \
                          0xdf, 0xc4, 0x35, 0x0a
#define CLIENT_VMUID_TUI  0x3f, 0x4d, 0x15, 0x4a, \
                          0x92, 0xd3, 0x54, 0xd0, \
                          0x92, 0x41, 0x08, 0xe3, \
                          0x4c, 0x8b, 0xc6, 0x70
#define CLIENT_VMUID_OEM  0x11, 0x9b, 0x0c, 0x65, \
                          0x8c, 0xe2, 0x1c, 0x5f, \
                          0x8a, 0x1d, 0x27, 0x4f, \
                          0x66, 0x30, 0xd1, 0x45
