/*
 * c7hook.h
 *
 * https://ccldaout.github.io/libc7/group__c7hook.html
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_HOOK_H_LOADED__
#define __C7_HOOK_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7mlog.h>


typedef void (*c7_hook_memory_error_t)(int err, size_t z);
typedef void (*c7_hook_thread_error_t)(c7_api_t api, int err);
typedef void (*c7_hook_poll_error_t)(c7_api_t api, int err);
typedef void (*c7_hook_echo_t)(const char *file, int line,
			       int c7_log_level, const char *string);

c7_hook_memory_error_t c7_hook_set_memory_error(c7_hook_memory_error_t hook);
c7_hook_thread_error_t c7_hook_set_thread_error(c7_hook_thread_error_t hook);
c7_hook_poll_error_t c7_hook_set_poll_error(c7_hook_poll_error_t hook);
c7_hook_echo_t c7_hook_set_echo(c7_hook_echo_t hook);
void c7_hook_set_echo_mlog(c7_mlog_t log);


#if defined(__cplusplus)
}
#endif
#endif /* c7hook.h */
