/********************************************************************
 Copyright (c) 2016, 2022-2024 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
 *********************************************************************/
#include "VmOsal.h"
#include "check.h"
#include "cdefs.h"
#include "Heap.h"
#include "lxcom_sock.h"
#include "minksocket.h"
#include "memscpy.h"
#include "msforwarder.h"
#include "Utils.h"

#define GENERIC_HANDLE            0

static inline
void MSForwarder_delete(MSForwarder *me)
{
  LOG_TRACE("deleted msforwarder = %p, minksocket = %p\n", me, me->conn);

  MinkSocket_preDeleteForwarder(me->conn, me);
  vm_osal_mutex_deinit(&me->mutex);
  HEAP_FREE_PTR(me);
}

int32_t MSForwarder_detach(MSForwarder *me)
{
  int32_t handle = me->handle;

  MinkSocket_detachForwarder(me->conn, me);

  return handle;
}

static int32_t
MSForwarder_invoke(void *cxt, ObjectOp op, ObjectArg *args, ObjectCounts k)
{
  MSForwarder *me = (MSForwarder*) cxt;
  ObjectOp method = ObjectOp_methodID(op);

  if (ObjectOp_isLocal(op)) {
    switch (method) {
      case Object_OP_retain:
        if (me->refs > INT_MAX - 1) {
          LOG_ERR("Error, refs exceeds the max\n");
          return Object_ERROR;
        }
        vm_osal_atomic_add(&me->refs, 1);
        return Object_OK;

      case Object_OP_release:
        if (me->refs < 1) {
          LOG_ERR("Error, refs has been 0 before\n");
          return Object_ERROR;
        }
        if (vm_osal_atomic_add(&me->refs, -1) == 0) {
          MSForwarder_delete(me);
        }
        return Object_OK;

      case Object_OP_unwrapFd:
        if (k != ObjectCounts_pack(0, 1, 0, 0)) {
          break;
        }
        int fd = -1;
        memscpy(args[0].b.ptr, args[0].b.size, &fd, sizeof(fd));
        return Object_OK;
      }

    LOG_ERR("Mismatched operation\n");
    return Object_ERROR;
  }

  return MinkSocket_invoke(me->conn, me->handle, op, args, k);
}

MSForwarder *MSForwarderFromObject(Object obj)
{
  return (obj.invoke == MSForwarder_invoke ? (MSForwarder*) obj.context : NULL);
}

int32_t MSForwarder_new(MinkSocket *conn, int handle, Object *objOut)
{
  MSForwarder *me = HEAP_ZALLOC_REC(MSForwarder);
  if (!me) {
    LOG_ERR("Failed to allocate msforwarder\n");
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  me->handle = handle;
  me->conn = conn;
  QNode_construct(&me->node);

  if (MinkSocket_attachForwarder(conn, me)) {
    LOG_ERR("Failed to invoke MinkSocket_attachForwarder()\n");
    HEAP_FREE_PTR(me);
    return Object_ERROR;
  }

  vm_osal_mutex_init(&me->mutex, NULL);
  QList_construct(&me->qlCloseNotifier);
  *objOut = (Object) {MSForwarder_invoke, me};

  LOG_TRACE("constructed msforwarder = %p, handle = %d, minksocket = %p\n", me,
             me->handle, me->conn);
  return Object_OK;
}
