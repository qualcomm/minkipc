/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#ifndef __MINKSOCKET_H
#define __MINKSOCKET_H

#include "VmOsal.h"
#include "msforwarder.h"
#include "object.h"
#include "Types.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* -------- MinkSocket Version START --------
 *
 * MinkSocket version is represented as a 32-bit unsigned integer:
 *  - Upper 16 bits of the version represents a major version.
 *  - Lower 16 bits of the version represents a minor version.
 *
 * MinkSocket Version Histories
 *
 * | Major Version | Minor Version |                   Note                 |
 * |      1        |       0       |           Legacy MinkSocket            |
 * |      2        |       0       | Support indirect SharedMem of intra-VM |
 */

#define MINKSOCK_VER_UNINITIALIZED      (0U)
#define MINKSOCK_VER_LEGACY             ((1U << 16) + 0U)
#define MINKSOCK_VER_INDIRECT_MEM       ((2U << 16) + 0U)

#define MAJOR_VER_LOCAL                 (2U)
#define MINOR_VER_LOCAL                 (0U)
#define MINKSOCK_VER_LOCAL              ((MAJOR_VER_LOCAL << 16) + MINOR_VER_LOCAL)

#define GET_MAJOR_VER(version)    ((version) >> 16)
#define GET_MINOR_VER(version)    ((version) & ((1U << 16) - 1U))

#define GENERIC_HANDLE            0

/* -------- MinkSocket Version END -------- */

typedef struct MinkSocket MinkSocket;
typedef struct SockAgnostic SockAgnostic;

int32_t MinkSocket_attachForwarder(MinkSocket *me, MSForwarder *msFwd);
int32_t MinkSocket_detachForwarder(MinkSocket *me, MSForwarder *msFwd);
int32_t MinkSocket_detachForwarderAll(MinkSocket *me);
void MinkSocket_preDeleteForwarder(MinkSocket *me, MSForwarder *msFwd);
int32_t MinkSocket_initCredentials(MinkSocket *me, bool bModule);
int32_t MinkSocket_initObjectTable(MinkSocket *me, Object endpoint, bool workForModule);
MinkSocket *MinkSocket_new_internal(SockAgnostic *inSockAgn, Object endpoint);
MinkSocket *MinkSocket_new(Object endpoint, int32_t sockType, int32_t sock);
void MinkSocket_retain(MinkSocket *me);
void MinkSocket_release(MinkSocket *me);
int MinkSocket_detach(MinkSocket *me);
/** MinkSocket_attachObject attempts to attach an object
    with a specified handle much like MinkIPC_Connect does with an endpoint.
 **/
int32_t MinkSocket_attachObject(MinkSocket *me, int handle, Object *obj);
int MinkSocket_detachObject(Object *obj);
int32_t MinkSocket_invoke(MinkSocket *me, int32_t h,
                  ObjectOp op, ObjectArg *args, ObjectCounts k);
int32_t MinkSocket_sendClose(MinkSocket *me, int handle);
int32_t MinkSocket_handleConnected(MinkSocket *me);
void MinkSocket_start(MinkSocket *me, int sock);
void MinkSocket_close(MinkSocket *me, int32_t err);
void MinkSocket_delete(MinkSocket *me);
bool MinkSocket_isAlive(MinkSocket *me);
bool MinkSocket_isRemote(MinkSocket *me);
int32_t MinkSocket_getSockAgnostic(MinkSocket *me, SockAgnostic **outSockAgn);

int32_t MinkSocket_getVersion(MinkSocket *me, uint32_t *ackVersion);
int32_t MinkSocket_setVersion(MinkSocket *me, uint32_t ackVersion);
int32_t MinkSocket_syncCtlInfo(MinkSocket *me);

#if defined (__cplusplus)
}
#endif

#endif //__MINKSOCKET_H
