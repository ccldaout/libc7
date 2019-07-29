/*
 * c7gitbr.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *readall(const char *path, ssize_t *size)
{
    struct stat st;
    char *buf;
    int fd;

    if ((fd = open(path, O_RDONLY)) != 0) {
	ssize_t sizealt;
	if (size == 0)
	    size = &sizealt;
	(void)fstat(fd, &st);
	if ((buf = malloc(st.st_size + 1)) != 0) {
	    *size = read(fd, buf, st.st_size);
	    if (*size == st.st_size) {
		(void)close(fd);
		buf[*size] = 0;
		return buf;
	    }
	    free(buf);
	}
	(void)close(fd);
    }
    return 0;
}

static void print(const char *curr_dir, const char *base_dir, const char *branch)
{
    size_t n = strlen(base_dir);
    size_t m = 40;
    char *e = getenv("C7_PROMPT_MAX");
    char buff[strlen(curr_dir) + strlen(branch ? branch : "") + 32];

    if (e != NULL) {
	if ((m = strtol(e, NULL, 0)) < 24)
	    m = 24;
    }

    if (strncmp(curr_dir, base_dir, n) == 0) {
	if (branch != NULL)
	    (void)sprintf(buff, "~%s [%s] ", curr_dir+n, branch);
	else
	    (void)sprintf(buff, "~%s ", curr_dir+n);
    } else {
	const char *p = strrchr(curr_dir, '/');
	if (p != NULL && p != curr_dir && p[1] == 0) {
	    for (p--; curr_dir != p && *p != '/'; p--);
	}
	if (p == NULL)
	    p = curr_dir;
	else
	    p++;
	if (branch != NULL)
	    (void)sprintf(buff, "(%s)%% [%s] ", p, branch);
	else
	    (void)sprintf(buff, "(%s)%% ", p);
    }
    
    if ((n = strlen(buff)) > m) {
	n -= m;
	buff[n] = buff[n+1] = buff[n+2] = '.';
    } else {
	n = 0;
    }
    (void)fputs(&buff[n], stdout);
    (void)fflush(stdout);
}

static int cdgit(void)
{
    struct stat st1, st2;
    
    (void)stat(".", &st1);

    for (;;) {
	if (access(".git", F_OK) == 0) {
	    if (chdir(".git") == 0)
		return 1;
	    char *p, *s = readall(".git", NULL);
	    if (s == NULL)
		return 0;
	    if (strncmp(s, "gitdir: ", 8) != 0)
		return 0;
	    s += 8;
	    if ((p = strchr(s, '\n')) != NULL)
		*p = 0;
	    if (chdir(s) == 0)
		return 1;
	    return 0;
	}
	st2 = st1;
	if (chdir("..") != 0)
	    return 0;
	(void)stat(".", &st1);
	if (st1.st_ino == st2.st_ino)
	    return 0;
    }
}

int main(int argc, char **argv)
{
    ssize_t size;
    char *HEAD;

    if (argc == 1 || argc > 3) {
	(void)fprintf(stderr, "Usage: c7prompt CURRENT_DIR [BASE_DIR]\n");
	exit(1);
    }
    char *curr_dir = argv[1];
    char *base_dir = argv[2];
    if (base_dir == NULL && (base_dir = getenv("HOME")) == NULL)	
	base_dir = "";

    if (cdgit() == 0 ||
	(HEAD = readall("HEAD", &size)) == NULL) {
	print(curr_dir, base_dir, NULL);
	return 1;
    }

    char *p = HEAD + size;
    if (HEAD != p && p[-1] == '\n')
	*--p = 0;
    for (; HEAD != p; p--) {
	if (*p == '/') {
	    print(curr_dir, base_dir, p+1);
	    return 0;
	}
    }
    print(curr_dir, base_dir, "?");
    return 0;
}
