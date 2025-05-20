/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#include <sys/socket.h>
#include <sys/un.h>
#include "cdefs.h"
#include "check.h"
#include "Heap.h"
#include "memscpy.h"
#include "minkipc.h"
#include "minksocket.h"
#include "msforwarder.h"
#include "fdwrapper.h"
#include "qlist.h"
#include "SockAgnostic.h"
#include "Types.h"
#include "Utils.h"
#include "Profiling.h"
#include "Marshalling.h"

#define GENERIC_HANDLE            0

extern
size_t strlcpy(char *dst, const char *src, size_t size);

extern
int itoa(long in, char *buffer);

typedef struct MinkSession {
  QNode qn;
  MinkSocket *minksock;
} MinkSession;

struct MinkIPC {
  int refs;
  bool bServer;
  bool bModule;
  bool bReady;
  bool bDone;
  bool bServerDone;
  IPC_TYPE ipcType;
  SockAgnostic sockAgnostic;
  Object endpoint;
  vm_osal_thread listenerThread;
  QList sessionList;
  vm_osal_mutex mutex;
  vm_osal_cond cond;
};

static inline
void MinkIPC_closeSession(MinkIPC *me, MinkSession *session, int32_t reason)
{
  (void)me;
  MinkSocket_close(session->minksock, reason);
  MinkSocket_detachForwarderAll(session->minksock);
}

static
void MinkIPC_removeSession(MinkIPC *me, MinkSession *session)
{
  (void)me;
  MinkSocket_release(session->minksock);
  QNode_dequeue(&(session->qn));
  HEAP_FREE_PTR(session);
}

/*@brief: Close all affiliated MinkSocket connections after MinkIPC set to be bDone
 *        Considering performance impact, the list is not protected by mutex because
 *        nothing added/removed during iteration. Or user take care of the mutex.
*/
static
void MinkIPC_closeAllSessions(MinkIPC *me)
{
  QNode * pQn = NULL;
  QNode * pQnNext = NULL;
  MinkSession *session = NULL;

  QLIST_NEXTSAFE_FOR_ALL(&me->sessionList, pQn, pQnNext) {
    session = c_containerof(pQn, MinkSession, qn);
    if (NULL == session) {
      LOG_ERR("Weird, failed to get minkSession, minkipc %p\n", me);
      continue;
    } else {
      MinkIPC_closeSession(me, session, Object_ERROR_UNAVAIL);
    }
  }
}

/*@brief: Much performance sacrificed if introduce mutex to protect list operation here
 *        As a compromise, caller must ensure there is no race condition invoking
 *
 * @param[deadOnly] discriminate cleanup closed all connections or not
*/
static
void MinkIPC_cleanupSessions(MinkIPC *me, bool deadOnly)
{
  QNode * pQn = NULL;
  QNode * pQnNext = NULL;
  MinkSession *session = NULL;

  QLIST_NEXTSAFE_FOR_ALL(&me->sessionList, pQn, pQnNext) {
    session = c_containerof(pQn, MinkSession, qn);
    if (NULL == session) {
      LOG_ERR("Weird, failed to get minkSession, minkipc %p\n", me);
      continue;
    } else {
      if (!deadOnly) {
        MinkIPC_closeSession(me, session, Object_ERROR_UNAVAIL);
      }
      if (!MinkSocket_isAlive(session->minksock)) {
        MinkIPC_removeSession(me, session);
      }
    }
  }
}

static inline
void MinkIPC_cleanupDeadSessions(MinkIPC *me)
{
  return MinkIPC_cleanupSessions(me, true);
}

static inline
void MinkIPC_cleanupAllSessions(MinkIPC *me)
{
  return MinkIPC_cleanupSessions(me, false);
}

static
int32_t MinkIPC_retrieveSession(MinkIPC *me, SockAgnostic *inSockAgn,
                                MinkSession **objOut)
{
  QNode *pQn = NULL;
  MinkSession *session = NULL;
  SockAgnostic *pSockAgn = NULL;

  QLIST_FOR_ALL(&me->sessionList, pQn) {
    session = c_containerof(pQn, MinkSession, qn);
    if (NULL == session) {
      LOG_ERR("Weird, failed to get minkSession, minkipc %p\n", me);
      continue;
    } else {
      MinkSocket_getSockAgnostic(session->minksock, &pSockAgn);
      if (SockAgnostic_fullMatched(pSockAgn, inSockAgn)) {
        *objOut = session;
        return Object_OK;
      }
    }
  }

  *objOut = NULL;
  return Object_ERROR;
}

