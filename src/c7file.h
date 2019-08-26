/*
 * c7file.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_FILE_H_LOADED__
#define __C7_FILE_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <stdio.h>
#include <c7string.h>


#define c7_path_name(p)		c7strrchr_next(p, '/', p)
#define c7_path_suffix(p)	c7strrchr_x(p, '.', 0)

char *c7_fgets(c7_str_t *sbp, FILE *fp);

c7_bool_t c7_file_write(const char *path, int mode, void *buf, size_t size);
c7_bool_t c7_file_rewrite(const char *path, void *buf, size_t size,
			  const char *bck_suffix);

ssize_t c7_file_read(const char *path, void *buf, size_t size);
void *c7_file_read_x(const char *path, size_t *sizep_o);
char **c7_file_readlines_x(const char *path, size_t *nlinep_o);

void *c7_file_mmap_r(const char *path, size_t *sizep_o);
void *c7_file_mmap_rw(const char *path, size_t *sizep_io, c7_bool_t create);
void c7_file_munmap(void *addr, size_t size);

c7_bool_t c7_file_inherit_owner(const char *path);

c7_bool_t c7_file_search(c7_str_t *sbp,
			 const char *name,
			 const char **pathlistv,	// vector of pathlist
			 const char *default_suffix);	// include '.'

c7_str_t *c7_file_special_path(c7_str_t *sbp,
			       const char *envname_op,
			       const char *name,
			       const char *suffix);	// include '.'
c7_str_t *c7_file_special_find(c7_str_t *sbp,
			       const char *envname_op,
			       const char *name,
			       const char *suffix);	// include '.'

c7_bool_t c7_file_mkdir(const char *path, mode_t mode, uid_t uid, gid_t gid);


#if defined(__cplusplus)
}
#endif
#endif /* c7file.h */
