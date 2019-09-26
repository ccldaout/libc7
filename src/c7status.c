/*
 * c7status.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <c7dconf.h>
#include <c7status.h>
#include <c7thread.h>


typedef struct sts_data_t_ {
    const char *file;
    int line;
    c7_status_t status;
} sts_data_t;


/*----------------------------------------------------------------------------
                   status catalogue and convert status code
----------------------------------------------------------------------------*/

static pthread_mutex_t _Mutex = PTHREAD_MUTEX_INITIALIZER;
#define _LOCK		(void)pthread_mutex_lock(&_Mutex);
#define _UNLOCK		(void)pthread_mutex_unlock(&_Mutex)

static c7_status_catalog_t *_CatalogList;

static struct {
    c7_str_t *(*convert)(c7_str_t *, c7_status_t, void *);
    void *arg;    
} _Converter;

#define C7_ELM_TYPE	c7_status_def_t
#define C7_ELM_LT(p,q)	((p)->sts < (q)->sts)
#define C7_QSORT_ST	_sortdef
#include <c7sortdef.h>

#define C7_KEY_GT(ka, kz, elm)	((*(c7_status_t *)(ka)) > (elm)->sts)
#define C7_KEY_LT(ka, kz, elm)	((*(c7_status_t *)(ka)) < (elm)->sts)
#define C7_BSEARCH	_finddef
#include <c7bsearch.h>

void c7_status_add_catalog(c7_status_catalog_t *cat)
{
    _sortdef(cat->defv, cat->defc);
    _LOCK;
    cat->__next = _CatalogList;
    _CatalogList = cat;
    _UNLOCK;
}

void c7_status_add_convert(c7_str_t *(*convert)(c7_str_t *, c7_status_t, void *),
			   void *arg)
{
    _Converter.convert = convert;
    _Converter.arg = arg;
}

c7_str_t *c7_status_str(c7_str_t *sbp, c7_status_t status)
{
    c7_status_t cat = C7_STATUS_CAT(status);
    c7_status_t sts = C7_STATUS_STS(status);

    if (cat == 0)
	return c7_sprintf(sbp, "errno: %d, %s", sts, strerror(sts));

    _LOCK;
    c7_status_catalog_t *catp;
    for (catp = _CatalogList; catp != NULL; catp = catp->__next) {
	if (catp->cat == cat)
	    break;
    }
    _UNLOCK;

    if (catp != NULL) {
	c7_status_t s2 = C7_STATUS(cat, sts);	// remove OPTION bits
	c7_status_def_t *def = _finddef(catp->defv, catp->defc,
					&s2, sizeof(s2), NULL);
	if (def != NULL)
	    return c7_sprintf(sbp, "%s: %s, %s",
			      catp->cat_id, def->sts_id, def->sts_msg);
	return c7_sprintf(sbp, "%s(%u): 0x%04x(%u)",
			  catp->cat_id, cat, sts, sts);
	
    }

    if (_Converter.convert != NULL) {
	c7_str_t *sbp2 = _Converter.convert(sbp, status, _Converter.arg);
	if (sbp2 != NULL && C7_STR_OK(sbp2))
	    return sbp2;
    }
    return c7_sprintf(sbp, "0x%04x(%u): 0x%04x(%u)", cat, cat, sts, sts);
}


/*----------------------------------------------------------------------------
                  management of multiple status informations
----------------------------------------------------------------------------*/

static c7_thread_local c7_status_stack_t status_stack = {
    .rbuf = {
	C7_CONFIG_STS_RBUF_SIZE,
	C7_CONFIG_STS_RBUF_SIZE*2,
	C7_CONFIG_STS_RBUF_SIZE*2
    },
    .pushed = NULL,
};

static c7_thread_local c7_status_stack_t *status_stack_p;

static c7_bool_t init_thread(void)
{
    status_stack_p = &status_stack;
    return C7_TRUE;
}

#define get_status_stack_p()	status_stack_p

void __c7_status_push(c7_status_stack_t *ssp)
{
    ssp->pushed = status_stack_p;
    c7_rbuf_init(ssp->rbuf, sizeof(ssp->rbuf));
    status_stack_p = ssp;
}

void __c7_status_pop(void)
{
    if (status_stack_p->pushed != NULL)
	status_stack_p = status_stack_p->pushed;
}

void c7_status_clear(void)
{
    c7_status_stack_t *ssp = get_status_stack_p();
    c7_rbuf_clear(ssp->rbuf);
}

void c7_status_save(c7_status_stack_t *save)
{
    c7_status_stack_t *ssp = get_status_stack_p();
    *save = *ssp;
}

void c7_status_saveclear(c7_status_stack_t *save)
{
    c7_status_stack_t *ssp = get_status_stack_p();
    *save = *ssp;
    c7_rbuf_clear(ssp->rbuf);
}

void c7_status_restore(const c7_status_stack_t *saved)
{
    c7_status_stack_t *ssp = get_status_stack_p();
    *ssp = *saved;
}

void __c7_status_reset(const char *file, int line,
		       c7_status_t status, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    c7_status_clear();
    __c7_status_add_va(file, line, status, fmt, ap);
    va_end(ap);
}

