/*
 * c7mlog.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "_private.h"
#include <c7file.h>
#include <c7fd.h>
#include <c7mlog.h>
#include <c7status.h>
#include <c7thread.h>
#include <c7app.h>


// _REVISION history:
// 1: initial
// 2: lnk data lnk -> lnk data thread_name lnk
// 3: thread name, source name
// 4: record max length for thread name and source name
#define _REVISION	4


typedef uint32_t raddr_t;


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

typedef struct _rbuf_t {
    char *top, *end;
    raddr_t size;
} _rbuf_t;

static void rbuf_init(_rbuf_t *rb, void *addr, raddr_t size)
{
    rb->end = (rb->top = addr) + (rb->size = size);
}

static void rbuf_get(_rbuf_t *rb, raddr_t addr, raddr_t size, void *__ubuf)
{
    char *ubuf = __ubuf;
    char *rbuf;
    raddr_t cpsize, rrest;

    rbuf = rb->top + (addr % rb->size);
    rrest = rb->end - rbuf;
	
    while (size > 0) {
	cpsize = (size < rrest) ? size : rrest;

	(void)memcpy(ubuf, rbuf, cpsize);
	ubuf += cpsize;
	size -= cpsize;

	rbuf = rb->top;
	rrest = rb->size;
    }
}

static raddr_t rbuf_put(_rbuf_t *rb, raddr_t addr, raddr_t size, const void *__ubuf)
{
    const raddr_t ret_addr = addr + size;
    const char *ubuf = __ubuf;
    char *rbuf;
    raddr_t cpsize, rrest;

    rbuf = rb->top + (addr % rb->size);
    rrest = rb->end - rbuf;
	
    while (size > 0) {
	cpsize = (size < rrest) ? size : rrest;

	(void)memcpy(rbuf, ubuf, cpsize);
	ubuf += cpsize;
	size -= cpsize;

	rbuf = rb->top;
	rrest = rb->size;
    }

    return ret_addr;
}


/*----------------------------------------------------------------------------
                                  structure
----------------------------------------------------------------------------*/

#define _IHDRSIZE		c7_align(sizeof(_hdr_t), 16)
#define _LNK_CONTROL_CHOICE	(1U << 0)

// mlog flags
#define _MLOG_MUTEX		(1U << 28)
#define _MLOG_MUTEX_INTERNAL	(1U << 29)
#define _MLOG_ADVLOCK		(1U << 31)

#define _TN_MAX			(63)	// tn_size:6
#define _SN_MAX			(63)	// sn_size:6

typedef struct c7_mlog_hdr_t_ {	// ring-log header
    uint32_t rev;
    uint32_t cnt;
    uint32_t hdrsize_b;		// user header size
    uint32_t logsize_b;		// ring buffer size
    uint32_t nextlnkaddr;	// ring addrress to next log_lnk_t
    char hint[64];
    uint8_t max_tn_size;	// maximum length of thread name
    uint8_t max_sn_size;	// maximum length of source name
} _hdr_t;

typedef struct c7_mlog_lnk_t_ {	// linkage data
    // prevlnkoff must be head of _lnk_t (cf. comment of calling rbuf_put in setuplnk())
    uint32_t prevlnkoff;	// offset to previous record
    uint32_t nextlnkoff;	// offset to next record
    c7_time_t time_us;		// time stamp in micro sec.
    uint64_t minidata;		// mini data (out of libc7)
    uint64_t level   :  3;	// log level 0..7
    uint64_t category:  5;	// log category 0..31
    uint64_t tn_size :  6;	// length of thread name (exclude null character)
    uint64_t sn_size :  6;	// length of source name (exclude null character)
    uint64_t src_line: 14;	// source line
    uint64_t control :  6;	// (internal control flags)
    uint64_t __rsv1  : 24;
    uint32_t th_id;		// thread id
    uint32_t order;		// record serial number
    uint32_t size;		// record size (data + xxx name)
    uint32_t __rsv2;
} _lnk_t;

struct c7_mlog_t_ {
    _rbuf_t rb;
    _hdr_t *hdr;
    char *path;
    char snbuf[_SN_MAX + 1];
    size_t mmapsize_b;
    size_t maxdsize_b;
    uint32_t flags;
    void *lock;
    void (*locker)(void *lock);
    void (*unlocker)(void *lock);
};


