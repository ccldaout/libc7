/*
 * c7thread.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "_private.h"
#include <c7jmp.h>
#include <c7memory.h>
#include <c7mpool.h>
#include <c7status.h>
#include <c7thread.h>


static struct timespec timespec_at_tmo(int tmo_us, const c7_time_t *ref_op)
{
    struct timespec abstime;
    c7_time_t reftime_us;
    if (ref_op == NULL)
	reftime_us = c7_time_us();
    else
	reftime_us = *ref_op;
    reftime_us += tmo_us;
    abstime.tv_sec = reftime_us / C7_TIME_S_us;
    abstime.tv_nsec = (reftime_us % C7_TIME_S_us) * 1000;
    return abstime;
}


/*----------------------------------------------------------------------------
                  alternative mutax and condition functions
----------------------------------------------------------------------------*/

c7_bool_t __c7_thread_mutex_init(const char *file, int line,
				 pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
    int ret;
    if ((ret = pthread_mutex_init(mutex, attr)) != C7_SYSOK) {
	// errno is not changed in next hook.
	__c7_hook_thread_error(file, line, C7_API_thread_mutex_init, ret);
    }
    return (ret == C7_SYSOK);
}

c7_bool_t __c7_thread_recursive_mutex_init(const char *file, int line,
					   pthread_mutex_t *mutex, const pthread_mutexattr_t *attr_op)
{
    pthread_mutexattr_t attr;
    if (attr_op != NULL)
	attr = *attr_op;
    else
	(void)pthread_mutexattr_init(&attr);
    (void)pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    return __c7_thread_mutex_init(file, line, mutex, &attr);
}

c7_bool_t __c7_thread_cond_init(const char *file, int line,
				pthread_cond_t *cond, pthread_condattr_t *attr)
{
    int ret;
    if ((ret = pthread_cond_init(cond, attr)) != C7_SYSOK) {
	// errno is not changed in next hook.
	__c7_hook_thread_error(file, line, C7_API_thread_cond_init, ret);
    }
    return (ret == C7_SYSOK);
}

c7_bool_t __c7_thread_lock(const char *file, int line, pthread_mutex_t *mutex)
{
    int ret;
    if ((ret = pthread_mutex_lock(mutex)) != C7_SYSOK) {
	// errno is not changed in next hook.
	__c7_hook_thread_error(file, line, C7_API_thread_lock, ret);
    }
    return (ret == C7_SYSOK);
}

c7_bool_t __c7_thread_trylock(const char *file, int line, pthread_mutex_t *mutex)
{
    int ret;
    if ((ret = pthread_mutex_trylock(mutex)) == C7_SYSOK)
	return C7_TRUE;
    if (ret != EBUSY) {
	__c7_hook_thread_error(file, line, C7_API_thread_trylock, ret);
    }
    errno = ret;    // errno is used by caller to judge EBUSY or NOT.
    return C7_FALSE;
}

c7_bool_t __c7_thread_unlock(const char *file, int line, pthread_mutex_t *mutex)
{
    int ret;
    if ((ret = pthread_mutex_unlock(mutex)) != C7_SYSOK) {
	// errno is not changed in next hook.
	__c7_hook_thread_error(file, line, C7_API_thread_unlock, ret);
    }
    return (ret == C7_SYSOK);
}

c7_bool_t __c7_thread_notify(const char *file, int line, pthread_cond_t *cond)
{
    int ret;
    if ((ret = pthread_cond_signal(cond)) != C7_SYSOK) {
	// errno is not changed in next hook.
	__c7_hook_thread_error(file, line, C7_API_thread_notify, ret);
    }
    return (ret == C7_SYSOK);
}

c7_bool_t __c7_thread_notify_all(const char *file, int line, pthread_cond_t *cond)
{
    int ret;
    if ((ret = pthread_cond_broadcast(cond)) != C7_SYSOK) {
	// errno is not changed in next hook.
	__c7_hook_thread_error(file, line, C7_API_thread_notify_all, ret);
    }
    return (ret == C7_SYSOK);
}

