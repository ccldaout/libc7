/*
 * c7proc.h
 *
 * https://ccldaout.github.io/libc7/group__c7proc.html
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_PROC_H_LOADED__
#define __C7_PROC_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <stdio.h>
#include <c7types.h>


#define C7_PROC_WAIT_ERR	(1<<16)


int c7_cleaner_reserve(void (*callback)(pid_t pid,
					int wait_status,
					void *__param),
			    void *__param);
void c7_cleaner_register(int reserved_slot, pid_t newpid);
void c7_cleaner_waitprocs(void);
void c7_cleaner_free(int reserved_slot);


pid_t c7_proc_spawn(const char *progname, char **argv,	
		    int tmp_fdnum_in_child,
		    c7_bool_t (*preexec)(const char *progname, char **argv,
					 void *__param),
		    void (*postwait)(pid_t pid,
				     int status,
				     void *__param),
		    void *__param);
pid_t c7_proc_fork(void (*postwait)(pid_t pid,
				    int status,
				    void *__param),
		   void *__param);
int c7_proc_wait(pid_t pid);	// >=0:exit_status, <0:-signal_number


typedef struct c7_filter_t_ *c7_filter_t;

c7_filter_t c7_filter_start(int fd0, int fd1, char **av);
c7_bool_t c7_filter_wait(c7_filter_t flt, int *stsp);
void c7_filter_kill(c7_filter_t flt);


FILE *c7_popen(const char *redir_o, const char *mode, char **av);
int c7_pclose(FILE *fp);


#if defined(__cplusplus)
}
#endif
#endif /* c7proc.h */
