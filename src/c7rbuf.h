/*
 * c7rbuf.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_RBUF_H_LOADED__
#define __C7_RBUF_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7types.h>
#include <sys/uio.h>


typedef uint32_t rbuf_unit_t;

typedef struct rbuf_mark_t_ {
    rbuf_unit_t rewindcnt;
    rbuf_unit_t nextaddr;
} rbuf_mark_t;

#define C7_RBUF_HDR_n		(4)

#define C7_RBUF_SIZE_MAX	(512*1024*1024)		// 512MB

#define C7_RBUF_SIZE(size_b)	(C7_RBUF_HDR_n + ((size_b) / sizeof(rbuf_unit_t)))

#define C7_RBUF_DECLARE(name, size_b)	\
    rbuf_unit_t name[C7_RBUF_SIZE(size_b)] = { (size_b), (size_b)*2, (size_b)*2 }

// Usage: static C7_RBUF_DECLARE(RingBuffer, 1024);


rbuf_unit_t *c7_rbuf_new(uint32_t size_of_data);
void c7_rbuf_init(rbuf_unit_t *rba, uint32_t size_of_rball);
void c7_rbuf_put(rbuf_unit_t *rba, void *data, rbuf_unit_t size_b);
void c7_rbuf_putiov(rbuf_unit_t *rba, struct iovec *iov, int ioc);
void c7_rbuf_clear(rbuf_unit_t *rba);
c7_bool_t c7_rbuf_is_empty(rbuf_unit_t *rba);
c7_bool_t c7_rbuf_scan(rbuf_unit_t *rba, unsigned max_call,
		       c7_bool_t (*callback)(void *data, rbuf_unit_t size_b, void *__prm),
		       void *__prm);
c7_bool_t c7_rbuf_scan_r(rbuf_unit_t *rba, unsigned max_call,
			 c7_bool_t (*callback)(void *data, rbuf_unit_t size_b, void *__prm),
			 void *__prm);


#if defined(__cplusplus)
}
#endif
#endif // c7rbuf.h
