/*
 * c7sortdef.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#if defined(__cplusplus)
extern "C" {
#endif


/*** DON'T INCLUDE c7config.h to BE USED UNDER C89 ***/
/***#include <c7config.h>***/


/*
 * c7sortdef.h
 *
 * [MACROS PREDEFINED BY USER SIDE]
 * 
 *  - TYPE NAME of element of array:
 *
 *    #define C7_ELM_TYPE	elm_t
 *
 *        typedef struct elm_t {
 *            uint32_t key;
 *            ... other data ...
 *        } elm_t;
 *
 *  - LESSTHAN operator:
 *
 *    #define C7_ELM_LT(p, q)		((p)->key < (q)->key)
 *
 *  - BIT TEST operator (in case of RADIX SORT):
 *
 *    #define C7_KEY_BIT_TEST(p, bitmask)	((p)->key & (bitmask))	
 *
 *  - select sort algorithm and define FUNCTION NAME:	 
 *
 *    #define C7_MSORT_MT	XXX
 *    #define C7_MSORT_ST	XXX
 *    #define C7_QSORT_MT	XXX
 *    #define C7_QSORT_ST	XXX
 *    #define C7_RSORT_MT	XXX
 *    #define C7_RSORT_ST	XXX
 *    #define C7_HSORT_ST	XXX
 *
 *    C7_MSORT_MT:
 *
 *       static void XXX(C7_ELM_TYPE *left, ptrdiff_t n, void *work, int thread_depth);
 *       static void XXX_st(C7_ELM_TYPE *left, ptrdiff_t n, void *work);	// single thread
 *
 *    C7_MSORT_ST:
 *
 *       static void XXX(C7_ELM_TYPE *left, ptrdiff_t n, void *work);
 *
 *    C7_QSORT_MT:
 *
 *       static void XXX(C7_ELM_TYPE *left, ptrdiff_t n, int thread_depth);
 *       static void XXX_st(C7_ELM_TYPE *left, ptrdiff_t n);		// single thread
 *       static void XXX_hs(C7_ELM_TYPE *left, ptrdiff_t n);		// heap sort
 *
 *    C7_QSORT_ST:
 *
 *       static void XXX(C7_ELM_TYPE *left, ptrdiff_t n);
 *       static void XXX_hs(C7_ELM_TYPE *left, ptrdiff_t n);		// heap sort
 *
 *    C7_RSORT_MT:
 *
 *       static void XXX(C7_ELM_TYPE *left, ptrdiff_t n, size_t keymask, int thread_depth);
 *       static void XXX_st(C7_ELM_TYPE *left, ptrdiff_t n, size_t keymask);
 *
 *    C7_RSORT_ST:
 *
 *       static void XXX(C7_ELM_TYPE *left, ptrdiff_t n, size_t keymask);
 *
 *    C7_HSORT_ST:
 *
 *       static void XXX(C7_ELM_TYPE *left, ptrdiff_t n);
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

#if !defined(C7_ELM_LT)
# error "C7_ELM_LT is not defined."
#endif

#if !defined(C7_KEY_BIT_TEST) && (defined(C7_RSORT_ST) || defined(C7_RSORT_MT))
# error "C7_KEY_BIT_TEST is required for radix sort."
#endif

#define __C7_NUMOF(a)	(sizeof(a)/sizeof((a)[0]))

#undef __C7_MM
#undef __C7_MS
#undef __C7_QM
#undef __C7_QS
#undef __C7_RS
#undef __C7_HS

#if defined(C7_MSORT_MT)
# define __C7_MM 1
#else
# define __C7_MM 0
#endif
#if defined(C7_MSORT_ST)
# define __C7_MS 1
#else
# define __C7_MS 0
#endif
#if defined(C7_QSORT_MT)
# define __C7_QM 1
#else
# define __C7_QM 0
#endif
#if defined(C7_QSORT_ST)
# define __C7_QS 1
#else
# define __C7_QS 0
#endif
#if defined(C7_RSORT_MT)
# define __C7_RM 1
#else
# define __C7_RM 0
#endif
#if defined(C7_RSORT_ST)
# define __C7_RS 1
#else
# define __C7_RS 0
#endif
#if defined(C7_HSORT_ST)
# define __C7_HS 1
#else
# define __C7_HS 0
#endif

#if (__C7_MM + __C7_MS + __C7_QM + __C7_QS + __C7_RS + __C7_RM + __C7_HS) != 1
# error "Please define JUST ONE sort algorithm."
#endif

#undef __C7_MM
#undef __C7_MS
#undef __C7_QM
#undef __C7_QS
#undef __C7_RM
#undef __C7_RS
#undef __C7_HS

/*----------------------------------------------------------------------------
                             internal definition
----------------------------------------------------------------------------*/