/*----------------------------------------------------------------------------
                               write operation
----------------------------------------------------------------------------*/

static char *mlogpath_x(const char *name, c7_bool_t exists)
{
    c7_str_t sb = C7_STR_INIT_MA();
    if (exists)
	(void)c7_file_special_find(&sb, C7_MLOG_DIR_ENV, name, ".mlog");
    else
	(void)c7_file_special_path(&sb, C7_MLOG_DIR_ENV, name, ".mlog");
    if (C7_STR_ERR(&sb))
	return NULL;
    return c7_str_release(&sb);
}

static c7_mlog_t mlog_new(const char *path, size_t hdrsize_b, size_t logsize_b)
{
    c7_mlog_t g;
    if (logsize_b < 1024 || logsize_b > (1U<<30)) {
	c7_status_add(EINVAL, ": invalid logsize_b: %ld\n", logsize_b);
	return NULL;
    }
    if ((g = c7_calloc(sizeof(*g), 1)) == NULL)
	return NULL;
    g->path = NULL;
    g->mmapsize_b = _IHDRSIZE + hdrsize_b + logsize_b;
    g->maxdsize_b = logsize_b - 2*sizeof(_lnk_t);
    if ((g->hdr = c7_file_mmap_rw(path, &g->mmapsize_b, C7_TRUE)) == NULL) {
	free(g);
	return NULL;
    }
    rbuf_init(&g->rb, (char *)g->hdr + _IHDRSIZE + hdrsize_b, logsize_b);
    return g;
}

c7_mlog_t c7_mlog_open_w(const char *name, size_t hdrsize_b, size_t logsize_b,
			 const char *hint_op, uint32_t flags)
{
    char *path = mlogpath_x(name, C7_FALSE);
    if (path == NULL)
	return NULL;
    c7_mlog_t g = mlog_new(path, hdrsize_b, logsize_b);
    if (g == NULL) {
	free(path);
	return NULL;
    }

    g->path = path;
    g->flags = flags;
    g->lock = NULL;
    g->locker = NULL;
    g->unlocker = NULL;

    if (g->hdr->hdrsize_b != hdrsize_b ||
	g->hdr->logsize_b != logsize_b ||
	g->hdr->rev != _REVISION) {
	(void)memset(g->hdr, 0, g->mmapsize_b);
	g->hdr->rev = _REVISION;
	g->hdr->hdrsize_b = hdrsize_b;
	g->hdr->logsize_b = logsize_b;
	if (hint_op != NULL)
	    (void)strncat(g->hdr->hint, hint_op, sizeof(g->hdr->hint) - 1);
    }

    if (g->hdr->cnt == 0 && g->hdr->nextlnkaddr == 0) {
	_lnk_t lnk = {0};
	lnk.prevlnkoff = g->hdr->logsize_b * 2;
	(void)rbuf_put(&g->rb, 0, sizeof(lnk), &lnk);
    }
    return g;
}

static raddr_t setuplnk(c7_mlog_t g,
			size_t logsize, size_t tn_size, size_t sn_size, int src_line,
			c7_time_t time_us, uint32_t level, uint32_t category,
			uint64_t minidata)
{
    _lnk_t lnk;
    raddr_t const prevlnkaddr = g->hdr->nextlnkaddr;
    raddr_t addr;
    
    // null character is not counted for *_size, but it's put to rbuf.
    if (tn_size > 0) {
	if (g->hdr->max_tn_size < tn_size)
	    g->hdr->max_tn_size = tn_size;
	logsize += (tn_size + 1);
    }
    if (sn_size > 0) {
	if (g->hdr->max_sn_size < sn_size)
	    g->hdr->max_sn_size = sn_size;
	logsize += (sn_size + 1);
    }

    // put lnk as a new terminator
    addr = prevlnkaddr + sizeof(lnk) + logsize;	// address of new terminator
    lnk.prevlnkoff = addr - prevlnkaddr;
    lnk.nextlnkoff = 0;				// it mean 'terminator'
    lnk.order = 0;
    lnk.control = 0;
    (void)rbuf_put(&g->rb, addr, sizeof(lnk), &lnk);
    g->hdr->nextlnkaddr = addr;
    
    // update previous terminator as lnk of last data
    lnk.nextlnkoff = g->hdr->nextlnkaddr - prevlnkaddr;
    lnk.time_us = time_us;
    lnk.order = ++g->hdr->cnt;
    lnk.size = logsize;
    lnk.th_id = c7_thread_id(NULL);
    lnk.tn_size = tn_size;
    lnk.sn_size = sn_size;
    lnk.src_line = src_line;
    lnk.minidata = minidata;
    lnk.level = level;
    lnk.category = category;
    lnk.control = 0;
    // next rbuf_put re-write tail part of a lnk to KEEP prevlnkaddr.
    // this is depend on prevlnkaddr located at head of lnk.
    (void)rbuf_put(&g->rb,
		   prevlnkaddr + sizeof(lnk.prevlnkoff),
		   sizeof(lnk) - sizeof(lnk.prevlnkoff),
		   (char *)&lnk + sizeof(lnk.prevlnkoff));

    return prevlnkaddr + sizeof(lnk);	// address of data
}

