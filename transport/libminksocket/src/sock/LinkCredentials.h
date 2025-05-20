/********************************************************************
 Copyright (c) 2022-2024 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 *********************************************************************/
#ifndef __LINKCREDENTIALS_H
#define __LINKCREDENTIALS_H

#include "VmOsal.h"
#include "object.h"
#include "Types.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define ELOC_LOCAL   0
#define ELOC_REMOTE  1

typedef struct CredInfo {
  uint32_t pid;
  uint32_t uid;
  uint32_t gid;
} CredInfo;

int32_t LinkCred_new(CredInfo *info, int32_t lloc, int32_t ltype, Object *objOut);

int32_t LinkCredRemote_new(uint8_t const *vmuuid, size_t vmuuidLen, int32_t lloc,
                           int32_t ltype, Object *objOut);

int32_t LinkCredComposite_new(Object remoteCred, Object hubCred, Object *objOut);

/* deprecated interfaces will be removed after META absorbs new ones.
 * integration mechanism asks it.
 */
int32_t LinkCredentials_new(CredInfo *info, int32_t lloc, int32_t ltype, Object *objOut);
int32_t LinkCredentialsRemote_new(uint8_t const *vmuuid, size_t vmuuidLen, int32_t lloc,
                                  int32_t ltype, Object *objOut);
int32_t LinkCredentials_newFromCred(Object remoteCred, Object hubCred, Object *objOut);
#if defined (__cplusplus)
}
#endif

#endif //__LINKCREDENTIALS_H