#define __C7_SUBR_NAME_cat(n, s)	__C7_##n##_##s
#define __C7_SUBR_NAME(n, s)		__C7_SUBR_NAME_cat(n, s)
#define __C7_SORT_NAME_cat(n, s)	n##_##s
#define __C7_SORT_NAME(n, s)		__C7_SORT_NAME_cat(n, s)

#if defined(C7_MSORT_MT)
# define __C7_TARGET_NAME	C7_MSORT_MT
# define __C7_MSORT_MERGE	__C7_SUBR_NAME(__C7_TARGET_NAME, merge)
# define __C7_MSORT_ST_MAIN	__C7_SUBR_NAME(__C7_TARGET_NAME, st_main)
# define __C7_MSORT_ST		__C7_SORT_NAME(__C7_TARGET_NAME, st)
# define __C7_MSORT_MERGE_ASC	__C7_SUBR_NAME(__C7_TARGET_NAME, merge_asc)
# define __C7_MSORT_MERGE_DSC	__C7_SUBR_NAME(__C7_TARGET_NAME, merge_dsc)
# define __C7_MSORT_MT_MAIN	__C7_SUBR_NAME(__C7_TARGET_NAME, main)
# define __C7_MSORT_MT		__C7_TARGET_NAME
#elif defined(C7_MSORT_ST)
# define __C7_TARGET_NAME	C7_MSORT_ST
# define __C7_MSORT_MERGE	__C7_SUBR_NAME(__C7_TARGET_NAME, merge)
# define __C7_MSORT_ST_MAIN	__C7_SUBR_NAME(__C7_TARGET_NAME, main)
# define __C7_MSORT_ST		__C7_TARGET_NAME
#elif defined(C7_QSORT_MT)
# define __C7_TARGET_NAME	C7_QSORT_MT
# define __C7_HSORT_UP_HEAP	__C7_SUBR_NAME(__C7_TARGET_NAME, hs_up_heap)
# define __C7_HSORT_DOWN_HEAP	__C7_SUBR_NAME(__C7_TARGET_NAME, hs_down_heap)
# define __C7_HSORT_ST		__C7_SORT_NAME(__C7_TARGET_NAME, hs)
# define __C7_QSORT_ST_MAIN	__C7_SUBR_NAME(__C7_TARGET_NAME, st_main)
# define __C7_QSORT_ST		__C7_SORT_NAME(__C7_TARGET_NAME, st)
# define __C7_QSORT_MT_MAIN	__C7_SUBR_NAME(__C7_TARGET_NAME, main)
# define __C7_QSORT_MT		__C7_TARGET_NAME
#elif defined(C7_QSORT_ST)
# define __C7_TARGET_NAME	C7_QSORT_ST
# define __C7_HSORT_UP_HEAP	__C7_SUBR_NAME(__C7_TARGET_NAME, hs_up_heap)
# define __C7_HSORT_DOWN_HEAP	__C7_SUBR_NAME(__C7_TARGET_NAME, hs_down_heap)
# define __C7_HSORT_ST		__C7_SORT_NAME(__C7_TARGET_NAME, hs)
# define __C7_QSORT_ST_MAIN	__C7_SUBR_NAME(__C7_TARGET_NAME, main)
# define __C7_QSORT_ST		__C7_TARGET_NAME
#elif defined(C7_RSORT_MT)
# define __C7_TARGET_NAME	C7_RSORT_MT
# define __C7_RSORT_ST_MAIN	__C7_SUBR_NAME(__C7_TARGET_NAME, st_main)
# define __C7_RSORT_ST		__C7_SORT_NAME(__C7_TARGET_NAME, st)
# define __C7_RSORT_MT_MAIN	__C7_SUBR_NAME(__C7_TARGET_NAME, main)
# define __C7_RSORT_MT		__C7_TARGET_NAME
#elif defined(C7_RSORT_ST)
# define __C7_TARGET_NAME	C7_RSORT_ST
# define __C7_RSORT_ST_MAIN	__C7_SUBR_NAME(__C7_TARGET_NAME, main)
# define __C7_RSORT_ST		__C7_TARGET_NAME
#elif defined(C7_HSORT_ST)
# define __C7_TARGET_NAME	C7_HSORT_ST
# define __C7_HSORT_UP_HEAP	__C7_SUBR_NAME(__C7_TARGET_NAME, up_heap)
# define __C7_HSORT_DOWN_HEAP	__C7_SUBR_NAME(__C7_TARGET_NAME, down_heap)
# define __C7_HSORT_ST		__C7_TARGET_NAME
#endif

