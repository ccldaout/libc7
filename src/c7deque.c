/*
 * c7deque.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <c7deque.h>
#include <c7status.h>
#include <c7app.h>


#if !defined(_MIN_COUNT)
# define _MIN_COUNT	32
#endif


struct c7_deque_t_ {
    size_t item_size;	/* item size */
    char *head;
    char *tail;
    char *b_top;
    char *b_lim;
    void (*on_remove)(const c7_deque_t dq, void *item);
};


c7_deque_t c7_deque_create(size_t item_size,
			   void (*on_remove)(const c7_deque_t dq, void *item))
{
    c7_deque_t dq = c7_malloc(sizeof(*dq));
    if (dq != NULL) {
	size_t z = item_size * _MIN_COUNT;
	if ((dq->b_top = c7_malloc(z)) != NULL) {
	    dq->item_size = item_size;
	    dq->head = dq->b_top;
	    dq->tail = dq->b_top;
	    dq->b_lim = dq->b_top + z;
	    dq->on_remove = on_remove;
	} else {
	    free(dq);
	    dq = NULL;
	}
    }
    return dq;
}

ssize_t __c7_deque_foreach_next(c7_deque_t dq, ssize_t i, void **vp, ssize_t *idxp)
{
    if (0 <= i && i < c7_deque_count(dq)) {
	*vp = (void *)(dq->head + (dq->item_size * i));
	if (idxp != NULL)
	    *idxp = i;
    } else {
	*vp = NULL;
	if (idxp != NULL)
	    *idxp = -1;
    }
    return i + 1;
}

ssize_t c7_deque_index(const c7_deque_t dq, void *item)
{
    const char * const ip = item;
    if (dq->head <= ip && ip <= dq->tail)
	return (ip - dq->head) / dq->item_size;
    c7_status_add(EINVAL, "c7_deque_index: itemp:%p is out of buffer.", item);
    return -1;
}

ssize_t c7_deque_count(const c7_deque_t dq)
{
    return c7_deque_index(dq, dq->tail);
}

void *c7_deque_nth(const c7_deque_t dq, ssize_t idx)
{
    if (0 <= idx && idx < c7_deque_count(dq))
	return (void *)(dq->head + (dq->item_size * idx));
    c7_status_add(EINVAL, "c7_deque_nth: idx:%ld is out of buffer.", idx);
    return NULL;
}

void *c7_deque_buffer(const c7_deque_t dq)
{
    return (void *)dq->head;
}

void *c7_deque_pop_head(c7_deque_t dq)
{
    if (dq->head < dq->tail) {
	void *h = dq->head;
	dq->head += dq->item_size;
	return h;
    }
    return NULL;
}

void *c7_deque_pop_tail(c7_deque_t dq)
{
    if (dq->head < dq->tail) {
	dq->tail -= dq->item_size;
	return dq->tail;
    }
    return NULL;
}

static c7_bool_t bufrealloc(c7_deque_t dq, size_t new_count,
			    void **itempp, c7_bool_t *moved_o)
{
    size_t nz = new_count * dq->item_size;
    char *np = dq->b_top;

    np = c7_realloc(np, nz);
    if (np == NULL) {
	return C7_FALSE;
    }
    if ((*itempp == NULL) ||
	((char *)*itempp < dq->head) || (dq->tail <= (char *)*itempp)) {
	itempp = NULL;
    }
    if (moved_o != NULL) {
	*moved_o = C7_FALSE;
    }

    if (np != dq->b_top) {
	if (moved_o != NULL) {
	    *moved_o = C7_TRUE;
	}
	dq->head = np + (dq->head - dq->b_top);
	dq->tail = np + (dq->tail - dq->b_top);
	if (itempp != NULL) {
	    *itempp = np + ((char *)*itempp - dq->b_top);
	}
	dq->b_top = np;
    }
    dq->b_lim = np + nz;
    return C7_TRUE;
}

static void centering(c7_deque_t dq, void **itempp)
{
    ssize_t shift, free_size, new_headoff;

    if ((*itempp == NULL) ||
	((char *)*itempp < dq->head) || (dq->tail <= (char *)*itempp)) {
	itempp = NULL;
    }
    
    free_size = (dq->b_lim - dq->b_top) - (dq->tail - dq->head);
    new_headoff =  ((free_size / dq->item_size) / 2) * dq->item_size;
    shift = (dq->b_top + new_headoff) - dq->head;

    (void)memmove(dq->head + shift, dq->head, dq->tail - dq->head);
    dq->head += shift;
    dq->tail += shift;
    if (itempp != NULL) {
	*itempp = (char *)*itempp + shift;
    }
}

void *c7_deque_push_head(c7_deque_t dq, void *item_opt)
{
    if (dq->b_top == dq->head) {
	/* left side has no enough space */

	if (dq->tail >= (dq->b_top + ((dq->b_lim - dq->b_top)/2))) {
	    size_t n = ((dq->b_lim - dq->b_top)/dq->item_size) * 2;
	    n = (n < _MIN_COUNT) ? _MIN_COUNT : n;
	    if (!bufrealloc(dq, n, &item_opt, NULL)) {
		return NULL;
	    }
	}

	centering(dq, &item_opt);
    }

    /* add item to left */
    dq->head -= dq->item_size;
    if (item_opt != NULL) {
	(void)memmove(dq->head, item_opt, dq->item_size);
    }
    return dq->head;
}

