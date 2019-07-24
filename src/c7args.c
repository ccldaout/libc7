/*
 * c7args.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <ctype.h>
#include <regex.h>
#include <stdlib.h>
#include <time.h>
#include <c7args.h>
#include <c7deque.h>
#include <c7status.h>
#include <c7app.h>


typedef struct _opt_t {
    const c7_args_optdef_t *optdef;
    c7_deque_t rexv;		/* deque of regex_t (if C7_ARGS_T_REX) */
    c7_deque_t prmv;		/* deque of c7_args_prm_t */
} _opt_t;

struct c7_args_t_ {
    c7_mgroup_t mg;		// point to mg_cond or mg_parse
    c7_mgroup_t mg_cond;
    c7_mgroup_t mg_parse;
    c7_deque_t optv;		/* deque of _opt_t */
    c7_str_t *simple_opts;
    c7_args_params_t params;
    const char *specified_opt;
    c7_bool_t sorted;
    void *__uctx;
};


//----------------------------------------------------------------------------
//                             build condition
//----------------------------------------------------------------------------

static c7_args_optdef_t *dupoptdef(c7_mgroup_t mg, const c7_args_optdef_t *optdef)
{
    c7_args_optdef_t *nopt = c7_mg_malloc(mg, sizeof(*nopt));
    if (nopt == NULL)
	return NULL;
    *nopt = *optdef;
    if (optdef->long_opt != NULL)
	if ((nopt->long_opt = c7strdup_mg(mg, optdef->long_opt)) == NULL)
	    return NULL;
    if (optdef->optrepr != NULL)
	if ((nopt->optrepr = c7strdup_mg(mg, optdef->optrepr)) == NULL)
	    return NULL;
    if (optdef->prmrepr != NULL)
	if ((nopt->prmrepr = c7strdup_mg(mg, optdef->prmrepr)) == NULL)
	    return NULL;
    if (optdef->prmword != NULL)
	if ((nopt->prmword = c7strdup_mg(mg, optdef->prmword)) == NULL)
	    return NULL;
    if (optdef->conds != NULL)
	if ((nopt->conds = c7strvdup_mg(mg, optdef->conds, -1)) == NULL)
	    return NULL;
    return nopt;
}

static void freeregex(const c7_deque_t dq, void *item)
{
    regfree(item);
}

static c7_bool_t setupregex(c7_deque_t rxq, char ** const conds)
{
    c7_str_t rexstr = C7_STR_INIT_SG();

    for (int i = 0; conds[i] != NULL; i++) {
	regex_t rex;
	c7_str_reuse(&rexstr);
	c7_sprintf(&rexstr, "^%s$", conds[i]);
	int ret;
	if ((ret = regcomp(&rex, c7_strbuf(&rexstr), REG_EXTENDED)) != C7_SYSOK) {
	    char errstr[512];
	    (void)regerror(ret, &rex, errstr, sizeof(errstr));
	    c7_status_add(EINVAL, ": regcomp error: \"%s\"\n: %s\n", conds[i], errstr);
	    return C7_FALSE;
	}
	if (c7_deque_push_tail(rxq, &rex) == NULL)
	    return C7_FALSE;
    }
    return C7_TRUE;
}

