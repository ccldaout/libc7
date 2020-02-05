/*
 * c7coroutine.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <c7coroutine.h>
#include <c7status.h>
#include <c7string.h>
#include <c7thread.h>
#include <c7app.h>
#if C7_CONFIG_UCONTEXT
# include <ucontext.h>
# include <signal.h>	// MINSIGSTKSZ
#else
# include <signal.h>
# include <setjmp.h>
# include <unistd.h>
#endif


const char __C7_COROUTINE_YIELD_SPECIAL[3];


//----------------------------------------------------------------------------
//                                coroutine
//----------------------------------------------------------------------------

struct c7_coroutine_t_ {
    c7_mgroup_t mg;
    void (*coroutine)(c7_mgroup_t, void *);
    c7_coroutine_t yield_from;
    void *yield_data;
#if C7_CONFIG_UCONTEXT
    ucontext_t context;
#else
    jmp_buf jmpbuf;
    size_t stksize_kb;
#endif
};

static c7_thread_local struct c7_coroutine_t_ MainRoutine;
static c7_thread_local c7_coroutine_t Self;

static c7_bool_t init_thread(void)
{
    if (Self == NULL) {
	Self = &MainRoutine;
	Self->yield_from = Self;
    }
    return C7_TRUE;
}

c7_coroutine_t c7_coroutine_self(void)
{
    return Self;
}

#if C7_CONFIG_UCONTEXT

# define _STACK_MIN_b	MINSIGSTKSZ

static void enter_coroutine(void)
{
    Self->coroutine(Self->mg, Self->yield_data);
    c7_coroutine_exit();
}

#else // ! C7_CONFIG_UCONTEXT

static pthread_mutex_t __mutex = PTHREAD_MUTEX_INITIALIZER;

# define _STACK_MIN_b	MINSIGSTKSZ
# define _LOCK()	c7_thread_lock(&__mutex)
# define _UNLOCK()	c7_thread_unlock(&__mutex)

static void enter_coroutine(int __dummy)
{
    if (setjmp(Self->jmpbuf) == 0) {
	longjmp(MainRoutine.jmpbuf, 1);
    }
    Self->coroutine(Self->mg, Self->yield_data);
    c7_coroutine_exit();
}

static c7_bool_t setup_coroutine(c7_coroutine_t co, void *stack, size_t stacksize_b)
{
    const int SIG = SIGILL;

    sigset_t sigmask, o_sigmask;
    (void)sigemptyset(&sigmask);
    (void)sigaddset(&sigmask, SIG);
    (void)pthread_sigmask(SIG_UNBLOCK, &sigmask, &o_sigmask);

    stack_t o_stack;
    if (sigaltstack(NULL, &o_stack) == C7_SYSERR) {
	c7_status_add(errno, ": sigaltstack(#1) failed.\n");
	return C7_FALSE;
    }
    struct sigaction o_sa;
    if (sigaction(SIG, NULL, &o_sa) == C7_SYSERR) {
	c7_status_add(errno, ": sigaction(#1) failed.\n");
	return C7_FALSE;
    }
    c7_coroutine_t o_co = Self;
    Self = co;

    c7_bool_t ret = C7_FALSE;

    if (setjmp(MainRoutine.jmpbuf) == 0) {
	stack_t n_stack = { .ss_sp=stack, .ss_size=stacksize_b };
	if (sigaltstack(&n_stack, NULL) == C7_SYSOK) {
	    struct sigaction sa;
	    (void)memset(&sa, 0, sizeof(sa));
	    sa.sa_handler = enter_coroutine;
	    sa.sa_flags = SA_ONSTACK;
	    if (sigaction(SIG, &sa, NULL) == C7_SYSOK) {
		(void)kill(getpid(), SIG);
		c7echo_err(0, ": [FATAL] setup_coroutine should reach here.\n");
	    } else
		c7_status_add(errno, ": sigaction(#2) failed.\n");
	} else
	    c7_status_add(errno, ": sigaltstack(#2) failed.\n");
    } else
	ret = C7_TRUE;

    Self = o_co;

    (void)sigaction(SIG, &o_sa, NULL);
    (void)sigaltstack(&o_stack, NULL);
    (void)pthread_sigmask(SIG_SETMASK, &o_sigmask, NULL);

    return ret;
}

#endif // C7_CONFIG_UCONTEXT

c7_coroutine_t c7_coroutine_new(size_t stacksize_kb,
				void (*coroutine)(c7_mgroup_t, void *start_prm))
{
    c7_mgroup_t mg = c7_mg_new();
    if (mg == NULL)
	return NULL;

    c7_coroutine_t co = c7_mg_malloc(mg, sizeof(*co));
    if (co != NULL) {
	co->mg = mg;
	co->coroutine = coroutine;
	co->yield_from = NULL;
	co->yield_data = NULL;
	size_t stacksize_b = _STACK_MIN_b + stacksize_kb * 1024;
	void *stack = c7_mg_malloc(co->mg, stacksize_b);
	if (stack != NULL) {
#if C7_CONFIG_UCONTEXT
	    if (getcontext(&co->context) == C7_SYSOK) {
		co->context.uc_stack.ss_sp = stack;
		co->context.uc_stack.ss_size = stacksize_b;
		co->context.uc_stack.ss_flags = 0;
		co->context.uc_link = &co->context;
		makecontext(&co->context, (void (*)())enter_coroutine, 0);
		return co;
	    }
	    c7_status_add(errno, ": getcontext failure\n");
#else // ! C7_CONFIG_UCONTEXT
	    _LOCK();
	    c7_bool_t ret = setup_coroutine(co, stack, stacksize_b);
	    _UNLOCK();
	    if (ret)
		return co;
#endif // C7_CONFIG_UCONTEXT
	}
    }

    c7_mg_destroy(co->mg);
    return NULL;
}

void *c7_coroutine_start(c7_coroutine_t co_target, void *start_prm)
{
    if (co_target == NULL) {
	c7_status_add(EINVAL, ": Invalid co_target\n");
	return __C7_COROUTINE_YIELD_FAIL;
    }
    if (co_target->yield_from != NULL) {
	c7_status_add(EINVAL, ": Specified co_target is already started\n");
	return __C7_COROUTINE_YIELD_FAIL;
    }
    co_target->yield_from = Self;
    return c7_coroutine_yield(co_target, start_prm);
}

void *c7_coroutine_yield(c7_coroutine_t co_target, void *yield_data)
{
    c7_coroutine_t co_self = Self;
    if (co_target == NULL)
	co_target = co_self->yield_from;
    if (co_target->yield_from == NULL) {
	c7_status_add(EINVAL, ": Specified co_target is not started\n");
	return __C7_COROUTINE_YIELD_FAIL;
    }
    co_target->yield_from = co_self;
    co_target->yield_data = yield_data;
    Self = co_target;
#if C7_CONFIG_UCONTEXT
    if (swapcontext(&co_self->context, &co_target->context) == C7_SYSERR) {
	c7_status_add(errno, ": swapcontext failure\n");
	Self = co_self;
	return __C7_COROUTINE_YIELD_FAIL;
    }
#else // ! C7_CONFIG_UCONTEXT
    if (setjmp(co_self->jmpbuf) == 0) {
	longjmp(co_target->jmpbuf, 1);
    }
#endif // C7_CONFIG_UCONTEXT
    Self = co_self;
    return co_self->yield_data;
}

void c7_coroutine_exit(void)
{
    for (;;)
	(void)c7_coroutine_yield(NULL, __C7_COROUTINE_YIELD_EXIT);
}

void c7_coroutine_abort(void)
{
    for (;;)
	(void)c7_coroutine_yield(NULL, __C7_COROUTINE_YIELD_ABORT);
}

void c7_coroutine_free(c7_coroutine_t co)
{
    c7_mg_destroy(co->mg);
}


//----------------------------------------------------------------------------
//                 implementation for c7_coroutine_foreach
//----------------------------------------------------------------------------

c7_coroutine_t __c7_coroutine_for_init(size_t stacksize_kb,
				       void (*generator)(c7_mgroup_t, void *param),
				       void *param,
				       void **vpp)
{
    c7_coroutine_t co;
    if ((co = c7_coroutine_new(stacksize_kb, generator)) != NULL) {
	*vpp = c7_coroutine_start(co, param);
	if (c7_coroutine_is_valid(*vpp)) {
	    return co;
	}
	c7_coroutine_free(co);
    } else
	*vpp = __C7_COROUTINE_YIELD_FAIL;
    return NULL;
}

void *__c7_coroutine_for_next(c7_coroutine_t *cop)
{
    if (*cop == NULL)
	return __C7_COROUTINE_YIELD_EXIT;	// maybe stopped
    void *v = c7_coroutine_yield(*cop, NULL);
    if (!c7_coroutine_is_valid(v)) {
	c7_coroutine_free(*cop);
	*cop = NULL;
    }
    return v;
}

void *__c7_coroutine_for_stop(c7_coroutine_t *cop)
{
    c7_coroutine_free(*cop);
    *cop = NULL;
    return __C7_COROUTINE_YIELD_EXIT;
}


//----------------------------------------------------------------------------
//                                generator
//----------------------------------------------------------------------------

struct __c7_generator_prm_t_ {
    void *start_param;
    void (*finalize)(c7_mgroup_t, void *param);
};

c7_bool_t c7_generator_init(c7_generator_t *gen,
			    size_t stacksize_kb,
			    void (*generator)(c7_mgroup_t, void *param),
			    void (*finalize)(c7_mgroup_t, void *param),
			    void *param)
{
    if ((gen->__co = c7_coroutine_new(stacksize_kb, generator)) != NULL) {
	gen->__param = c7_mg_malloc(gen->__co->mg, sizeof(*gen->__param));
	if (gen->__param != NULL) {
	    gen->__param->start_param = param;
	    gen->__param->finalize = finalize;
	    gen->__end_value = (void *)gen;
	    return C7_TRUE;
	}
	c7_coroutine_free(gen->__co);
    }
    gen->__end_value = __C7_COROUTINE_YIELD_FAIL;
    return C7_FALSE;
}

static void finalize_generator(c7_generator_t *gen)
{
    if (gen->__param->finalize != NULL) {
	gen->__param->finalize(gen->__co->mg, gen->__param->start_param);
    }
    c7_coroutine_free(gen->__co);
    gen->__co = NULL;
}

c7_bool_t __c7_generator_next(c7_generator_t *gen, void **vpp)
{
    if (gen->__co != NULL) {
	if (gen->__end_value == NULL)
	    *vpp = c7_coroutine_yield(gen->__co, NULL);
	else {	// gen->__end_value == (void *)gen
	    gen->__end_value = NULL;
	    *vpp = c7_coroutine_start(gen->__co, gen->__param->start_param);
	}
	if (c7_coroutine_is_valid(*vpp)) {
	    return C7_TRUE;
	}
	finalize_generator(gen);
	gen->__end_value = *vpp;
    } else {
	*vpp = gen->__end_value;
    }
    return C7_FALSE;
}

void c7_generator_stop(c7_generator_t *gen)
{
    if (gen->__co != NULL) {
	finalize_generator(gen);
	gen->__end_value = __C7_COROUTINE_YIELD_EXIT;
    }
}


/*----------------------------------------------------------------------------
                 library initializer / per-thread initializer
----------------------------------------------------------------------------*/

void __c7_coroutine_init(void)
{
    static c7_thread_iniend_t iniend = {
	.init   = init_thread,
    };
    (void)init_thread();
    c7_thread_register_iniend(&iniend);
}
