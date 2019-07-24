/*
 * c7mlog.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include <c7config.h>

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <c7args.h>
#include <c7file.h>
#include <c7mlog.h>
#include <c7app.h>


typedef struct prm_t_ {
    c7_args_t print_ap;
    size_t maxlines;
    size_t order_min, order_max;
    size_t tid_min, tid_max;	// thread id
    c7_time_t time_us_min, time_us_max;
    uint32_t level_max;		// C7_LOG_xxx
    uint32_t category_mask;
    const char *logname;
    c7_bool_t pr_category;
    c7_bool_t pr_loglevel;
    char *md_format;		// minidata format
    char *tm_format;
    int tn_width;		// max width of thread name
    int sn_width;		// width of source name
    int sl_width;		// width of source line
    c7_bool_t clear;
} prm_t;


/*----------------------------------------------------------------------------
                                 print option
----------------------------------------------------------------------------*/

static c7_bool_t option_category(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->pr_category = C7_TRUE;
    return C7_TRUE;
}

static c7_bool_t option_loglevel(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->pr_loglevel = C7_TRUE;
    return C7_TRUE;
}

static c7_bool_t option_minidata(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    free(prm->md_format);
    if (params->prmc == 0)
	prm->md_format = c7strdup("(%04lx)");
    else
	prm->md_format = c7strdup(params->prmv[0].u.s);
    return (prm->md_format != NULL);
}

static c7_bool_t option_thread_name(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    if (params->prmc == 0)
	prm->tn_width = 16;
    else
	prm->tn_width = params->prmv[0].u.i;
    return C7_TRUE;
}

static c7_bool_t option_tm_format(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    if (prm->tm_format != NULL)
	free(prm->tm_format);
    if ((prm->tm_format = c7strdup(params->prmv[0].u.s)) == NULL)
	return C7_FALSE;
    return C7_TRUE;
}

static c7_args_optdef_t PrintOptions[] = {
    {
	.short_opt = "c",
	.optrepr = "print category",
	.handler = option_category },
    {
	.short_opt = "g",
	.optrepr = "print log level",
	.handler = option_loglevel },
    {
	.long_opt = "mini",
	.short_opt = "m",
	.optrepr = "print minidata",
	.prmrepr = "format of printf [default: '(%04lx)']",
	.prmword = "INT64_FORMAT",
	.prmtype = C7_ARGS_T_ANY,
	.prmc_min = 0,
	.prmc_max = 1,
	.handler = option_minidata },
    {
	.long_opt = "thread",
	.short_opt = "t",
	.optrepr = "print thread name",
	.prmrepr = "width of thread name [default: 16]",
	.prmword = "WIDTH",
	.prmtype = C7_ARGS_T_INT,
	.prmc_min = 0,
	.prmc_max = 1,
	.handler = option_thread_name },
    {
	.long_opt = "date",
	.short_opt = "D",
	.optrepr = "date time format",
	.prmrepr = "format of strftime()",
	.prmword = "TIME_FORMAT",
	.prmtype = C7_ARGS_T_ANY,
	.prmc_min = 1,
	.prmc_max = 1,
	.handler = option_tm_format },
};


/*----------------------------------------------------------------------------
                                common option
----------------------------------------------------------------------------*/

static c7_bool_t option_max_line(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->maxlines = params->prmv[0].u.i;
    if (prm->maxlines == 0)
	prm->maxlines = -1UL;
    return C7_TRUE;
}

static c7_bool_t option_log_level(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->level_max = params->prmv[0].u.i;
    return C7_TRUE;
}

static c7_bool_t option_category_list(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    for (int i = 0; i < params->prmc; i++)
	prm->category_mask |= (1U << params->prmv[i].u.i);
    return C7_TRUE;
}

static c7_bool_t option_thread_range(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->tid_min = params->prmv[0].u.i;
    if (params->prmc == 2)
	prm->tid_max = params->prmv[1].u.i;
    else
	prm->tid_max = prm->tid_min;
    return C7_TRUE;
}

static c7_bool_t option_order_range(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->order_min = params->prmv[0].u.i;
    if (params->prmc == 2)
	prm->order_max = params->prmv[1].u.i;
    else
	prm->order_max = (size_t)-1;
    return C7_TRUE;
}

static c7_bool_t option_date_range(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->time_us_min = params->prmv[0].u.tm.v * C7_TIME_S_us;
   if (params->prmc == 2) {
	prm->time_us_max = params->prmv[1].u.tm.v * C7_TIME_S_us;
	if (params->prmv[1].u.tm.form == C7_ARGS_TIME_DOT)
	    prm->time_us_max += C7_TIME_S_us;
   } else {
       prm->time_us_max = prm->time_us_min + 60 * C7_TIME_S_us - 1;
   }

    return C7_TRUE;
}

