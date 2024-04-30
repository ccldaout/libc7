/*
 * c7signal.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */


#include "_config.h"

#include <unistd.h>
#include <semaphore.h>
#include <c7proc.h>
#include <c7signal.h>
#include <c7status.h>
#include <c7thread.h>
#include "_private.h"


static struct sigman_t {
    pthread_mutex_t lock;
    sigset_t blocked_mask;
    sigset_t user_mask;
    void (*handlers[NSIG])(int);
    c7_thread_t th;
} sigm_ = { PTHREAD_MUTEX_INITIALIZER };
    

static void _sigm_default(int sig)
{
    sigset_t mask;
    (void)sigemptyset(&mask);
    (void)sigaddset(&mask, sig);
    (void)pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    (void)kill(getpid(), sig);
}

static void _sigm_sigset_em(sigset_t *mask)
{
    int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGBUS, SIGABRT };
    for (size_t i = 0; i < c7_numberof(sigs); i++) {
	sigdelset(mask, sigs[i]);
    }
}

static void _sigm_sigset_on(sigset_t *mask, const sigset_t *on)
{
    for (int i = 0; i < NSIG; i++) {
	if (sigismember(on, i)) {
	    sigaddset(mask, i);
	}
    }
}

static void _sigm_sigset_off(sigset_t *mask, const sigset_t *off)
{
    for (int i = 0; i < NSIG; i++) {
	if (sigismember(off, i)) {
	    sigdelset(mask, i);
	}
    }
}

static void _sigm_check_blocked(void)
{
    for (int i = 0; i < NSIG; i++) {
	if (sigismember(&sigm_.blocked_mask, i) && !sigismember(&sigm_.user_mask, i)) {
	    (void)kill(getpid(), i);
	    sigdelset(&sigm_.blocked_mask, i);
	}
    }
}

static void _sigm_monitor(void *__unuse)
{
    sigset_t wait_mask;
    (void)sigfillset(&wait_mask);
    _sigm_sigset_em(&wait_mask);

    for (;;) {
	int sig;
	if (sigwait(&wait_mask, &sig) == C7_SYSOK) {
	    void (*handler)(int) = NULL;
	    C7_THREAD_GUARD_ENTER(&sigm_.lock);
	    if (sigismember(&sigm_.user_mask, sig)) {
		sigaddset(&sigm_.blocked_mask, sig);
	    } else {
		handler = sigm_.handlers[sig];
	    }
	    C7_THREAD_GUARD_EXIT(&sigm_.lock);
	    if (handler != NULL) {
		handler(sig);
	    }
	} else {
	    //c7error(c7result_err(errno, "sigwait() failed"));
	}
    }
}

static void sigm_init(void)
{
    for (size_t i = 0; i < c7_numberof(sigm_.handlers); i++) {
	sigm_.handlers[i] = _sigm_default;
    }
    (void)sigemptyset(&sigm_.user_mask);
    (void)sigemptyset(&sigm_.blocked_mask);

    sigset_t wait_mask;
    (void)sigfillset(&wait_mask);
    _sigm_sigset_em(&wait_mask);
    (void)sigprocmask(SIG_SETMASK, &wait_mask, NULL);

    sigm_.th = c7_thread_run(_sigm_monitor, NULL, NULL, "signal_monitor", 0);
    if (sigm_.th == NULL) {
	c7abort_err(0, ": [FATAL] cannot run signal monitor thread\n");
    }
}

static void (*sigm_register(int sig, void (*new_handler)(int)))(int)
{
    void (*old)(int) = sigm_.handlers[sig];
    if (new_handler == SIG_IGN) {
	(void)signal(sig, new_handler);
    } else {
	if (new_handler == SIG_DFL) {
	    new_handler = _sigm_default;
	}
    }
    sigm_.handlers[sig] = new_handler;
    if (old == _sigm_default) {
	old = SIG_DFL;
    }
    return old;
}

static sigset_t sigm_block(const sigset_t *mask)
{
    sigset_t old_mask;
    C7_THREAD_GUARD_ENTER(&sigm_.lock);
    old_mask = sigm_.user_mask;
    _sigm_sigset_on(&sigm_.user_mask, mask);
    C7_THREAD_GUARD_EXIT(&sigm_.lock);
    return old_mask;
}

static sigset_t sigm_unblock(const sigset_t *mask)
{
    sigset_t old_mask;
    C7_THREAD_GUARD_ENTER(&sigm_.lock);
    old_mask = sigm_.user_mask;
    _sigm_sigset_off(&sigm_.user_mask, mask);
    _sigm_check_blocked();
    C7_THREAD_GUARD_EXIT(&sigm_.lock);
    return old_mask;
}

static sigset_t sigm_setmask(const sigset_t *mask)
{
    sigset_t old_mask;
    C7_THREAD_GUARD_ENTER(&sigm_.lock);
    old_mask = sigm_.user_mask;
    sigm_.user_mask = *mask;
    _sigm_check_blocked();
    C7_THREAD_GUARD_EXIT(&sigm_.lock);
    return old_mask;
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

static volatile int once_flag;

static void handle_SIGCHLD(int sig1)
{
    c7_cleaner_waitprocs();
}

static void reset_once_flag(void)
{
    __sync_lock_release(&once_flag);
}

static void call_init_once(void)
{
    if (__sync_lock_test_and_set(&once_flag, 1) == 0) {
	sigm_init();
	sigm_register(SIGCHLD, handle_SIGCHLD);
	pthread_atfork(NULL, NULL, reset_once_flag);
    }
}

void (*c7_signal_register(int sig1,
			  const sigset_t *sigmask_on_call__unused,
			  void (*callback)(int sig)))(int)
{
    call_init_once();
    return sigm_register(sig1, callback);
}

void c7_signal_sigmask(int how, const sigset_t *sigs, sigset_t *o_sigs)
{
    call_init_once();

    sigset_t o_default;
    if (o_sigs == NULL) {
	o_sigs = &o_default;
    }

    switch (how) {
    case SIG_SETMASK:
	*o_sigs = sigm_setmask(sigs);
	break;
    case SIG_UNBLOCK:
	*o_sigs = sigm_unblock(sigs);
	break;
    default:
	*o_sigs = sigm_block(sigs);
	break;
    }
}

sigset_t c7_signal_sigblock(void)
{
    call_init_once();

    const int sigs[] = {
	SIGINT, SIGTERM, SIGHUP, SIGABRT, SIGQUIT, SIGTSTP, SIGUSR1, SIGUSR2, SIGWINCH
    };
    sigset_t n_set;
    (void)sigemptyset(&n_set);
    for (int i = 0; i < c7_numberof(sigs); i++) {
	(void)sigaddset(&n_set, sigs[i]);
    }
    return sigm_block(&n_set);
}

void __c7_signal_init(void)
{
    // do nothing
}
