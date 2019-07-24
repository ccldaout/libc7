/*
 * c7string.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_STRING_H_LOADED__
#define __C7_STRING_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <stdarg.h>
#include <string.h>
#include <c7memory.h>


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

const char *c7strtime_x(c7_time_t time_us);
const char *c7getenv_x(const char *env, const char *alt);


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

int c7strcount(const char *s, int ch);

int c7strmatch_head(const char *s, ...);
int c7strmatch_headv(const char *s, int cand_n, const char *cand_v[]);
int c7strmatch_tail(const char *s, ...);
int c7strmatch_tailv(const char *s, int cand_n, const char *cand_v[]);

const char *c7strskip(const char *t, const char *list);
const char *c7strskip_ws(const char *t);
const char *c7strfind(const char *t, const char *list);
const char *c7strfind_ws(const char *t);

const char *c7strskip_on(const char *t, const unsigned int *tab, unsigned int mask);
const char *c7strfind_on(const char *t, const unsigned int *tab, unsigned int mask);

const char *c7strchr_x(const char *t, char c, const char *alt);
const char *c7strchr_next(const char *t, char c, const char *alt);
const char *c7strrchr_x(const char *t, char c, const char *alt);
const char *c7strrchr_next(const char *t, char c, const char *alt);
const char *c7strpbrk_x(const char *t, const char *c, const char *alt);
const char *c7strpbrk_next(const char *t, const char *c, const char *alt);

char *c7strcpy_x(char *t, const char *s);
char *c7strbcpy_x(char *t, const char *s, const char *e);

char *c7strdup_mg(c7_mgroup_t mg, const char *addr);
char *c7strbdup_mg(c7_mgroup_t mg, const char *addr, const char *brk);
#define c7strdup(a)		c7strdup_mg(NULL, (a))
#define c7strbdup(a,b)		c7strbdup_mg(NULL, (a), (b))
#define c7strdup_sg(a)		c7strdup_mg(c7_sg_current_mg(), (a))
#define c7strbdup_sg(a,b)	c7strbdup_mg(c7_sg_current_mg(), (a), (b))

char **c7strvdup_mg(c7_mgroup_t mg, char ** const strv, int n);
void c7strvfree_mg(c7_mgroup_t mg, char ** const strv);
#define c7strvdup(sv, n)	c7strvdup_mg(NULL, (sv), (n))
#define c7strvfree(sv)		c7strvfree_mg(NULL, (sv))
#define c7strvdup_sg(sv, n)	c7strvdup_mg(c7_sg_current_mg(), (sv), (n))
#define c7strvfree_sg(sv)	c7strvfree_mg(c7_sg_current_mg(), (sv))


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

typedef struct c7_str_t_ {
    char *__buf;
    char *__lim;
    char *__cur;
    unsigned int __f;
#define __C7_STR_ERR		(1U<<3)	// Invalid state: memory allocation error, ...
#define __C7_STR_HEAP_SELF	(1U<<6)	// allocated by c7_*malloc
#define __C7_STR_HEAP_BUF	(1U<<7) // allocated by c7_*re*alloc
    c7_mgroup_t mg;
} c7_str_t;

extern const char __C7_STR_NULL[];
extern const c7_str_t __C7_STR_None;

#define C7_STR_None			((c7_str_t *)&__C7_STR_None)

#define C7_STR_INIT_ARG(b,n)		{ (b), (((char *)(b))+(n)), (b), 0, NULL }
#define __C7_STR_INIT_3(p,f,mg)		{ (p), (p), (p), (f), (mg) }
#define C7_STR_INIT_MG(mg)		__C7_STR_INIT_3((char *)&__C7_STR_NULL[1], __C7_STR_HEAP_BUF, (mg))
#define C7_STR_INIT_MA()		C7_STR_INIT_MG(NULL)
#define C7_STR_INIT_SG()		C7_STR_INIT_MG(c7_sg_current_mg())
#define C7_STR_INIT_TLS()		C7_STR_INIT_MG(c7_tg_thread_mg)

#define __C7_STR_RESET_ERR(sbp)		((sbp)->__f &= (~__C7_STR_ERR))

#if !defined(C7_CONFIG_SCA)
# define C7_STR_LEN(sbp)		((sbp)->__cur - (sbp)->__buf)
# define C7_STR_CHAR(sbp, idx)		((sbp)->__buf[(idx)])
# define C7_STR_CHAR_R(sbp, idx)	((sbp)->__cur[(idx)])
# define C7_STR_ERR(sbp)		(((sbp)->__f & __C7_STR_ERR) != 0)
# define C7_STR_OK(sbp)			(((sbp)->__f & __C7_STR_ERR) == 0)
# define C7_STR_REUSE(sbp)		((sbp) == C7_STR_None ? C7_FALSE : \
					 (__C7_STR_RESET_ERR(sbp), (sbp)->__cur = (sbp)->__buf), C7_TRUE)
# define c7_strbuf(sbp)			((sbp)->__buf)
#else
# define C7_STR_LEN(sbp)		c7_str_len(sbp)
# define C7_STR_CHAR(sbp, idx)		c7_str_char(sbp, idx)
# define C7_STR_CHAR_R(sbp, idx)	c7_str_char_r(sbp, idx)
# define C7_STR_ERR(sbp)		c7_str_err(sbp)
# define C7_STR_OK(sbp)			c7_str_ok(sbp)
# define C7_STR_REUSE(sbp)		c7_str_reuse(sbp)
# define c7_strbuf(sbp)			c7_str_buf(sbp)
#endif

c7_str_t *c7_str_new_ma(void);			//       malloc
#define   c7_str_new_sg()			c7_str_new_mg(c7_sg_current_mg())
c7_str_t *c7_str_new_mg(c7_mgroup_t);		// c7_mg_malloc
c7_bool_t c7_str_setbuf(c7_str_t *sbp, char *buf, size_t size);
c7_bool_t c7_str_alloc(c7_str_t *sbp, size_t extend);
c7_str_t *c7_str_unlink(c7_str_t *sbp);		// unlink from memory group
c7_str_t *c7_strclone(c7_str_t *sbp);		// c7_sg_malloc
char *c7_str_strdup(c7_str_t *sbp);
char *c7_str_release(c7_str_t *sbp);
char *c7_str_buf(c7_str_t *sbp);
void c7_str_free(c7_str_t *sbp);

c7_bool_t c7_str_reuse(c7_str_t *sbp);
c7_bool_t c7_str_err(c7_str_t *sbp);
c7_bool_t c7_str_ok(c7_str_t *sbp);

int c7_str_len(c7_str_t *sbp);
int c7_str_char(c7_str_t *sbp, int idx);
int c7_str_char_r(c7_str_t *sbp, int r_idx);	// r_idx must be negative
c7_str_t *c7_strtrunc(c7_str_t *sbp, int n);
c7_str_t *c7_stradd(c7_str_t *sbp, char ch);
c7_str_t *c7_stradd_n(c7_str_t *sbp, char ch, int n);
c7_str_t *c7_strcpy(c7_str_t *sbp, const char *s);
c7_str_t *c7_strcpy_n(c7_str_t *sbp, const char *s, int n);
c7_str_t *c7_strncpy(c7_str_t *sbp, const char *s, int n);
c7_str_t *c7_strbcpy(c7_str_t *sbp, const char *s, const char *e);
c7_str_t *c7_strupr(c7_str_t *sbp, const char *s);
c7_str_t *c7_strnupr(c7_str_t *sbp, const char *s, int n);
c7_str_t *c7_strbupr(c7_str_t *sbp, const char *s, const char *e);
c7_str_t *c7_strlwr(c7_str_t *sbp, const char *s);
c7_str_t *c7_strnlwr(c7_str_t *sbp, const char *s, int n);
c7_str_t *c7_strblwr(c7_str_t *sbp, const char *s, const char *e);
c7_str_t *c7_stredit(c7_str_t *sbp, int off, int del_n, const char *s);
c7_str_t *c7_strnedit(c7_str_t *sbp, int off, int del_n, const char *s, int n);
c7_str_t *c7_strbedit(c7_str_t *sbp, int off, int del_n, const char *s, const char *e);
c7_str_t *c7_strconcat(c7_str_t *sbp, const char *sep, const char **sv, int sc);
c7_str_t *c7_sprintf(c7_str_t *sbp, const char *fmt, ...);
c7_str_t *c7_vsprintf(c7_str_t *sbp, const char *fmt, va_list ap);
#define c7_stropen(sbp, off, n)		c7_strnedit((sbp), (off), 0, NULL, (n))


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

c7_str_t *c7_streval_C(c7_str_t *out, const char *in);
c7_str_t *c7_streval_custom(c7_str_t *out, const char *in, char mark, char escape,
			    const char *(*translator)(c7_str_t *out, const char *vn,
						      c7_bool_t enclosed, void *__arg),
			    void *__arg);
c7_str_t *c7_streval_env(c7_str_t *out, const char *in);


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/


#if defined(__cplusplus)
}
#endif
#endif /* c7string.h */