#undef __C7_MSORT_THRESHOLD
#if defined(C7_MSORT_THRESHOLD)
# define __C7_MSORT_THRESHOLD	C7_MSORT_THRESHOLD
#else
# define __C7_MSORT_THRESHOLD	50
#endif

#undef __C7_MSORT_MAX_DEPTH
#if defined(C7_MSORT_MAX_DEPTH)
# define __C7_MSORT_MAX_DEPTH	C7_MSORT_MAX_DEPTH
#else
# define __C7_MSORT_MAX_DEPTH	64
#endif

#undef __C7_QSORT_THRESHOLD
#if defined(C7_QSORT_THRESHOLD)
# define __C7_QSORT_THRESHOLD	C7_QSORT_THRESHOLD
#else
# define __C7_QSORT_THRESHOLD	60
#endif

#undef __C7_QSORT_MAX_DEPTH
#if defined(C7_QSORT_MAX_DEPTH)
# define __C7_QSORT_MAX_DEPTH	C7_QSORT_MAX_DEPTH
#else
# define __C7_QSORT_MAX_DEPTH	48
#endif

#undef __C7_RSORT_THRESHOLD
#if defined(C7_RSORT_THRESHOLD)
# define __C7_RSORT_THRESHOLD	C7_RSORT_THRESHOLD
#else
# define __C7_RSORT_THRESHOLD	50
#endif

#if defined(__C7_MSORT_MT) || defined(__C7_QSORT_MT) || defined(__C7_RSORT_MT)
# include <pthread.h>
#endif

/* inline insert sort */
#undef __C7_ISORT
#define __C7_ISORT(p, q, left, right, tmp)				\
    do {								\
	for ((p)++; (p) <= (right); (p)++) {				\
	    if (C7_ELM_LT((p), ((p)-1))) {				\
		(tmp) = *(p);						\
		(q) = (p);						\
		do {							\
		    *(q) = *((q)-1);					\
		    (q)--;						\
		} while (((q) > (left)) && C7_ELM_LT(&(tmp), (q)-1));	\
		*(q) = (tmp);						\
	    }								\
	}								\
    } while (0)

/*----------------------------------------------------------------------------
                                  heap sort
----------------------------------------------------------------------------*/

#if defined(__C7_HSORT_ST)

#undef __C7_HSORT_LEFT_CHILD
#define __C7_HSORT_LEFT_CHILD(i)	((((i))<<1) + 1)
#undef __C7_HSORT_RIGHT_CHILD
#define __C7_HSORT_RIGHT_CHILD(i)	((((i))<<1) + 2)
#undef __C7_HSORT_PARENT
#define __C7_HSORT_PARENT(i)		(((i) - 1)>>1)

static void __C7_HSORT_UP_HEAP(C7_ELM_TYPE * const array, ptrdiff_t c)
{
    ptrdiff_t c_save = c;
    C7_ELM_TYPE target = array[c];
    while (c > 0) {
	ptrdiff_t p = __C7_HSORT_PARENT(c);
	if (C7_ELM_LT(&array[p], &target)) {
	    array[c] = array[p];
	} else {
	    break;
	}
	c = p;
    }
    if (c != c_save) {
	array[c] = target;
    }
}

static void __C7_HSORT_DOWN_HEAP(C7_ELM_TYPE * const array, ptrdiff_t n, C7_ELM_TYPE *target)
{
    ptrdiff_t p = 0;

    for (;;) {
	ptrdiff_t c = __C7_HSORT_LEFT_CHILD(p);
	if (c >= n) {
	    break;
	}
	if (((c + 1) < n) && C7_ELM_LT(&array[c], &array[c+1])) {
	    c = c + 1;
	}
	if (!C7_ELM_LT(target, &array[c])) {
	    break;
	}
	array[p] = array[c];
	p = c;
    }
    array[p] = *target;
}

static void __C7_HSORT_ST(C7_ELM_TYPE *left, ptrdiff_t n)
{
    ptrdiff_t i = 0;
    
    while (++i < n) {
	__C7_HSORT_UP_HEAP(left, i);
    }

    while (--i > 0) {
	C7_ELM_TYPE tmp = left[i];
	left[i] = left[0];
	__C7_HSORT_DOWN_HEAP(left, i, &tmp);
    }
}

#endif /* __C7_HSORT */

/*----------------------------------------------------------------------------
                         merage sort - single thread
----------------------------------------------------------------------------*/

#if defined(__C7_MSORT_ST)

