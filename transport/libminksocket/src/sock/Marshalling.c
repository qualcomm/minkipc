/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#include "cdefs.h"
#include "check.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "memscpy.h"
#include "msforwarder.h"
#include "Marshalling.h"
#include "Utils.h"
#include "Profiling.h"

/* flags for marshalling */
#define LXCOM_NULL_OBJECT               0
#define LXCOM_CALLER_OBJECT             1
#define LXCOM_CALLEE_OBJECT             2

#define NULL_OBJECT_HANDLE      UINT16_MAX
#define INVALID_OBJECT_HANDLE   (UINT16_MAX - 1)

#define MARSHAL_ERROR_MISMATCH (-1000)

static uint8_t PadBuf[8] = {0xF};

//return how much to add for the alignment
#define PADDED(x)     ({   \
  size_t sizeAligned = 0;  \
  if (0 != x) {            \
    sizeAligned = ((size_t)((x) + (((uint64_t)(~(x)) + 1) & (LXCOM_MSG_ALIGNMENT - 1)))); \
  }   \
  sizeAligned; })

#define ObjectCounts_numObjects(k)  (ObjectCounts_numOI(k) + \
                                     ObjectCounts_numOO(k))

#define ObjectCounts_indexObjects(k) \
  ObjectCounts_indexOI(k)

#define ObjectCounts_indexBUFFERS(k) \
  ObjectCounts_indexBI(k)

#define ObjectCounts_numBUFFERS(k) \
  (ObjectCounts_numBI(k) + ObjectCounts_numBO(k))

#define ObjectCounts_numIn(k) \
  (ObjectCounts_numBUFFERS(k) + ObjectCounts_numOI(k))

#define ObjectCounts_numOut(k) \
  (ObjectCounts_numBO(k) + ObjectCounts_numOO(k))

#define Sizeof_invReq(k, extraOI) \
  (c_offsetof(lxcom_inv_req, a) + (ObjectCounts_numIn(k) + extraOI) * sizeof(lxcom_arg))

#define Sizeof_invSucc(k, extraOO) \
  (c_offsetof(lxcom_inv_succ, a) + (ObjectCounts_numOut(k) + extraOO) * sizeof(lxcom_arg))

#define Sizeof_dataInput(args, k, idxNow, sizeOnWire)    ({     \
  size_t __sizeExtend = PADDED((sizeOnWire));                   \
  CONTINUE_ARGS(__ii, (idxNow), k, BI) {                        \
    __sizeExtend += PADDED(args[__ii].b.size);                  \
  }                                                             \
  __sizeExtend; })

#define Sizeof_dataOutput(args, k, idxNow, sizeOnWire)    ({    \
  size_t __sizeExtend = PADDED((sizeOnWire));                   \
  CONTINUE_ARGS(__ii, (idxNow), k, BO) {                        \
    __sizeExtend += PADDED(args[__ii].b.size);                  \
  }                                                             \
  __sizeExtend; })

#define CHECK_MAX_ARGS(k, dir, numExtra)                           \
  do {                                                             \
    int numArgs = ObjectCounts_num##dir(k);                        \
    int numTotalArgs = numArgs + numExtraArgs;                     \
    if (numTotalArgs > LXCOM_MAX_ARGS) {                           \
      LOG_ERR("Too many args: %d (args : %d, extra args: %d)\n",   \
        (uint32_t)numTotalArgs, (uint32_t)numArgs,                 \
        (uint32_t)numExtraArgs);                                   \
      return Object_ERROR_MAXARGS;                                 \
    }                                                              \
  } while(0)

// To avoid potential risk on present platform, we just do simple check.
// TODO: we need to check it with ObjectCounts k in next generation.
#define CHECK_OBJ_INDEX_CLEAN(i)                              \
  do {                                                        \
    if ((i) >= LXCOM_MAX_ARGS) {                              \
      LOG_ERR("Out of index: index = %d\n", (uint32_t)(i));   \
      ERR_CLEAN(Object_ERROR_MAXARGS);                        \
    }                                                         \
  } while (0)

