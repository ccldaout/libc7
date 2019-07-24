/*
 * c7parray.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <string.h>
#include <stdlib.h>
#include <c7parray.h>
#include <c7status.h>


struct c7_parray_t_ {
    ssize_t itemsize;
    c7_bool_t (*init)(void *addr, int index);
    void (*deinit)(void *addr, int index);
    void **array;
    int alcnt;
    int last; 
    int nitem;
};


c7_parray_t c7_parray_create(size_t itemsize,
			     c7_bool_t (*init)(void *addr, int index),
			     void (*deinit)(void *addr, int index))
{
    c7_parray_t pa = c7_malloc(sizeof(*pa));
    if (pa != NULL) {
	pa->itemsize = itemsize;
	pa->init = init;
	pa->deinit = deinit;
	pa->array = NULL;
	pa->alcnt = 0;
	pa->last  = -1;
	pa->nitem = 0;
    }
    return pa;
}

int __c7_parray_loop_next(c7_parray_t pa, int i, void **vp, int *idxp)
{
    int last = i;
    for (; i <= pa->last; i++) {
	if (pa->array[i] != NULL) {
	    *vp = pa->array[i];
	    if (idxp != NULL)
		*idxp = i;
	    return i + 1;
	}
    }
    pa->last = last - 1;
    *vp = NULL;
    if (idxp != NULL)
	*idxp = -1;
    return last;
}

int c7_parray_count(const c7_parray_t pa)
{
    return pa->nitem;
}

c7_bool_t c7_parray_check(const c7_parray_t pa, int index)
{
    return (0 <= index && index < pa->alcnt && pa->array[index]);
}

void *c7_parray_get(c7_parray_t pa, int index)
{
    if (c7_parray_check(pa, index))
	return pa->array[index];
    c7_status_add(EINVAL, ": c7_parray_get: invalid index:%1d\n", index);
    return NULL;
}

static void *__c7_parray_newitem(c7_parray_t pa, int index)
{
    void *item;
    if (index >= pa->alcnt) {
	int newalcnt = (index + 16) + (index >> 2);
	void **newp = c7_realloc(pa->array, sizeof(*newp) * newalcnt);
	if (newp == NULL)
	    return NULL;
	(void)memset(&newp[pa->alcnt], 0,
		     sizeof(*newp) * (newalcnt - pa->alcnt));	/* NOT STRICT */
	pa->array = newp;
	pa->alcnt = newalcnt;
    }

    if ((item = c7_calloc(pa->itemsize, 1)) != NULL) {
	if (pa->init == NULL || pa->init(item, index)) {
	    pa->array[index] = item;
	    pa->nitem++;
	    if (index > pa->last)
		pa->last = index;
	    return item;
	}
	free(item);
    }
    return NULL;
}

void *c7_parray_new(c7_parray_t pa, int index)
{
    if (!c7_parray_check(pa, index))
	return __c7_parray_newitem(pa, index);
    c7_status_add(EINVAL, ": c7_parray_new: index:%1d is already used.\n", index);
    return NULL;
}

void *c7_parray_newif(c7_parray_t pa, int index, c7_bool_t *exist_op)
{
    if (c7_parray_check(pa, index)) {
	if (exist_op)
	    *exist_op = C7_TRUE;
	return pa->array[index];
    }
    if (exist_op)
	*exist_op = C7_FALSE;
    return __c7_parray_newitem(pa, index);
}

void *c7_parray_new_auto(c7_parray_t pa, int *indexp)
{
    int i;
    void *item;
    for (i = 0; i <= pa->last; i++) {
	if ((item = pa->array[i]) == NULL)
	    return __c7_parray_newitem(pa, *indexp = i);
    }
    return __c7_parray_newitem(pa, *indexp = i);
}

c7_bool_t c7_parray_move(c7_parray_t pa, int src_index, int dst_index,
			 c7_bool_t overwrite)
{
    if (src_index == dst_index)
	return C7_TRUE;
    if (c7_parray_check(pa, dst_index)) {
	if (!overwrite) {
	    c7_status_add(EINVAL, ": c7_parray_move: index:%1d exist and not overwrite mode\n");
	    return C7_FALSE;
	}
	c7_parray_free(pa, dst_index);
    }
    pa->array[dst_index] = pa->array[src_index];
    pa->array[src_index] = NULL;
    if (src_index == pa->last)
	pa->last--;			/* NOT STRICT */
    if (dst_index > pa->last)
	pa->last = dst_index;
    return C7_TRUE;
}

void *c7_parray_release(c7_parray_t pa, int index)
{
    if (c7_parray_check(pa, index)) {
	void *item = pa->array[index];
	pa->array[index] = NULL;
	if (index == pa->last)
	    pa->last--;			/* NOT STRICT */
	pa->nitem--;
	return item;
    }
    c7_status_add(EINVAL, ": c7_parray_release: invalid index:%1d\n", index);
    return NULL;
}

int c7_parray_last_index(c7_parray_t pa)
{
    for (int i = pa->last; i >= 0; i--) {
	if (pa->array[i] != NULL) {
	    pa->last = i;
	    return i;
	}
    }
    return -1;
}

void c7_parray_free(c7_parray_t pa, int index)
{
    if (c7_parray_check(pa, index)) {
	void *item = pa->array[index];
	if (pa->deinit)
	    pa->deinit(item, index);
	free(item);
	pa->array[index] = NULL;
	if (index == pa->last)
	    pa->last--;			/* NOT STRICT */
	pa->nitem--;
    }
}

void c7_parray_destroy(c7_parray_t pa)
{
    if (pa != NULL) {
	for (int i = 0; i <= pa->last; i++) {
	    void *item = pa->array[i];
	    if (item != NULL) {
		if (pa->deinit)
		    pa->deinit(item, i);
		free(item);
		pa->array[i] = NULL;
	    }
	}
	free(pa->array);
	(void)memset(pa, 0, sizeof(*pa));
	free(pa);
    }
}
