/*
 * c7proc.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <c7fd.h>
#include <c7ipc.h>
#include <c7parray.h>
#include <c7proc.h>
#include <c7signal.h>
#include <c7status.h>
#include <c7thread.h>

#define _ C7_UNUSED_INT


static c7_thread_r_mutex_t __r_mutex = C7_THREAD_R_MUTEX_INITIALIZER;

static void _CRITICAL_ENTER(void)
{
    c7_thread_r_lock(&__r_mutex);
}

static void _CRITICAL_QUIT(void)
{
    if (!c7_thread_r_unlock(&__r_mutex))
	c7abort_err(0, NULL);
}

// Don't call sigunblock_in_child on parent process.
static void sigunblock_in_child(void)
{
    sigset_t sigs;
    (void)sigemptyset(&sigs);
    (void)sigprocmask(SIG_SETMASK, &sigs, NULL);
}


/*----------------------------------------------------------------------------
                         fundamental fork+exec / wait
----------------------------------------------------------------------------*/

static pid_t fork_x(int *chkpipe)
{
    int pipefd[2];
    if (pipe(pipefd) == C7_SYSERR) {
	c7_status_add(errno, ": pipe failed\n.");
	return C7_SYSERR;
    }

    pid_t newpid;
    if ((newpid = fork()) == C7_SYSERR) {
	c7_status_add(errno, ": fork failed\n.");
	(void)close(pipefd[0]);
	(void)close(pipefd[1]);
	return C7_SYSERR;
    }

    (void)close(pipefd[newpid != 0]);
    *chkpipe = pipefd[newpid == 0];

    return newpid;
}

static pid_t forkexec(const char *progname, char **argv,
		      int tmp_fdnum_in_child,
		      c7_bool_t (*preexec)(const char *progname, char **argv,
					   void *__param),
		      void *__param)
{
    pid_t newpid;
    int chkpipe;
    char errval;

    if ((newpid = fork_x(&chkpipe)) == C7_SYSERR)
	return C7_SYSERR;

    if (newpid > 0) {
	int n = read(chkpipe, &errval, 1);
	(void)close(chkpipe);
	if (n == 1) {
	    (void)waitpid(newpid, 0, 0);
	    c7_status_add(errno = errval, ": cannot start: %s\n", progname);
	    newpid = C7_SYSERR;
	}
	return newpid;
    }

    // child process
    (void)close(tmp_fdnum_in_child);
    sigunblock_in_child();
    if (c7_fd_renumber(&chkpipe, tmp_fdnum_in_child)) {
	if (preexec == NULL || (*preexec)(progname, argv, __param)) {
	    if (c7_fd_setcloexec(chkpipe, C7_TRUE)) {
		(void)execvp(progname, argv);
	    }
	}
    }
    errval = errno;
    _ = write(chkpipe, &errval, 1);
    _exit(EXIT_FAILURE);		// don't use exit to prevent unexpected side-effects
    return C7_SYSERR;			// dummy return statement
}

pid_t c7_proc_spawn(const char *progname, char **argv,	
		    int tmp_fdnum_in_child,
		    c7_bool_t (*preexec)(const char *progname, char **argv,
					 void *__param),
		    void (*postwait)(pid_t pid,
				     int status,
				     void *__param),
		    void *__param)
{
    _CRITICAL_ENTER();
    int slot = C7_SYSERR;
    pid_t pid = C7_SYSERR;
    if (postwait != NULL)
	slot = c7_cleaner_reserve(postwait, __param);
    if (postwait == NULL || slot != C7_SYSERR)
	pid = forkexec(progname, argv, tmp_fdnum_in_child, preexec, __param);
    if (pid != C7_SYSERR && slot != C7_SYSERR)
	(void)c7_cleaner_register(slot, pid);
    else if (pid == C7_SYSERR && slot != C7_SYSERR)
	(void)c7_cleaner_free(slot);
    _CRITICAL_QUIT();
    return pid;
}


