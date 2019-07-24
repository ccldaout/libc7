/*
 * c7app.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"
#include "_private.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#if !defined(__C7_NO_setsid)
# include <sys/ioctl.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <c7dconf.h>
#include <c7mlog.h>
#include <c7status.h>
#include <c7string.h>
#include <c7thread.h>
#include <c7app.h>


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

const char *__c7_app_progname = "";

static void set_progname(const char *progname)
{
    if (progname == NULL)
	return;
    const char *p = strdup(c7strrchr_next(progname, '/', progname));
    if (p != NULL) {
	*(char *)c7strchr_x(p, '.', NULL) = 0;
	if (__c7_app_progname[0] != 0)
	    free((void *)__c7_app_progname);
	__c7_app_progname = p;
    }
}

static char *getself(char *buf, size_t size)
{
#if defined(__linux)||defined(__CYGWIN__)
    ssize_t n = readlink("/proc/self/exe", buf, size-1);
    if (n != C7_SYSERR) {
	buf[n] = 0;
	return buf;
    }
#endif    
    return NULL;
}

// implicit initializer to be called from __c7_init.
void __c7_app_init(void)
{
    char buffer[512];
    set_progname(getself(buffer, sizeof(buffer)));
}

void c7_app_init(const char *progname_opt, uint32_t flags)
{
    __c7_init();
    set_progname(progname_opt);
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

#define _MSGONLY	C7_STATUS_INVALID

static void _echo_va2(const char *file, int line, c7_status_t errcode, int level,
		      FILE *stdxx, const char *fmt, va_list ap)
{
    static c7_thread_local c7_str_t sb = C7_STR_INIT_TLS();
    c7_str_reuse(&sb);

    if (errcode != _MSGONLY) {
	if ((C7_STATUS_OPT_CLEAR & errcode) != 0)
	    c7_status_clear();
	__c7_status_add(file, line, errcode, NULL);
	(void)c7_status_string(&sb);
    }

    if (fmt != NULL) {
	if (fmt[0] == ':' && fmt[1] == ' ') {
	    (void)c7_status_prefix(&sb, file, line);
	    fmt += 2;
	}
	(void)c7_vsprintf(&sb, fmt, ap);
    }

    if (level <= c7_dconf_i(C7_DCONF_ECHO)) {
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	C7_THREAD_GUARD_ENTER(&mutex);
	(void)fputs(c7_strbuf(&sb), stdxx);
	(void)fflush(stdxx);
	C7_THREAD_GUARD_EXIT(&mutex);
    }

    __c7_hook_echo(file, line, level, c7_strbuf(&sb));
}

static void _echo_va(const char *file, int line, c7_status_t errcode, int level,
		     FILE *stdxx, const char *fmt, va_list ap)
{
    static c7_thread_local c7_bool_t recursive_guard;
    // prevent __c7_hook_echo to call _echo_va2 recursively 
    if (!recursive_guard) {
	recursive_guard = C7_TRUE;
	_echo_va2(file, line, errcode, level, stdxx, fmt, ap);
	recursive_guard = C7_FALSE;
    }
}

void c7_app_echo_va(int level,
		     const char *file, int line,
		     const char *fmt, va_list ap)
{
    _echo_va(file, line, _MSGONLY, level, stdout, fmt, ap);
}

void c7_app_echo_err_va(const char *file, int line,
			 c7_status_t errcode, const char *fmt, va_list ap)
{
    _echo_va(file, line, errcode, C7_LOG_WRN, stderr, fmt, ap);
}

void c7_app_echo(int level,
		  const char *file, int line,
		  const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _echo_va(file, line, _MSGONLY, level, stdout, fmt, ap);
    va_end(ap);
}

void c7_app_echo_err(const char *file, int line,
		      c7_status_t errcode, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _echo_va(file, line, errcode, C7_LOG_WRN, stderr, fmt, ap);
    va_end(ap);
}

void c7_app_exit_va(const char *file, int line,
		     const char *fmt, va_list ap)
{
    _echo_va(file, line, _MSGONLY, C7_LOG_INF, stdout, fmt, ap);
    exit(EXIT_SUCCESS);
}

void c7_app_exit_err_va(const char *file, int line,
			 c7_status_t errcode, const char *fmt, va_list ap)
{
    _echo_va(file, line, errcode, C7_LOG_ERR, stderr, fmt, ap);
    exit(EXIT_FAILURE);
}

void c7_app_exit(const char *file, int line,
		  const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _echo_va(file, line, _MSGONLY, C7_LOG_INF, stdout, fmt, ap);
    va_end(ap);
    exit(EXIT_SUCCESS);
}

void c7_app_exit_err(const char *file, int line,
		      c7_status_t errcode, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _echo_va(file, line, errcode, C7_LOG_ERR, stderr, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

void c7_app_abort_va(const char *file, int line,
		      const char *fmt, va_list ap)
{
    _echo_va(file, line, _MSGONLY, C7_LOG_ERR, stderr, fmt, ap);
    abort();
}

void c7_app_abort_err_va(const char *file, int line,
			  c7_status_t errcode, const char *fmt, va_list ap)
{
    _echo_va(file, line, errcode, C7_LOG_ERR, stderr, fmt, ap);
    abort();
}

void c7_app_abort(const char *file, int line,
		   const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _echo_va(file, line, _MSGONLY, C7_LOG_ERR, stderr, fmt, ap);
    va_end(ap);
    abort();
}

void c7_app_abort_err(const char *file, int line,
		       c7_status_t errcode, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    _echo_va(file, line, errcode, C7_LOG_ERR, stderr, fmt, ap);
    va_end(ap);
    abort();
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

void c7_app_daemon(int maxdesc, const char *wdir, const char *new_stderr, mode_t new_umask)
{
    pid_t pid;
    int fd;

    if (wdir != NULL && chdir(wdir) == C7_SYSERR)
	c7abort_err(errno, ": [c7_app_daemon] chdir() failure.\n");

    if ((pid = fork()) < 0)
	c7abort_err(errno, ": [c7_app_daemon] 1st fork() failure.\n");
    else if (pid > 0)
	_exit(0);

    (void)signal(SIGHUP,  SIG_IGN);
    (void)signal(SIGTTOU, SIG_IGN);
    (void)signal(SIGTTIN, SIG_IGN);
    (void)signal(SIGTSTP, SIG_IGN);

#if !defined(__C7_NO_setsid)
    if (setsid() == -1)
	c7abort_err(errno, ": [c7_app_daemon] setsid() failure.\n");
#else
    /* disconnect control terminal */
    if ((fd = open("/dev/tty", O_RDWR)) != C7_SYSERR) {
	(void)ioctl(fd, TIOCNOTTY, (char *)0);
	(void)close(fd);
    } else
	c7_app_echo(errno, ": [c7_app_daemon] /dev/tty open failed but continue.\n");

    /* become process group leader (exit from current process group) */
    if (setpgid(0, getpid()) == C7_SYSERR)
	c7abort_err(errno, ": [c7_app_daemon] setpgid() failure.\n");
