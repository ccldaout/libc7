/*
 * c7signal.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_SIGNAL_H_LOADED__
#define __C7_SIGNAL_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <signal.h>


extern pthread_mutex_t c7_signal_glock;

void (*c7_signal_register(int sig,
			  const sigset_t *sigmask_on_call,
			  void (*callback)(int sig)))(int);

void c7_signal_sigmask(int how, const sigset_t *sigs, sigset_t *o_sigs);

sigset_t c7_signal_sigblock(void);

#define c7_signal_sigrestore(sigs)	c7_signal_sigmask(SIG_SETMASK, (sigs), NULL)


#if defined(__cplusplus)
}
#endif
#endif /* c7signal.h */
