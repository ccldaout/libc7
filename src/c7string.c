/*
 * c7string.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <c7memory.h>
#include <c7string.h>
#include <c7app.h>


#if !defined(va_copy) && defined(__va_copy)
# define va_copy __va_copy
#endif


static const char * const WhiteSpaces = " \t";


/*----------------------------------------------------------------------------
                              C string function
----------------------------------------------------------------------------*/

const char *c7strtime_x(c7_time_t time_us)
{
    static c7_thread_local char timebuf[32];
    struct tm tm;
    time_t tv = time_us / C7_TIME_S_us;
    size_t n = strftime(timebuf, sizeof(timebuf)-8, "%Y %m/%d %H:%M:%S",
		   localtime_r(&tv, &tm));
    (void)sprintf(&timebuf[n], ".%06ld", time_us % C7_TIME_S_us);
    return timebuf;
}

const char *c7getenv_x(const char *env, const char *alt)
{
    const char *p = getenv(env);
    return (p != 0) ? p : alt;
}

int c7strcount(const char *s, int ch)
{
    int n = 0;
    while (*s != 0) {
	if (*s++ == ch) {
	    n++;
	}
    }
    return n;
}

int c7strmatch_head(const char *s, ...)
{
    int i, n;
    va_list ap;

    va_start(ap, s);
    for (i = 0;; i++) {
	char *cand = va_arg(ap, char *);
	if (cand == NULL) {
	    i = -1;
	    break;
	}
	n = strlen(cand);
	if (strncmp(s, cand, n) == 0)
	    break;
    }
    va_end(ap);
    return i;
}

int c7strmatch_headv(const char *s, int cand_n, const char *cand_v[])
{
    int i, n;

    if (cand_n == -1)
	for (cand_n = 0; cand_v[cand_n] != NULL; cand_n++);
    for (i = 0; i < cand_n; i++) {
	n = strlen(cand_v[i]);
	if (strncmp(s, cand_v[i], n) == 0)
	    return i;
    }
    return -1;
}

int c7strmatch_tail(const char *s, ...)
{
    int i, n, sn;
    va_list ap;

    sn = strlen(s);
    s += sn;
    va_start(ap, s);
    for (i = 0;; i++) {
	char *cand = va_arg(ap, char *);
	if (cand == NULL) {
	    i = -1;
	    break;
	}
	n = strlen(cand);
	if (sn >= n && strcmp(s - n, cand) == 0)
	    break;
    }
    va_end(ap);
    return i;
    
}

int c7strmatch_tailv(const char *s, int cand_n, const char *cand_v[])
{
    int i, n, sn;

    if (cand_n == -1)
	for (cand_n = 0; cand_v[cand_n] != NULL; cand_n++);
    sn = strlen(s);
    s += sn;
    for (i = 0; i < cand_n; i++) {
	n = strlen(cand_v[i]);
	if (sn >= n && strcmp(s - n, cand_v[i]) == 0)
	    return i;
    }
    return -1;
}

const char *c7strskip(const char *s, const char *list)
{
    return s + strspn(s, list);
}

const char *c7strskip_ws(const char *s)
{
    return s + strspn(s, WhiteSpaces);
}

const char *c7strfind(const char *s, const char *list)
{
    return s + strcspn(s, list);
}

const char *c7strfind_ws(const char *s)
{
    return s + strcspn(s, WhiteSpaces);
}

const char *c7strskip_on(const char *s, const unsigned int *tab, unsigned int mask)
{
    for (; (tab[(unsigned)*s] & mask) != 0; s++) {
        ;
    }
    return s;
}

const char *c7strfind_on(const char *s, const unsigned int *tab, unsigned int mask)
{
    for (; (tab[(unsigned)*s] & mask) == 0; s++) {
        ;
    }
    return s;
}

const char *c7strchr_x(const char *s, char c, const char *alt)
{
    char *p = strchr(s, c);
    return (p != 0) ? p : ((alt == 0) ? strchr(s, 0) : (char *)alt);
}

const char *c7strchr_next(const char *s, char c, const char *alt)
{
    char *p = strchr(s, c);
    return (p != 0) ? (p + 1) : ((alt == 0) ? strchr(s, 0) : (char *)alt);
}

const char *c7strrchr_x(const char *s, char c, const char *alt)
{
    char *p = strrchr(s, c);
    return (p != 0) ? p : ((alt == 0) ? strchr(s, 0) : (char *)alt);
}

