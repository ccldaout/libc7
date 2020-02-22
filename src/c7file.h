/*
 * c7file.h
 *
 * https://ccldaout.github.io/libc7/group__c7file.html
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


// path operations

#define c7_path_name(p)		c7strrchr_next(p, '/', p)
#define c7_path_suffix(p)	c7strrchr_x(p, '.', 0)
c7_str_t *c7_path_untildize(c7_str_t *sbp,
			    const char *path);
c7_str_t *c7_path_ortho(c7_str_t *sbp, char *path);
c7_str_t *c7_path_abs(c7_str_t *sbp,
		      const char *path,
		      const char *base_op,
		      c7_bool_t untildize);
c7_bool_t c7_path_isdir(const char *path);
c7_str_t *c7_path_getcwd(c7_str_t *sbp);
c7_bool_t c7_path_search(c7_str_t *sbp,
			 const char *name,
			 const char **pathlistv,
			 const char *default_suffix);
c7_str_t *c7_path_c7spec(c7_str_t *sbp,
			 const char *envname_op,
			 const char *name, const char *suffix,
			 c7_bool_t produce);


// file read/write operations

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


// other file operations

c7_bool_t c7_file_mkdir(const char *path, mode_t mode, uid_t uid, gid_t gid);
c7_bool_t c7_file_inherit_owner(const char *path);


// for compatibility

#define c7_file_search(s, n, v, x)		c7_path_search((s), (n), (v), (x))
#define c7_file_special_path(s, e, n, x)	c7_path_c7spec((s), (e), (n), (x), C7_TRUE)
#define c7_file_special_find(s, e, n, x)	c7_path_c7spec((s), (e), (n), (x), C7_FALSE)


#if defined(__cplusplus)
}
#endif
#endif /* c7file.h */
