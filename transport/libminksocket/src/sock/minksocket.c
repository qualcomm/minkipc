/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/


#if defined(USE_GLIB) && !defined (__aarch64__)
/* FIXME SDX60:
   The Olympic bits/socket.h header is patched out to only include the required
   asm/socket.h if __USE_MISC is defined. Explicitly define this flag as a
   workaround for 32-bit LE targets until the root-issue can be addressed in
   the kernel. Otherwise SO_PEERCRED is not defined. */
#define __USE_MISC 1
#endif
#include <sys/socket.h>
#include <sys/un.h>
#include "cdefs.h"
#include "check.h"
#include "CredentialsAdapter.h"
#include "fdwrapper.h"
#include "Heap.h"
#include "LinkCredentials.h"
#include "lxcom_sock.h"
#include "memscpy.h"
#include "minksocket.h"
#include "msforwarder.h"
#include "Marshalling.h"
#include "object.h"
#include "ObjectTable.h"
#include "SockAgnostic.h"
#include "ThreadPool.h"
#include "Utils.h"
#include "Profiling.h"
#include "vmuuid.h"

#define BEGIN_INVOKE_ID         1

struct MinkSocket {
  int refs;
  int firstShot;
  bool bDone;
  bool bServer;
  ObjectTable table;
  bool closeOnOTEmpty;
  int sockPair[2];
  uint32_t invoke_id;
  vm_osal_mutex mutex;
  vm_osal_cond cond;
  QList qlInvokes;
  QList qlMSForwarder;
  ThreadPool *pool;
  CredInfo credInfo;
  int msForwarderCount;
  SockAgnostic sockAgnostic;
  Object linkCredentials;
  Object credAdapter;
  uint8_t *vmuuid;
  size_t vmuuidLen;
  uint32_t version;
};

static vm_osal_key gSpecificInvokeId;

int32_t MinkSocket_getVersion(MinkSocket *me, uint32_t *ackVersion)
{
  int32_t ret = Object_OK;

  if (!me || !ackVersion) {
    LOG_ERR("Either minksock or ackVersion is NULL.\n");
    return Object_ERROR_INVALID;
  }

  *ackVersion = me->version;

  return ret;
}

int32_t MinkSocket_setVersion(MinkSocket *me, uint32_t ackVersion)
{
  int32_t ret = Object_OK;

  if (!me) {
    LOG_ERR("minksock is NULL.\n");
    return Object_ERROR_INVALID;
  }

  me->version = ackVersion;

  LOG_TRACE("Set version=%u.%02u for minksock=%p.\n", GET_MAJOR_VER(me->version),
            GET_MINOR_VER(me->version), me);

  return ret;
}

static inline
bool MinkSocket_isFirstShot(MinkSocket *me)
{
  if (vm_osal_atomic_add(&me->firstShot, -1) == 0) {
    return true;
  }

  return false;
}

bool MinkSocket_isRemote(MinkSocket *me)
{
  return false;
}

int32_t MinkSocket_getSockAgnostic(MinkSocket *me, SockAgnostic **outSockAgn)
{
  *outSockAgn = &(me->sockAgnostic);

  return Object_OK;
}

/*@brief: pop out MSForwarder from qlMSForwarder of MinkSocket
 *        private interface, caller takes care of the mutex
**/
static inline MSForwarder *MinkSocket_popForwarder(MinkSocket *me)
{
  QNode *msFwd_node = QList_pop(&me->qlMSForwarder);
  if (NULL == msFwd_node)
    return NULL;

  return c_containerof(msFwd_node, MSForwarder, node);
}

/*@brief: attach MSForwarder to qlMSForwarder of MinkSocket
 *        public interface, function itself takes care of the mutex
**/
int32_t MinkSocket_attachForwarder(MinkSocket *me, MSForwarder *msFwd)
{
  if (NULL == me || NULL == msFwd) {
    LOG_ERR("NULL input parameter\n");
    return Object_ERROR;
  }

  vm_osal_mutex_lock(&me->mutex);

  if (!me->bDone && SockAgnostic_isAlive(&(me->sockAgnostic))) {
    vm_osal_atomic_add(&me->msForwarderCount, 1);
    MinkSocket_retain(msFwd->conn);
    QList_appendNode(&me->qlMSForwarder, &msFwd->node);
    vm_osal_mutex_unlock(&me->mutex);
    return Object_OK;
  }

  vm_osal_mutex_unlock(&me->mutex);
  return Object_ERROR;
}

/*@brief: detach MSForwarder from qlMSForwarder of MinkSocket
 *        private interface, caller takes care of the mutex
 *
 *@return: Object_OK when MinkSocket instance just be released but not freed
 *         Object_ERROR_UNAVAIL when MinkSocket instance be freed
**/
static inline
int32_t MinkSocket_detachForwarderLocked(MinkSocket *me, MSForwarder *msFwd)
{
  if (me && (me == msFwd->conn)) {
    QNode_dequeueIf(&msFwd->node);
    msFwd->conn = NULL;
    vm_osal_atomic_add(&me->msForwarderCount, -1);
    if (vm_osal_atomic_add(&me->refs, -1) == 0) {
      return Object_ERROR_UNAVAIL;
    }
  }

  return Object_OK;
}

