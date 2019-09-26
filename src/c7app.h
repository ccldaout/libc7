/*
 * c7app.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_APP_H_LOADED__
#define __C7_APP_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7status.h>
#include <pwd.h>


#define c7_init(pn, f)		c7_app_init((pn), (f))

#define c7progname()		__c7_app_progname

#define c7dbg(...)		c7_app_echo(C7_LOG_DBG, __FILE__, __LINE__, __VA_ARGS__)
#define c7detail(...)		c7_app_echo(C7_LOG_DTL, __FILE__, __LINE__, __VA_ARGS__)
#define c7brief(...)		c7_app_echo(C7_LOG_BRF, __FILE__, __LINE__, __VA_ARGS__)
#define c7echo(...)		c7_app_echo(C7_LOG_INF, __FILE__, __LINE__, __VA_ARGS__)
#define c7echo_err(e, ...)	c7_app_echo_err(__FILE__, __LINE__, (e), __VA_ARGS__)
#define c7echo_err1(e, ...)	c7echo_err(C7_STATUS_OPT_CLEAR|(e), __VA_ARGS__)
#define c7exit(...)		c7_app_exit(__FILE__, __LINE__, __VA_ARGS__)
#define c7exit_err(e, ...)	c7_app_exit_err(__FILE__, __LINE__, (e), __VA_ARGS__)
#define c7exit_err1(e, ...)	c7exit_err(C7_STATUS_OPT_CLEAR|(e), __VA_ARGS__)
#define c7abort(...)		c7_app_abort(__FILE__, __LINE__, __VA_ARGS__)
#define c7abort_err(e, ...)	c7_app_abort_err(__FILE__, __LINE__, (e), __VA_ARGS__)
#define c7abort_err1(e, ...)	c7abort_err(C7_STATUS_OPT_CLEAR|(e), __VA_ARGS__)

#define c7_time_us()		c7_app_time_us()
#define c7_sleep_us(u)		c7_app_sleep_us(u)


extern const char *__c7_app_progname;

void c7_app_init(const char *progname_opt, uint32_t flags);

void c7_app_echo_va(int level,
		     const char *file, int line,
		     const char *fmt, va_list ap);
void c7_app_echo_err_va(const char *file, int line,
			 c7_status_t, const char *fmt, va_list ap);
void c7_app_echo(int level,
		  const char *file, int line,
		  const char *fmt, ...);
void c7_app_echo_err(const char *file, int line,
		      c7_status_t, const char *fmt, ...);
void c7_app_exit_va(const char *file, int line,
		     const char *fmt, va_list ap);
void c7_app_exit_err_va(const char *file, int line,
			 c7_status_t, const char *fmt, va_list ap);
void c7_app_exit(const char *file, int line,
		  const char *fmt, ...);
void c7_app_exit_err(const char *file, int line,
		      c7_status_t, const char *fmt, ...);
void c7_app_abort_va(const char *file, int line,
		      const char *fmt, va_list ap);
void c7_app_abort_err_va(const char *file, int line,
			  c7_status_t, const char *fmt, va_list ap);
void c7_app_abort(const char *file, int line,
		   const char *fmt, ...);
void c7_app_abort_err(const char *file, int line,
		       c7_status_t, const char *fmt, ...);

void c7_app_daemon(int maxdesc, const char *wdir, const char *new_stderr, mode_t new_umask);

c7_time_t c7_app_time_us(void);

c7_time_t c7_app_sleep_us(int64_t time_us);

struct passwd *c7_app_getpwnam_x(const char *name);

struct passwd *c7_app_getpwuid_x(uid_t uid);


#if defined(__cplusplus)
}
#endif
#endif /* c7app.h */
