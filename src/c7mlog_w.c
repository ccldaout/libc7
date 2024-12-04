/*
 * c7mlog_w.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"


#include <unistd.h>
#include <c7app.h>
#include <c7file.h>
#include <c7status.h>
#include "_private_mlog.h"


c7_mlog_t c7_mlog_open_w(const char *name, size_t hdrsize_b, size_t logsize_b,
			 const char *hint_op, uint32_t flags)
{
    if (logsize_b < 1024 || logsize_b > (1U<<30)) {
	c7_status_add(EINVAL, ": invalid logsize_b: %ld\n", logsize_b);
	return NULL;
    }

    c7_mlog_t g;
    if ((g = c7_calloc(sizeof(*g), 1)) == NULL) {
	return NULL;
    }

    char *path = mlogpath_x(name, C7_FALSE);
    if (path == NULL) {
	free(g);
	return NULL;
    }

    g->mmapsize_b = _IHDRSIZE + hdrsize_b + logsize_b;
    g->hdr = c7_file_mmap_rw(path, &g->mmapsize_b, C7_TRUE);
    free(path);
    if (g->hdr == NULL) {
	free(g);
	return NULL;
    }

    g->flags = flags;
    g->pid = getpid();

    if (g->hdr->rev            != _REVISION ||
	g->hdr->hdrsize_b      != hdrsize_b ||
	g->hdr->part[0].size_b != logsize_b ||
	g->hdr->part[1].size_b != 0 ||
	g->hdr->part[2].size_b != 0 ||
	g->hdr->part[3].size_b != 0 ||
	g->hdr->part[4].size_b != 0 ||
	g->hdr->part[5].size_b != 0 ||
	g->hdr->part[6].size_b != 0 ||
	g->hdr->part[7].size_b != 0
	) {
	(void)memset(g->hdr, 0, g->mmapsize_b);
	g->hdr->rev       = _REVISION;
	g->hdr->hdrsize_b = hdrsize_b;
	g->hdr->part[0].size_b = logsize_b;
	g->hdr->part[1].size_b = 0;
	g->hdr->part[2].size_b = 0;
	g->hdr->part[3].size_b = 0;
	g->hdr->part[4].size_b = 0;
	g->hdr->part[5].size_b = 0;
	g->hdr->part[6].size_b = 0;
	g->hdr->part[7].size_b = 0;
	if (hint_op != NULL) {
	    (void)strncat(g->hdr->hint, hint_op, sizeof(g->hdr->hint) - 1);
	}
    }

    uint64_t off = _IHDRSIZE + hdrsize_b;
    rbuf_init(&g->rb[0], (char *)g->hdr, off, &g->hdr->part[0]);
    g->n_rb = 1;

    if (g->hdr->cnt == 0) {
	rbuf_clear(&g->rb[0]);
    }

    g->recs = NULL;
    g->vbuf = NULL;

    return g;
}

static _rec_t make_rechdr(c7_mlog_t g,
			  size_t logsize, size_t tn_size, size_t sn_size, int src_line,
			  c7_time_t time_us, uint32_t level, uint32_t category,
			  uint64_t minidata)
{
    _rec_t rec;
    
    // null character is not counted for *_size, but it's put to rbuf.
    if (tn_size > 0) {
	logsize += (tn_size + 1);
    }
    if (sn_size > 0) {
	logsize += (sn_size + 1);
    }
    logsize += sizeof(raddr_t);
    logsize += sizeof(_rec_t);

    rec.size = logsize;
    // rec.order is assigned later
    rec.time_us = time_us;
    rec.pid = g->pid;
    rec.th_id = c7_thread_id(NULL);
    rec.tn_size = tn_size;
    rec.sn_size = sn_size;
    rec.src_line = src_line;
    rec.minidata = minidata;
    rec.level = level;
    rec.category = category;
    rec.control = 0;
    // rec.br_order is assigned later

    return rec;
}

c7_bool_t c7_mlog_put(c7_mlog_t g, c7_time_t time_us,
		      uint32_t level, uint32_t category, uint64_t minidata,
		      const char *src_name, int src_line,
		      const void *logaddr, size_t logsize_b)
{
    if (level > c7_dconf_i(C7_DCONF_MLOG)) {
	return C7_TRUE;
    }

    // log data size
    if (logsize_b == -1UL) {
	logsize_b = strlen(logaddr) + 1;	// logaddr point to string
    }

    // thread name size
    const char *th_name = NULL;
    if ((g->flags & C7_MLOG_F_THREAD_NAME) != 0) {
	th_name = c7_thread_name(NULL);
    }
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
    }

    if (time_us == C7_MLOG_AUTO_TIME) {
	time_us = c7_time_us();
    }

    _hdr_t * const hdr_ = g->hdr;

    // build record header adn calculate size of whole record.
    _rec_t rechdr = make_rechdr(g, logsize_b, tn_size, sn_size, src_line,
				time_us, level, category, minidata);


    // lock-free operation: get address of out record and update hdr_->nextaddr.
    raddr_t addr = rbuf_reserve(&g->rb[0], rechdr.size);
    if (addr == _TOO_LARGE) {
	return C7_FALSE;
    }

    // lock-free operation: update record sequence number.
    //                    : next rechdr.order is not strict, but we accept it.
    rechdr.order    = __sync_add_and_fetch(&hdr_->cnt, 1);
    rechdr.br_order = ~rechdr.order;

    // write whole record: log data -> thread_name -> source name	// (A) cf.(B)
    addr = rbuf_put(&g->rb[0], addr, sizeof(rechdr), &rechdr);		// record header
    addr = rbuf_put(&g->rb[0], addr, logsize_b, logaddr);			// log data
    if (tn_size > 0) {
	addr = rbuf_put(&g->rb[0], addr, tn_size+1, th_name);		// +1: null character
    }
    if (sn_size > 0) {
	char ch = 0;
	addr = rbuf_put(&g->rb[0], addr, sn_size, src_name);		// exclude suffix
	addr = rbuf_put(&g->rb[0], addr, 1, &ch);
    }
    rbuf_put(&g->rb[0], addr, sizeof(rechdr.size), &rechdr.size);	// put size data to tail

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
    if (time_us == C7_MLOG_AUTO_TIME) {
	time_us = c7_time_us();
    }

    _putstatus_t prm = {
	.sb = C7_STR_INIT_MA(), .log = log, .time_us = time_us, .category = category
    };
    if (include_old) {
	c7_status_scan(putstatus, &prm);
    }
    if (status != 0) {
	putstatus(src_name, src_line, status, NULL, &prm);
    }
    if (format != NULL) {
	(void)c7_mlog_vpfx(log, time_us, C7_LOG_ERR, category, minidata,
			   src_name, src_line, format, ap);
    }
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
                   write lock (obsolete: for comptibility)
----------------------------------------------------------------------------*/

