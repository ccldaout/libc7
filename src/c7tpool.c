/*
 * c7tpool.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <c7deque.h>
#include <c7memory.h>
#include <c7status.h>
#include <c7tpool.h>


typedef enum _req_type_t {
    _REQ_TYPE_SHUTDOWN,
    _REQ_TYPE_FUNCTION
} _req_type_t;


typedef struct _req_t {
    _req_type_t type;
    uint64_t id;
    c7_thread_counter_t finish_countdown;	/* optional */
    void (*function)(void *__arg);
    void (*finalize)(void *__arg);
    void *__arg;
} _req_t;

struct c7_tpool_t_ {
    c7_thread_counter_t thr_counter;
    struct {
	pthread_mutex_t mutex;
	pthread_cond_t wakeup;
	c7_deque_t que;				/* queue of _req_t */
	uint64_t id;
    } req;
};

static c7_thread_local struct {
    _req_t *req;
    jmp_buf jmpbuf;
} Context;


static void worker_thread(void *__arg)
{
    c7_tpool_t tp = __arg;
    for (;;) {
	_req_t req;

	C7_THREAD_GUARD_ENTER(&tp->req.mutex);
	while (c7_deque_count(tp->req.que) == 0)
	    c7_thread_wait(&tp->req.wakeup, &tp->req.mutex, NULL);
	req = *(_req_t *)c7_deque_pop_head(tp->req.que);
	C7_THREAD_GUARD_EXIT(&tp->req.mutex);

	if (req.type == _REQ_TYPE_SHUTDOWN) {
	    return;
	}

	pthread_cleanup_push(req.finalize, req.__arg);
	Context.req = &req;
	if (setjmp(Context.jmpbuf) == 0)
	    req.function(req.__arg);
	pthread_cleanup_pop(1);

	if (req.finish_countdown)
	    c7_thread_counter_down(req.finish_countdown);
    }
}

static void default_finalize(void *__arg)
{
    ;
}

static void worker_finish(void *__arg)
{
    c7_tpool_t tp = __arg;
    c7_thread_counter_down(tp->thr_counter);
}

static c7_tpool_t startthreads(c7_tpool_t tp, int thread_count, int stacksize_kb)
{
    int i;
    for (i = 0; i < thread_count; i++) {
	c7_bool_t start = C7_FALSE;
	c7_thread_t thr = c7_thread_new(worker_thread, worker_finish, tp);
	if (thr) {
	    if (stacksize_kb != 0)
		c7_thread_set_stacksize(thr, stacksize_kb);
	    c7_thread_set_autofree(thr);
	    start = c7_thread_start(thr);
	}
	if (!start) {
	    if (thr)
		c7_thread_free(thr);
	    c7_thread_counter_set(tp->thr_counter, i);
	    c7_tpool_shutdown(tp);
	    return NULL;
	}
    }
    c7_thread_counter_set(tp->thr_counter, i);
    return tp;
}

c7_tpool_t c7_tpool_init(int thread_count, int stacksize_kb)
{
    c7_tpool_t tp = c7_malloc(sizeof(*tp));
    if (tp == NULL)
	return NULL;

    tp->thr_counter = c7_thread_counter_init(0);
    if (tp->thr_counter != NULL) {
	if (c7_thread_mutex_init(&tp->req.mutex, NULL)) {
	    if (c7_thread_cond_init(&tp->req.wakeup, NULL)) {
		tp->req.que = c7_deque_create(sizeof(_req_t), NULL);
		if (tp->req.que != NULL) {
		    tp->req.id = 0;
		    return startthreads(tp, thread_count, stacksize_kb);
		}
		c7_deque_destroy(tp->req.que);
	    }
	    (void)pthread_mutex_destroy(&tp->req.mutex);
	}
	c7_thread_counter_free(tp->thr_counter);
    }
    free(tp);
    return NULL;
}

uint64_t c7_tpool_register(c7_tpool_t tp,
			   void (*function)(void *__arg),
			   void *__arg,
			   c7_thread_counter_t finish_countdown_opt)
{
    return c7_tpool_enqueue(tp, function, NULL, __arg, finish_countdown_opt);
}

uint64_t c7_tpool_enqueue(c7_tpool_t tp,
			  void (*function)(void *__arg),
			  void (*finalize)(void *__arg),
			  void *__arg,
			  c7_thread_counter_t finish_countdown_opt)
{
    _req_t req;

    c7_thread_lock(&tp->req.mutex);
    req.type = _REQ_TYPE_FUNCTION;
    if (tp->req.id == C7_TPOOL_REGISTER_FAIL)
	tp->req.id = C7_TPOOL_REGISTER_FAIL + 1;
    req.id = tp->req.id++;
    req.finish_countdown = finish_countdown_opt;
    req.function = function;
    req.finalize = (finalize != NULL) ? finalize : default_finalize;
    req.__arg = __arg;
    if (c7_deque_push_tail(tp->req.que, &req))
	c7_thread_notify_all(&tp->req.wakeup);
    else {
	req.id = C7_TPOOL_REGISTER_FAIL;
	c7_status_add(0, "c7_tpool_register: error\n");
    }
    c7_thread_unlock(&tp->req.mutex);

    return req.id;
}

void c7_tpool_exit(void)
{
    longjmp(Context.jmpbuf, 1);
}

void *c7_tpool_arg(void)
{
    return Context.req->__arg;
}

void c7_tpool_shutdown(c7_tpool_t tp)
{
    int thr_count = c7_thread_counter_value(tp->thr_counter);
    _req_t req;
    req.type = _REQ_TYPE_SHUTDOWN;

    c7_thread_lock(&tp->req.mutex);
    while (thr_count--)
	(void)c7_deque_push_tail(tp->req.que, &req);
    c7_thread_notify_all(&tp->req.wakeup);
    c7_thread_unlock(&tp->req.mutex);
    
    (void)c7_thread_counter_wait(tp->thr_counter, 0, -1);

    c7_thread_counter_free(tp->thr_counter);
    c7_deque_destroy(tp->req.que);
    (void)pthread_cond_destroy(&tp->req.wakeup);
    (void)pthread_mutex_destroy(&tp->req.mutex);
    (void)memset(tp, 0, sizeof(*tp));
    free(tp);
}
