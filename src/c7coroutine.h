/*
 * c7coroutine.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_COROUTINE_H_LOADED__
#define __C7_COROUTINE_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7memory.h>


extern const char __C7_COROUTINE_YIELD_SPECIAL[];
#define __C7_COROUTINE_YIELD_FAIL	((void *)&__C7_COROUTINE_YIELD_SPECIAL[0])
#define __C7_COROUTINE_YIELD_ABORT	((void *)&__C7_COROUTINE_YIELD_SPECIAL[1])
#define __C7_COROUTINE_YIELD_EXIT	((void *)&__C7_COROUTINE_YIELD_SPECIAL[2])


typedef struct c7_coroutine_t_ *c7_coroutine_t;

c7_coroutine_t c7_coroutine_self(void);
c7_coroutine_t c7_coroutine_new(size_t stacksize_kb,
				void (*coroutine)(c7_mgroup_t, void *start_param));
void *c7_coroutine_start(c7_coroutine_t co_target, void *start_param);
void *c7_coroutine_yield(c7_coroutine_t co_target, void *yield_data);
void c7_coroutine_exit(void);
void c7_coroutine_abort(void);
void c7_coroutine_free(c7_coroutine_t co);
#define c7_coroutine_is_failed(yd)	((void *)(yd) == __C7_COROUTINE_YIELD_FAIL)
#define c7_coroutine_is_exited(yd)	((void *)(yd) == __C7_COROUTINE_YIELD_EXIT)
#define c7_coroutine_is_aborted(yd)	((void *)(yd) == __C7_COROUTINE_YIELD_ABORT)
static inline c7_bool_t c7_coroutine_is_valid(void *yield_data)
{
    return (!c7_coroutine_is_failed(yield_data) &&
	    !c7_coroutine_is_exited(yield_data) &&
	    !c7_coroutine_is_aborted(yield_data));
}


#define c7_coroutine_foreach(sz, gn, prm, vn)				\
    __c7_coroutine_foreach(sz, gn, prm, vn, __c7_coroutine_foreach_##vn)

#define __c7_coroutine_foreach(sz, gn, prm, vn, con)	\
    for (c7_coroutine_t con =				\
	     __c7_coroutine_for_init((sz), (gn), (prm), (void **)(&(vn))); \
	 con != NULL;							\
	 (vn) = __c7_coroutine_for_next(&con))

#define c7_coroutine_foreach_stop(vn)	__c7_coroutine_for_stop(&__c7_coroutine_foreach_##vn)

c7_coroutine_t __c7_coroutine_for_init(size_t stacksize_kb,
				       void (*generator)(c7_mgroup_t, void *param),
				       void *param,
				       void **vpp);
void *__c7_coroutine_for_next(c7_coroutine_t *cop);
void *__c7_coroutine_for_stop(c7_coroutine_t *cop);


typedef struct c7_generator_t_ {
    c7_coroutine_t __co;
    void *__end_value;
    struct __c7_generator_prm_t_ *__param;
} c7_generator_t;

c7_bool_t c7_generator_init(c7_generator_t *gen,
			    size_t stacksize_kb,
			    void (*generator)(c7_mgroup_t, void *param),
			    void (*finalize)(c7_mgroup_t, void *param),
			    void *param);
c7_bool_t __c7_generator_next(c7_generator_t *gen, void **vpp);
#define c7_generator_next(gen, pp)	__c7_generator_next((gen), (void **)(pp))
void c7_generator_stop(c7_generator_t *gen);


#if defined(__cplusplus)
}
#endif
#endif /* __C7_COROUTINE_H_LOADED__ */