static c7_bool_t check_optdef(c7_args_t ap,
			       const c7_args_optdef_t *optdef)
{
    _opt_t *opt;

    if (optdef->short_opt != NULL && strlen(optdef->short_opt) != 1) {
	c7_status_add(EINVAL, ": Short option must be one character: '%s'\n", optdef->short_opt);
	return C7_FALSE;
    }

    c7_deque_foreach(ap->optv, opt) {
	if (opt->optdef->short_opt != NULL && optdef->short_opt != NULL &&
	    strcmp(opt->optdef->short_opt, optdef->short_opt) == 0) {
	    c7_status_add(EINVAL, ": Duplicate option '-%s' is specified.\n", optdef->short_opt);
	    return C7_FALSE;
	}
	if (opt->optdef->long_opt != NULL && optdef->long_opt != NULL &&
	    strcmp(opt->optdef->long_opt, optdef->long_opt) == 0) {
	    c7_status_add(EINVAL, ": Duplicate option '--%s' is specified.\n", optdef->long_opt);
	    return C7_FALSE;
	}
    }

    c7_str_t *opt_sb = c7_str_new_mg(ap->mg);
    if (optdef->short_opt != NULL && optdef->long_opt != NULL)
	c7_sprintf(opt_sb, "-%s (--%s)", optdef->short_opt, optdef->long_opt);
    else if (optdef->short_opt != NULL)
	c7_sprintf(opt_sb, "-%s", optdef->short_opt);
    else
	c7_sprintf(opt_sb, "--%s", optdef->long_opt);
    char *opt_str = c7_strbuf(opt_sb);

    if (optdef->prmtype == C7_ARGS_T_none &&
	(optdef->prmc_min != 0 || optdef->prmc_max != 0)) {
	c7_status_add(EINVAL, ": Invalid non-zero prmc_min, prmc_max for option '%s'.\n", opt_str);
	return C7_FALSE;
    }

    if (optdef->short_opt != NULL && optdef->long_opt == NULL &&
	optdef->prmtype != C7_ARGS_T_none && optdef->prmc_min == 0) {
	c7_status_add(EINVAL, ": Invalid zero prmc_min for option '%s'.\n", opt_str);
	return C7_FALSE;
    }

    if (optdef->prmtype != C7_ARGS_T_none && optdef->prmc_min > optdef->prmc_max) {
	c7_status_add(EINVAL, ": Invalid prmc_min (> prmc_max) for optin '%s'.\n", opt_str);
	return C7_FALSE;
    }

    if (optdef->prmtype == C7_ARGS_T_REX || optdef->prmtype == C7_ARGS_T_KEY) {
	if (optdef->conds == NULL || optdef->conds[0] == 0) {
	    c7_status_add(EINVAL, ": conds[] is required for option '%s'.\n", opt_str);
	    return C7_FALSE;
	}
    }

    c7_str_free(opt_sb);
    
    return C7_TRUE;
}

static c7_bool_t add_optdef(c7_args_t ap,
			    const c7_args_optdef_t *optdef)
{
    if (!check_optdef(ap, optdef))
	return C7_FALSE;

    _opt_t newopt = {
	.optdef = dupoptdef(ap->mg, optdef),
	.rexv = c7_deque_create(sizeof(regex_t), freeregex),
	.prmv = c7_deque_create(sizeof(c7_args_prm_t), NULL),
    };
    if (newopt.optdef == NULL || newopt.rexv == NULL || newopt.prmv == NULL)
	return C7_FALSE;
    optdef = newopt.optdef;

    _opt_t *opt;
    if ((opt = c7_deque_push_tail(ap->optv, &newopt)) == NULL)
	return C7_FALSE;

    if (optdef->short_opt != NULL && optdef->prmc_max == 0)
	if (c7_stradd(ap->simple_opts, optdef->short_opt[0]) == NULL)
	    return C7_FALSE;

    if (optdef->prmtype == C7_ARGS_T_REX)
	if (!setupregex(opt->rexv, optdef->conds))
	    return C7_FALSE;

    return C7_TRUE;
}

c7_bool_t c7_args_add(c7_args_t ap,
		      const c7_args_optdef_t *optdef)
{
    c7_sg_push();
    ap->mg = ap->mg_cond;
    ap->sorted = C7_FALSE;
    c7_bool_t ret = add_optdef(ap, optdef);
    ap->mg = NULL;
    c7_sg_pop();
    return ret;
}

c7_bool_t c7_args_add_list(c7_args_t ap,
			   const c7_args_optdef_t *optv,
			   int optc)
{
    for (int i = 0; i < optc; i++) {
	if (!c7_args_add(ap, &optv[i]))
	    return C7_FALSE;
    }
    return C7_TRUE;
}


//----------------------------------------------------------------------------
//                                 parsing
//----------------------------------------------------------------------------

static int _gt_longopt(_opt_t *p, _opt_t *q)
{
    char *ps = (p->optdef->long_opt != NULL) ? p->optdef->long_opt : "-";
    char *qs = (q->optdef->long_opt != NULL) ? q->optdef->long_opt : "-";
    return (strcmp(ps, qs) > 0);
}