static
int32_t MinkIPC_newSession(MinkIPC *me, SockAgnostic *inSockAgn,
                           MinkSession **objOut)
{
  int32_t ret = Object_OK;
  MinkSession *session = NULL;
  MinkSocket *minksock = NULL;

  session= HEAP_ZALLOC_REC(MinkSession);
  if (NULL == session) {
    LOG_ERR("Failed to allocate MinkSession\n");
    ret = Object_ERROR_MEM;
    goto cleanup;
  }

  minksock = MinkSocket_new_internal(inSockAgn, me->endpoint);
  if (NULL == minksock) {
    LOG_ERR("Failed on MinkSocket_new_internal()\n");
    ret = Object_ERROR;
    goto cleanup;
  }

  if (me->bServer) {
    if (me->bModule) {
      ret = MinkSocket_initCredentials(minksock, me->bModule);
      TRUE_OR_CLEAN(Object_OK == ret,
                    "Failed on MinkSocket_initCredentials(), ret %d\n", ret);
    }

    ret = MinkSocket_initObjectTable(minksock, me->endpoint, me->bModule);
    TRUE_OR_CLEAN(Object_OK == ret,
                  "Failed on MinkSocket_initObjectTable(), ret %d\n", ret);
  }

  session->minksock = minksock;
  QList_appendNode(&me->sessionList, &session->qn);
  *objOut = session;
  LOG_TRACE("In minkipc %p, constructed MinkSession = %p with MinkSocket %p \
             in sockType = %d\n", me, session, minksock, SockAgnostic_getSockType(inSockAgn));
  return Object_OK;

cleanup:
  LOG_ERR("Error occurs, releasing MinkSession\n");
  if (NULL != minksock) {
    MinkSocket_release(minksock);
  }
  if (NULL != session) {
    HEAP_FREE_PTR(session);
  }
  *objOut = NULL;
  return ret;
}

static
int32_t MinkIPC_retrieveOrNewSession(MinkIPC *me, SockAgnostic *inSockAgn,
                                     MinkSession **objOut)
{
  int32_t ret = Object_OK;

  ret = MinkIPC_retrieveSession(me, inSockAgn, objOut);
  if (Object_OK != ret){
    return MinkIPC_newSession(me, inSockAgn, objOut);
  }

  return Object_OK;
}

static void *MinkIPC_serviceConnected(void *data)
{
  int32_t ret = 0;
  MinkIPC *me = (MinkIPC *)data;
  SockAgnostic newSockAgn = {0};
  MinkSession *session = NULL;

  vm_osal_mutex_lock(&me->mutex);
  me->bReady = true;
  vm_osal_cond_set(&me->cond);
  vm_osal_mutex_unlock(&me->mutex);

  do {
    MinkIPC_cleanupDeadSessions(me);

    ret = SockAgnostic_accept(&(me->sockAgnostic), &newSockAgn);
    if (ret <= 0) {
      LOG_ERR("Failed on SockAgnostic_accept() with ret %d. Probably the \
                 application releases minkipc proactively.\n", ret);
      continue;
    }

    ret = MinkIPC_newSession(me, &newSockAgn, &session);
    if (Object_OK != ret) {
      LOG_ERR("Failed on MinkIPC_newSession(). Abandon accepted connection\n");
      SockAgnostic_shutdown(&newSockAgn, SHUT_RDWR);
      SockAgnostic_close(&newSockAgn);
      continue;
    } else {
      MinkSocket_handleConnected(session->minksock);
    }
    /*if (me == NULL) {
        LOG_ERR("Abort as me is NULL unexpectedly\n");
        return NULL;
    }*/
  } while(!me->bDone);

  vm_osal_mutex_lock(&me->mutex);
  MinkIPC_closeAllSessions(me);
  me->bServerDone = true;
  vm_osal_cond_set(&me->cond);
  vm_osal_mutex_unlock(&me->mutex);

  return NULL;
}