/*@brief: detach specific MSForwarder from MinkSocket
 *        only can be invoked after after MinkSocket_close()
 *        public interface, function itself takes care of the mutex
 *
 *@return: Object_OK when MinkSocket instance just be released but not freed
 *         Object_ERROR_UNAVAIL when MinkSocket instance be freed
**/
int32_t MinkSocket_detachForwarder(MinkSocket *me, MSForwarder *msFwd)
{
  int32_t ret = Object_OK;

  if (NULL == me || NULL == msFwd) {
    LOG_ERR("NULL input parameter\n");
    return Object_ERROR;
  }
  if (me != msFwd->conn) {
    LOG_ERR("mismatched MinkSocket and MSForwarder\n");
    return Object_ERROR;
  }

  vm_osal_mutex_lock(&me->mutex);
  ret = MinkSocket_detachForwarderLocked(me, msFwd);
  vm_osal_mutex_unlock(&me->mutex);

  if (Object_ERROR_UNAVAIL == ret) {
    LOG_TRACE("MinkSocket instance is freed in disorder\n");
    MinkSocket_delete(me);
  }

  return ret;
}

/*@brief: detach all MSForwarder from MinkSocket
 *        only can be invoked after after MinkSocket_close()
 *        public interface, function itself takes care of the mutex
 *
 *@return: Object_OK when MinkSocket instance just be released but not freed
 *         Object_ERROR_UNAVAIL when MinkSocket instance be freed
*/
int32_t MinkSocket_detachForwarderAll(MinkSocket *me)
{
  int32_t ret = Object_OK;
  MSForwarder *msFwd;

  if (NULL == me) {
    LOG_ERR("NULL input parameter\n");
    return Object_ERROR;
  }

  vm_osal_mutex_lock(&me->mutex);
  while ((msFwd = MinkSocket_popForwarder(me))) {
    if((ret = MinkSocket_detachForwarderLocked(me, msFwd))) {
      break;
    }
  }
  vm_osal_mutex_unlock(&me->mutex);

  if (Object_ERROR_UNAVAIL == ret) {
    LOG_TRACE("MinkSocket instance is freed in disorder\n");
    MinkSocket_delete(me);
  }

  return ret;
}


/******************************************************************
  terminate relation between MinkSockt and MSForwarder
  public interface, function itself takes care of the mutex
*******************************************************************/
void MinkSocket_preDeleteForwarder(MinkSocket *me, MSForwarder *msFwd)
{
  int32_t ret = Object_OK;

  if (NULL == me) {
    LOG_TRACE("NULL input parameter\n");
    return;
  }

  vm_osal_mutex_lock(&me->mutex);

  if (msFwd->handle >= 0) {
    MinkSocket_sendClose(me, msFwd->handle);
  }

  ret = MinkSocket_detachForwarderLocked(me, msFwd);

  vm_osal_mutex_unlock(&me->mutex);

  if (Object_ERROR_UNAVAIL == ret) {
    LOG_TRACE("MinkSocket instance is freed in disorder\n");
    MinkSocket_delete(me);
  }

  return;
}

// ----------------------------------------------------------------------------
// Functions for checking VM name, needed for interacting with libvmmem
// ----------------------------------------------------------------------------

typedef struct InvokeInfo {
  QNode qn;
  uint32_t invoke_id;
  int handle;
  ObjectOp op;
  ObjectArg *args;
  ObjectCounts k;
  int result;
  bool bComplete;
  pthread_cond_t cond;
} InvokeInfo;

static void InvokeInfo_init(InvokeInfo *me, int32_t h,
                ObjectOp op, ObjectArg *args, ObjectCounts k)
{
  C_ZERO(*me);
  me->handle = h;
  me->op = op;
  me->args = args;
  me->k = k;
}

static inline void
InvokeInfo_setResult(InvokeInfo *me, int32_t result) {
  me->bComplete = true;
  me->result = result;
  vm_osal_cond_set(&me->cond);
}

int32_t MinkSocket_initCredentials(MinkSocket *me, bool bModule)
{
  int32_t ret = Object_OK;
  int32_t sockType = SockAgnostic_getSockType(&(me->sockAgnostic));

  if (!me->bServer || !bModule) {
    LOG_ERR("minksocket %p doesn't supports credentials\n", me);
    return Object_ERROR_INVALID;
  }

  if (SockAgnostic_getPeerIdentity(&(me->sockAgnostic), &(me->credInfo),
                                   &(me->vmuuid), &(me->vmuuidLen))) {
    LOG_ERR("Failed on MinkSocket_getPeerIdentity()\n");
    return Object_ERROR;
  }

  if (UNIX == sockType) {
    ret = LinkCred_new(&(me->credInfo), ELOC_LOCAL, sockType,
                       &(me->linkCredentials));
  } else {
    ret = LinkCredRemote_new(me->vmuuid, me->vmuuidLen, ELOC_REMOTE,
                             sockType, &(me->linkCredentials));
  }
  if (ret) {
    LOG_ERR("Failed to new LinkCred, ret = %d, minksocket = %p\n", ret, me);
  }

  return ret;
}

