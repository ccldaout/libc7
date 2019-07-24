/*
 * c7bsearch.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


/*
 * c7bsearch.h
 *
 * [MACROS PREDEFINED BY USER SIDE]
 * 
 *  - TYPE NAME of element of array:
 *
 *    #define C7_ELM_TYPE	elm_t
 *
 *        typedef struct elm_t {
 *            uint32_t id;
 *            char name[32];
 *            ... other data ...
 *        } elm_t;
 *
 *  - KEY COMPARE operator:
 *
 *    #define C7_KEY_GT(keyaddr, keysize, elm)		((*(uint32_t*)(key)) > (elm)->id)
 *    #define C7_KEY_LT(keyaddr, keysize, elm)		((*(uint32_t*)(key)) < (elm)->id)
 *      or
 *    #define C7_KEY_COMP(keyaddr, keysize, elm)	strcmp((key), (elm)->name)
 *
 *  - select sort algorithm and define FUNCTION NAME:	 
 *
 *    #define C7_BSEARCH		XXX
 *
 *    C7_BSEARCH:
 *
 *       static C7_ELM_TYPE *XXX(C7_ELM_TYPE *left, ptrdiff_t n,
 *                            void *keyaddr, size_t keysize,
 *                            C7_ELM_TYPE **insert_pos);
 *
 *    In each case, some subroutines whose name is __C7_XXX_* are defined.
 *
 * - Optional MACROs
 *
 *   ...
 */

/*----------------------------------------------------------------------------
                   verify some macros to be pre-defined by user
----------------------------------------------------------------------------*/

#if !defined(C7_ELM_TYPE)
# error "C7_ELM_TYPE is not defined."
#endif


/*----------------------------------------------------------------------------
                             internal definition
----------------------------------------------------------------------------*/

#define __C7_SUBR_NAME_cat(n, s)	__C7_##n##_##s
#define __C7_SUBR_NAME(n, s)		__C7_SUBR_NAME_cat(n, s)
#define __C7_MAIN_NAME_cat(n, s)	n##_##s
#define __C7_MAIN_NAME(n, s)		__C7_SORT_NAME_cat(n, s)

#if defined(C7_BSEARCH)
# define __C7_TARGET_NAME		C7_BSEARCH
# define __C7_BSEARCH_MAIN		__C7_SUBR_NAME(__C7_TARGET_NAME, main)
# define __C7_BSEARCH			__C7_TARGET_NAME
#endif


/*----------------------------------------------------------------------------
                          binary search (find only)
----------------------------------------------------------------------------*/

#if defined(__C7_BSEARCH)

static C7_ELM_TYPE *__C7_BSEARCH(C7_ELM_TYPE *left, ptrdiff_t n,
					void *keyaddr, size_t keysize,
					C7_ELM_TYPE **insert_pos_if)
{
    C7_ELM_TYPE *right = left + n;
    while (left < right) {
	C7_ELM_TYPE *mid = left + ((right - left) / 2);
#if defined(C7_KEY_COMP)
	int cmp = C7_KEY_COMP(keyaddr, keysize, mid);
	if (cmp < 0) {
	    right = mid;
	} else if (cmp > 0) {
	    left = mid + 1;
	} else {
	    if (insert_pos_if != 0) {
		*insert_pos_if = mid;
	    }
	    return mid;
	}
#else
	if (C7_KEY_LT(keyaddr, keysize, mid)) {
	    right = mid;
	} else if (C7_KEY_GT(keyaddr, keysize, mid)) {
	    left = mid + 1;
	} else {
	    if (insert_pos_if != 0) {
		*insert_pos_if = mid;
	    }
	    return mid;
	}
#endif
    }
    if (insert_pos_if != 0) {
	*insert_pos_if = left;
    }
    return 0;
}

#endif


/*----------------------------------------------------------------------------
                                   cleanup
----------------------------------------------------------------------------*/

/* keep C7_ELM_TYPE */
/* keep C7_KEY_GT */
/* keep C7_KEY_LT */
/* keep C7_KEY_COMP */
#undef C7_BSEARCH

#undef __C7_TARGET_NAME
#undef __C7_SUBR_NAME_cat
#undef __C7_SUBR_NAME
#undef __C7_SORT_NAME_cat
#undef __C7_BSEARCH_MAIN
#undef __C7_BSEARCH


#if defined(__cplusplus)
}
#endif
/* c7bsearch */