static
MinkIPC *MinkIPC_new(SockAgnostic sockAgnostic, int32_t ipcType,
                     Object endpoint, int32_t endpointType)
{
  int32_t ret = Object_OK;
  MinkIPC *me = NULL;

  if (!SockAgnostic_isAlive(&sockAgnostic)) {
    LOG_ERR("Invalid input\n");
    return NULL;
  }

  me = HEAP_ZALLOC_REC(MinkIPC);
  if (!me) {
    LOG_ERR("Failed to allocate MinkIPC\n");
    return NULL;
  }

  me->refs = 1;
  me->bDone = false;
  me->ipcType = ipcType;
  Object_ASSIGN(me->endpoint, endpoint);
  me->bServer = !(Object_isNull(endpoint));
  if (endpointType == MODULE) {
    me->bModule = true;
  }
  memscpy(&(me->sockAgnostic), sizeof(SockAgnostic),
          &sockAgnostic, sizeof(SockAgnostic));
  QList_construct(&me->sessionList);
  ret = vm_osal_mutex_init(&me->mutex, NULL);
  TRUE_OR_CLEAN(Object_OK == ret, "Failed to init mutex\n");
  ret = vm_osal_cond_init(&me->cond, NULL);
  TRUE_OR_CLEAN(Object_OK == ret, "Failed to init cond\n");

  return me;

cleanup:
  LOG_ERR("Error occurs, releasing MinkIPC = %p\n", me);
  MinkIPC_release(&me);
  return NULL;
}

int32_t MinkIPC_beginService(const char *address, int32_t sockFd, int32_t ipcType,
                             Object endpoint, int32_t endpointType,
                             MinkIPC** instanceOut)
{
  int32_t ret = Object_OK;
  MinkIPC *me = NULL;
  SockAgnostic sockAgnostic = {0};

  Utils_configTrace();
  Profiling_configProfile();

  ret = SockAgnostic_new(address, sockFd, ipcType, true, &sockAgnostic);
  if (Object_OK != ret) {
    LOG_ERR("Failed on SockAgnostic_new(), ret = %d\n", ret);
    return ret;
  }

  me = MinkIPC_new(sockAgnostic, ipcType, endpoint, endpointType);
  if (!me) {
    LOG_ERR("Failed on MinkIPC_new()\n");
    return Object_ERROR;
  }

  // The parameters of address and sockFd are exclusive.
  // When address is NULL, it means new sockFd need to be created and initialized. 
  if (address != NULL) {
    ret = SockAgnostic_bind(&(me->sockAgnostic));
    TRUE_OR_CLEAN(Object_OK == ret, "Failed on SockAgnostic_bind(), ret %d\n", ret);
  }

  ret = SockAgnostic_listen(&(me->sockAgnostic), MAX_QUEUE_LENGTH);
  TRUE_OR_CLEAN(Object_OK == ret, "Failed on SockAgnostic_listen(), ret %d\n", ret);
  ret = vm_osal_thread_create(&me->listenerThread, MinkIPC_serviceConnected, me, NULL);
  TRUE_OR_CLEAN(Object_OK == ret, "Failed to create thread , ret %d\n", ret);

  vm_osal_mutex_lock(&me->mutex);
  while (!me->bReady) {
    vm_osal_cond_wait(&me->cond, &me->mutex, NULL);
  }
  vm_osal_mutex_unlock(&me->mutex);
  LOG_TRACE("constructed minkipc = %p, sockAgnostic = %p, ipctype = %d, endpoint = %p\n",
            me, &me->sockAgnostic, me->ipcType, &endpoint);

  *instanceOut = me;
  return Object_OK;

cleanup:
  LOG_ERR("Error occurs, releasing MinkIPC = %p\n", me);
  MinkIPC_release(&me);
  return ret;
}

MinkIPC *MinkIPC_startService(const char *address, Object endpoint)
{
  int32_t ret = Object_OK;
  MinkIPC* instanceOut = NULL;

  ret = MinkIPC_beginService(address, -1, UNIX, endpoint, OPENER, &instanceOut);
  if (Object_OK == ret) {
    return instanceOut;
  } else {
    return NULL;
  }
}

