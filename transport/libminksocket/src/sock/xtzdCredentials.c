/***********************************************************************
* Copyright (c) 2022-2024 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
**********************************************************************/
#include "VmOsal.h"
#include "Heap.h"
#include "ICredentials_invoke.h"
#include "ICredentials.h"
#include "Utils.h"
#include "memscpy.h"
#include "object.h"
#include "string.h"
#include "xtzdCredentials.h"

struct XtzdCredentials
{
  int32_t refs;
  uint32_t callerId;
  Object linkCred;
};

/**************************************************************************
* Chance are there race condition occurs between retain() and release()
* Extra check are introduced to mitigate possible impact. But it cannot
* eliminate the risk because me would turn to be NULL during the 2 check
* Long way to overcome it
**************************************************************************/
static inline
int32_t XtzdCredentials_retain(XtzdCredentials *me)
{
  if (NULL != me && vm_osal_atomic_add(&me->refs, 0) > 0) {
    vm_osal_atomic_add(&me->refs, 1);
    return Object_OK;
  }

  return Object_ERROR;
}

static inline
int32_t XtzdCredentials_release(XtzdCredentials *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    LOG_TRACE("released xtzdCredentials = %p, linkCred = %p\n", me, &me->linkCred);
    Object_ASSIGN_NULL(me->linkCred);
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static
int32_t XtzdCredentials_getPropertyByIndex(XtzdCredentials *me, uint32_t index,
                                     void *name, size_t nameLen, size_t *nameLenOut,
                                     void *value, size_t valueLen, size_t *valueLenOut)
{
  return ICredentials_ERROR_NOT_FOUND;
}

static
int32_t XtzdCredentials_getValueByName(XtzdCredentials *me, const void *name, size_t nameLen,
                                       void *value, size_t valueLen, size_t *valueLenOut)
{
  LOG_TRACE("querying on xtzdCredentials = %p, name = %s\n", me, (const char *)name);
  if (8 == nameLen && 0 == strncmp((const char *)name, "callerId", nameLen)) {
    if (valueLen == sizeof(me->callerId)) {
      memscpy(value, valueLen, &(me->callerId), sizeof(me->callerId));
      *valueLenOut = valueLen;
      return Object_OK;
    } else {
      return Object_ERROR_SIZE_IN;
    }

  } else {
    return ICredentials_ERROR_NOT_FOUND;
  }
}

static
ICredentials_DEFINE_INVOKE(XtzdCredentials_invoke, XtzdCredentials_, XtzdCredentials*);

int32_t XtzdCredentials_newFromCBO(Object linkCred, Object *objOut)
{
  size_t valueLenOut;
  XtzdCredentials *me = NULL;

  if(Object_isNull(linkCred)) {
    LOG_ERR("linkCred error to be NULL\n");
    return Object_ERROR;
  }

  me = HEAP_ZALLOC_TYPE(XtzdCredentials);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  if (Object_OK != ICredentials_getValueByName(linkCred, "lpid", 4, &(me->callerId),
                                               sizeof(me->callerId), &valueLenOut)) {
    HEAP_FREE_PTR(me);
    LOG_ERR("ICredentials query lpid failed, xtzdCredentials = %p, \
             linkCred = %p\n", me, &linkCred);
    return Object_ERROR;
  }
  Object_INIT(me->linkCred, linkCred);
  *objOut = (Object) {XtzdCredentials_invoke, me};

  LOG_TRACE("constructed xtzdCredentials = %p, linkCred = %p, objOut = %p\n", me,
             &me->linkCred, objOut);
  return Object_OK;
}
