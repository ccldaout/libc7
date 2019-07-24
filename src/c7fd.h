/*
 * c7fd.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_IO_H_LOADED__
#define __C7_IO_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7types.h>


c7_bool_t c7_fd_advlock(int fd, c7_bool_t enable);
c7_bool_t c7_fd_setnonblock(int fd, c7_bool_t enable);
c7_bool_t c7_fd_getnonblock(int fd, c7_bool_t *status);
c7_bool_t c7_fd_setcloexec(int fd, c7_bool_t enable);
c7_bool_t c7_fd_getcloexec(int fd, c7_bool_t *status);
c7_bool_t c7_fd_renumber(int *fdptr, int lowest);


#if defined(__cplusplus)
}
#endif
#endif /* c7fd.h */
