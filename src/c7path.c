/*
 * c7path.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <sys/stat.h>
#include <c7app.h>
#include <c7deque.h>
#include <c7file.h>
#include <c7string.h>
#include <c7status.h>


/*----------------------------------------------------------------------------
                        fundamental path manipulation
----------------------------------------------------------------------------*/

c7_str_t *c7_path_getcwd(c7_str_t *sbp)
{
    if (sbp == NULL)
	sbp = c7_str_new_sg();
    int off = C7_STR_LEN(sbp);
    for (int z = 64; ; z += 64) {
	if (!c7_str_alloc(sbp, z)) {
	    return sbp;
	}
	int n = C7_STR_SIZE(sbp) - off;
	if (getcwd(c7_strbuf(sbp) + off, n) != NULL) {
	    C7_STR_SETCUR(sbp);
	    return sbp;	
	}
	if (errno != ERANGE) {
	    c7_status_add(errno, "getcwd: failure\n");
	    C7_STR_SETERR(sbp);
	    return sbp;
	}
    }
}


c7_bool_t c7_path_isdir(const char *path)
{
    struct stat st;
    return (stat(path, &st) == C7_SYSOK && S_ISDIR(st.st_mode));
}


c7_str_t *c7_path_untildize(c7_str_t *sbp, const char *path)
{
    if (sbp == NULL)
	sbp = c7_str_new_sg();
    if (path[0] != '~')
	return c7_strcpy(sbp, path);
    if (path[1] == '/' || path[1] == 0) {
	char *home = getenv("HOME");
	if (home != NULL && home[0] == '/' && c7_path_isdir(home)) {
	    (void)c7_strcpy(sbp, home);
	    path++;
	} else {
	    struct passwd *pw = c7_app_getpwuid_x(geteuid());
	    if (pw != NULL) {
		(void)c7_strcpy(sbp, pw->pw_dir);
		free(pw);
		path++;
	    }
	}
    } else {
	const char *name_end = c7strchr_x(path, '/', NULL);
	char user[name_end - path];			// [CAUTION] stack
	(void)c7strbcpy_x(user, &path[1], name_end);
	struct passwd *pw = c7_app_getpwnam_x(user);
	if (pw != NULL) {
	    (void)c7_strcpy(sbp, pw->pw_dir);
	    free(pw);
	    path = name_end;
	}
    }
    return c7_strcpy(sbp, path);
}


c7_str_t *c7_path_ortho(c7_str_t *sbp, char *path)
{
    if (sbp == NULL)
	sbp = c7_str_new_sg();

    c7_deque_t dq = c7_deque_create(sizeof(char*), NULL);
    if (dq == NULL) {
	C7_STR_SETERR(sbp);
	return sbp;
    }

    char *ctx = NULL, *dir;
    if ((dir = strtok_r(path, "/", &ctx)) != NULL) {
	do {
	    if (*dir == 0)
		continue;
	    if (strcmp(dir, "..") == 0) {
		(void)c7_deque_pop_tail(dq);
	    } else if (strcmp(dir, ".") == 0) {
		;
	    } else {
		if (c7_deque_push_tail(dq, &dir) == NULL) {
		    C7_STR_SETERR(sbp);
		    c7_deque_destroy(dq);
		    return sbp;
		}
	    }
	} while ((dir = strtok_r(NULL, "/", &ctx)) != NULL);
    }

    char **dirp;
    c7_deque_foreach(dq, dirp) {
	(void)c7_strcpy(c7_stradd(sbp, '/'), *dirp);
    }

    c7_deque_destroy(dq);
    return sbp;
}


static c7_str_t *joinpath(const char *path, const char *base_op, c7_bool_t untildize)
{
    c7_str_t *tmp;

    if (base_op == NULL)
	tmp = c7_path_getcwd(NULL);
    else if (base_op[0] == '~' && untildize)
	tmp = c7_path_untildize(NULL, base_op);
    else
	tmp = c7_strcpy(NULL, base_op);
    if (C7_STR_ERR(tmp))
	return tmp;

    if (c7_strbuf(tmp)[0] != '/') {
	c7_status_add(errno = EINVAL, "Cannot get joinpath from base_op:%s\n",
		      base_op == NULL ? "(null)" : base_op);
	C7_STR_SETERR(tmp);
	return tmp;
    }

    return c7_strcpy(c7_stradd(tmp, '/'), path);
}

c7_str_t *c7_path_abs(c7_str_t *sbp, const char *path, const char *base_op, c7_bool_t untildize)
{
    if (sbp == NULL)
	sbp = c7_str_new_sg();

    c7_sg_push();

    if (path[0] == '/') {
	c7_str_t *tmp = c7_strcpy(NULL, path);
	if (C7_STR_OK(tmp))
	    path = c7_strbuf(tmp);
	else
	    C7_STR_SETERR(sbp);
    }
    if (untildize && path[0] == '~') {
	c7_str_t *tmp = c7_path_untildize(NULL, path);
	if (c7_strbuf(tmp)[0] == '/')
	    path = c7_strbuf(tmp);
    } 
    if (path[0] != '/') {
	c7_str_t *tmp = joinpath(path, base_op, untildize);
	if (C7_STR_OK(tmp))
	    path = c7_strbuf(tmp);
	else
	    C7_STR_SETERR(sbp);
    }
    if (C7_STR_OK(sbp)) {
	c7_path_ortho(sbp, (char *)path);
    }

    c7_sg_pop();
    return sbp;
}