c7_bool_t __c7_thread_wait(const char *file, int line,
			   pthread_cond_t *cond, pthread_mutex_t *mutex,
			   const struct timespec *limit_time)
{
    int ret;

    if (limit_time == NULL) {
	if ((ret = pthread_cond_wait(cond, mutex)) != C7_SYSOK)
	    __c7_hook_thread_error(file, line, C7_API_thread_wait, ret);
	return (ret == C7_SYSOK);
    }

    for (;;) {
	if ((ret = pthread_cond_timedwait(cond, mutex, limit_time)) == C7_SYSOK)
	    return C7_TRUE;
	if (ret == EINTR)
	    continue;
	if (ret != ETIMEDOUT) {
	    __c7_hook_thread_error(file, line, C7_API_thread_wait, ret);
	}
	errno = ret;    // errno is used by caller to judge ETIMEDOUT or NOT.
	return C7_FALSE;
    }
}

c7_bool_t (c7_thread_mutex_init)(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
    return c7_thread_mutex_init(mutex, attr);
}

c7_bool_t (c7_thread_recursive_mutex_init)(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr_op)
{
    return c7_thread_recursive_mutex_init(mutex, attr_op);
}

c7_bool_t (c7_thread_cond_init)(pthread_cond_t *cond, pthread_condattr_t *attr)
{
    return c7_thread_cond_init(cond, attr);
}

c7_bool_t (c7_thread_lock)(pthread_mutex_t *mutex)
{
    return c7_thread_lock(mutex);
}

c7_bool_t (c7_thread_trylock)(pthread_mutex_t *mutex)
{
    return c7_thread_trylock(mutex);
}

c7_bool_t (c7_thread_unlock)(pthread_mutex_t *mutex)
{
    return c7_thread_unlock(mutex);
}

c7_bool_t (c7_thread_notify)(pthread_cond_t *cond)
{
    return c7_thread_notify(cond);
}

c7_bool_t (c7_thread_notify_all)(pthread_cond_t *cond)
{
    return c7_thread_notify_all(cond);
}

c7_bool_t (c7_thread_wait)(pthread_cond_t *cond, pthread_mutex_t *mutex,
			   const struct timespec *limit_time_op)
{
    return c7_thread_wait(cond, mutex, limit_time_op);
}


/*----------------------------------------------------------------------------
                              thread operations
----------------------------------------------------------------------------*/

static pthread_mutex_t GlobalLock = PTHREAD_MUTEX_INITIALIZER;
static c7_ll_base_t IniendList = C7_LL_INIT(&IniendList);
static uint64_t ThreadCounter;
static c7_thread_local c7_thread_t Thread;

typedef enum _thread_state_t {
    _STATE_IDLE,		/* c7_thread_t is initalized but no pthread */
    _STATE_RUNNING,		/* target function is running */
    _STATE_FINISHING,
    _STATE_FINISHED,
    _STATE_numof
} _thread_state_t;

struct c7_thread_t_ {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_attr_t thread_attr;
    pthread_t thread;
    _thread_state_t state;
    void (*target)(void *__arg);
    void (*finish)(void *__arg);
    void *__arg;
    c7_thread_end_t endstatus;
    c7_bool_t autofree;
    uint64_t id;
    c7_str_t *name;
};

void c7_thread_register_iniend(c7_thread_iniend_t *iniend)
{
    (void)pthread_mutex_lock(&GlobalLock);
    C7_LL_PUTTAIL(&IniendList, iniend);
    (void)pthread_mutex_unlock(&GlobalLock);
}

void c7_thread_call_init(void)
{
    c7_thread_iniend_t *iniend;
    C7_THREAD_GUARD_ENTER(&GlobalLock);
    C7_LL_FOREACH(&IniendList, iniend) {
	if (iniend->init != NULL)
	    iniend->init();
    }
    C7_THREAD_GUARD_EXIT(&GlobalLock);
}

void c7_thread_call_deinit(void)
{
    c7_thread_iniend_t *iniend;
    C7_THREAD_GUARD_ENTER(&GlobalLock);
    C7_LL_FOREACH_R(&IniendList, iniend) {
	if (iniend->deinit != NULL)
	    iniend->deinit();
    }
    C7_THREAD_GUARD_EXIT(&GlobalLock);
}

