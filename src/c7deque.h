/*
 * c7deque.h
 *
 * https://ccldaout.github.io/libc7/group__c7deque.html
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_DEQUE_H_LOADED__
#define __C7_DEQUE_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7string.h>


typedef struct c7_deque_t_ *c7_deque_t;

#define c7_deque_foreach(dq, vn)					\
    for (ssize_t __iter = __c7_deque_foreach_next((dq), 0, (void **)&(vn), NULL); \
	 (vn) != NULL;							\
	 __iter = __c7_deque_foreach_next((dq), __iter, (void **)&(vn), NULL))

#define c7_deque_foreach_r(dq, vn)					\
    for (ssize_t __iter = __c7_deque_foreach_r_next((dq), 0, (void **)&(vn), NULL); \
	 (vn) != NULL;							\
	 __iter = __c7_deque_foreach_r_next((dq), __iter, (void **)&(vn), NULL))

#define c7_deque_foreach_idx(dq, vn, iv)				\
    for (ssize_t __iter = __c7_deque_foreach_next((dq), 0, (void **)&(vn), &(iv)); \
	 (vn) != NULL;							\
	 __iter = __c7_deque_foreach_next((dq), __iter, (void **)&(vn), &(iv)))

#define c7_deque_foreach_r_idx(dq, vn, iv)				\
    for (ssize_t __iter = __c7_deque_foreach_r_next((dq), 0, (void **)&(vn), &(iv)); \
	 (vn) != NULL;							\
	 __iter = __c7_deque_foreach_r_next((dq), __iter, (void **)&(vn), &(iv)))

c7_deque_t c7_deque_create(size_t item_size,
			   void (*on_remove)(const c7_deque_t dq, void *item));

ssize_t c7_deque_index(const c7_deque_t dq, void *item);

ssize_t c7_deque_count(const c7_deque_t dq);

void *c7_deque_nth(const c7_deque_t dq, ssize_t idx);

void *c7_deque_buffer(const c7_deque_t dq);

ssize_t __c7_deque_foreach_next(c7_deque_t dq, ssize_t i, void **vp, ssize_t *idxp);

ssize_t __c7_deque_foreach_r_next(c7_deque_t dq, ssize_t i, void **vp, ssize_t *idxp);

void *c7_deque_pop_head(c7_deque_t dq);

void *c7_deque_pop_tail(c7_deque_t dq);

void *c7_deque_push_head(c7_deque_t dq, void *item_opt);

void *c7_deque_push_tail(c7_deque_t dq, void *item_opt);

void *c7_deque_insert(c7_deque_t dq, size_t index, void *item, size_t count);

void *c7_deque_append(c7_deque_t dq, void *item, size_t count);

void *c7_deque_extend(c7_deque_t dq, const c7_deque_t dq_ext);

c7_bool_t c7_deque_remove(c7_deque_t dq, size_t index, size_t count);

void c7_deque_reset(c7_deque_t dq);

void c7_deque_destroy(c7_deque_t dq);

c7_str_t *c7_deque_debug(c7_deque_t dq, c7_str_t *sbp);


#if defined(__cplusplus)
}
#endif
#endif /* __C7_DEQUE_H_LOADED__ */
