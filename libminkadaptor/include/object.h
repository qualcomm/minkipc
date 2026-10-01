// Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef __OBJECT_H
#define __OBJECT_H

#include <stddef.h>
#include <stdint.h>

/* 'OBJECT OPS' */

/**
 * @defgroup ObjectOperations The Object Operation being requested
 * @brief Defines helper macros for ObjectOp
 * @{
 */

 /**
 * @def ObjectOp
 * @brief The operation being requested from the Object is encoded in a
 *        32-bit quantity `ObjectOp`.
 */
typedef uint32_t ObjectOp;

/**
 * @def ObjectOp_METHOD_MASK
 * @brief Method mask for figuring out the type of Object operation and the
 *        Method ID.
 */
#define ObjectOp_METHOD_MASK     ((ObjectOp) 0x0000FFFFu)

/**
 * @def ObjectOp_methodID
 * @brief Method ID bits are not modified by transport layers. These describe
 *        the method (member function) being requested by the client.
 */
#define ObjectOp_methodID(op)    ((op) & ObjectOp_METHOD_MASK)

/**
 * @def ObjectOp_METHOD_USERMAX
 * @brief User-defined method maximum ID range
 */
#define ObjectOp_METHOD_USERMAX  ((ObjectOp) 0x00003FFF)

/**
 * @def ObjectOp_METHOD_USERMAX
 * @brief Modifier bits are reserved for transport-layer semantics.
 */
#define ObjectOp_MODIFIER_MASK   ((ObjectOp) 0xFFFF0000u)

/**
 * @def ObjectOp_REMOTE_BUFS
 * @brief Set by transports when buffers may reside in untrusted memory and
 * buffer alignment is untrusted. Implementations of invoke may skip some
 * validation and/or copying when this is clear.
 */
#define ObjectOp_REMOTE_BUFS     ((ObjectOp) 0x00010000u)

/**
 * @def ObjectOp_LOCAL
 * @brief Local ops are not forwarded by transports.
 */
#define ObjectOp_LOCAL           ((ObjectOp) 0x00008000U)

/**
 * @def ObjectOp_isLocal
 * @brief Check if the operation being invoked is local.
 */
#define ObjectOp_isLocal(op)     (((op) & ObjectOp_LOCAL) != 0)

/**
 * @def Object_OP_release
 * @brief An operation to release the object.
 */
#define Object_OP_release       (ObjectOp_METHOD_MASK - 0)

/**
 * @def Object_OP_retain
 * @brief An operation to retain the object.
 */
#define Object_OP_retain        (ObjectOp_METHOD_MASK - 1)

/**
* @def Object_OP_version
* @brief An operation to query the API version implemented by the object.
*
* Emitted by mink-idl-compiler >= 0.3.0, which generates an
* I<Interface>_api_version() helper for every interface and a matching
* `case Object_OP_version:` arm in the I<Interface>_DEFINE_INVOKE skeleton
* macro.
*
* Value matches the reference object.h in mink-idl-compiler (tests/c/object.h).
* It derives from ObjectOp_LOCAL rather than ObjectOp_METHOD_MASK.
*/
#define Object_OP_version	(ObjectOp_LOCAL - 1)

/** @} */ // end of ObjectOperations

/* 'OBJECT COUNTS' */

/**
 * @defgroup ObjectCounting Helpers for ObjectCounts
 * @brief Defines helper macros for ObjectCounts
 * @{
 */

/**
 * @def ObjectCounts
 * @brief The number and kinds of arguments passed to invoke are encoded in a
 * 32-bit quantity `ObjectCounts`. Currently only 16-bits are used; the
 * remainder are reserved for future enhancements.
 */
typedef uint32_t ObjectCounts;

/**
 * @def ObjectCounts_pack
 * @brief Packs information about the number of BI, BO, OI and OO in
 *        in an ObjectCounts mask.
 *        BI = BuffersIn, BO = BuffersOut, OI = ObjectsIn, OO = ObjectsOut
 */