#define C7_ELM_TYPE	_opt_t
#define C7_ELM_LT(p,q)	_gt_longopt(p,q)
#define C7_QSORT_ST	_sort4findlong
#include <c7sortdef.h>

static c7_bool_t parse_type_keys(c7_args_t ap, _opt_t *opt, const char *s, c7_args_prm_t *prm)
{
    prm->keyindex = -1;
    for (int i = 0; opt->optdef->conds[i]; i++) {
	if (strcmp(opt->optdef->conds[i], s) == 0) {
	    prm->u.s = s;
	    prm->keyindex = i;
	    return C7_TRUE;
	}
    }

    c7_str_t *kws = c7_strconcat(NULL, ",",
				 (const char **)opt->optdef->conds, -1);
    c7_status_add(EINVAL, ": parameter '%s' of '%s' must be one of %s\n",
		    s, ap->specified_opt, c7_strbuf(kws));
    c7_str_free(kws);
    return C7_FALSE;
}

static c7_bool_t parse_type_regs(c7_args_t ap, _opt_t *opt, const char *s, c7_args_prm_t *prm)
{

    prm->keyindex = -1;
    regex_t *rex;
    c7_deque_foreach(opt->rexv, rex) {
	prm->keyindex = c7_deque_index(opt->rexv, rex);
	int sub_n = c7strcount(opt->optdef->conds[prm->keyindex], '(');
	regmatch_t rm[sub_n + 1];
	if (regexec(rex, s, sub_n + 1, rm, 0) == C7_SYSOK) {
	    prm->u.rxv = c7_mg_calloc(ap->mg, sizeof(prm->u.rxv[0]), sub_n + 1);
	    if (prm->u.rxv == NULL)
		return C7_FALSE;
	    for (int i = 0; i <= sub_n; i++) {
		if (rm[i].rm_so != -1) {
		    prm->u.rxv[i] = c7strbdup_mg(ap->mg, s+rm[i].rm_so, s+rm[i].rm_eo);
		    if (prm->u.rxv[i] == NULL)
			return C7_FALSE;
		} else
		    prm->u.rxv[i] = NULL;
	    }
	    return C7_TRUE;
	}
    }

    c7_status_add(EINVAL, ": parameter '%s' of '%s' is not suitable for %s\n",
		    s, ap->specified_opt, opt->optdef->prmrepr);
    return C7_FALSE;
}

static int twodigits(const char *s)
{
    return (s[0] - '0') * 10 + s[1] - '0';
}

static void mktime_s(const char *s, int form_n, c7_args_prm_t *prm)
{
    time_t tv = time(0);
    struct tm tm;
    localtime_r(&tv, &tm);

    if (form_n == 10) {
	tm.tm_year = 2000 - 1900 + twodigits(s);
	s += 2;
    }
    if (form_n >= 8) {
	tm.tm_mon = twodigits(s) - 1;	// 0 begin
	s += 2;
	tm.tm_mday = twodigits(s);
	s += 2;
    }
    if (form_n >= 4) {
	tm.tm_hour = twodigits(s);
	s += 2;
	tm.tm_min = twodigits(s);
	s += 2;
    }
    if (*s++ == 0) {
	tm.tm_sec = 0;
	prm->u.tm.form = C7_ARGS_TIME_NOSEC;
    } else if (*s == 0) {		// '.' only
	tm.tm_sec = 0;
	prm->u.tm.form = C7_ARGS_TIME_DOT;
    } else {
	tm.tm_sec = twodigits(s);
	prm->u.tm.form = C7_ARGS_TIME_SEC;
    }
	
    prm->u.tm.v = mktime(&tm);
}

static int check_datetime(const char *s)
{
    const char *p = c7strskip(s, "0123456789");
    int n = p - s;
    if (n != 0 && n != 4 && n != 8 && n != 10)
	return -1;
    if (*p == 0)
	return n;
    if (*p++ != '.')
	return -1;
    if (*p == 0)
	return (n == 0) ? 0 : -1;
    if (!isdigit(*p++))
	return -1;
    if (!isdigit(*p++))
	return -1;
    if (*p != 0)
	return -1;
    return n;
}