int32_t MinkSocket_initObjectTable(MinkSocket *me, Object endpoint, bool bModule)
{
  int32_t ret = Object_OK;
  Object adapter = Object_NULL;

  if (!me->bServer) {
    LOG_ERR("ObjectTable initialization should only happens on server endpoint\n");
    return Object_ERROR_INVALID;
  }

  if (bModule) {
    if (UNIX == SockAgnostic_getSockType(&(me->sockAgnostic))) {
      ret = LocalCredAdapter_new(endpoint, me->linkCredentials, &(me->credAdapter));
    } else {
      ret = RemoteCredAdapter_new(endpoint, me->linkCredentials, &(me->credAdapter));
    }
    if (ret) {
      LOG_ERR("Failed to new CredAdapter, ret = %d, minksocket = %p, endpoint = %p\n",
               ret, me, &endpoint);
      goto cleanup;
    }

    adapter = me->credAdapter;
  } else {
    adapter = endpoint;
  }

  ret = ObjectTable_addObject(&me->table, adapter);
  if (GENERIC_HANDLE != ret) {
    LOG_ERR("Failed on ObjectTable_addObject(), ret = %d, minksocket = %p, table = %p\n",
            ret, me, &me->table);
    ret = Object_ERROR_NOSLOTS;
    goto cleanup;
  }

  LOG_TRACE("Initialized objectTable with CredAdapter = %p, belongs to minksocket = %p\n",
            (me->credAdapter).context, me);

  return Object_OK;

cleanup:
  if(!Object_isNull(me->linkCredentials)) {
    Object_ASSIGN_NULL(me->linkCredentials);
  }
  if(!Object_isNull(me->credAdapter)) {
    Object_ASSIGN_NULL(me->credAdapter);
  }
  return ret;
}

MinkSocket *MinkSocket_new_internal(SockAgnostic *inSockAgn, Object endpoint)
{
  int32_t ret = Object_OK;
  MinkSocket *me = NULL;

  me = HEAP_ZALLOC_REC(MinkSocket);
  if (!me) {
    return NULL;
  }

  me->refs = 1;
  me->firstShot = 1;
  me->bDone = false;
  me->msForwarderCount = 0;
  me->invoke_id = BEGIN_INVOKE_ID;
  me->version = MINKSOCK_VER_UNINITIALIZED;
  me->sockPair[0] = -1;
  me->sockPair[1] = -1;

  SockAgnostic_adaptAndCopy(&(me->sockAgnostic), inSockAgn);
  QList_construct(&me->qlInvokes);
  QList_construct(&me->qlMSForwarder);

  ret = socketpair(AF_UNIX, SOCK_STREAM, 0, me->sockPair);
  TRUE_OR_CLEAN(Object_OK == ret, "Failed on socketpair(), ret %d\n", ret);

  vm_osal_mutex_init(&me->mutex, NULL);
  vm_osal_cond_init(&me->cond, NULL);
  if (!Object_isNull(endpoint)) {
    me->bServer = true;
  }
  me->pool = ThreadPool_new(me->bServer);
  TRUE_OR_CLEAN(NULL != me->pool, "Failed to create threadPool\n");

  ret = ObjectTable_construct(&me->table, MAX_OBJECT_COUNT);
  TRUE_OR_CLEAN(Object_OK == ret, "Failed on ObjectTable_construct(), ret %d\n", ret);

  vm_osal_create_TLS_key(&gSpecificInvokeId, NULL);

  LOG_TRACE("constructed threadPool %p, objectTable %p, belongs to minksocket %p of sockAgnostic %p\n",
           me->pool, &me->table, me, &me->sockAgnostic);

  return me;

cleanup:
  MinkSocket_release(me);
  return NULL;
}

MinkSocket *MinkSocket_new(Object endpoint, int32_t sockType, int32_t sock)
{
  bool bServer = false;
  SockAgnostic sockAgnostic = {0};

  if (!Object_isNull(endpoint)) {
    bServer = true;
  }

  SockAgnostic_populate(&sockAgnostic, sock, bServer, sockType);
  return MinkSocket_new_internal(&sockAgnostic, endpoint);
}

/*
 * Retain reference count of MinkSocket to keep its lifetime
*/
void MinkSocket_retain(MinkSocket *me)
{
  vm_osal_atomic_add(&me->refs, 1);
}

bool MinkSocket_isAlive(MinkSocket *me)
{
  bool ret = false;

  if (me == NULL) {
    return false;
  }

  vm_osal_mutex_lock(&me->mutex);
  if (!me->bDone && SockAgnostic_isAlive(&(me->sockAgnostic))) {
    ret = true;
  }
  vm_osal_mutex_unlock(&me->mutex);

  return ret;
}

/*@brief: only if MinkSocket working for server, release its reference count
 *        once finish that ThreadWork.
*/
static inline
void MinkSocket_dequeue(MinkSocket *me)
{
  if (NULL == me) {
    return;
  }

  if (me->bServer) {
    MinkSocket_release(me);
  }

  return;
}

/*@brief: only if MinkSocket working for server, retain its reference count
 *        once start new ThreadWork. Release the reference count once finish
 *        that ThreadWork. Thus MinkSocket never be freed before all TheadWorks done.
*/
void MinkSocket_enqueue(MinkSocket *me, ThreadWork *work)
{
  if (NULL == me || NULL == work) {
    return;
  }

  vm_osal_mutex_lock(&me->mutex);
  if (!me->bDone && SockAgnostic_isAlive(&(me->sockAgnostic))) {
    if (me->bServer) {
      MinkSocket_retain(me);
    }
    ThreadPool_queue(me->pool, work);
  } else {
    HEAP_FREE_PTR(work);
  }
  vm_osal_mutex_unlock(&me->mutex);

  return;
}

