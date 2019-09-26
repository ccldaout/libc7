/*
 * c7poll.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <errno.h>
#include <c7deque.h>
#include <c7memory.h>
#include <c7mpool.h>
#include <c7parray.h>
#include <c7poll.h>
#include <c7status.h>
#include <c7thread.h>
#include <c7timer.h>
#include <c7app.h>
#include "_private.h"


/*----------------------------------------------------------------------------
                             common declaraetion
----------------------------------------------------------------------------*/

typedef struct _poller_t _poller_t;

typedef enum _poll_sts_t {
    _POLL_STS_FATAL,
    _POLL_STS_CNTL_ERROR,
    _POLL_STS_STOP,
    _POLL_STS_CONTINUE,
    _POLL_STS_RECALTMO,
    _POLL_STS_numof
} _poll_sts_t;

typedef enum _cntl_opc_t {
#if defined(C7_CONFIG_NO_LINUX_EPOLL)
    _CNTL_OPC_ADD_FD,
    _CNTL_OPC_MOD_FD,
    _CNTL_OPC_DEL_FD,
#endif
    _CNTL_OPC_CALLBACK,
    _CNTL_OPC_STOP,
    _CNTL_OPC_RECALTMO,
    _CNTL_OPC_numof
} _cntl_opc_t;

typedef struct _cntl_t {
    _cntl_opc_t opc;
    union {
	struct {
	    int fd;
	    uint32_t evmask;
	} poll;
	struct {
	    void (*func)(_poller_t *, void *__arg);
	    void *__arg;
	} callback;
    } u;
} _cntl_t;

static c7_bool_t poll_cntl_append(_poller_t *poller, const _cntl_t *cntl);
static _poll_sts_t _poll_cntl_action(_poller_t *poller);


#if !defined(C7_CONFIG_NO_LINUX_EPOLL)
/*----------------------------------------------------------------------------
                                 linux epoll
----------------------------------------------------------------------------*/

# include <sys/epoll.h>

#if defined(C7_CONFIG_EPOLL_EVENTS_SIZE)
# define _EPOLL_EVENTS_SIZE C7_CONFIG_EPOLL_EVENTS_SIZE
#else
# define _EPOLL_EVENTS_SIZE 64
#endif

struct _poller_t {
    pthread_mutex_t mutex;
    int epfd;
    struct epoll_event events[_EPOLL_EVENTS_SIZE];
    int nfds;
    c7_deque_t cntls;			/* array of _cntl_t */
    c7_deque_t cntls_copied;
    int cntl_pipe[2];
    struct {
	void (*callback)(_poller_t *poller,
			 int fd,
			 uint32_t evmask,
			 void *__arg);
	void *__arg;
    } event;
};

static c7_bool_t poll_setup(_poller_t *poller)
{
    if ((poller->epfd = epoll_create1(EPOLL_CLOEXEC)) == C7_SYSERR) {
	c7_status_add(errno, "poll_init: epoll_create1 error\n");
	return C7_FALSE;
    }

    (void)c7_thread_mutex_init(&poller->mutex, NULL);

    poller->cntls        = c7_deque_create(sizeof(_cntl_t), NULL);
    poller->cntls_copied = c7_deque_create(sizeof(_cntl_t), NULL);
    if (poller->cntls == NULL || poller->cntls_copied == NULL)
	return C7_FALSE;

    if (pipe(poller->cntl_pipe) == C7_SYSERR) {
	c7_status_add(errno, "poll_init: pipe error\n");
	return C7_FALSE;
    }

    struct epoll_event ev = { .events = C7_POLL_EVENT_RD, .data.fd = poller->cntl_pipe[0] };
    if (epoll_ctl(poller->epfd, EPOLL_CTL_ADD, poller->cntl_pipe[0], &ev) == C7_SYSERR) {
	c7_status_add(errno, "poll_init: epoll_ctl(CTL_ADD) error\n");
	return C7_FALSE;
    }

    return C7_TRUE;
}