static void finalize_thread(void *__void)
{
    c7_thread_t th = __void;
    void (*finish)(void *);

    if (th == NULL)
	return;

    c7_thread_lock(&th->mutex);
    finish = th->finish;
    th->state = _STATE_FINISHING;
    c7_thread_unlock(&th->mutex);
    if (finish)
	finish(th->__arg);
    c7_thread_call_deinit();

    if (th->autofree) {
	(void)pthread_mutex_destroy(&th->mutex);
	(void)pthread_cond_destroy(&th->cond);
	c7_str_free(th->name);
	(void)memset(th, 0, sizeof(*th));
	free(th);
    } else {
	c7_thread_lock(&th->mutex);
	th->state = _STATE_FINISHED;
	c7_thread_notify_all(&th->cond);
	c7_thread_unlock(&th->mutex);
    }
}

static void *thread(void *__arg)
{
    c7_thread_t th = __arg;
    Thread = th;
    pthread_cleanup_push(finalize_thread, th);
    c7_thread_call_init();
    th->state = _STATE_RUNNING;
    th->target(th->__arg);
    th->endstatus = C7_THREAD_END_RETURN;
    pthread_cleanup_pop(1);
    return NULL;
}

static c7_thread_t allocate_thread(void)
{
    c7_thread_t th = c7_malloc(sizeof(*th));
    if (th == NULL)
	return NULL;

    int ret;
    if ((ret = pthread_mutex_init(&th->mutex, NULL)) == C7_SYSOK) {
	if ((ret = pthread_cond_init(&th->cond, NULL)) == C7_SYSOK) {
	    (void)pthread_attr_init(&th->thread_attr);
	    (void)pthread_attr_setdetachstate(&th->thread_attr, PTHREAD_CREATE_DETACHED);
	    th->state = _STATE_IDLE;
	    th->target = NULL;
	    th->finish = NULL;
	    th->__arg = NULL;
	    th->endstatus = C7_THREAD_END_NOT;
	    th->autofree = C7_FALSE;
	    c7_thread_lock(&GlobalLock);
	    if (++ThreadCounter == 0)
		ThreadCounter = 1;
	    th->id = ThreadCounter;
	    c7_thread_unlock(&GlobalLock);
	    th->name = c7_sprintf(c7_str_new_ma(), "t(%02x)", th->id);
	    return th;
	} else
	    c7_status_add(ret, "allcoate_thread: cond_init\n");
	(void)pthread_mutex_destroy(&th->mutex);
    } else
	c7_status_add(ret, "allcoate_thread: mutex_init\n");

    free(th);
    return NULL;
    
}

c7_thread_t c7_thread_new(void (*target)(void *thread_arg),
			  void (*finish)(void *thread_arg),
			  void *thread_arg)
{
    c7_thread_t th;
    if ((th = allocate_thread()) != NULL) {
	th->target = target;
	th->finish = finish;
	th->__arg = thread_arg;
    }
    return th;
}

void c7_thread_set_name(c7_thread_t th, const char *name)
{
    c7_str_reuse(th->name);
    c7_sprintf(th->name, "%s(%02x)", name, th->id);
}

c7_bool_t c7_thread_set_stacksize(c7_thread_t th, int stacksize_kb)
{
    int ret = pthread_attr_setstacksize(&th->thread_attr, stacksize_kb*1024);
    if (ret != C7_SYSOK) {
	c7_status_add(ret, "c7_thread_set_stacksize error\n");
	return C7_FALSE;
    }
    return C7_TRUE;
}

void c7_thread_set_autofree(c7_thread_t th)
{
    th->autofree = C7_TRUE;
}

c7_bool_t c7_thread_start(c7_thread_t th)
{
    int ret = pthread_create(&th->thread, &th->thread_attr, thread, th);
    if (ret != C7_SYSOK) {
	// Don't free thread object th even so autofree is enabled,
	// because error handling is complicated.
	////if (th->autofree)
	////    c7_thread_free(th);
	c7_status_add(ret, "c7_thread_start error\n");
	return C7_FALSE;
    }
    return C7_TRUE;
}

c7_thread_t c7_thread_run(void (*target)(void *),
			  void (*finish)(void *),
			  void *thread_arg,
			  const char *name,
			  int stksize_kb)
{
    c7_thread_t th;
    if ((th = c7_thread_new(target, finish, thread_arg)) != NULL) {
	if (name != NULL)
	    c7_thread_set_name(th, name);
	if (stksize_kb == 0 || c7_thread_set_stacksize(th, stksize_kb)) {
	    if (c7_thread_start(th)) {
		return th;
	    }
	    c7_status_add(0, "cannot start thread: name:%s, stk_kb:%1d\n",
			  c7_thread_name(th), stksize_kb);
	} else
	    c7_status_add(0, "cannot set stacksize: name:%s, stk_kb:%1d\n",
			  c7_thread_name(th), stksize_kb);
	if (th != NULL)
	    c7_thread_free(th);
    } else
	c7_status_add(0, "cannot create thread: name:%s\n", name ? name : "");
    return NULL;
}