pid_t c7_proc_fork(void (*postwait)(pid_t pid,
				    int status,
				    void *__param),
		   void *__param)
{
    _CRITICAL_ENTER();
    int slot = C7_SYSERR;
    pid_t pid = C7_SYSERR;
    if (postwait != NULL)
	slot = c7_cleaner_reserve(postwait, __param);
    if (postwait == NULL || slot != C7_SYSERR) {
	if ((pid = fork()) == 0) {
	    _CRITICAL_QUIT();
	    return 0;
	} else if (pid == C7_SYSERR)
	    c7_status_add(errno, ": fork failed\n.");
    }
    if (pid != C7_SYSERR && slot != C7_SYSERR)
	(void)c7_cleaner_register(slot, pid);
    else if (pid == C7_SYSERR && slot != C7_SYSERR)
	(void)c7_cleaner_free(slot);
    _CRITICAL_QUIT();
    return pid;
}


int c7_proc_wait(pid_t pid)
{
    int status, ret;
    while ((ret = waitpid(pid, &status, 0)) == C7_SYSERR && errno == EINTR);
    if (ret == C7_SYSERR)
	return C7_PROC_WAIT_ERR;
    c7_status_clear();
    return (WIFEXITED(status)) ? WEXITSTATUS(status) : -WTERMSIG(status);
}


/*----------------------------------------------------------------------------
                         filter process (main parts)
----------------------------------------------------------------------------*/

struct c7_filter_t_ {
    int csock;		// for ending communication on endfilter
    pid_t pid;		// pid of 1st forked process (not filter process)
};

static int mvfd(int ofd, int limitfd)
{
    int nfd = fcntl(ofd, F_DUPFD, limitfd);
    if (nfd != C7_SYSERR)
	(void)close(ofd);
    return nfd;
}

static void errquit(int err, int sock)
{
    char b = err;
    _ = write(sock, &b, 1);
    // don't use exit to prevent unexpected side-effects
    _exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}

static pid_t genfilter(int fd0, int fd1, int cs[], char **av)
{
    char b;
    pid_t pid;
    int n, xfd[2];
    
    // parent process

    if (pipe(xfd) == C7_SYSERR) {	// used until execvp is called.
	c7_status_add(errno, ": pipe failed.\n");
	return C7_SYSERR;
    }

    if ((pid = fork()) > 0) {
	(void)close(cs[1]);
	(void)close(xfd[1]);
	if (read(xfd[0], &b, 1) == 1) {		// (A)<-
	    // error on 1st forked process or evecvp failed on 2nd forked process
	    (void)kill(pid, SIGKILL);
	    (void)c7_proc_wait(pid);
	    pid = C7_SYSERR;
	    c7_status_add(errno = b, ": error on 1st forked process or execvp failed\n");
	}
	// execvp is success on 2nd forked process.
	(void)close(xfd[0]);
	return pid;
    } else if (pid == C7_SYSERR) {
	c7_status_add(errno, ": 1st fork failed\n");
	(void)close(xfd[0]);
	(void)close(xfd[1]);
	return C7_SYSERR;
    }

    // 1st forked process : wait order from parent

    (void)close(cs[0]);
    (void)close(xfd[0]);
    if (fd0 == C7_SYSERR && (fd0 = open("/dev/null", O_RDONLY)) == C7_SYSERR)
	errquit(errno, xfd[1]);
    if (fd1 == C7_SYSERR && (fd1 = open("/dev/null", O_WRONLY)) == C7_SYSERR)
	errquit(errno, xfd[1]);

    if ((fd0     = mvfd(fd0,   5)) == C7_SYSERR ||
	(fd1     = mvfd(fd1,   5)) == C7_SYSERR ||
	(cs[1]   = mvfd(cs[1], 5)) == C7_SYSERR ||
	(xfd[1] = mvfd(xfd[1], 5)) == C7_SYSERR)
	errquit(errno, xfd[1]);			// ->(A) (mvfd failed, read -> 1)
    for (n = 0; n < 5; n++)
	(void)close(n);
    (void)mvfd(fd0, 0);				// fd0 -> 0
    (void)mvfd(fd1, 1);				// fd1 -> 1
    (void)open("/dev/null", O_WRONLY);
    (void)mvfd(cs[1], 3);			// cs[1] -> 3
    (void)mvfd(xfd[1], 4);			// xfd[1] -> 4
    for (n = 5; n < 128; n++)
	(void)close(n);

    if ((pid = fork()) > 0) {
	int s;
	(void)close(0);
	(void)close(1);
	(void)close(2);
	(void)close(4);
	if (read(3, &b, 1) != 1 || b == 'K')	// (B)<- (wait command from endfilter)
	    (void)kill(pid, SIGKILL);
	s = c7_proc_wait(pid);			// wait 2nd forked process terminate
	if (s < 0) {
	    (void)signal(-s, SIG_DFL);
	    (void)kill(getpid(), -s);
	}
	// don't use exit to prevent unexpected side-effects
	_exit(s);				// ->(C)
    } else if (pid == C7_SYSERR)
	errquit(errno, 4);			// ->(A) (2nd fork failed, read -> 1)

    // 2nd forked process : execute filter process

    (void)close(3);				// (void)fd_setcloexec(3, C7_TRUE);
    (void)c7_fd_setcloexec(4, C7_TRUE);		
    sigunblock_in_child();
    (void)execvp(av[0], av);			// ->(A) (execvp is succes, read -> 0)
    errquit(errno, 4);				// ->(A) (execvp failed,    read -> 1)
    return -1;					// dummy return statement
}