void __c7_status_reset_va(const char *file, int line,
			  c7_status_t status, const char *fmt, va_list ap)
{
    c7_status_clear();
    __c7_status_add_va(file, line, status, fmt, ap);
}

void __c7_status_add(const char *file, int line,
		     c7_status_t status, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    __c7_status_add_va(file, line, status, fmt, ap);
    va_end(ap);
}

void __c7_status_add_va(const char *file, int line,
			c7_status_t status, const char *fmt, va_list ap)
{
    int errno_save = errno;

    struct iovec iov[2];

    sts_data_t sts = { .file = file, .line = line, .status = status };
    iov[0].iov_base = &sts;
    iov[0].iov_len = sizeof(sts);

    if (fmt != NULL) {
	int n = 1024;
	char buff[n];
	int rn = vsnprintf(buff, n, fmt, ap);
	if (rn >= n)
	    rn = n - 1;
	iov[1].iov_base = buff;
	iov[1].iov_len = rn + 1;
    } else {
	iov[1].iov_base = NULL;
	iov[1].iov_len = 0;
    }

    c7_status_stack_t *ssp = get_status_stack_p();
    c7_rbuf_putiov(ssp->rbuf, iov, 2);

    errno = errno_save;
}


/*----------------------------------------------------------------------------
                 handle saved status information (to string)
----------------------------------------------------------------------------*/

c7_bool_t c7_status_has_error(void)
{
    c7_status_stack_t *ssp = get_status_stack_p();
    return !c7_rbuf_is_empty(ssp->rbuf);
}

c7_str_t *c7_status_prefix(c7_str_t *sbp, const char *file, int line)
{
    file = c7strrchr_next(file, '/', file);
    int fn = c7strchr_x(file, '.', 0) - file;
    switch ((int)c7_dconf_i(C7_DCONF_PREF)) {
      case 0:
	return c7_sprintf(sbp, "(%.*s:%1d): ", fn, file, line);
      case 1:
	return c7_sprintf(sbp, "%s: ", c7progname());
      default:
	return c7_sprintf(sbp, "%s(%.*s:%1d): ", c7progname(), fn, file, line);
    }
}

static c7_bool_t sts_callback(void *__data, rbuf_unit_t size_b, void *__prm)
{
    sts_data_t * const sts = __data;
    c7_str_t *sbp = __prm;

    char prefbuf[128];
    c7_str_t prefix = C7_STR_INIT_ARG(prefbuf, sizeof(prefbuf));
    c7_status_prefix(&prefix, sts->file, sts->line);

    if (sts->status != 0) {
	(void)c7_strcpy(sbp, prefbuf);
	(void)c7_stradd(c7_status_str(sbp, sts->status), '\n');
    }

    if (size_b != sizeof(*sts)) {
	char *p = (char *)(sts + 1);
	do {
	    if (*p == ':' && *(p+1) == ' ') {
		(void)c7_strcpy(sbp, prefbuf);
		p += 2;
	    }
	    char *q = (char *)c7strchr_next(p, '\n', NULL);
	    (void)c7_strbcpy(sbp, p, q);
	    p = q;
	} while (*p);
	if (C7_STR_CHAR_R(sbp, -1) != '\n')
	    (void)c7_stradd(sbp, '\n');
    }
    return C7_TRUE;
}

c7_str_t *c7_status_string(c7_str_t *sbp)
{
    if (sbp == NULL)
	sbp = c7_str_new_sg();
    c7_status_stack_t *ssp = get_status_stack_p();
    c7_status_push;
    (void)c7_rbuf_scan(ssp->rbuf, c7_dconf_i(C7_DCONF_STSSCN_MAX), sts_callback, sbp);
    c7_status_pop;
    return sbp;
}

typedef struct scn_ctx_t_ {
    void (*u_func)(const char *file, int line,
		   c7_status_t status, const char *message,
		   const void *__uctx);
    void *u_ctx;
} scn_ctx_t;

static c7_bool_t scn_callback(void *__data, rbuf_unit_t size_b, void *__prm)
{
    sts_data_t *sts = __data;
    scn_ctx_t *ctx = __prm;
    const char *message = (size_b != sizeof(*sts)) ? (char *)(sts + 1) : NULL;
    ctx->u_func(sts->file, sts->line, sts->status, message, ctx->u_ctx);
    return C7_TRUE;
}

void c7_status_scan(void (*scanner)(const char *file, int line,
				    c7_status_t status, const char *message,
				    const void *__uctx),
		    void *__uctx)
{
    scn_ctx_t ctx = { .u_func = scanner, .u_ctx = __uctx };
    c7_status_stack_t *ssp = get_status_stack_p();
    c7_status_push;
    (void)c7_rbuf_scan(ssp->rbuf, c7_dconf_i(C7_DCONF_STSSCN_MAX), scn_callback, &ctx);
    c7_status_pop;
}


/*----------------------------------------------------------------------------
                 library initializer / per-thread initializer
----------------------------------------------------------------------------*/

void __c7_status_init(void)
{
    static c7_thread_iniend_t iniend = {
	.init   = init_thread,
    };
    (void)init_thread();
    c7_thread_register_iniend(&iniend);
}
