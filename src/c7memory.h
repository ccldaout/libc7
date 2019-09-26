/*
 * c7memory.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_MEMORY_H_LOADED__
#define __C7_MEMORY_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <stdlib.h>	// free
#include <c7types.h>
#include <c7lldef.h>


/*----------------------------------------------------------------------------
                           standard memory function
----------------------------------------------------------------------------*/

#define c7_memdup(p, z)		__c7_memdup(__FILE__, __LINE__, (p), (z))
#define c7_malloc(z)		__c7_malloc(__FILE__, __LINE__, (z))
#define c7_calloc(n, z)		__c7_calloc(__FILE__, __LINE__, (n), (z))
#define c7_realloc(p, z)	__c7_realloc(__FILE__, __LINE__, (p), (z))

void *__c7_memdup(const char *file, int line, const void *addr, size_t size);
void *__c7_malloc(const char *file, int line, size_t z);
void *__c7_calloc(const char *file, int line, size_t n, size_t z);
void *__c7_realloc(const char *file, int line, void *p, size_t n);

void *(c7_memdup)(const void *addr, size_t size);
void *(c7_malloc)(size_t z);
void *(c7_calloc)(size_t n, size_t z);
void *(c7_realloc)(void *p, size_t n);


/*----------------------------------------------------------------------------
                                 memory group
----------------------------------------------------------------------------*/

#define C7_MG_INIT(mg)	{ C7_LL_INIT(mg) }

typedef struct c7_mgroup_t_ {
    c7_ll_base_t base;		// list of *alloced memory
    c7_ll_base_t base_ex;	// _exobj_t
} *c7_mgroup_t;

#define c7_mg_new()		__c7_mg_new(__FILE__, __LINE__)
#define c7_mg_manage(g, o, f)	__c7_mg_manage(__FILE__, __LINE__, (g), (o), (void (*)(void*))(f))
#define c7_mg_memdup(g, p, z)	__c7_mg_memdup(__FILE__, __LINE__, (g), (p), (z))
#define c7_mg_malloc(g, z)	__c7_mg_malloc(__FILE__, __LINE__, (g), (z))
#define c7_mg_calloc(g, n, z)	__c7_mg_calloc(__FILE__, __LINE__, (g), (n), (z))
#define c7_mg_realloc(g, p, z)	__c7_mg_realloc(__FILE__, __LINE__, (g), (p), (z))

c7_mgroup_t __c7_mg_new(const char *file, int line);
void c7_mg_init(c7_mgroup_t mg);
void *__c7_mg_manage(const char *file, int line, c7_mgroup_t mg, void *obj, void (*freeobj)(void *));
c7_bool_t c7_mg_unmanage(c7_mgroup_t mg, void *obj);
void *__c7_mg_memdup(const char *file, int line, c7_mgroup_t mg, const void *addr, size_t size);
void *__c7_mg_malloc(const char *file, int line, c7_mgroup_t mg, size_t size);
void *__c7_mg_calloc(const char *file, int line, c7_mgroup_t mg, size_t n, size_t z);
void *__c7_mg_realloc(const char *file, int line, c7_mgroup_t mg, void *u_addr, size_t size);
void c7_mg_trade(c7_mgroup_t trgmg, c7_mgroup_t srcmg, void *src_addr);
void *c7_mg_unlink(c7_mgroup_t mg, void *u_addr, size_t content_size);	/* [CAUTION] address is changed. */
void c7_mg_free(c7_mgroup_t mg, void *u_addr);
void c7_mg_freeall(c7_mgroup_t mg);
void c7_mg_destroy(c7_mgroup_t mg);

c7_mgroup_t (c7_mg_new)(void);
void *(c7_mg_manage)(c7_mgroup_t mg, void *obj, void (*freeobj)(void *));
void *(c7_mg_memdup)(c7_mgroup_t mg, const void *addr, size_t size);
void *(c7_mg_malloc)(c7_mgroup_t mg, size_t size);
void *(c7_mg_calloc)(c7_mgroup_t mg, size_t n, size_t z);
void *(c7_mg_realloc)(c7_mgroup_t mg, void *u_addr, size_t size);


/*----------------------------------------------------------------------------
                             thread memory group
----------------------------------------------------------------------------*/

extern const struct c7_mgroup_t_ __c7_mg_thread_dummy;

