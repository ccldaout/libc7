/*
 * _private_mlog.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_PRIVATE_MLOG_H_LOADED__
#define __C7_PRIVATE_MLOG_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif


#include <c7deque.h>
#include <c7memory.h>
#include <c7mlog.h>


// BEGIN: same definition with libc7xx/_private_mlog.hpp

// _REVISION history:
// 1: initial
// 2: lnk data lnk -> lnk data thread_name lnk
// 3: thread name, source name
// 4: record max length for thread name and source name
// 5: new linkage format & enable inter process lock
// 6: lock free, no max_{tn|sn}_size
// 7: multi partition
#define _REVISION	(7)


typedef uint32_t raddr_t;


#define _IHDRSIZE		c7_align(sizeof(_hdr_t), 16)

// _rec_t internal control flags (6bits)
#define _REC_CONTROL_CHOICE	(1U << 0)

#define _TN_MAX			(63)	// tn_size:6
#define _SN_MAX			(63)	// sn_size:6

#define _PART_CNT		(C7_LOG_MAX+1)
#define _TOO_LARGE		((raddr_t)(-1))


// partition entry
typedef struct c7_mlog_partition_t {
    volatile raddr_t nextaddr;
    uint32_t size_b;		// ring buffer size
} _partition_t;


// file header
typedef struct c7_mlog_hdr_t_ {
    uint32_t rev;
    volatile uint32_t cnt;
    uint32_t hdrsize_b;		// user header size
    uint32_t __pad;
    char hint[64];
    _partition_t part[_PART_CNT];
} _hdr_t;


// record header
typedef struct c7_mlog_rec_t_ {
    raddr_t size;		// record size (rec_t + log data + names + raddr_t)
    uint32_t order;		// (weak )record order
    c7_time_t time_us;		// time stamp in micro sec.
    uint64_t minidata;		// mini data (out of libc7)
    uint64_t level   :  3;	// log level 0..7
    uint64_t category:  5;	// log category 0..31
    uint64_t tn_size :  6;	// length of thread name (exclude null character)
    uint64_t sn_size :  6;	// length of source name (exclude null character)
    uint64_t src_line: 14;	// source line
    uint64_t control :  6;	// (internal control flags)
    uint64_t __rsv1  : 24;
    uint32_t pid;		// process id
    uint32_t th_id;		// thread id
    uint32_t br_order;		// ~order
} _rec_t;

// END


/*----------------------------------------------------------------------------
                                   rbuffer
----------------------------------------------------------------------------*/

typedef struct _rbuf_t {
    _partition_t *part;
    char *top, *end;
    raddr_t size;
} _rbuf_t;

static inline void rbuf_init(_rbuf_t *rb, void *headaddr, uint64_t off, _partition_t *part)
{
    rb->part = part;
    rb->top  = (char *)headaddr + off;
    rb->end  = rb->top + part->size_b;
    rb->size = part->size_b;
}

static inline raddr_t rbuf_size(_rbuf_t *rb)
{
    return rb->size;
}

static inline raddr_t rbuf_nextaddr(_rbuf_t *rb)
{
    return rb->part->nextaddr;
}

static inline raddr_t rbuf_reserve(_rbuf_t *rb, raddr_t size_b)
{
    // check size to be written
    if ((size_b + 32) > rb->size) {	// (C) ensure rechdr.size < hdr_->logsize_b
	return _TOO_LARGE;		// data size too large
    }

    volatile raddr_t * const nextaddr_p = &rb->part->nextaddr;
    raddr_t addr, next;
    do {
	addr = *nextaddr_p;
	next = (addr + rb->size) % rb->size;
	// addr != next is ensured by above check (C)
    } while (__sync_val_compare_and_swap(nextaddr_p, addr, next) != addr);
    return addr;
}

static inline void rbuf_get(_rbuf_t *rb, raddr_t addr, raddr_t size, void *__ubuf)
{
    char *ubuf = __ubuf;
    char *rbuf = rb->top + (addr % rb->size);
    raddr_t rrest = rb->end - rbuf;
	
    while (size > 0) {
	raddr_t cpsize = (size < rrest) ? size : rrest;

	(void)memcpy(ubuf, rbuf, cpsize);
	ubuf += cpsize;
	size -= cpsize;

	rbuf  = rb->top;
	rrest = rb->size;
    }
}

static inline raddr_t rbuf_put(_rbuf_t *rb, raddr_t addr, raddr_t size, const void *__ubuf)
{
    const raddr_t ret_addr = addr + size;

    const char *ubuf = __ubuf;
    char *rbuf = rb->top + (addr % rb->size);
    raddr_t rrest = rb->end - rbuf;

    while (size > 0) {
	raddr_t cpsize = (size < rrest) ? size : rrest;

	(void)memcpy(rbuf, ubuf, cpsize);
	ubuf += cpsize;
	size -= cpsize;

	rbuf  = rb->top;
	rrest = rb->size;
    }

    return ret_addr;
}

static inline void rbuf_clear(_rbuf_t *rb)
{
    if (rb->size > 0) {
	raddr_t addr = 0;
	rb->part->nextaddr = rbuf_put(rb, addr, sizeof(addr), &addr);
    }
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

struct c7_mlog_t_ {
    // writer/reader
    _hdr_t *hdr;		// first member
    _rbuf_t rb[_PART_CNT];
    int n_rb;

    // writer
    size_t mmapsize_b;
    uint32_t flags;
    uint32_t pid;

    // reader
    c7_deque_t recs;		// rec_index_t
    c7_vbuf_t vbuf;
    int max_sn_size;
    int max_tn_size;
};


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

static inline char *mlogpath_x(const char *name, c7_bool_t exists)
{
    c7_str_t sb = C7_STR_INIT_MA();
    if (exists) {
	(void)c7_file_special_find(&sb, C7_MLOG_DIR_ENV, name, ".mlog");
    } else {
	(void)c7_file_special_path(&sb, C7_MLOG_DIR_ENV, name, ".mlog");
    }
    if (C7_STR_ERR(&sb)) {
	return NULL;
    }
    return c7_str_release(&sb);
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/


#if defined(__cplusplus)
}
#endif
#endif // _private_mlog.h
