/*
 * c7heapdef.h
 *
 * https://ccldaout.github.io/libc7/group__c7heapdef.html
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
 * c7heapdef.h
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
 *    #define C7_ELM_LT(p, q)	((p)->key < (q)->key)
 *
 *  - NAME of heap structure:	 
 *
 *    #define C7_HEAP_NAME			XXX
 *
 * [POSTDEFINED NAMES]
 *
 *	XXX_base_t:
 *      	typedef struct XXX_base_t { ... } XXX_base_t;
 *
 *	c7_heap_init:
 *		static C7_ELM_TYPE HeapStorage[...];
 *		static XXX_base_t HeapBase = c7_heap_init(HeapStorage);
 *
 *	void c7_heap_setarray(XXX_base_t *base, void *storage);
 *
 *	inttype c7_heap_parent(inttype index);
 *	inttype c7_heap_left(inttype index);
 *      inttype c7_heap_right(inttype index);
 *
 *	size_t c7_heap_count(XXX_base_t *base);
 *	C7_ELM_TYPE *c7_heap_top(XXX_base_t *base);
 *	C7_ELM_TYPE *c7_heap_Nth(XXX_base_t *base, inttype index);
 *	ptrdiff_t c7_heap_index(XXX_base_t *base, C7_ELM_TYPE *element);
 *	void c7_heap_reset(XXX_base_t *base);
 *
 *	static void XXX_add(XXX_base_t *base, C7_ELM_TYPE *elm);
 *	static void XXX_remove(XXX_base_t *base, size_t idx);
 *
 *    In addition, some subroutines whose name is __C7_XXX_* are defined.
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

#if !defined(C7_HEAP_NAME)
# error "C7_HEAP_NAME is not defined."
#endif


/*----------------------------------------------------------------------------
                             internal definition
----------------------------------------------------------------------------*/

#define __C7_PRIVATE_NAME_cat(n, s)	__C7_##n##_##s
#define __C7_PRIVATE_NAME(n, s)		__C7_PRIVATE_NAME_cat(n, s)
#define __C7_PUBLIC_NAME_cat(n, s)	n##_##s
#define __C7_PUBLIC_NAME(n, s)		__C7_PUBLIC_NAME_cat(n, s)

#if defined(C7_HEAP_NAME)
# define __C7_TARGET_NAME	C7_HEAP_NAME
# define __C7_HEAP_BASE_TAG	__C7_PUBLIC_NAME(__C7_TARGET_NAME, base_t_)
# define __C7_HEAP_BASE_TYPE	__C7_PUBLIC_NAME(__C7_TARGET_NAME, base_t)
# define __C7_HEAP_SHIFT_UP	__C7_PRIVATE_NAME(__C7_TARGET_NAME, _shift_up)
# define __C7_HEAP_SHIFT_DOWN	__C7_PRIVATE_NAME(__C7_TARGET_NAME, _shift_down)
# define __C7_HEAP_ADD		__C7_PUBLIC_NAME(__C7_TARGET_NAME, add)
# define __C7_HEAP_REMOVE	__C7_PUBLIC_NAME(__C7_TARGET_NAME, remove)
# define __C7_HEAP_VERIFY	__C7_PUBLIC_NAME(__C7_TARGET_NAME, verify)
#endif


/*----------------------------------------------------------------------------
                             internal definition
----------------------------------------------------------------------------*/

typedef struct __C7_HEAP_BASE_TAG {
    C7_ELM_TYPE *_a;	/* head of array */
    size_t _n;	/* number of contained elements (not size of array) */
} __C7_HEAP_BASE_TYPE;

#define c7_heap_init(a)		{ (a), 0 }
#define c7_heap_setarray(b, a)	((b)->_a = (a))
#define c7_heap_parent(i)	(((i) - 1) >> 1)
#define c7_heap_left(i)		((((i)) << 1) + 1)
#define c7_heap_right(i)	(c7_heap_left(i) + 1)
#define c7_heap_count(b)	((b)->_n)
#define c7_heap_top(b)		c7_heap_Nth(b, 0)
#define c7_heap_Nth(b, n)	(&(b)->_a[(n)])
#define c7_heap_index(b, e)	((e) - (b)->_a)
#define c7_heap_reset(b)	((b)->_n = 0)


