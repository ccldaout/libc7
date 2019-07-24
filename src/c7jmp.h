/*
 * c7jmp.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_JMP_H_LOADED__
#define __C7_JMP_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <setjmp.h>
#include <c7string.h>


/*
 *  C7_JMP_try {
 *     ....
 *  }
 *  C7_JMP_catch {
 *    if (C7_JMP_SUCCESS) {
 *        ...
 *    } else if (C7_JMP_RAISED) {
 *        if (C7_JMP_DATA ...) ...;
 *    }
 *  }
 *  C7_JMP_end;
 */


typedef struct __c7_jmp_context_t_ {
    struct __c7_jmp_context_t_ *prev;
    jmp_buf jb;
    c7_bool_t status;
    void *data;
    c7_str_t *r_msg;
    const char *r_file;
    int r_line;
} __c7_jmp_context_t;

void __c7_jmp_context(__c7_jmp_context_t *new_context);
void __c7_jmp_longjmp(c7_bool_t success, void *data,
		      c7_str_t *msg_opt, const char *file_opt, int line_opt);

#define C7_JMP_try				\
    do {					\
    __c7_jmp_context_t __new_context;		\
    __c7_jmp_context(&__new_context);		\
    if (setjmp(__new_context.jb) == 0) {
#define C7_JMP_catch				\
    C7_JMP_return(0);				\
    } else {
#define C7_JMP_end		} } while (0)

#define C7_JMP_return(d)	__c7_jmp_longjmp(C7_TRUE, d, NULL, NULL, 0)
#define C7_JMP_raise(d)		__c7_jmp_longjmp(C7_FALSE, d, NULL, __FILE__, __LINE__)
#define C7_JMP_raise_vs(d, vsp)	__c7_jmp_longjmp(C7_FALSE, d, vsp, __FILE__, __LINE__)

#define C7_JMP_SUCCESS		(__new_context.status)
#define C7_JMP_RAISED		(!__new_context.status)
#define C7_JMP_DATA		(__new_context.data)


#if defined(__cplusplus)
}
#endif
#endif /* c7jmp.h */
