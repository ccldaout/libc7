/*
 * c7lldef.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"


#include <c7lldef.h>


void c7_ll_init(c7_ll_base_t *bp)
{
    bp->ll.next = (void *)bp;
    bp->ll.prev = (void *)bp;
}

void *c7_ll_head(const c7_ll_base_t *bp)
{
    return bp->ll.next;
}

void *c7_ll_tail(const c7_ll_base_t *bp)
{
    return bp->ll.prev;
}

c7_bool_t c7_ll_is_empty(const c7_ll_base_t *bp)
{
    return (bp->ll.next == (void *)bp);
}

c7_bool_t c7_ll_is_terminal(c7_ll_base_t *bp, c7_ll_link_t *lnk)
{
    return ((void *)bp == (void *)lnk);
}

c7_ll_link_t *c7_ll_next(c7_ll_link_t *lnk)
{
    return lnk->next;
}

c7_ll_link_t *c7_ll_prev(c7_ll_link_t *lnk)
{
    return lnk->prev;
}

__c7_ll_iter_t __c7_ll_loop_init(c7_ll_base_t *bp, ptrdiff_t lnkoff, void **vp)
{
    __c7_ll_iter_t iter = { .bp = bp, .lnkoff = lnkoff };
    if (c7_ll_is_empty(bp)) {
	*vp = NULL;
	iter.next = &bp->ll;
	return iter;
    }
    iter.next = c7_ll_head(bp);
    *vp = (char *)iter.next - lnkoff;
    iter.next = c7_ll_next(iter.next);
    return iter;
}

void __c7_ll_loop_next(__c7_ll_iter_t *iter, void **vp)
{
    if (c7_ll_is_terminal(iter->bp, iter->next)) {
	*vp = NULL;
    } else {
	*vp = (char *)iter->next - iter->lnkoff;
	iter->next = c7_ll_next(iter->next);
    }
}

__c7_ll_iter_t __c7_ll_loop_init_r(c7_ll_base_t *bp, ptrdiff_t lnkoff, void **vp)
{
    __c7_ll_iter_t iter = { .bp = bp, .lnkoff = lnkoff };
    if (c7_ll_is_empty(bp)) {
	*vp = NULL;
	iter.next = &bp->ll;
	return iter;
    }
    iter.next = c7_ll_tail(bp);
    *vp = (char *)iter.next - lnkoff;
    iter.next = c7_ll_prev(iter.next);
    return iter;
}

void __c7_ll_loop_next_r(__c7_ll_iter_t *iter, void **vp)
{
    if (c7_ll_is_terminal(iter->bp, iter->next)) {
	*vp = NULL;
    } else {
	*vp = (char *)iter->next - iter->lnkoff;
	iter->next = c7_ll_prev(iter->next);
    }
}

void c7_ll_putnext(c7_ll_link_t *cur, c7_ll_link_t *new)
{
    new->prev = cur;
    new->next = cur->next;
    cur->next->prev = new;
    cur->next = new;
}

void c7_ll_putprev(c7_ll_link_t *cur, c7_ll_link_t *new)
{
    new->next = cur;
    new->prev = cur->prev;
    cur->prev->next = new;
    cur->prev = new;
}

void c7_ll_unlink(c7_ll_link_t *cur)
{
    cur->prev->next = cur->next;
    cur->next->prev = cur->prev;
}
