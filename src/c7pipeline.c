/*
 * c7pipeline.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"
#define _GNU_SOURCE 1	// execvpe

#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <c7fd.h>
#include <c7memory.h>
#include <c7pipeline.h>
#include <c7proc.h>
#include <c7status.h>


/*----------------------------------------------------------------------------

----------------------------------------------------------------------------*/

/* private */
typedef struct pl_proc_t {
    pid_t pid;
    char **av;
    char **ev;
    char *dir;
} pl_proc_t;

struct c7_pipeline_t_ {
    c7_mgroup_t mg;
    int pc;		/* process counter */
    pl_proc_t *pv;	/* array of process context */
    int *wsv;		/* array of wait status */
    int sndfd;		/* for send data to pipeline */
    int rcvfd;		/* for receive data from pipeline */
};


/*
 * pl_init: initialize pipeline control data
 */
c7_pipeline_t c7_pipeline_init(void)
{
    c7_mgroup_t mg = c7_mg_new();
    if (mg == NULL)
	return NULL;
    c7_pipeline_t pl = c7_mg_malloc(mg, sizeof(*pl));
    if (pl == NULL) {
	c7_mg_destroy(mg);
	return NULL;
    }

    pl->mg = mg;
    pl->pc = 0;
    pl->pv = NULL;
    pl->wsv = NULL;
    pl->sndfd = pl->rcvfd = C7_SYSERR;
    return pl;
}


/*
 * c7_pipeline_add: append program information
 */
c7_bool_t c7_pipeline_add(c7_pipeline_t pl, const char *wdir, char **av, char **ev)
{
    void *p;

    if ((p = c7_mg_realloc(pl->mg, pl->wsv, sizeof(pl->wsv[0]) * (pl->pc+1)))) {
	pl->wsv = p;
	if ((p = c7_mg_realloc(pl->mg, pl->pv, sizeof(pl->pv[0]) * (pl->pc+1)))) {
	    pl_proc_t *plp = &(pl->pv = p)[pl->pc];
	    plp->pid = C7_SYSERR;
	    plp->av = plp->ev = NULL;
	    if ((plp->dir = c7strdup_mg(pl->mg, wdir))) {
		if ((plp->av = c7strvdup_mg(pl->mg, av, -1))) {
		    if (ev == NULL || (plp->ev = c7strvdup_mg(pl->mg, ev, -1))) {
			pl->pc++;
			return C7_TRUE;
		    }
		    c7strvfree_mg(pl->mg, plp->av);
		    plp->av = NULL;
		}
		c7_mg_free(pl->mg, plp->dir);
		plp->dir = NULL;
	    }
	}
    }
    return C7_FALSE;
}


/*
 * c7_pipeline_exec: spawn pipeline
 *
 * c7_pipeline_bool_t c7_pipeline_exce(c7_pipeline_t pl, const char *workdir, int fd0, int fd1, int fd2);
 *
 *   fd0: renumber to 0 in first process of pipeline
 *   fd1: renumber to 1 in last process of  pipeline
 *   fd2: renumber to 2 in all process of pipeline
 *
 *   << fd0 and fd1 are closed by this function. >>
 */
c7_bool_t c7_pipeline_exec(c7_pipeline_t pl, int fd0, int fd1, int fd2)
{
    int i, p0, p1;

    p1 = fd1;

    for (i = pl->pc; i-- > 0;) {
	pl_proc_t *plp = &pl->pv[i];
	int pp[2];
	if (i != 0) {
	    if (pipe(pp) == C7_SYSERR) {
		c7_status_add(errno, ": pipe failed\n");
		break;
	    }
	    p0 = pp[0];
	} else
	    p0 = fd0;

	if ((plp->pid = fork()) > 0) {
	    (void)close(p0);
	    (void)close(p1);
	} else if (plp->pid == 0) {
	    int i;
	    if (chdir(plp->dir) == C7_SYSERR)
		exit(errno);
	    if (!c7_fd_renumber(&p0, 3) ||
		!c7_fd_renumber(&p1, 3) ||
		!c7_fd_renumber(&fd2, 3)) {
		exit(errno);
	    }
	    (void)close(0);
	    (void)close(1);
	    (void)close(2);
	    (void)dup(p0);
	    (void)dup(p1);
	    (void)dup(fd2);
	    (void)close(p0);
	    (void)close(p1);
	    (void)close(fd2);
	    for (i = 3; i < 256; i++)
		(void)close(i);
	    (void)signal(SIGPIPE, SIG_DFL);
	    if (plp->ev)
		(void)execvpe(plp->av[0], plp->av, plp->ev);
	    else
		(void)execvp(plp->av[0], plp->av);
	    exit(errno);
	} else {
	    c7_status_add(errno, ": fork failed\n");
	    break;
	}
	p1 = pp[1];
    }

    if (i >= 0) {
	int save_errno = errno;
	c7_pipeline_kill(pl, SIGKILL);
	(void)c7_pipeline_wait(pl, 0);
	errno = save_errno;
	return C7_FALSE;
    }
    return C7_TRUE;
}
	     

/*
 * c7_pipeline_kill: send signal
 */
void c7_pipeline_kill(c7_pipeline_t pl, int sig)
{
    int i;
    for (i = 0; i < pl->pc; i++) {
	if (pl->pv[i].pid > 0)
	    (void)kill(pl->pv[i].pid, sig);
    }    
}


/*
 * c7_pipeline_wait: wait for pipeline end
 * return: array of wait status (0:success, >0:error, <0:killed)
 */
int *c7_pipeline_wait(c7_pipeline_t pl, int *pc)
{
    int i;
    for (i = 0; i < pl->pc; i++) {
	if (pl->pv[i].pid > 0) {
	    pl->wsv[i] = c7_proc_wait(pl->pv[i].pid);
	    pl->pv[i].pid = C7_SYSERR;
	} else
	    pl->wsv[i] = ECHILD;
    }
    if (pc != NULL)
	*pc = pl->pc;
    return pl->wsv;
}


/*
 * c7_pipeline_free: free pipeline context data
 */
void c7_pipeline_free(c7_pipeline_t pl)
{
    c7_pipeline_kill(pl, SIGKILL);
    c7_pipeline_wait(pl, NULL);
    c7_mg_freeall(pl->mg);
}
