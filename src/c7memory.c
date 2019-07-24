/*
 * c7memory.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <stdlib.h>
#include <string.h>
#include <c7memory.h>
#include <c7status.h>
#include <c7app.h>
#include "_private.h"


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

void *__c7_memdup(const char *file, int line, const void *addr, size_t size)
{
    void *p = __c7_malloc(file, line, size);
    if (p != NULL)
	(void)memcpy(p, addr, size);
    return p;
}

void *__c7_malloc(const char *file, int line, size_t z)
{
    void *p = malloc(z);
    if (p != NULL)
	return p;
    __c7_hook_memory_error(file, line, errno, z);
    return NULL;
}

void *__c7_calloc(const char *file, int line, size_t n, size_t z)
{
    void *p = calloc(n, z);
    if (p != NULL)
	return p;
    __c7_hook_memory_error(file, line, errno, n * z);
    return NULL;
}

void *__c7_realloc(const char *file, int line, void *p, size_t z)
{
    p = realloc(p, z);
    if (p != NULL)
	return p;
    __c7_hook_memory_error(file, line, errno, z);
    return NULL;
}

void *(c7_memdup)(const void *addr, size_t size)
{
    return c7_memdup(addr, size);
}

void *(c7_malloc)(size_t z)
{
    return c7_malloc(z);
}

void *(c7_calloc)(size_t n, size_t z)
{
    return c7_calloc(n, z);
}

void *(c7_realloc)(void *p, size_t n)
{
    return c7_realloc(p, n);
}


/*----------------------------------------------------------------------------
                                 Memory group
----------------------------------------------------------------------------*/

#if 0
# define __dbg(...)		c7echo(__VA_ARGS__)
#else
# define __dbg(...)
#endif


#define _HDR_OFFSET	c7_align(sizeof(_mhead_t), C7_CONFIG_MEMALIGN)

typedef struct _mhead_t {
    c7_ll_link_t ll;
} _mhead_t;

const struct c7_mgroup_t_ __c7_mg_thread_dummy;
c7_thread_local struct c7_mgroup_t_ __c7_mg_thread;

static void mg_init_thread(void)
{
    c7_mg_init(&__c7_mg_thread);
}

static void mg_deinit_thread(void)
{
    c7_mg_freeall(&__c7_mg_thread);
}

c7_mgroup_t __c7_mg_new(const char *file, int line)
{
    c7_mgroup_t mgrp = __c7_malloc(file, line, sizeof(*mgrp));
    if (mgrp == NULL)
	return NULL;
    c7_mg_init(mgrp);
    return mgrp;
}

void c7_mg_init(c7_mgroup_t mg)
{
    c7_ll_init(&mg->base);
}

void *__c7_mg_memdup(const char *file, int line, c7_mgroup_t mg, const void *addr, size_t size)
{
    void *p = __c7_mg_malloc(file, line, mg, size);
    if (p != NULL)
	(void)memcpy(p, addr, size);
    return p;
}

void *__c7_mg_malloc(const char *file, int line, c7_mgroup_t mg, size_t size)
{
    _mhead_t *m;
    if (mg == NULL)
	return __c7_malloc(file, line, size);
    if (mg == c7_tg_thread_mg)
	mg = &__c7_mg_thread;
    if ((m = __c7_malloc(file, line, _HDR_OFFSET + size)) == NULL)
	return NULL;
    C7_LL_PUTHEAD(&mg->base, &m->ll);
    __dbg("mg_malloc: mg:%p %p\n", mg, m);
    return (char *)m + _HDR_OFFSET;
}

void *__c7_mg_calloc(const char *file, int line, c7_mgroup_t mg, size_t n, size_t z)
{
    _mhead_t *m;
    if (mg == NULL)
	return __c7_calloc(file, line, n, z);
    if (mg == c7_tg_thread_mg)
	mg = &__c7_mg_thread;
    size_t size = n * z;
    if ((m = __c7_calloc(file, line, 1, _HDR_OFFSET + size)) == NULL)
	return NULL;
    C7_LL_PUTHEAD(&mg->base, &m->ll);
    __dbg("mg_calloc: mg:%p %p\n", mg, m);
    return (char *)m + _HDR_OFFSET;
}

void *__c7_mg_realloc(const char *file, int line, c7_mgroup_t mg, void *u_addr, size_t size)
{
    _mhead_t *m0, *m1;
    if (mg == NULL)
	return __c7_realloc(file, line, u_addr, size);
    if (mg == c7_tg_thread_mg)
	mg = &__c7_mg_thread;
    if (u_addr == NULL)
	return __c7_mg_malloc(file, line, mg, size);
    m0 = (_mhead_t *)((char *)u_addr - _HDR_OFFSET);
    if ((m1 = __c7_realloc(file, line, m0, _HDR_OFFSET + size)) == NULL)
	return NULL;
    if (m0 != m1) {
	m1->ll.prev->next = &m1->ll;
	m1->ll.next->prev = &m1->ll;
    }
    __dbg("mg_realloc: mg:%p %p->%p\n", mg, m0, m1);
    return (char *)m1 + _HDR_OFFSET;
}

