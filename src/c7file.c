/*
 * c7file.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <c7file.h>
#include <c7ipc.h>
#include <c7signal.h>
#include <c7status.h>

#define _ C7_UNUSED_INT


/*----------------------------------------------------------------------------
                              fgets for c7_str_t
----------------------------------------------------------------------------*/

char *c7_fgets(c7_str_t *sbp, FILE *fp)
{
    char buf[256];
    int n = 0;

    c7_status_clear();
    c7_str_reuse(sbp);

    while (fgets(buf, sizeof(buf)-1, fp) != 0) {
	n++;
	if (C7_STR_ERR(c7_strcpy(sbp, buf)))
	    return NULL;
	if (C7_STR_CHAR_R(sbp, -1) == '\n')
	    return c7_strbuf(sbp);
    }
    if (n == 0) {
	if (ferror(fp))
	    c7_status_add(errno, ": fgets error\n");
	else
	    errno = 0;		// EOF
	return NULL;
    }
    if (C7_STR_ERR(c7_strcpy(sbp, "\n")))
	return NULL;
    return c7_strbuf(sbp);
}


/*----------------------------------------------------------------------------
                              write entire file
----------------------------------------------------------------------------*/

c7_bool_t c7_file_write(const char *path, int mode, void *buf, size_t size)
{
    int fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, mode);
    if (fd != C7_SYSERR) { 
	if (write(fd, buf, size) == (ssize_t)size) {
	    (void)close(fd);
	    return C7_TRUE;
	} else
	    c7_status_add(errno, ": c7_file_write: %s\n", path);
	(void)close(fd);
    } else
	c7_status_add(errno, ": c7_file_write: <%s>\n", path);
    return C7_FALSE;
}

static c7_bool_t fchstat_ref(int fd, const char *ref_path)
{
    struct stat st;
    if (stat(ref_path, &st) == C7_SYSOK) {
	if (fchmod(fd, st.st_mode) == C7_SYSOK) {
	    if (geteuid() == 0) {
		_ = fchown(fd, st.st_uid, st.st_gid);
	    }
	    return C7_TRUE;
	}
    }
    return C7_FALSE;
}

static c7_str_t *reservetmp(const char *ref_path)
{
    c7_str_t *tmp = c7_str_new_sg();
    size_t dirsize = c7_path_name(ref_path) - ref_path;
    (void)c7_strncpy(tmp, ref_path, dirsize);

    for (uint64_t u = 1; u != 0; u++) {
	(void)c7_strtrunc(tmp, dirsize);
	(void)c7_sprintf(tmp, ".regrep.%lx", u);
	int fd = open(c7_strbuf(tmp), O_WRONLY|O_CREAT|O_EXCL, 0600);
	if (fd != C7_SYSERR) {
	    (void)fchstat_ref(fd, ref_path);
	    (void)close(fd);
	    return tmp;
	}
    }

    c7_status_add(ENOENT, ": cannot ready temporary file.\n");
    return C7_STR_None;
}

static c7_bool_t _rewrite(const char *path, void *buf, size_t size,
			  const char * const bck_suffix)
{    
    c7_str_t *bck, *tmp = reservetmp(path);
    if (bck_suffix != NULL)
	bck = c7_sprintf(NULL, "%s.%s", path, bck_suffix);
    else
	bck = reservetmp(path);
    if (C7_STR_ERR(tmp) || C7_STR_ERR(bck))
	return C7_FALSE;

    if (!c7_file_write(c7_strbuf(tmp), 0600, buf, size))
	return C7_FALSE;

    sigset_t o_set = c7_signal_sigblock();

    if (rename(path, c7_strbuf(bck)) == C7_SYSOK) {
	if (rename(c7_strbuf(tmp), path) == C7_SYSOK) {
	    if (bck_suffix == NULL)
		(void)unlink(c7_strbuf(bck));
	    c7_signal_sigrestore(&o_set);
	    return C7_TRUE;
	}
	c7_status_add(errno, ": rename failure\n: %s -> %s\n", c7_strbuf(tmp), path);
	(void)rename(c7_strbuf(bck), path);
	(void)unlink(c7_strbuf(tmp));
    } else
	c7_status_add(errno, ": rename failure\n: %s -> %s\n", path, c7_strbuf(bck));

    c7_signal_sigrestore(&o_set);

    return C7_FALSE;
}

c7_bool_t c7_file_rewrite(const char *path, void *buf, size_t size,
			  const char *bck_suffix)
{
    c7_sg_push();
    c7_bool_t ret = _rewrite(path, buf, size, bck_suffix);
    c7_sg_pop();
    return ret;
}


/*----------------------------------------------------------------------------
                              read partial file
----------------------------------------------------------------------------*/

static const char *path_s(const char *path)
{
    if (path == NULL || *path == 0 || (*path == '-' && path[1] == 0))
	return "<stdin>";
    return path;
}