// To avoid potential risk on present platform, we just do simple check.
// TODO: we need to check it with ObjectCounts k in next generation.
#define CHECK_OBJ_INDEX_RETURN(i)                             \
  do {                                                        \
    if ((i) >= LXCOM_MAX_ARGS) {                              \
      LOG_ERR("Out of index: index = %d\n", (uint32_t)(i));   \
      return Object_ERROR_MAXARGS;                            \
    }                                                         \
  } while (0)

#define CONTINUE_OR_RETURN(func, caseStr)   ({                     \
  err = (func);                                                    \
  if (Object_isOK(err)) {                                          \
    continue;                                                      \
  } else if(MARSHAL_ERROR_MISMATCH != err) {                       \
    LOG_ERR("Failed to marshal %s with err=%d.\n", caseStr, err);  \
    return err;                                                    \
  } else {                                                         \
    /*go on next attempt on other scenario*/                       \
  }  })

#define CONTINUE_OR_CLEANUP(func, caseStr)   ({                    \
  err = (func);                                                    \
  if (Object_isOK(err)) {                                          \
    continue;                                                      \
  } else if(MARSHAL_ERROR_MISMATCH != err) {                       \
    LOG_ERR("Failed to marshal %s with err=%d.\n", caseStr, err);  \
    goto cleanup;                                                  \
  } else {                                                         \
    /*go on next attempt on other scenario*/                       \
  }  })

static
void Flatdata_dump(const void *data, size_t size)
{
  const char *byte = (char *)data;

  for (size_t i = 0; i < size; i++) {
    for (int j = 7; j >= 0; j--) {
      printf("%c", (byte[i] & (1 << j)) ? '1' : '0');
    }
    printf(" ");
  }
  printf("\n");
}

/* Marshalling scenario for Object_NULL.
 */
static inline
int32_t MarshalOut_nullObject(Object obj,lxcom_arg *args, int32_t pos)
{
  if (Object_isNull(obj)) {
    args[pos].o.flags = LXCOM_NULL_OBJECT;
    args[pos].o.handle = NULL_OBJECT_HANDLE;
    return Object_OK;
  }

  return MARSHAL_ERROR_MISMATCH;
}

static inline
int32_t MarshalIn_nullObject(lxcom_arg *args, Object *obj, int32_t pos)
{
  uint16_t flags = args[pos].o.flags;
  //uint16_t handle = args[pos].o.handle;

  if (LXCOM_NULL_OBJECT == flags) {
    *obj = Object_NULL;
    return Object_OK;
  }

  return MARSHAL_ERROR_MISMATCH;
}

static inline
int32_t MarshalOut_caller_nullObject(Object obj,lxcom_arg *args, int32_t pos)
{
  return MarshalOut_nullObject(obj, args, pos);
}

static inline
int32_t MarshalOut_callee_nullObject(Object obj,lxcom_arg *args, int32_t pos)
{
  return MarshalOut_nullObject(obj, args, pos);
}

static inline
int32_t MarshalIn_callee_nullObject(lxcom_arg *args, Object *obj, int32_t pos)
{
  return MarshalIn_nullObject(args, obj, pos);
}

static inline
int32_t MarshalIn_caller_nullObject(lxcom_arg *args, Object *obj, int32_t pos)
{
  return MarshalIn_nullObject(args, obj, pos);
}

static inline
int32_t MarshalOut_msforwarder(MinkSocket *minksock, Object obj, lxcom_arg *args,
                               int32_t pos, int32_t objFlag)
{
  MSForwarder *msf = MSForwarderFromObject(obj);

  if (!msf || msf->conn != minksock) {
    return MARSHAL_ERROR_MISMATCH;
  }

  args[pos].o.flags = objFlag;
  args[pos].o.handle = msf->handle;

  return Object_OK;
}