static void __C7_MSORT_MERGE(C7_ELM_TYPE *o, C7_ELM_TYPE *p, C7_ELM_TYPE *q, C7_ELM_TYPE *eq)
{
    C7_ELM_TYPE *ep = q;
    for (;;) {
	if (C7_ELM_LT(p, q)) {
	    *o++ = *p++;
	    if (p == ep) {
		p = q;
		ep = eq;
		break;
	    }
	} else {
	    *o++ = *q++;
	    if (q == eq) {
		break;
	    }
	}
    }
    while (p < ep) {
	*o++ = *p++;
    }
}

static void __C7_MSORT_ST_MAIN(int parity, C7_ELM_TYPE *out, C7_ELM_TYPE *in, ptrdiff_t n)
{
    struct {
	enum { OP_SORT, OP_MERGE } op;
	int parity;
	C7_ELM_TYPE *out;
	C7_ELM_TYPE *in;
	ptrdiff_t n;
    } stack[__C7_MSORT_MAX_DEPTH * 2];
    int stack_idx = 0;
    int op = OP_SORT;

    for (;;) {
	ptrdiff_t h = n >> 1;

	if (op == OP_SORT) {
	    if ((n < __C7_MSORT_THRESHOLD) && (parity == 0)) {
		C7_ELM_TYPE *p = out;
		C7_ELM_TYPE *right = out + (n - 1);
		C7_ELM_TYPE tmp;
		/* p:out, q:in, left:out, right:right, tmp:tmp */
		__C7_ISORT(p, in, out, right, tmp);
		if (stack_idx == 0) {
		    break;
		}
		stack_idx--;
		op = stack[stack_idx].op;
		parity = stack[stack_idx].parity;
		out = stack[stack_idx].out;
		in = stack[stack_idx].in;
		n = stack[stack_idx].n;
	    } else {
		C7_ELM_TYPE *tmp;
		if ((stack_idx + 2) > __C7_NUMOF(stack)) {
		    abort();
		}
		stack[stack_idx].op = OP_MERGE;
		stack[stack_idx].parity = parity;
		stack[stack_idx].out = out;
		stack[stack_idx].in = in;
		stack[stack_idx].n = n;
		stack_idx++;
		stack[stack_idx].op = OP_SORT;
		stack[stack_idx].parity = !parity;
		stack[stack_idx].out = in + h;
		stack[stack_idx].in = out + h;
		stack[stack_idx].n = n - h;
		stack_idx++;

		/* continue sort left half */
		parity = !parity;
		tmp = out, out = in, in = tmp;
		n = h;
	    }
	} else {
	    __C7_MSORT_MERGE(out, in, in+h, in+n);
	    if (stack_idx == 0) {
		break;
	    }
	    stack_idx--;
	    op = stack[stack_idx].op;
	    parity = stack[stack_idx].parity;
	    out = stack[stack_idx].out;
	    in = stack[stack_idx].in;
	    n = stack[stack_idx].n;
	}

    }
}

static void __C7_MSORT_ST(C7_ELM_TYPE *left, ptrdiff_t n, void *work)
{
    __C7_MSORT_ST_MAIN(0, left, work, n);
}

#endif /* __C7_MSORT_ST */


/*----------------------------------------------------------------------------
                          merage sort - multi thread
----------------------------------------------------------------------------*/

#if defined(__C7_MSORT_MT)

#if !defined(__C7_MSORT_MT_PARAM_TYPE)
# define __C7_MSORT_MT_PARAM_TYPE
typedef struct __C7_msort_mt_param_t_ {
    pthread_t thread;
    void *out;
    void *in;
    ptrdiff_t n;
    ptrdiff_t h;
    int parity;
    int level;
} __C7_msort_mt_param_t;
#endif

static void *__C7_MSORT_MERGE_ASC(void *__C7_ms)
{
    const __C7_msort_mt_param_t * const ms = __C7_ms;
    C7_ELM_TYPE *o = ms->out;
    C7_ELM_TYPE *eo = o + ms->h;
    C7_ELM_TYPE *p = ms->in;
    C7_ELM_TYPE *q = p + ms->h;

    while (o != eo) {
	if (!C7_ELM_LT(q, p)) {		/* !(q < p) <=> p <= q */
	    *o++ = *p++;
	} else {
	    *o++ = *q++;
	}
    }
    return 0;
}

static void *__C7_MSORT_MERGE_DSC(void *__C7_ms)
{
    const __C7_msort_mt_param_t * const ms = __C7_ms;
    C7_ELM_TYPE *o = (C7_ELM_TYPE *)ms->out + (ms->n - 1);
    C7_ELM_TYPE *bo = (C7_ELM_TYPE *)ms->out + ms->h;
    C7_ELM_TYPE *p = (C7_ELM_TYPE *)ms->in + (ms->h - 1);
    C7_ELM_TYPE *q = (C7_ELM_TYPE *)ms->in + (ms->n - 1);

    while (o != bo) {
	if (!C7_ELM_LT(q, p)) {			/* !(q < p) <=> p <= q */
	    *o-- = *q--;
	} else {
	    *o-- = *p--;
	}
    }
    if ((p == (ms->in - 1)) || C7_ELM_LT(p, q)) {
	*o = *q;
    } else {
	*o = *p;
    }
    return 0;
}