const char *c7strrchr_next(const char *s, char c, const char *alt)
{
    char *p = strrchr(s, c);
    return (p != 0) ? (p + 1) : ((alt == 0) ? strchr(s, 0) : (char *)alt);
}

const char *c7strpbrk_x(const char *s, const char *c, const char *alt)
{
    char *p = strpbrk(s, c);
    return (p != 0) ? p : ((alt == 0) ? strchr(s, 0) : (char *)alt);
}

const char *c7strpbrk_next(const char *s, const char *c, const char *alt)
{
    char *p = strpbrk(s, c);
    return (p != 0) ? (p + 1) : ((alt == 0) ? strchr(s, 0) : (char *)alt);
}

char *c7strcpy_x(char *t, const char *s)
{
    while (*s != 0) {
	*t++ = *s++;
    }
    *t = 0;
    return t;
}

char *c7strbcpy_x(char *t, const char *s, const char *e)
{
    while (s < e) {
	*t++ = *s++;
    }
    *t = 0;
    return t;
}


/*----------------------------------------------------------------------------
                          C string function - malloc
----------------------------------------------------------------------------*/

char *c7strdup_mg(c7_mgroup_t mg, const char *s)
{
    size_t n = strlen(s) + 1;
    void *p = c7_mg_malloc(mg, n);
    if (p != NULL)
	(void)memcpy(p, s, n);
    return p;
}

char *c7strbdup_mg(c7_mgroup_t mg, const char *s, const char *b)
{
    size_t n = b - s;
    char *p = c7_mg_malloc(mg, n + 1);
    if (p != NULL) {
	(void)memcpy(p, s, n);
	p[n] = 0;
    }
    return p;
}

char **c7strvdup_mg(c7_mgroup_t mg, char ** const strv, int n)
{
    if (n == -1)
	for (n = 0; strv[n] != NULL; n++);
    char ** const nv = c7_mg_malloc(mg, (n + 1) * sizeof(*nv));
    if (nv == NULL)
	return NULL;
    for (int i = 0; i < n; i++) {
	if ((nv[i] = c7strdup_mg(mg, strv[i])) == NULL) {
	    c7strvfree_mg(mg, nv);
	    return NULL;
	}
    }
    nv[n] = NULL;
    return nv;
}

void c7strvfree_mg(c7_mgroup_t mg, char ** const strv)
{
    for (int i = 0; strv[i] != NULL; i++)
	c7_mg_free(mg, strv[i]);
    c7_mg_free(mg, strv);
}


/*----------------------------------------------------------------------------
                         c7_str_t memory manupulation
----------------------------------------------------------------------------*/

const char __C7_STR_NULL[3];
#define _BUF_NULL	((char *)(&__C7_STR_NULL[1]))

const c7_str_t __C7_STR_None = {
    _BUF_NULL, _BUF_NULL, _BUF_NULL, __C7_STR_ERR
};

#define _SET_ERR(sbp)	((sbp)->__f |= __C7_STR_ERR)

#define _ALLOC_UNIT	16

static char *_buf_realloc(c7_str_t *sbp, size_t reqsize)
{
    char *p = sbp->__buf;
    return c7_mg_realloc(sbp->mg, (p == _BUF_NULL) ? NULL : p, reqsize);
}

static void _buf_free(c7_str_t *sbp)
{
    if (sbp->__buf != _BUF_NULL)
	c7_mg_free(sbp->mg, sbp->__buf);
}

static char *_buf_unlink(c7_str_t *sbp)
{
    char *p = sbp->__buf;
    if (p != _BUF_NULL)
	p = c7_mg_unlink(sbp->mg, sbp->__buf, C7_STR_LEN(sbp)+1);
    return p;
}

static c7_bool_t allocbuf(c7_str_t *sbp, size_t more)
{
    if ((sbp->__f & __C7_STR_ERR) != 0)
	return C7_FALSE;

    size_t off     = sbp->__cur - sbp->__buf;
    size_t cursize = sbp->__lim - sbp->__buf;
    size_t reqsize = off + more;
    if (cursize > reqsize) {	/* cursize >= reqsize + 1 */
	return C7_TRUE;
    }
    if ((sbp->__f & __C7_STR_HEAP_BUF) == 0) {
	c7_status_add(EINVAL, "cannot extend fixed size buffer.");
	_SET_ERR(sbp);
	return C7_FALSE;
    }

    reqsize += (reqsize >> 1);
    reqsize = c7_align(reqsize, _ALLOC_UNIT);
    char *p = _buf_realloc(sbp, reqsize);
    if (p == NULL) {
	_SET_ERR(sbp);
	return C7_FALSE;
    }
    sbp->__buf = p;
    sbp->__lim = p + reqsize;
    sbp->__cur = p + off;
    return C7_TRUE;
}

