/***********************************************************************
* Copyright (c) 2022-2024 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
**********************************************************************/
#include "Heap.h"
#include "ICredentials_invoke.h"
#include "ICredentials.h"
#include "LinkCredentials.h"
#include "memscpy.h"
#include "object.h"
#include "string.h"
#include "Utils.h"
#include "vmuuid.h"

typedef struct LinkCred {
  int32_t refs;
  int32_t lloc;
  int32_t ltype;
  CredInfo credInfo;
} LinkCred;

typedef struct LinkCredRemote {
  int32_t refs;
  int32_t lloc;
  int32_t ltype;
  uint8_t lvmuuid[VMUUID_MAX_SIZE];
  size_t lvmuuidLen;
} LinkCredRemote;

typedef struct LinkCredComposite {
  int32_t refs;
  LinkCredRemote lCredRemote;
  Object hubCred;
} LinkCredComposite;

#ifndef container_of
#define container_of(ptr, type, member) \
((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/*
 *We left this query method here just for backwards compatibility.
 *They need to be replaced by "lloc", "ltype", "lpid" and "lvmuuid" in the future.
 */
static
int32_t LinkCredLegacy_getValueByName(void *me, const void *name, size_t nameLen,
                                      void *value, size_t valueLen, size_t *valueLenOut)
{
  Object hubCredTemp = Object_NULL;

  LOG_TRACE("querying on LinkCredLegacy = %p, name = %s\n", me, (const char *)name);
  if (4 == nameLen && 0 == strncmp((const char *)name, "eloc", nameLen)) {
    if (valueLen == sizeof(((LinkCred *)me)->lloc)) {
      *valueLenOut = memscpy(value, valueLen, &(((LinkCred *)me)->lloc),
                             sizeof(((LinkCred *)me)->lloc));
      return Object_OK;
    } else {
      return ICredentials_ERROR_VALUE_SIZE;
    }

  } else if (4 == nameLen && 0 == strncmp((const char *)name, "link", nameLen)) {
    if (valueLen == sizeof(((LinkCred *)me)->ltype)) {
      *valueLenOut = memscpy(value, valueLen, &(((LinkCred *)me)->ltype),
                             sizeof(((LinkCred *)me)->ltype));
      return Object_OK;
    } else {
      return ICredentials_ERROR_VALUE_SIZE;
    }

  } else if (8 == nameLen && 0 == strncmp((const char *)name, "callerId", nameLen)) {
    if ((ELOC_LOCAL == (((LinkCred *)me)->lloc))
        && (valueLen == sizeof(((LinkCred *)me)->credInfo.pid))) {
      *valueLenOut = memscpy(value, valueLen, &(((LinkCred *)me)->credInfo.pid),
                             sizeof(((LinkCred *)me)->credInfo.pid));
      return Object_OK;
    } else if ((ELOC_REMOTE == ((LinkCredRemote *)me)->lloc)) {
      hubCredTemp = ((LinkCredComposite *)container_of(me,LinkCredComposite,
                                                       lCredRemote))->hubCred;
      if (!Object_isNull(hubCredTemp)) {
        return ICredentials_getValueByName(hubCredTemp, "callerId", sizeof("callerId"), value,
                                           valueLen, valueLenOut);
      } else {
        LOG_ERR("hubCredTemp is NULL\n");
        return ICredentials_ERROR_NOT_FOUND;
      }
    } else {
      return ICredentials_ERROR_VALUE_SIZE;
    }

  } else {
    return ICredentials_ERROR_NOT_FOUND;
  }

}

static
int32_t queryLocAndType(void *me, const void *name, size_t nameLen,
                        void *value, size_t valueLen, size_t *valueLenOut)
{
  if (4 == nameLen && 0 == strncmp((const char *)name, "lloc", nameLen)) {
    if (valueLen == sizeof(((LinkCred *)me)->lloc)) {
      *valueLenOut = memscpy(value, valueLen, &(((LinkCred *)me)->lloc),
                             sizeof(((LinkCred *)me)->lloc));
      return Object_OK;
    } else {
      return ICredentials_ERROR_VALUE_SIZE;
    }

  } else if (5 == nameLen && 0 == strncmp((const char *)name, "ltype", nameLen)) {
    if (valueLen == sizeof(((LinkCred *)me)->ltype)) {
      *valueLenOut = memscpy(value, valueLen, &(((LinkCred *)me)->ltype),
                             sizeof(((LinkCred *)me)->ltype));
      return Object_OK;
    } else {
      return ICredentials_ERROR_VALUE_SIZE;
    }
  } else {
    return ICredentials_ERROR_NOT_FOUND;
  }
}

/**************************************************************************
* Chance are there race condition occurs between retain() and release()
* Extra check are introduced to mitigate possible impact. But it cannot
* eliminate the risk because me would turn to be NULL during the 2 check
* Long way to overcome it
**************************************************************************/
static inline
int32_t LinkCred_retain(LinkCred *me)
{
  if (NULL != me && vm_osal_atomic_add(&me->refs, 0) > 0) {
    vm_osal_atomic_add(&me->refs, 1);
    return Object_OK;
  }

  return Object_ERROR;
}

static inline
int32_t LinkCred_release(LinkCred *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static
int32_t LinkCred_getPropertyByIndex(LinkCred *me, uint32_t index,
                                    void *name, size_t nameLen, size_t *nameLenOut,
                                    void *value, size_t valueLen, size_t *valueLenOut)
{
  return ICredentials_ERROR_NOT_FOUND;
}

static
int32_t LinkCred_getValueByName(LinkCred *me, const void *name, size_t nameLen,
                                void *value, size_t valueLen, size_t *valueLenOut)
{
  int32_t ret = ICredentials_ERROR_NOT_FOUND;
  CredInfo info = me->credInfo;

  ret = LinkCredLegacy_getValueByName(me, name, nameLen, value, valueLen, valueLenOut);
  if (ret == Object_OK || ret == ICredentials_ERROR_VALUE_SIZE) {
    return ret;
  }

  LOG_TRACE("querying on LinkCred = %p, name = %s\n", me, (const char *)name);
  ret = queryLocAndType(me, name, nameLen, value, valueLen, valueLenOut);
  if (ret == Object_OK || ret == ICredentials_ERROR_VALUE_SIZE) {
    return ret;
  }

  if (4 == nameLen && 0 == strncmp((const char *)name, "luid", nameLen)) {
    if (valueLen == sizeof(info.uid)) {
      *valueLenOut = memscpy(value, valueLen, &(info.uid), sizeof(info.uid));
      return Object_OK;
    } else {
      return ICredentials_ERROR_VALUE_SIZE;
    }

  } else if (4 == nameLen && 0 == strncmp((const char *)name, "lpid", nameLen)) {
    if (valueLen == sizeof(info.pid)) {
      *valueLenOut = memscpy(value, valueLen, &(info.pid), sizeof(info.pid));
      return Object_OK;
    } else {
      return ICredentials_ERROR_VALUE_SIZE;
    }

  } else {
    return ICredentials_ERROR_NOT_FOUND;
  }
}

static
ICredentials_DEFINE_INVOKE(LinkCred_invoke, LinkCred_, LinkCred*);

/**************************************************************************
* Construct a LinkCred of localHub.The CredInfo is obtained via getsockopt()
*
* @param[in] info: The info used to construct the cred.
* @param[in] lloc: Distinguish whether the connection is from local or remote.
* @param[in] ltype: Type of protocol.
* @param[out] objOut: the object LinkCred to be returned.
**************************************************************************/
int32_t LinkCred_new(CredInfo *info, int32_t lloc, int32_t ltype, Object *objOut)
{
  LinkCred *me = NULL;

  if (NULL == info) {
    LOG_ERR("credInfo error to be NULL\n");
    return Object_ERROR;
  }

  me = HEAP_ZALLOC_TYPE(LinkCred);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  memscpy(&(me->credInfo), sizeof(me->credInfo), info, sizeof(*info));
  me->lloc = lloc;
  me->ltype = ltype;
  *objOut = (Object) {LinkCred_invoke, me};

  return Object_OK;
}

static inline
int32_t LinkCredRemote_retain(LinkCredRemote *me)
{
  if (NULL != me && vm_osal_atomic_add(&me->refs, 0) > 0) {
    vm_osal_atomic_add(&me->refs, 1);
    return Object_OK;
  }

  return Object_ERROR;
}

static inline
int32_t LinkCredRemote_release(LinkCredRemote *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static
int32_t LinkCredRemote_getPropertyByIndex(LinkCredRemote *me, uint32_t index,
                                          void *name, size_t nameLen, size_t *nameLenOut,
                                          void *value, size_t valueLen, size_t *valueLenOut)
{
  return ICredentials_ERROR_NOT_FOUND;
}

static
int32_t LinkCredRemote_getValueByName(LinkCredRemote *me, const void *name, size_t nameLen,
                                      void *value, size_t valueLen, size_t *valueLenOut)
{
  int32_t ret = ICredentials_ERROR_NOT_FOUND;

  ret = LinkCredLegacy_getValueByName(me, name, nameLen, value, valueLen, valueLenOut);
  if (ret == Object_OK || ret == ICredentials_ERROR_VALUE_SIZE) {
    return ret;
  }

  LOG_TRACE("querying on LinkCredRemote = %p, name = %s\n", me, (const char *)name);
  ret = queryLocAndType(me, name, nameLen, value, valueLen, valueLenOut);
  if (ret == Object_OK || ret == ICredentials_ERROR_VALUE_SIZE) {
    return ret;
  }

  if (7 == nameLen && 0 == strncmp((const char *)name, "lvmuuid", nameLen)) {
    if ((me->lvmuuidLen != 0) && (valueLen >= me->lvmuuidLen)) {
      *valueLenOut = memscpy(value, me->lvmuuidLen, me->lvmuuid, me->lvmuuidLen);
      return Object_OK;
    } else {
      return ICredentials_ERROR_VALUE_SIZE;
    }

  } else {
    return ICredentials_ERROR_NOT_FOUND;
  }
}

static
ICredentials_DEFINE_INVOKE(LinkCredRemote_invoke, LinkCredRemote_, LinkCredRemote*);

/**************************************************************************
* Construct a LinkCredRemote of remoteHub.The VMUUID is hard coded in
* SockAgnostic_getPeerIdentity().
*
* @param[in] vmuuid: The UUID of remoteHub.
* @param[in] vmuuidLen: The length of UUID
* @param[in] lloc: Distinguish whether the connection is from local or remote.
* @param[in] ltype: Type of protocol.
* @param[out] objOut: the object LinkCredRemote to be returned.
**************************************************************************/
int32_t LinkCredRemote_new(uint8_t const *vmuuid, size_t vmuuidLen, int32_t lloc,
                           int32_t ltype, Object *objOut)
{
  LinkCredRemote *me = NULL;

  if ((NULL == vmuuid) || (0 == vmuuidLen) || (vmuuidLen > VMUUID_MAX_SIZE)) {
    LOG_ERR("vmuuid illegal\n");
    return Object_ERROR_SIZE_IN;
  }
  if ((ELOC_LOCAL == lloc) || (UNIX == ltype)) {
    LOG_ERR("lloc or ltype wrong\n");
    return Object_ERROR;
  }

  me = HEAP_ZALLOC_TYPE(LinkCredRemote);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  me->lloc = lloc;
  me->ltype = ltype;
  me->lvmuuidLen = memscpy(me->lvmuuid, VMUUID_MAX_SIZE, vmuuid, vmuuidLen);
  *objOut = (Object) {LinkCredRemote_invoke, me};

  return Object_OK;
}

static inline
int32_t LinkCredComposite_retain(LinkCredComposite *me)
{
  if (NULL != me && vm_osal_atomic_add(&me->refs, 0) > 0) {
    vm_osal_atomic_add(&me->refs, 1);
    return Object_OK;
  }

  return Object_ERROR;
}

static inline
int32_t LinkCredComposite_release(LinkCredComposite *me)
{
  if (vm_osal_atomic_add(&me->refs, -1) == 0) {
    Object_ASSIGN_NULL(me->hubCred);
    HEAP_FREE_PTR(me);
  }

  return Object_OK;
}

static
int32_t LinkCredComposite_getPropertyByIndex(LinkCredComposite *me, uint32_t index,
                                             void *name, size_t nameLen, size_t *nameLenOut,
                                             void *value, size_t valueLen, size_t *valueLenOut)
{
  return ICredentials_ERROR_NOT_FOUND;
}

static
int32_t LinkCredComposite_getValueByName(LinkCredComposite *me, const void *name,
                                         size_t nameLen, void *value,
                                         size_t valueLen, size_t *valueLenOut)
{
  int32_t ret = ICredentials_ERROR_NOT_FOUND;

  ret = LinkCredLegacy_getValueByName(&(me->lCredRemote), name, nameLen,
                                      value, valueLen, valueLenOut);
  if (ret == Object_OK || ret == ICredentials_ERROR_VALUE_SIZE) {
    return ret;
  }

  LOG_TRACE("querying on LinkCredComposite = %p, name = %s\n", me, (const char *)name);
  ret = queryLocAndType(&(me->lCredRemote), name, nameLen, value, valueLen, valueLenOut);
  if (ret == Object_OK || ret == ICredentials_ERROR_VALUE_SIZE) {
    return ret;
  }

  if (7 == nameLen && 0 == strncmp((const char *)name, "lvmuuid", nameLen)) {
    if ((me->lCredRemote.lvmuuidLen != 0) && (valueLen >= me->lCredRemote.lvmuuidLen)) {
      *valueLenOut = memscpy(value, valueLen, me->lCredRemote.lvmuuid,
                             me->lCredRemote.lvmuuidLen);
      return Object_OK;
    } else {
      return Object_ERROR_SIZE_IN;
    }
  }

  if(!Object_isNull(me->hubCred)) {
    return ICredentials_getValueByName(me->hubCred, name, nameLen,
                                      value, valueLen, valueLenOut);
  } else {
    LOG_ERR("query %s failed, hubCred is NULL\n", (const char *)name);
    return ICredentials_ERROR_NOT_FOUND;
  }

}

static
ICredentials_DEFINE_INVOKE(LinkCredComposite_invoke, LinkCredComposite_, LinkCredComposite*);

/**************************************************************************
* Construct a compositeCred based on the linkCredRemote and complexCred of
* the remote Hub in the remoteAdapter.
* @param[in] remoteCred:linkCredRemote of remoteHub.
* @param[in] hubCred: complexCred of remoteHub.
* @param[out] objOut: the compositeCred to be returned.
**************************************************************************/
int32_t LinkCredComposite_new(Object remoteCred, Object hubCred, Object *objOut)
{
  LinkCredRemote *cred = NULL;
  LinkCredComposite *me = NULL;

  //Due to incomplete support for HUB from some customers, we have to abandon the
  //verification of hubCred, allowing them to use NULL credentials for temporary use.
  if (Object_isNull(remoteCred)) {
    LOG_ERR("input a null cred\n");
    return Object_ERROR;
  }

  if (LinkCredRemote_invoke != remoteCred.invoke) {
    LOG_ERR("remoteCred error type\n");
    return Object_ERROR;
  }

  cred = (LinkCredRemote *)remoteCred.context;

  me = HEAP_ZALLOC_TYPE(LinkCredComposite);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  memscpy(&(me->lCredRemote), sizeof(LinkCredRemote), cred, sizeof(LinkCredRemote));
  Object_INIT(me->hubCred, hubCred);
  *objOut = (Object) {LinkCredComposite_invoke, me};

  return Object_OK;
}

/* deprecated interfaces will be removed after META absorbs new ones.
 * integration mechanism asks it.
 */
int32_t LinkCredentials_new(CredInfo *info, int32_t lloc, int32_t ltype, Object *objOut)
{
  LinkCred *me = NULL;

  if (NULL == info) {
    LOG_ERR("credInfo error to be NULL\n");
    return Object_ERROR;
  }

  me = HEAP_ZALLOC_TYPE(LinkCred);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  memscpy(&(me->credInfo), sizeof(me->credInfo), info, sizeof(*info));
  me->lloc = lloc;
  me->ltype = ltype;
  *objOut = (Object) {LinkCred_invoke, me};

  return Object_OK;
}

int32_t LinkCredentialsRemote_new(uint8_t const *vmuuid, size_t vmuuidLen, int32_t lloc,
                                  int32_t ltype, Object *objOut)
{
  LinkCredRemote *me = NULL;

  if ((NULL == vmuuid) || (0 == vmuuidLen) || (vmuuidLen > VMUUID_MAX_SIZE)) {
    LOG_ERR("vmuuid illegal\n");
    return Object_ERROR_SIZE_IN;
  }
  if ((ELOC_LOCAL == lloc) || (UNIX == ltype)) {
    LOG_ERR("lloc or ltype wrong\n");
    return Object_ERROR;
  }

  me = HEAP_ZALLOC_TYPE(LinkCredRemote);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  me->lloc = lloc;
  me->ltype = ltype;
  me->lvmuuidLen = memscpy(me->lvmuuid, VMUUID_MAX_SIZE, vmuuid, vmuuidLen);
  *objOut = (Object) {LinkCredRemote_invoke, me};

  return Object_OK;
}

/*
 * For backward compatibility, we retained this function here.
 * Once the tech pack is applied, this function should be removed.
 */
int32_t LinkCredentials_newFromCred(Object remoteCred, Object hubCred, Object *objOut)
{
  LinkCredRemote *cred;
  LinkCredComposite *me = NULL;

  //Due to incomplete support for HUB from some customers, we have to abandon the
  //verification of hubCred, allowing them to use NULL credentials for temporary use.
  if (Object_isNull(remoteCred)) {
    LOG_ERR("input a null cred\n");
    return Object_ERROR;
  }

  if (LinkCredRemote_invoke != remoteCred.invoke) {
    LOG_ERR("remoteCred error type\n");
    return Object_ERROR;
  }

  cred = (LinkCredRemote *)remoteCred.context;

  me = HEAP_ZALLOC_TYPE(LinkCredComposite);
  if (!me) {
    return Object_ERROR_MEM;
  }

  me->refs = 1;
  memscpy(&(me->lCredRemote), sizeof(LinkCredRemote), cred, sizeof(LinkCredRemote));
  Object_INIT(me->hubCred, hubCred);
  *objOut = (Object) {LinkCredComposite_invoke, me};

  return Object_OK;
}