c7_bool_t c7_mlog_put(c7_mlog_t g, c7_time_t time_us,
		      uint32_t level, uint32_t category, uint64_t minidata,
		      const char *src_name, int src_line,
		      const void *logaddr, size_t logsize_b)
{
    if (level > c7_dconf_i(C7_DCONF_MLOG))
	return C7_TRUE;

    // log data size
    if (logsize_b == -1UL)
	logsize_b = strlen(logaddr) + 1;	// logaddr point to string

    // thread name size
    const char *th_name = NULL;
    if ((g->flags & C7_MLOG_F_THREAD_NAME) != 0)
	th_name = c7_thread_name(NULL);
    size_t tn_size = th_name ? strlen(th_name) : 0;
    if (tn_size > _TN_MAX) {
	th_name += (tn_size - _TN_MAX);
	tn_size = _TN_MAX;
    }

    // source name size
    size_t sn_size;
    if ((g->flags & C7_MLOG_F_SOURCE_NAME) == 0 || src_name == NULL) {
	src_name = NULL;
	sn_size = 0;
    } else {
	src_name = c7_path_name(src_name);
	const char *sfx = c7_path_suffix(src_name);
	if ((sn_size = sfx - src_name) > _SN_MAX) {
	    src_name += (sn_size - _SN_MAX);
	    sn_size = _SN_MAX;
	}
	// DON'T setup snbuf here, because thread-unsafe.
    }

    // check size to be written
    if ((logsize_b + tn_size + sn_size + 2) > g->maxdsize_b)
	return C7_FALSE;				// data size too large

    if (time_us == C7_MLOG_AUTO_TIME)
	time_us = c7_time_us();

    if (g->lock != NULL)
	g->locker(g->lock);

    // write data (log, [thread name], [source name])
    raddr_t addr = setuplnk(g, logsize_b, tn_size, sn_size, src_line,
			    time_us, level, category, minidata);
    addr = rbuf_put(&g->rb, addr, logsize_b, logaddr);		// log data
    if (tn_size > 0)
	addr = rbuf_put(&g->rb, addr, tn_size+1, th_name);	// include null character
    if (sn_size > 0) {
	(void)c7strbcpy_x(g->snbuf, src_name, src_name + sn_size);
	addr = rbuf_put(&g->rb, addr, sn_size+1, g->snbuf);	// include null character
    }
    g->hdr->nextlnkaddr %= g->hdr->logsize_b;

    if (g->lock != NULL)
	g->unlocker(g->lock);

    return C7_TRUE;
}

c7_bool_t c7_mlog_vpfx(c7_mlog_t log, c7_time_t time_us,
		       uint32_t level, uint32_t category, uint64_t minidata,
		       const char *src_name, int src_line,
		       const char *format, va_list ap)
{
    static c7_thread_local c7_str_t sb = C7_STR_INIT_TLS();
    (void)C7_STR_REUSE(&sb);
    c7_vsprintf(&sb, format, ap);
    c7_bool_t r = c7_mlog_put(log, time_us, level, category, minidata,
			      src_name, src_line, c7_strbuf(&sb), C7_STR_LEN(&sb)+1);
    return r;
}