static _poller_t *poll_init(void)
{
    _poller_t *poller = c7_malloc(sizeof(*poller));
    if (poller != NULL) {
	poller->epfd = C7_SYSERR;
	poller->cntl_pipe[0] = poller->cntl_pipe[1] = C7_SYSERR;
	if (poll_setup(poller)) {
	    return poller;
	}
	(void)close(poller->epfd);
	(void)close(poller->cntl_pipe[0]);
	(void)close(poller->cntl_pipe[1]);
	c7_deque_destroy(poller->cntls);
	c7_deque_destroy(poller->cntls_copied);
	free(poller);
    }
    return NULL;
}

static void poll_set_event_callback(_poller_t *poller,
				    void (*callback)(_poller_t *poller,
						     int fd,
						     uint32_t evmask,
						     void *__arg),
				    void *__arg)
{
    poller->event.callback = callback;
    poller->event.__arg = __arg;
}

static c7_bool_t poll_register(_poller_t *poller, int fd, uint32_t evmask)
{
    struct epoll_event ev;
    ev.events = evmask;
    ev.data.fd = fd;
    int ret = epoll_ctl(poller->epfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret == C7_SYSERR) {
	c7_status_add(errno, "poll_register: epoll_ctl(CTL_ADD) error\n");
	return C7_FALSE;
    }
    return C7_TRUE;
}

static c7_bool_t poll_modify(_poller_t *poller, int fd, uint32_t evmask)
{
    struct epoll_event ev;
    ev.events = evmask;
    ev.data.fd = fd;
    int ret = epoll_ctl(poller->epfd, EPOLL_CTL_MOD, fd, &ev);
    if (ret == C7_SYSERR) {
	c7_status_add(errno, "poll_register: epoll_ctl(CTL_MOD) error\n");
	return C7_FALSE;
    }
    return C7_TRUE;
}

static c7_bool_t poll_unregister(_poller_t *poller, int fd)
{
    int ret = epoll_ctl(poller->epfd, EPOLL_CTL_DEL, fd, NULL);
    if (ret == C7_SYSERR) {
	c7_status_add(errno, "poll_unregister: epoll_ctl(CTL_DEL) error\n");
	return C7_FALSE;
    }
    return C7_TRUE;
}

static int poll_wait(_poller_t *poller, int tmo_ms)
{
    int nfd;
    do {
	nfd = epoll_wait(poller->epfd, poller->events, _EPOLL_EVENTS_SIZE, tmo_ms);
    } while (nfd == C7_SYSERR && errno == EINTR);
    if (nfd == C7_SYSERR) {
	c7_status_add(errno, "poll_poll: epoll_pwait error\n");
	return C7_SYSERR;
    }
    return nfd;
}

static _poll_sts_t poll_action(_poller_t *poller, int nfd)
{
    for (int i = 0; i < nfd; i++) {
	struct epoll_event *evp = &poller->events[i];
	if (evp->data.fd == poller->cntl_pipe[0]) {
	    _poll_sts_t sts = _poll_cntl_action(poller);
	    if (sts == _POLL_STS_STOP || sts == _POLL_STS_FATAL ||
		sts == _POLL_STS_RECALTMO)
		return sts;
	    /* POLL_STS_CNTL_ERROR: ignored */
	} else {
	    uint32_t errmsk = (C7_POLL_EVENT_INV|C7_POLL_EVENT_HUP|C7_POLL_EVENT_ERR);
	    poller->event.callback(poller, evp->data.fd, evp->events, poller->event.__arg);
	    if ((evp->events & errmsk) != 0)
		(void)poll_unregister(poller, evp->data.fd);
	}
    }
    return _POLL_STS_CONTINUE;
}

static c7_bool_t poll_free(_poller_t *poller)
{
    (void)close(poller->epfd);
    (void)pthread_mutex_destroy(&poller->mutex);
    c7_deque_destroy(poller->cntls);
    c7_deque_destroy(poller->cntls_copied);
    (void)close(poller->cntl_pipe[0]);
    (void)close(poller->cntl_pipe[1]);
    (void)memset(poller, 0, sizeof(*poller));
    free(poller);
    return C7_TRUE;
}


#else
/*----------------------------------------------------------------------------
                               *nix/cygwin poll
----------------------------------------------------------------------------*/

# include <poll.h>