void c7_mg_trade(c7_mgroup_t trgmg, c7_mgroup_t srcmg, void *src_addr)
{
    _mhead_t *m;
    if (srcmg == c7_tg_thread_mg)
	srcmg = &__c7_mg_thread;
    if (srcmg == NULL) {
	// c7_realloc cannot be called because memory size pointed by
	// u_addr is unknown.
	c7abort_err(EINVAL, ": c7_mg_trade can't accept unmanaged memory.\n");
    }
    if (src_addr == NULL)
	return;
    m = (_mhead_t *)((char *)src_addr - _HDR_OFFSET);
    C7_LL_UNLINK(&m->ll);
    C7_LL_PUTHEAD(&trgmg->base, &m->ll);
}

void *c7_mg_unlink(c7_mgroup_t mg, void *u_addr, size_t content_size)
{
    _mhead_t *m;
    if (mg == NULL)
	return u_addr;
    if (mg == c7_tg_thread_mg)
	mg = &__c7_mg_thread;
    if (u_addr == NULL)
	return NULL;
    m = (_mhead_t *)((char *)u_addr - _HDR_OFFSET);
    C7_LL_UNLINK(&m->ll);
    if (content_size != 0)
	(void)memmove(m, u_addr, content_size);
    return m;
}

void c7_mg_free(c7_mgroup_t mg, void *u_addr)
{
    if (mg == NULL) {
	free(u_addr);
	return;
    }
    if (mg == c7_tg_thread_mg)
	mg = &__c7_mg_thread;
    if (u_addr != NULL) {
	_mhead_t *m = (_mhead_t *)((char *)u_addr - _HDR_OFFSET);
	C7_LL_UNLINK(&m->ll);
	__dbg("mg_free: mg:%p %p\n", mg, m);
	free(m);
    }
}

void c7_mg_freeall(c7_mgroup_t mg)
{
    void *mll;
    if (mg != NULL) {
	if (mg == c7_tg_thread_mg)
	    mg = &__c7_mg_thread;
	C7_LL_FOREACH(&mg->base, mll) {
	    _mhead_t *m = (void *)((char *)mll - offsetof(_mhead_t, ll));
	    __dbg("mg_freeall: mg:%p %p\n", mg, m);
	    free(m);
	}
	c7_mg_init(mg);
    }
}

void c7_mg_destroy(c7_mgroup_t mg)
{
    if (mg != NULL) {
	if (mg == c7_tg_thread_mg)
	    mg = &__c7_mg_thread;
	c7_mg_freeall(mg);
	free(mg);
    }
}

c7_mgroup_t (c7_mg_new)(void)
{
    return c7_mg_new();
}

void *(c7_mg_memdup)(c7_mgroup_t mg, const void *addr, size_t size)
{
    return c7_mg_memdup(mg, addr, size);
}

void *(c7_mg_malloc)(c7_mgroup_t mg, size_t size)
{
    return c7_mg_malloc(mg, size);
}

void *(c7_mg_calloc)(c7_mgroup_t mg, size_t n, size_t z)
{
    return c7_mg_calloc(mg, n, z);
}

void *(c7_mg_realloc)(c7_mgroup_t mg, void *u_addr, size_t size)
{
    return c7_mg_realloc(mg, u_addr, size);
}


/*----------------------------------------------------------------------------
                             thread memory group
----------------------------------------------------------------------------*/

void *(c7_tg_memdup)(const void *addr, size_t size)
{
    return c7_tg_memdup(addr, size);
}

void *(c7_tg_malloc)(size_t size)
{
    return c7_tg_malloc(size);
}

void *(c7_tg_calloc)(size_t n, size_t z)
{
    return c7_tg_calloc(n, z);
}

void *(c7_tg_realloc)(void *u_addr, size_t size)
{
    return c7_tg_realloc(u_addr, size);
}

void *(c7_tg_unlink)(void *u_addr, size_t content_size)		/* [CAUTION] address is changed. */
{
    return c7_tg_unlink(u_addr, content_size);
}

void (c7_tg_free)(void *u_addr)
{
    c7_tg_free(u_addr);
}

void (c7_tg_freeall)(void)
{
    c7_tg_freeall();
}


/*----------------------------------------------------------------------------
                            Stackable memory group
----------------------------------------------------------------------------*/