#define ObjectCounts_pack(nBuffersIn, nBuffersOut, nObjectsIn, nObjectsOut) \
   ((ObjectCounts) ((nBuffersIn) |         \
                    ((nBuffersOut) << 4) | \
                    ((nObjectsIn) << 8)  | \
                    ((nObjectsOut) << 12)))

/**
 * @def ObjectCounts_maxBI
 * @brief Maximum number of BI that can be passed as arguments.
 */
#define ObjectCounts_maxBI   0xF

/**
 * @def ObjectCounts_maxBO
 * @brief Maximum number of BO that can be passed as arguments.
 */
#define ObjectCounts_maxBO   0xF

/**
 * @def ObjectCounts_maxOI
 * @brief Maximum number of OI that can be passed as arguments.
 */
#define ObjectCounts_maxOI   0xF

/**
 * @def ObjectCounts_maxOO
 * @brief Maximum number of OO that can be passed as arguments.
 */
#define ObjectCounts_maxOO   0xF

/**
 * @def ObjectCounts_numBI
 * @brief Number of BI in the `ObjectCounts` mask.
 */
#define ObjectCounts_numBI(k)   ( (size_t) ( ((k) >> 0) & ObjectCounts_maxBI) )

/**
 * @def ObjectCounts_numBO
 * @brief Number of BO in the `ObjectCounts` mask.
 */
#define ObjectCounts_numBO(k)   ( (size_t) ( ((k) >> 4) & ObjectCounts_maxBO) )

/**
 * @def ObjectCounts_numOI
 * @brief Number of OI in the `ObjectCounts` mask.
 */
#define ObjectCounts_numOI(k)   ( (size_t) ( ((k) >> 8) & ObjectCounts_maxOI) )

/**
 * @def ObjectCounts_numOO
 * @brief Number of OO in the `ObjectCounts` mask.
 */
#define ObjectCounts_numOO(k)   ( (size_t) ( ((k) >> 12) & ObjectCounts_maxOO) )

/**
 * @def ObjectCounts_numBuffers
 * @brief Number of BI and BO in the `ObjectCounts` mask.
 */
#define ObjectCounts_numBuffers(k)  (ObjectCounts_numBI(k) + ObjectCounts_numBO(k))

/**
 * @def ObjectCounts_numObjects
 * @brief Number of OI and OO in the `ObjectCounts` mask.
 */
#define ObjectCounts_numObjects(k)  (ObjectCounts_numOI(k) + ObjectCounts_numOO(k))

/**
 * @def ObjectCounts_indexBI
 * @brief Index of BI in the `ObjectArg` list.
 */
#define ObjectCounts_indexBI(k)   0

/**
 * @def ObjectCounts_indexBO
 * @brief Index of BO in the `ObjectArg` list.
 */
#define ObjectCounts_indexBO(k)   (ObjectCounts_indexBI(k) + ObjectCounts_numBI(k))

/**
 * @def ObjectCounts_indexOI
 * @brief Index of OI in the `ObjectArg` list.
 */
#define ObjectCounts_indexOI(k)   (ObjectCounts_indexBO(k) + ObjectCounts_numBO(k))

/**
 * @def ObjectCounts_indexOO
 * @brief Index of OO in the `ObjectArg` list.
 */
#define ObjectCounts_indexOO(k)   (ObjectCounts_indexOI(k) + ObjectCounts_numOI(k))

/**
 * @def ObjectCounts_total
 * @brief Total number of elements in the `ObjectArg` list.
 */
#define ObjectCounts_total(k)     (ObjectCounts_indexOO(k) + ObjectCounts_numOO(k))

/**
 * @def ObjectCounts_indexBuffers
 * @brief Index of Buffers in the `ObjectArg` list.
 */
#define ObjectCounts_indexBuffers(k)   ObjectCounts_indexBI(k)

/**
 * @def ObjectCounts_indexObjects
 * @brief Index of Objects in the `ObjectArg` list.
 */
#define ObjectCounts_indexObjects(k)   ObjectCounts_indexOI(k)