#define c7_tg_thread_mg		((c7_mgroup_t)&__c7_mg_thread_dummy)
#define c7_tg_manage(o, f)	__c7_mg_manage(__FILE__, __LINE__, c7_tg_thread_mg, (o), (void (*)(void*))(f))
#define c7_tg_unmanage(o)	c7_mg_unmanage(c7_tg_thread_mg, (o))
#define c7_tg_memdup(p, z)	__c7_mg_memdup(__FILE__, __LINE__, c7_tg_thread_mg, (p), (z))
#define c7_tg_malloc(z)		__c7_mg_malloc(__FILE__, __LINE__, c7_tg_thread_mg, (z))
#define c7_tg_calloc(n, z)	__c7_mg_calloc(__FILE__, __LINE__, c7_tg_thread_mg, (n), (z))
#define c7_tg_realloc(p, z)	__c7_mg_realloc(__FILE__, __LINE__, c7_tg_thread_mg, (p), (z))
#define c7_tg_unlink(p, z)	c7_mg_unlink(c7_tg_thread_mg, p, z)
#define c7_tg_free(p)		c7_mg_free(c7_tg_thread_mg, p)
#define c7_tg_freeall()		c7_mg_freeall(c7_tg_thread_mg)
void *(c7_tg_manage)(void *obj, void (*freeobj)(void *));
c7_bool_t (c7_tg_unmanage)(void *obj);
void *(c7_tg_memdup)(const void *addr, size_t size);
void *(c7_tg_malloc)(size_t size);
void *(c7_tg_calloc)(size_t n, size_t z);
void *(c7_tg_realloc)(void *u_addr, size_t size);
void *(c7_tg_unlink)(void *u_addr, size_t content_size);
void (c7_tg_free)(void *u_addr);
void (c7_tg_freeall)(void);


/*----------------------------------------------------------------------------
                            stackable memory group
----------------------------------------------------------------------------*/

typedef struct __c7_sg_stack_t {
    struct __c7_sg_stack_t *pushed;
    struct c7_mgroup_t_ mg;
} __c7_sg_stack_t;

extern c7_thread_local c7_mgroup_t __c7_sg_thread;

#define c7_sg_current_mg()	(__c7_sg_thread)
#define c7_sg_push()		__c7_sg_push2(&(__c7_sg_stack_t){})
#define c7_sg_pop()		__c7_sg_pop2()
#define c7_sg_manage(o, f)	__c7_mg_manage(__FILE__, __LINE__, c7_sg_current_mg(), (o), (void (*)(void*))(f))
#define c7_sg_unmanage(o)	c7_mg_unmanage(c7_sg_current_mg(), (o))
#define c7_sg_memdup(p, z)	__c7_mg_memdup(__FILE__, __LINE__, c7_sg_current_mg(), (p), (z))
#define c7_sg_malloc(z)		__c7_mg_malloc(__FILE__, __LINE__, c7_sg_current_mg(), (z))
#define c7_sg_calloc(n, z)	__c7_mg_calloc(__FILE__, __LINE__, c7_sg_current_mg(), (n), (z))
#define c7_sg_realloc(p, z)	__c7_mg_realloc(__FILE__, __LINE__, c7_sg_current_mg(), (p), (z))
#define c7_sg_unlink(p, z)	c7_mg_unlink(c7_sg_current_mg(), p, z)
#define c7_sg_free(p)		c7_mg_free(c7_sg_current_mg(), p)
#define c7_sg_freeall()		c7_mg_freeall(c7_sg_current_mg())

void __c7_sg_push2(__c7_sg_stack_t *newstack);
void __c7_sg_pop2(void);

c7_mgroup_t (c7_sg_current_mg)(void);
void *(c7_sg_manage)(void *obj, void (*freeobj)(void *));
c7_bool_t (c7_sg_unmanage)(void *obj);
void *(c7_sg_memdup)(const void *addr, size_t size);
void *(c7_sg_malloc)(size_t size);
void *(c7_sg_calloc)(size_t n, size_t z);
void *(c7_sg_realloc)(void *u_addr, size_t size);
void *(c7_sg_unlink)(void *u_addr, size_t content_size);	/* [CAUTION] address is changed. */
void (c7_sg_free)(void *u_addr);
void (c7_sg_freeall)(void);


/*----------------------------------------------------------------------------
                             variable size buffer
----------------------------------------------------------------------------*/

typedef struct c7_vbuf_t_ *c7_vbuf_t;

#define c7_vbuf_new_std()	__c7_vbuf_new(NULL)
#define c7_vbuf_new_mg(mg)	__c7_vbuf_new((mg))
#define c7_vbuf_new_sg()	__c7_vbuf_new(c7_sg_current_mg())

c7_vbuf_t __c7_vbuf_new(c7_mgroup_t mg);
void *c7_vbuf_get(c7_vbuf_t,  size_t n);
void c7_vbuf_free(c7_vbuf_t);

c7_vbuf_t (c7_vbuf_new_std)(void);
c7_vbuf_t (c7_vbuf_new_mg)(c7_mgroup_t mg);
c7_vbuf_t (c7_vbuf_new_sg)(void);


#if defined(__cplusplus)
}
#endif
#endif /* c7memory.h */