typedef struct _sg_stack_t_ {
    struct _sg_stack_t_ *next;
    c7_mgroup_t mg;
} _sg_stack_t;

static c7_thread_local struct c7_mgroup_t_ __c7_sg_top_mg;
       c7_thread_local        c7_mgroup_t  __c7_sg_thread;
static c7_thread_local _sg_stack_t *gc_stack;

static void sg_init_thread(void)
{
    __c7_sg_thread = &__c7_sg_top_mg;
    c7_mg_init(__c7_sg_thread);
}

static void sg_deinit_thread(void)
{
    while (c7_sg_pop());
}

c7_bool_t __c7_sg_push(const char *file, int line)
{
    __dbg("sg_push\n");
    _sg_stack_t *stk = __c7_malloc(file, line, sizeof(*stk));
    if (stk != NULL) {
	c7_mgroup_t nmg = __c7_mg_new(file, line);
	if (nmg != NULL) {
	    stk->next = gc_stack;
	    stk->mg = __c7_sg_thread;
	    gc_stack = stk;
	    __c7_sg_thread = nmg;
	    return C7_TRUE;
	}
	free(stk);
    }
    return C7_FALSE;
}

c7_bool_t c7_sg_pop(void)
{
    __dbg("sg_pop ...\n");
    c7_sg_freeall();
    if (gc_stack == NULL)
	return C7_FALSE;
    c7_mg_destroy(__c7_sg_thread);
    _sg_stack_t *stk = gc_stack;
    __c7_sg_thread = stk->mg;
    gc_stack = stk->next;
    free(stk);
    __dbg("sg_pop ... end\n");
    return C7_TRUE;
}

c7_mgroup_t (c7_sg_current_mg)(void)
{
    return c7_sg_current_mg();
}

c7_bool_t (c7_sg_push)(void)
{
    return c7_sg_push();
}

void *(c7_sg_memdup)(const void *addr, size_t size)
{
    return c7_sg_memdup(addr, size);
}

void *(c7_sg_malloc)(size_t size)
{
    return c7_sg_malloc(size);
}

void *(c7_sg_calloc)(size_t n, size_t z)
{
    return c7_sg_calloc(n, z);
}

void *(c7_sg_realloc)(void *u_addr, size_t size)
{
    return c7_sg_realloc(u_addr, size);
}

void *(c7_sg_unlink)(void *u_addr, size_t content_size)
{
    return c7_sg_unlink(u_addr, content_size);
}

void (c7_sg_free)(void *u_addr)
{
    c7_sg_free(u_addr);
}

void (c7_sg_freeall)(void)
{
    c7_sg_freeall();
}


/*----------------------------------------------------------------------------
                            simple variable buffer
----------------------------------------------------------------------------*/

struct c7_vbuf_t_ {
    c7_mgroup_t mg;
    void *addr;
    size_t size;
};

c7_vbuf_t __c7_vbuf_new(c7_mgroup_t mg)
{
    c7_vbuf_t vbuf;
    if ((vbuf = c7_mg_malloc(mg, sizeof(*vbuf))) == NULL)
	return NULL;
    vbuf->mg = mg;
    vbuf->addr = NULL;
    vbuf->size = 0;
    return vbuf;
}

void *c7_vbuf_get(c7_vbuf_t vbuf,  size_t n)
{
    if (vbuf->size >= n)
	return vbuf->addr;
    n = c7_align(n, 8192);
    void *p = c7_mg_realloc(vbuf->mg, vbuf->addr, n);
    if (p != NULL) {
	vbuf->addr = p;
	vbuf->size = n;
    }
    return p;
}

void c7_vbuf_free(c7_vbuf_t vbuf)
{
    c7_mg_free(vbuf->mg, vbuf->addr);
    c7_mg_free(vbuf->mg, vbuf);
}

c7_vbuf_t (c7_vbuf_new_std)(void)
{
    return c7_vbuf_new_std();
}

c7_vbuf_t (c7_vbuf_new_mg)(c7_mgroup_t mg)
{
    return c7_vbuf_new_mg(mg);
}

c7_vbuf_t (c7_vbuf_new_sg)(void)
{
    return c7_vbuf_new_sg();
}


/*----------------------------------------------------------------------------
                 library initializer / per-thread initializer
----------------------------------------------------------------------------*/

static void init_thread(void)
{
    mg_init_thread();
    sg_init_thread();
}

static void deinit_thread(void)
{
    mg_deinit_thread();
    sg_deinit_thread();
}

void __c7_memory_init(void)
{
    static c7_thread_iniend_t iniend = {
	.init   = init_thread,
	.deinit = deinit_thread,
    };
    (void)init_thread();
    c7_thread_register_iniend(&iniend);
}
