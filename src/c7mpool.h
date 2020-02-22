/*
 * c7mpool.h
 *
 * https://ccldaout.github.io/libc7/group__c7mpool.html
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_MPOOL_H_LOADED__
#define __C7_MPOOL_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7types.h>


#define C7_MPOOL_MT_WAITABLE	(1U << 0)	// allocation once and wait for free


typedef struct c7_mpool_t_ *c7_mpool_t;

c7_mpool_t c7_mpool_init(size_t size, int alccnt,
			 c7_bool_t (*on_get)(void *),
			 void (*on_put)(void *),
			 void (*on_free)(void *));
c7_mpool_t c7_mpool_init_mt(size_t size, int alccnt,
			    c7_bool_t (*on_get)(void *),
			    void (*on_put)(void *),
			    void (*on_free)(void *),
			    unsigned flags);
void *c7_mpool_get(c7_mpool_t mp);
void c7_mpool_ref(void *addr);
#define c7_mpool_unref	c7_mpool_put
void c7_mpool_put(void *addr);
void c7_mpool_close(c7_mpool_t mp);
void c7_mpool_free(c7_mpool_t mp);


#if defined(__cplusplus)
}
#endif
#endif /* c7mpool.h */
