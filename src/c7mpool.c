/*
 * c7mpool.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <stdlib.h>
#include <c7memory.h>
#include <c7mpool.h>
#include <c7thread.h>
#include <c7status.h>


typedef struct _hdr_t {
    union {
	struct _hdr_t *next_free;
	c7_mpool_t refpool;
    } link;
    int refcnt;
} _hdr_t;

typedef struct _mpool_ops_t {
    c7_bool_t (*init)(c7_mpool_t mp);
    _hdr_t *(*get)(c7_mpool_t mp);
    void (*ref)(c7_mpool_t mp, _hdr_t *);
    void (*put)(c7_mpool_t mp, _hdr_t *);
    void (*fini)(c7_mpool_t mp);
} _mpool_ops_t;

typedef struct _chunk_t {
    struct _chunk_t *next;
    char storage[];
} _chunk_t;

struct c7_mpool_t_ {
    _hdr_t *frees;
    _chunk_t *chunks;
    const _mpool_ops_t *ops;
    c7_bool_t (*on_get)(void *);
    void (*on_put)(void *);
    void (*on_free)(void *);
    int elmsize;			/* include _hdr_t */
    int alccnt;	
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};


static c7_bool_t mpooladd(c7_mpool_t mp)
{
    size_t alz = sizeof(_chunk_t) + mp->elmsize * mp->alccnt;
    _chunk_t * const chunk = c7_calloc(alz, 1);
    if (chunk == NULL)
	return C7_FALSE;

    /* chunk list */
    chunk->next = mp->chunks;
    mp->chunks = chunk;

    /* free list */
    _hdr_t *hdr = NULL;
    char *p = (void *)(chunk + 1);
    for (int i = 0; i < mp->alccnt; i++) {
	hdr = (_hdr_t *)p;
	p += mp->elmsize;
	hdr->link.next_free = (_hdr_t *)p;
    }
    hdr->link.next_free = mp->frees;
    mp->frees = (void *)(chunk + 1);

    return C7_TRUE;
}


/*----------------------------------------------------------------------------
                                  operators
----------------------------------------------------------------------------*/

// standard operations

static c7_bool_t std_init(c7_mpool_t mp)
{
    return C7_TRUE;
}

static _hdr_t *std_get(c7_mpool_t mp)
{
    _hdr_t *hdr = NULL;
    if (mp->frees != NULL || mpooladd(mp)) {
	hdr = mp->frees;
	mp->frees = hdr->link.next_free;
    }
    return hdr;
}

static void std_ref(c7_mpool_t mp, _hdr_t *hdr)
{
    hdr->refcnt++;
}

static void std_put(c7_mpool_t mp, _hdr_t *hdr)
{
    hdr->refcnt--;
    if (hdr->refcnt == 0) {
	if (mp->on_put)
	    mp->on_put(hdr+1);
	hdr->link.next_free = mp->frees;
	mp->frees = hdr;
    }
}

static void std_fini(c7_mpool_t mp)
{
    ;
}

static const _mpool_ops_t std_ops = {
    std_init, std_get, std_ref, std_put, std_fini
};

// multithread nowait - mpooladd is called if no mpool

static c7_bool_t mtnowait_init(c7_mpool_t mp)
{
    return c7_thread_mutex_init(&mp->mutex, NULL);
}

static _hdr_t *mtnowait_get(c7_mpool_t mp)
{
    c7_thread_lock(&mp->mutex);
    _hdr_t *hdr = std_get(mp);
    c7_thread_unlock(&mp->mutex);
    return hdr;
}

static void mtnowait_ref(c7_mpool_t mp, _hdr_t *hdr)
{
    c7_thread_lock(&mp->mutex);
    std_ref(mp, hdr);
    c7_thread_unlock(&mp->mutex);
}

static void mtnowait_put(c7_mpool_t mp, _hdr_t *hdr)
{
    c7_thread_lock(&mp->mutex);
    std_put(mp, hdr);
    c7_thread_unlock(&mp->mutex);
}

static void mtnowait_fini(c7_mpool_t mp)
{
    (void)pthread_mutex_destroy(&mp->mutex);
}

static const _mpool_ops_t mtnowait_ops = {
    mtnowait_init, mtnowait_get, mtnowait_ref, mtnowait_put, mtnowait_fini
 };

// multithread wait - mpooladd is called at once on initializing and wait while no mpool

static c7_bool_t mtwait_init(c7_mpool_t mp)
{
    if (c7_thread_mutex_init(&mp->mutex, NULL)) {
	if (c7_thread_cond_init(&mp->cond, NULL))
	    return C7_TRUE;
	(void)pthread_mutex_destroy(&mp->mutex);
    }
    return C7_FALSE;
}

