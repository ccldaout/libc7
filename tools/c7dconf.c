/*
 * c7dconf.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include <stdio.h>
#include <stdlib.h>
#include <c7dconf.h>
#include <c7status.h>
#include <c7app.h>

// c7dconf NAME show
// c7dconf NAME set {IDENT|NUMBER} VALUE

static void exiterr(const char *fmt, ...)
{
    c7_str_t *err = c7_status_string(NULL);
    va_list ap;
    if (fmt[0] == ':')
	(void)fputs(c7progname(), stderr);
    va_start(ap, fmt);
    (void)vfprintf(stderr, fmt, ap);
    (void)fputs(c7_strbuf(err), stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void showusage(void)
{
    c7_status_clear();
    exiterr("Usage: %s NAME show\n"
	    "       %s NAME get {IDENT|INDEX}\n"
	    "       %s NAME set {IDENT|INDEX} VALUE [...]\n",
	    c7progname(), c7progname(), c7progname());
}

static void showdconf(int dc, c7_dconf_def_t * const def)
{
    int id_maxlen = 0;
    for (int i = 0; i < dc; i++) {
	c7_dconf_def_t *d = &def[i];
	if (id_maxlen < strlen(d->ident))
	    id_maxlen = strlen(d->ident);
    }
    for (int i = 0; i < dc; i++) {
	c7_dconf_def_t *d = &def[i];
	if (d->type == C7_DCONF_TYPE_I64) {
	    (void)printf("[%2d] %-*s : %15ld ... %s\n",
			 d->index, id_maxlen, d->ident, c7_dconf_i(d->index), d->descrip);
	} else {
	    (void)printf("[%2d] %-*s : %15g ... %s\n",
			 d->index, id_maxlen, d->ident, c7_dconf_r(d->index), d->descrip);
	}
    }
    exit(0);
}

static c7_dconf_def_t *getdef(int dc, c7_dconf_def_t * const def, char *s)
{
    char *p;
    int index = strtol(s, &p, 0);
    if (*p == 0) {
	for (int i = 0; i < dc; i++) {
	    c7_dconf_def_t *d = &def[i];
	    if (d->index == index)
		return d;
	}
    } else {
	for (int i = 0; i < dc; i++) {
	    c7_dconf_def_t *d = &def[i];
	    if (strcmp(d->ident, s) == 0)
		return d;
	}
    }
    return NULL;
}

static void getvalue(int dc, c7_dconf_def_t *def, char *s)
{
    def = getdef(dc, def, s);
    if (def == NULL)
	exiterr(": invalid IDENT or INDEX: '%s'\n", s);
    if (def->type == C7_DCONF_TYPE_I64)
	(void)printf("%ld", c7_dconf_i(def->index));
    else
	(void)printf("%g", c7_dconf_r(def->index));
    exit(0);
}

static void setvalues(int dc, c7_dconf_def_t * const def, char **argv)
{
    while (*argv) {
	c7_dconf_def_t *d = getdef(dc, def, *argv);
	if (d == NULL)
	    exiterr(": invalid IDENT or INDEX: '%s'\n", *argv);
	if (*++argv == NULL)
	    exiterr(": VALUE is required for IDENT(INDEX) '%s'\n", *--argv);
	if (d->type == C7_DCONF_TYPE_I64) {
	    char *p;
	    int64_t v = strtol(*argv, &p, 0);
	    if (*p != 0)
		exiterr(": integer is required for IDENT(INDEX) '%s'\n", *argv);
	    c7_dconf_i_set(d->index, v);
	} else {
	    char *p;
	    double v = strtod(*argv, &p);
	    if (*p != 0)
		exiterr(": real is required for IDENT(INDEX) '%s'\n", *argv);
	    c7_dconf_r_set(d->index, v);
	}
	argv++;
    }
    exit(0);
}

int main(int argc, char **argv)
{
    c7_init(*argv++, 0);

    if (argc <= 2 || strcmp(*argv, "-h") == 0 || strcmp(*argv, "--help") == 0)
	showusage();

    int dc;
    c7_dconf_def_t *def = c7_dconf_load(*argv, &dc);
    if (def == NULL)
	exiterr(": cannot load dconf '%s'\n", *argv);

    argv++;
    c7_status_clear();
    if (strcmp(*argv, "show") == 0) {
	if (argv[1] != NULL)
	    exiterr(": too many parameter(s): %s\n", argv[1]);
	showdconf(dc, def);
    } else if (strcmp(*argv, "get") == 0) {
	if (argv[1] == NULL || argv[2] != NULL)
	    exiterr(": 'get' command require just one parameter\n");
	getvalue(dc, def, argv[1]);
    } else if (strcmp(*argv, "set") == 0) {
	setvalues(dc, def, argv+1);
    }

    return 0;
}