static c7_bool_t parse_type_datetime(c7_args_t ap, _opt_t *opt, const char *s, c7_args_prm_t *prm)
{
    const char *datetime_spec = "[[[YY]MMDD]hhmm][.ss]";
    const char *opt_s = ap->specified_opt;

    int n = check_datetime(s);
    if (n == -1) {
	c7_status_add(EINVAL, ": parameter '%s' of '%s' is not match with %s\n",
			s, opt_s, datetime_spec);
	return C7_FALSE;
    }
    mktime_s(s, n, prm);
    return C7_TRUE;
}

static c7_bool_t parse_param(c7_args_t ap, _opt_t *opt, const char *s)
{
    c7_args_prm_t prm;
    const char *opt_s = ap->specified_opt;
    char *p;

    if (ap->params.prmc == opt->optdef->prmc_max) {
	c7_status_add(EINVAL, ": Too many parameter '%s' of '%s'\n", s, opt_s);
	return C7_FALSE;
    }

    prm.type = opt->optdef->prmtype;
    
    switch (prm.type) {
      case C7_ARGS_T_INT:
	prm.u.i = strtol(s, &p, 0);
	if (*p != 0) {
	    c7_status_add(EINVAL, ": parameter '%s' of '%s' must be interger\n", s, opt_s);
	    return C7_FALSE;
	}
	break;

      case C7_ARGS_T_REAL:
	prm.u.r = strtod(s, &p);
	if (*p != 0) {
	    c7_status_add(EINVAL, ": parameter '%s' of '%s' must be numeric\n", s, opt_s);
	    return C7_FALSE;
	}
	break;

      case C7_ARGS_T_KEY:
	if (!parse_type_keys(ap, opt, s, &prm))
	    return C7_FALSE;
	break;
	
      case C7_ARGS_T_REX:
	if (!parse_type_regs(ap, opt, s, &prm))
	    return C7_FALSE;
	break;

      case C7_ARGS_T_DTV:
	if (!parse_type_datetime(ap, opt, s, &prm))
	    return C7_FALSE;
	break;

      case C7_ARGS_T_ANY:
      default:
	prm.u.s = s;
	break;
    }

    c7_deque_push_tail(opt->prmv, &prm);
    ap->params.prmv = c7_deque_buffer(opt->prmv);
    ap->params.prmc++;

    return C7_TRUE;
}

static c7_bool_t do_option_noprm(c7_args_t ap, _opt_t *opt)
{
    if (opt->optdef->prmc_min > 0) {
	c7_status_add(EINVAL, ": Option '%s' has at least one parameter.\n", ap->specified_opt);
	return C7_FALSE;
    }

    ap->params.optdef = opt->optdef;
    ap->params.specified_prm = NULL;
    ap->params.prmc = 0;
    c7_status_clear();
    if (!opt->optdef->handler(ap, &ap->params, ap->__uctx)) {
	c7_status_add(0, ": Handler of option '%s' failed.\n", ap->specified_opt);
	return C7_FALSE;
    }
    return C7_TRUE;
}

static c7_bool_t parse_option(c7_args_t ap, _opt_t *opt, const char *prmpos)
{
    if (prmpos == NULL)
	return do_option_noprm(ap, opt);

    if (opt->optdef->prmtype == C7_ARGS_T_none) {
	c7_status_add(EINVAL, ": Option '%s' don't accept any parameter: '%s'\n",
			ap->specified_opt, prmpos);
	return C7_FALSE;
    }

    c7_status_clear();
    ap->params.optdef = opt->optdef;
    ap->params.specified_prm = prmpos;
    ap->params.prmc = 0;
    ap->params.prmv = NULL;
    c7_deque_reset(opt->prmv);

    char *prmstr = c7strdup_mg(ap->mg, prmpos);
    if (prmstr == NULL)	
	return C7_FALSE;

    if (opt->optdef->prmc_max == 1) {
	if (!parse_param(ap, opt, prmstr))
	    return C7_FALSE;
    } else {
	char *sp;
	if ((prmstr = strtok_r(prmstr, ",", &sp)) != NULL) {
	    do {
		if (!parse_param(ap, opt, prmstr))
		    return C7_FALSE;
	    } while ((prmstr = strtok_r(NULL, ",", &sp)) != NULL);
	}
    }

    if (!opt->optdef->handler(ap, &ap->params, ap->__uctx)) {
	c7_status_add(0, ": Handler of option '%s' failed.\n", ap->specified_opt);
	return C7_FALSE;
    }
    return C7_TRUE;
}