static c7_bool_t option_clear(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    prm->clear = C7_TRUE;
    return C7_TRUE;
}

static c7_bool_t option_help(c7_args_t ap, const c7_args_params_t *params, void *__uctx)
{
    prm_t *prm = __uctx;
    c7_str_t *sbp = c7_str_new_ma();
    c7_sprintf(sbp,
	       "Usage: %s [common option ...] LOG_NAME [print option ...]\n\n"
	       " common option:\n\n",
	       c7progname());
    c7_args_usage(ap, sbp, 32);
    c7_strcpy(sbp, " print option:\n\n");
    c7_args_usage(prm->print_ap, sbp, 32);
    c7exit("%s\n", c7_strbuf(sbp));
    c7_str_free(sbp);
   return C7_TRUE;
}

static c7_args_optdef_t CommonOptions[] = {
    {
	.long_opt = "record",
	.short_opt = "r",
	.optrepr = "maximum count of record",
	.prmrepr = "count record",
	.prmword = "MAX_COUNT",
	.prmtype = C7_ARGS_T_INT,
	.prmc_min = 1,
	.prmc_max = 1,
	.handler = option_max_line },
    {
	.long_opt = "level",
	.short_opt = "g",
	.optrepr = "maximum log level",
	.prmrepr = "log level (0..7)",
	.prmword = "LOG_LEVEL",
	.prmtype = C7_ARGS_T_INT,
	.prmc_min = 1,
	.prmc_max = 1,
	.handler = option_log_level },
    {
	.long_opt = "category",
	.short_opt = "c",
	.optrepr = "print only specified category",
	.prmrepr = "category (0..31)",
	.prmword = "CATEGORY",
	.prmtype = C7_ARGS_T_INT,
	.prmc_min = 1,
	.prmc_max = -1,
	.handler = option_category_list },
    {
	.long_opt = "thread",
	.short_opt = "t",
	.optrepr = "print only specified thread",
	.prmrepr = "thread id",
	.prmword = "THREAD_ID",
	.prmtype = C7_ARGS_T_INT,
	.prmc_min = 1,
	.prmc_max = 1,
	.handler = option_thread_range },
    {
	.long_opt = "order",
	.short_opt = "s",
	.optrepr = "range of order",
	.prmrepr = "order of record",
	.prmword = "ORDER",
	.prmtype = C7_ARGS_T_INT,
	.prmc_min = 1,
	.prmc_max = 2,
	.handler = option_order_range },
    {
	.long_opt = "date",
	.short_opt = "d",
	.optrepr = "range of date time",
	.prmrepr = "timestamp: [[[YY]MMDD]hhmm][.ss] or '.'",
	.prmword = "DATETIME",
	.prmtype = C7_ARGS_T_DTV,
	.prmc_min = 1,
	.prmc_max = 2,
	.handler = option_date_range },
    {
	.long_opt = "clear",
	.optrepr = "clear contents after print",
	.handler = option_clear },
    {
	.long_opt = "help",
	.short_opt = "h",
	.optrepr = "this help",
	.handler = option_help },
};

