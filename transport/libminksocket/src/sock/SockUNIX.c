/***********************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * All rights reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 **********************************************************************/
#include "cdefs.h"
#include "Heap.h"
#include "object.h"
#include "Marshalling.h"
#include "SockUNIX.h"
#include "Utils.h"
#include <sys/file.h>

extern
size_t strlcpy(char *dst, const char *src, size_t size);

int32_t SockUNIX_new(const char *address, struct sockaddr_un *sockAddr, int32_t *sock)
{
    if (address) {
        sockAddr->sun_family = AF_UNIX;
        strlcpy(sockAddr->sun_path, address, sizeof(sockAddr->sun_path)-1);
        *sock = socket(AF_UNIX, SOCK_STREAM, 0);
    }

    return 0;
}

static char lock_str[] = ".lock";
int SockUNIX_bind(int sockFd, struct sockaddr_un *sockAddr, socklen_t addrLen)
{
    // define the lock file for the sock address to ensure socket is opened exactly once
    int lock_fd = -1;
    char lock_path [108 + sizeof(lock_str)] = {0};
    int ret = snprintf(lock_path, sizeof(lock_path), "%s%s", sockAddr->sun_path, lock_str);
    if (ret >= sizeof(lock_path)) {
        // path was trunctated
        return -1;
    }

    // open lock file
    lock_fd = open(lock_path, O_RDONLY | O_CREAT, 0600);
    if (lock_fd == -1)
        return -1;

    // try to acquire lock
    ret = flock(lock_fd, LOCK_EX | LOCK_NB);
    if (ret != 0)
        return -1;   // the lock is held by another process

    // Remove path if left behind by another process, else encounter EADDRINUSE
    unlink(sockAddr->sun_path);

    return bind(sockFd, (struct sockaddr *)sockAddr, addrLen);

}

int SockUNIX_listen(int sockFd, int32_t backlog)
{
    return listen(sockFd, backlog);
}


int SockUNIX_accept(int sockFd, struct sockaddr_un *addr, socklen_t *addrLen)
{
    return accept(sockFd, (struct sockaddr *)addr, addrLen);
}

int SockUNIX_connect(int sockFd, struct sockaddr_un *addr, socklen_t addrLen)
{
    return connect(sockFd, (struct sockaddr *)addr, addrLen);
}

int SockUNIX_recvMsg(int sockFd, void *data, size_t size, int *fds, int numFdsMax)
{
    int fdCount = 0;
    struct msghdr msg;
    struct iovec io = {.iov_base = data, .iov_len = size};
    struct cmsghdr *cmsg;
    char buf[sizeof(struct cmsghdr) + (ObjectCounts_maxOI * sizeof(int))];

    C_ZERO(msg);
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    //setup control data buffer
    cmsg = CMSG_FIRSTHDR(&msg);
    if (!cmsg) {
        LOG_ERR("Failed on CMSG_FIRSTHDR()\n");
        return -1;
    }
    //init fd buffer to -1
    memset(CMSG_DATA(cmsg), -1, numFdsMax);

    while (io.iov_len > 0) {
        C_ZERO(buf); //reset the control buffer
        memset(CMSG_DATA(cmsg), -1, numFdsMax);
        ssize_t cb = recvmsg(sockFd, &msg, MSG_NOSIGNAL);
        if (cb <= 0) {
            LOG_ERR("Failed on recvmsg()\n");
            return -1;
        }

        //collect ancillary data
        int msg_fds = cmsg->cmsg_len > sizeof(struct cmsghdr)
        ? (cmsg->cmsg_len - sizeof(struct cmsghdr)) / sizeof(int)
        : 0;

        for(int i = 0; i < msg_fds; i++) {
            if (fdCount >= numFdsMax) {
                for (int x = 0; x < fdCount; x++) {
                    //close fds that were collected
                    vm_osal_mem_close(fds[x]);
                }

                for (int y = i; y < msg_fds; y++) {
                    //close fds that we weren't expecting
                    int tmp;
                    memcpy(&tmp, CMSG_DATA(cmsg)+(y * sizeof(int)), sizeof(int));
                    vm_osal_socket_close(tmp);
                }

                //this shouldn't have happened
                LOG_ERR("Unexpected error, fdCount exceeds the maximum\n");
                return -1;
            }
            fdCount++;
        }

        memcpy(fds, CMSG_DATA(cmsg), fdCount*sizeof(int));
        if (cb <= (ssize_t) io.iov_len) {
            io.iov_base = (void *) (cb + (char*)io.iov_base);
            io.iov_len -= cb;
        }
    }
    return fdCount;
}