c7_bool_t c7_mlog_pfx(c7_mlog_t log, c7_time_t time_us,
		      uint32_t level, uint32_t category, uint64_t minidata,
		      const char *src_name, int src_line,
		      const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    c7_bool_t r = c7_mlog_vpfx(log, time_us, level, category, minidata,
			       src_name, src_line, format, ap);
    va_end(ap);
    return r;
}

typedef struct _putstatus_t_ {
    c7_str_t sb;
    c7_mlog_t log;
    c7_time_t time_us;
    uint32_t category;
} _putstatus_t;

static void putstatus(const char *src_name, int src_line,
		      c7_status_t status, const char *message,
		      const void *__uctx)
{
    _putstatus_t *prm = (_putstatus_t *)__uctx;
    c7_str_reuse(&prm->sb);
    if (status != 0) {
	c7_status_str(&prm->sb, status);
	c7_mlog_put(prm->log, prm->time_us, C7_LOG_ERR, prm->category, 0,
		    src_name, src_line, c7_strbuf(&prm->sb), C7_STR_LEN(&prm->sb)+1);
    }
    if (message != NULL) {
	c7_mlog_put(prm->log, prm->time_us, C7_LOG_ERR, prm->category, 0,
		    src_name, src_line, message, -1UL);
    }
}

c7_bool_t c7_mlog_vpfx_status(c7_mlog_t log, c7_time_t time_us,
			      uint32_t category, uint64_t minidata,
			      c7_status_t status, c7_bool_t include_old,
			      const char *src_name, int src_line,
			      const char *format, va_list ap)
{
    if (time_us == C7_MLOG_AUTO_TIME)
	time_us = c7_time_us();

    _putstatus_t prm = {
	.sb = C7_STR_INIT_MA(), .log = log, .time_us = time_us, .category = category
    };
    if (include_old)
	c7_status_scan(putstatus, &prm);
    if (status != 0)
	putstatus(src_name, src_line, status, NULL, &prm);
    if (format != NULL)
	(void)c7_mlog_vpfx(log, time_us, C7_LOG_ERR, category, minidata,
			   src_name, src_line, format, ap);
    c7_str_free(&prm.sb);
    return C7_TRUE;
}

c7_bool_t c7_mlog_pfx_status(c7_mlog_t log, c7_time_t time_us,
			     uint32_t category, uint64_t minidata,
			     c7_status_t status, c7_bool_t include_old,
			     const char *src_name, int src_line,
			     const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    c7_bool_t r = c7_mlog_vpfx_status(log, time_us, category, minidata,
				      status, include_old,
				      src_name, src_line, format, ap);
    va_end(ap);
    return r;
}


/*----------------------------------------------------------------------------
                                  write lock
----------------------------------------------------------------------------*/

static void mutex_locker(void *lock)
{
    c7_thread_lock(lock);
}

static void mutex_unlocker(void *lock)
{
    c7_thread_unlock(lock);
}

c7_bool_t c7_mlog_mutex(c7_mlog_t g, pthread_mutex_t *mutex_op)
{
    if (g->lock != NULL) {
	c7_status_add(EINVAL, ": lock is already used\n");
	return C7_FALSE;
    }
    if (mutex_op == NULL) {
	if ((mutex_op = c7_malloc(sizeof(*mutex_op))) == NULL)
	    return C7_FALSE;
	g->flags |= _MLOG_MUTEX_INTERNAL;
	(void)pthread_mutex_init(mutex_op, NULL);
    }
    g->flags |= _MLOG_MUTEX;
    g->lock = mutex_op;
    g->locker = mutex_locker;
    g->unlocker = mutex_unlocker;
    return C7_TRUE;
}

static void adv_locker(void *lock)
{
    if (!c7_fd_advlock((int)(long)lock, C7_TRUE))
	c7echo_err(0, 0);
}

static void adv_unlocker(void *lock)
{
    if (!c7_fd_advlock((int)(long)lock, C7_FALSE))
	c7echo_err(0, 0);
}