static __inline__ c7_str_t *extendbuf(c7_str_t *sbp, size_t extend)
{
    if (sbp == NULL)
	sbp = c7_str_new_sg();
    if (sbp != C7_STR_None)
	(void)allocbuf(sbp, extend);
    return sbp;
}

static c7_str_t *new_string(c7_mgroup_t mg,
			    unsigned int flag)
{
    c7_str_t *sbp = c7_mg_malloc(mg, sizeof(*sbp));
    char *buf = c7_mg_malloc(mg, _ALLOC_UNIT);

    if (sbp == NULL || buf == NULL) {
	c7_mg_free(mg, sbp);
	c7_mg_free(mg, buf);
	return C7_STR_None;
    }

    buf[0] = 0;
    sbp->__buf = buf;
    sbp->__lim = buf + _ALLOC_UNIT;
    sbp->__cur = buf;
    sbp->__f   = flag;
    sbp->mg = mg;
    return sbp;
}

c7_str_t *c7_str_new_ma(void)
{
    return c7_str_new_mg(NULL);
}

c7_str_t *c7_str_new_mg(c7_mgroup_t mg)
{
    return new_string(mg, __C7_STR_HEAP_SELF|__C7_STR_HEAP_BUF);
}

c7_bool_t c7_str_setbuf(c7_str_t *sbp, char *buf, size_t size)
{
    if (sbp == C7_STR_None) {
	c7_status_add(EINVAL, "%s: sbp is C7_STR_None.", __func__);
	return C7_FALSE;
    }
    if (buf == NULL) {
	c7_status_add(EINVAL, "%s: buf is NULL.", __func__);
	_SET_ERR(sbp);
	return C7_FALSE;
    }

    if ((sbp->__f & __C7_STR_HEAP_BUF) != 0) {
	_buf_free(sbp);
	sbp->__f &= ~(__C7_STR_HEAP_BUF);
    }
    sbp->__buf = buf;
    sbp->__cur = buf;
    sbp->__lim = sbp->__buf + size;
    __C7_STR_RESET_ERR(sbp);
    return C7_TRUE;
}

c7_bool_t c7_str_alloc(c7_str_t *sbp, size_t extend)
{
    return allocbuf(sbp, extend);
}

c7_str_t *c7_str_unlink(c7_str_t *sbp)
{
    if ((sbp->__f & __C7_STR_HEAP_BUF) != 0 && sbp->mg != NULL) {
	off_t p_off   = sbp->__cur - sbp->__buf;
	off_t lim_off = sbp->__lim - sbp->__buf;
	sbp->__buf = _buf_unlink(sbp);
	sbp->__cur = sbp->__buf + p_off;
	sbp->__lim = sbp->__buf + lim_off;
    }

    if ((sbp->__f & __C7_STR_HEAP_SELF) != 0 && sbp->mg != NULL) {
	sbp = c7_mg_unlink(sbp->mg, sbp, sizeof(*sbp));
    }

    sbp->mg = NULL;
    return sbp;
}

c7_str_t *c7_strclone(c7_str_t *sbp)
{
    c7_str_t *sbp2 = c7_str_new_sg();
    if (sbp == C7_STR_None)
	return sbp2;
    return c7_strcpy(sbp2, c7_strbuf(sbp));
}

char *c7_str_strdup(c7_str_t *sbp)
{
    return c7strdup(sbp->__buf);
}

char *c7_str_release(c7_str_t *sbp)
{
    char *p = sbp->__buf;
    if ((sbp->__f & __C7_STR_HEAP_BUF) != 0 && sbp->mg != NULL) {
	p = _buf_unlink(sbp);
    }
    sbp->__buf = (char *)_BUF_NULL;
    sbp->__lim = sbp->__buf;
    sbp->__cur = sbp->__buf;
    sbp->__f &= ~(__C7_STR_ERR);
    return p;
}

char *c7_str_buf(c7_str_t *sbp)
{
    return sbp->__buf;
}