static void *__C7_MSORT_MT_MAIN(void *__C7_ms)
{
    __C7_msort_mt_param_t ms1, ms2;
    const __C7_msort_mt_param_t * const ms = __C7_ms;

    if ((ms->level == 0) || (ms->n < __C7_MSORT_THRESHOLD)) {
	__C7_MSORT_ST_MAIN(ms->parity, ms->out, ms->in, ms->n);
	return 0;
    }

    ms1.out = ms->in;
    ms1.in = ms->out;
    ms1.n = ms->h;
    ms1.h = ms1.n >> 1;
    ms1.parity = !ms->parity;
    ms1.level = ms->level - 1;
    ms2.out = (C7_ELM_TYPE *)ms->in + ms->h;
    ms2.in = (C7_ELM_TYPE *)ms->out + ms->h;
    ms2.n = ms->n - ms->h;
    ms2.h = ms2.n >> 1;
    ms2.parity = !ms->parity;
    ms2.level = ms->level - 1;

    if (pthread_create(&ms1.thread, 0, __C7_MSORT_MT_MAIN, &ms1) == 0) {
	if (pthread_create(&ms2.thread, 0, __C7_MSORT_MT_MAIN, &ms2) == 0) {
	    (void)pthread_join(ms1.thread, 0);
	    (void)pthread_join(ms2.thread, 0);
	    /* ms1 is reused for __C7_MSORT_MERGE_* */
	    ms1 = *ms;
	    if (pthread_create(&ms1.thread, 0, __C7_MSORT_MERGE_ASC, &ms1) == 0) {
		(void)__C7_MSORT_MERGE_DSC(&ms1);
		(void)pthread_join(ms1.thread, 0);
	    } else {
		__C7_MSORT_MERGE(ms->out, ms->in,
			      (C7_ELM_TYPE *)ms->in + ms->h, (C7_ELM_TYPE *)ms->in + ms->n);
	    }
	    return 0;
	} else {
	    __C7_MSORT_ST_MAIN(ms2.parity, ms2.out, ms2.in, ms2.n);
	}
	(void)pthread_join(ms1.thread, 0);
	__C7_MSORT_MERGE(ms->out, ms->in,
		      (C7_ELM_TYPE *)ms->in + ms->h, (C7_ELM_TYPE *)ms->in + ms->n);
	return 0;
    } else {
	__C7_MSORT_ST_MAIN(ms->parity, ms->out, ms->in, ms->n);
    }
    return 0;
}

static void __C7_MSORT_MT(C7_ELM_TYPE *left, ptrdiff_t n, void *work, int thread_depth)
{
    __C7_msort_mt_param_t ms;
    ms.out = left;
    ms.in = work;
    ms.n = n;
    ms.h = ms.n >> 1;
    ms.parity = 0;
    ms.level = thread_depth;
    (void)__C7_MSORT_MT_MAIN(&ms);
}

#endif /* __C7_MSORT_MT */

/*----------------------------------------------------------------------------
                          quick sort - single thread
----------------------------------------------------------------------------*/

#if defined(__C7_QSORT_ST)

static void __C7_QSORT_ST_MAIN(C7_ELM_TYPE *left, C7_ELM_TYPE *right)
{
    struct {
	C7_ELM_TYPE *left;
	C7_ELM_TYPE *right;
    } stack[__C7_QSORT_MAX_DEPTH];
    int stack_idx = 0;

    for (;;) {
	C7_ELM_TYPE *p = left;
	C7_ELM_TYPE *q = right;
	C7_ELM_TYPE s;
	C7_ELM_TYPE tmp;
	C7_ELM_TYPE *v[3];

	if ((right - left) < __C7_QSORT_THRESHOLD) {
	    __C7_ISORT(p, q, left, right, tmp);
	    if (stack_idx == 0) {
		break;
	    }
	    stack_idx--;
	    left = stack[stack_idx].left;
	    right = stack[stack_idx].right;
	    continue;
	}

	/* q - p > __C7_QSORT_THRESHOLD */

	v[0] = &p[       1];
	v[1] = &p[(q-p)>>1];
	v[2] = &q[      -1];

	if (C7_ELM_LT(v[0], v[1])) {
	    if (C7_ELM_LT(v[1], v[2])) {
		s = *v[1];
	    } else if (C7_ELM_LT(v[0], v[2])) {
		s = *v[2];
	    } else {
		s = *v[0];
	    }
	} else {
	    if (C7_ELM_LT(v[0], v[2])) {
		s = *v[0];
	    } else if (C7_ELM_LT(v[1], v[2])) {
		s = *v[2];
	    } else {
		s = *v[1];
	    }
	}

	do {
	    while (C7_ELM_LT(p, &s)) {
		p++;
	    }
	    while (C7_ELM_LT(&s, q)) {
		q--;
	    }
	    if (p <= q) {
		tmp = *p;
		*p++ = *q;
		*q-- = tmp;
	    }
	} while (p < q);

	if (stack_idx == __C7_NUMOF(stack)) {
	    __C7_HSORT_ST(left, (q - left) + 1);
	    __C7_HSORT_ST(p,    (right - p) + 1);
	    stack_idx--;
	    left = stack[stack_idx].left;
	    right = stack[stack_idx].right;
	} else {
	    stack[stack_idx].left = p;
	    stack[stack_idx].right = right;
	    stack_idx++;
	    right = q;
	}
    }
}

