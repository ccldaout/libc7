/*
 * c7jmp.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <c7jmp.h>
#include <c7app.h>


static c7_thread_local __c7_jmp_context_t *jmp_context;
# define get_jmp_context_pp()		(&jmp_context)

void __c7_jmp_context(__c7_jmp_context_t *new_context)
{
    __c7_jmp_context_t **curpp = get_jmp_context_pp();
    new_context->prev = *curpp;
    new_context->status = C7_FALSE;
    new_context->data = NULL;
    new_context->r_msg = NULL;
    new_context->r_file = NULL;
    new_context->r_line = 0;
    *curpp = new_context;
}

void __c7_jmp_longjmp(c7_bool_t success, void *data,
		      c7_str_t *msg_opt, const char *file_opt, int line_opt)
{
    __c7_jmp_context_t **curpp = get_jmp_context_pp();
    __c7_jmp_context_t *cur = *curpp;
    if (cur != NULL) {
	cur->status = success;
	cur->data = data;
	cur->r_msg = msg_opt;
	cur->r_file = file_opt;
	cur->r_line = line_opt;
	*curpp = cur->prev;
	longjmp(cur->jb, 1);
    }
    /* TODO: show message */
}