/*@brief: clean up resource occupied by MinkSocket
*/
void MinkSocket_delete(MinkSocket *me)
{
  if (me->pool) {
    ThreadPool_stop(me->pool);
    ThreadPool_release(me->pool);
  }
  if (me->sockPair[0] > 0) {
    vm_osal_socket_close(me->sockPair[0]);
    me->sockPair[0] = -1;
  }
  if (me->sockPair[1] > 0) {
    vm_osal_socket_close(me->sockPair[1]);
    me->sockPair[1] = -1;
  }

  Object_ASSIGN_NULL(me->credAdapter);
  Object_ASSIGN_NULL(me->linkCredentials);
  if (me->vmuuid) {
    HEAP_FREE_PTR(me->vmuuid);
  }
  ObjectTable_destruct(&me->table);
  vm_osal_mutex_deinit(&me->mutex);
  vm_osal_cond_deinit(&me->cond);

  HEAP_FREE_PTR(me);
}

/*@brief: close socket connection but not detach MSForwarder or
 *        clean up resource
*/
void MinkSocket_close(MinkSocket *me, int32_t err)
{
  if (NULL == me) {
    return;
  }

  vm_osal_mutex_lock(&me->mutex);

  if (!me->bDone && SockAgnostic_isAlive(&(me->sockAgnostic))) {
    me->bDone = true;
    for (int i = 0; i < THREADPOOL_MAX_THREADS; i++ ) {
      // interrupt worker threads from the invocation of poll().
      if(me->sockPair[1] > 0) {
        write(me->sockPair[1], "x", 1);
      }
    }

    SockAgnostic_close(&(me->sockAgnostic));

    QNode *pqn;
    while ((pqn = QList_pop(&me->qlInvokes))) {
      InvokeInfo_setResult(c_containerof(pqn, InvokeInfo, qn), err);
    }
  }

  vm_osal_mutex_unlock(&me->mutex);
}

/*@brief: Release reference count of MinkSocket to monitor its lifetime
 *        Only if reference count decrease to be 0, will MinkSocket be closed,
 *        stopped and deleted.
*/
void MinkSocket_release(MinkSocket *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    LOG_TRACE("released minkSocket = %p, threadPool = %p, objectTable = %p\n",
             me, me->pool, &me->table);
    MinkSocket_close(me, Object_ERROR_UNAVAIL);
    MinkSocket_delete(me);
  }
}

int MinkSocket_detach(MinkSocket *me)
{
  if (me->msForwarderCount != 0 || !SockAgnostic_isAlive(&(me->sockAgnostic))) {
    LOG_ERR("The MinkSocket instance is not alive\n");
    return -1;
  }

  int retFd = -1;
  retFd = vm_osal_fd_dup(SockAgnostic_getSockFd(&(me->sockAgnostic)));
  MinkSocket_close(me, Object_ERROR_UNAVAIL);
  MinkSocket_detachForwarderAll(me);

  return retFd;
}

int MinkSocket_detachObject(Object *obj)
{
  MSForwarder *msforwarder = MSForwarderFromObject(*obj);
  if (msforwarder) {
    return MSForwarder_detach(msforwarder);
  }

  return -1;
}

int32_t MinkSocket_attachObject(MinkSocket *me, int handle, Object *obj)
{
  return MSForwarder_new(me, handle, obj);
}

int32_t MinkSocket_invoke(MinkSocket *me, int32_t h, ObjectOp op,
                          ObjectArg *args, ObjectCounts k)
{
  int32_t err = Object_OK;
  static int seedStart = 0;
  int fds[ObjectCounts_maxOI] = {0}, currentSeed = 0;
  memset(fds, -1, sizeof(fds));
  size_t sizeData = 0, numFds = C_LENGTHOF(fds);
  uint32_t *specificInvokeId = NULL, invokeId = 0, invokeIdSeed = 0;

  FlatData flatdata = {0};
  FlatData *data = &flatdata;
  InvokeInfo invInfo = {0};
  vm_osal_timeval tv;

  if (!MinkSocket_isAlive(me)) {
    return Object_ERROR_UNAVAIL;
  }
  LOG_TRACE("minkSocket = %p, handle = %d, sockAgnostic = %p\n", me, h, &(me->sockAgnostic));

  // Allocate an ID for this invocation
  specificInvokeId = (uint32_t *)vm_osal_retrieve_TLS_key(gSpecificInvokeId);
  if (NULL == specificInvokeId) {
    /* a new request does not belong to any existing request's lifetime
     * generate a new random invoke id.
     */
    currentSeed = vm_osal_atomic_add(&seedStart, 1);
    vm_osal_getTimeOfDay(&tv, NULL);

    LOG_TRACE("sec = %lu, usec = %lu, seed = %d \n", tv.tv_sec, tv.tv_usec, currentSeed);
    invokeIdSeed = (unsigned int)(((tv.tv_sec & 0xFF) << 24) | ((tv.tv_usec & 0xFF) << 16) |
                                  ((vm_osal_getPid() & 0xFF) << 8) | (currentSeed & 0xFF));
    invokeId = (uint32_t)vm_osal_rand_r(&invokeIdSeed);
  } else {
    /* a new request belongs to a certain request's lifetime,
     * reuse the existing invoke id.
     */
    invokeId = *specificInvokeId;
  }

  LOG_PERF("msock = %p, invId = %u, h = %d, op = %d, k = %u, startMinkIPC \n",
           me, invokeId, h, op, k);

  ERR_CLEAN(MarshalOut_caller(me, &me->table, invokeId, h, op, args, k, &data, &sizeData,
                              fds, &numFds));

  LOG_PERF("msock = %p, invId = %u, endMarshalOutCaller \n", me, invokeId);

  InvokeInfo_init(&invInfo, h, op, args, k);
  if (0 != vm_osal_cond_init(&invInfo.cond, NULL)) {
    return Object_ERROR_MEM;
  }

  vm_osal_mutex_lock(&me->mutex);

  lxcom_inv_req *req = (lxcom_inv_req *)&data->msg;
  me->invoke_id = invokeId;
  req->hdr.invoke_id = me->invoke_id;
  invInfo.invoke_id = me->invoke_id;
  QList_appendNode(&me->qlInvokes, &invInfo.qn);

  LOG_PERF("msock = %p, invId = %u, sendMessage \n", me, invokeId);

  if (-1 == SockAgnostic_sendVec(&(me->sockAgnostic), (void *)data, sizeData, fds, numFds)) {
    QNode_dequeue(&invInfo.qn);
    err = Object_ERROR_UNAVAIL;
  }

  // in case the invoke id remains the same with the incoming invocation, so we reset it here.
  vm_osal_store_TLS_key(gSpecificInvokeId, NULL);
  //wait for the response
  if (Object_OK == err) {
    while (!invInfo.bComplete) {
      vm_osal_cond_wait(&invInfo.cond, &me->mutex, NULL);
    }
    QNode_dequeueIf(&invInfo.qn);
    err = invInfo.result;
  }
  if(Object_OK == err) {
    LOG_PERF("msock = %p, invId = %u, invokeSuccess \n", me, invokeId);
  } else {
    LOG_PERF("msock = %p, invId = %u, invokeError \n", me, invokeId);
  }
  vm_osal_mutex_unlock(&me->mutex);

cleanup:
  vm_osal_cond_deinit(&invInfo.cond);
  if (data != &flatdata) {
    HEAP_FREE_PTR(data);
  }
  return err;
}