void c7_str_free(c7_str_t *sbp)
{
    if ((sbp->__f & __C7_STR_HEAP_BUF) != 0)
	_buf_free(sbp);
    if ((sbp->__f & __C7_STR_HEAP_SELF) != 0)
	c7_mg_free(sbp->mg, sbp);
}

c7_bool_t c7_str_reuse(c7_str_t *sbp)
{
    if (sbp == C7_STR_None)
	return C7_FALSE;
    __C7_STR_RESET_ERR(sbp);
    sbp->__cur = sbp->__buf;
    return C7_TRUE;
}

c7_bool_t c7_str_err(c7_str_t *sbp)
{
    return ((sbp->__f & __C7_STR_ERR) != 0);
}

c7_bool_t c7_str_ok(c7_str_t *sbp)
{
    return ((sbp->__f & __C7_STR_ERR) == 0);
}


/*----------------------------------------------------------------------------
                           c7_str_t basic function
----------------------------------------------------------------------------*/

int c7_str_len(c7_str_t *sbp)
{
    return sbp->__cur - sbp->__buf;
}	

int c7_str_char(c7_str_t *sbp, int idx)
{
    if (0 <= idx && idx < (sbp->__lim - sbp->__buf))
	return sbp->__buf[idx];
    return EOF;
}

int c7_str_char_r(c7_str_t *sbp, int r_idx)
{
    return c7_str_char(sbp, c7_str_len(sbp) + r_idx);
}

c7_str_t *c7_strtrunc(c7_str_t *sbp, int n)
{
    if (n < 0)
	n = C7_STR_LEN(sbp) - n;
    if (0 <= n && n < C7_STR_LEN(sbp)) {
	sbp->__cur = sbp->__buf + n;
	*(sbp->__cur) = 0;
    }
    return sbp;
}

c7_str_t *c7_stradd(c7_str_t *sbp, char ch)
{
    return c7_strbcpy(sbp, &ch, &ch + 1);
}

c7_str_t *c7_stradd_n(c7_str_t *sbp, char ch, int n)
{
    if (C7_STR_ERR(sbp = extendbuf(sbp, n)))
	n = sbp->__lim - sbp->__cur - 1;
    (void)memset(sbp->__cur, ch, n);
    sbp->__cur += n;
    *sbp->__cur = 0;
    return sbp;
}

c7_str_t *c7_strcpy(c7_str_t *sbp, const char *s)
{
    return c7_strbcpy(sbp, s, strchr(s, 0));
}

c7_str_t *c7_strcpy_n(c7_str_t *sbp, const char *s, int n)
{
    size_t sz = strlen(s);
    size_t z = sz * n;
    if (C7_STR_ERR(sbp = extendbuf(sbp, z))) {
	n = (sbp->__lim - sbp->__cur) / sz + 1;
	for (int i = 0; i < n; i++)
	    sbp = c7_strcpy(sbp, s);
    } else {
	for (int i = 0; i < n; i++)
	    sbp->__cur = (char *)memcpy(sbp->__cur, s, sz) + sz;
	*sbp->__cur = 0;
    }
    return sbp;
}

c7_str_t *c7_strncpy(c7_str_t *sbp, const char *s, int n)
{
    return c7_strbcpy(sbp, s, s + n);
}

c7_str_t *c7_strupr(c7_str_t *sbp, const char *s)
{
    return c7_strbupr(sbp, s, strchr(s, 0));
}

c7_str_t *c7_strnupr(c7_str_t *sbp, const char *s, int n)
{
    return c7_strbupr(sbp, s, s + n);
}

c7_str_t *c7_strlwr(c7_str_t *sbp, const char *s)
{
    return c7_strblwr(sbp, s, strchr(s, 0));
}

c7_str_t *c7_strnlwr(c7_str_t *sbp, const char *s, int n)
{
    return c7_strblwr(sbp, s, s + n);
}

c7_str_t *c7_stredit(c7_str_t *sbp, int del_off, int del_n, const char *ins)
{
    return c7_strnedit(sbp, del_off, del_n, ins, strlen(ins));
}

c7_str_t *c7_strbedit(c7_str_t *sbp, int del_off, int del_n, const char *ins, const char *ins_end)
{
    return c7_strnedit(sbp, del_off, del_n, ins, ins_end - ins);
}

c7_str_t *c7_sprintf(c7_str_t *sbp, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    sbp = c7_vsprintf(sbp, fmt, ap);
    va_end(ap);
    return sbp;
}

