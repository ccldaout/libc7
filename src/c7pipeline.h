/*
 * c7pipeline.h
 *
 * https://ccldaout.github.io/libc7/group__c7pipeline.html
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_PIPELINE_H_LOADED__
#define __C7_PIPELINE_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7types.h>


typedef struct c7_pipeline_t_ *c7_pipeline_t;

c7_pipeline_t c7_pipeline_init(void);

c7_bool_t c7_pipeline_add(c7_pipeline_t pl, const char *wdir, char **av, char **ev);

/*
 * c7_pipeline_exec: spawn pipeline
 *
 *   fd0: renumber to 0 in first process of pipeline
 *   fd1: renumber to 1 in last process of  pipeline
 *   fd2: renumber to 2 in all process of pipeline
 *
 *   << fd0 and fd1 are closed in this function. >>
 */
c7_bool_t c7_pipeline_exec(c7_pipeline_t pl, int fd0, int fd1, int fd2);

void c7_pipeline_kill(c7_pipeline_t pl, int sig);

// return: array of wait status (0:success, >0:error, <0:killed)
int *c7_pipeline_wait(c7_pipeline_t pl, int *pc);

void c7_pipeline_free(c7_pipeline_t pl);


#if defined(__cplusplus)
}
#endif
#endif /* c7pipeline.h */