static inline
int32_t MarshalIn_msforwarder(MinkSocket *minksock, ObjectTable *objTable, lxcom_arg *args,
                              Object *obj, int32_t *pos, int32_t objFlag)
{
  uint16_t flags = args[*pos].o.flags;
  uint16_t handle = args[*pos].o.handle;
  uint32_t version = MINKSOCK_VER_UNINITIALIZED;

  if (!(flags & objFlag)) {
    return MARSHAL_ERROR_MISMATCH;
  }

  if (Object_isERROR(MinkSocket_getVersion(minksock, &version))) {
    LOG_ERR("Failed to get version of minksock=%p.\n", minksock);
    return Object_ERROR_UNAVAIL;
  }

  *obj = ObjectTable_recoverObject(objTable, handle);
  if (Object_isNull(*obj)) {
    LOG_ERR("Fail to recover handle=%d from objTable=%p.\n", handle, objTable);
    return Object_ERROR_UNAVAIL;
  }

  return Object_OK;
}

static inline
int32_t MarshalOut_caller_msforwarder(MinkSocket *minksock, Object obj,
                                      lxcom_arg *args, int32_t pos)
{
  return MarshalOut_msforwarder(minksock, obj, args, pos, LXCOM_CALLEE_OBJECT);
}

static inline
int32_t MarshalOut_callee_msforwarder(MinkSocket *minksock, Object obj,
                                      lxcom_arg *args, int32_t pos)
{
  return MarshalOut_msforwarder(minksock, obj, args, pos, LXCOM_CALLER_OBJECT);
}

static inline
int32_t MarshalIn_callee_msforwarder(MinkSocket *minksock, ObjectTable *objTable, lxcom_arg *args,
                                     Object *obj, int32_t *pos)
{
  return MarshalIn_msforwarder(minksock, objTable, args, obj, pos, LXCOM_CALLEE_OBJECT);
}

static inline
int32_t MarshalIn_caller_msforwarder(MinkSocket *minksock, ObjectTable *objTable, lxcom_arg *args,
                                     Object *obj, int32_t *pos)
{
  return MarshalIn_msforwarder(minksock, objTable, args, obj, pos, LXCOM_CALLER_OBJECT);
}

/* Marshalling scenario for genericObject.
 */
static inline
int32_t MarshalOut_genericObject(ObjectTable *objTable, Object obj,
                                 lxcom_arg *args, int32_t pos, int32_t objFlag)
{
  int32_t handle = -1;

  handle = ObjectTable_addObject(objTable, obj);
  if (handle == -1) {
    LOG_ERR("Failed to add obj.context=%p to objTable=%p.\n", obj.context, objTable);
    return Object_ERROR_KMEM;
  }
  args[pos].o.flags = objFlag;
  args[pos].o.handle = handle;

  return Object_OK;
}

static inline
int32_t MarshalIn_genericObject(MinkSocket *minksock, lxcom_arg *args,
                                Object *obj, int32_t pos, int32_t objFlag)
{
  int32_t ret = Object_ERROR;
  uint16_t flags = args[pos].o.flags;
  uint16_t handle = args[pos].o.handle;

  if (!(flags & objFlag)){
    return MARSHAL_ERROR_MISMATCH;
  }

  ret = MSForwarder_new(minksock, handle, obj);
  if (!Object_isOK(ret) || Object_isNull(*obj)) {
    LOG_ERR("Fail on MSForwarder_new() in the position[%d]\n", pos);
    return Object_ERROR_UNAVAIL;
  }

  return Object_OK;
}

static inline
int32_t MarshalOut_caller_genericObject(ObjectTable *objTable, Object obj,
                                        lxcom_arg *args, int32_t pos)
{
  return MarshalOut_genericObject(objTable, obj, args, pos, LXCOM_CALLER_OBJECT);
}

static inline
int32_t MarshalOut_callee_genericObject(ObjectTable *objTable, Object obj,
                                        lxcom_arg *args, int32_t pos)
{
  return MarshalOut_genericObject(objTable, obj, args, pos, LXCOM_CALLEE_OBJECT);
}

static inline
int32_t MarshalIn_callee_genericObject(MinkSocket *minksock, lxcom_arg *args,
                                       Object *obj, int32_t pos)
{
  return MarshalIn_genericObject(minksock, args, obj, pos, LXCOM_CALLER_OBJECT);
}

static inline
int32_t MarshalIn_caller_genericObject(MinkSocket *minksock, lxcom_arg *args,
                                       Object *obj, int32_t pos)
{
  return MarshalIn_genericObject(minksock, args, obj, pos, LXCOM_CALLEE_OBJECT);
}

