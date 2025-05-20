/********************************************************************
 Copyright (c) 2024 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 *********************************************************************/
#include "VmOsal.h"
#include "CredentialsAdapter.h"
#include "Heap.h"
#include "IOpener.h"
#include "IOpener_invoke.h"
#include "IModule.h"
#include "IModule_invoke.h"
#include "LinkCredentials.h"
#include "object.h"
#include "Utils.h"
#include "xtzdCredentials.h"

typedef struct
{
  int32_t refs;
  Object endpoint;
  Object linkCred;
} LocalCredAdapter;

typedef struct
{
  int32_t refs;
  Object endpoint;
  Object linkCred;
} RemoteCredAdapter;

/**************************************************************************
* Chance are there race condition occurs between retain() and release()
* Extra check are introduced to mitigate possible impact. But it cannot
* eliminate the risk because me would turn to be NULL during the 2 check
* Long way to overcome it
**************************************************************************/
static inline
int32_t LocalCredAdapter_retain(LocalCredAdapter *me)
{
  if (NULL != me && vm_osal_atomic_add(&me->refs, 0) > 0) {
    vm_osal_atomic_add(&me->refs, 1);
    return Object_OK;
  }

  return Object_ERROR;
}

static inline
int32_t LocalCredAdapter_release(LocalCredAdapter *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    LOG_TRACE("released openerForwarder = %p, endpoint = %p, linkCred = %p\n", me,
               &me->endpoint, &me->linkCred);
    Object_ASSIGN_NULL(me->endpoint);
    Object_ASSIGN_NULL(me->linkCred);
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static inline
int32_t LocalCredAdapter_open(LocalCredAdapter *me, uint32_t id, Object *obj)
{
  LOG_TRACE("open openerForwarder = %p, endpoint = %p, linkCred = %p, id = %d, \
             objOut = %p\n", me, &me->endpoint, &me->linkCred, id, obj);
  return IModule_open(me->endpoint, id, me->linkCred, obj);
}

static
IOpener_DEFINE_INVOKE(LocalCredAdapter_invoke, LocalCredAdapter_, LocalCredAdapter*);

int32_t LocalCredAdapter_new(Object endpoint, Object credentials, Object *objOut)
{
  LocalCredAdapter *me = HEAP_ZALLOC_TYPE(LocalCredAdapter);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  Object_INIT(me->endpoint, endpoint);
  Object_INIT(me->linkCred, credentials);
  *objOut = (Object) {LocalCredAdapter_invoke, me};

  return Object_OK;
}

/**************************************************************************
* Chance are there race condition occurs between retain() and release()
* Extra check are introduced to mitigate possible impact. But it cannot
* eliminate the risk because me would turn to be NULL during the 2 check
* Long way to overcome it
**************************************************************************/
static inline
int32_t RemoteCredAdapter_retain(RemoteCredAdapter *me)
{
  if (NULL != me && vm_osal_atomic_add(&me->refs, 0) > 0) {
    vm_osal_atomic_add(&me->refs, 1);
    return Object_OK;
  }

  return Object_ERROR;
}

static inline
int32_t RemoteCredAdapter_release(RemoteCredAdapter *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    LOG_TRACE("released moduleForwarder = %p, linkCred = %p, endpoint = %p\n", me,
               &me->linkCred, &me->endpoint);
    Object_ASSIGN_NULL(me->linkCred);
    Object_ASSIGN_NULL(me->endpoint);
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static inline
int32_t RemoteCredAdapter_shutdown(RemoteCredAdapter *me)
{
  return Object_OK;
}

static
int32_t RemoteCredAdapter_open(RemoteCredAdapter *me, uint32_t id, Object credentials,
                           Object *objOut)
{
  int32_t res;
  Object credComposite = Object_NULL;

  if (Object_isOK(LinkCredComposite_new(me->linkCred, credentials, &credComposite))) {
    res = IModule_open(me->endpoint, id, credComposite, objOut);
    Object_ASSIGN_NULL(credComposite);
    LOG_TRACE("open moduleForwarder = %p, linkCred = %p, endpoint = %p, \
               objOut = %p\n", me, &me->linkCred, &me->endpoint, objOut);
    return res;
  }

  LOG_ERR("LinkCredentials_newFromCred failed, moduleForwarder = %p, \
           credentials = %p\n", me, &credentials);
  return Object_ERROR_UNAVAIL;
}

static
IModule_DEFINE_INVOKE(RemoteCredAdapter_invoke, RemoteCredAdapter_, RemoteCredAdapter*);

int32_t RemoteCredAdapter_new(Object endpoint, Object credentials, Object *objOut)
{
  RemoteCredAdapter *me = HEAP_ZALLOC_TYPE(RemoteCredAdapter);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  Object_INIT(me->endpoint, endpoint);
  Object_INIT(me->linkCred, credentials);
  *objOut = (Object){RemoteCredAdapter_invoke, me};

  return Object_OK;
}