static _opt_t *findshortopt_only(c7_args_t ap, const char optch)
{
    _opt_t *opt;
    c7_deque_foreach(ap->optv, opt) {
	if (opt->optdef->short_opt == NULL)
	    continue;
	if (opt->optdef->short_opt[0] == optch)
	    return opt;
    }
    return NULL;
}

static _opt_t *findshortopt(c7_args_t ap, const char optch)
{
    _opt_t *opt = findshortopt_only(ap, optch);
    if (opt != NULL) {
	c7_str_t *sbp = c7_str_new_mg(ap->mg);
	ap->specified_opt = c7_strbuf(c7_sprintf(sbp, "-%s", opt->optdef->short_opt));
	return opt;
    }
    c7_status_add(EINVAL, ": Unknown option '-%c'\n", optch);
    return NULL;
}

static _opt_t *findlongopt(c7_args_t ap, const char *optstr, const char **prmpos)
{
    _opt_t *opt;
    c7_deque_foreach(ap->optv, opt) {
	if (opt->optdef->long_opt == NULL)
	    continue;
	int n = strlen(opt->optdef->long_opt);
	if (strncmp(opt->optdef->long_opt, optstr, n) == 0) {
	    *prmpos = NULL;
	    c7_str_t *sbp = c7_str_new_mg(ap->mg);
	    ap->specified_opt = c7_strbuf(c7_sprintf(sbp, "--%s", opt->optdef->long_opt));
	    if (optstr[n] == 0)
		return opt;
	    if (optstr[n] == '=') {
		*prmpos = &optstr[n + 1];
		return opt;
	    }
	    break;	// not found
	}
    }
    c7_status_add(EINVAL, ": Unknown option '--%s'\n", optstr);
    return NULL;
}

static c7_bool_t parse_simples(c7_args_t ap, const char *simples)
{
    const char *p = simples;
    for (; *p; p++) {
	_opt_t *opt = findshortopt(ap, *p);
	if (opt == NULL)
	    c7abort_err(0,": [INTERNAL BUG] findshortopt failure: '-%c'\n", *p);
	if (!do_option_noprm(ap, opt))
	    return C7_FALSE;
    }
    return C7_TRUE;
}

static char **parse_args(c7_args_t ap, char **argv)
{
    _opt_t *opt;
    
    for (; *argv; argv++) {
	const char *arg = *argv;
	if (strcmp(arg, "-") == 0 || strcmp(arg, "--") == 0)
	    return ++argv;
	if (strncmp(arg, "--", 2) == 0) {
	    // long option
	    if ((opt = findlongopt(ap, arg + 2, &arg)) == NULL)
		return NULL;
	    if (!parse_option(ap, opt, arg))
		return NULL;
	} else if (arg[0] == '-' && strlen(arg) > 2 &&
		   *c7strskip(arg+1, c7_strbuf(ap->simple_opts)) == 0) {
	    // combination of simpoptions
	    if (!parse_simples(ap, arg + 1))
		return NULL;
	} else if (arg[0] == '-') {
	    // short option (may need parameter)
	    if ((opt = findshortopt(ap, arg[1])) == NULL)
		return NULL;
	    if (opt->optdef->prmtype == C7_ARGS_T_none) {
		if (arg[2] != 0) {
		    c7_status_add(EINVAL,
				    ": Option '%s' don't accept any parameter: '%s'\n",
				    ap->specified_opt, &arg[2]);
		    return NULL;
		}
		arg = NULL;
	    } else if (arg[2] != 0) {
		arg = arg + 2;
	    } else if (argv[1] != NULL) {
		arg = *++argv;
		if (opt->optdef->prmc_min == 0) {
		    // ambiguous case
		    if (strcmp(arg, "-") == 0 || strcmp(arg, "--") == 0 ||
			(arg[0] == '-' && findshortopt_only(ap, arg[1]) != NULL)) {
			arg = NULL;
			--argv;
		    }
		}
	    } else {
		arg = NULL;
	    }
	    if (!parse_option(ap, opt, arg))
		return NULL;
	    if (*argv == NULL)
		return argv;
	} else {  // general parameter
	    return argv;
	}
    }
    return argv;
}