static void __C7_QSORT_ST(C7_ELM_TYPE *left, ptrdiff_t n)
{
    __C7_QSORT_ST_MAIN(left, (left + n) -1);
}

#endif /* __C7_QSORT_ST */

/*----------------------------------------------------------------------------
                          quick sort - multi thread
----------------------------------------------------------------------------*/

#if defined(__C7_QSORT_MT)

#if !defined(__C7_QSORT_MT_PARAM_TYPE)
# define __C7_QSORT_MT_PARAM_TYPE
typedef struct __C7_qsort_mt_param_t_ {
    pthread_t thread;
    void *left;
    void *right;
    int level;
} __C7_qsort_mt_param_t;
#endif

static void *__C7_QSORT_MT_MAIN(void *__C7_qs)
{
    __C7_qsort_mt_param_t qs1, qs2;
    const __C7_qsort_mt_param_t * const qs = __C7_qs;
    C7_ELM_TYPE *p = qs->left;
    C7_ELM_TYPE *q = qs->right;
    C7_ELM_TYPE s;
    C7_ELM_TYPE tmp;
    C7_ELM_TYPE *v[3];

    if ((qs->level == 0) || ((q - p) < __C7_QSORT_THRESHOLD)) {
	__C7_QSORT_ST_MAIN(p, q);
	return 0;
    }

    /* q - p > __C7_QSORT_THRESHOLD */

    v[0] = &p[       1];
    v[1] = &p[(q-p)>>1];
    v[2] = &q[      -1];

    if (C7_ELM_LT(v[0], v[1])) {
	if (C7_ELM_LT(v[1], v[2])) {
	    s = *v[1];
	} else if (C7_ELM_LT(v[0], v[2])) {
	    s = *v[2];
	} else {
	    s = *v[0];
	}
    } else {
	if (C7_ELM_LT(v[0], v[2])) {
	    s = *v[0];
	} else if (C7_ELM_LT(v[1], v[2])) {
	    s = *v[2];
	} else {
	    s = *v[1];
	}
    }

    do {
	while (C7_ELM_LT(p, &s)) {
	    p++;
	}
	while (C7_ELM_LT(&s, q)) {
	    q--;
	}
	if (p <= q) {
	    tmp = *p;
	    *p++ = *q;
	    *q-- = tmp;
	}
    } while (p < q);

    if (qs->left < (void *)q) {
	qs1.left = qs->left;
	qs1.right = q;
	qs1.level = qs->level - 1;
	if (pthread_create(&qs1.thread, 0, __C7_QSORT_MT_MAIN, &qs1) != 0) {
	    __C7_QSORT_ST_MAIN(qs1.left, qs1.right);
	    q = qs->left;	/* skip pthread_join */
	}
    }
    if ((void *)p < qs->right) {
	qs2.left = p;
	qs2.right = qs->right;
	qs2.level = qs->level - 1;
	if (pthread_create(&qs2.thread, 0, __C7_QSORT_MT_MAIN, &qs2) != 0) {
	    __C7_QSORT_ST_MAIN(qs2.left, qs2.right);
	    p = qs->right;	/* skip pthread_join */
	}
    }
    if (qs->left < (void *)q) {
	(void)pthread_join(qs1.thread, 0);
    }
    if ((void *)p < qs->right) {
	(void)pthread_join(qs2.thread, 0);
    }

    return 0;
}

static void __C7_QSORT_MT(C7_ELM_TYPE *left, ptrdiff_t n, int thread_depth)
{
    __C7_qsort_mt_param_t qs;
    qs.left = left;
    qs.right = left + (n - 1);
    qs.level = thread_depth;
    __C7_QSORT_MT_MAIN(&qs);
}

