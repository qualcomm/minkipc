/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/

// ObjectTable
//
// An object table keeps track of local objects held by a remote domain,
// while will identify them with small non-negative integers.  The object
// table also counts reference to handles (not to be confused with the
// internal reference counting performed by each object).  When the handle
// reference count goes to zero, the object table slot is freed.
//
// The object table suports the following operations:
//
//  addObject()      Store an object in a slot in the table, yielding a handle.
//  recoverObject()  Recover an object, given a handle.
//  releaseHandle()  Decrement the reference count, and free the slot when it
//                   reaches zero.
//

#ifndef __OBJECTTABLE_H
#define __OBJECTTABLE_H

#include "VmOsal.h"
#include "object.h"
#include "Heap.h"
#include "fdwrapper.h"
#include "Utils.h"

typedef struct {
  // An array of objects held by a remote domain.
  Object *objects;
  // maximum number of the objects[]
  size_t objectsLen;
  // occupied number of the objects[]
  size_t objectsCount;
  //Mutex
  pthread_mutex_t mutex;
} ObjectTable;

bool ObjectTable_isCleaned(ObjectTable *me);

int32_t ObjectTable_addObject(ObjectTable *me, Object obj);

Object ObjectTable_recoverObject(ObjectTable *me, int32_t h);

int32_t ObjectTable_releaseHandle(ObjectTable *me, int32_t h);

void ObjectTable_closeAllHandles(ObjectTable *me);

void ObjectTable_destruct(ObjectTable *me);

int32_t ObjectTable_construct(ObjectTable *me, uint32_t size);

#endif // __OBJECTTABLE_H