static int ropen(const char *path)
{
    if (path == NULL || *path == 0 || (*path == '-' && path[1] == 0))
	return dup(0);
    int fd = open(path, O_RDONLY);
    if (fd == C7_SYSERR)
	c7_status_add(errno, ": read open <%s>\n", path);
    return fd;
}

ssize_t c7_file_read(const char *path, void *buf, size_t size)
{
    int fd = ropen(path);
    if (fd == C7_SYSERR)
	return -1;
    errno = 0;
    ssize_t az;
    az = read(fd, buf, size);
    (void)close(fd);
    if (az == -1) {
	c7_status_add(errno,
		      ": c7_file_read: %s, request: %lu, actual: %ld\n",
		      path_s(path), size, az);
	return -1;
    }
    return (size - az);
}


/*----------------------------------------------------------------------------
                               read entire file
----------------------------------------------------------------------------*/

static int rx_open(const char *path, struct stat *st)
{
    int fd = ropen(path);
    if (fd == C7_SYSERR)
	return C7_SYSERR;
    if (fstat(fd, st) == C7_SYSERR) {
	c7_status_add(errno, NULL);
	(void)close(fd);
	return C7_SYSERR;
    }
    return fd;
}

static char *rx_once(int fd, ssize_t rz)
{
    char *p;
    if ((p = c7_malloc(rz + 1)) != NULL) {
	ssize_t az;
	if ((az = read(fd, p, rz)) == rz) {
	    p[rz] = 0;		/* for text file */
	} else {
	    c7_status_add(az == C7_SYSERR ? errno : EIO, NULL);
	    free(p);
	    p = NULL;
	}
    }
    return p;
}

static char *rx_repeat(int fd, size_t *sizep_o)
{
    const int xz = 16 * 1024;
    char *tp = NULL;
    char *ep = tp;
    char *cp = tp;
    char *np;

    for (;;) {
	size_t n;
	if (ep == cp) {
	    if ((np = c7_realloc(tp, (n = cp - tp) + xz)) == NULL) {
		free(tp);
		return NULL;
	    }
	    ep = (cp = (tp = np) + n) + xz;
	}

	n = ep - cp;
	ssize_t rn = read(fd, cp, n);
	if (rn == C7_SYSERR) {
	    c7_status_add(errno, NULL);
	    free(tp);
	    return NULL;
	} else if (rn == 0) {
	    *cp = 0;
	    n = cp - tp;
	    if (sizep_o != NULL)
		*sizep_o = n;
	    np = c7_realloc(tp, n + 1);	// +1: last added null character
	    return (np != NULL) ? np : tp;
	}
	cp += rn;
    }
}

void *c7_file_read_x(const char *path, size_t *sizep_o)
{
    struct stat st;
    int fd = rx_open(path, &st);
    if (fd == C7_SYSERR)
	return NULL;

    char *p;
    if (S_ISREG(st.st_mode)) {
	p = rx_once(fd, st.st_size);
	if (p != NULL && sizep_o != NULL)
	    *sizep_o = st.st_size;
    } else {
	p = rx_repeat(fd, sizep_o);
    }
    if (p == NULL)
	c7_status_add(0, ": c7_file_read_x: %s\n", path_s(path));
    (void)close(fd);
    return p;
}

char **c7_file_readlines_x(const char *path, size_t *nlinep_o)
{
    char *whole, *line, **svtop, **svp, *str;
    int nlines;
    size_t z;

    if ((whole = c7_file_read_x(path, &z)) == NULL)
	return NULL;
    
    nlines = c7strcount(whole, '\n') + 2;
    z += nlines * (sizeof(*svp) + 1);	// `+1` mean NUL terminator
    if ((svtop = c7_malloc(z)) == NULL) {
	c7_status_add(0, ": c7_file_readlines_x: %s\n", path_s(path));
	free(whole);
	return NULL;
    }

    svp = svtop;
    line = whole;
    str = (char *)&svp[nlines];
    while (*line != 0) {
	char *end = (char *)c7strchr_next(line, '\n', 0);
	*svp++ = str;
	str = c7strbcpy_x(str, line, end) + 1;
	line = end;
    }
    *svp = NULL;
    free(whole);

    if (nlinep_o != NULL)
	*nlinep_o = svp - svtop;
    return svtop;
}


/*----------------------------------------------------------------------------
                                mmap interface
----------------------------------------------------------------------------*/

static void *dommap_fd(const char *path, int fd, int prot, size_t *sizep_io)
{
    struct stat sbuf;

    if (fstat(fd, &sbuf) == C7_SYSERR) {
	c7_status_add(errno, ": c7_file_mmap*: fstat: <%s>\n", path);
	return NULL;
    }

    if (*sizep_io == 0)
	*sizep_io = sbuf.st_size;
    else if (*sizep_io > sbuf.st_size) {
	if (ftruncate(fd, *sizep_io) == C7_SYSERR) {
	    c7_status_add(errno, ": c7_file_mmap*: ftruncate: <%s>, size: %ld\n",
			    path, *sizep_io);
	    return NULL;
	}
    }

    void *addr;
    int mflag = MAP_SHARED;
    if ((addr = mmap(NULL, *sizep_io, prot, mflag, fd, 0)) == (void *)C7_SYSERR) {
	c7_status_add(errno, ": c7_file_mmap*: mmap: <%s>, size: %ld\n",
			path, *sizep_io);
	return NULL;
    }

    return addr;
}