struct _poller_t {
    pthread_mutex_t mutex;
    c7_deque_t fds;			/* array of struct pollfd */
    c7_deque_t fds_copied;
    c7_deque_t cntls;			/* array of _cntl_t */
    c7_deque_t cntls_copied;
    int cntl_pipe[2];
    struct {
	void (*callback)(_poller_t *poller,
			 int fd,
			 uint32_t evmask,
			 void *__arg);
	void *__arg;
    } event;
};

static c7_bool_t poll_setup(_poller_t *poller)
{
    (void)pthread_mutex_init(&poller->mutex, NULL);

    poller->fds        = c7_deque_create(sizeof(struct pollfd), NULL);
    poller->fds_copied = c7_deque_create(sizeof(struct pollfd), NULL);
    poller->cntls        = c7_deque_create(sizeof(_cntl_t), NULL);
    poller->cntls_copied = c7_deque_create(sizeof(_cntl_t), NULL);
    if (poller->fds == NULL || poller->fds_copied == NULL ||
	poller->cntls == NULL || poller->cntls_copied == NULL)
	return C7_FALSE;

    if (pipe(poller->cntl_pipe) == C7_SYSERR) {
	c7_status_add(errno, "poll_init: pipe error\n");
	return C7_FALSE;
    }

    struct pollfd pollfd = { .fd = poller->cntl_pipe[0], .events = C7_POLL_EVENT_RD };
    if (c7_deque_append(poller->fds, &pollfd, 1) == NULL)
	return C7_FALSE;

    return C7_TRUE;
}

static _poller_t *poll_init(void)
{
    _poller_t *poller = c7_malloc(sizeof(*poller));
    if (poller != NULL) {
	poller->cntl_pipe[0] = poller->cntl_pipe[1] = C7_SYSERR;
	if (poll_setup(poller)) {
	    return poller;
	}
	(void)close(poller->cntl_pipe[0]);
	(void)close(poller->cntl_pipe[1]);
	c7_deque_destroy(poller->fds);
	c7_deque_destroy(poller->fds_copied);
	c7_deque_destroy(poller->cntls);
	c7_deque_destroy(poller->cntls_copied);
	free(poller);
    }
    return NULL;
}

static void poll_set_event_callback(_poller_t *poller,
				    void (*callback)(_poller_t *poller,
						     int fd,
						     uint32_t evmask,
						     void *__arg),
				    void *__arg)
{
    poller->event.callback = callback;
    poller->event.__arg = __arg;
}

static int _poll_find(_poller_t *poller, int fd)
{
    struct pollfd *p;
    c7_deque_foreach(poller->fds, p) {
	if (p->fd == fd)
	    return c7_deque_index(poller->fds, p);
    }
    return -1;
}

static c7_bool_t _poll_register_direct(_poller_t *poller, int fd, uint32_t evmask)
{
    struct pollfd pollfd;
    if (_poll_find(poller, fd) != -1) {
	c7_status_add(errno = EEXIST, "_poll_register: duplicate fd: %d\n", fd);
	return C7_FALSE;
    }
    pollfd.fd = fd;
    pollfd.events = evmask;
    return (c7_deque_append(poller->fds, &pollfd, 1) != NULL);
}

static c7_bool_t poll_register(_poller_t *poller, int fd, uint32_t evmask)
{
    _cntl_t cntl;
    cntl.opc = _CNTL_OPC_ADD_FD;
    cntl.u.poll.fd = fd;
    cntl.u.poll.evmask = evmask;
    return poll_cntl_append(poller, &cntl);
}

static c7_bool_t _poll_modify_direct(_poller_t *poller, int fd, uint32_t evmask)
{
    struct pollfd *pollfdp;
    int index;
    if ((index = _poll_find(poller, fd)) == -1) {
	c7_status_add(errno = ENOENT, "_poll_modify: unknown fd: %d\n", fd);
	return C7_FALSE;
    }
    pollfdp = c7_deque_nth(poller->fds, index);
    pollfdp->events = evmask;
    return C7_TRUE;
}

static c7_bool_t poll_modify(_poller_t *poller, int fd, uint32_t evmask)
{
    _cntl_t cntl;
    cntl.opc = _CNTL_OPC_MOD_FD;
    cntl.u.poll.fd = fd;
    cntl.u.poll.evmask = evmask;
    return poll_cntl_append(poller, &cntl);
}

