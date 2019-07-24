/*
 * _private.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef ___PRIVATE_H_LOADED__
#define ___PRIVATE_H_LOADED__


#include <c7mlog.h>


// module initializer

void __c7_init(void);
void __c7_app_init(void);
void __c7_coroutine_init(void);
void __c7_dconf_init(void);
void __c7_memory_init(void);
void __c7_proc_init(void);
void __c7_signal_init(void);
void __c7_status_init(void);


// hooks

void __c7_hook_memory_error(const char *file, int line,
			    int err, size_t z);
void __c7_hook_thread_error(const char *file, int line,
			    c7_api_t api, int err);
void __c7_hook_poll_error(const char *file, int line,
			  c7_api_t api, int err, const char *fmt, ...);
void __c7_hook_echo(const char *file, int line,
		    int mlog_level, const char *string);


#endif /* private.h */
