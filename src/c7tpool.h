/*
 * c7tpool.h
 *
 * https://ccldaout.github.io/libc7/group__c7tpool.html
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_TPOOL_H_LOADED__
#define __C7_TPOOL_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7thread.h>


#define C7_TPOOL_REGISTER_FAIL	(0)

typedef struct c7_tpool_t_ *c7_tpool_t;

c7_tpool_t c7_tpool_init(int thread_count, int stacksize_kb);
uint64_t c7_tpool_enqueue(c7_tpool_t tp,
			  void (*function)(void *__arg),
			  void (*finalize)(void *__arg),
			  void *__arg,
			  c7_thread_counter_t finish_countdown_opt);
void *c7_tpool_arg(void);
void c7_tpool_exit(void);
void c7_tpool_shutdown(c7_tpool_t tp);

// OBSOLETE
uint64_t c7_tpool_register(c7_tpool_t tp,
			   void (*function)(void *__arg),
			   void *__arg,
			   c7_thread_counter_t finish_countdown_opt);

#if defined(__cplusplus)
}
#endif
#endif /* c7tpool.h */
