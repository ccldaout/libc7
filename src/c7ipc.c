/*
 * c7ipc.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <c7ipc.h>
#include <c7status.h>
#include <c7app.h>


#if !defined(SUN_LEN)
# define SUN_LEN(su) (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif


/*----------------------------------------------------------------------------
                            INET(v4) domain socket
----------------------------------------------------------------------------*/

void c7_sockaddr_in_setport(struct sockaddr_in *inaddr, int port)
{
    inaddr->sin_port = htons(port);
}

c7_bool_t c7_sockaddr_in_ip(struct sockaddr_in *inaddr, uint32_t ipaddr, int port)
{
    (void)memset(inaddr, 0, sizeof(*inaddr));
    inaddr->sin_addr.s_addr = ipaddr;
    inaddr->sin_family = AF_INET;
    inaddr->sin_port   = htons(port);
#if defined(C7_CONFIG_SOCK_SIN_LEN)
    inaddr->sin_len = sizeof(*inaddr);
#endif
    return C7_TRUE;
}

c7_bool_t c7_sockaddr_in(struct sockaddr_in *inaddr, const char *host, int port)
{
    uint32_t ipaddr;
    if (host == 0 || host[0] == 0 || (host[0] == '*' && host[1] == 0))
	ipaddr = htonl(INADDR_ANY);
    else {
	struct addrinfo hints = { .ai_family = AF_INET };
	struct addrinfo *result;
	int err = getaddrinfo(host, NULL, &hints, &result);
	if (err != C7_SYSOK) {
	    c7_status_add(ENOENT, "getaddrinfo: %s\n", gai_strerror(err));
	    return C7_FALSE;
	}
	ipaddr = ((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	freeaddrinfo(result);
    }
    return c7_sockaddr_in_ip(inaddr, ipaddr, port);
}

c7_bool_t c7_sock_getself_in(int sock, struct sockaddr_in *inaddr)
{
    socklen_t n = sizeof(*inaddr);
    int ret = getsockname(sock, (struct sockaddr *)inaddr, &n);
    if (ret == C7_SYSERR)
	c7_status_add(errno, "setsockname(%1d, ...) [IPv4]\n", sock);
    return (ret == C7_SYSOK);
}

c7_bool_t c7_sock_getpeer_in(int sock, struct sockaddr_in *inaddr)
{
    socklen_t n = sizeof(*inaddr);
    int ret = getpeername(sock, (struct sockaddr *)inaddr, &n);
    if (ret == C7_SYSERR)
	c7_status_add(errno, "setpeername(%1d, ...) [IPv4]\n", sock);
    return (ret == C7_SYSOK);
}

int c7_udp(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == C7_SYSERR)
	c7_status_add(errno, "socket(UDP) error\n");
    return sock;
}

int c7_tcp(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == C7_SYSERR)
	c7_status_add(errno, "socket(TCP) error\n");
    return sock;
}

c7_bool_t c7_sock_bind_in(int sock, struct sockaddr_in *inaddr)
{
    int reuse = 1;
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (bind(sock, (struct sockaddr *)inaddr, sizeof(*inaddr)) != C7_SYSERR) {
	return C7_TRUE;
    }
    uint8_t *ip = (uint8_t *)&inaddr->sin_addr.s_addr;
    c7_status_add(errno, "bind(addr:%u.%u.%u.%u, port:%d)\n",
		    ip[0], ip[1], ip[2], ip[3], ntohs(inaddr->sin_port));
    return C7_FALSE;
}

c7_bool_t c7_sock_bind(int sock, const char *host, int port)
{
    struct sockaddr_in inaddr;
    if (c7_sockaddr_in(&inaddr, host, port)) {
	return c7_sock_bind_in(sock, &inaddr);
    }
    c7_status_add(0, "c7_sock_bind(host: %s)\n", host);
    return C7_FALSE;
}

c7_bool_t c7_sock_connect_in(int sock, struct sockaddr_in *inaddr)
{
    if (connect(sock, (struct sockaddr *)inaddr, sizeof(*inaddr)) != C7_SYSERR) {
	c7_tcp_keepalive(sock);
	return C7_TRUE;
    }
    uint8_t *ip = (uint8_t *)&inaddr->sin_addr.s_addr;
    c7_status_add(errno, "connect(addr:%u.%u.%u.%u, port:%d)\n",
		    ip[0], ip[1], ip[2], ip[3], ntohs(inaddr->sin_port));
    return C7_FALSE;
}

