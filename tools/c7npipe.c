/*
 * c7npipe.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>


typedef enum bool_t {
    _FALSE = 0,
    _TRUE
} bool_t;

static ssize_t izwrite(int fd, const void *p, ssize_t z)
{
    ssize_t w, oz = z;
    while ((w = write(fd, p, z)) > 0 && w != z) {
	p = (char *)p + w;
	z -= w;
    } 
    return (w > 0) ? oz : -1;
}

static void eprintf(const char *format, ...)
{
    if (format) {
	va_list ap;
	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);
    }
}

static void eabort(int err, const char *format, ...)
{
    eprintf("npipe: %s\n", strerror(err));
    if (format) {
	va_list ap;
	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);
    }
    exit(1);
}

#if !defined(h_addr)
# define h_addr	h_addr_list[0]
#endif

static bool_t mksockaddr(const char *hostname, int portnum,
			 struct sockaddr_in *inaddr)
{
    struct hostent *hostent;

    (void)memset(inaddr, 0, sizeof(*inaddr));
    inaddr->sin_family = AF_INET;
    inaddr->sin_port   = htons((unsigned short)portnum);

    if (hostname == 0)
	inaddr->sin_addr.s_addr = INADDR_ANY;
    else {
	if (!(hostent = gethostbyname(hostname)))
	    return _FALSE;
	(void)memcpy(&inaddr->sin_addr, hostent->h_addr, hostent->h_length);
    }

    return _TRUE;
}

static int startserver(int sock, int portnum)
{
    int newsock;

    if (portnum != 0) {
	struct sockaddr_in inaddr;
	if (mksockaddr(0, portnum, &inaddr)) {
	    if (bind(sock, (struct sockaddr *)&inaddr, sizeof(inaddr)) != -1) {
		if (listen(sock, SOMAXCONN) != -1) {
		    eprintf("port number: %1d\n", portnum);
		    if ((newsock = accept(sock, 0, 0)) != -1) {
			(void)close(sock);
			return newsock;
		    }
		}
	    }
	}
	return -1;
    }

    for (portnum = 2000; portnum < 5000; portnum += 1) {
	if ((newsock = startserver(sock, portnum)) != -1)
	    return newsock;
    }
    return -1;
}

static int startclient(int sock, const char *hostname, int portnum)
{
    struct sockaddr_in inaddr;

    if (hostname == 0)
	hostname = "localhost";

    if (mksockaddr(hostname, portnum, &inaddr)) {
	if (connect(sock, (struct sockaddr *)&inaddr, sizeof(inaddr)) != -1)
	    return sock;
    }

    return -1;
}

static void copy(int in, int out)
{
    static char buffer[32768];
    ssize_t size;

    while ((size = read(in, buffer, sizeof(buffer))) > 0) {
	if (izwrite(out, buffer, size) == -1)
	    eabort(errno, "cannot write.\n");
    }
    if (size < 0)
	eabort(errno, "cannot raed.\n");
}

int main(int argc, char **argv)
{
    int sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	eabort(errno, "cannot create socket\n");

    if (argc == 1) {
	if ((sock = startserver(sock, 0)) == -1)
	    eabort(errno, "cannot start service.\n");
	copy(sock, 1);
    } else {
	char *sep, *addr = argv[1];
	if ((sep = strrchr(addr, ':')) == 0)
	    eabort(0, "<addr> must be <host:portnum>\n");
	*sep = 0;
	if ((sock = startclient(sock, *addr ? addr : 0, atol(sep+1))) == -1)
	    eabort(errno, "cannot connect to service.\n");
	copy(0, sock);
    }

    return 0;
}