/*----------------------------------------------------------------------------
                         sarch file by directory list
----------------------------------------------------------------------------*/

static const char *file_search(const char *name,
			       const char **pathlistv)
{
    c7_str_t *paths = c7_str_new_sg();
    c7_str_t *path = c7_str_new_sg();

    for (; *pathlistv; pathlistv++) {
	c7_str_reuse(paths);
	if (C7_STR_ERR(c7_streval_env(paths, *pathlistv)))
	    continue;
	char *p = c7_strbuf(paths);
	char *context;
	if ((p = strtok_r(p, ":", &context)) != NULL) {
	    do {
		c7_str_reuse(path);
		(void)c7_sprintf(path, "%s/%s", p, name);
		if (access(c7_strbuf(path), F_OK) == C7_SYSOK) {
		    return c7_strbuf(path);
		}
	    } while ((p = strtok_r(NULL, ":", &context)) != NULL);
	}
    }
    return NULL;
}

c7_bool_t c7_path_search(c7_str_t *sbp,
			 const char *name,
			 const char **pathlistv,
			 const char *default_suffix)
{	
    c7_sg_push();
    c7_status_clear();
    c7_str_t *fn = c7_strcpy(NULL, name);
    if (strchr(name, '.') == NULL) {
	(void)c7_strcpy(fn, default_suffix);
	name = c7_strbuf(fn);
    }
    const char *path;
    if (strchr(name, '/') != NULL)
	path = (access(name, F_OK) == C7_SYSOK) ? name : NULL;
    else
	path  = file_search(name, pathlistv);
    if (path != NULL)
	(void)c7_strcpy(sbp, path);
    else
	c7_status_add(0, ": cannot find '%s'\n", name);
    c7_sg_pop();

    return (path != NULL);
}


/*----------------------------------------------------------------------------
                               c7 special file
----------------------------------------------------------------------------*/

static c7_bool_t validdir(const char *path)
{
    return (c7_path_isdir(path) &&
	    access(path, W_OK) == C7_SYSOK);
}

static c7_str_t *init_c7spec(c7_str_t *sbp,
			     const char *envname_op,
			     const char *name, const char *suffix)
{
    if (c7strmatch_tail(name, suffix, (void *)NULL) != -1)
	suffix = "";

    if (strchr(name, '/') != NULL)
	return c7_strcpy(c7_strcpy(sbp, name), suffix);

    const char *s;
    if (envname_op != NULL && (s = getenv(envname_op)) != NULL) {
	if (validdir(s))
	    return c7_sprintf(sbp, "%s/%s%s", s, name, suffix);
    }

    envname_op = "C7_ROOT_DIR";
    if (envname_op != NULL && (s = getenv(envname_op)) != NULL) {
	if (validdir(s))
	    return c7_sprintf(sbp, "%s/%s%s", s, name, suffix);
    }

    const char *home = c7getenv_x("HOME", "");
    sbp = c7_sprintf(sbp, "%s/.c7", home);
    if (validdir(c7_strbuf(sbp)))
	return c7_sprintf(sbp, "/%s%s", name, suffix);
    c7_str_reuse(sbp);

    return c7_sprintf(sbp, "%s/.%s%s", home, name, suffix);
}

static c7_str_t *find_c7spec(c7_str_t *sbp,
			     const char *envname_op,
			     const char *name, const char *suffix)
{
    if (c7strmatch_tail(name, suffix, (void *)NULL) != -1)
	suffix = "";

    if (strchr(name, '/') != NULL)
	return c7_strcpy(c7_strcpy(sbp, name), suffix);

    const char *s;
    if (envname_op != NULL && (s = getenv(envname_op)) != NULL) {
	sbp = c7_sprintf(sbp, "%s/%s%s", s, name, suffix);
	if (access(c7_strbuf(sbp), F_OK) == C7_SYSOK)
	    return sbp;
	c7_str_reuse(sbp);
    }

    envname_op = "C7_ROOT_DIR";
    if (envname_op != NULL && (s = getenv(envname_op)) != NULL) {
	sbp = c7_sprintf(sbp, "%s/%s%s", s, name, suffix);
	if (access(c7_strbuf(sbp), F_OK) == C7_SYSOK)
	    return sbp;
	c7_str_reuse(sbp);
    }

    const char *home = c7getenv_x("HOME", "");
    sbp = c7_sprintf(sbp, "%s/.c7/%s%s", home, name, suffix);
    if (access(c7_strbuf(sbp), F_OK) == C7_SYSOK)
	return sbp;
    c7_str_reuse(sbp);

    return c7_sprintf(sbp, "%s/.%s%s", home, name, suffix);
}

c7_str_t *c7_path_c7spec(c7_str_t *sbp,
			 const char *envname_op,
			 const char *name, const char *suffix,
			 c7_bool_t produce)
{
    if (produce)
	return init_c7spec(sbp, envname_op, name, suffix);
    else
	return find_c7spec(sbp, envname_op, name, suffix);
}
