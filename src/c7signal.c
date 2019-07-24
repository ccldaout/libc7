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


#define _CNTL_SIG	SIGCHLD	// signal for restarting sgiwait

typedef struct _callback_t_ {
    void (*func)(int sig);
    sigset_t sigmask;
} _callback_t;

static struct {
    pthread_mutex_t lock;
    _callback_t callback[NSIG];
    c7_thread_t th;
    c7_thread_event_t reload;
    sigset_t block_sigs;	// signals to be blocked just before sigwait
    sigset_t wait_sigs;		// signals to be passed to sigwait
    sigset_t managed_sigs;	// all registered signals
} SigMonitor = { PTHREAD_MUTEX_INITIALIZER };

pthread_mutex_t c7_signal_glock = PTHREAD_MUTEX_INITIALIZER;

static void c7_signal_SIGCHLD_callback(int sig1)
{
    c7_cleaner_waitprocs();
}

static void signal_monitor_thread(void *__nouse)
{
    for (;;) {
	int sig1;
	(void)pthread_sigmask(SIG_SETMASK, &SigMonitor.block_sigs, NULL);
	c7_thread_event_set(SigMonitor.reload);
	if (sigwait(&SigMonitor.wait_sigs, &sig1) == C7_SYSOK) {
	    _callback_t *cb = &SigMonitor.callback[sig1 - 1];
	    if (cb->func == NULL)
		continue;
	    sigset_t o_sigmask;
	    (void)pthread_sigmask(SIG_BLOCK, &cb->sigmask, &o_sigmask);
	    C7_THREAD_GUARD_ENTER(&c7_signal_glock);
	    c7_status_clear();
	    cb->func(sig1);
	    c7_status_clear();
	    C7_THREAD_GUARD_EXIT(&c7_signal_glock);
	    (void)pthread_sigmask(SIG_SETMASK, &o_sigmask, NULL);
	}
    }
}


static void (*register_callback(int sig1,
				const sigset_t *sigmask_on_call,
				void (*callback)(int sig)))(int)
{
    if (sig1 < 1 || NSIG < sig1)
	c7abort_err(EINVAL, ": [FATAL] c7_signal_register: sig:%1d is over NSIG:%d\n",
		    sig1, NSIG);

    _callback_t *cb = &SigMonitor.callback[sig1 - 1];
    void (*o_callback)(int) = cb->func;
    cb->func = callback;
    if (sigmask_on_call != NULL)
	cb->sigmask = *sigmask_on_call;
    else
	(void)sigemptyset(&cb->sigmask);
    (void)sigaddset(&cb->sigmask, sig1);

    if (callback == SIG_IGN || callback == SIG_DFL) {
	(void)signal(sig1, callback);
	(void)sigdelset(&SigMonitor.block_sigs, sig1);
	(void)sigdelset(&SigMonitor.wait_sigs, sig1);
	(void)sigdelset(&SigMonitor.managed_sigs, sig1);
    } else {
	(void)sigaddset(&SigMonitor.block_sigs, sig1);
	(void)sigaddset(&SigMonitor.wait_sigs, sig1);
	(void)sigaddset(&SigMonitor.managed_sigs, sig1);
    }

    if (SigMonitor.th != NULL) {
	// dummy signal to reload SigMonitor.*_sigs
	(void)c7_thread_kill(SigMonitor.th, _CNTL_SIG);
	c7_thread_event_wait(SigMonitor.reload, -1);
    }
#if defined(__CYGWIN__)
    (void)sigprocmask(SIG_BLOCK, &SigMonitor.wait_sigs, NULL);
#endif

    return o_callback;
}

void (*c7_signal_register(int sig1,
			  const sigset_t *sigmask_on_call,
			  void (*callback)(int sig)))(int)
{
    void (*o_func)(int);
    C7_THREAD_GUARD_ENTER(&SigMonitor.lock);
    o_func = register_callback(sig1, sigmask_on_call, callback);
    C7_THREAD_GUARD_EXIT(&SigMonitor.lock);
    return o_func;
}