char **c7_args_parse(c7_args_t ap,
		     char **argv,
		     void *__uctx)	/* argv[0] is 1st argument */
{
    if (!ap->sorted) {
	_sort4findlong(c7_deque_buffer(ap->optv), c7_deque_count(ap->optv));
	ap->sorted = C7_TRUE;
    }
    ap->__uctx = __uctx;
    ap->mg = ap->mg_parse;
    c7_mg_freeall(ap->mg);
    argv = parse_args(ap, argv);
    ap->mg = NULL;
    return argv;
}


//----------------------------------------------------------------------------
//                              generate usage
//----------------------------------------------------------------------------

static int _lt_allopt(c7_args_optdef_t **p, c7_args_optdef_t **q)
{
    char *p_s = (*p)->short_opt;
    char *p_l = (*p)->long_opt;
    char *q_s = (*q)->short_opt;
    char *q_l = (*q)->long_opt;

    if (p_s && q_s) {
	char pc = tolower(*p_s);
	char qc = tolower(*q_s);
	if (pc == qc)
	    return islower(*p_s);
	else
	    return pc < qc;
    } else if (p_s && !q_s) {
	char pc = tolower(*p_s);
	char qc = tolower(*q_l);
	if (pc == qc)
	    return 1;
	else
	    return (pc < qc);
    } else if (!p_s && q_s) {
	char pc = tolower(*p_l);
	char qc = tolower(*q_s);
	if (pc == qc)
	    return 0;
	else
	    return (pc < qc);
    } else { // !p_s && !q_s
	return (strcmp(p_l, q_l) < 0);
    }
}

#undef C7_ELM_TYPE
#undef C7_ELM_LT
#define C7_ELM_TYPE	c7_args_optdef_t*
#define C7_ELM_LT(p,q)	_lt_allopt(p,q)
#define C7_QSORT_ST	_sort4usage
#include <c7sortdef.h>

static c7_args_optdef_t **create_optdefv(c7_mgroup_t mg, c7_deque_t optv)
{
    int odc = c7_deque_count(optv);
    c7_args_optdef_t ** const odv = c7_mg_malloc(mg, sizeof(*odv) * (odc + 1));
    if (odv == NULL)
	return NULL;
    int i = 0;
    _opt_t *opt;
    c7_deque_foreach(optv, opt)
	odv[i++] = (c7_args_optdef_t *)opt->optdef;
    _sort4usage(odv, odc);
    odv[odc] = NULL;
    return odv;
}

static void create_optstr(c7_str_t *sbp, c7_args_optdef_t *desc)
{
    if (desc->short_opt != NULL && desc->long_opt != NULL) {
	(void)c7_sprintf(sbp, "  -%s, --%s", desc->short_opt, desc->long_opt);
	if (desc->prmc_min > 0)
	    (void)c7_stradd(sbp, '=');
    } else if (desc->short_opt != NULL && desc->long_opt == NULL) {
	(void)c7_sprintf(sbp, "  -%s", desc->short_opt);
	if (desc->prmc_min > 0)
	    (void)c7_stradd(sbp, ' ');
    } else /* if (desc->short_opt == NULL && desc->long_opt != NULL) */ {
	(void)c7_sprintf(sbp, "      --%s", desc->long_opt);
	if (desc->prmc_min > 0)
	    (void)c7_stradd(sbp, '=');
    }

    if (desc->prmc_min == 1 && desc->prmc_max == 1)
	(void)c7_strcpy(sbp, desc->prmword);
    else if (desc->prmc_min == 0 && desc->prmc_max == 1)
	(void)c7_sprintf(sbp, "[=%s]", desc->prmword);
    else if (desc->prmc_min > 0 && desc->prmc_max == 2)
	(void)c7_sprintf(sbp, "%s[,%s]", desc->prmword, desc->prmword);
    else if (desc->prmc_min > 0 && desc->prmc_max > 2)
	(void)c7_sprintf(sbp, "%s[,...]", desc->prmword);
    else if (desc->prmc_min == 0 && desc->prmc_max == 2)
	(void)c7_sprintf(sbp, "[=%s[,%s]]", desc->prmword, desc->prmword);
    else if (desc->prmc_min == 0 && desc->prmc_max > 2)
	(void)c7_sprintf(sbp, "[=%s[,...]]", desc->prmword);
}