c7_bool_t c7_sock_connect(int sock, const char *host, int port)
{
    struct sockaddr_in inaddr;
    if (c7_sockaddr_in(&inaddr, host, port)) {
	return c7_sock_connect_in(sock, &inaddr);
    }
    c7_status_add(0, "c7_sock_connect(host: %s)\n", host);
    return C7_FALSE;
}

int c7_sock_accept(int server_sock)
{
    int sock = accept(server_sock, NULL, NULL);
    if (sock != C7_SYSERR)
	c7_tcp_keepalive(sock);
    else
	c7_status_add(errno, "accept(server_sock:%d)\n", server_sock);
    return sock;
}

int c7_udp_server(const char *host, int port)
{
    int sock = c7_udp();
    if (sock != C7_SYSERR) {
	if (c7_sock_bind(sock, host, port))
	    return sock;
	(void)close(sock);
    }
    return C7_SYSERR;
}
    
int c7_tcp_server(const char *host, int port, int listen_n)
{
    int sock;
    if ((sock = c7_tcp()) != C7_SYSERR) {
	if (c7_sock_bind(sock, host, port)) {
	    if (listen_n == -1)
		listen_n = SOMAXCONN;
	    if (listen(sock, listen_n) != C7_SYSERR)
		return sock;
	    c7_status_add(errno, "listen(sock<%s:%d>, listen_n:%d)\n",
			    host, port, listen_n);
	}
	(void)close(sock);
    }
    return C7_SYSERR;
}

int c7_tcp_client(const char *host, int port)
{
    int sock;
    if ((sock = c7_tcp()) != C7_SYSERR) {
	if (c7_sock_connect(sock, host, port)) {
	    return sock;
	}
	(void)close(sock);
    }
    return C7_SYSERR;
}

/*----------------------------------------------------------------------------
                              UNIX domain socket
----------------------------------------------------------------------------*/

static void setup_sockaddr_un(struct sockaddr_un *unaddr, size_t *addrlen, const char *path)
{
    (void)memset(unaddr, 0, sizeof(*unaddr));
    (void)strcpy(unaddr->sun_path, path);
    unaddr->sun_family = AF_UNIX;
    *addrlen = SUN_LEN(unaddr);
#if defined(__alpha) && (defined(_SOCKADDR_LEN) || defined(_XOPEN_SOURCE_EXTENDED))
    unaddr->sun_len = *addrlen + 1;
#endif
}

int c7_unixstream(void)
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == C7_SYSERR)
	c7_status_add(errno, "socket(UNIX/STREAM) error\n");
    return sock;
}

int c7_unixstream_server(const char *path, int listen_n)
{
    struct sockaddr_un unaddr;
    size_t addrlen;
    int sock;
    
    setup_sockaddr_un(&unaddr, &addrlen, path);

    if ((sock = c7_unixstream()) != C7_SYSERR) {
	if (bind(sock, (struct sockaddr *)&unaddr, addrlen) != C7_SYSERR) {
	    if (listen_n == -1)
		listen_n = SOMAXCONN;
	    if (listen(sock, listen_n) != C7_SYSERR)
		return sock;
	    c7_status_add(errno, "listen(sock<%s>, listen_n:%d)\n",
			    path, listen_n);
	} else
	    c7_status_add(errno, "bind(\"%s\")\n", path);
	(void)close(sock);
    }
    return C7_SYSERR;
}

int c7_unixstream_client(const char *path)
{
    struct sockaddr_un unaddr;
    size_t addrlen;
    int sock;
    
    setup_sockaddr_un(&unaddr, &addrlen, path);

    if ((sock = c7_unixstream()) != C7_SYSERR) {
	if (connect(sock, (struct sockaddr *)&unaddr, addrlen) != C7_SYSERR)
	    return sock;
	c7_status_add(errno, "connect(sock<%s>)\n", path);
	(void)close(sock);
    }
    return C7_SYSERR;
}


/*----------------------------------------------------------------------------
                                socket option
----------------------------------------------------------------------------*/

void c7_tcp_keepalive(int sock)
{
    c7_tcp_keepalive_detail(sock, 180, 5, 12);
}

void c7_tcp_keepalive_detail(int sock, int kpidle, int kpintvl, int kpcnt)
{
    int avail = (kpidle > 0);
    (void)setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &avail, sizeof(avail));
    if (avail == 0)
	return;
#if defined(TCP_KEEPALIVE)
    (void)setsockopt(sock, IPPROTO_TCP, TCP_KEEPALIVE, &kpidle, sizeof(kpidle));