MinkIPC * MinkIPC_startServiceOnSocket(int32_t sock, Object endpoint)
{
  int32_t ret = Object_OK;
  MinkIPC* instanceOut = NULL;

  ret = MinkIPC_beginService(NULL, sock, UNIX, endpoint, OPENER, &instanceOut);
  if (Object_OK == ret) {
    return instanceOut;
  } else {
    return NULL;
  }
}

MinkIPC *MinkIPC_startServiceModule(const char *address, Object endpoint)
{
  int32_t ret = Object_OK;
  MinkIPC* instanceOut = NULL;

  ret = MinkIPC_beginService(address, -1, UNIX, endpoint, MODULE, &instanceOut);
  if (Object_OK == ret) {
    return instanceOut;
  } else {
    return NULL;
  }
}

MinkIPC * MinkIPC_startServiceModuleOnSocket(int32_t sock, Object endpoint)
{
  int32_t ret = Object_OK;
  MinkIPC* instanceOut = NULL;

  ret = MinkIPC_beginService(NULL, sock, UNIX, endpoint, MODULE, &instanceOut);
  if (Object_OK == ret) {
    return instanceOut;
  } else {
    return NULL;
  }
}

MinkIPC *MinkIPC_startServiceModule_simulated(const char *address, Object endpoint)
{
  int32_t ret = Object_OK;
  MinkIPC* instanceOut = NULL;

  ret = MinkIPC_beginService(address, -1, SIMULATED, endpoint, MODULE, &instanceOut);
  if (Object_OK == ret) {
    return instanceOut;
  } else {
    return NULL;
  }
}

MinkIPC * MinkIPC_startServiceModuleOnSocket_simulated(int32_t sock, Object endpoint)
{
  int32_t ret = Object_OK;
  MinkIPC* instanceOut = NULL;

  ret = MinkIPC_beginService(NULL, sock, SIMULATED, endpoint, MODULE, &instanceOut);
  if (Object_OK == ret) {
    return instanceOut;
  } else {
    return NULL;
  }
}

/**
   wait for the service to finish ..
   waits until stopped or the service dies
**/
void MinkIPC_join(MinkIPC *me) {
  if (me->bServer && me->listenerThread) {
    //wait for thread to die
    vm_osal_thread_join(me->listenerThread, NULL);
    me->listenerThread = 0;
  }
}

/*@brief: actual implementation to connect target Mink service for kinks of scenarios
 *
 *@param[in] address: string of socket file name or port number of target Mink service
 *@param[in] ipcType: protocol type based on which established interaction works
 *@param[out] proxyOut: object output standing for proxy of target service
 *@param[out] instanceOut: MinkIPC instance pointing to established Mink interaction
 *
 *@return: succeed: Object_OK. fail: other positive or negative value;
*/
static
int32_t MinkIPC_connectService(const char *address, int32_t ipcType, Object *proxyOut,
                               MinkIPC** instanceOut)
{
  int32_t ret = Object_OK;
  MinkIPC *me = NULL;
  SockAgnostic sockAgnostic = {0};
  MinkSession *session = NULL;

  Utils_configTrace();
  Profiling_configProfile();

  ret = SockAgnostic_new(address, -1, ipcType, false, &sockAgnostic);
  if (Object_OK != ret) {
    LOG_ERR("Failed on SockAgnostic_new()\n");
    return ret;
  }

  me = MinkIPC_new(sockAgnostic, ipcType, Object_NULL, 0);
  if (!me) {
    LOG_ERR("Failed on MinkIPC_new()\n");
    return Object_ERROR;
  }

  ret = SockAgnostic_connect(&(me->sockAgnostic));
  TRUE_OR_CLEAN(Object_OK == ret,
		"Failed on SockAgnostic_connect(), ret %d, errno %d, ipcType %d\n", ret, errno, ipcType);
  ret = MinkIPC_newSession(me, &(me->sockAgnostic), &session);
  TRUE_OR_CLEAN(Object_OK == ret,"Failed on MinkIPC_newSession(), ret %d\n", ret);

  MinkSocket_handleConnected(session->minksock);

  if (Object_isERROR(MinkSocket_syncCtlInfo(session->minksock))) {
    // Set the version to legacy version if sync failed
    ret = MinkSocket_setVersion(session->minksock, MINKSOCK_VER_LEGACY);
    TRUE_OR_CLEAN(Object_OK == ret,"Failed to MinkSocket_setVersion(), ret %d\n", ret);

    LOG_MSG("Failed to sync clientCtlInfo(version) with server, \
            client will keep using default ctlInfo(version=%u.%02u), \
            session=%p\n", GET_MAJOR_VER(MINKSOCK_VER_LEGACY), GET_MINOR_VER(MINKSOCK_VER_LEGACY),
            session);
  }

  ret = MSForwarder_new(session->minksock, GENERIC_HANDLE, proxyOut);
  TRUE_OR_CLEAN(Object_OK == ret, "Failed on MSForwarder_new(), ret %d\n", ret);

  LOG_TRACE("constructed minkipc = %p, sockAgnostic = %p, ipctype = %d, session = %p, msforwarder = %p\n",
            me, &sockAgnostic, me->ipcType, session, proxyOut->context);
  *instanceOut = me;
  return ret;

cleanup:
  LOG_ERR("Release minkipc %p due to error\n", me);
  MinkIPC_release(&me);
  return ret;
}