static c7_bool_t endfilter(c7_filter_t flt, int *stsp, int cmd)
{
    int __status;
    if (stsp == NULL) {
	stsp = &__status;
    }

    char b = cmd;
    if (write(flt->csock, &b, 1) != 1)		// ->(B) (write 'W' or 'K')
	(void)kill(flt->pid, SIGKILL);
    _ = read(flt->csock, &b, 1);		// (C)<- (wait 1st forked process terminate)
    *stsp = c7_proc_wait(flt->pid);		// get status of 1st forked process

    (void)close(flt->csock);
    free(flt);

    if (*stsp == 0) {
	c7_status_clear();
    } else if (*stsp > 0)
	c7_status_add(errno = EBADF, ": filter process failed (exit:%d).\n", *stsp);
    else //if (*stsp < 0)
	c7_status_add(errno = EINTR, ": filter process is killed (signal:%d).\n", -*stsp);
    return (*stsp == 0);
}


/*----------------------------------------------------------------------------
                                filter process
----------------------------------------------------------------------------*/

c7_filter_t c7_filter_start(int fd0, int fd1, char **av)
{
    (void)fflush(0);

    c7_filter_t flt;
    if ((flt = c7_malloc(sizeof(*flt)))) {
	int cs[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, cs) == C7_SYSOK) {
	    flt->csock = cs[0];
	    flt->pid = genfilter(fd0, fd1, cs, av);
	    int saveerr = errno;
	    if (flt->pid != C7_SYSERR)
		return flt;
	    (void)close(cs[0]);
	    (void)close(cs[1]);
	    errno = saveerr;
	} else
	    c7_status_add(errno, ": socketpair failed\n.");
	free(flt);
    }
    return NULL;
}

c7_bool_t c7_filter_wait(c7_filter_t flt, int *stsp)
{
    return endfilter(flt, stsp, 'W');
}

void c7_filter_kill(c7_filter_t flt)
{
    (void)endfilter(flt, 0, 'K');
}


/*----------------------------------------------------------------------------
                            extended popen/pclose
----------------------------------------------------------------------------*/

typedef struct po_t {
    c7_filter_t flt;
    char mode;
} po_t;

static c7_parray_t FdTable;