c7_bool_t c7_thread_kill(c7_thread_t th, int sig)
{
    int ret = pthread_kill(th->thread, sig);
    if (ret != C7_SYSOK)
	c7_status_add(ret, "c7_thread_kill error\n");
    return (ret == C7_SYSOK);
}

void c7_thread_exit(void)
{
    if (Thread != NULL)
	Thread->endstatus = C7_THREAD_END_EXIT;
    pthread_exit(NULL);
}

void c7_thread_abort(void)
{
    if (Thread != NULL)
	Thread->endstatus = C7_THREAD_END_ABORT;
    pthread_exit(NULL);
}

#if !defined(__ANDROID__)
c7_bool_t c7_thread_cancel(c7_thread_t th)
{
    th->endstatus = C7_THREAD_END_CANCEL;
    int ret = pthread_cancel(th->thread);
    if (ret != C7_SYSOK) {
	th->endstatus = C7_THREAD_END_NOT;
	c7_status_add(ret, "c7_thread_cancel error\n");
    }
    return (ret == C7_SYSOK);
}
#endif

c7_bool_t c7_thread_join(c7_thread_t th, volatile int tmo_us)
{
    struct timespec tmo_time, *tmsp = NULL;
    if (tmo_us >= 0)
	*(tmsp = &tmo_time) = timespec_at_tmo(tmo_us, NULL);

    C7_THREAD_GUARD_ENTER(&th->mutex);
    while (th->state != _STATE_FINISHED) {
	if (!c7_thread_wait(&th->cond, &th->mutex, tmsp)) {
	    c7_thread_unlock(&th->mutex);
	    return C7_FALSE;		// timeout (ETIMEDOUT) or error
	}
    }
    C7_THREAD_GUARD_EXIT(&th->mutex);
    return C7_TRUE;
}

c7_bool_t c7_thread_is_alive(c7_thread_t th)
{
    return (th->state != _STATE_FINISHED);
}

uint64_t c7_thread_id(c7_thread_t th_op)
{
    if (th_op == NULL) {
	if ((th_op = Thread) == NULL)
	    return 0;
    }
    return th_op->id;
}

const char *c7_thread_name(c7_thread_t th_op)
{
    if (th_op == NULL) {
	if ((th_op = Thread) == NULL)
	    return "";
    }
    return c7_strbuf(th_op->name);
}

void *c7_thread_arg(c7_thread_t th_op)
{
    if (th_op == NULL) {
	if ((th_op = Thread) == NULL)
	    return NULL;
    }
    return th_op->__arg;
}

c7_thread_end_t c7_thread_endstatus(c7_thread_t th_op)
{
    if (th_op == NULL) {
	if ((th_op = Thread) == NULL)
	    return C7_THREAD_END_Unknown;
    }
    return th_op->endstatus;
}

c7_thread_t c7_thread_self(void)
{
    return Thread;
}

c7_bool_t c7_thread_free(c7_thread_t th)
{
    _thread_state_t state;
    c7_thread_lock(&th->mutex);
    state = th->state;
    c7_thread_unlock(&th->mutex);
    if (state != _STATE_IDLE && state != _STATE_FINISHED) {
	errno = EINVAL;
	return C7_FALSE;
    }
    (void)pthread_mutex_destroy(&th->mutex);
    (void)pthread_cond_destroy(&th->cond);
    c7_str_free(th->name);
    (void)memset(th, 0, sizeof(*th));
    free(th);
    return C7_TRUE;
}


/*----------------------------------------------------------------------------
      original recursive mutex implementation (statically initializable)
----------------------------------------------------------------------------*/

static c7_thread_local int __owner;

c7_bool_t c7_thread_r_mutex_init(c7_thread_r_mutex_t *r_mutex)
{
    if (c7_thread_mutex_init(&r_mutex->lock, NULL)) {
	if (c7_thread_cond_init(&r_mutex->cond, NULL)) {
	    r_mutex->owner = NULL;
	    r_mutex->count = 0;
	    return C7_TRUE;
	}
    }
    return C7_FALSE;
}