static InvokeInfo * MinkSocket_getInvokeInfo(MinkSocket *me, uint32_t id)
{
  vm_osal_mutex_lock(&me->mutex);

  QNode *pqn;
  QLIST_FOR_ALL(&me->qlInvokes, pqn) {
    InvokeInfo *pii = c_containerof(pqn, InvokeInfo, qn);
    if (pii->invoke_id == id) {
      vm_osal_mutex_unlock(&me->mutex);
      return pii;
    }
  }
  vm_osal_mutex_unlock(&me->mutex);
  return NULL;
}

/* Client uses to SYNC clientCtlInfo with server
 * clientCtlInfo includes:
 *   - clientVersion
 */
int32_t
MinkSocket_syncCtlInfo(MinkSocket *me)
{
  int32_t err = Object_OK;
  uint32_t clientVersion = MINKSOCK_VER_LOCAL;
  lxcom_ctl clientCtlInfo = {0}, ackCtlInfo = {0};
  ObjectArg ctlInfoArgs[2] = {{{0,0}}};

  C_ZERO(clientCtlInfo);
  C_ZERO(ackCtlInfo);
  clientCtlInfo.version = clientVersion;

  ctlInfoArgs[0].b = (ObjectBuf) { &clientCtlInfo, sizeof(lxcom_ctl) };
  ctlInfoArgs[1].b = (ObjectBuf) { &ackCtlInfo, sizeof(lxcom_ctl) };

  LOG_TRACE("Client is syncing ctlInfo(version=%u.%02u) with server, minksock=%p.\n",
            GET_MAJOR_VER(clientVersion), GET_MINOR_VER(clientVersion), me);

  ERR_CLEAN(MinkSocket_invoke(me, MINKSOCK_CTL_HANDLE, MINKSOCK_CTL_OP_SYNC,
                              ctlInfoArgs, ObjectCounts_pack(1, 1, 0, 0)));

  LOG_TRACE("Server acks version=%u.%02u to client, minksock=%p.\n",
            GET_MAJOR_VER(ackCtlInfo.version), GET_MINOR_VER(ackCtlInfo.version), me);

  ERR_CLEAN(MinkSocket_setVersion(me, ackCtlInfo.version));

cleanup:

  return err;
}

/* Server uses to ACK ackCtlInfo to client
 * ackCtlInfo includes:
 *   - ackVersion
 */
static int32_t
MinkSocket_ackCtlInfo(MinkSocket *me, ObjectArg *args, ObjectCounts k)
{
  int32_t err = Object_OK;
  uint32_t serverVersion = MINKSOCK_VER_LOCAL;
  uint32_t clientVersion = MINKSOCK_VER_UNINITIALIZED;
  uint32_t ackVersion = MINKSOCK_VER_UNINITIALIZED;

  if (k != ObjectCounts_pack(1, 1, 0, 0) ||
      args[0].b.size != sizeof(lxcom_ctl) ||
      args[0].b.size != sizeof(lxcom_ctl)) {
    LOG_ERR("Invalid args for CTL INFO SYNC request.\n");
    return Object_ERROR_INVALID;
  }

  const lxcom_ctl *clientCtlInfo = (const lxcom_ctl *)args[0].b.ptr;
  lxcom_ctl *ackCtlInfo = (lxcom_ctl *)args[1].b.ptr;

  clientVersion = clientCtlInfo->version;

  LOG_TRACE("Comparing: server version=%u.%02u, client version=%u.%02u\n",
            GET_MAJOR_VER(serverVersion), GET_MINOR_VER(serverVersion),
            GET_MAJOR_VER(clientVersion), GET_MINOR_VER(clientVersion));

  // ackVersion = min(clientVersion, serverVersion)
  ackVersion = (serverVersion < clientVersion) ? serverVersion : clientVersion;

  C_ZERO(*ackCtlInfo);
  ackCtlInfo->version = ackVersion;
  err = MinkSocket_setVersion(me, ackVersion);

  return err;
}