c7_bool_t c7_mlog_mutex(c7_mlog_t g, pthread_mutex_t *mutex_op)
{
    return C7_TRUE;
}

c7_bool_t c7_mlog_advlock(c7_mlog_t g)
{
    return C7_TRUE;
}

c7_bool_t c7_mlog_has_lock(c7_mlog_t g)
{
    return C7_TRUE;
}


/*----------------------------------------------------------------------------
                               other operation
----------------------------------------------------------------------------*/

void *c7_mlog_hdraddr(c7_mlog_t g, size_t *hdrsize_b_op)
{
    if (hdrsize_b_op != NULL) {
	*hdrsize_b_op = g->hdr->hdrsize_b;
    }
    return (char *)g->hdr + _IHDRSIZE;
}

c7_bool_t c7_mlog_clear(const char *name)
{
    char *path = mlogpath_x(name, C7_TRUE);
    if (path == NULL) {
	return C7_FALSE;
    }

    uint64_t mmapsize_b = 0;
    _hdr_t *hdr = c7_file_mmap_rw(path, &mmapsize_b, C7_FALSE);
    free(path);

    if (mmapsize_b < _IHDRSIZE || hdr->rev != _REVISION) {
	c7_file_munmap(hdr, mmapsize_b);
	return C7_FALSE;
    }
    uint64_t logsize_b = _IHDRSIZE + hdr->hdrsize_b;
    for (int i = 0; i < _PART_CNT; i++) {
	logsize_b += hdr->part[i].size_b;
    }
    if (mmapsize_b < logsize_b) {
	c7_file_munmap(hdr, mmapsize_b);
	return C7_FALSE;
    }

    hdr->cnt = 0;
    uint64_t off = _IHDRSIZE + hdr->hdrsize_b;
    for (int i = 0; i < _PART_CNT; i++) {
	_rbuf_t rb;
	rbuf_init(&rb, (char *)hdr, off, &hdr->part[i]);
	rbuf_clear(&rb);
	off += hdr->part[i].size_b;
    }

    c7_file_munmap(hdr, mmapsize_b);
    return C7_TRUE;
}

void c7_mlog_close(c7_mlog_t g)
{
    c7_deque_destroy(g->recs);
    c7_vbuf_free(g->vbuf);
    if (g->mmapsize_b != 0) {
	c7_file_munmap(g->hdr, g->mmapsize_b);
    } else {
	free(g->hdr);		// cf. c7_mlog_open_r
    }
    free(g);
}
