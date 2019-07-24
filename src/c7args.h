/*
 * c7args.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_ARGS_H_LOADED__
#define __C7_ARGS_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7deque.h>
#include <c7string.h>


#define C7_ARGS_PRMC_UNLIM	(-1U)

typedef struct c7_args_t_ *c7_args_t;

typedef enum c7_args_type_t_ {
    C7_ARGS_T_none,
    C7_ARGS_T_INT,
    C7_ARGS_T_REAL,
    C7_ARGS_T_KEY,		// keyword list
    C7_ARGS_T_REX,		// extended regular expression
    C7_ARGS_T_DTV,		// [[[YY]MMDD]hhmm][.ss] or . (-> time_t)
    C7_ARGS_T_ANY,
    C7_ARGS_T_numof
} c7_args_type_t;

typedef struct c7_args_params_t_ c7_args_params_t;

typedef c7_bool_t (*c7_args_handler_t)(
    c7_args_t ap,
    const c7_args_params_t *params,
    void *__uctx);

typedef struct c7_args_optdef_t_ {
    char *long_opt;		// long option string ('--' is not included)
    char *short_opt;		// short option character
    char *optrepr;		// option description 
    char *prmrepr;		// parameter description
    char *prmword;		// parameter show word
    c7_args_type_t prmtype;
    char **conds;		// *_T_KEY|*_T_REX
    /*
     * prmc_min, prmc_max
     *        0,       0
     *        0,       1
     *        1,       1
     *        0,       n
     *        1,       n
     *        0,       C7_ARGS_PRMC_UNLIM
     *        1,       C7_ARGS_PRMC_UNLIM
     *        m,       n
     *        m,       C7_ARGS_PRMC_UNLIM
     */
    unsigned int prmc_min;
    unsigned int prmc_max;
    c7_args_handler_t handler;
} c7_args_optdef_t;

typedef struct c7_args_prm_t_ {
    c7_args_type_t type;
    int keyindex;		// *_T_KEY / *_T_REX
    union {
	int64_t i;		// *_T_INT
	double r;		// *_T_REAL
	const char *s;		// *_T_KEY / *_T_ANY
	const char **rxv;	// *_T_REX: rxv[0]..rxv[n] (n is count of '(')
	struct {
	    time_t v;
	    enum c7_args_prm_time_t {
		C7_ARGS_TIME_DOT,	// '.' only
		C7_ARGS_TIME_NOSEC,	// no seconds specified
		C7_ARGS_TIME_SEC,	// seconds specified
	    } form;
	} tm;			// *_T_DTV(time_t)
    } u;
} c7_args_prm_t;

struct c7_args_params_t_ {
    const c7_args_optdef_t *optdef;
    const char *specified_prm;
    int prmc;
    c7_args_prm_t *prmv;	// (internal) point to _opt_t.prmv
};

c7_args_t c7_args_init(void);

c7_bool_t c7_args_add(c7_args_t ap,
		      const c7_args_optdef_t *opt);

c7_bool_t c7_args_add_list(c7_args_t ap,
			   const c7_args_optdef_t *optv,
			   int optc);

c7_str_t *c7_args_usage(c7_args_t ap,
			c7_str_t *sbp,
			int expl_off);

char **c7_args_parse(c7_args_t ap,
		     char **argv,
		     void *__uctx);	// argv[0] is 1st argument

void c7_args_free(c7_args_t);


#if defined(__cplusplus)
}
#endif
#endif /* c7args.h */