void *c7_deque_push_tail(c7_deque_t dq, void *item_opt)
{
    if (dq->b_lim == dq->tail) {
	/* right side has no enough space */
	if (dq->head <= (dq->b_top + ((dq->b_lim - dq->b_top)/2))) {
	    size_t n = ((dq->b_lim - dq->b_top)/dq->item_size) * 2;
	    n = (n < _MIN_COUNT) ? _MIN_COUNT : n;
	    if (!bufrealloc(dq, n, &item_opt, NULL)) {
		return NULL;
	    }
	    /* DON'T CENTERING in case of right side extension */
	} else {
	    centering(dq, &item_opt);
	}
    }

    /* add item to rigth */
    if (item_opt != NULL) {
	(void)memmove(dq->tail, item_opt, dq->item_size);
    }
    dq->tail += dq->item_size;
    return dq->tail - dq->item_size;
}

void *c7_deque_insert(c7_deque_t dq, size_t index, void *item, size_t count)
{
    const ssize_t z = count * dq->item_size;
    char *insert = c7_deque_nth(dq, index);

    if (insert == NULL) {
	c7_status_add(errno = EINVAL, ": c7_deque_insert: index is over\n");
	return NULL;
    }

    if ((dq->b_lim - dq->tail) < z) {
	/* right side has no enough space */
	size_t n = (((dq->tail - dq->b_top) + z) / dq->item_size) * 2;
	n = (n < _MIN_COUNT) ? _MIN_COUNT : n;
	if (!bufrealloc(dq, n, &item, NULL)) {
	    return NULL;
	}
	/* `insert` might be invalid by bufrealloc */
	insert = c7_deque_nth(dq, index);
    }

    /* insert items to rigth side of index */
    dq->tail += z;	// _tail must be updated prior to call c7_deque_nth
    char *shifted = c7_deque_nth(dq, index + count);
    (void)memmove(shifted, insert, z);
    (void)memmove(insert, item, z);
    return insert;
}

void *c7_deque_append(c7_deque_t dq, void *item, size_t count)
{
    const ssize_t z = count * dq->item_size;

    if ((dq->b_lim - dq->tail) < z) {
	/* right side has no enough space */
	size_t n = (((dq->tail - dq->b_top) + z) / dq->item_size) * 2;
	n = (n < _MIN_COUNT) ? _MIN_COUNT : n;
	if (!bufrealloc(dq, n, &item, NULL)) {
	    return NULL;
	}
    }

   /* add items to rigth */
    (void)memmove(dq->tail, item, z);
    dq->tail += z;
    return dq->tail - z;
}

void *c7_deque_extend(c7_deque_t dq, const c7_deque_t dq_ext)
{
    return c7_deque_append(dq, c7_deque_buffer(dq_ext), c7_deque_count(dq_ext));
}

c7_bool_t c7_deque_remove(c7_deque_t dq, size_t index, size_t count)
{
    ssize_t z;
    char *beg, *end;

    if (index > c7_deque_count(dq)) {
	c7_status_add(errno = EINVAL, ": c7_deque_remove: index is over\n");
	return C7_FALSE;
    }
    
    if (count > (c7_deque_count(dq) - index)) {
	count = (c7_deque_count(dq) - index);
    }
    z = count * dq->item_size;
    beg = c7_deque_nth(dq, index);
    end = c7_deque_nth(dq, index + count);
    if (dq->on_remove != NULL) {
	char *item;
	for (item = beg; item < end; item += dq->item_size)
	    dq->on_remove(dq, item);
    }
    if ((beg - dq->head) < (dq->tail - end)) {
	(void)memmove(dq->head + z, dq->head, beg - dq->head);
	dq->head += z;
    } else {
	(void)memmove(beg, end, dq->tail - end);
	dq->tail -= z;
    }

    return C7_TRUE;
}

void c7_deque_reset(c7_deque_t dq)
{
    if (dq->on_remove != NULL) {
	char *item;
	for (item = dq->head; item < dq->tail; item += dq->item_size)
	    dq->on_remove(dq, item);
    }
    dq->head = dq->tail = dq->b_top;
}

void c7_deque_destroy(c7_deque_t dq)
{
    if (dq != NULL) {
	c7_deque_reset(dq);
	free(dq->b_top);
	(void)memset(dq, 0, sizeof(*dq));
	free(dq);
    }
}

c7_str_t *c7_deque_debug(c7_deque_t dq, c7_str_t *sbp)
{
    if (dq == NULL) {
	return c7_strcpy(sbp, "---------------- dq is NULL ----------------\n");
    }
    return c7_sprintf(sbp,
		      "b_top:[%ld]%p, head:[0]:%p, tail:[%ld]:%p, b_lim:[%ld]%p, item_size:%ld ",
		      (dq->b_top - dq->head)/(ssize_t)dq->item_size, dq->b_top,
		      dq->head,
		      (dq->tail  - dq->head)/dq->item_size, dq->tail,
		      (dq->b_lim - dq->head)/dq->item_size, dq->b_lim,
		      dq->item_size);
}