#endif
#if defined(TCP_KEEPIDLE)
    (void)setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &kpidle, sizeof(kpidle));
#endif
#if defined(TCP_KEEPINTVL)
    (void)setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &kpintvl, sizeof(kpintvl));
#endif
#if defined(TCP_KEEPCNT)
    (void)setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &kpcnt, sizeof(kpcnt));
#endif
}

#define _setsockopt(s,lv,on,ov,ol)	__setsockopt(#lv ", " #on, s, lv, on, ov, ol)
static c7_bool_t __setsockopt(const char *args,
			      int sock, int level, int optname, void *optval, socklen_t optlen)
{
    int ret = setsockopt(sock, level, optname, optval, optlen);
    if (ret == C7_SYSERR)
	c7_status_add(errno, "setsockopt(%1d, %s, ...)\n", sock, args);
    return (ret == C7_SYSOK);
}

c7_bool_t c7_tcp_nodelay(int tcpdesc, c7_bool_t apply)
{
    return _setsockopt(tcpdesc, IPPROTO_TCP, TCP_NODELAY, &apply, 4);
}

c7_bool_t c7_sock_rcvbuf(int sock, int nbytes)
{
    return _setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &nbytes, sizeof(nbytes));
}

c7_bool_t c7_sock_sndbuf(int sock, int nbytes)
{
    return _setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &nbytes, sizeof(nbytes));
}

c7_bool_t c7_sock_sndtmo(int sock, int tmo_us)
{
    struct timeval tmout;
    tmout.tv_sec = tmo_us / C7_TIME_S_us;
    tmout.tv_usec = tmo_us % C7_TIME_S_us;
    return _setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tmout, sizeof(tmout));
}

c7_bool_t c7_sock_rcvtmo(int sock, int tmo_us)
{
    struct timeval tmout;
    tmout.tv_sec = tmo_us / C7_TIME_S_us;
    tmout.tv_usec = tmo_us % C7_TIME_S_us;
    return _setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tmout, sizeof(tmout));
}


/*----------------------------------------------------------------------------
                             read/write (binary)
----------------------------------------------------------------------------*/

c7_ipc_result_t
c7_ipc_read_n(int desc, void *b, const ssize_t req_n, ssize_t *act_n)
{
    ssize_t z, n = 0;
    while (n != req_n) {
	z = read(desc, b, (req_n - n));
	if (z <= 0) {
	    if (act_n)
		*act_n = n;
	    if (z == 0 && n == 0) {
		c7_status_reset(ECONNRESET, "read_n(req:%ld) -> 0 (maybe clsoed)\n", req_n);
	    } else {
		if (z == 0)
		    errno = EIO;
		c7_status_add(errno, "read_n(req:%ld) -> act:%ld\n", req_n, n);
	    }
	    return ((z == 0 && n == 0)     ? C7_IPC_RESULT_CLS :
		    (z == 0)               ? C7_IPC_RESULT_INCOMP :
		    (errno == EWOULDBLOCK) ? C7_IPC_RESULT_BUSY :	// non-blocking
		    /*                    */ C7_IPC_RESULT_ERR);
	}
	b = (char *)b + z;
	n += z;
    }
    if (act_n)
	*act_n = req_n;
    return C7_IPC_RESULT_OK;
}

c7_ipc_result_t
c7_ipc_write_n(int desc, const void *b, ssize_t req_n, ssize_t *act_n)
{
    ssize_t z, n = 0;
    while (n != req_n) {
	z = write(desc, b, (req_n - n));
	if (z <= 0) {
	    if (act_n)
		*act_n = n;
	    c7_status_add(errno, "write_n(req:%ld) -> act:%ld\n", req_n, n);
	    return ((errno == EWOULDBLOCK) ? C7_IPC_RESULT_BUSY :	// non-blocking
		    /*                    */ C7_IPC_RESULT_ERR);
	}
	b = (char *)b + z;
	n += z;
    }
    if (act_n)
	*act_n = req_n;
    return C7_IPC_RESULT_OK;
}

c7_ipc_result_t
c7_ipc_writev(int desc, struct iovec *iov, int ioc)
{
    ssize_t z;

    while (ioc > 0) {
	if (iov->iov_len == 0) {
	    ioc--;
	    iov++;
	    continue;
	}
	z = writev(desc, iov, ioc);
	if (z <= 0) {
	    c7_status_add(errno, "writev (not complete)\n");
	    return ((errno == EWOULDBLOCK) ? C7_IPC_RESULT_BUSY :	// non-blocking 
		    /*                    */ C7_IPC_RESULT_ERR);
	}
	while (z > 0) {
	    if (iov->iov_len <= (size_t)z) {
		z -= iov->iov_len;
		iov->iov_len = 0;
		iov->iov_base = NULL;
		iov++;
		ioc--;
	    } else if (iov->iov_len > (size_t)z) {
		iov->iov_len -= z;
		iov->iov_base = (char *)iov->iov_base + z;
		break;
	    }
	}
    }

    return C7_IPC_RESULT_OK;
}