#endif /* __C7_QSORT_MT */

/*----------------------------------------------------------------------------
                          radix sort - single thread
----------------------------------------------------------------------------*/

#if defined(__C7_RSORT_ST)

static void __C7_RSORT_ST_MAIN(C7_ELM_TYPE *left, C7_ELM_TYPE *right, size_t keymask, size_t bitmask)
{
    struct {
	C7_ELM_TYPE *left;
	C7_ELM_TYPE *right;
	size_t bitmask;
    } stack[sizeof(size_t) * 8];
    int stack_idx = 0;

    /* find start bit */
    for (; (bitmask |= 0) && ((keymask & bitmask) == 0); bitmask >>= 1) {;}
    if (bitmask == 0) {
	return;
    }

    for (;;) {
	C7_ELM_TYPE *p = left;
	C7_ELM_TYPE *q = right;
	C7_ELM_TYPE tmp;

	if ((right - left) < __C7_RSORT_THRESHOLD) {
	    __C7_ISORT(p, q, left, right, tmp);
	    if (stack_idx == 0) {
		return;
	    }
	    /* pop */
	    stack_idx--;
	    left = stack[stack_idx].left;
	    right = stack[stack_idx].right;
	    bitmask = stack[stack_idx].bitmask;
	    continue;
	}

	while ((C7_KEY_BIT_TEST(p, bitmask) == 0) && (p <= q)) {
	    p++;
	}
	while ((C7_KEY_BIT_TEST(q, bitmask) != 0) && (p <= q)) {
	    q--;
	}
	while (p < q) {
	    tmp = *p, *p = *q, *q = tmp;
	    p++;
	    q--;
	    while (C7_KEY_BIT_TEST(p, bitmask) == 0) {
		p++;
	    }
	    while (C7_KEY_BIT_TEST(q, bitmask) != 0) {
		q--;
	    }
	}

	/* find next lower bit */
	for (bitmask >>= 1; (bitmask |= 0) && ((keymask & bitmask) == 0); bitmask >>= 1) {;}

	if (bitmask == 0) {
	    if (stack_idx == 0) {
		return;
	    }
	    /* pop */
	    stack_idx--;
	    left = stack[stack_idx].left;
	    right = stack[stack_idx].right;
	    bitmask = stack[stack_idx].bitmask;
	} else {
	    /* push sort p..right */
	    stack[stack_idx].left = p;
	    stack[stack_idx].right = right;
	    stack[stack_idx].bitmask = bitmask;
	    stack_idx++;
	    /* sort left..q */
	    right = q;
	}
    }
}

static void __C7_RSORT_ST(C7_ELM_TYPE *left, ptrdiff_t n, size_t keymask)
{
    __C7_RSORT_ST_MAIN(left, (left + n) -1, keymask, (1UL<<63));
}

#endif /* __C7_RSORT_ST */

/*----------------------------------------------------------------------------
                          radix sort - multi thread
----------------------------------------------------------------------------*/

#if defined(__C7_RSORT_MT)

#if !defined(__C7_RSORT_MT_PARAM_TYPE)
# define __C7_RSORT_MT_PARAM_TYPE
typedef struct __C7_rsort_mt_param_t_ {
    pthread_t thread;
    void *left;
    void *right;
    size_t keymask;
    size_t bitmask;
    int level;
} __C7_rsort_mt_param_t;
#endif