static c7_bool_t posetupfds(const char *redir_o, const char *mode,
			    int *fd0, int *fd1, int *fdp)
{
    int fdv[2], fdf = -2;

    if (pipe(fdv) == C7_SYSOK) {
	if (*mode == 'r') {
	    if (redir_o != NULL)
		*fd0 = fdf = open(redir_o, O_RDONLY);
	    else
		*fd0 = C7_SYSERR;
	    *fd1 = fdv[1];
	    *fdp = fdv[0];
	} else { // *mode == 'w'
	    *fd0 = fdv[0];
	    if (redir_o != NULL)
		*fd1 = fdf = open(redir_o, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	    else
		*fd1 = C7_SYSERR;
	    *fdp = fdv[1];
	}
	if (fdf != C7_SYSERR) {
	    return C7_TRUE;
	}
	c7_status_add(errno, ": open failed: %s\n", redir_o);
	(void)close(fdv[0]);
	(void)close(fdv[1]);
    } else
	c7_status_add(errno, ": pipe failed.\n");
    return C7_FALSE;
}

FILE *c7_popen(const char *redir_o, const char *mode, char **av)
{
    int fd0, fd1, fdp;
    FILE *fp;
    po_t *po;

    if (*mode != 'r' && *mode != 'w') {
	errno = EINVAL;
	return NULL;
    }

    if (posetupfds(redir_o, mode, &fd0, &fd1, &fdp)) {
	if ((fp = fdopen(fdp, mode)) != NULL) {
	    if ((po = c7_parray_new(FdTable, fdp)) != NULL) {
		if ((po->flt = c7_filter_start(fd0, fd1, av)) != NULL) {
		    po->mode = *mode;
		    (void)close(fd0);
		    (void)close(fd1);
		    return fp;
		}
		c7_parray_free(FdTable, fdp);
	    }
	    (void)fclose(fp);
	} else
	    c7_status_add(errno, ": fdopen failure\n");
	(void)close(fd0);
	(void)close(fd1);
	(void)close(fdp);
	if (*mode == 'w' && redir_o != NULL)
	    (void)unlink(redir_o);
    }
    return NULL;
}

int c7_pclose(FILE *fp)
{
    int fdp = fileno(fp);
    po_t *po = c7_parray_get(FdTable, fdp);
    if (po == NULL) {
	c7_status_add(errno = EINVAL, ": FILE (fd:%1d) is not managed\n", fdp);
	return EOF;
    }

    if (po->mode == 'w')
	(void)fclose(fp);
    c7_bool_t ret = c7_filter_wait(po->flt, 0);
    int err = errno;
    if (po->mode == 'r')
	(void)fclose(fp);
    errno = err;

    free(po);
    c7_parray_free(FdTable, fdp);
    return ret ? 0 : EOF;
}


/*----------------------------------------------------------------------------
                                   cleaner
----------------------------------------------------------------------------*/

typedef struct _cleaner_t_ {
    pid_t pid;
    void *__param;
    void (*callback)(pid_t, int, void *);
} _cleaner_t;

static c7_parray_t Cleaners;


int c7_cleaner_reserve(void (*callback)(pid_t pid,
					int wait_status,
					void *__param),
		       void *__param)
{
    _CRITICAL_ENTER();
    int slot;
    _cleaner_t *cleaner = c7_parray_new_auto(Cleaners, &slot);
    _CRITICAL_QUIT();

    if (cleaner == NULL)
	return C7_SYSERR;

    cleaner->pid = C7_SYSERR;
    cleaner->__param = __param;
    cleaner->callback = callback;
    return slot;
}

void c7_cleaner_register(int reserved_slot, pid_t newpid)
{
    _CRITICAL_ENTER();
    _cleaner_t *cleaner = c7_parray_get(Cleaners, reserved_slot);
    _CRITICAL_QUIT();

    if (cleaner == NULL) {
	c7abort_err(EINVAL,
		    ": [FATAL] c7_cleaner_register: slot:%d is not reserved\n",
		    reserved_slot);
    }
    cleaner->pid = newpid;
}

void c7_cleaner_waitprocs(void)
{
    _CRITICAL_ENTER();

    int slot;
    _cleaner_t *cleaner;
    c7_parray_foreach_idx(Cleaners, cleaner, slot) {
	if (cleaner->pid == C7_SYSERR)
	    continue;
	int status;
	if (waitpid(cleaner->pid, &status, WNOHANG) > 0) {
	    status = WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);
	    cleaner->callback(cleaner->pid, status, cleaner->__param);
	    c7_parray_free(Cleaners, slot);
	}
    }

    _CRITICAL_QUIT();
}

void c7_cleaner_free(int reserved_slot)
{
    _CRITICAL_ENTER();
    c7_parray_free(Cleaners, reserved_slot);
    _CRITICAL_QUIT();
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

void __c7_proc_init(void)
{
    FdTable = c7_parray_create(sizeof(po_t), NULL, NULL);
    Cleaners = c7_parray_create(sizeof(_cleaner_t), NULL, NULL);
    if (FdTable == NULL || Cleaners == NULL) {
	c7abort_err(EINVAL, ": [FATAL] cannot allocate storage for c7proc\n");
    }
}