#endif

    if ((pid = fork()) < 0)
	c7abort_err(errno, ": [c7_app_daemon] 2nd fork() failure.\n");
    else if (pid > 0)
	_exit(0);

    for (fd = 0; fd < maxdesc; fd++)
	(void)close(fd);

    (void)open("/dev/null", O_RDONLY);
    (void)open("/dev/null", O_WRONLY);
    if (new_stderr == NULL || open(new_stderr, O_WRONLY|O_CREAT, 0666) == C7_SYSERR)
	(void)open("/dev/null", O_WRONLY);

    (void)umask(new_umask);
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

c7_time_t c7_app_time_us(void)
{
    struct timeval tv;
    (void)gettimeofday(&tv, NULL);
    return ((c7_time_t)tv.tv_sec) * C7_TIME_S_us + tv.tv_usec;
}

c7_time_t c7_app_sleep_us(int64_t time_us)
{
    struct timespec req, rem;
    req.tv_sec = time_us / C7_TIME_S_us;
    req.tv_nsec = (time_us % C7_TIME_S_us) * 1000;
    if (nanosleep(&req, &rem) == C7_SYSERR) {
	c7_status_add(errno, ": nanosleep (%lu micro sec.) failed\n", time_us);
	return C7_SYSERR;
    }
    time_us = (rem.tv_sec * C7_TIME_S_us) + (rem.tv_nsec + 500) / 1000;
    if (time_us < 0)
	time_us = 0;
    return time_us;
}