c7_str_t *c7_strbcpy(c7_str_t *sbp, const char *s, const char *e)
{
    size_t n = e - s;
    if (C7_STR_ERR(sbp = extendbuf(sbp, n)))
	e = s + (sbp->__lim - sbp->__cur - 1);
    while (s < e)
	*sbp->__cur++ = *s++;
    *sbp->__cur = 0;
    return sbp;
}

c7_str_t *c7_strbupr(c7_str_t *sbp, const char *s, const char *e)
{
    size_t n = e - s;
    if (C7_STR_ERR(sbp = extendbuf(sbp, n)))
	e = s + (sbp->__lim - sbp->__cur - 1);
    while (s < e)
	*sbp->__cur++ = toupper(*s++);
    *sbp->__cur = 0;
    return sbp;
}

c7_str_t *c7_strblwr(c7_str_t *sbp, const char *s, const char *e)
{
    size_t n = e - s;
    if (C7_STR_ERR(sbp = extendbuf(sbp, n)))
	e = s + (sbp->__lim - sbp->__cur - 1);
    while (s < e)
	*sbp->__cur++ = tolower(*s++);
    *sbp->__cur = 0;
    return sbp;
}

c7_str_t *c7_strnedit(c7_str_t *sbp, int del_off, int del_n, const char *ins, int ins_n)
{
    char  *tail = sbp->__buf + del_off + del_n;
    int tail_n = C7_STR_LEN(sbp) - (del_off + del_n) + 1;	// +1 is for a null character.
    int extend;

    if (tail_n <= 0 || ins_n < 0) {
	c7_status_add(EINVAL, "range over: len:%d, del_off:%d, del_n:%d, ins_n:%d\n",
		      (int)C7_STR_LEN(sbp), del_off, del_n, ins_n);
	_SET_ERR(sbp);
	return sbp;
    }

    if ((extend = ins_n - del_n) > 0)
	sbp = extendbuf(sbp, extend);

    if (C7_STR_OK(sbp)) {
	if (extend)
	    (void)memmove(tail + extend, tail, tail_n);
	if (ins != NULL)
	    (void)memmove(sbp->__buf + del_off, ins, ins_n);
	sbp->__cur += extend;
    }
    return sbp;
}

c7_str_t *c7_strconcat(c7_str_t *sbp, const char *sep, const char **sv, int sc)
{
    if (sbp == NULL)
	sbp = c7_str_new_sg();
    if (sc < 0) {
	const char **pp = sv;
	for (sc = 0; *pp != NULL; pp++, sc++);
    }
    if (sc == 0)
	return sbp;
    c7_strcpy(sbp, *sv);
    sc--, sv++;
    for (; sc > 0; sc--, sv++)
	c7_strcpy(c7_strcpy(sbp, sep), *sv);
    return sbp;
}

c7_str_t *c7_vsprintf(c7_str_t *sbp, const char *fmt, va_list ap)
{
    va_list ap2;
    int n;
    if (sbp == NULL) 
	sbp = c7_str_new_sg();
    if (sbp != C7_STR_None) {
	va_copy(ap2, ap);
	n = vsnprintf(0, 0, fmt, ap2);
	va_end(ap2);
	if (n >= 0) {
	    if (allocbuf(sbp, n)) {
		(void)vsprintf(sbp->__cur, fmt, ap);
		sbp->__cur += n;
	    } else {
		n = sbp->__lim - sbp->__cur - 1;
		if (n > 0)
		    (void)vsnprintf(sbp->__cur, n, fmt, ap);
		sbp->__cur += n;
	    }
	} else {
	    c7_status_add(errno, NULL);
	    _SET_ERR(sbp);
	}
    }
    return sbp;
}


/*----------------------------------------------------------------------------
                          c7_str_t advanced function
----------------------------------------------------------------------------*/

static inline c7_bool_t isoctal(char ch)
{
    return (strchr("01234567", ch) != NULL);
}

c7_str_t *c7_streval_C(c7_str_t *out, const char *in)
{
    if (out == NULL)
	out = c7_str_new_sg();
    while (*in != 0) {
	if (*in != '\\') {
	    (void)c7_stradd(out, *in++);
	} else {
	    const char *p, * const cvlist = "\\\\t\ta\007n\nf\fr\r";
	    p = strchr(cvlist, *++in);
	    if (*in == 0) {
		(void)c7_stradd(out, '\\');
	    } else if (p != NULL && ((p - cvlist) & 0x1) == 0) {
		(void)c7_stradd(out, p[1]);
		in++;
	    } else if (*in == 'x' && isxdigit(in[1])) {
		(void)c7_stradd(out, strtol(in+1, (char **)&in, 16));
	    } else if (isoctal(*in)) {
		int ch = *in++ - '0';
		if (isoctal(*in))
		    ch = (ch << 3) + *in++ - '0';
		if (isoctal(*in))
		    ch = (ch << 3) + *in++ - '0';
		(void)c7_stradd(out, ch);
	    } else {
		(void)c7_stradd(out, *in++);
	    }
	}
    }
    return out;
}

