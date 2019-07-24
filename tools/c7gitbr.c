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

static void print(int prompt, const char *s)
{
    if (prompt)
	(void)fputs(" [", stdout);
    (void)fputs(s, stdout);
    if (prompt)
	(void)fputc(']', stdout);
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
    int prompt = 0;
    ssize_t size;
    char *HEAD;

    if (argv[1] && strcmp(argv[1], "-p") == 0)
	prompt = 1;

    if (cdgit() == 0)
	return 1;

    if ((HEAD = readall("HEAD", &size)) == NULL)
	return 1;

    char *p = HEAD + size;
    if (HEAD != p && p[-1] == '\n')
	*--p = 0;
    for (; HEAD != p; p--) {
	if (*p == '/') {
	    print(prompt, p+1);
	    return 0;
	}
    }
    print(prompt, "???");
    return 0;
}