c7_bool_t c7_thread_r_lock(c7_thread_r_mutex_t *r_mutex)
{
    C7_THREAD_GUARD_ENTER(&r_mutex->lock);
    if (r_mutex->owner != &__owner) {
	while (r_mutex->owner != NULL) {
	    if (!c7_thread_wait(&r_mutex->cond, &r_mutex->lock, NULL)) {
		(void)c7_thread_unlock(&r_mutex->lock);
		return C7_FALSE;
	    }
	}
	r_mutex->owner = &__owner;
    }
    r_mutex->count++;
    C7_THREAD_GUARD_EXIT(&r_mutex->lock);
    return C7_TRUE;
}

c7_bool_t c7_thread_r_unlock(c7_thread_r_mutex_t *r_mutex)
{
    c7_thread_lock(&r_mutex->lock);
    if (r_mutex->owner != &__owner) {
	c7_status_add(EPERM, ": [FATAL] c7_thread_runlock failed.");
	c7_thread_unlock(&r_mutex->lock);
	return C7_FALSE;
    }
    r_mutex->count--;
    if (r_mutex->count == 0) {
	r_mutex->owner = NULL;
	c7_thread_notify_all(&r_mutex->cond);
    }
    c7_thread_unlock(&r_mutex->lock);
    return C7_TRUE;
}


/*----------------------------------------------------------------------------
                   counter - simple synchronization mechanism
----------------------------------------------------------------------------*/

struct c7_thread_counter_t_ {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int64_t counter;
};

c7_thread_counter_t c7_thread_counter_init(int ini_count)
{
    c7_thread_counter_t ct = c7_malloc(sizeof(*ct));
    if (ct != NULL) {
	if (c7_thread_mutex_init(&ct->mutex, NULL)) {
	    if (c7_thread_cond_init(&ct->cond, NULL)) {
		ct->counter = ini_count;
		return ct;
	    }
	    (void)pthread_mutex_destroy(&ct->mutex);
	}
	free(ct);
    }
    return NULL;
}
    
int c7_thread_counter_value(c7_thread_counter_t ct)
{
    int v;
    c7_thread_lock(&ct->mutex);
    v = ct->counter;
    c7_thread_unlock(&ct->mutex);
    return v;
}

c7_bool_t c7_thread_counter_is(c7_thread_counter_t ct, int count)
{
    c7_bool_t v;
    c7_thread_lock(&ct->mutex);
    v = (ct->counter == count);
    c7_thread_unlock(&ct->mutex);
    return v;
}

c7_bool_t c7_thread_counter_down_if(c7_thread_counter_t ct, volatile int tmo_us)
{
    struct timespec tmo_time, *tmsp = NULL;
    if (tmo_us >= 0)
	*(tmsp = &tmo_time) = timespec_at_tmo(tmo_us, NULL);

    C7_THREAD_GUARD_ENTER(&ct->mutex);
    while (ct->counter <= 0) {
	if (!c7_thread_wait(&ct->cond, &ct->mutex, tmsp)) {
	    c7_thread_unlock(&ct->mutex);
	    return C7_FALSE;		// timeout (ETIMEDOUT) or error
	}
    }
    ct->counter--;
    C7_THREAD_GUARD_EXIT(&ct->mutex);
    return C7_TRUE;
}

void c7_thread_counter_move(c7_thread_counter_t ct, int delta)
{
    c7_thread_lock(&ct->mutex);
    ct->counter += delta;
    c7_thread_notify_all(&ct->cond);
    c7_thread_unlock(&ct->mutex);
}

void c7_thread_counter_set(c7_thread_counter_t ct, int count)
{
    c7_thread_lock(&ct->mutex);
    ct->counter = count;
    c7_thread_notify_all(&ct->cond);
    c7_thread_unlock(&ct->mutex);
}

c7_bool_t c7_thread_counter_wait(c7_thread_counter_t ct, int expect, volatile int tmo_us)
{
    struct timespec tmo_time, *tmsp = NULL;
    if (tmo_us >= 0)
	*(tmsp = &tmo_time) = timespec_at_tmo(tmo_us, NULL);

    C7_THREAD_GUARD_ENTER(&ct->mutex);
    while (ct->counter != expect) {
	if (!c7_thread_wait(&ct->cond, &ct->mutex, tmsp)) {
	    c7_thread_unlock(&ct->mutex);
	    return C7_FALSE;		// timeout (ETIMEDOUT) or error
	}
    }
    C7_THREAD_GUARD_EXIT(&ct->mutex);
    return C7_TRUE;
}