/******************************************************************
  send close(LXCOM_CLOSE) massage to the receiver
  caller takes care of the mutex
*******************************************************************/
int32_t MinkSocket_sendClose(MinkSocket *me, int handle)
{
  int32_t ret;
  int size = sizeof(lxcom_inv_close);

  if (me == NULL || me->bDone) {
    return Object_ERROR_UNAVAIL;
  }

  lxcom_inv_close cls = (lxcom_inv_close) {size, LXCOM_CLOSE, handle};
  LOG_PERF("msock = %p, closeId = %d, sendClose \n", me, handle);

  ret = SockAgnostic_sendMsg(&(me->sockAgnostic), &cls, size);
  return (ret == -1) ? Object_ERROR_UNAVAIL : Object_OK;
}

/* Handles Invoke Success messages and marshals out arguments
 *
 * The Success buffer (mb->msg.succ of type lxcom_inv_succ) is
 * used to send information about out buffers and out objects.
 * MinkSocket_SendInvokeSuccess populates this with output argument
 * details.
 * Indicies from 0-numBO pertain to out Buffers and
 * Indicies from numBo-numOO pertain to out Objects
*/
static
int32_t MinkSocket_recvInvocationSuccess(MinkSocket *me, FlatData *data, size_t sizeData,
                                         int *fds, int numFds)
{
  int32_t err = Object_OK;
  InvokeInfo *pii = NULL;

  pii = MinkSocket_getInvokeInfo(me, data->hdr.invoke_id);
  if (NULL == pii) {
    LOG_ERR("incredible failure to get invokeInfo of invoke_id %d in minksocket %p\
         Leading to caller never be replied\n", data->hdr.invoke_id, (void *)me);
    return Object_ERROR_UNAVAIL;
  }

  err = MarshalIn_caller(me, &me->table, data, sizeData, fds, numFds, pii->k, pii->args);

  LOG_PERF("msock = %p, invId = %d, endMarshalInCaller \n", me, pii->invoke_id);

  if (err) {
    LOG_ERR("MarshalIn_caller error = %d\n", err);
    goto cleanup;
  }

cleanup:
  InvokeInfo_setResult(pii, err);
  return err;
}

static
int32_t MinkSocket_recvInvocationError(MinkSocket *me, FlatData *mb)
{
  int32_t err = Object_OK;
  InvokeInfo *pii = NULL;

  if (mb->hdr.size != sizeof(lxcom_inv_err)) {
    return Object_ERROR_INVALID;
  }

  LOG_TRACE("minkSocket = %p, err = %d\n", me, mb->msg.err.err);
  LOG_PERF("msock = %p, invId = %u, recvERROR \n", me, mb->hdr.invoke_id);

  pii = MinkSocket_getInvokeInfo(me, mb->hdr.invoke_id);
  if (NULL == pii) {
    LOG_ERR("incredible failure to get invokeInfo of invoke_id %d in minksocket %p\
         Leading to caller never be replied\n", mb->hdr.invoke_id, (void *)me);
    return Object_ERROR_UNAVAIL;
  }

  InvokeInfo_setResult(pii, mb->msg.err.err);

  return err;
}

static
int32_t MinkSocket_recvClose(MinkSocket *me, FlatData *mb)
{
  if (mb->hdr.size != sizeof(lxcom_inv_close)) {
    return Object_ERROR_INVALID;
  }

  LOG_TRACE("minkSocket = %p, table = %p, handle = %d\n", me, &me->table,
             mb->msg.close.handle);
  LOG_PERF("msock = %p, closeId = %u, startCLOSE \n",me, mb->msg.close.handle);

  if (ObjectTable_releaseHandle(&me->table, mb->msg.close.handle)) {
    return Object_ERROR;
  }

  LOG_PERF("msock = %p, closeId = %u, endCLOSE \n", me, mb->msg.close.handle);

  return Object_OK;
}

static
int32_t MinkSocket_sendInvokeSuccess(MinkSocket *me, lxcom_inv_req *req, ObjectArg *args)
{
  int32_t err = Object_OK;
  FlatData flatdata;
  FlatData *data = &flatdata;
  size_t sizeData = 0;
  int fds[ObjectCounts_maxOI] = {0};
  memset(fds, -1, sizeof(fds));
  size_t numFds = C_LENGTHOF(fds);

  err = MarshalOut_callee(me, &me->table, req->hdr.invoke_id, args, req->k,
                          &data, &sizeData, fds, &numFds);

  LOG_PERF("msock = %p, invId = %u, sendSUCCESS \n", me, req->hdr.invoke_id);

  if (err) {
    LOG_ERR("MarshalOut_caller fail\n");
    return Object_ERROR_INVALID;
  }

  vm_osal_mutex_lock(&me->mutex);
  if (-1 == SockAgnostic_sendVec(&(me->sockAgnostic), (void *)data, sizeData, fds, numFds)) {
    if (errno == ENOMEM) {
      LOG_ERR("memory not available in transport\n");
      err = Object_ERROR_MEM;
    } else {
      LOG_ERR("Error sending message: returning Object_ERROR_DEFUNCT\n");
      err = Object_ERROR_DEFUNCT;
    }
  }
  vm_osal_mutex_unlock(&me->mutex);

  if (data != &flatdata) {
    HEAP_FREE_PTR(data);
  }

  return err;
}