c7_bool_t c7_mlog_advlock(c7_mlog_t g)
{
    int fd = open(g->path, O_RDWR);
    if (fd == C7_SYSERR) {
	c7_status_add(errno, ": open failed: %s\n", g->path);
	return C7_FALSE;
    }
    g->flags |= _MLOG_ADVLOCK;
    g->lock = (void *)(long)fd;
    g->locker = adv_locker;
    g->unlocker = adv_unlocker;
    return C7_TRUE;
}

c7_bool_t c7_mlog_has_lock(c7_mlog_t g)
{
    return (g->lock != NULL);
}


/*----------------------------------------------------------------------------
                            read (scan) operation
----------------------------------------------------------------------------*/

c7_mlog_t c7_mlog_open_r(const char *name)
{
    c7_mlog_t g;
    if ((g = c7_calloc(sizeof(*g), 1)) == NULL)
	return NULL;
    if ((g->path = mlogpath_x(name, C7_TRUE)) == NULL) {
	free(g);
	return NULL;
    }
    if ((g->hdr = c7_file_read_x(g->path, NULL)) != NULL) {
	rbuf_init(&g->rb,
		  (char *)g->hdr + _IHDRSIZE + g->hdr->hdrsize_b,
		  g->hdr->logsize_b);
	return g;
    }
    free(g->path);
    free(g);
    return NULL;
}


static c7_bool_t default_choice(const c7_mlog_info_t *info, void *__param)
{
    return C7_TRUE;
}

static void copyattr(c7_mlog_info_t *info, const _lnk_t *lnk, const char *data)
{
    info->thread_name = "";
    info->thread_id = lnk->th_id;
    info->source_name = "";
    info->source_line = lnk->src_line;
    info->order = lnk->order;
    info->size_b = lnk->size;
    if (lnk->sn_size > 0) {
	info->size_b -= (lnk->sn_size + 1);
	info->source_name = data + info->size_b;
    }
    if (lnk->tn_size > 0) {
	info->size_b -= (lnk->tn_size + 1);
	info->thread_name = data + info->size_b;
    }
    info->time_us = lnk->time_us;
    info->level = lnk->level;
    info->category = lnk->category;
    info->minidata = lnk->minidata;
}

__attribute__((unused))
static void dumplnk(c7_mlog_t g, raddr_t addr, const _lnk_t *lnk)
{
    c7echo("%10d: order: %d, prevoff: %d (prevaddr: %d), nextoff: %d (nextaddr: %d), size_b: %d\n"
	   "          : control: %d, time_us: %ld, minidata: %ld, level: %d, category: %d, th_id: %ld\n",
	   addr % g->hdr->logsize_b,
	   lnk->order,
	   lnk->prevlnkoff, (addr - lnk->prevlnkoff) % g->hdr->logsize_b,
	   lnk->nextlnkoff, (addr + lnk->nextlnkoff) % g->hdr->logsize_b,
	   lnk->size,
	   lnk->control,
	   lnk->time_us,
	   lnk->minidata,
	   lnk->level,
	   lnk->category,
	   lnk->th_id);
}

static raddr_t findorigin(c7_mlog_t g,
			  c7_vbuf_t vbuf,
			  size_t maxcount,
			  uint32_t order_min,
			  c7_time_t time_us_min,
			  c7_bool_t (*choice)(const c7_mlog_info_t *info, void *__param),
			  void *__param)
{
    raddr_t rewindsize = sizeof(_lnk_t);
    raddr_t lnkaddr = g->hdr->nextlnkaddr + g->hdr->logsize_b * 2;
    _lnk_t lnk;
    
    rbuf_get(&g->rb, lnkaddr, sizeof(lnk), &lnk);
    rewindsize += lnk.prevlnkoff;
    lnkaddr -= lnk.prevlnkoff;

    for (;;) {
	rbuf_get(&g->rb, lnkaddr, sizeof(lnk), &lnk);
	rewindsize += lnk.prevlnkoff;
	void *data = c7_vbuf_get(vbuf, lnk.size);
	if (data == NULL)
	    c7exit_err(0, NULL);
	rbuf_get(&g->rb, lnkaddr + sizeof(lnk), lnk.size, data);
	c7_mlog_info_t info;
	copyattr(&info, &lnk, data);
	if (lnk.nextlnkoff != 0 && (*choice)(&info, __param)) {
	    lnk.control |= _LNK_CONTROL_CHOICE;
	    (void)rbuf_put(&g->rb, lnkaddr, sizeof(lnk), &lnk);
	    if (--maxcount == 0)
		break;
	}
	if (lnk.order < order_min || lnk.time_us < time_us_min)
	    break;
	if (rewindsize > g->hdr->logsize_b)
	    break;
	if (lnk.prevlnkoff <= 0)
	    break;			// might be broken
	lnkaddr -= lnk.prevlnkoff;
    }

    return lnkaddr;
}