static c7_str_t *create_usage(c7_args_t ap, c7_str_t *sbp, int expl_off)
{
    c7_args_optdef_t **odp = create_optdefv(ap->mg, ap->optv);
    if (odp == NULL)
	return c7_status_string(sbp);

    c7_str_t *optstr = c7_str_new_mg(ap->mg);
    for (; *odp; odp++) {
	c7_args_optdef_t *desc = *odp;
	c7_str_reuse(optstr);
	create_optstr(optstr, *odp);
	if (C7_STR_LEN(optstr) + 2 > expl_off)
	    c7_sprintf(sbp, "%s\n%*s%s\n", c7_strbuf(optstr), expl_off, "", desc->optrepr);
	else
	    c7_sprintf(sbp, "%-*s%s\n", expl_off, c7_strbuf(optstr), desc->optrepr);
	if (desc->prmword != NULL) {
	    c7_sprintf(sbp, "%*s%s: %s\n", expl_off, "", desc->prmword, desc->prmrepr);
	    if (desc->prmtype == C7_ARGS_T_KEY) {
		c7_str_t *kws = c7_strconcat(NULL, ",",
					     (const char **)desc->conds, -1);
		c7_sprintf(sbp, "%*s  %s\n",
			   expl_off + strlen(desc->prmword), "", c7_strbuf(kws));
		c7_str_free(kws);
	    }
	}
    }

    return sbp;
}

c7_str_t *c7_args_usage(c7_args_t ap,
			c7_str_t *sbp,
			int expl_off)
{
    c7_mgroup_t save_mg = ap->mg;
    if ((ap->mg = c7_mg_new()) != NULL) {
	if (sbp == NULL)
	    sbp = c7_str_new_sg();
	sbp = create_usage(ap, sbp, expl_off);
	c7_mg_destroy(ap->mg);
    }
    ap->mg = save_mg;
    return sbp;
}


//----------------------------------------------------------------------------
//                          initialize / finalize
//----------------------------------------------------------------------------

static void freeopt(const c7_deque_t dq, void *item)
{
    _opt_t *opt = item;
    c7_deque_destroy(opt->rexv);
    c7_deque_destroy(opt->prmv);
}

c7_args_t c7_args_init(void)
{
    c7_mgroup_t mg_cond, mg_parse;
    c7_args_t ap;
    if ((mg_cond = c7_mg_new()) != NULL) {
	if ((mg_parse = c7_mg_new()) != NULL) {
	    if ((ap = c7_mg_malloc(mg_cond, sizeof(*ap))) != NULL &&
		(ap->simple_opts = c7_str_new_mg(mg_cond)) != C7_STR_None) {
		ap->sorted = C7_FALSE;
		ap->mg = NULL;
		ap->mg_cond = mg_cond;
		ap->mg_parse = mg_parse;
		ap->optv = c7_deque_create(sizeof(_opt_t), freeopt);
		ap->__uctx = NULL;
		return ap;
	    }
	    c7_mg_destroy(mg_parse);
	}
	c7_mg_destroy(mg_cond);
    }
    return NULL;
}

void c7_args_free(c7_args_t ap)
{
    if (ap != NULL) {
	c7_deque_destroy(ap->optv);
	c7_mg_destroy(ap->mg_parse);
	c7_mg_destroy(ap->mg_cond);	// free ap self
    }
}