static
int32_t MinkSocket_sendInvokeError(MinkSocket *me, lxcom_inv_req *req, int32_t error)
{
  int32_t ret;
  lxcom_inv_err err;
  err.hdr.type = LXCOM_ERROR;
  err.hdr.size = sizeof(err);
  err.hdr.invoke_id = req->hdr.invoke_id;
  err.err = error;

  LOG_PERF("msock = %p, invId = %u, sendERROR \n", me, req->hdr.invoke_id);

  LOG_TRACE("minkSocket = %p, sockAgnostic = %p, error = %d\n", me,
             &(me->sockAgnostic), error);
  vm_osal_mutex_lock(&me->mutex);
  ret = SockAgnostic_sendMsg(&(me->sockAgnostic), &err, err.hdr.size);
  vm_osal_mutex_unlock(&me->mutex);

  return (ret == -1) ? Object_ERROR_DEFUNCT : Object_OK;
}

static
int32_t MinkSocket_recvInvocationRequest(MinkSocket *me, FlatData *data, size_t sizeData,
                                         int *fds, int numFds)
{
  int32_t err = Object_OK, errInvoke = Object_OK;
  ObjectArg args[LXCOM_MAX_ARGS] = {{{0,0}}};
  char resvBuf[MSG_BUFFER_PREALLOC] = {0};
  void *ptrResv = resvBuf;
  Object targetObj = Object_NULL;
  uint32_t *specificInvoke = NULL;

  specificInvoke = HEAP_ZALLOC_REC(uint32_t);
  if (NULL == specificInvoke) {
    LOG_ERR("Memory allocation failure for specific invoke id\n");
    ERR_CLEAN(Object_ERROR_KMEM);
  }
  *specificInvoke = data->hdr.invoke_id;
  vm_osal_store_TLS_key(gSpecificInvokeId, specificInvoke);

  LOG_PERF("msock = %p, invId = %u, startMarshalInCallee \n",
            me, data->msg.req.hdr.invoke_id);

  ERR_CLEAN(MarshalIn_callee(me, &me->table, data, sizeData,
                             fds, numFds, args, &ptrResv, MSG_BUFFER_PREALLOC));

  LOG_PERF("msock = %p, invId = %u, endMarshalInCallee \n", me, data->msg.req.hdr.invoke_id);

  LOG_PERF("msock = %p, invId = %u, op = %d, k = %d, recvInvocation \n",
           me, data->msg.req.hdr.invoke_id, data->msg.req.op,
           data->msg.req.k);

  LOG_TRACE("get object out with minkSocket = %p, objectTable = %p, request handle = %d\n",
             me, &me->table, data->msg.req.handle);

  if (data->msg.req.handle == MINKSOCK_CTL_HANDLE &&
      data->msg.req.op == MINKSOCK_CTL_OP_SYNC &&
      data->msg.req.k == ObjectCounts_pack(1, 1, 0, 0)) {

    // CTL INFO SYNC request
    LOG_TRACE("Received MS_CTL_INFO_SYNC request with minksock=%p.\n", me);
    errInvoke = MinkSocket_ackCtlInfo(me, args, data->msg.req.k);
  } else {
    // Normal request
    targetObj = ObjectTable_recoverObject(&me->table, data->msg.req.handle);
    if (Object_isNull(targetObj)) {
      LOG_ERR("target object %d is NULL !!\n", data->msg.req.handle);

      // Clean up request/input data before sending back response/output.
      MarshalIn_calleeRelease(data->msg.req.k, args, fds, numFds);

      err = MinkSocket_sendInvokeError(me, &data->msg.req, Object_ERROR_INVALID);
      goto cleanup;
    }

    errInvoke = Object_invoke(targetObj, ObjectOp_methodID(data->msg.req.op),
                              args, data->msg.req.k);
  }

  LOG_PERF("msock = %p, invId = %u, ObjHandle = %d, endInvokation \n",
           me, data->msg.req.hdr.invoke_id, data->msg.req.handle);

  // Clean up request/input data before sending back response/output.
  MarshalIn_calleeRelease(data->msg.req.k, args, fds, numFds);
  LOG_PERF("msock = %p, invId = %u, OI = %zu, endCleanInbound \n", me,
           data->msg.req.hdr.invoke_id, ObjectCounts_numOI(data->msg.req.k));

  if (Object_OK != errInvoke) {
    LOG_ERR("Not the MinkSocket but the service[%d] behind reports error = %d\n",
            data->msg.req.handle, errInvoke);
    err = MinkSocket_sendInvokeError(me, &data->msg.req, errInvoke);
    goto cleanup;
  }

  //send Success unless there is an internal error
  err = MinkSocket_sendInvokeSuccess(me, &data->msg.req, args);
  if (Object_OK != err) {
    LOG_ERR("fail to sendInvokeSuccess for request on target handle %d,\
         error = %d\n", data->msg.req.handle, err);
    goto cleanup;
  }

cleanup:
  if (Object_OK != err) {
    MinkSocket_sendInvokeError(me, &data->msg.req, err);
  }

  /* Clean up response/output data.
     Note that MarshalOut_callee() is processed in MinkSocket_sendInvokeSuccess()
   */
  MarshalOut_calleeRelease(resvBuf, ptrResv, targetObj, data->msg.req.k, args, errInvoke);

  if (NULL != specificInvoke) {
    vm_osal_store_TLS_key(gSpecificInvokeId, NULL);
    free(specificInvoke);
  }

  return err;
}

typedef struct {
  MinkSocket *minksock;
  void *data;
  size_t sizeData;
} MinkContext;