__attribute__((unused))
static void __C7_HEAP_SHIFT_UP(C7_ELM_TYPE * const array, size_t idx)
{
     C7_ELM_TYPE target = array[idx];
    while (idx != 0) {
	size_t pidx = c7_heap_parent(idx);
	if (C7_ELM_LT(&array[pidx], &target)) {
	    break;
	}
	array[idx] = array[pidx];
	idx = pidx;
    }
    array[idx] = target;
}

__attribute__((unused))
static void __C7_HEAP_SHIFT_DOWN(C7_ELM_TYPE * const array, size_t idx, const size_t n)
{
    C7_ELM_TYPE target = array[idx];
    for (;;) {
	size_t cidx = c7_heap_left(idx);
	if (cidx >= n) {
	    break;
	}
	if (((cidx + 1) < n) && C7_ELM_LT(&array[cidx+1], &array[cidx])) {
	    cidx += 1;
	}
	if (C7_ELM_LT(&target, &array[cidx])) {
	    break;
	}
	array[idx] = array[cidx];
	idx = cidx;
    }
    array[idx] = target;
}

__attribute__((unused))
static void __C7_HEAP_ADD(__C7_HEAP_BASE_TYPE *base, C7_ELM_TYPE *elm)
{
    base->_a[base->_n] = *elm;
    __C7_HEAP_SHIFT_UP(base->_a, base->_n);
    base->_n++;
}

__attribute__((unused))
static void __C7_HEAP_REMOVE(__C7_HEAP_BASE_TYPE *base, size_t idx)
{
    base->_a[idx] = base->_a[base->_n - 1];
    base->_n--;
    __C7_HEAP_SHIFT_DOWN(base->_a, idx, base->_n);
    if (idx != 0) {
	__C7_HEAP_SHIFT_UP(base->_a, idx);
    }
}

__attribute__((unused))
static int __C7_HEAP_VERIFY(__C7_HEAP_BASE_TYPE *base)
{
    size_t i;
    int err = 0;
    for (i = 1; i < base->_n; i++) {
	size_t left = c7_heap_left(i);
	size_t right = c7_heap_right(i);
	C7_ELM_TYPE *elm = &base->_a[i];
	if ((left < base->_n) &&  C7_ELM_LT(&base->_a[left], elm)) {
	    (void)fprintf(stderr, "Wrong node index: %ld [left is smaller]\n", i);
	    err++;
	}
	if ((right < base->_n) && C7_ELM_LT(&base->_a[right], elm)) {
	    (void)fprintf(stderr, "Wrong node index: %ld [right is smaller]\n", i);
	    err++;
	}
    }
    return (err == 0);
}


/*----------------------------------------------------------------------------
                                   cleanup
----------------------------------------------------------------------------*/

/* keep C7_ELM_TYPE */
/* keep C7_ELM_LT */
#undef C7_HEAP_NAME

/* keep c7_heap_init */
/* keep c7_heap_setarray */
/* keep c7_heap_parent */
/* keep c7_heap_left */
/* keep c7_heap_right */
/* keep c7_heap_count */
/* keep c7_heap_top */
/* keep c7_heap_Nth */
/* keep c7_heap_index */
/* keep c7_heap_reset */

#undef __C7_TARGET_NAME
#undef __C7_HEAP_BASE_TAG
#undef __C7_HEAP_BASE_TYPE
#undef __C7_HEAP_SHIFT_UP
#undef __C7_HEAP_SHIFT_DOWN
#undef __C7_HEAP_ADD
#undef __C7_HEAP_REMOVE
#undef __C7_HEAP_VERIFY


#if defined(__cplusplus)
}
#endif
/* c7heapdef.h */
