/*
 * c7regrep.h
 *
 * https://ccldaout.github.io/libc7/group__c7regrep.html
 *p
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_REGREP_H_LOADED__
#define __C7_REGREP_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>

#include <c7string.h>

typedef struct c7_regrep_t_ *c7_regrep_t;

#define C7_REGREP_MATCH_CNT	10

#define C7_REGREP_HIGHLIGHT	(1U<<0)
#define C7_REGREP_EVAL_CBSS	(1U<<1)		// C backslash sequence
#define C7_REGREP_OLDRULE	(1U<<2)
#define C7_REGREP_RULEONLY	(1U<<3)

c7_regrep_t c7_regrep_new(const char *regpattern,
			  const char *rule,
			  uint32_t regcomp_flag,
			  uint32_t flag);

c7_bool_t c7_regrep_exec(c7_regrep_t rr,
			 const char *in,
			 c7_str_t *out);

void c7_regrep_free(c7_regrep_t rr);


#if defined(__cplusplus)
}
#endif
#endif // __C7_REGREP_H_LOADED__