static
int32_t MinkSocket_processException(MinkSocket *me, int32_t exception)
{
  int32_t ret = Object_OK;

  if (Object_ERROR_DEFUNCT == exception) {
    if (MinkSocket_isFirstShot(me)) {
      MinkSocket_close(me, exception);
      // close all gateway handles as remote
      // cannot trigger a release after fd is closed
      ObjectTable_closeAllHandles(&me->table);
      ret = MinkSocket_detachForwarderAll(me);
    }
  } else {
    LOG_ERR("Unexpected error occurs %d\n", exception);
  }

  return ret;
}

static
int32_t MinkSocket_processMsgInternal(MinkSocket *me, FlatData *msg, size_t sizeMsg,
                                      int *fds, int numFds)
{
  int32_t ret = Object_OK;

  switch (msg->hdr.type) {
    case LXCOM_REQUEST:
      ret = MinkSocket_recvInvocationRequest(me, msg, sizeMsg, fds, numFds);
      break;

    case LXCOM_SUCCESS:
      ret = MinkSocket_recvInvocationSuccess(me, msg, sizeMsg, fds, numFds);
      break;

    case LXCOM_ERROR:
      ret = MinkSocket_recvInvocationError(me, msg);
      break;

    case LXCOM_CLOSE:
      ret = MinkSocket_recvClose(me, msg);
      break;

    default:
      ret = Object_ERROR_DEFUNCT;
  }

  return ret;
}

void* MinkSocket_processMsg(void *data)
{
  int32_t status = Object_OK;
  int32_t ret = Object_OK;
  MinkContext *ctx = (MinkContext *)data;
  FlatData *msg = (FlatData *)ctx->data;
  size_t sizeMsg = ctx->sizeData;

  status = MinkSocket_processMsgInternal(ctx->minksock, msg, sizeMsg, NULL, 0);
  if (Object_OK != status) {
    ret = MinkSocket_processException(ctx->minksock, status);
  }

  // Object_ERROR_UNAVAIL indicates the MinkSocket instance has been freed in disorder
  if (Object_ERROR_UNAVAIL != ret) {
    MinkSocket_dequeue(ctx->minksock);
  }

  HEAP_FREE_PTR(msg);
  HEAP_FREE_PTR(data);

  return NULL;
}

void *MinkSocket_processReq(void *data)
{
  int32_t ret = Object_OK;
  //int32_t status = Object_OK;
  bool locked = false;
  FlatData defBuf;
  void *msgBuf = &defBuf;
  size_t sizeMsg = 0;;
  int fds[ObjectCounts_maxOI] = {0};
  memset(fds, -1, sizeof(fds));
  size_t numFds = ObjectCounts_maxOI;
  MinkSocket *me = (MinkSocket *)data;

  ret = SockAgnostic_prepareBuffer(&(me->sockAgnostic), &msgBuf, &sizeMsg);
  TRUE_OR_CLEAN(Object_OK == ret, "Failed on SockAgnostic_prepareBuffer(), ret %d, IPCtype %d\n",
                ret, me->sockAgnostic.sockType);

  if (MinkSocket_isAlive(me)) {
    ret = SockAgnostic_poll(&(me->sockAgnostic), &(me->sockPair[0]), 1, -1);
    SILENT_CHECK(Object_OK == ret);

    /*
      hold mutex while minksocket receiving message, actual execution of MinkSocket_release()
      will be delayed until receiving finished.
    */
    vm_osal_mutex_lock(&me->mutex);
    locked = true;

    ret = SockAgnostic_recv(&(me->sockAgnostic), (void **)&msgBuf, &sizeMsg, fds, &numFds);
    TRUE_OR_CLEAN(Object_OK == ret, "Failed on SockAgnostic_recvMsgGeneric(), ret %d, IPCtype %d\n",
                  ret, me->sockAgnostic.sockType);

    Marshal_perfEntryTag(me, ((FlatData *)msgBuf)->hdr.type, (FlatData *)msgBuf);

    vm_osal_mutex_unlock(&me->mutex);
    locked = false;
  }

  ThreadWork* work = NULL;
  if (!me->bDone && SockAgnostic_isAlive(&(me->sockAgnostic)) && ret == Object_OK) {
    work = HEAP_ZALLOC_REC(ThreadWork);
    if (!work) {
      LOG_ERR("Failed to allocate ThreadWork\n");
      ret = Object_ERROR_MEM;
      goto cleanup;
    }
    ThreadWork_init(work, MinkSocket_processReq, me);
    MinkSocket_enqueue(me, work);
  }

  ret = MinkSocket_processMsgInternal(me, (FlatData *)msgBuf, sizeMsg, fds, numFds);

cleanup:
  if (locked) {
    vm_osal_mutex_unlock(&me->mutex);
  }

  if (Object_OK != ret) {
    ret = MinkSocket_processException(me, ret);
  }

  if (msgBuf != &defBuf) {
    HEAP_FREE_PTR(msgBuf);
  }

  if (Object_ERROR_UNAVAIL != ret) {
    MinkSocket_dequeue(me);
  }

  return NULL;
}

int32_t MinkSocket_handleConnected(MinkSocket *me)
{
  ThreadWork* work = HEAP_ZALLOC_REC(ThreadWork);
  if (!work) {
    LOG_ERR("Failed to allocate ThreadWork\n");
    return Object_ERROR_MEM;
  }
  ThreadWork_init(work, MinkSocket_processReq, me);
  MinkSocket_enqueue(me, work);

  return Object_OK;
}

void MinkSocket_start(MinkSocket *me, int sock)
{
  (void)sock;

  MinkSocket_handleConnected(me);
}