MinkIPC* MinkIPC_connect_common(const char *address, Object *proxyOut, int32_t ipcType)
{
  int32_t ret = Object_OK;
  MinkIPC* instanceOut = NULL;

  ret = MinkIPC_connectService(address, ipcType, proxyOut, &instanceOut);
  if (Object_OK == ret) {
    return instanceOut;
  } else {
    return NULL;
  }
}

MinkIPC* MinkIPC_connect(const char *address, Object *proxyOut)
{
  return MinkIPC_connect_common(address, proxyOut, UNIX);
}

MinkIPC* MinkIPC_connect_simulated(const char *address, Object *proxyOut)
{
  return MinkIPC_connect_common(address, proxyOut, SIMULATED);
}

MinkIPC *MinkIPC_connectModule(const char *address, Object *proxyOut)
{
  return MinkIPC_connect_common(address, proxyOut, UNIX);
}

MinkIPC *MinkIPC_connectModule_simulated(const char *address, Object *proxyOut)
{
  return MinkIPC_connect_common(address, proxyOut, SIMULATED);
}

static void MinkIPC_stop(MinkIPC *me)
{
  vm_osal_mutex_lock(&me->mutex);
  me->bDone = true;
  if (SockAgnostic_isAlive(&(me->sockAgnostic))) {
    SockAgnostic_shutdown(&(me->sockAgnostic), SHUT_RDWR);
    SockAgnostic_close(&(me->sockAgnostic));
  }
  vm_osal_mutex_unlock(&me->mutex);

  if (me->listenerThread) {
    //Wait for thread to die, but we cannot join here, since the caller
    //might have caller MinkIPC_join. So let's use cond for it
    vm_osal_mutex_lock(&me->mutex);
    while (!me->bServerDone) {
      vm_osal_cond_wait(&me->cond, &me->mutex, NULL);
    }
    vm_osal_thread_join(me->listenerThread, NULL);
    me->listenerThread = 0;
    vm_osal_mutex_unlock(&me->mutex);
  }

  //MinkIPC_cleanupAllSessions() is not allowed to be invoked in parallel
  //Because it contains list operation but lack mutex protection for performance
  //bServerDone flag indicates receiving/accepting process has exited
  //Thus it is the only invoking in whole MinkIPC, not need mutex protection
  MinkIPC_cleanupAllSessions(me);
}

void MinkIPC_retain(MinkIPC *me)
{
  vm_osal_atomic_add(&me->refs, 1);
}

void MinkIPC_release(MinkIPC **me)
{
  if (!(*me)) {
    LOG_ERR("MinkIPC instance has been freed before invoking MinkIPC_release(),"
            "Probably mistaken or illegal operation happens in applications\n");
    return;
  }

  if (0 == vm_osal_atomic_add(&((*me)->refs), -1)) {
    LOG_TRACE("released minkipc = %p, sockAgnostic = %p, endpoint = %p\n",
              (*me), &(*me)->sockAgnostic, &(*me)->endpoint);

    MinkIPC_stop((*me));

    Object_ASSIGN_NULL((*me)->endpoint);
    vm_osal_mutex_deinit(&((*me)->mutex));
    vm_osal_cond_deinit(&((*me)->cond));
    HEAP_FREE_PTR(*me);
  }
}