static
int32_t MarshalIn_reserveBuffer(ObjectArg *args, lxcom_inv_req *req,
                                void **bufResv, size_t sizeResv)
{
  size_t sizeActual = 0;
  size_t sizeRemain = 0;
  size_t remain = 0;
  size_t pad = 0;
  void *ptr = NULL;

  FOR_ARGS(i, req->k, BO) {
    args[i].b.size = req->a[i].size;
    if(0 != args[i].b.size) {
      sizeActual += PADDED(args[i].b.size);
    } else {
      sizeActual += LXCOM_MSG_ALIGNMENT;
    }

    LOG_TRACE("Callee is requested for bufOut[%d] in size = %d\n",
               i, req->a[i].size);
  }

  if (sizeActual > sizeResv) {
    if (sizeActual > MSG_BUFFER_MAX - 4) {
      LOG_ERR("reserved buffer size for BO exceeds the maximum\n");
      return Object_ERROR_INVALID;
    }
    ptr = HEAP_ZALLOC(sizeActual + 4);
    if (NULL == ptr) {
      LOG_ERR("Allocate reservedBuffer failed\n");
      return Object_ERROR_KMEM;
    }
    *bufResv = ptr;
  } else {
    ptr = *bufResv;
  }

  sizeRemain = sizeActual + 4;
  FOR_ARGS(i, req->k, BO) {
    remain = (uintptr_t) ptr % LXCOM_MSG_ALIGNMENT;
    if (remain) {
      pad = LXCOM_MSG_ALIGNMENT - remain;
      if (pad > sizeRemain || pad + args[i].b.size > sizeRemain) {
        LOG_ERR("Weird error in reserved buffer\n");
        return Object_ERROR_INVALID;
      }
    }
    args[i].b.ptr = (char *)ptr + pad;
    ptr = (char *)ptr + pad + args[i].b.size;
    sizeRemain -= pad + args[i].b.size;
  }

  return Object_OK;
}

static
int32_t Flatdata_extendAndCopy(FlatData **data, size_t sizeOnWire,
                               size_t sizeExtend, size_t sizeMax)
{
  FlatData *ptr = NULL;

  if (NULL == *data || 0 == sizeExtend) {
    LOG_ERR("invalid input\n");
    return Object_ERROR_INVALID;
  }

  if (sizeExtend > sizeMax) {
    LOG_ERR("Flatdata extending size exceeds the maximum\n");
    return Object_ERROR_INVALID;
  }

  ptr = (FlatData *)HEAP_ZALLOC(sizeExtend);
  if (NULL == ptr) {
    LOG_ERR("Allocate flatdata failed\n");
    return Object_ERROR_KMEM;
  }

  memscpy(ptr, sizeOnWire, *data, sizeOnWire);
  *data = ptr;

  return Object_OK;
}

static
int32_t MarshalOut_caller_buffer(ObjectArg arg, FlatData *data,
                                 size_t sizeOnWire, size_t sizeMax, size_t *sizeAdded)
{
  int32_t pad = PADDED(sizeOnWire) - sizeOnWire;

  *sizeAdded = 0;
  if (sizeOnWire + pad + arg.b.size > sizeMax) {
    LOG_TRACE("Buffer outboundary occurs, try to extend\n");
    return Object_ERROR_MAXARGS;
  }

  if (pad) {
    memscpy((char *)data + sizeOnWire, pad, PadBuf, pad);
    sizeOnWire += pad;
  }
  memscpy((char *)data + sizeOnWire, arg.b.size, arg.b.ptr, arg.b.size);

  *sizeAdded = pad + arg.b.size;

  return Object_OK;
}

static
int32_t MarshalIn_callee_buffer(FlatData *data, ObjectArg *arg, size_t sizeArg,
                                size_t sizeOnWire, size_t sizeMax, size_t *sizeAdded)
{
  size_t pad = PADDED(sizeOnWire) - sizeOnWire;

  *sizeAdded = 0;
  if (sizeOnWire + pad > sizeMax) {
    LOG_TRACE("Buffer outboundary occurs, try to extend\n");
    return Object_ERROR_MAXARGS;
  }

  if (pad) {
    sizeOnWire += pad;
  }
  arg->b.ptr = (uint8_t *)data->buf + sizeOnWire;
  arg->b.size = sizeArg;

  *sizeAdded = pad + sizeArg;

  return Object_OK;
}

