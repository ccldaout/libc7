/*
 * c7fd.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <c7fd.h>
#include <c7status.h>


/*----------------------------------------------------------------------------
                                    fcntl
----------------------------------------------------------------------------*/

c7_bool_t c7_fd_advlock(int fd, c7_bool_t enable)
{
    struct flock flk = {0};

    flk.l_whence = SEEK_SET;

    flk.l_type = enable ? F_WRLCK : F_UNLCK;
    if (fcntl(fd, F_SETLKW, &flk) == C7_SYSOK)
	return C7_TRUE;
    flk.l_type = enable ? F_RDLCK : F_UNLCK;
    if (fcntl(fd, F_SETLKW, &flk) == C7_SYSOK)
	return C7_TRUE;

    c7_status_add(errno, ": advlock: fcntl(%d, F_SETLKW, ...)\n", fd);
    return C7_FALSE;
}

c7_bool_t c7_fd_setnonblock(int fd, c7_bool_t enable)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == C7_SYSERR) {
	c7_status_add(errno, ": setnonblock: fcntl(%d, F_GETFL, ...)\n", fd);
	return C7_FALSE;
    }

    if (enable)
	flags |= O_NONBLOCK;
    else
	flags &= ~O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags) == C7_SYSERR) {
	c7_status_add(errno, ": setnonblock: fcntl(%d, F_SETFL, ...)\n", fd);
	return C7_FALSE;
    }
    return C7_TRUE;
}

c7_bool_t c7_fd_getnonblock(int fd, c7_bool_t *status)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == C7_SYSERR) {
	c7_status_add(errno, ": getnonblock: fcntl(%d, F_GETFL, ...)\n", fd);
	return C7_FALSE;
    }
    if (status != 0)
	*status = ((flags & O_NONBLOCK) != 0);
    return C7_TRUE;
}

c7_bool_t c7_fd_setcloexec(int fd, c7_bool_t enable)
{
    if (fcntl(fd, F_SETFD, enable ? FD_CLOEXEC : 0) == C7_SYSERR) {
	c7_status_add(errno, ": setcloexec: fcntl(%d, F_SETFD, ...)\n", fd);
	return C7_FALSE;
    }
    return C7_TRUE;
}

c7_bool_t c7_fd_getcloexec(int fd, c7_bool_t *status)
{
    int ret = fcntl(fd, F_GETFD, 0);
    if (ret == C7_SYSERR) {
	c7_status_add(errno, ": getcloexec: fcntl(%d, F_GETFD, ...)\n", fd);
	return C7_FALSE;
    }
    if (status != 0)
	*status = (ret == FD_CLOEXEC);
    return C7_TRUE;
}

c7_bool_t c7_fd_renumber(int *fdp, int lowest)
{
    int newfd;
    newfd = fcntl(*fdp, F_DUPFD, lowest);
    if (newfd == C7_SYSERR) {
	c7_status_add(errno, ": renumber: fcntl(%d, F_DUPFD, ...)\n", *fdp);
	return C7_FALSE;
    }
    (void)close(*fdp);
    *fdp = newfd;
    return C7_TRUE;
}