static void *__C7_RSORT_MT_MAIN(void *__rs)
{
    __C7_rsort_mt_param_t rs1, rs2;
    const __C7_rsort_mt_param_t * const rs = __rs;
    C7_ELM_TYPE *p = rs->left;
    C7_ELM_TYPE *q = rs->right;
    C7_ELM_TYPE tmp;
    size_t keymask = rs->keymask;
    size_t bitmask = rs->bitmask;

    if ((rs->level == 0) || ((q - p) < __C7_RSORT_THRESHOLD)) {
	__C7_RSORT_ST_MAIN(p, q, rs->keymask, rs->bitmask);
	return 0;
    }

    bitmask = rs->bitmask;

    while ((C7_KEY_BIT_TEST(p, bitmask) == 0) && (p <= q)) {
	p++;
    }
    while ((C7_KEY_BIT_TEST(q, bitmask) != 0) && (p <= q)) {
	q--;
    }
    while (p < q) {
	tmp = *p, *p = *q, *q = tmp;
	p++;
	q--;
	while (C7_KEY_BIT_TEST(p, bitmask) == 0) {
	    p++;
	}
	while (C7_KEY_BIT_TEST(q, bitmask) != 0) {
	    q--;
	}
    }

    /* find next lower bit */
    for (bitmask >>= 1; (bitmask |= 0) && ((keymask & bitmask) == 0); bitmask >>= 1) {;}

    if (bitmask == 0) {
	return 0;
    }

    if (rs->left < (void *)q) {
	rs1.left = rs->left;
	rs1.right = q;
	rs1.level = rs->level - 1;
	rs1.keymask = keymask;
	rs1.bitmask = bitmask;
	if (pthread_create(&rs1.thread, 0, __C7_RSORT_MT_MAIN, &rs1) != 0) {
	    __C7_RSORT_ST_MAIN(rs1.left, rs1.right, keymask, bitmask);
	    q = rs->left;	/* skip pthread_join */
	}
    }
    if ((void *)p < rs->right) {
	rs2.left = p;
	rs2.right = rs->right;
	rs2.level = rs->level - 1;
	rs2.keymask = keymask;
	rs2.bitmask = bitmask;
	if (pthread_create(&rs2.thread, 0, __C7_RSORT_MT_MAIN, &rs2) != 0) {
	    __C7_RSORT_ST_MAIN(rs2.left, rs2.right, keymask, bitmask);
	    p = rs->right;	/* skip pthread_join */
	}
    }
    if (rs->left < (void *)q) {
	(void)pthread_join(rs1.thread, 0);
    }
    if ((void *)p < rs->right) {
	(void)pthread_join(rs2.thread, 0);
    }

    return 0;
}

static void __C7_RSORT_MT(C7_ELM_TYPE *left, ptrdiff_t n, size_t keymask, int thread_depth)
{
    __C7_rsort_mt_param_t rs;
    size_t bitmask = (1UL<<63);
    /* find start bit */
    for (; (bitmask |= 0) && ((keymask & bitmask) == 0); bitmask >>= 1) {;}
    if (bitmask != 0) {
	rs.left = left;
	rs.right = left + (n - 1);
	rs.level = thread_depth;
	rs.keymask = keymask;
	rs.bitmask = bitmask;
    }
    __C7_RSORT_MT_MAIN(&rs);
}

#endif /* __C7_RSORT_MT */

/*----------------------------------------------------------------------------
                                   cleanup
----------------------------------------------------------------------------*/

/* keep C7_ELM_TYPE */
/* keep C7_ELM_LT */
#undef C7_MSORT_ST
#undef C7_MSORT_MT
#undef C7_QSORT_ST
#undef C7_QSORT_MT
#undef C7_RSORT_ST
#undef C7_RSORT_MT
#undef C7_HSORT_ST
/* keep C7_MSORT_THRESHOLD */
/* keep C7_MSORT_MAX_DEPTH */
/* keep C7_QSORT_THRESHOLD */
/* keep C7_QSORT_MAX_DEPTH */
/* keep C7_RSORT_THRESHOLD */

#undef __C7_NUMOF
#undef __C7_MSORT_THRESHOLD
#undef __C7_MSORT_MAX_DEPTH
#undef __C7_QSORT_THRESHOLD
#undef __C7_QSORT_MAX_DEPTH
#undef __C7_RSORT_THRESHOLD
#undef __C7_TARGET_NAME
#undef __C7_SUBR_NAME_cat
#undef __C7_SUBR_NAME
#undef __C7_SORT_NAME_cat
#undef __C7_SORT_NAME
#undef __C7_HSORT_UP_HEAP
#undef __C7_HSORT_DOWN_HEAP
#undef __C7_HSORT_ST
#undef __C7_MSORT_MERGE
#undef __C7_MSORT_ST_MAIN
#undef __C7_MSORT_ST
#undef __C7_MSORT_MERGE_ASC
#undef __C7_MSORT_MERGE_DSC
#undef __C7_MSORT_MT_MAIN
#undef __C7_MSORT_MT
#undef __C7_QSORT_ST_MAIN
#undef __C7_QSORT_ST
#undef __C7_QSORT_MT_MAIN
#undef __C7_QSORT_MT
#undef __C7_RSORT_ST_MAIN
#undef __C7_RSORT_ST
#undef __C7_RSORT_MT_MAIN
#undef __C7_RSORT_MT
#undef __C7_ISORT
#undef __C7_HSORT_LEFT_CHILD
#undef __C7_HSORT_RIGHT_CHILD
#undef __C7_HSORT_PARENT
/* Don't undefine __C7_MSORT_MT_PARAM_TYPE */
/* Don't undefine __C7_QSORT_MT_PARAM_TYPE */
/* Don't undefine __C7_RSORT_MT_PARAM_TYPE */


#if defined(__cplusplus)
}
#endif
/* c7sortdef.h */