static inline
int32_t MarshalOut_callee_buffer(ObjectArg arg, FlatData *data,
                                 size_t sizeOnWire, size_t sizeMax, size_t *sizeAdded)
{
  return MarshalOut_caller_buffer(arg, data, sizeOnWire, sizeMax, sizeAdded);
}

static
int32_t MarshalIn_caller_buffer(FlatData *data, ObjectArg *arg, size_t sizeArg,
                                size_t sizeOnWire, size_t sizeMax, size_t *sizeAdded)
{
  size_t pad = PADDED(sizeOnWire) - sizeOnWire;

  *sizeAdded = 0;
  if (sizeOnWire + pad + sizeArg> sizeMax) {
    LOG_TRACE("Buffer outboundary occurs, try to extend\n");
    return Object_ERROR_MAXARGS;
  }

  if (pad) {
    sizeOnWire += pad;
  }
  memscpy(arg->b.ptr, sizeArg, (uint8_t *)data->buf + sizeOnWire, sizeArg);

  LOG_TRACE("Caller receives bufOut in actual size = %zu, compared to the given\
             size = %zu\n", sizeArg, arg->b.size);

  arg->b.size = sizeArg;
  *sizeAdded = pad + sizeArg;

  return Object_OK;
}

/*@brief: marshalling when caller sends out message
 *        transform data from "ObjectArg[] + ObjectCounts" to "flatdata + fds[]"
 *        flatdata structure:
 *        |1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|1 byte|......|
 *        |        lxcom_inv_req         |pad|     args.b     |pad|  args.b |pad|..... |
*/
int32_t MarshalOut_caller(MinkSocket *minksock, ObjectTable *objTable, uint32_t invId,
                          int32_t handle, ObjectOp op, ObjectArg *args, ObjectCounts k,
                          FlatData **data, size_t *sizeData,
                          int *fds, size_t *numFds)
{
  int32_t err = Object_ERROR;
  size_t numExtraArgs = 0;
  size_t sizeOnWire = Sizeof_invReq(k, numExtraArgs);
  size_t sizeLimit = MSG_BUFFER_PREALLOC;
  size_t sizeExtend = 0;
  size_t sizeAdded = 0;
  FlatData *buff = *data;
  lxcom_inv_req *invReq = NULL;

  CHECK_MAX_ARGS(k, In, numExtraArgs);
  C_ZERO(*buff);
  for (int i = 0; i < *numFds; i++) {
    fds[i] = -1;
  }

  FOR_ARGS(i, k, BI) {
    err = MarshalOut_caller_buffer(args[i], buff, sizeOnWire, sizeLimit, &sizeAdded);
    sizeOnWire += sizeAdded;
    if (err) {
      sizeExtend = Sizeof_dataInput(args, k, i, sizeOnWire);
      err = Flatdata_extendAndCopy(data, sizeOnWire, sizeExtend, MSG_BUFFER_MAX);
      if (err) {
        LOG_ERR("Flatdata_extendAndCopy failed with error %d\n", err);
        return err;
      }
      buff = *data;
      sizeLimit = sizeExtend;
      i--;
    }
  }
  *sizeData = sizeOnWire;

  FOR_ARGS(i, k, BO) {
    if (!args[i].b.ptr && args[i].b.size != 0) {
      LOG_ERR("invalid BO parameter\n");
      return Object_ERROR_INVALID;
    }
  }

  invReq = (lxcom_inv_req *)&buff->msg;
  FOR_ARGS(i, k, BUFFERS) {
    invReq->a[i].size = args[i].b.size;
  }

  invReq->hdr.type = LXCOM_REQUEST;
  invReq->handle = handle;
  invReq->op = op;
  invReq->k = k;
  invReq->hdr.size = *sizeData;

  //to match the iOI++ at the beginning of every object marshalling iteration
  int32_t iOI = (int32_t)(ObjectCounts_indexOI(k)) - 1;
  size_t fdIndex = 0;
  FOR_ARGS(i, k, OI) {
    iOI++;
    if (iOI < 0 || iOI >= LXCOM_MAX_ARGS) {
      LOG_ERR("invalid array position\n");
      return Object_ERROR_MAXARGS;
    }

    CONTINUE_OR_RETURN(MarshalOut_caller_nullObject(args[i].o, invReq->a, iOI),
                       "ObjectNull");
    CONTINUE_OR_RETURN(MarshalOut_caller_msforwarder(minksock, args[i].o, invReq->a,
                       iOI), "msforwarder");
    CONTINUE_OR_RETURN(MarshalOut_caller_genericObject(objTable, args[i].o,
                       invReq->a, iOI), "generic obj");

    //generally, object must match one of the marshalling situations above.
    LOG_ERR("never matched marshalling situation of object occurs\n");
    return Object_ERROR;
  }
  *numFds = fdIndex;

  return Object_OK;
}

