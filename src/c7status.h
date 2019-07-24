/*
 * c7status.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_STATUS_H_LOADED__
#define __C7_STATUS_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7rbuf.h>
#include <c7string.h>


#if !defined(C7_CONFIG_STS_RBUF_SIZE)
# define C7_CONFIG_STS_RBUF_SIZE	2048
#endif


typedef uint32_t c7_status_t;

typedef struct c7_status_stack_t_ {
    rbuf_unit_t rbuf[C7_RBUF_SIZE(C7_CONFIG_STS_RBUF_SIZE)];
    struct c7_status_stack_t_ *pushed;
} c7_status_stack_t;


#define C7_STATUS(c, s)		((((c7_status_t)(c))<<16) | ((c7_status_t)(s)))
#define C7_STATUS_OPT(s)	( ((c7_status_t)(s)) & 0xf0000000       )
#define C7_STATUS_CAT(s)	((((c7_status_t)(s)) & 0x0fff0000) >> 16)
#define C7_STATUS_STS(s)	( ((c7_status_t)(s)) & 0x00007fff       )	// 15bits width
#define C7_STATUS_CAT_LIBC	0x0000
#define C7_STATUS_CAT_C7	0x0001
#define C7_STATUS_CAT_USER_MIN	0x0800
#define C7_STATUS_CAT_USER_MAX	0x0fff
#define C7_STATUS_OPT_CLEAR	0x40000000	// c7_app_*_err
#define C7_STATUS_INVALID	C7_STATUS(C7_STATUS_CAT_C7, 0x7fff)


typedef struct c7_status_def_t_ {
    const char *sts_msg;
    const char *sts_id;
    c7_status_t sts;		// include catalog code
} c7_status_def_t;

typedef struct c7_status_catalog_t_ {
    struct c7_status_catalog_t_ *__next;
    c7_status_t cat;
    const char *cat_id;
    c7_status_def_t *defv;	// Must point to a permanent area.
    int defc;
} c7_status_catalog_t;


// 'cat' must point to a permanent area.
void c7_status_add_catalog(c7_status_catalog_t *cat);

void c7_status_add_convert(c7_str_t *(*convert)(c7_str_t *, c7_status_t, void *),
			   void *arg);

#define c7_status_push				\
    do { c7_status_stack_t __save_stack;	\
    __c7_status_push(&__save_stack)
#define c7_status_pop				\
    __c7_status_pop();				\
    } while (0)
void __c7_status_push(c7_status_stack_t *ss);
void __c7_status_pop(void);

void c7_status_clear(void);
void c7_status_save(c7_status_stack_t *save);
void c7_status_saveclear(c7_status_stack_t *save);
void c7_status_restore(const c7_status_stack_t *saved);

#define c7_status_reset(s, ...)				\
    __c7_status_reset(__FILE__, __LINE__, s, __VA_ARGS__)
void __c7_status_reset(const char *file, int line,
		       c7_status_t status, const char *fmt, ...);
#define c7_status_reset_va(e, fm, ap)			\
    __c7_status_reset_va(__FILE__, __LINE__, e, fm, ap)
void __c7_status_reset_va(const char *file, int line,
			  c7_status_t status, const char *fmt, va_list ap);

#define c7_status_add(s, ...)				\
    __c7_status_add(__FILE__, __LINE__, s, __VA_ARGS__)
void __c7_status_add(const char *file, int line,
		     c7_status_t status, const char *fmt, ...);
#define c7_status_add_va(e, fm, ap)			\
    __c7_status_add_va(__FILE__, __LINE__, e, fm, ap)
void __c7_status_add_va(const char *file, int line,
			c7_status_t status, const char *fmt, va_list ap);


c7_bool_t c7_status_has_error(void);
c7_str_t *c7_status_prefix(c7_str_t *sbp, const char *file, int line);
c7_str_t *c7_status_string(c7_str_t *sbp);
void c7_status_scan(void (*scanner)(const char *file,
				    int line,
				    c7_status_t status,
				    const char *message,
				    const void *__uctx),
		    void *__uctx);


#if defined(__cplusplus)
}
#endif
#endif /* c7status.h */