c7_bool_t c7_ipc_waitfor(int desc, c7_bool_t *readable, c7_bool_t *writable, int tmo_us)
{
    fd_set rfds, wfds, *rfdsp, *wfdsp;
    struct timeval tmo, *tmop;
    int n;

    if (readable) {
	*readable = C7_FALSE;
	rfdsp = &rfds;
	FD_ZERO(rfdsp);
    } else
	rfdsp = NULL;

    if (writable) {
	*writable = C7_FALSE;
	wfdsp = &wfds;
	FD_ZERO(wfdsp);
    } else
	wfdsp = NULL;

    tmop = (tmo_us == -1) ? NULL : &tmo;

    for (;;) {
	c7_time_t beg_tv = 0, end_tv;
	if (tmo_us != -1) {
	    tmo.tv_sec  = tmo_us / 1000000;
	    tmo.tv_usec = tmo_us % 1000000;
	    if (tmo_us > 0)
		beg_tv = c7_time_us();
	}

	if (rfdsp)
	    FD_SET(desc, rfdsp);
	if (wfdsp)
	    FD_SET(desc, wfdsp);
	n = select(desc+1, rfdsp, wfdsp, NULL, tmop);
	if (n > 0) {
	    if (rfdsp && FD_ISSET(desc, rfdsp))
		*readable = C7_TRUE;
	    if (wfdsp && FD_ISSET(desc, wfdsp))
		*writable = C7_TRUE;
	    return C7_TRUE;
	}
	if (n == 0) {
	    c7_status_clear();
	    errno = EWOULDBLOCK;
	    return C7_FALSE;		// timeout
	}
	if (errno != EINTR) {
	    c7_status_add(errno, "waitfor: select error\n");
	    return C7_FALSE;
	}

	/* interrupt by signal: retry polling */
	if (tmo_us > 0) {
	    end_tv = c7_time_us();
	    tmo_us -= (end_tv - beg_tv);
	    if (tmo_us < 0)
		tmo_us = 0;
	}
    }
}

c7_bool_t c7_ipc_readable(int desc, int tmo_us)
{
    c7_bool_t readable;
    return c7_ipc_waitfor(desc, &readable, NULL, tmo_us);
}

c7_bool_t c7_ipc_writable(int desc, int tmo_us)
{
    c7_bool_t writable;
    return c7_ipc_waitfor(desc, NULL, &writable, tmo_us);
}


/*----------------------------------------------------------------------------
                             read/write (length + string)
----------------------------------------------------------------------------*/

c7_bool_t c7_ipc_puts(int desc, const char *s)
{
    int32_t size = strlen(s) + 1;
    struct iovec iov_buf[2];
    struct iovec *iov = iov_buf;
    int ioc = 2;
    iov[0].iov_base = &size;
    iov[0].iov_len = 4;
    iov[1].iov_base = (void *)s;
    iov[1].iov_len = size;
    size = htonl(size);
    return (c7_ipc_writev(desc, iov, ioc) == C7_IPC_RESULT_OK);
}

c7_bool_t c7_ipc_putsb(int desc, const c7_str_t *sbp)
{
    return c7_ipc_puts(desc, c7_strbuf(sbp));
}

char *c7_ipc_getsb(int desc, c7_str_t *sbp)
{
    c7_str_reuse(sbp);
    return c7_ipc_getsb_append(desc, sbp);
}

char *c7_ipc_getsb_append(int desc, c7_str_t *sbp)
{
    int32_t size;
    c7_ipc_result_t res = c7_ipc_read_n(desc, &size, 4, NULL);
    if (res == C7_IPC_RESULT_OK) {
	size = ntohl(size);
	int off = C7_STR_LEN(sbp);
	if (C7_STR_OK(c7_stropen(sbp, off, size))) {
	    char *buf = c7_strbuf(sbp) + off;
	    res = c7_ipc_read_n(desc, buf, size, NULL);
	    if (res == C7_IPC_RESULT_OK)
		return buf;
	}
    }
    return NULL;
}