/*@brief: marshalling when callee receives in message
 *        transform data from "flatdata + fds[]" to "ObjectArg[] + ObjectCounts"
*/
int32_t MarshalIn_callee(MinkSocket *minksock, ObjectTable *objTable,
                         FlatData *data, size_t sizeData,
                         int *fds, size_t numFds,
                         ObjectArg *args, void **bufResv, size_t sizeResv)
{
  int32_t err = Object_ERROR;
  lxcom_inv_req *invReq = (lxcom_inv_req *)&data->msg;
  size_t numExtraArgs = 0;
  size_t sizeReq = Sizeof_invReq(invReq->k, numExtraArgs);
  size_t sizeOnWire = sizeReq;
  size_t sizeAdded = 0;

  CHECK_MAX_ARGS(invReq->k, In, numExtraArgs);
  if (sizeReq > invReq->hdr.size) {
    LOG_ERR("Unexpected error on flatdata size\n");
    return Object_ERROR_MAXARGS;
  }

  FOR_ARGS(i, invReq->k, BI) {
    err = MarshalIn_callee_buffer(data, &args[i], invReq->a[i].size, sizeOnWire,
                                  invReq->hdr.size, &sizeAdded);
    if (err) {
      LOG_ERR("MarshalIn_callee_buffer failed with error %d\n", err);
      return err;
    }
    sizeOnWire += sizeAdded;
  }

  if (0 != ObjectCounts_numBO(invReq->k)) {
    err = MarshalIn_reserveBuffer(args, invReq, bufResv, sizeResv);
    if (err) {
      LOG_ERR("MarshalIn_reserveBuffer failed with error %d\n", err);
      return err;
    }
  }

  //to match the iOI++ at the beginning of every object marshalling iteration
  int32_t iOI = (int32_t)(ObjectCounts_indexOI(invReq->k)) - 1;
  size_t fdIndex = 0;
  FOR_ARGS(i, data->msg.req.k, OI) {
    iOI++;
    if (iOI < 0 || iOI >= LXCOM_MAX_ARGS) {
      LOG_ERR("invalid array position\n");
      return Object_ERROR_MAXARGS;
    }

    CONTINUE_OR_RETURN(MarshalIn_callee_nullObject(data->msg.req.a, &args[i].o,
                       iOI), "ObjectNull");
    CONTINUE_OR_RETURN(MarshalIn_callee_msforwarder(minksock, objTable, data->msg.req.a,
                       &args[i].o, &iOI), "msforwarder");
    CONTINUE_OR_RETURN(MarshalIn_callee_genericObject(minksock, data->msg.req.a,
                       &args[i].o, iOI), "generic obj");

    //generally, object must match one of the marshalling situations above.
    LOG_ERR("never matched marshalling situation of object occurs\n");
    return Object_ERROR;
  }

  return Object_OK;
}

/*@brief: release input/resquest data after processing MarshalIn_callee()
 */
void MarshalIn_calleeRelease(int32_t invokeCounts, ObjectArg *invokeArgs,
                             int *fds, int numFds)
{
  if (invokeArgs != NULL) {
    //Release all input Object references since we're done with them
    FOR_ARGS(i, invokeCounts, OI) {
      Object_ASSIGN_NULL(invokeArgs[i].o);
    }
  }

  if (fds != NULL) {
    for (int i = 0; i < numFds; i++) {
      //close all fds found on input
      if (fds[i] != -1) {
        vm_osal_mem_close(fds[i]);
        fds[i] = -1;
      }
    }
  }
}