typedef struct _evalprm_t {
    char mark;
    char escape;
    char begin;
    char end;
    char *brks;
    const char *(*translator)(c7_str_t *out, const char *vn,
			      c7_bool_t enclosed, void *__arg);
    void *__arg;
} _evalprm_t;

static const char *evalvarref(c7_str_t *out, const char *in, _evalprm_t *prm)
{
    if (*in != prm->begin)
	return prm->translator(out, in, C7_FALSE, prm->__arg);

    const char *p;
    c7_str_t *var = c7_str_new_sg();
    in++;
    while ((p = strpbrk(in, prm->brks)) != NULL) {
	(void)c7_strbcpy(var, in, p);
	if (*p == prm->escape) {
	    if (p[1] == 0)
		return NULL;
	    (void)c7_stradd(var, p[1]);
	    in = p + 2;
	} else if (*p == prm->mark) {
	    c7_sg_push();
	    p = evalvarref(var, p + 1, prm);
	    c7_sg_pop();
	    if (p == NULL)
		return NULL;
	    in = p;
	} else if (*p == prm->end) {
	    if (C7_STR_OK(var) &&
		prm->translator(out, c7_strbuf(var), C7_TRUE, prm->__arg) != NULL)
		return p + 1;
	    return NULL;
	} else {
	    (void)c7_stradd(var, *p);
	    in = p + 1;
	}
    }
    return NULL;
}

c7_str_t *c7_streval_custom(c7_str_t *out, const char *in, char mark, char escape,
			    const char *(*translator)(c7_str_t *out, const char *vn,
						      c7_bool_t enclosed, void *__arg),
			    void *__arg)
{
    if (out == NULL)
	out = c7_str_new_sg();

    _evalprm_t prm = {
	.mark = mark,
	.escape = escape,
	.begin = '{',
	.end = '}',
	.brks = (char[]){ mark, escape, '}', 0 },
	.translator = translator,
	.__arg = __arg,
    };

    const char *p;
    while ((p = strpbrk(in, prm.brks)) != NULL) {
	(void)c7_strbcpy(out, in, p);
	if (*p == escape && p[1] == mark) {
	    (void)c7_stradd(out, mark);
	    in = p + 2;
	} else if (*p == mark) {
	    c7_sg_push();
	    p = evalvarref(out, p + 1, &prm);
	    c7_sg_pop();
	    if (p == NULL) {
		c7_status_add(EINVAL, "%s has Invalid form.", in);
		_SET_ERR(out);
		break;
	    }
	    in = p;
	} else {
	    (void)c7_stradd(out, *p);
	    in = p + 1;
	}
    }
    return c7_strcpy(out, in);
}

static const char *trans_env(c7_str_t *out, const char *vn,
			    c7_bool_t enclosed, void *__arg)
{
    const char *ve;
    if (enclosed) {
	ve = strchr(vn, 0);
	const char *m = c7strchr_x(vn, ':', ve);
	const char *val = c7getenv_x(c7_strbuf(c7_strbcpy(NULL, vn, m)), "");
	if (*m == 0) {		// m == ve
	    (void)c7_strcpy(out, val);
	} else if (m[1] == '+') {
	    if (*val != 0)
		(void)c7_strcpy(out, m + 2);
	} else if (m[1] == '-') {
	    (void)c7_strcpy(out, (*val == 0) ? m+2 : val);
	} else
	    (void)c7_sprintf(out, "${%s}", vn);
    } else {
	ve = vn + 1;
	if (isalpha(*vn) || *vn == '_')
	    for (; isalnum(*ve) || *ve == '_'; ve++);
	(void)c7_strcpy(out, c7getenv_x(c7_strbuf(c7_strbcpy(NULL, vn, ve)), ""));
    }
    return ve;
}

c7_str_t *c7_streval_env(c7_str_t *out, const char *in)
{
    return c7_streval_custom(out, in, '$', '\\', trans_env, NULL);
}
