/********************************************************************
 Copyright (c) 2016, 2022-2024 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 *********************************************************************/
#ifndef __MSFORWARDER_H
#define __MSFORWARDER_H

#include "object.h"
#include "pthread.h"
#include "qlist.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct MinkSocket MinkSocket;

typedef struct MSForwarder {
  QNode node;
  int refs;
  int handle;
  pthread_mutex_t mutex;
  QList qlCloseNotifier;
  MinkSocket *conn;
} MSForwarder;

int32_t MSForwarder_new(MinkSocket *conn, int handle, Object *objOut);
MSForwarder *MSForwarderFromObject(Object obj);
/**
  Detach this MSForwarder from the remote handle and free its memory.
  Do not use this MSForwarder after calling detach.
**/
int32_t MSForwarder_detach(MSForwarder *me);

#if defined (__cplusplus)
}
#endif

#endif // __MSFORWARDER_H