static prm_t parse_args(char **argv)
{
    prm_t prm = {
	.maxlines = -1UL,
	.order_min = 0,
	.order_max = -1UL,
	.time_us_min = 0,
	.time_us_max = (-1UL)/2,
	.tid_min = 0,
	.tid_max = -1UL,
	.level_max = C7_LOG_BRF,
	.sn_width = 12,
	.sl_width = 3,
    };

    c7_args_t c_ap = c7_args_init();
    if (c_ap == NULL)
	c7exit_err(0, ": c7_args_init failed\n");
    if (!c7_args_add_list(c_ap, CommonOptions, c7_numberof(CommonOptions)))
	c7exit_err(0, NULL);	

    c7_args_t p_ap = c7_args_init();
    if (p_ap == NULL)
	c7exit_err(0, ": c7_args_init failed\n");
    if (!c7_args_add_list(p_ap, PrintOptions, c7_numberof(PrintOptions)))
	c7exit_err(0, NULL);	

    prm.print_ap = p_ap;
    if ((argv = c7_args_parse(c_ap, argv, &prm)) == NULL)
	c7exit_err(0, NULL);

    if (*argv == NULL)
	c7exit_err(0, ": LOGNAME is not specified.\n");
    prm.logname = *argv++;

    if ((argv = c7_args_parse(p_ap, argv, &prm)) == NULL)
	c7exit_err(0, NULL);
    
    if (*argv != NULL)
	c7exit_err(0, ": too many parameter: <%s>\n", *argv);

    if (prm.tm_format == NULL)
	prm.tm_format = "%m/%d %H:%M:%S";

    if (prm.category_mask == 0)
	prm.category_mask = -1U;

    c7_args_free(c_ap);
    c7_args_free(p_ap);
    return prm;
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

static char *strtime_us(c7_time_t tv_us, const char *tm_format)
{
    static char buff[100];
    time_t tv_s = tv_us / C7_TIME_S_us;
    tv_us %= C7_TIME_S_us;
    struct tm tm;
    (void)strftime(buff, sizeof(buff) - 8, tm_format, localtime_r(&tv_s, &tm));
    (void)sprintf(strchr(buff, 0), ".%06ld", tv_us);
    return buff;
}

static c7_bool_t choice(const c7_mlog_info_t *info, void *__param)
{
    prm_t *prm = __param;
    return (info->time_us >= prm->time_us_min &&
	    info->time_us <= prm->time_us_max &&
	    info->order >= prm->order_min &&
	    info->order <= prm->order_max &&
	    info->level <= prm->level_max &&
	    ((1U << info->category) & prm->category_mask) != 0 &&
	    info->thread_id >= prm->tid_min &&
	    info->thread_id <= prm->tid_max);
}

static c7_bool_t printlog(const c7_mlog_info_t *info, void *__data, void *__param)
{
    static c7_str_t sb = C7_STR_INIT_MA();
    prm_t *prm = __param;

    // make prefix
    c7_str_reuse(&sb);
    c7_sprintf(&sb, "%4d %s", 
	       info->order,
	       strtime_us(info->time_us, prm->tm_format));

    if (prm->tn_width > 0) {
	int n = strlen(info->thread_name);
	n = (n <= prm->tn_width) ? 0 : (n - prm->tn_width);
	c7_sprintf(&sb, " %*s", prm->tn_width, &info->thread_name[n]);
    } else
	c7_sprintf(&sb, " @%02x", info->thread_id);

    if (prm->sn_width > 0) {
	if (info->source_name[0] != 0) {
	    int n = strlen(info->source_name);
	    n = (n <= prm->sn_width) ? 0 : (n - prm->sn_width);
	    c7_sprintf(&sb, " %*s:%*d",
		       prm->sn_width, &info->source_name[n],
		       prm->sl_width, info->source_line);
	} else {
	    c7_sprintf(&sb, " %*s:%*s", prm->sn_width, "---", prm->sl_width, "---");
	}
    }

    if (prm->pr_category && prm->pr_loglevel)
	c7_sprintf(&sb, " [%2d:%1d]", info->category, info->level);
    else if (prm->pr_category)
	c7_sprintf(&sb, " [%2d]", info->category);
    else if (prm->pr_loglevel)
	c7_sprintf(&sb, " [%1d]", info->level);
    c7_stradd(&sb, ' ');

   if (prm->md_format)
	c7_sprintf(&sb, prm->md_format, info->minidata);

    c7_strcpy(&sb, ": ");

    // print data with prefix
    char *p, *s = __data;
    while ((p = strchr(s, '\n')) != NULL) {
	(void)printf("%s%.*s", c7_strbuf(&sb), (int)(p-s+1), s);
	s = p + 1;
    }
    if (*s != 0)
	(void)printf("%s%s\n", c7_strbuf(&sb), s);

    return C7_TRUE;
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

__attribute__((unused))
static void dumpprm(const prm_t *prm)
{
    c7echo("maxlines: %lu\n", prm->maxlines);
    c7echo("order_min: %lu, order_max: %lu\n", prm->order_min, prm->order_max);
    c7echo("time_us_min: %s (%ld)\n",
	   strtime_us(prm->time_us_min, "%Y %m/%d %H:%M:%S"), prm->time_us_min);
    c7echo("time_us_max: %s (%ld)\n",
	   strtime_us(prm->time_us_max, "%Y %m/%d %H:%M:%S"), prm->time_us_max);
    c7echo("logname: <%s>\n", prm->logname);
    c7echo("md_format: %s\n", prm->md_format ? prm->md_format : "(null)");
    c7echo("tm_format: <%s>\n", prm->tm_format);
}

int main(int argc, char **argv)
{
    c7_init(*argv++, 0);
    prm_t prm = parse_args(argv);

    c7_mlog_t g = c7_mlog_open_r(prm.logname);
    if (g == NULL)
	c7exit_err(0, NULL);
    if (!c7_mlog_scan(g,
		      prm.maxlines, prm.order_min, prm.time_us_min,
		      choice, printlog, &prm))
	c7exit_err(0, NULL);
    if (prm.clear)
	c7_mlog_clear(prm.logname);

    return 0;
}
