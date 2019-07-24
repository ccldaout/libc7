/*
 * c7hook.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include "_private.h"
#include <c7hook.h>
#include <c7status.h>


static const char *C7_API[] = {
    "c7_thread_mutex_init",
    "c7_thread_cond_init",
    "c7_thread_lock",
    "c7_thread_trylock",
    "c7_thread_unlock",
    "c7_thread_notify",
    "c7_thread_notify_all",
    "c7_thread_wait",
    "_poll_register_direct",
    "_poll_modify_direct",
    "_poll_unregister_direct",
    "_poll_cntl_action",
};


/*----------------------------------------------------------------------------
                          memory allocation failure
----------------------------------------------------------------------------*/

static c7_hook_memory_error_t _memory_error;

c7_hook_memory_error_t c7_hook_set_memory_error(c7_hook_memory_error_t hook)
{
    c7_hook_memory_error_t cur = _memory_error;
    _memory_error = hook;
    return cur;
}

void __c7_hook_memory_error(const char *file, int line,
			    int err, size_t z)
{
    int errno_saved = errno;
    __c7_status_add(file, line, err, ": request size: %lu\n", z);
    if (_memory_error != NULL)
	_memory_error(err, z);
    errno = errno_saved;
}


/*----------------------------------------------------------------------------
                        thread/mutex/cond API failure
----------------------------------------------------------------------------*/

static c7_hook_thread_error_t _thread_error;

c7_hook_thread_error_t c7_hook_set_thread_error(c7_hook_thread_error_t hook)
{
    c7_hook_thread_error_t cur = _thread_error;
    _thread_error = hook;
    return cur;
}

void __c7_hook_thread_error(const char *file, int line,
			    c7_api_t api, int err)
{
    int errno_saved = errno;
    __c7_status_add(file, line, err, ": %s API failed\n", C7_API[api]);
    if (_thread_error != NULL)
	_thread_error(api, err);
    errno = errno_saved;
}


/*----------------------------------------------------------------------------
                               poll API failure
----------------------------------------------------------------------------*/

static c7_hook_poll_error_t _poll_error;

c7_hook_poll_error_t c7_hook_set_poll_error(c7_hook_poll_error_t hook)
{
    c7_hook_poll_error_t cur = _poll_error;
    _poll_error = hook;
    return cur;
}

void __c7_hook_poll_error(const char *file, int line,
			  c7_api_t api, int err, const char *fmt, ...)
{
    int errno_saved = errno;
    va_list ap;
    va_start(ap, fmt);
    __c7_status_add_va(file, line, err, fmt, ap);
    va_end(ap);
    if (_poll_error != NULL)
	_poll_error(api, err);
    errno = errno_saved;
}


/*----------------------------------------------------------------------------
                              echo API extention
----------------------------------------------------------------------------*/

static c7_hook_echo_t _echo;

c7_hook_echo_t c7_hook_set_echo(c7_hook_echo_t hook)
{
    c7_hook_echo_t cur = _echo;
    _echo = hook;
    return cur;
}

void __c7_hook_echo(const char *file, int line,
		    int mlog_level, const char *string)
{
    if (_echo != NULL)
	_echo(file, line, mlog_level, string);
}

// preinstalled echo hook for mlog

static struct {
    c7_mlog_t mlog;
    c7_hook_echo_t prev;
} mlog;

static void _echo_mlog(const char *file, int line,
		       int loglevel, const char *string)
{
    if (mlog.mlog != NULL) {
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	c7_bool_t haslock = c7_mlog_has_lock(mlog.mlog);
	if (!haslock)
	    c7_thread_lock(&lock);
	c7_mlog_put(mlog.mlog, C7_MLOG_AUTO_TIME, loglevel, 0, 0,
		    file, line, string, -1UL);
	if (!haslock)
	    c7_thread_unlock(&lock);
    }
    if (mlog.prev != NULL)
	mlog.prev(file, line, loglevel, string);
}

void c7_hook_set_echo_mlog(c7_mlog_t log)
{
    mlog.mlog = log;
    mlog.prev = _echo;
    _echo = _echo_mlog;
}