void c7_thread_counter_free(c7_thread_counter_t ct)
{
    (void)pthread_cond_destroy(&ct->cond);
    (void)pthread_mutex_destroy(&ct->mutex);
    free(ct);
}


/*----------------------------------------------------------------------------
                   mask - bitwise synchronization mechanism
----------------------------------------------------------------------------*/

struct c7_thread_mask_t_ {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint64_t mask;
};

c7_thread_mask_t c7_thread_mask_init(uint64_t ini_mask)
{
    c7_thread_mask_t m = c7_malloc(sizeof(*m));
    if (m != NULL) {
	if (c7_thread_mutex_init(&m->mutex, NULL)) {
	    if (c7_thread_cond_init(&m->cond, NULL)) {
		m->mask = ini_mask;
		return m;
	    }
	    (void)pthread_mutex_destroy(&m->mutex);
	}
	free(m);
    }
    return NULL;
}
    
uint64_t c7_thread_mask_value(c7_thread_mask_t m)
{
    uint64_t mask;
    c7_thread_lock(&m->mutex);
    mask = m->mask;
    c7_thread_unlock(&m->mutex);
    return mask;
    
}

void c7_thread_mask_change(c7_thread_mask_t m, uint64_t set, uint64_t clear)
{
    c7_thread_lock(&m->mutex);
    m->mask |= set;
    m->mask &= (~clear);
    c7_thread_notify_all(&m->cond);
    c7_thread_unlock(&m->mutex);
}

uint64_t c7_thread_mask_wait(c7_thread_mask_t m, uint64_t expect, uint64_t clear, volatile int tmo_us)
{
    struct timespec tmo_time, *tmsp = NULL;
    if (tmo_us >= 0)
	*(tmsp = &tmo_time) = timespec_at_tmo(tmo_us, NULL);

    C7_THREAD_GUARD_ENTER(&m->mutex);
    while ((m->mask & expect) == 0) {
	if (!c7_thread_wait(&m->cond, &m->mutex, tmsp)) {
	    c7_thread_unlock(&m->mutex);
	    return 0;			// timeout (ETIMEDOUT) or error
	}
    }
    expect &= m->mask;
    m->mask &= (~clear);
    C7_THREAD_GUARD_EXIT(&m->mutex);
    return expect;
}

void c7_thread_mask_free(c7_thread_mask_t m)
{
    (void)pthread_cond_destroy(&m->cond);
    (void)pthread_mutex_destroy(&m->mutex);
    free(m);
}


/*----------------------------------------------------------------------------
               inter-thread pipe (fixed size array of pointer)
----------------------------------------------------------------------------*/

struct c7_thread_fpipe_t_ {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    void **buffer;
    uint32_t size;
    uint32_t put_ptr;
    uint32_t get_ptr;
};

c7_thread_fpipe_t c7_thread_fpipe_init(int ent_count)
{
    c7_thread_fpipe_t fpipe = c7_calloc(sizeof(*fpipe), 1);
    if (fpipe == NULL)
	return NULL;

    if (c7_thread_mutex_init(&fpipe->mutex, NULL)) {
	if (c7_thread_cond_init(&fpipe->cond, NULL)) {
	    fpipe->buffer = c7_malloc(sizeof(*fpipe->buffer) * ent_count);
	    if (fpipe->buffer != NULL) {
		fpipe->size = ent_count;
		fpipe->put_ptr = fpipe->get_ptr = 0;
		return fpipe;
	    }
	    (void)pthread_cond_destroy(&fpipe->cond);
	}
	(void)pthread_mutex_destroy(&fpipe->mutex);
    }
    free(fpipe);
    return NULL;
}

static c7_bool_t fpipe_resize(c7_thread_fpipe_t fpipe, int ent_count)
{
    if (fpipe->put_ptr != fpipe->get_ptr) {
	c7_status_add(EINVAL, "c7_thread_fpipe_reset: not empty\n");
	return C7_FALSE;
    }
    if (fpipe->size == ent_count)
	return C7_TRUE;
    void *buffer = c7_realloc(fpipe->buffer, sizeof(*fpipe->buffer) * ent_count);
    if (buffer == NULL)
	return C7_FALSE;
    fpipe->buffer = buffer;
    fpipe->size = ent_count;
    fpipe->put_ptr = fpipe->get_ptr = 0;
    return C7_TRUE;
}