static c7_bool_t _poll_unregister_direct(_poller_t *poller, int fd)
{
    int index;
    if ((index = _poll_find(poller, fd)) == -1) {
	c7_status_add(errno = ENOENT, "_poll_unregister: unknown fd: %d\n", fd);
	return C7_FALSE;
    }
    return c7_deque_remove(poller->fds, index, 1);
}

static c7_bool_t poll_unregister(_poller_t *poller, int fd)
{
    _cntl_t cntl;
    cntl.opc = _CNTL_OPC_DEL_FD;
    cntl.u.poll.fd = fd;
    return poll_cntl_append(poller, &cntl);
}

static int poll_wait(_poller_t *poller, int tmo_ms)
{
    int nfd;
    do {
	nfd = poll(c7_deque_buffer(poller->fds),
		   c7_deque_count(poller->fds), tmo_ms);
    } while (nfd == C7_SYSERR && errno == EINTR);
    if (nfd == C7_SYSERR) {
	c7_status_add(errno, "poll_poll: poll error\n");
	return C7_SYSERR;
    }
    return nfd;
}

static _poll_sts_t poll_action(_poller_t *poller, int nfd)
{
    struct pollfd *fdp;
    c7_deque_reset(poller->fds_copied);
    if (!c7_deque_extend(poller->fds_copied, poller->fds))
	return _POLL_STS_FATAL;
    c7_deque_foreach(poller->fds_copied, fdp) {
	if (fdp->revents == 0)
	    continue;
	if (fdp->fd == poller->cntl_pipe[0]) {
	    _poll_sts_t sts = _poll_cntl_action(poller);
	    if (sts == _POLL_STS_STOP || sts == _POLL_STS_FATAL ||
		sts == _POLL_STS_RECALTMO)
		return sts;
	    /* POLL_STS_CNTL_ERROR: ignored */
	} else {
	    uint32_t errmsk = (C7_POLL_EVENT_INV|C7_POLL_EVENT_HUP|C7_POLL_EVENT_ERR);
	    poller->event.callback(poller, fdp->fd, fdp->revents, poller->event.__arg);
	    if ((fdp->revents & errmsk) != 0)
		(void)_poll_unregister_direct(poller, fdp->fd);
	}
    }
    return _POLL_STS_CONTINUE;
}

static c7_bool_t poll_free(_poller_t *poller)
{
    (void)pthread_mutex_destroy(&poller->mutex);
    c7_deque_destroy(poller->fds);
    c7_deque_destroy(poller->fds_copied);
    c7_deque_destroy(poller->cntls);
    c7_deque_destroy(poller->cntls_copied);
    (void)close(poller->cntl_pipe[0]);
    (void)close(poller->cntl_pipe[1]);
    (void)memset(poller, 0, sizeof(*poller));
    free(poller);
    return C7_TRUE;
}

#endif


/*----------------------------------------------------------------------------
                                 poll control
----------------------------------------------------------------------------*/

static c7_bool_t poll_cntl_append(_poller_t *poller, const _cntl_t *cntl)
{
    c7_thread_lock(&poller->mutex);
    if (c7_deque_append(poller->cntls, (void *)cntl, 1) == NULL) {
	c7_thread_unlock(&poller->mutex);
	return C7_FALSE;
    }
    if (c7_deque_count(poller->cntls) == 1) {
	char ch = 0;
	ssize_t w;
	C7_THREAD_UNLOCK_PUSH(&poller->mutex);
	w = write(poller->cntl_pipe[1], &ch, 1);
	C7_THREAD_UNLOCK_POP();
	if (w != 1) {
	    c7_status_add(errno, "poll_cntl_append: write error\n");
	    c7_thread_unlock(&poller->mutex);
	    return C7_FALSE;
	}
    }
    c7_thread_unlock(&poller->mutex);
    return C7_TRUE;
}

