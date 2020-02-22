/*
 * c7lldef.h
 *
 * https://ccldaout.github.io/libc7/group__c7lldef.html
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7LLDEF_H_LOADED__
#define __C7LLDEF_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7types.h>


/*
 *	struct NODE_TYPE {
 *	    c7_ll_link_t __list;
 *	    ... your data ...
 *	};
 *
 *	static c7_ll_baes_t BASE_OF_LINKEDLIST =
 *          C7_LL_INIT(&BASE_OF_LINKEDLIST);
 */


typedef struct c7_ll_link_t_ {
    struct c7_ll_link_t_ *prev, *next;
} c7_ll_link_t;

typedef struct c7_ll_base_t_ {
    c7_ll_link_t ll;			// prev:tail, next:head
} c7_ll_base_t;

typedef struct __c7_ll_iter_t_ {
    c7_ll_base_t *bp;
    c7_ll_link_t *next;
    ptrdiff_t lnkoff;
} __c7_ll_iter_t;


#define C7_LL_FOREACH(_bp, _v)		C7_LL_FOREACH_X((_bp), 0, (_v))

#define C7_LL_FOREACH_X(_bp, _off, _v)					\
    for (__c7_ll_iter_t __it =						\
	     __c7_ll_loop_init((_bp), (_off), (void **)&(_v));		\
	 (_v) != NULL;							\
	 __c7_ll_loop_next(&__it, (void **)&(_v)))

#define C7_LL_FOREACH_R(_bp, _v)	C7_LL_FOREACH_RX((_bp), 0, (_v))

#define C7_LL_FOREACH_RX(_bp, _off, _v)					\
    for (__c7_ll_iter_t __it =						\
	     __c7_ll_loop_init_r((_bp), (_off), (void **)&(_v));	\
	 (_v) != NULL;							\
	 __c7_ll_loop_next_r(&__it, (void **)&(_v)))

#define C7_LL_INIT(_bp)		{{(void *)(_bp), (void *)(_bp)}}
#define C7_LL_IS_EMPTY(_bp)	(C7_LL_HEAD(_bp) == (void *)(_bp))
#define C7_LL_IS_TERMINAL(_bp, _cur)	((void *)(_bp) == (void *)(_cur))
#define C7_LL_PUTHEAD(_bp, _n)	C7_LL_PUTNEXT(_bp, _n)
#define C7_LL_PUTTAIL(_bp, _n)	C7_LL_PUTPREV(_bp, _n)	

#if !defined(C7_CONFIG_SCA)

# define C7_LL_HEAD(_bp)	((void *)(_bp)->ll.next)
# define C7_LL_TAIL(_bp)	((void *)(_bp)->ll.prev)
# define C7_LL_NEXT(_cur)	((void *)((c7_ll_link_t*)(_cur))->next)
# define C7_LL_PREV(_cur)	((void *)((c7_ll_link_t*)(_cur))->prev)

# define C7_LL_PUTNEXT(_cur, _next)				\
    do {							\
	c7_ll_link_t *__cur = (c7_ll_link_t *)(_cur);		\
	c7_ll_link_t *__next = (c7_ll_link_t *)(_next);		\
	__next->prev = __cur;					\
	__next->next = __cur->next;				\
	__cur->next->prev = __next;				\
	__cur->next = __next;					\
    } while (0)

# define C7_LL_PUTPREV(_cur, _prev)					\
    do {								\
	c7_ll_link_t *__cur = (c7_ll_link_t *)(_cur);			\
	c7_ll_link_t *__prev = (c7_ll_link_t *)(_prev);			\
	__prev->next = __cur;						\
	__prev->prev = __cur->prev;					\
	__cur->prev->next = __prev;					\
	__cur->prev = __prev;						\
    } while (0)

# define C7_LL_UNLINK(_n)						\
    do {								\
	c7_ll_link_t *__n = (c7_ll_link_t *)(_n);			\
	__n->prev->next = __n->next;					\
	__n->next->prev = __n->prev;					\
    } while (0)

#else

# define C7_LL_HEAD(_bp)	c7_ll_head(_bp)
# define C7_LL_TAIL(_bp)	c7_ll_tail(_bp)
# define C7_LL_NEXT(_cur)	((void *)c7_ll_next((c7_ll_link_t*)(_cur)))
# define C7_LL_PREV(_cur)	((void *)c7_ll_prev((c7_ll_link_t*)(_cur)))
# define C7_LL_PUTNEXT(_cur, _next)	c7_ll_putnext((c7_ll_link_t *)(_cur), (c7_ll_link_t *)(_next))
# define C7_LL_PUTPREV(_cur, _prev)	c7_ll_putprev((c7_ll_link_t *)(_cur), (c7_ll_link_t *)(_prev))
# define C7_LL_UNLINK(_cur)	c7_ll_unlink((c7_ll_link_t *)(_cur))

#endif

void c7_ll_init(c7_ll_base_t *bp);
void *c7_ll_head(const c7_ll_base_t *bp);
void *c7_ll_tail(const c7_ll_base_t *bp);
c7_bool_t c7_ll_is_empty(const c7_ll_base_t *bp);
c7_bool_t c7_ll_is_terminal(c7_ll_base_t *bp, c7_ll_link_t *lnk);
c7_ll_link_t *c7_ll_next(c7_ll_link_t *lnk);
c7_ll_link_t *c7_ll_prev(c7_ll_link_t *lnk);
__c7_ll_iter_t __c7_ll_loop_init(c7_ll_base_t *bp, ptrdiff_t lnkoff, void **vp);
void __c7_ll_loop_next(__c7_ll_iter_t *iter, void **vp);
__c7_ll_iter_t __c7_ll_loop_init_r(c7_ll_base_t *bp, ptrdiff_t lnkoff, void **vp);
void __c7_ll_loop_next_r(__c7_ll_iter_t *iter, void **vp);
void c7_ll_putnext(c7_ll_link_t *cur, c7_ll_link_t *new);
void c7_ll_putprev(c7_ll_link_t *cur, c7_ll_link_t *new);
void c7_ll_unlink(c7_ll_link_t *cur);


#if defined(__cplusplus)
}
#endif
#endif /* c7lldef.h */
