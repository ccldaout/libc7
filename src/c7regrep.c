/*
 * c7regrep.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <regex.h>
#include <c7regrep.h>
#include <c7status.h>
#include <c7app.h>

struct c7_regrep_t_ {
    c7_mgroup_t mg;
    regex_t reg;
    regmatch_t match[C7_REGREP_MATCH_CNT];
    char *rule;
    uint32_t flag;
};

c7_regrep_t c7_regrep_new(const char *regpattern,
			  const char *rule,
			  uint32_t regcomp_flag,
			  uint32_t flag)
{
    c7_mgroup_t mg = c7_mg_new();
    if (mg == NULL)
	return NULL;
    c7_regrep_t rr = c7_mg_malloc(mg, sizeof(*rr));
    if (rr != NULL) {
	if ((flag & C7_REGREP_OLDRULE) == 0 && (flag & C7_REGREP_EVAL_CBSS) != 0) {
	    c7_str_t *r = c7_streval_C(c7_str_new_mg(mg), rule);
	    rr->rule = C7_STR_OK(r) ? c7_strbuf(r) : NULL;
	} else
	    rr->rule = c7strdup_mg(mg, rule);
	if (rr->rule != NULL) {
	    int err;
	    regcomp_flag &= (~REG_NOSUB);
	    if ((err = regcomp(&rr->reg, regpattern, regcomp_flag)) == 0) {
		rr->mg = mg;
		rr->flag = flag;
		return rr;
	    }
	    char buff[256];
	    (void)regerror(err, &rr->reg, buff, sizeof(buff));
	    c7_status_add(EINVAL, ": regcomp failure: %s\n", buff);
	}
    }
    c7_mg_destroy(mg);
    return NULL;
}

static c7_str_t *replace_oldrule(c7_regrep_t rr,
				 const char * const in,
				 c7_str_t *out)
{
    char *p, *rule = rr->rule;
    while ((p = strchr(rule, '\\')) != NULL) {
	(void)c7_strbcpy(out, rule, p);
	if (p[1] == '\\') {
	    c7_stradd(out, '\\');
	    rule = p + 2;
	} else if (strchr("0123456789", p[1]) != NULL) {
	    regmatch_t *m = &rr->match[p[1] - '0'];
	    if (m->rm_so != -1)
		(void)c7_strbcpy(out, in + m->rm_so, in + m->rm_eo);
	    rule = p + 2;
	} else {
	    c7_stradd(out, '\\');
	    rule = p + 1;
	}
    }
    return c7_strcpy(out, rule);
}

typedef struct _translate_t {
    c7_regrep_t rr;
    const char *in;
} _translate_t;

static const char *translate_ref(c7_str_t *out, const char *ref,
				 c7_bool_t enclosed, void *__arg)
{
    c7_str_t *(*copy)(c7_str_t *, const char *, const char *) = c7_strbcpy;

    if (strchr("0123456789%", *ref) == NULL)
	return NULL;
    int refch = *ref++;

    if (enclosed) {
	if (strcmp(ref, ":L") == 0)
	    copy = c7_strblwr;
	else if (strcmp(ref, ":U") == 0)
	    copy = c7_strbupr;
	else if (*ref != 0)
	    return NULL;
	ref += 2;		// point to null character
    }

    if (refch == '%')
	(void)c7_stradd(out, refch);
    else {
	_translate_t *trans = __arg;
	regmatch_t *m = &trans->rr->match[refch - '0'];
	if (m->rm_so != -1)
	    (void)copy(out, trans->in + m->rm_so, trans->in + m->rm_eo);
    }
    return ref;
}

static c7_str_t *replace_newrule(c7_regrep_t rr,
				 const char * const in,
				 c7_str_t *out)
{
    _translate_t trans = { rr, in };
    return c7_streval_custom(out, rr->rule, '%', '\\', translate_ref, &trans);
}

c7_bool_t c7_regrep_exec(c7_regrep_t rr,
			 const char *in,
			 c7_str_t *out)

{
    c7_bool_t replaced = C7_FALSE;
    c7_bool_t ruleonly = ((rr->flag & C7_REGREP_RULEONLY) != 0);
    const char *replbeg, *replend;

    if (ruleonly)
	replbeg = replend = "\n";	// 'replbeg =' is for old gcc
    else if ((rr->flag & C7_REGREP_HIGHLIGHT) != 0) {
	replbeg = "\x1b[31m";
	replend = "\x1b[0m";
    } else
	replbeg = replend = "";

    c7_str_t *(*replacer)(c7_regrep_t, const char * const, c7_str_t *);
    if ((rr->flag & C7_REGREP_OLDRULE) != 0)
	replacer = replace_oldrule;
    else
	replacer = replace_newrule;

    for (;;) {
	if (regexec(&rr->reg, in, c7_numberof(rr->match), rr->match, 0) != C7_SYSOK) {
	    if (!ruleonly)
		(void)c7_strcpy(out, in);
	    return replaced;
	}

	replaced = C7_TRUE;
	if (!ruleonly)
	    (void)c7_strcpy(c7_strncpy(out, in, rr->match[0].rm_so), replbeg);
	(void)c7_strcpy(replacer(rr, in, out), replend);

	in += rr->match[0].rm_eo;
    }
}

void c7_regrep_free(c7_regrep_t rr)
{
    regfree(&rr->reg);
    c7_mg_destroy(rr->mg);
}