c7_bool_t c7_mlog_scan(c7_mlog_t g,
		       size_t maxcount,
		       uint32_t order_min,
		       c7_time_t time_us_min,
		       c7_bool_t (*choice)(const c7_mlog_info_t *info, void *__param),
		       c7_bool_t (*access)(const c7_mlog_info_t *info, void *data, void *__param),
		       void *__param)
{
    c7_vbuf_t vbuf;
    if ((vbuf = c7_vbuf_new_std()) == NULL)
	return C7_FALSE;

    raddr_t addr = findorigin(g, vbuf,
			      maxcount, order_min, time_us_min,
			      choice ? choice : default_choice, __param);

    for (;;) {
	_lnk_t lnk;
	rbuf_get(&g->rb, addr, sizeof(lnk), &lnk);
	if (lnk.nextlnkoff == 0)		// terminator
	    break;
	addr += sizeof(lnk);			// data address
	if ((lnk.control & _LNK_CONTROL_CHOICE) != 0) {
	    void *data = c7_vbuf_get(vbuf, lnk.size);
	    if (data != NULL) {
		rbuf_get(&g->rb, addr, lnk.size, data);
		c7_mlog_info_t info;
		copyattr(&info, &lnk, data);
		if (!(*access)(&info, data, __param))
		    break;
	    }
	}
	addr += lnk.size;			// next lnk address
    }

    c7_vbuf_free(vbuf);
    return C7_TRUE;
}


/*----------------------------------------------------------------------------
                               other operation
----------------------------------------------------------------------------*/

void *c7_mlog_hdraddr(c7_mlog_t g, size_t *hdrsize_b_op)
{
    if (hdrsize_b_op != NULL)
	*hdrsize_b_op = g->hdr->hdrsize_b;
    return (char *)g->hdr + _IHDRSIZE;
}

int c7_mlog_thread_name_size(c7_mlog_t g)
{
    return g->hdr->max_tn_size;
}

int c7_mlog_source_name_size(c7_mlog_t g)
{
    return g->hdr->max_sn_size;
}

const char *c7_mlog_hint(c7_mlog_t g)
{
    return g->hdr->hint;
}

c7_bool_t c7_mlog_clear(const char *name)
{
    char *path = mlogpath_x(name, C7_TRUE);
    if (path == NULL)
	return C7_FALSE;
    _hdr_t hdr;
    if (c7_file_read(path, &hdr, sizeof(hdr)) != 0) {
	free(path);
	return C7_FALSE;
    }
    c7_mlog_t g = mlog_new(path, hdr.hdrsize_b, hdr.logsize_b);
    free(path);
    if (g == NULL)
	return C7_FALSE;

    g->hdr->cnt = 0;
    g->hdr->nextlnkaddr = 0;
    _lnk_t lnk = {0};
    lnk.prevlnkoff = g->hdr->logsize_b * 2;
    (void)rbuf_put(&g->rb, 0, sizeof(lnk), &lnk);

    c7_mlog_close(g);
    return C7_TRUE;
}

void c7_mlog_close(c7_mlog_t g)
{
    if ((g->flags & _MLOG_MUTEX_INTERNAL) != 0) {
	pthread_mutex_destroy(g->lock);
	free(g->lock);
    } else if ((g->flags & _MLOG_ADVLOCK) != 0) {
	(void)close((int)(long)g->lock);
    }
    if (g->mmapsize_b != 0)
	c7_file_munmap(g->hdr, g->mmapsize_b);
    else
	free(g->hdr);		// cf. c7_mlog_open_r
    free(g->path);
    free(g);
}
