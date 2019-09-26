/*
 * c7thread.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_THREAD_H_LOADED__
#define __C7_THREAD_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7lldef.h>
#include <c7app.h>
#include <pthread.h>


/*----------------------------------------------------------------------------
                  alternative mutax and condition functions
----------------------------------------------------------------------------*/

#define C7_THREAD_UNLOCK_PUSH(mp)	\
    pthread_cleanup_push((void (*)(void*))(c7_thread_unlock), mp)

#define C7_THREAD_UNLOCK_POP()	pthread_cleanup_pop(0)

#define C7_THREAD_GUARD_ENTER(mp)					\
    pthread_cleanup_push((void (*)(void*))(c7_thread_unlock), mp);	\
    c7_thread_lock(mp)

#define C7_THREAD_GUARD_EXIT(mp)	\
    c7_thread_unlock(mp);		\
    pthread_cleanup_pop(0)

#define c7_thread_mutex_init(m, a)		__c7_thread_mutex_init(__FILE__, __LINE__, (m), (a))
#define c7_thread_recursive_mutex_init(m, a)	__c7_thread_recursive_mutex_init(__FILE__, __LINE__, (m), (a))
#define c7_thread_cond_init(c, a)		__c7_thread_cond_init(__FILE__, __LINE__, (c), (a))
#define c7_thread_lock(m)	__c7_thread_lock(__FILE__, __LINE__, (m))
#define c7_thread_trylock(m)	__c7_thread_trylock(__FILE__, __LINE__, (m))
#define c7_thread_unlock(m)	__c7_thread_unlock(__FILE__, __LINE__, (m))
#define c7_thread_notify(c)	__c7_thread_notify(__FILE__, __LINE__, (c))
#define c7_thread_notify_all(c)	__c7_thread_notify_all(__FILE__, __LINE__, (c))
#define c7_thread_wait(c, m, t)	__c7_thread_wait(__FILE__, __LINE__, (c), (m), (t))