/** @} */ // end of ObjectCounting

/* 'OBJECT' */

/**
 * @defgroup ObjectDef Definitions of Objects
 * @brief Defines MINK IPC Objects
 * @{
 */

typedef struct Object Object;
typedef struct ObjectBuf ObjectBuf;
typedef struct ObjectBufIn ObjectBufIn;
typedef struct Object64    Object64;
typedef struct ObjectBuf64 ObjectBuf64;
typedef union ObjectArg ObjectArg;
typedef union  ObjectArg64 ObjectArg64;
typedef void *ObjectCxt;

/**
 * @def ObjectInvoke
 * @brief A function pointer which invokes an operation on an Object.
 */
typedef int32_t (*ObjectInvoke)(ObjectCxt h,
				ObjectOp op,
				ObjectArg *args,
				ObjectCounts counts);

/**
 * @def Object
 * @brief An object with a context and invoke function pointer.
 */
struct Object {
	ObjectInvoke invoke;
	ObjectCxt context;    /**< context data to pass to the invoke function. */
};

/**
 * @def Object64
 * @brief An object with a context and invoke function pointer.
 *        Suitable for transitioning between 32 bit and 64 bit
 *        environments.
 */
struct Object64 {
	ObjectInvoke invoke_l;
	ObjectInvoke invoke_h;
	ObjectCxt context_l;    /**< context data to pass to the invoke function. */
	ObjectCxt context_h;    /**< context data to pass to the invoke function. */
};

/**
 * @def ObjectBuf
 * @brief An Object Buffer whose contents is not shared with QTEE; the contents
 * are copied back and forth into a shared buffer.
 */
struct ObjectBuf {
	void *ptr;
	size_t size;
};

/**
 * @def ObjectBuf64
 * @brief An Object Buffer whose contents is not shared with QTEE; the contents
 * are copied back and forth into a shared buffer. Suitable for transitioning
 * between 32 bit and 64 bit environments.
 */
struct ObjectBuf64 {
	void  *ptr_l;
	void  *ptr_h;
	size_t size_l;
	size_t size_h;
};

/**
 * @def ObjectBufIn
 * @brief An Object Buffer marked as Input.
 */
struct ObjectBufIn {
	const void *ptr;
	size_t size;
};

/**
 * @def ObjectArg
 * @brief An argument passed to an object during invocation.
 */
union ObjectArg {
	ObjectBuf b;
	ObjectBufIn bi;
	Object o;
};

/**
 * @def ObjectArg64
 * @brief An argument passed to an object during invocation.
 *        Suitable for transitioning between 32 bit and 64 bit
 *        environments.
 */
union ObjectArg64 {
	ObjectBuf64 b;
	Object64    o;
};

/**
 * @def Object_invoke
 * @brief A wrapper which calls the `ObjectInvoke` function pointer
 *        and passes the context of the Object as the first argument.
 */
static inline int32_t Object_invoke(Object o, ObjectOp op, ObjectArg *args, ObjectCounts k)
{
	return o.invoke(o.context, op, args, k);
}

/**
 * @def Object_NULL
 * @brief A special MINK Object representing NULL.
 */
#define Object_NULL                ( (Object) { NULL, NULL } )

/** @} */ // end of ObjectDef

/* ''OBJECT INVOKE RETURN CODES'' */

/**
 * @defgroup ObjectErr Object Error Codes
 * @brief A value of zero (Object_OK) indicates that the invocation has succeeded.
 *
 * Negative values are reserved for use by transports -- implementations of
 * invoke() that forward requests to another protection domain -- and
 * indicate problems communicating between domains.  Positive error codes
 * are sub-divided into generic and user-defined errors.  Generic errors
 * can be used by IDL-generated code and object implementations.
 * User-case-specific error codes should be allocated from the user defined
 * range.
 * @{
 */

#define Object_isOK(err)        ((err) == 0)
#define Object_isERROR(err)     ((err) != 0)

/**
 * @def Object_OK
 * @brief Indicates that the invocation has succeeded.
 */