static _hdr_t *mtwait_get(c7_mpool_t mp)
{
    _hdr_t *hdr = NULL;
    C7_THREAD_GUARD_ENTER(&mp->mutex);
    while (mp->frees == NULL) {
	c7_thread_wait(&mp->cond, &mp->mutex, NULL);
    }
    hdr = mp->frees;
    mp->frees = hdr->link.next_free;
    C7_THREAD_GUARD_EXIT(&mp->mutex);
    return hdr;
}

static void mtwait_put(c7_mpool_t mp, _hdr_t *hdr)
{
    c7_thread_lock(&mp->mutex);
    std_put(mp, hdr);
    c7_thread_notify_all(&mp->cond);
    c7_thread_unlock(&mp->mutex);
}

static void mtwait_fini(c7_mpool_t mp)
{
    (void)pthread_mutex_destroy(&mp->mutex);
    (void)pthread_cond_destroy(&mp->cond);
}

static const _mpool_ops_t mtwait_ops = {
    mtwait_init, mtwait_get, mtnowait_ref, mtwait_put, mtwait_fini
};


/*----------------------------------------------------------------------------
                               public interface
----------------------------------------------------------------------------*/

static c7_mpool_t mpool_init(const _mpool_ops_t *ops,
			     size_t size, int alccnt,
			     c7_bool_t (*on_get)(void *),
			     void (*on_put)(void *),
			     void (*on_free)(void *))
{
    c7_mpool_t mp = c7_malloc(sizeof(*mp));
    if (mp == NULL)
	return NULL;

    mp->elmsize = c7_align(sizeof(_hdr_t) + size, 8);
    mp->alccnt = (alccnt < 1) ? 1 : alccnt;
    mp->ops = ops;
    mp->on_get = on_get;
    mp->on_put = on_put;
    mp->on_free = on_free;
    mp->frees = NULL;
    mp->chunks = NULL;

    if (ops->init(mp)) {
	if (mpooladd(mp))
	    return mp;
	ops->fini(mp);
    }
    free(mp);
    return NULL;
}

c7_mpool_t c7_mpool_init(size_t size, int alccnt,
			 c7_bool_t (*on_get)(void *),
			 void (*on_put)(void *),
			 void (*on_free)(void *))
{
    return mpool_init(&std_ops, size, alccnt, on_get, on_put, on_free);
}

c7_mpool_t c7_mpool_init_mt(size_t size, int alccnt,
			    c7_bool_t (*on_get)(void *),
			    void (*on_put)(void *),
			    void (*on_free)(void *),
			    unsigned flags)
{
    const _mpool_ops_t *ops;
    if ((flags & C7_MPOOL_MT_WAITABLE) != 0)
	ops = &mtwait_ops;
    else
	ops = &mtnowait_ops;
    return mpool_init(ops, size, alccnt, on_get, on_put, on_free);
}

void *c7_mpool_get(c7_mpool_t mp)
{
    _hdr_t *hdr = mp->ops->get(mp);
    if (hdr != NULL) {
	hdr->link.refpool = mp;
	hdr->refcnt = 1;
	c7_status_clear();
	if (mp->on_get == NULL || mp->on_get(hdr + 1))
	    return (void *)(hdr + 1);
	if (!c7_status_has_error())
	    c7_status_add(errno, "c7_mpool_get: on_get error\n");
	mp->ops->put(mp, hdr);
    }
    return NULL;
}

void c7_mpool_ref(void *addr)
{
    if (addr == NULL)
	return;

    _hdr_t *hdr = (_hdr_t *)addr - 1;
    c7_mpool_t mp = hdr->link.refpool;
    mp->ops->ref(mp, hdr);
}

void c7_mpool_put(void *addr)
{
    if (addr == NULL)
	return;

    _hdr_t *hdr = (_hdr_t *)addr - 1;
    c7_mpool_t mp = hdr->link.refpool;
    mp->ops->put(mp, hdr);
}

void c7_mpool_free(c7_mpool_t mp)
{
    if (mp == NULL)
	return;

    _chunk_t *chunk = mp->chunks;
    while (chunk) {
	_hdr_t *cur = (void *)(chunk + 1);
	if (mp->on_free) {
	    char *p = (char *)cur;
	    for (int i = 0; i < mp->alccnt; i++) {
		mp->on_free((_hdr_t *)p + 1);
		p += mp->elmsize;
	    }
	}
	_chunk_t *c = chunk;
	chunk = chunk->next;
	free(c);
    }

    mp->ops->fini(mp);
    free(mp);
}