static void update_sigmask(int how, const sigset_t *sigs, sigset_t *o_sigs)
{
    if (o_sigs != NULL)
	*o_sigs = SigMonitor.block_sigs;

    if (how == SIG_BLOCK) {
	for (int sig0 = 0; sig0 < NSIG; sig0++) {
	    int sig1 = sig0 + 1;
	    if (sig1 == _CNTL_SIG)
		continue;
	    if (sigismember(sigs, sig1)) {
		sigaddset(&SigMonitor.block_sigs, sig1);
		sigdelset(&SigMonitor.wait_sigs, sig1);
	    }
	}
    } else if (how == SIG_UNBLOCK) {
	for (int sig0 = 0; sig0 < NSIG; sig0++) {
	    int sig1 = sig0 + 1;
	    if (sig1 == _CNTL_SIG)
		continue;
	    if (sigismember(sigs, sig1)) {
		if (sigismember(&SigMonitor.managed_sigs, sig1)) {
		    sigaddset(&SigMonitor.block_sigs, sig1);
		    sigaddset(&SigMonitor.wait_sigs, sig1);
		} else {
		    sigdelset(&SigMonitor.block_sigs, sig1);
		    //sigdelset(&SigMonitor.wait_sigs, sig1);
		}
	    }
	}
    } else if (how == SIG_SETMASK) {
	for (int sig0 = 0; sig0 < NSIG; sig0++) {
	    int sig1 = sig0 + 1;
	    if (sig1 == _CNTL_SIG)
		continue;
	    if (sigismember(sigs, sig1)) {
		sigaddset(&SigMonitor.block_sigs, sig1);
		if (sigismember(&SigMonitor.managed_sigs, sig1)) {
		    sigaddset(&SigMonitor.wait_sigs, sig1);
		} else {
		    //sigdelset(&SigMonitor.wait_sigs, sig1);
		}
	    } else {
		if (sigismember(&SigMonitor.managed_sigs, sig1)) {
		    sigaddset(&SigMonitor.block_sigs, sig1);
		    sigaddset(&SigMonitor.wait_sigs, sig1);
		} else {
		    sigdelset(&SigMonitor.block_sigs, sig1);
		    //sigaddset(&SigMonitor.wait_sigs, sig1);
		}
	    }
	}
    }

    if (SigMonitor.th != NULL) {
	// dummy signal to reload SigMonitor.*_sigs
	(void)c7_thread_kill(SigMonitor.th, _CNTL_SIG);
	c7_thread_event_wait(SigMonitor.reload, -1);
    }
}

void c7_signal_sigmask(int how, const sigset_t *sigs, sigset_t *o_sigs)
{
    C7_THREAD_GUARD_ENTER(&SigMonitor.lock);
    update_sigmask(how, sigs, o_sigs);
    C7_THREAD_GUARD_EXIT(&SigMonitor.lock);
}


sigset_t c7_signal_sigblock(void)
{
    sigset_t n_set, o_set;
    const int sigs[] = {
	SIGINT, SIGTERM, SIGHUP, SIGABRT, SIGQUIT, SIGTSTP, SIGUSR1, SIGUSR2, SIGWINCH
    };
    (void)sigemptyset(&n_set);
    for (int i = 0; i < c7_numberof(sigs); i++)
	(void)sigaddset(&n_set, sigs[i]);
    c7_signal_sigmask(SIG_BLOCK, &n_set, &o_set);
    return o_set;
}


void __c7_signal_init(void)
{
    c7_status_clear();

    // All signals must be blocked at main thread for sigwait work well.
#if !defined(__CYGWIN__)
    sigset_t main_sigs;
    (void)sigfillset(&main_sigs);
    (void)sigprocmask(SIG_SETMASK, &main_sigs, NULL);
#endif

    (void)sigemptyset(&SigMonitor.block_sigs);
    (void)sigemptyset(&SigMonitor.wait_sigs);
    (void)sigemptyset(&SigMonitor.managed_sigs);

    (void)sigaddset(&SigMonitor.block_sigs, _CNTL_SIG);
    (void)sigaddset(&SigMonitor.wait_sigs, _CNTL_SIG);

    c7_signal_register(SIGCHLD, NULL, c7_signal_SIGCHLD_callback);

    SigMonitor.reload = c7_thread_event_init();
    SigMonitor.th = c7_thread_run(signal_monitor_thread, NULL, NULL,
				  "signal_monitor", 0);
    if (SigMonitor.th == NULL)
	c7abort_err(0, ": [FATAL] cannot run signal monitor thread\n");
}