#define Object_OK                  0

/**
 * @def Object_ERROR
 * @brief Non-specific, and can be used whenever the error
 * condition does not need to be distinguished from others in the interface.
 */
#define Object_ERROR               1

/**
 * @def Object_ERROR_INVALID
 * @brief Indicates that the request was not understood by the
 * object. This can result when `op` is unrecognized, or when the number
 * and/or sizes of arguments does not match what is expected for `op`.
 */
#define Object_ERROR_INVALID       2

/**
 * @def Object_ERROR_SIZE_IN
 * @brief Indicates that an input buffer was too large to be marshaled.
 */
#define Object_ERROR_SIZE_IN       3

/**
 * @def Object_ERROR_SIZE_OUT
 * @brief Indicates that an output buffer was too large to be marshaled.
 */
#define Object_ERROR_SIZE_OUT      4

/**
 * @def Object_ERROR_MEM
 * @brief Indicates a failure of memory allocation.
 */
#define Object_ERROR_MEM           5

/**
 * @def Object_ERROR_USERBASE
 * @brief Beginning of the user-defined range. Error
 * codes in this range can be defined on an object-by-object or
 * interface-by-interface basis. IDL-specified error codes are allocated
 * from this range.
 */
#define Object_ERROR_USERBASE     10

/**
 * @def Object_ERROR_DEFUNCT
 * @brief Indicates that the object reference will no longer work.  This is
 * returned when the process hosting the object has terminated, or when the
 * communication link to the object is unrecoverably lost.
 */
#define Object_ERROR_DEFUNCT     -90

/**
 * @def Object_ERROR_ABORT
 * @brief Indicates that the caller should return to the point
 * at which it was invoked from a remote domain.  Unlike other error codes,
 * this pertains to the state of the calling thread, not the state of the
 * target object or transport.
 *
 * For example, when a process is terminated while a kernel thread is
 * executing on its behalf, that kernel thread should return back to its
 * entry point so that it can be reaped safely. (Synchronously killing the
 * thread could leave kernel data structures in a corrupt state.)  If it
 * attempts to invoke an object that would result in it blocking on another
 * thread (or if it is already blocking in an invocation) the invocation
 * will immediately return this error code.
 */
#define Object_ERROR_ABORT       -91

/**
 * @def Object_ERROR_BADOBJ
 * @brief Indicates that the caller provided a mal-formed object structure
 * as a target object or an input parameter. In general, mal-formed Object
 * structures cannot be reliably distinguished from valid ones, since Object
 * contains a function pointer and a context pointer. In the case of some
 * transports, however, such as the Mink user-to-kernel transport, an
 * invalid context value will be detected and will result in this error code.
 */
#define Object_ERROR_BADOBJ      -92

/**
 * @def Object_ERROR_NOSLOTS
 * @brief Indicates that an object could not be returned
 * because the calling domain has reached the maximum number of remote
 * object references on this transport.
 */
#define Object_ERROR_NOSLOTS     -93

/**
 * @def Object_ERROR_MAXARGS
 * @brief Indicates that the `args` array length exceeds the
 * maximum supported by the object or by some transport between the caller
 * and the object.
 */
#define Object_ERROR_MAXARGS     -94

/**
 * @def Object_ERROR_MAXDATA
 * @brief Indicates the the complete payload (input buffers and/or output
 * buffers) exceed
 */
#define Object_ERROR_MAXDATA     -95

/**
 * @def Object_ERROR_UNAVAIL
 * @brief Indicates that the destination process cannot fulfill the request
 * at the current time, but that retrying the operation in the future might
 * result in success.
 *
 * This may be a result of resource constraints, such as exhaustion of the
 * object table in the destination process.
 */
#define Object_ERROR_UNAVAIL     -96

/**
 * @def Object_ERROR_KMEM
 * @brief Indicates a failure of memory allocation outside of the
 * caller's domain and outside of the destination domain.
 *
 * This may occur when marshaling objects.  It may also occur when passing
 * strings or other buffers that must be copied for security reasons in the
 * destination domain.
 */
