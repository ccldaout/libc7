/*
 * c7parray.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_PARRAY_H_LOADED__
#define __C7_PARRAY_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7memory.h>


typedef struct c7_parray_t_ *c7_parray_t;


#define c7_parray_foreach(pa, vn)					\
    for (int __iter = __c7_parray_loop_next((pa), 0, (void **)&(vn), NULL); \
	 (vn) != 0;							\
	 __iter = __c7_parray_loop_next((pa), __iter, (void **)&(vn), NULL))

#define c7_parray_foreach_idx(pa, vn, in)				\
    for (int __iter = __c7_parray_loop_next((pa), 0, (void **)&(vn), &(in)); \
	 (vn) != 0;							\
	 __iter = __c7_parray_loop_next((pa), __iter, (void **)&(vn), &(in)))

c7_parray_t c7_parray_create(size_t itemsize,
			     c7_bool_t (*init)(void *addr, int index),
			     void (*deinit)(void *addr, int index));

int __c7_parray_loop_next(c7_parray_t pa, int i, void **vp, int *idxp);

int c7_parray_count(const c7_parray_t pa);

c7_bool_t c7_parray_check(const c7_parray_t pa, int index);

void *c7_parray_get(c7_parray_t pa, int index);

void *c7_parray_new(c7_parray_t pa, int index);

void *c7_parray_newif(c7_parray_t pa, int index, c7_bool_t *exist_op);

void *c7_parray_new_auto(c7_parray_t pa, int *indexp);

c7_bool_t c7_parray_move(c7_parray_t pa, int src_index, int dst_index,
			 c7_bool_t overwrite);

void *c7_parray_release(c7_parray_t pa, int index);

int c7_parray_last_index(c7_parray_t pa);

void c7_parray_free(c7_parray_t pa, int index);

void c7_parray_destroy(c7_parray_t pa);


#if defined(__cplusplus)
}
#endif
#endif /* __C7_PARRAY_H_LOADED__ */