#define _hook_error(api, err, ...)					\
    __c7_hook_poll_error(__FILE__, __LINE__, C7_API_poll_##api, (err), __VA_ARGS__)

static _poll_sts_t _poll_cntl_action(_poller_t *poller)
{
    char ch;
    ssize_t r;
    c7_thread_lock(&poller->mutex);
    C7_THREAD_UNLOCK_PUSH(&poller->mutex);
    r = read(poller->cntl_pipe[0], &ch, 1);
    C7_THREAD_UNLOCK_POP();
    if (r != 1) {
	c7_status_add(errno = EIO, "_poll_cntl_action: read error\n");
	c7_thread_unlock(&poller->mutex);
	return _POLL_STS_FATAL;
    }
    c7_deque_reset(poller->cntls_copied);
    if (!c7_deque_extend(poller->cntls_copied, poller->cntls)) {
	c7_thread_unlock(&poller->mutex);
	return _POLL_STS_FATAL;
    }
    c7_deque_reset(poller->cntls);
    c7_thread_unlock(&poller->mutex);

    _cntl_t *cp;
    c7_bool_t opc_stop = C7_FALSE;
    c7_bool_t opc_recal = C7_FALSE;

    c7_deque_foreach(poller->cntls_copied, cp) {
	switch (cp->opc) {
#if defined(C7_CONFIG_NO_LINUX_EPOLL)
	  case _CNTL_OPC_ADD_FD:
	    if (!_poll_register_direct(poller, cp->u.poll.fd, cp->u.poll.evmask))
		_hook_error(register_direct, errno,
			    ": (internal) register failure: fd: %d\n", cp->u.poll.fd);
	    break;

	  case _CNTL_OPC_MOD_FD:
	    if (!_poll_modify_direct(poller, cp->u.poll.fd, cp->u.poll.evmask))
		_hook_error(modify_direct, errno,
			    ": (internal) modify failure: fd: %d\n", cp->u.poll.fd);
	    break;

	  case _CNTL_OPC_DEL_FD:
	    if (!_poll_unregister_direct(poller, cp->u.poll.fd))
		_hook_error(unregister_direct, errno,
			    ": (internal) unregister failure: fd: %d\n", cp->u.poll.fd);
	    break;
#endif

	  case _CNTL_OPC_CALLBACK:
	    cp->u.callback.func(poller, cp->u.callback.__arg);
	    break;

	  case _CNTL_OPC_STOP:
	    opc_stop = C7_TRUE;
	    break;

	  case _CNTL_OPC_RECALTMO:
	    opc_recal = C7_TRUE;
	    break;

	  default:
	    _hook_error(cntl_action, EINVAL,
			": (internal) invalid opc: %d\n", cp->opc);
	    break;
	}
    }

    return (opc_stop ? _POLL_STS_STOP :
	    opc_recal ? _POLL_STS_RECALTMO : _POLL_STS_CONTINUE);
}


/*----------------------------------------------------------------------------
                           C7 POLL public interface
----------------------------------------------------------------------------*/

typedef struct _alarm_arg_t {
    void (*on_alarm)(c7_poll_t pl, void *__arg);
    c7_poll_t poller;
    void *__arg;
} _alarm_arg_t;

typedef struct _fd_attr_t {
    uint32_t evmask;
    uint32_t evmask_saved;
    void (*on_event)(c7_poll_t poller,
		     int desc,
		     int evmask,
		     void *__arg);
    void *__arg;
} _fd_attr_t;

struct c7_poll_t_ {
    pthread_mutex_t glock;
    _poller_t *poller;
    c7_timer_t timer;
    c7_parray_t fdv;		// array of pointer to _fd_attr_t
    c7_mpool_t alarm_arg_pool;
    //sigset_t sigmask;
};

static void _c7_poll_callback(_poller_t *poller,
			      int fd,
			      uint32_t evmask,
			      void *__arg)
{
    c7_poll_t pl = __arg;
    _fd_attr_t *fda;
    void (*on_event)(c7_poll_t poller, int desc, int evmask, void *__arg) = NULL;

    c7_thread_lock(&pl->glock);
    if ((fda = c7_parray_get(pl->fdv, fd)) != NULL) {
	if (fda->on_event) {
	    if ((evmask & (fda->evmask|C7_POLL_EVENT_ERRORS)) != 0) {
		on_event = fda->on_event;
		__arg = fda->__arg;
	    }
	}
    }
    c7_thread_unlock(&pl->glock);

    if (on_event) {
	on_event(pl, fd, evmask, __arg);
    }
}

static _poll_sts_t _c7_poll_loop(void *__arg)
{
    c7_poll_t pl = __arg;

    for (;;) {
	c7_status_clear();

	int tmo_ms = c7_timer_get_delay_ms(pl->timer);
	int nfd = poll_wait(pl->poller, tmo_ms);
	if (nfd == C7_SYSERR)
	    return _POLL_STS_FATAL;

	_poll_sts_t sts = _POLL_STS_CONTINUE;

	// In this signal handling implementaion by using sigwait, it
	// is a heavy to change signal mask and it is not usually needed
	// because all system calls are not interrupeted in any thread
	// except signal handling thread which call sigwait.
	////sigset_t o_sigmask;
	////c7_signal_sigmask(SIG_BLOCK, &pl->sigmask, &o_sigmask);
	if (nfd > 0)
	    sts = poll_action(pl->poller, nfd);
	else // nfd == 0
	    c7_timer_call(pl->timer);
	////c7_signal_sigrestore(&o_sigmask);

	if (sts == _POLL_STS_FATAL || sts == _POLL_STS_STOP)
	    return sts;
    }
}

c7_poll_t c7_poll_init(void)
{
    c7_poll_t pl = c7_calloc(sizeof(*pl), 1);
    if (pl != NULL) {
	pl->alarm_arg_pool = c7_mpool_init(sizeof(_alarm_arg_t), 8, NULL, NULL, NULL);
	if (pl->alarm_arg_pool != NULL) {
	    if ((pl->poller = poll_init()) != NULL) {
		if ((pl->timer = c7_timer_init()) != NULL) {
		    pl->fdv = c7_parray_create(sizeof(_fd_attr_t), NULL, NULL);
		    if (pl->fdv != NULL) {
			(void)pthread_mutex_init(&pl->glock, NULL);
			poll_set_event_callback(pl->poller, _c7_poll_callback, pl);
			return pl;
		    }
		    c7_parray_destroy(pl->fdv);
		}
		poll_free(pl->poller);
	    }
	    c7_mpool_free(pl->alarm_arg_pool);
	}
	free(pl);
    }
    return NULL;
}

c7_bool_t c7_poll_start(c7_poll_t poller)
{
    return (_c7_poll_loop(poller) != _POLL_STS_FATAL);
}

c7_bool_t c7_poll_start_thread(c7_poll_t poller, int stacksize_kb)
{
    c7_thread_t th;
    th = c7_thread_new((void (*)(void *))_c7_poll_loop, NULL, poller);
    if (th == NULL)
	return C7_FALSE;
    if (stacksize_kb != 0) {
	if (!c7_thread_set_stacksize(th, stacksize_kb*1024)) {
	    c7_thread_free(th);
	    return C7_FALSE;
	}
    }
    c7_thread_set_autofree(th);
    return c7_thread_start(th);
}

c7_bool_t c7_poll_stop(c7_poll_t pl)
{
    _cntl_t cntl;
    cntl.opc = _CNTL_OPC_STOP;
    return poll_cntl_append(pl->poller, &cntl);
}

c7_bool_t c7_poll_register(c7_poll_t pl,
			   int desc,
			   uint32_t evmask,
			   void (*on_event)(c7_poll_t pl,
					    int desc,
					    int evmask,
					    void *__arg),
			   void *__arg)
{
    _fd_attr_t *fda;
    c7_bool_t status = C7_FALSE;
    C7_THREAD_GUARD_ENTER(&pl->glock);
    if ((fda = c7_parray_new(pl->fdv, desc)) == NULL) {
	c7_status_add(0, ": c7_poll_register: desc: %d\n", desc);
    } else {
	fda->evmask = evmask;
	fda->on_event = on_event;
	fda->__arg = __arg;
	status = poll_register(pl->poller, desc, evmask);
    }
    C7_THREAD_GUARD_EXIT(&pl->glock);
    return status;
}

c7_bool_t c7_poll_modify(c7_poll_t pl, int desc,  uint32_t evmask)
{
    _fd_attr_t *fda;
    c7_bool_t status = C7_FALSE;
    C7_THREAD_GUARD_ENTER(&pl->glock);
    if ((fda = c7_parray_get(pl->fdv, desc)) == NULL) {
	c7_status_add(0, ": c7_poll_modify: desc: %d\n", desc);
    } else {
	fda->evmask = evmask;
	status = poll_modify(pl->poller, desc, evmask);
    }
    C7_THREAD_GUARD_EXIT(&pl->glock);
    return status;
}

c7_bool_t c7_poll_unregister(c7_poll_t pl, int desc)
{
    c7_bool_t status = C7_FALSE;
    C7_THREAD_GUARD_ENTER(&pl->glock);
    if (!c7_parray_check(pl->fdv, desc)) {
	c7_status_add(errno = EINVAL,
			": c7_poll_unregister: desc: %d\n", desc);
    } else {
	c7_parray_free(pl->fdv, desc);
	status = poll_unregister(pl->poller, desc);
    }
    C7_THREAD_GUARD_EXIT(&pl->glock);
    return status;
}

c7_bool_t c7_poll_pause(c7_poll_t pl, int desc)
{
    _fd_attr_t *fda = NULL;
    c7_bool_t status = C7_FALSE;
    C7_THREAD_GUARD_ENTER(&pl->glock);
    if ((fda = c7_parray_get(pl->fdv, desc)) == NULL) {
	c7_status_add(0, ": c7_poll_pause: desc: %d\n", desc);
    } else {
	fda->evmask_saved = fda->evmask;
	fda->evmask = 0;
	status = poll_modify(pl->poller, desc, fda->evmask);
    }
    C7_THREAD_GUARD_EXIT(&pl->glock);
    return status;
}

c7_bool_t c7_poll_resume(c7_poll_t pl, int desc)
{
    _fd_attr_t *fda = NULL;
    c7_bool_t status = C7_FALSE;
    C7_THREAD_GUARD_ENTER(&pl->glock);
    if ((fda = c7_parray_get(pl->fdv, desc)) == NULL) {
	c7_status_add(0, ": c7_poll_resume: desc: %d\n", desc);
    } else {
	fda->evmask = fda->evmask_saved;
	status = poll_modify(pl->poller, desc, fda->evmask);
    }
    C7_THREAD_GUARD_EXIT(&pl->glock);
    return status;
}

static void _call_on_alarm(void *__arg)
{
    _alarm_arg_t *arg = __arg;
    c7_status_clear();
    arg->on_alarm(arg->poller, arg->__arg);
    c7_status_clear();
    c7_mpool_put(arg);
}

c7_alarm_t c7_poll_alarm_on(c7_poll_t pl,
			    int delay_ms,
			    void (*on_alarm)(c7_poll_t pl,
					     void *__arg),
			    void *__arg)
{
    c7_alarm_t alarm = C7_TIMER_INV_ALARM;

    C7_THREAD_GUARD_ENTER(&pl->glock);
    _alarm_arg_t *arg = c7_mpool_get(pl->alarm_arg_pool);
    if (arg != NULL) {
	arg->on_alarm = on_alarm;
	arg->poller = pl;
	arg->__arg = __arg;
	int64_t tv_us = c7_time_us() + delay_ms * 1000;
	alarm = c7_timer_alarm_on(pl->timer, tv_us, _call_on_alarm, arg);
	if (alarm != C7_TIMER_INV_ALARM) {
	    _cntl_t cntl = { .opc=_CNTL_OPC_RECALTMO };
	    if (!poll_cntl_append(pl->poller, &cntl)) {
		c7_poll_alarm_off(pl, alarm);
		alarm = C7_TIMER_INV_ALARM;
	    }
	}
    }
    C7_THREAD_GUARD_EXIT(&pl->glock);
    return alarm;
}

void c7_poll_alarm_off(c7_poll_t pl,
		       c7_alarm_t alarm)
{
    c7_timer_alarm_off(pl->timer, alarm);
}

void c7_poll_free(c7_poll_t pl)
{
    C7_THREAD_GUARD_ENTER(&pl->glock);
    poll_free(pl->poller);
    c7_timer_free(pl->timer);
    c7_parray_destroy(pl->fdv);
    c7_mpool_free(pl->alarm_arg_pool);
    C7_THREAD_GUARD_EXIT(&pl->glock);
    free(pl);
}