static void *dommap(const char *path, size_t *sizep_io, int oflag, int prot)
{
    int fd;
    if ((fd = open(path, oflag, 0600)) == C7_SYSERR) {
	c7_status_add(errno, ": c7_file_mmap*: open: <%s>\n", path);
	return NULL;
    }
    if ((oflag & O_CREAT) != 0)
	c7_file_inherit_owner(path);

    size_t __mapsize = 0;
    if (sizep_io == NULL) {
	sizep_io = &__mapsize;
    }
    void *addr = dommap_fd(path, fd, prot, sizep_io);
    (void)close(fd);
    return addr;
}

void *c7_file_mmap_r(const char *path, size_t *sizep_io)
{
    return dommap(path, sizep_io, O_RDONLY, PROT_READ);
}

void *c7_file_mmap_rw(const char *path, size_t *sizep_io, c7_bool_t create)
{
    int oflag = O_RDWR;
    if (create)
	oflag |= O_CREAT;
    return dommap(path, sizep_io, oflag, PROT_READ|PROT_WRITE);
}

void c7_file_munmap(void *addr, size_t size)
{
    (void)munmap(addr, size);
}


/*----------------------------------------------------------------------------
                            other file opearation
----------------------------------------------------------------------------*/

c7_bool_t c7_file_inherit_owner(const char *path)
{
    struct stat st;
    size_t len = strlen(path);
    char dir[len + 1];
    (void)c7strbcpy_x(dir, path, c7_path_name(path));
    if (stat(dir, &st) == C7_SYSOK) {
	if (chown(path, st.st_uid, st.st_gid) == C7_SYSOK) {
	    return C7_TRUE;
	}
	c7_status_add(errno, ": c7_file_inherit_owner: chown: %s, %u, %u\n",
		      path, (uint32_t)st.st_uid, (uint32_t)st.st_gid);
    } else
	c7_status_add(errno, ": c7_file_inherit_owner: stat: %s\n", dir);
    return C7_FALSE;
}


/*----------------------------------------------------------------------------
                               recursive mkdir
----------------------------------------------------------------------------*/

typedef struct _mkdir_prm_t_ {
    uid_t uid;
    gid_t gid;
    mode_t mode;
    char *path;
} _mkdir_prm_t;

static c7_bool_t mkdir_x(_mkdir_prm_t *prm)
{
    if (mkdir(prm->path, prm->mode) == C7_SYSOK) {
	_ = chown(prm->path, prm->uid, prm->gid);
	return C7_TRUE;
    }
    if (errno == EEXIST)
	return C7_TRUE;
    c7_status_add(errno, "mkdir failure: <%s>\n", prm->path);
    return C7_FALSE;
}

static c7_bool_t stepmkdir(char *namepos, _mkdir_prm_t *prm)
{
    for (;;) {
	if (namepos[0] == 0)
	    return C7_TRUE;
	char *nextslash = strchr(namepos, '/');
	if (nextslash == NULL)
	    return mkdir_x(prm);
	nextslash[0] = 0;
	if (!mkdir_x(prm))
	    return C7_FALSE;
	nextslash[0] = '/';
	namepos = nextslash + 1;
    }
}

c7_bool_t c7_file_mkdir(const char *path, mode_t mode, uid_t uid, gid_t gid)
{
    size_t n = strlen(path);
    char buf[n + 1];
    (void)strcpy(buf, path);
    _mkdir_prm_t prm = { .mode = mode, .uid = uid, .gid = gid, .path = buf };
    return stepmkdir((buf[0] == '/') ? &buf[1] : buf, &prm);
}


/*----------------------------------------------------------------------------
                              for compatibility
----------------------------------------------------------------------------*/

c7_bool_t (c7_file_search)(c7_str_t *sbp,
			   const char *name,
			   const char **pathlistv,
			   const char *default_suffix)
{
    return c7_path_search(sbp, name, pathlistv, default_suffix);
}

c7_str_t *(c7_file_special_path)(c7_str_t *sbp,
				 const char *envname_op,
				 const char *name, const char *suffix)
{
    return c7_path_c7spec(sbp, envname_op, name, suffix, C7_TRUE);
}

c7_str_t *(c7_file_special_find)(c7_str_t *sbp,
			       const char *envname_op,
			       const char *name, const char *suffix)
{
    return c7_path_c7spec(sbp, envname_op, name, suffix, C7_FALSE);
}