c7_bool_t c7_thread_fpipe_resize(c7_thread_fpipe_t fpipe, int ent_count)
{
    c7_bool_t ret;
    C7_THREAD_GUARD_ENTER(&fpipe->mutex);
    ret = fpipe_resize(fpipe, ent_count);
    C7_THREAD_GUARD_EXIT(&fpipe->mutex);
    return ret;
}

void c7_thread_fpipe_reset(c7_thread_fpipe_t fpipe)
{
    c7_thread_lock(&fpipe->mutex);
    fpipe->put_ptr = fpipe->get_ptr = 0;
    c7_thread_unlock(&fpipe->mutex);
}

void c7_thread_fpipe_reset_and_put(c7_thread_fpipe_t fpipe, void *data)
{
    c7_thread_lock(&fpipe->mutex);
    fpipe->buffer[fpipe->get_ptr = 0] = data;
    fpipe->put_ptr = 1;
    c7_thread_notify_all(&fpipe->cond);
    c7_thread_unlock(&fpipe->mutex);
}

c7_bool_t c7_thread_fpipe_put(c7_thread_fpipe_t fpipe, void *data, volatile int tmo_us)
{
    struct timespec tmo_time, *tmsp = NULL;
    if (tmo_us >= 0)
	*(tmsp = &tmo_time) = timespec_at_tmo(tmo_us, NULL);

    c7_bool_t ret = C7_TRUE;
    C7_THREAD_GUARD_ENTER(&fpipe->mutex);
    while (fpipe->put_ptr - fpipe->get_ptr >= fpipe->size) {
	if (!c7_thread_wait(&fpipe->cond, &fpipe->mutex, tmsp)) {
	    c7_thread_unlock(&fpipe->mutex);
	    return C7_FALSE;		// timeout (ETIMEDOUT) or error
	}
    }
    if (fpipe->put_ptr != fpipe->get_ptr &&
	fpipe->buffer[(fpipe->put_ptr - 1) % fpipe->size] == NULL) {
	c7_status_add(EINVAL, "c7_thread_fpipe_put: already EOF.\n");
	ret = C7_FALSE;
    } else {
	fpipe->buffer[fpipe->put_ptr % fpipe->size] = data;
	fpipe->put_ptr++;
	c7_thread_notify_all(&fpipe->cond);
    }
    C7_THREAD_GUARD_EXIT(&fpipe->mutex);
    return ret;
}

void *c7_thread_fpipe_get(c7_thread_fpipe_t fpipe, volatile int tmo_us)
{
    void *data;
    struct timespec tmo_time, *tmsp = NULL;
    if (tmo_us >= 0)
	*(tmsp = &tmo_time) = timespec_at_tmo(tmo_us, NULL);

    C7_THREAD_GUARD_ENTER(&fpipe->mutex);
    while (fpipe->get_ptr >= fpipe->put_ptr) {
	if (!c7_thread_wait(&fpipe->cond, &fpipe->mutex, tmsp)) {
	    c7_thread_unlock(&fpipe->mutex);
	    return NULL;		// timeout (ETIMEDOUT) or error
	}
    }
    if (fpipe->get_ptr >= fpipe->size) {
	fpipe->get_ptr -= fpipe->size;
	fpipe->put_ptr -= fpipe->size;
    }
    if ((data = fpipe->buffer[fpipe->get_ptr]) != NULL) {
	fpipe->get_ptr++;
    }
    c7_thread_notify_all(&fpipe->cond);
    C7_THREAD_GUARD_EXIT(&fpipe->mutex);

    if (data == NULL) {
	c7_status_clear();
	errno = 0;
    }
    return data;
}

void c7_thread_fpipe_free(c7_thread_fpipe_t fpipe)
{
    if (fpipe != NULL) {
	(void)pthread_cond_destroy(&fpipe->cond);
	(void)pthread_mutex_destroy(&fpipe->mutex);
	free(fpipe->buffer);
	free(fpipe);
    }
}


/*----------------------------------------------------------------------------
                       inter-thread pipe (linked list)
----------------------------------------------------------------------------*/

