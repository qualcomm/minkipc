// Copyright (c) 2004-2015 Qualcomm Technologies, Inc.
// All Rights Reserved.
// Confidential and Proprietary - Qualcomm Technologies, Inc.

/*=======================================================================
QList

    QList keeps a doubly-linked list of QNode structures.  Each queue is
    represented by a `head` node, not a head pointer, simplifying and
    streamlining many operations.  Because it is doubly-linked it permits
    constant-time insertion or removal of items or of entire queues.
    Because it is circular it permits constant-time operations at both the
    tail and the head of the queue.  Circularity also streamlines some
    operations by eliminating conditional branches.

    General rules:

    * QLists are always in a defined state; they should be constructed
      before use, using QList_construct() functions.

    * QNodes are null when unqueued and non-null when enqueued.

=====================================================================*/
#ifndef __QLIST_H_
#define __QLIST_H_

#include "kdefs.h"

typedef struct QNode QNode;
struct QNode {
   QNode *pNext;
   QNode *pPrev;
};

#define QLIST_DEFINE_INIT(f) QList f = { { &f.n, &f.n } }

typedef struct QList QList;
struct QList {
   QNode n;
};


static inline void QNode_insPrev(QNode *me, QNode *pn)
{
   QNode *pPrev = me->pPrev;

   pn->pNext    = me;
   pn->pPrev    = pPrev;
   pPrev->pNext = pn;
   me->pPrev    = pn;
}


static inline void QNode_insNext(QNode *me, QNode *pn)
{
   QNode *pNext = me->pNext;

   pn->pPrev    = me;
   pn->pNext    = pNext;
   pNext->pPrev = pn;
   me->pNext    = pn;
}



static inline void QNode_dequeue(QNode *me)
{
   QNode *pNext = me->pNext;
   QNode *pPrev = me->pPrev;

   pPrev->pNext = pNext;
   pNext->pPrev = pPrev;
   me->pNext = 0;
   me->pPrev = 0;
}


static inline void QNode_construct(QNode *me)
{
   me->pNext = me->pPrev = 0;
}


static inline int QNode_isQueued(QNode *me)
{
   return (0 != me->pNext);
}


static inline void QNode_dequeueIf(QNode *me)
{
   if (QNode_isQueued(me)) {
      QNode_dequeue(me);
   }
}


//--------------------------------------------------------------------
//--  QList functions  ----------------------------------------------
//--------------------------------------------------------------------


static inline void QList_construct(QList *me)
{
   me->n.pNext = me->n.pPrev = &me->n;
}


static inline int QList_isEmpty(QList *me)
{
   return me->n.pNext == &me->n;
}


static inline void QList_appendNode(QList *me, QNode *pn)
{
   kassert(!QNode_isQueued(pn));
   QNode_insPrev(&me->n, pn);
}


static inline void QList_prependNode(QList *me, QNode *pn)
{
   kassert(!QNode_isQueued(pn));
   QNode_insNext(&me->n, pn);
}


static inline void QList_constructFrom(QList *me, QList *psrc)
{
   QNode *s = &psrc->n;
   QNode *d = &me->n;

   s->pNext->pPrev = d;
   d->pPrev        = s->pPrev;
   d->pNext        = s->pNext;
   s->pPrev->pNext = d;

   QList_construct(psrc);
}

static inline void QList_appendList(QList *me, QList *psrc)
{
   QNode *s = &psrc->n;
   QNode *d = &me->n;
   QNode *dp = d->pPrev;
   QNode *sn = s->pNext;
   QNode *sp;

   sn->pPrev   = dp;
   dp->pNext   = sn;
   d->pPrev    = (sp = s->pPrev);
   sp->pNext   = d;

   QList_construct(psrc);
}


#define QLIST_FOR_ALL(pList, pNode) \
   for ((pNode) = (pList)->n.pNext; \
        (pNode) != &(pList)->n; \
        (pNode) = (pNode)->pNext)

#define QLIST_FOR_REST(pList, pNode) \
   for (; \
        (pNode) != &(pList)->n; \
        (pNode) = (pNode)->pNext)

#define QLIST_REV_FOR_ALL(pList, pNode) \
   for ((pNode) = (pList)->n.pPrev; \
        (pNode) != &(pList)->n; \
        (pNode) = (pNode)->pPrev)

#define QLIST_REV_FOR_REST(pList, pNode) \
   for (; \
        (pNode) != &(pList)->n; \
        (pNode) = (pNode)->pPrev)

/* Allows dequeing QNodes during iteration */
#define QLIST_NEXTSAFE_FOR_ALL(pList, pNode, pNodeNext) \
    for ((pNode) = (pList)->n.pNext, (pNodeNext) = (pNode)->pNext; \
         (pNode) != &(pList)->n; \
         (pNode) = (pNodeNext), (pNodeNext) = (pNode)->pNext)


static inline QNode *QList_getFirst(QList *me)
{
   QNode *pn = me->n.pNext;

   return (pn == &me->n ? 0 : pn);
}


static inline QNode *QList_getLast(QList *me)
{
   QNode *pn = me->n.pPrev;

   return (pn == &me->n ? 0 : pn);
}


static inline QNode *QList_pop(QList *me)
{
   QNode *pn = me->n.pNext;
   QNode *pnn = pn->pNext;

   if (pn == &me->n) {
      return 0;
   }

   me->n.pNext = pnn;
   pnn->pPrev   = &me->n;
   QNode_construct(pn);
   return pn;
}


static inline QNode *QList_popLast(QList *me)
{
   QNode *pp = me->n.pPrev;
   QNode *ppp = pp->pPrev;

   if (pp == &me->n) {
      return 0;
   }

   me->n.pPrev = ppp;
   ppp->pNext  = &me->n;
   QNode_construct(pp);

   return pp;
}

#endif // __QLIST_H_
