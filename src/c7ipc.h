/*
 * c7ipc.h
 *
 * https://ccldaout.github.io/libc7/group__c7ipc.html
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_IPC_H_LOADED__
#define __C7_IPC_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <signal.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <c7string.h>


typedef enum c7_ipc_result_t_ {
    // c7_ipc_write_n / c7_ipc_writev / c7_ipc_read_n
    C7_IPC_RESULT_OK,		/* all data has been processed successfully */
    C7_IPC_RESULT_ERR,		/* detect other error */
    C7_IPC_RESULT_BUSY,		/* detect EWOULDBLOCK error (non-blocking socket) */
    // only c7_ipc_read_n
    C7_IPC_RESULT_CLS,		/* detect CLOSED before some data is read */
    C7_IPC_RESULT_INCOMP,	/* detect CLOSED after some data has been read */
} c7_ipc_result_t;

void c7_sockaddr_in_setport(struct sockaddr_in *inaddr, int port);
c7_bool_t c7_sockaddr_in_ip(struct sockaddr_in *inaddr, uint32_t ipaddr, int port);
c7_bool_t c7_sockaddr_in(struct sockaddr_in *inaddr, const char *host, int port);
c7_bool_t c7_sock_getself_in(int sock, struct sockaddr_in *inaddr);
c7_bool_t c7_sock_getpeer_in(int sock, struct sockaddr_in *inaddr);

int c7_udp(void);
int c7_tcp(void);
int c7_unixstream(void);
c7_bool_t c7_sock_bind_in(int sock, struct sockaddr_in *inaddr);
c7_bool_t c7_sock_bind(int sock, const char *host, int port);
c7_bool_t c7_sock_connect_in(int sock, struct sockaddr_in *inaddr);
c7_bool_t c7_sock_connect(int sock, const char *host, int port);
int c7_sock_accept(int server_socket);
int c7_udp_server(const char *host, int port);
int c7_tcp_server(const char *host, int port, int listen_n);
int c7_tcp_client(const char *host, int port);
int c7_unixstream_server(const char *path, int listen_n);
int c7_unixstream_client(const char *path);

void c7_tcp_keepalive(int sock);
void c7_tcp_keepalive_detail(int sock, int kpidle, int kpintvl, int kpcnt);
c7_bool_t c7_tcp_nodelay(int tcpdesc, c7_bool_t apply);
c7_bool_t c7_sock_rcvbuf(int sock, int nbytes);
c7_bool_t c7_sock_sndbuf(int sock, int nbytes);
c7_bool_t c7_sock_sndtmo(int sock, int tmo_us);
c7_bool_t c7_sock_rcvtmo(int sock, int tmo_us);

c7_ipc_result_t c7_ipc_read_n(int desc, void *b, const ssize_t req_n, ssize_t *act_n);
c7_ipc_result_t c7_ipc_write_n(int desc, const void *b, ssize_t req_n, ssize_t *act_n);
c7_ipc_result_t c7_ipc_writev(int desc, struct iovec *iov, int ioc);	/* iov will be changed */
c7_bool_t c7_ipc_waitfor(int desc, c7_bool_t *readable, c7_bool_t *writable, int tmp_us);
c7_bool_t c7_ipc_readable(int desc, int tmo_us);
c7_bool_t c7_ipc_writable(int desc, int tmo_us);

c7_bool_t c7_ipc_puts(int desc, const char *s);
c7_bool_t c7_ipc_putsb(int desc, const c7_str_t *sbp);
char *c7_ipc_getsb(int desc, c7_str_t *sbp);
char *c7_ipc_getsb_append(int desc, c7_str_t *sbp);


#if defined(__cplusplus)
}
#endif
#endif /* c7ipc.h */