struct c7_thread_vpipe_t_ {
    ptrdiff_t linkoff;
    c7_thread_vpipe_link_t *root;
    c7_thread_vpipe_link_t *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

static c7_thread_vpipe_link_t VpipeEOF;

c7_thread_vpipe_t c7_thread_vpipe_init(ptrdiff_t linkoff)
{
    c7_thread_vpipe_t vpipe = c7_calloc(sizeof(*vpipe), 1);
    if (vpipe == NULL)
	return NULL;

    vpipe->linkoff = linkoff;
    vpipe->root = vpipe->tail = NULL;
    if (c7_thread_mutex_init(&vpipe->mutex, NULL)) {
	if (c7_thread_cond_init(&vpipe->cond, NULL))
	    return vpipe;
	(void)pthread_mutex_destroy(&vpipe->mutex);
    }
    return NULL;
}

static void *vpipe_reset(c7_thread_vpipe_t vpipe,
			 c7_thread_vpipe_link_t *next_link)
{
    c7_thread_lock(&vpipe->mutex);
    void *data = vpipe->root;
    vpipe->root = vpipe->tail = next_link;
    c7_thread_notify_all(&vpipe->cond);
    c7_thread_unlock(&vpipe->mutex);

    if (data != NULL && data != &VpipeEOF)
	data = (char *)data - vpipe->linkoff;
    else
	data = NULL;
    return data;
}

void *c7_thread_vpipe_reset(c7_thread_vpipe_t vpipe)
{
    return vpipe_reset(vpipe, NULL);
}

void *c7_thread_vpipe_reset_and_put(c7_thread_vpipe_t vpipe, void *data)
{
    c7_thread_vpipe_link_t *link_of_data;
    if (data != NULL)
	link_of_data = (void *)((char *)data + vpipe->linkoff);
    else
	link_of_data = &VpipeEOF;
    link_of_data->next = NULL;

    return vpipe_reset(vpipe, link_of_data);
}

c7_bool_t c7_thread_vpipe_put(c7_thread_vpipe_t vpipe, void *data)
{
    c7_thread_vpipe_link_t *link_of_data;
    if (data != NULL)
	link_of_data = (void *)((char *)data + vpipe->linkoff);
    else
	link_of_data = &VpipeEOF;

    c7_bool_t ret = C7_TRUE;
    C7_THREAD_GUARD_ENTER(&vpipe->mutex);
    if (vpipe->tail == &VpipeEOF) {
	c7_status_add(EINVAL, "c7_thread_vpipe_put: already EOF.\n");
	ret = C7_FALSE;
    } else {
	link_of_data->next = NULL;
	if (vpipe->tail == NULL) {
	    vpipe->root = vpipe->tail = link_of_data;
	} else {
	    vpipe->tail->next = link_of_data;
	    vpipe->tail = link_of_data;
	}
	c7_thread_notify_all(&vpipe->cond);
    }
    C7_THREAD_GUARD_EXIT(&vpipe->mutex);
    return ret;
}

void *c7_thread_vpipe_get(c7_thread_vpipe_t vpipe, volatile int tmo_us)
{
    struct timespec tmo_time, *tmsp = NULL;
    if (tmo_us >= 0)
	*(tmsp = &tmo_time) = timespec_at_tmo(tmo_us, NULL);

    c7_thread_vpipe_link_t *link_of_data;

    C7_THREAD_GUARD_ENTER(&vpipe->mutex);
    while (vpipe->root == NULL) {
	if (!c7_thread_wait(&vpipe->cond, &vpipe->mutex, tmsp)) {
	    c7_thread_unlock(&vpipe->mutex);
	    return NULL;		// timeout (ETIMEDOUT) or error
	}
    }
    if ((link_of_data = vpipe->root) != &VpipeEOF) {
	vpipe->root = link_of_data->next;
	if (vpipe->root == NULL)
	    vpipe->tail = NULL;
    }
    C7_THREAD_GUARD_EXIT(&vpipe->mutex);
    
    if (link_of_data == &VpipeEOF) {
	c7_status_clear();
	errno = 0;
	return NULL;	
    }
    return (((char *)(void *)link_of_data) - vpipe->linkoff);
}

void c7_thread_vpipe_free(c7_thread_vpipe_t vpipe)
{
    if (vpipe != NULL) {
	(void)pthread_mutex_destroy(&vpipe->mutex);
	(void)pthread_cond_destroy(&vpipe->cond);
	free(vpipe);
    }
}
