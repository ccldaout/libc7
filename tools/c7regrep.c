/*
 * c7regrep.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include <c7args.h>
#include <c7file.h>
#include <c7regrep.h>
#include <c7status.h>
#include <c7app.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/stat.h>


typedef enum _result_t_ {
    _RESULT_KEEP,
    _RESULT_REPLACE,
    _RESULT_ERROR,
} _result_t;

typedef struct prm_t {
    uint32_t regcomp_flag;
    uint32_t regrep_flag;
    const char *pattern;
    const char *rule;
    char **files;
    c7_bool_t progress;
    _result_t (*replace)(const char *, char **, struct prm_t *);
    c7_regrep_t regrep;
    regex_t *filter_regex;
} prm_t;


//----------------------------------------------------------------------------
//                                check mode
//----------------------------------------------------------------------------

static _result_t repl_checkonly(const char *source, char **lines, prm_t *prm)
{
    c7_bool_t putpath = C7_FALSE;
    c7_str_t *out = c7_str_new_sg();
    char ** const top = lines;

    for (; *lines; lines++) {
	c7_str_reuse(out);
	if (prm->filter_regex != NULL &&
	    regexec(prm->filter_regex, *lines, 0, NULL, 0) != C7_SYSOK)
	    continue;
	if (c7_regrep_exec(prm->regrep, *lines, out)) {
	    if (!putpath) {
		putpath = C7_TRUE;
		c7echo("%s ...\n", source);
	    }
	    c7echo("%7d %s", (int)(lines - top) + 1, c7_strbuf(out));
	}
    }

    return _RESULT_KEEP;
}


//----------------------------------------------------------------------------
//                                edit mode
//----------------------------------------------------------------------------

static _result_t replace_all(c7_str_t *out, char **lines, prm_t* prm)
{
    c7_bool_t replaced = C7_FALSE;
    for (; *lines; lines++) {
	if (prm->filter_regex != NULL &&
	    regexec(prm->filter_regex, *lines, 0, NULL, 0) != C7_SYSOK) {
	    c7_strcpy(out, *lines);
	    continue;
	}
	if (c7_regrep_exec(prm->regrep, *lines, out))
	    replaced = C7_TRUE;
    }
    if (C7_STR_OK(out)) 
	return replaced ? _RESULT_REPLACE : _RESULT_KEEP;
    return _RESULT_ERROR;
}

// inline (no backup)
static _result_t repl_inline(const char *source, char **lines, prm_t *prm)
{
    c7_str_t *out = c7_str_new_sg();
    _result_t res = replace_all(out, lines, prm);
    if (res == _RESULT_REPLACE) {
	if (c7_file_rewrite(source, c7_strbuf(out), C7_STR_LEN(out), NULL))
	    return _RESULT_REPLACE;
	res = _RESULT_ERROR;
    }
    return res;
}

// keep original
static _result_t repl_keep(const char *source, char **lines, prm_t *prm)
{
    c7_str_t *out = c7_str_new_sg();
    _result_t res = replace_all(out, lines, prm);
    if (res == _RESULT_REPLACE) {
	c7_str_t *path = c7_sprintf(NULL, "%s.rep", source);
	if (C7_STR_OK(path) &&
	    c7_file_write(c7_strbuf(path), 0644, c7_strbuf(out), C7_STR_LEN(out)))
	    return _RESULT_REPLACE;
	res = _RESULT_ERROR;
    }
    return res;
}

// copy original with other name
static _result_t repl_copy(const char *source, char **lines, prm_t *prm)
{
    c7_str_t *out = c7_str_new_sg();
    _result_t res = replace_all(out, lines, prm);
    if (res == _RESULT_REPLACE) {
	if (c7_file_rewrite(source, c7_strbuf(out), C7_STR_LEN(out), ".bck"))
	    return _RESULT_REPLACE;
	res = _RESULT_ERROR;
    }
    return res;
}

// print to stdout
static _result_t repl_stdout(const char *source, char **lines, prm_t *prm)
{
    c7_str_t *out = c7_str_new_sg();
    _result_t res = replace_all(out, lines, prm);
    if (res != _RESULT_ERROR) {
	(void)fputs(c7_strbuf(out), stdout);
	res = _RESULT_KEEP;
    }
    return res;
}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

static c7_bool_t option_inline(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->replace = repl_inline;
    prm->regrep_flag &= (~C7_REGREP_HIGHLIGHT);
    prm->progress = C7_TRUE;
    return C7_TRUE;
}

static c7_bool_t option_keep(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->replace = repl_keep;
    prm->regrep_flag &= (~C7_REGREP_HIGHLIGHT);
    prm->progress = C7_TRUE;
    return C7_TRUE;
}

static c7_bool_t option_copy(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->replace = repl_copy;
    prm->regrep_flag &= (~C7_REGREP_HIGHLIGHT);
    prm->progress = C7_TRUE;
    return C7_TRUE;
}

static c7_bool_t option_stdout(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->replace = repl_stdout;
    prm->regrep_flag &= (~C7_REGREP_HIGHLIGHT);
    return C7_TRUE;
}

static c7_bool_t option_extended(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->regcomp_flag |= REG_EXTENDED;
    return C7_TRUE;
}

static c7_bool_t option_highlight(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    if (params->prmc == 0 || params->prmv[0].keyindex == 1)
	prm->regrep_flag |= C7_REGREP_HIGHLIGHT;
    else // if (params->prmc == 1 && params->prmv[0].keyindex == 0)
	prm->regrep_flag &= (~C7_REGREP_HIGHLIGHT);
    return C7_TRUE;
}

static c7_bool_t option_eval_c_bss(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    if ((prm->regrep_flag & C7_REGREP_OLDRULE) != 0) {
	c7_status_reset(0, ": Conflicted with old-style RULE.\n");
	return C7_FALSE;
    }
    prm->regrep_flag |= C7_REGREP_EVAL_CBSS;
    return C7_TRUE;
}

static c7_bool_t option_oldrule(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    if ((prm->regrep_flag & C7_REGREP_EVAL_CBSS) != 0) {
	c7_status_reset(0, ": Conflicted with C backslash sequence translation.\n");
	return C7_FALSE;
    }
    prm->regrep_flag |= C7_REGREP_OLDRULE;
    return C7_TRUE;
}

static c7_bool_t option_ruleonly(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->regrep_flag |= C7_REGREP_RULEONLY;
    return C7_TRUE;
}

static c7_bool_t option_filter(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    static regex_t rex;
    int ret = regcomp(&rex, params->prmv[0].u.s, prm->regcomp_flag|REG_NOSUB);
    if (ret != C7_SYSOK) {
	char buff[128];
	(void)regerror(ret, &rex, buff, sizeof(buff));
	c7_status_reset(0, ": filter regexp '%s' compile failed.\n", params->prmv[0].u.s);
	return C7_FALSE;
    }
    prm->filter_regex = &rex;
    return C7_TRUE;
}

static c7_bool_t option_help(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    c7_str_t *sbp = c7_str_new_ma();
    c7_sprintf(sbp,
	       "Usage: %s [options ...] REG_PATTERN REPLACE_RULE [file ...]\n"
	       "\n"
	       "options:\n"
	       "\n",
	       c7progname());
    c7exit("%s", c7_strbuf(c7_args_usage(ap, sbp, 32)));
    c7_str_free(sbp);
   return C7_TRUE;
}

static c7_args_optdef_t OptionDefs[] = {
    {
	.long_opt = "inline",
	.short_opt = "i",
	.optrepr = "replace original without backup",
	.handler = option_inline },
    {
	.long_opt = "keep",
	.short_opt = "k",
	.optrepr = "keep original",
	.handler = option_keep },
    {
	.long_opt = "copy",
	.short_opt = "c",
	.optrepr = "copy original as other file",
	.handler = option_copy },
    {
	.long_opt = "stdout",
	.short_opt = "s",
	.optrepr = "print replaced contents",
	.handler = option_stdout },
    {
	.long_opt = "extended",
	.short_opt = "e",
	.optrepr = "extended regular expression",
	.handler = option_extended },
    {
	.long_opt = "highlight",
	.short_opt = "H",
	.optrepr = "highlight replaced string",
	.prmrepr = "highlight mode (default:color)",
	.prmword = "HIGHLIGHT",
	.prmtype = C7_ARGS_T_KEY,
	.conds = (char *[]) { "none", "color", NULL },
	.prmc_min = 0,
	.prmc_max = 1,
	.handler = option_highlight },
    {
	.long_opt = "translate-c-bss",
	.optrepr = "tranclate C backslash sequence",
	.handler = option_eval_c_bss },
    {
	.long_opt = "old-rule",
	.optrepr = "use old-style RULE",
	.handler = option_oldrule },
    {
	.long_opt = "rule-only",
	.optrepr = "print translated RULE only",
	.handler = option_ruleonly },
    {
	.long_opt = "filter-regexp",
	.short_opt = "f",
	.optrepr = "filter lines by additional regexp",
	.prmrepr = "regular expression",
	.prmword = "REGEXP",
	.prmtype = C7_ARGS_T_ANY,
	.prmc_min = 1,
	.prmc_max = 1,
	.handler = option_filter },
    {
	.long_opt = "help",
	.short_opt = "h",
	.optrepr = "this help",
	.handler = option_help },
};

static prm_t parse_args(char **argv)
{
    prm_t param = {0};
    c7_args_t ap = c7_args_init();
    param.replace = repl_checkonly;
    param.regrep_flag = C7_REGREP_HIGHLIGHT;
    param.progress = C7_FALSE;
    if (ap == NULL)
	c7exit_err(0, ": c7_args_init failed\n");
    if (!c7_args_add_list(ap, OptionDefs, c7_numberof(OptionDefs)))
	c7exit_err(0, NULL);	
    if ((argv = c7_args_parse(ap, argv, &param)) == NULL)
	c7exit_err(0, NULL);
    if ((param.regrep_flag & C7_REGREP_RULEONLY) != 0 &&
	param.replace != repl_stdout)
	c7exit(": rule-only option is availabe stdout mode\n");
    if ((param.pattern = *argv++) == NULL)
	c7exit(": REG_PATTERN is not specified.\n");
    if ((param.rule = *argv++) == NULL)
	c7exit(": REPLACE_RULE is not specified.\n");
    param.files = argv;
    if (*argv == NULL &&
	(param.replace == repl_checkonly || param.replace == repl_stdout)) {
	static char *argv[] = { "", NULL };
	param.files = argv;
    }
    c7_args_free(ap);
    return param;
}


//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

static c7_bool_t is_regular(const char *path)
{
    struct stat st;
    if (stat(path, &st) == C7_SYSOK)
	return S_ISREG(st.st_mode);
    c7echo_err(errno, ": %s\n", path);
    return C7_FALSE;
}

int main(int argc, char **argv)
{
    c7_init(*argv++, 0);
    prm_t prm = parse_args(argv);
    prm.regrep = c7_regrep_new(prm.pattern, prm.rule,
			       prm.regcomp_flag, prm.regrep_flag);
    if (prm.regrep == NULL)
	c7exit_err(0, NULL);

    for (argv = prm.files; *argv; argv++) {
	c7_status_clear();
	char **lines = c7_file_readlines_x(*argv, NULL);
	if (lines != NULL) {
	    c7_sg_push();
	    _result_t res = prm.replace(*argv, lines, &prm);
	    if (prm.progress && res != _RESULT_KEEP) {
		c7echo(": %s ... %s\n",
		       *argv, (res == _RESULT_REPLACE) ? "ok" : "ERROR");
	    }
	    c7_sg_pop();
	} else {
	    if (is_regular(*argv)) {
		c7echo_err(0, NULL);
	    }
	}
	free(lines);
    }
    return 0;
}
