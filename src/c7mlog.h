/*
 * c7mlog.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_MLOG_H_LOADED__
#define __C7_MLOG_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7dconf.h>
#include <c7thread.h>


#define C7_MLOG_DIR_ENV		"C7_MLOG_DIR"

#define C7_MLOG_SIZE_MIN	(1024)
#define C7_MLOG_SIZE_MAX	(1U << 30)

#define C7_MLOG_AUTO_TIME	((c7_time_t)-1)

// log level: 0..7 (cf. C7_LOG_xx defined at c7types.h)

// log category: 0..31
#define C7_MLOG_C_MIN		(0U)
#define C7_MLOG_C_MAX		(31U)

// flags of c7_mlog_open_w()
#define C7_MLOG_F_THREAD_NAME	(1U << 0)	// record thread name
#define C7_MLOG_F_SOURCE_NAME	(1U << 1)	// record source name


typedef struct c7_mlog_info_t_ {
    const char *thread_name;
    uint64_t thread_id;
    const char *source_name;
    int source_line;
    uint32_t order;		// record serial number
    int32_t size_b;		// record size
    c7_time_t time_us;		// time stamp in micro sec.
    uint32_t level;
    uint32_t category;
    uint64_t minidata;
} c7_mlog_info_t;

typedef struct c7_mlog_t_ *c7_mlog_t;


c7_mlog_t c7_mlog_open_w(const char *name,
			 size_t hdrsize_b,
			 size_t logsize_b,
			 const char *hint_op,
			 uint32_t flags);

void *c7_mlog_hdraddr(c7_mlog_t log, size_t *hdrsize_b_op);

const char *c7_mlog_hint(c7_mlog_t log);

c7_bool_t c7_mlog_mutex(c7_mlog_t g, pthread_mutex_t *mutex_op);

c7_bool_t c7_mlog_advlock(c7_mlog_t g);

c7_bool_t c7_mlog_has_lock(c7_mlog_t g);

c7_bool_t c7_mlog_put(c7_mlog_t log, c7_time_t time_us,
		      uint32_t level, uint32_t categroy, uint64_t minidata,
		      const char *src_name, int src_line,
		      const void *logaddr, size_t logsize_b);

c7_bool_t c7_mlog_pfx(c7_mlog_t log, c7_time_t time_us,
		      uint32_t level, uint32_t categroy, uint64_t minidata,
		      const char *src_name, int src_line,
		      const char *format, ...);

c7_bool_t c7_mlog_vpfx(c7_mlog_t log, c7_time_t time_us,
		       uint32_t level, uint32_t categroy, uint64_t minidata,
		       const char *src_name, int src_line,
		       const char *format, va_list ap);

#define c7_mlog_pfc(g, v, c, ...)	c7_mlog_pfx((g), C7_MLOG_AUTO_TIME, (v), (c), \
						    0, __FILE__, __LINE__, __VA_ARGS__)

#define c7_mlog_pf(g, v, ...)		c7_mlog_pfc((g), (v), 0, __VA_ARGS__)

#define c7mlog(g, v, ...)		(void)((C7_LOG_##v <= c7_dconf_i(C7_DCONF_MLOG)) ? \
					       c7_mlog_pf((g), C7_LOG_##v, __VA_ARGS__) : 0)

c7_mlog_t c7_mlog_open_r(const char *name);

c7_bool_t c7_mlog_scan(c7_mlog_t log,
		       size_t maxcount,
		       uint32_t order_min,
		       c7_time_t time_us_min,
		       c7_bool_t (*choice)(const c7_mlog_info_t *info, void *__param),
		       c7_bool_t (*access)(const c7_mlog_info_t *info, void *data, void *__param),
		       void *__param);

c7_bool_t c7_mlog_clear(const char *name);

void c7_mlog_close(c7_mlog_t log);


#if defined(__cplusplus)
}
#endif
#endif // c7mlog.h