c7_bool_t __c7_thread_mutex_init(const char *, int, pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
c7_bool_t __c7_thread_recursive_mutex_init(const char *file, int line,
					   pthread_mutex_t *mutex, const pthread_mutexattr_t *attr_op);
c7_bool_t __c7_thread_cond_init(const char *, int, pthread_cond_t *cond, pthread_condattr_t *attr);
c7_bool_t __c7_thread_lock(const char *, int, pthread_mutex_t *mutex);
c7_bool_t __c7_thread_trylock(const char *, int, pthread_mutex_t *mutex);
c7_bool_t __c7_thread_unlock(const char *, int, pthread_mutex_t *mutex);
c7_bool_t __c7_thread_notify(const char *, int, pthread_cond_t *cond);
c7_bool_t __c7_thread_notify_all(const char *, int, pthread_cond_t *cond);
c7_bool_t __c7_thread_wait(const char *, int,
			   pthread_cond_t *cond, pthread_mutex_t *mutex,
			   const struct timespec *limit_time_op);

c7_bool_t (c7_thread_mutex_init)(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
c7_bool_t (c7_thread_recursive_mutex_init)(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr_op);
c7_bool_t (c7_thread_cond_init)(pthread_cond_t *cond, pthread_condattr_t *attr);
c7_bool_t (c7_thread_lock)(pthread_mutex_t *mutex);
c7_bool_t (c7_thread_trylock)(pthread_mutex_t *mutex);
c7_bool_t (c7_thread_unlock)(pthread_mutex_t *mutex);
c7_bool_t (c7_thread_notify)(pthread_cond_t *cond);
c7_bool_t (c7_thread_notify_all)(pthread_cond_t *cond);
c7_bool_t (c7_thread_wait)(pthread_cond_t *cond, pthread_mutex_t *mutex,
			   const struct timespec *limit_time_op);


/*----------------------------------------------------------------------------
      original recursive mutex implementation (statically initializable)
----------------------------------------------------------------------------*/

#define C7_THREAD_R_MUTEX_INITIALIZER	\
    { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER }

typedef struct c7_thread_r_mutex_t_ {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int *owner;
    int count;
} c7_thread_r_mutex_t;

c7_bool_t c7_thread_r_mutex_init(c7_thread_r_mutex_t *r_mutex);
c7_bool_t c7_thread_r_lock(c7_thread_r_mutex_t *r_mutex);
c7_bool_t c7_thread_r_unlock(c7_thread_r_mutex_t *r_mutex);


/*----------------------------------------------------------------------------
                              thread operations
----------------------------------------------------------------------------*/

typedef struct c7_thread_t_ *c7_thread_t;

typedef struct c7_thread_iniend_t_ {
    c7_ll_link_t __link;
    c7_bool_t (*init)(void);
    void (*deinit)(void);
} c7_thread_iniend_t;

typedef enum c7_thread_end_t_ {
    C7_THREAD_END_Unknown = -1,
    C7_THREAD_END_NOT,
    C7_THREAD_END_RETURN,		// returned from target.
    C7_THREAD_END_EXIT,			// c7_thread_exit
    C7_THREAD_END_ABORT,		// c7_thread_abort
    C7_THREAD_END_CANCEL,		// c7_thread_cancel by other thread
} c7_thread_end_t;

void c7_thread_register_iniend(c7_thread_iniend_t *iniend);	// iniend must be point persistent memory
c7_bool_t c7_thread_call_init(void);	// API for thread created by except c7thread
void c7_thread_call_deinit(void);	// API for thread created by except c7thread

c7_thread_t c7_thread_new(void (*target)(void *thread_arg),
			  void (*finish)(void *thread_arg),
			  void *thread_arg);
void c7_thread_set_name(c7_thread_t th, const char *name);
c7_bool_t c7_thread_set_stacksize(c7_thread_t th, int stacksize_kb);
void c7_thread_set_autofree(c7_thread_t th);

c7_bool_t c7_thread_start(c7_thread_t th);

c7_thread_t c7_thread_run(void (*target)(void *thread_arg),
			  void (*finish)(void *thread_arg),
			  void *thread_arg,
			  const char *name,
			  int stksize_kb);

void c7_thread_exit(void);
void c7_thread_abort(void);
#if !defined(__ANDROID__)
c7_bool_t c7_thread_cancel(c7_thread_t th);
#endif
c7_bool_t c7_thread_join(c7_thread_t th, int tmo_us);
c7_bool_t c7_thread_kill(c7_thread_t th, int sig);

c7_bool_t c7_thread_is_alive(c7_thread_t th);
uint64_t c7_thread_id(c7_thread_t th_op);
const char *c7_thread_name(c7_thread_t th_op);
void *c7_thread_arg(c7_thread_t th_op);
c7_thread_end_t c7_thread_endstatus(c7_thread_t th_op);
c7_thread_t c7_thread_self(void);

c7_bool_t c7_thread_free(c7_thread_t th);


/*----------------------------------------------------------------------------
                   counter - simple synchronization mechanism
----------------------------------------------------------------------------*/

typedef struct c7_thread_counter_t_ *c7_thread_counter_t;

c7_thread_counter_t c7_thread_counter_init(int ini_count);
int c7_thread_counter_value(c7_thread_counter_t ct);
c7_bool_t c7_thread_counter_is(c7_thread_counter_t ct, int count);
#define c7_thread_counter_up(c)		c7_thread_counter_move(c, 1)
#define c7_thread_counter_down(c)	c7_thread_counter_move(c, -1)
c7_bool_t c7_thread_counter_down_if(c7_thread_counter_t ct, int tmo_us);
void c7_thread_counter_move(c7_thread_counter_t ct, int delta);
void c7_thread_counter_set(c7_thread_counter_t ct, int count);
c7_bool_t c7_thread_counter_wait(c7_thread_counter_t ct, int expect, int tmo_us);
void c7_thread_counter_free(c7_thread_counter_t ct);


/*----------------------------------------------------------------------------
                   event - simple synchronization mechanism
----------------------------------------------------------------------------*/

typedef c7_thread_counter_t c7_thread_event_t;

#define c7_thread_event_init()		c7_thread_counter_init(0)
#define c7_thread_event_lock(e)		c7_thread_counter_lock(e)
#define c7_thread_event_unlock(e)	c7_thread_counter_unlock(e)
#define c7_thread_event_is_set(e)	c7_thread_counter_is(e, 1)
#define c7_thread_event_set(e)		c7_thread_counter_set(e, 1)
#define c7_thread_event_clear(e)	c7_thread_counter_set(e, 0)
#define c7_thread_event_wait(e, t)	c7_thread_counter_wait(e, 1, t)
#define c7_thread_event_waitclear(e, t)	c7_thread_counter_down_if(e, t)
#define c7_thread_event_free(e)		c7_thread_counter_free(e)


/*----------------------------------------------------------------------------
                   mask - bitwise synchronization mechanism
----------------------------------------------------------------------------*/

typedef struct c7_thread_mask_t_ *c7_thread_mask_t;

c7_thread_mask_t c7_thread_mask_init(uint64_t ini_mask);
uint64_t c7_thread_mask_value(c7_thread_mask_t m);
#define c7_thread_mask_clear(m)		c7_thread_mask_change(m, 0, -1UL);
#define c7_thread_mask_on(m,s)		c7_thread_mask_change(m, s, 0)
#define c7_thread_mask_off(m,c)		c7_thread_mask_change(m, 0, c)
void c7_thread_mask_change(c7_thread_mask_t m, uint64_t set, uint64_t clear);
uint64_t c7_thread_mask_wait(c7_thread_mask_t m, uint64_t expect, uint64_t clear, int tmo_us);
void c7_thread_mask_free(c7_thread_mask_t m);


/*----------------------------------------------------------------------------
                              randezvous threads
----------------------------------------------------------------------------*/

typedef struct c7_thread_randezvous_t_ *c7_thread_randezvous_t;
c7_thread_randezvous_t c7_thread_randezvous_init(int n_entry);
c7_bool_t c7_thread_randezvous_wait(c7_thread_randezvous_t rndv, int tmo_us);
void c7_thread_randezvous_abort(c7_thread_randezvous_t rndv);
void c7_thread_randezvous_reset(c7_thread_randezvous_t rndv);
void c7_thread_randezvous_free(c7_thread_randezvous_t rndv);


/*----------------------------------------------------------------------------
               inter-thread pipe (fixed size array of pointer)
----------------------------------------------------------------------------*/

typedef struct c7_thread_fpipe_t_ *c7_thread_fpipe_t;

c7_thread_fpipe_t c7_thread_fpipe_init(int ent_count);
c7_bool_t c7_thread_fpipe_resize(c7_thread_fpipe_t pipe, int ent_count);
void c7_thread_fpipe_reset(c7_thread_fpipe_t pipe);
void c7_thread_fpipe_reset_and_put(c7_thread_fpipe_t pipe, void *data);
c7_bool_t c7_thread_fpipe_put(c7_thread_fpipe_t pipe, void *data, int tmo_us);
void *c7_thread_fpipe_get(c7_thread_fpipe_t pipe, int tmo_us);
void c7_thread_fpipe_free(c7_thread_fpipe_t pipe);


/*----------------------------------------------------------------------------
                       inter-thread pipe (linked list)
----------------------------------------------------------------------------*/

typedef struct c7_thread_vpipe_t_ *c7_thread_vpipe_t;

typedef struct c7_thread_vpipe_link_t_ {
    struct c7_thread_vpipe_link_t_ *next;
} c7_thread_vpipe_link_t;

c7_thread_vpipe_t c7_thread_vpipe_init(ptrdiff_t linkoff);
void *c7_thread_vpipe_reset(c7_thread_vpipe_t vpipe);
void *c7_thread_vpipe_reset_and_put(c7_thread_vpipe_t vpipe, void *data);
c7_bool_t c7_thread_vpipe_put(c7_thread_vpipe_t vpipe, void *data);
void *c7_thread_vpipe_get(c7_thread_vpipe_t vpipe, int tmo_us);
void c7_thread_vpipe_free(c7_thread_vpipe_t vpipe);


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/


#if defined(__cplusplus)
}
#endif
#endif /* c7thread.h */
