/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#include "ObjectTable.h"

#define GENERIC_HANDLE            0

bool ObjectTable_isCleaned(ObjectTable *me)
{
  bool state = false;

  vm_osal_mutex_lock(&me->mutex);
  if (1 == me->objectsCount) {
    state = true;
  }
  vm_osal_mutex_unlock(&me->mutex);

  return state;
}

// actual step when adding generic object to objectTable
// the mutex is acquired and released in upper invocation for better performance
static int32_t ObjectTable_add(ObjectTable *me, Object obj, int32_t n)
{
  if (Object_isNull(me->objects[n])) {
    me->objects[n] = obj;
    me->objectsCount++;
    Object_retain(obj);
    return Object_OK;

  } else {
    return Object_ERROR;
  }
}

// Add a generic object to the table, assigning it a handle.
// On success, return the handle.
// On failure, return Object_ERROR.
int32_t ObjectTable_addObject(ObjectTable *me, Object obj)
{
  vm_osal_mutex_lock(&me->mutex);
  for (int32_t n = GENERIC_HANDLE; n < me->objectsLen; ++n) {
    if (Object_OK == ObjectTable_add(me, obj, n)) {
      vm_osal_mutex_unlock(&me->mutex);
      LOG_TRACE("add obj to objectTable = %p, me->objects = %p, obj = %p, n = %d\n",
               me, me->objects, &obj, n);
      return n;
    }
  }
  vm_osal_mutex_unlock(&me->mutex);

  // Object_ERROR is 1 which can be a valid index of some entry in OT
  return -1;
}

// Return the kernel object to which an outbound object forwards invokes.
// If there is no object at that slot, return Object_NULL.  Otherwise, the
// returned object has been retained, and the caller is repsonsible for
// releasing it.
Object ObjectTable_recoverObject(ObjectTable *me, int32_t h)
{
  vm_osal_mutex_lock(&me->mutex);
  if (h >= 0 && h < (int) me->objectsLen) {
    Object o = me->objects[h];
    if (!Object_isNull(o)) {
      Object_retain(o);
      vm_osal_mutex_unlock(&me->mutex);
      LOG_TRACE("recover obj from ObjectTable = %p, h = %d, objOut = %p\n", me,
               h, &o);
      return o;
    }
  }
  vm_osal_mutex_unlock(&me->mutex);

  return Object_NULL;
}

// actual step when releasing generic object from objectTable
// the mutex is acquired and released in upper invocation for better performance
static int32_t ObjectTable_close(ObjectTable *me, int32_t h)
{
  Object o = Object_NULL;

  vm_osal_mutex_lock(&me->mutex);
  if (Object_isNull(me->objects[h])) {
    vm_osal_mutex_unlock(&me->mutex);
    return Object_ERROR;
  }

  LOG_TRACE("close objectTable = %p, me->objects = %p, h = %d\n", me, me->objects, h);

  o = me->objects[h];
  me->objects[h] = Object_NULL;
  me->objectsCount--;
  vm_osal_mutex_unlock(&me->mutex);

  Object_release(o);

  return Object_OK;
}

// release specific object from objectTable
int32_t ObjectTable_releaseHandle(ObjectTable *me, int32_t h)
{
  int res = Object_ERROR;

  if (h >= GENERIC_HANDLE && h < me->objectsLen) {
    res = ObjectTable_close(me, h);
  } else {
    LOG_ERR("objectTable = %p, handle %d is illegal\n", me, h);
  }

  return res;
}

// release all objects from objectTable
void ObjectTable_closeAllHandles(ObjectTable *me)
{
  if (NULL == me->objects) {
    return;
  }

  for (int h = GENERIC_HANDLE; h < me->objectsLen; ++h) {
    ObjectTable_close(me, h);
  }
}

void ObjectTable_destruct(ObjectTable *me)
{
  int ret = Object_ERROR;

  for (int h = 0; h < me->objectsLen; h++) {
    ret = ObjectTable_close(me, h);
    if (Object_isOK(ret)) {
      LOG_ERR("objectTable = %p, possible object leak on handle %d\n", me, h);
#ifdef OFFTARGET
      abort();
#endif
    }
  }

  me->objectsLen = 0;
  me->objectsCount = 0;
  HEAP_FREE_PTR(me->objects);

  vm_osal_mutex_deinit(&me->mutex);
}

int32_t ObjectTable_construct(ObjectTable *me, uint32_t size)
{
  me->objects = HEAP_ZALLOC_ARRAY(Object, size);
  if (me->objects == NULL) {
    me->objectsLen = 0;
    return Object_ERROR;
  }
  me->objectsLen = size;
  me->objectsCount = 0;
  vm_osal_mutex_init(&me->mutex, NULL);

  return Object_OK;
}
