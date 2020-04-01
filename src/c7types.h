/*
 * c7types.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_TYPES_LOADED__
#define __C7_TYPES_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>


#if defined(__linux)
# include <stdint.h>
# if defined(__LP64__)
#   define __C7_CONFIG_LP64
# endif
#elif defined(__CYGWIN__)
# if defined(__x86_64)
#   define __C7_CONFIG_LP64
# endif
#elif defined(__sun)
# include <inttypes.h>
# if defined(__x86_64)
#   define __C7_CONFIG_LP64
# endif
#endif


#if defined(C7_CONFIG_BOOL_INT)
typedef int c7_bool_t;
# define C7_FALSE	(0)
# define C7_TRUE	(1)
#else
typedef enum c7_bool_t_ {
    C7_FALSE,
    C7_TRUE
} c7_bool_t;
#endif

#define C7_UNUSED_INT	__c7_unused_int
extern int C7_UNUSED_INT;

#define C7_TIME_S_us	((c7_time_t)1000000)

typedef int64_t c7_time_t;

#define C7_SYSERR      	(-1)
#define C7_SYSOK	(0)
#define C7_SYSNOERR	C7_SYSOK

// log level: 0..7
#define C7_LOG_MIN	(0U)
#define C7_LOG_MAX	(7U)
#define C7_LOG_ALW	C7_LOG_MIN	// always
#define C7_LOG_ERR	(1U)
#define C7_LOG_WRN	(2U)
#define C7_LOG_INF	(3U)
#define C7_LOG_BRF	(4U)		// more information briefly
#define C7_LOG_DTL	(5U)		// more information in detail
#define C7_LOG_TRC	(6U)		// trace (short message, many log)
#define C7_LOG_DBG	C7_LOG_MAX

#define c7_align(_v, _power_of_2)	(1 + (((_v)-1) | ((_power_of_2)-1)))
#define c7_numberof(_a)			((ssize_t)(sizeof(_a)/sizeof((_a)[0])))

#define c7_thread_local	__thread

typedef enum c7_api_t_ {	// This enum must be synchronized with c7hook.c
    C7_API_thread_mutex_init,
    C7_API_thread_cond_init,
    C7_API_thread_lock,
    C7_API_thread_trylock,
    C7_API_thread_unlock,
    C7_API_thread_notify,
    C7_API_thread_notify_all,
    C7_API_thread_wait,
    C7_API_poll_register_direct,
    C7_API_poll_modify_direct,
    C7_API_poll_unregister_direct,
    C7_API_poll_cntl_action,
} c7_api_t;


#if defined(__cplusplus)
}
#endif
#endif /* c7types.h */