/*@brief: marshalling when callee replies back message
 *        transform data from "ObjectArg[] + ObjectCounts" to "flatdata + fds[]"
*/
int32_t MarshalOut_callee(MinkSocket *minksock, ObjectTable *objTable,
                          uint32_t invId, ObjectArg *args, ObjectCounts k,
                          FlatData **data, size_t *sizeData,
                          int *fds, size_t *numFds)
{
  int32_t err = Object_ERROR;
  size_t numExtraArgs = 0;
  size_t sizeOnWire = Sizeof_invSucc(k, numExtraArgs);
  size_t sizeLimit = MSG_BUFFER_PREALLOC;
  size_t sizeExtend = 0;
  size_t sizeAdded = 0;
  FlatData *buff = *data;
  lxcom_inv_succ *invSucc = (lxcom_inv_succ *)&buff->msg;

  CHECK_MAX_ARGS(k, Out, numExtraArgs);
  C_ZERO(*buff);
  for (int i = 0; i < *numFds; i++) {
    fds[i] = -1;
  }

  // To prevent size_t from underflow
  int32_t j = 0;
  FOR_ARGS(i, k, BO) {
    invSucc->a[j].size = args[i].b.size;
    err = MarshalOut_callee_buffer(args[i], buff, sizeOnWire, sizeLimit, &sizeAdded);
    sizeOnWire += sizeAdded;
    if (err) {
      sizeExtend = Sizeof_dataOutput(args, k, i, sizeOnWire);
      err = Flatdata_extendAndCopy(data, sizeOnWire, sizeExtend, MSG_BUFFER_MAX);
      if (err) {
        LOG_ERR("Flatdata_extendAndCopy failed with error %d\n", err);
        return err;
      }
      buff = *data;
      invSucc = (lxcom_inv_succ *)&buff->msg;
      sizeLimit = sizeExtend;
      i--;
      j--;
    }
    j++;

    LOG_TRACE("Callee responds bufOut[%d] in actual size = %zu", i, args[i].b.size);
  }
  *sizeData = sizeOnWire;

  invSucc->hdr.type = LXCOM_SUCCESS;
  invSucc->hdr.invoke_id = invId;
  invSucc->hdr.size = *sizeData;

  //to match the iOO++ at the beginning of every object marshalling iteration
  int32_t iOO = (int32_t)(ObjectCounts_numBO(k)) - 1;
  size_t fdIndex = 0;
  FOR_ARGS(i, k, OO) {
    iOO++;
    if (iOO < 0 || iOO >= LXCOM_MAX_ARGS) {
      LOG_ERR("invalid array position\n");
      return Object_ERROR_MAXARGS;
    }

    CONTINUE_OR_RETURN(MarshalOut_callee_nullObject(args[i].o, invSucc->a, iOO),
                       "ObjectNull");
    CONTINUE_OR_RETURN(MarshalOut_callee_msforwarder(minksock, args[i].o, invSucc->a,
                       iOO), "msforwarder");
    CONTINUE_OR_RETURN(MarshalOut_callee_genericObject(objTable, args[i].o, invSucc->a,
                       iOO), "generic obj");

    //generally, object must match one of the marshalling situations above.
    LOG_ERR("never matched marshalling situation of object occurs\n");
    return Object_ERROR;
  }
  *numFds = fdIndex;

  return Object_OK;
}

/*@brief: release output/response data after processing MarshalOut_callee()
 */
void MarshalOut_calleeRelease(char *resvBuf, void *ptrResv,
                              Object targetObj, int32_t invokeCounts,
                              ObjectArg *invokeArgs, int32_t errInvoke)
{
  if (ptrResv != NULL && ptrResv != resvBuf) {
    HEAP_FREE_PTR(ptrResv);
  }

  if (!Object_isNull(targetObj)) {
    //Release all output Object references since we're done with them
    if (Object_isOK(errInvoke) && invokeArgs != NULL) {
      FOR_ARGS(i, invokeCounts, OO) {
        Object_ASSIGN_NULL(invokeArgs[i].o);
      }
    }
    Object_release(targetObj);
  }
}