#define Object_ERROR_KMEM        -97

/**
 * @def Object_ERROR_REMOTE
 * @brief Indicates that a *local* operation has been requested
 * when the target object is remote.  Transports do not forward local
 * operations.
 */
#define Object_ERROR_REMOTE      -98

/**
 * @def Object_ERROR_BUSY
 * @brief Indicates that the target domain or process is busy and
 * cannot currently accept an invocation.
 */
#define Object_ERROR_BUSY        -99

/**
 * @def Object_ERROR_AUTH
 * @brief Cannot authenticate message.
 */
#define Object_ERROR_AUTH        -100

/**
 * @def Object_ERROR_REPLAY
 * @brief Message has been replayed.
 */
#define Object_ERROR_REPLAY      -101

/**
 * @def Object_ERROR_MAXREPLAY
 * @brief Replay counter cannot be incremented
 */
#define Object_ERROR_MAXREPLAY   -102

/**
 * @def Object_ERROR_TIMEOUT
 * @brief Call Back Object invocation timed out.
 */
#define Object_ERROR_TIMEOUT     -103

/** @} */ // end of ObjectErr

/* ''OBJECT UTILITIES'' */

/**
 * @defgroup ObjectUtils Object Utilities
 * @brief Utility functions and macros for Objects
 *
 * @{
 */

/**
 * @def OBJECT_NOT_RETAINED
 * @brief This annotation is used when a returned value or output parameter
 * conveys an object reference but the function will not increment the
 * reference count. In these cases, the caller is not responsible for releasing
 * it.  The object reference count is guaranteed to be non-zero because the
 * caller knows of some other reference to the object (usually an input
 * parameter to the same function).
 */
#define OBJECT_NOT_RETAINED

/**
 * @def OBJECT_CONSUMED
 * @brief This annotation is used when an input parameter is an object
 * reference that will be released by the function.
 */
#define OBJECT_CONSUMED

/**
 * @def Object_release
 * @brief Release the MINK Object.
 */
static inline int32_t Object_release(OBJECT_CONSUMED Object o) {
  return Object_invoke((o), Object_OP_release, 0, 0);
}

/**
 * @def Object_retain
 * @brief Retain the MINK Object.
 */
static inline int32_t Object_retain(Object o) {
  return Object_invoke((o), Object_OP_retain, 0, 0);
}

/**
 * @def Object_isNull
 * @brief Check if Object is NULL.
 */
#define Object_isNull(o)           ( (o).invoke == NULL )

/**
 * @def Object_RELEASE_IF
 * @brief Conditionally release an object if not NULL.
 */
#define Object_RELEASE_IF(o)                                            \
   do { Object o_ = (o); if (!Object_isNull(o_)) (void) Object_release(o_); } while (0)

/**
 * @def Object_replace
 * @brief Replace a reference to a MINK Object with another.
 */
static inline void Object_replace(Object *loc, Object objNew)
{
	if (!Object_isNull(*loc)) {
		Object_release(*loc);
	}
	if (!Object_isNull(objNew)) {
		Object_retain(objNew);
	}
	*loc = objNew;
}

/**
 * @def Object_ASSIGN
 * @brief Assign a given reference to the MINK object.
 */
#define Object_ASSIGN(loc, obj)  Object_replace(&(loc), (obj))

/**
 * @def Object_ASSIGN
 * @brief Assign a NULL reference to the MINK object.
 */
#define Object_ASSIGN_NULL(loc)  Object_replace(&(loc), Object_NULL)

/**
 * @def Object_INIT
 * @brief Initialize a MINK object.
 */
#define Object_INIT(loc, obj)                   \
  do {                                          \
    Object o_ = (obj);                          \
    (loc) = o_;                                 \
    if (!Object_isNull(o_)) {                   \
      Object_retain(o_);                        \
    }                                           \
  } while(0)

/** @} */ // end of ObjectUtils

#endif /* __OBJECT_H */