int SockUNIX_recv(int sockFd, void **msg, size_t *sizeMsg, int fds[], size_t *numFds)
{
    int statHdr = 0;
    int statBdy = 0;
    FlatData *buf = *msg;
    size_t sizeAct = 0;

    statHdr = SockUNIX_recvMsg(sockFd, buf, sizeof(lxcom_hdr), fds, *numFds);
    if (statHdr < 0) {
        LOG_ERR("Failed on first SockUNIX_recvMsg, ret = %d\n", statHdr);
        return Object_ERROR_DEFUNCT;
    }

    sizeAct = buf->hdr.size;
    if (sizeAct > MSG_BUFFER_PREALLOC) {
        if (sizeAct > *sizeMsg) {
            LOG_ERR("Message size exceeds the max\n");
            return Object_ERROR_INVALID;
        }
        buf = (FlatData *)HEAP_ZALLOC(sizeAct);
        if (buf == NULL) {
            LOG_ERR("Failed to allocate flatdata\n");
            return Object_ERROR_MEM;
        }
        buf->hdr = ((FlatData *)(*msg))->hdr;
    }

    statBdy = SockUNIX_recvMsg(sockFd, (uint8_t *)buf + sizeof(lxcom_hdr),
                               sizeAct - sizeof(lxcom_hdr), fds + statHdr, *numFds - statHdr);
    if (statBdy < 0) {
        for (int i = 0; i < statHdr; i++) {
            vm_osal_mem_close(fds[i]);
        }
        LOG_ERR("Failed on second SockUNIX_recvMsg, ret = %d\n", statBdy);
        return Object_ERROR_DEFUNCT;
    }

    *sizeMsg = sizeAct;
    *numFds = statHdr + statBdy;
    *msg = buf;

    return 0;
}

int32_t SockUNIX_sendMsg(int sockFd, const void *buf, size_t len)
{
    ssize_t cb = 0;

    if (NULL == buf || len <= 0) {
        LOG_ERR("Buf is NULL or len is invalid\n");
        return -1;
    }

    while (len > 0) {
        cb = send(sockFd, buf, len, MSG_NOSIGNAL);
        if (cb <= 0) {
            LOG_ERR("Send failed: sockFd = %d, errno = %d\n", sockFd, errno);
            return -1;
        }

        if (cb <= (ssize_t)len) {
            buf = (cb + (char *)buf);
            len -= (size_t)cb;
        }
    }

    return 0;
}

int32_t SockUNIX_sendVec(int sockFd, void *data, size_t sizeData,
                         int *fds, int numFdsMax)
{
    char buffer[sizeof(struct cmsghdr) + ObjectCounts_maxOO * sizeof(int)];
    ssize_t sizeSend = -1;
    struct msghdr msg;
    struct iovec iov = {0};

    if (NULL == data || sizeData <= 0) {
        LOG_ERR("Data is NULL or sizeData is invalid\n");
        return -1;
    }

    C_ZERO(buffer);
    C_ZERO(msg);
    msg.msg_control = NULL;
    iov.iov_base = data;
    iov.iov_len = sizeData;

    if (numFdsMax > 0) {
        struct cmsghdr *cmsg;
        msg.msg_control = buffer;
        /*The msg_controllen should be determined based on the actual length of the fd*/
        msg.msg_controllen = sizeof(struct cmsghdr) + numFdsMax * sizeof(int);
        cmsg = CMSG_FIRSTHDR(&msg);
        if (!cmsg) {
            LOG_ERR("cmsg is invalid\n");
            return -1;
        }
        cmsg->cmsg_len = msg.msg_controllen;
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        memcpy(CMSG_DATA(cmsg), fds, numFdsMax * sizeof(int));
    }

    while (iov.iov_len > 0) {
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        sizeSend = sendmsg(sockFd, &msg, MSG_NOSIGNAL);
        if (sizeSend < 0) {
            LOG_ERR("Send msg failed: sockFd = %d, errno = %d\n", sockFd, errno);
            return -1;
        }

        if (msg.msg_control) {
            msg.msg_control = NULL;
            msg.msg_controllen = 0;
        }

        iov.iov_len -= sizeSend;
        iov.iov_base = (void*) ((char*)iov.iov_base + sizeSend);
    }

    return 0;
}