/*@brief: marshalling when caller receives in message
 *        transform data from "flatdata + fds[]" to "ObjectArg[] + ObjectCounts"
*/
int32_t MarshalIn_caller(MinkSocket *minksock, ObjectTable *objTable,
                         FlatData *data, size_t sizeData,
                         int *fds, size_t numFds,
                         int32_t k, ObjectArg *args)
{
  int32_t err = Object_OK;
  lxcom_inv_succ *invSucc = (lxcom_inv_succ *)&data->msg;
  size_t numExtraArgs = 0;
  size_t sizeSucc = Sizeof_invSucc(k, numExtraArgs);
  size_t sizeOnWire = sizeSucc;
  size_t sizeAdded = 0;
  size_t j = 0;

  if (sizeSucc > invSucc->hdr.size) {
    LOG_ERR("Unexpected error on flatdata size\n");
    return Object_ERROR_MAXARGS;
  }
  if (ObjectCounts_total(k) > C_LENGTHOF(invSucc->a)) {
    LOG_ERR("ObjectArgs format error\n");
    return Object_ERROR_MAXARGS;
  }

  FOR_ARGS(i, k, BO) {
    err = MarshalIn_caller_buffer(data, &args[i], invSucc->a[j].size,
                                  sizeOnWire, invSucc->hdr.size, &sizeAdded);
    if (err) {
      LOG_ERR("MarshalIn_caller_buffer failed with error %d\n", err);
      return err;
    }

    sizeOnWire += sizeAdded;
    j++;
  }

  //to match the iOO++ at the beginning of every object marshalling iteration
  int32_t iOO = (int32_t)(ObjectCounts_numBO(k)) - 1;
  size_t fdIndex = 0;
  FOR_ARGS(i, k, OO) {
    iOO++;
    if (iOO < 0 || iOO >= LXCOM_MAX_ARGS) {
      LOG_ERR("invalid array position\n");
      return Object_ERROR_MAXARGS;
    }

    CONTINUE_OR_CLEANUP(MarshalIn_caller_nullObject(invSucc->a, &args[i].o, iOO),
                       "ObjectNull");
    CONTINUE_OR_CLEANUP(MarshalIn_caller_msforwarder(minksock, objTable, invSucc->a, &args[i].o,
                        &iOO), "msforwarder");
    CONTINUE_OR_CLEANUP(MarshalIn_caller_genericObject(minksock, invSucc->a, &args[i].o,
                        iOO), "generic obj");

    //generally, object must match one of the marshalling situations above.
    LOG_ERR("never matched marshalling situation of object occurs\n");
    return Object_ERROR;
  }

  return Object_OK;

cleanup:
  FOR_ARGS(i, k, OO) {
    Object_ASSIGN_NULL(args[i].o);
  }

  for (int i = 0; i < numFds; i++) {
    if (fds[i] != -1) {
      vm_osal_mem_close(fds[i]);
    }
  }

  return err;
}

void Marshal_perfEntryTag(MinkSocket *minksock, uint32_t ipcType, FlatData *Msg)
{
  switch (ipcType) {
    case LXCOM_REQUEST:
      LOG_PERF("msock = %p, invId = %u, h = %d, op = %d, recvREQUEST \n",
               minksock, Msg->msg.req.hdr.invoke_id, Msg->msg.req.handle, Msg->msg.req.op);
      break;
    case LXCOM_SUCCESS:
      LOG_PERF("msock = %p, invId = %u, recvSUCCESS \n", minksock, Msg->msg.succ.hdr.invoke_id);
      break;
    case LXCOM_ERROR:
      LOG_PERF("msock = %p, invId = %u, recvERROR \n", minksock, Msg->msg.succ.hdr.invoke_id);
      break;
    case LXCOM_CLOSE:
      LOG_PERF("msock = %p, closeId = %u, recvCLOSE \n", minksock, Msg->msg.close.handle);
      break;
    default:
      LOG_ERR("Error, unsupported IPC type = %u \n", ipcType);
      break;
  }
}
