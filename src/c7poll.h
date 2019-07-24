/*
 * c7poll.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_POLL_H_LOADED__
#define __C7_POLL_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <signal.h>
#include <c7timer.h>


#if !defined(__linux) && !defined(C7_CONFIG_NO_LINUX_EPOLL)
# define C7_CONFIG_NO_LINUX_EPOLL 1
#endif

#if !defined(C7_CONFIG_NO_LINUX_EPOLL)
# include <sys/epoll.h>
# define C7_POLL_EVENT_RD	EPOLLIN
# define C7_POLL_EVENT_WR	EPOLLOUT
# define C7_POLL_EVENT_HUP	EPOLLHUP
# define C7_POLL_EVENT_ERR	EPOLLERR
# define C7_POLL_EVENT_INV	EPOLLERR
#else
# include <poll.h>
# define C7_POLL_EVENT_RD	POLLIN
# define C7_POLL_EVENT_WR	POLLOUT
# define C7_POLL_EVENT_HUP	POLLHUP
# define C7_POLL_EVENT_ERR	POLLERR
# define C7_POLL_EVENT_INV	POLLNVAL
#endif
#define C7_POLL_EVENT_ERRORS	(C7_POLL_EVENT_HUP|C7_POLL_EVENT_ERR|C7_POLL_EVENT_INV)

typedef struct c7_poll_t_ *c7_poll_t;

c7_poll_t c7_poll_init(void);

c7_bool_t c7_poll_start(c7_poll_t poller);

c7_bool_t c7_poll_start_thread(c7_poll_t poller, int stacksize_kb);

c7_bool_t c7_poll_stop(c7_poll_t poller);

c7_bool_t c7_poll_register(c7_poll_t poller,
			   int desc,
			   uint32_t evmask,		/* RD|WR */
			   void (*on_event)(c7_poll_t poller,
					    int desc,
					    int evmask,
					    void *__arg),
			   void *__arg);

c7_bool_t c7_poll_modify(c7_poll_t pl,
			 int desc,
			 uint32_t evmask);

c7_bool_t c7_poll_unregister(c7_poll_t poller,
			     int desc);

c7_bool_t c7_poll_pause(c7_poll_t poller,
			int desc);

c7_bool_t c7_poll_resume(c7_poll_t poller,
			 int desc);

c7_alarm_t c7_poll_alarm_on(c7_poll_t poller,
			    int delay_ms,
			    void (*on_alarm)(c7_poll_t poller,
					     void *__arg),
			    void *__arg);

void c7_poll_alarm_off(c7_poll_t pl,
		       c7_alarm_t alarm);

void c7_poll_free(c7_poll_t poller);


#if defined(__cplusplus)
}
#endif
#endif /* c7poll.h */
