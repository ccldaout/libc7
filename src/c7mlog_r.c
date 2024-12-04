/*
 * c7mlog_r.c
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
#include "_private_mlog.h"


typedef struct rec_index_t {
    int part;
    raddr_t addr;
} _index_t;


typedef struct rec_desc_t {
    c7_time_t time_us;
    uint32_t order;
    _index_t idx;
    int tn_size;
    int sn_size;
} _desc_t;


typedef struct rec_reader_t {
    _rbuf_t *rbuf;
    int part;
    raddr_t recaddr;
    raddr_t brkaddr;
} _reader_t;


#define C7_HEAP_NAME	prioq
#define C7_ELM_TYPE	_desc_t
#define C7_ELM_LT(b,a)	(((a)->time_us < (b)->time_us) ||		\
			 ((a)->time_us == (b)->time_us && (a)->order < (b)->order))
#include <c7heapdef.h>


static c7_mlog_info_t make_info(const _rec_t *rec, const char *data)
{
    c7_mlog_info_t info;

    info.thread_id   = rec->th_id;
    info.source_line = rec->src_line;
    info.weak_order  = rec->order;
    info.size_b      = rec->size;
    info.time_us     = rec->time_us;
    info.level       = rec->level;
    info.category    = rec->category;
    info.minidata    = rec->minidata;
    info.pid         = rec->pid;

    // (B) cf.(A)

    info.size_b -= sizeof(*rec);
    info.size_b -= sizeof(raddr_t);

    if (rec->sn_size > 0) {
	info.size_b -= (rec->sn_size + 1);
	info.source_name = data + info.size_b;
    } else {
	info.source_name = "";
    }

    if (rec->tn_size > 0) {
	info.size_b -= (rec->tn_size + 1);
	info.thread_name = data + info.size_b;
    } else {
	info.thread_name = "";
    }

    return info;
}


static void reader_init(_reader_t *reader, _rbuf_t *rbuf, int part)
{
    reader->rbuf = rbuf;
    reader->part = part;
    reader->recaddr = rbuf->part->nextaddr + rbuf->part->size_b * 2;
    reader->brkaddr = reader->recaddr - rbuf->part->size_b;
    
}


static c7_bool_t reader_get(_reader_t *reader,
			    c7_vbuf_t vbuf,
			    raddr_t order_min,
			    c7_time_t time_us_min,
			    c7_bool_t (*choice)(const c7_mlog_info_t *info, void *__param),
			    void *__param,
			    _desc_t *desc)
{
    _rec_t rec;
    _rbuf_t * const rbuf = reader->rbuf;

    desc->time_us = 0;	// no more records

    for (;;) {
	raddr_t size;
	rbuf_get(rbuf, reader->recaddr - sizeof(size), sizeof(size), &size);
	reader->recaddr -= size;

	if (size == 0 || reader->recaddr < reader->brkaddr) {
	    return C7_TRUE;
	}

	rbuf_get(rbuf, reader->recaddr, sizeof(rec), &rec);
	if (rec.size != size || rec.order != ~rec.br_order ||
	    rec.order < order_min || rec.time_us < time_us_min) {
	    return C7_TRUE;
	}

	size_t dsize = size - sizeof(rec);
	void *data = c7_vbuf_get(vbuf, dsize);
 	if (data == NULL) {
	    return C7_FALSE;
	}

	rbuf_get(rbuf, reader->recaddr + sizeof(rec), dsize, data);
	c7_mlog_info_t info = make_info(&rec, data);
	if ((*choice)(&info, __param)) {
	    desc->time_us  = rec.time_us;
	    desc->order    = rec.order;
	    desc->idx.part = reader->part;
	    desc->idx.addr = reader->recaddr;
	    desc->sn_size  = rec.sn_size;
	    desc->tn_size  = rec.tn_size;
	    return C7_TRUE;
	}
    }
}


static c7_bool_t check_hdr(_hdr_t *hdr, size_t size_b)
{
    if (size_b < _IHDRSIZE || hdr->rev != _REVISION) {
	return C7_FALSE;
    }
    uint64_t reqsize_b = _IHDRSIZE + hdr->hdrsize_b;
    for (int i = 0; i < _PART_CNT; i++) {
	reqsize_b += hdr->part[i].size_b;
    }
    return (reqsize_b <= size_b);
}

static void setup_rbufs(c7_mlog_t g, _hdr_t *hdr)
{
    uint64_t off = _IHDRSIZE + hdr->hdrsize_b;
    for (int i = 0; i < _PART_CNT; i++) {
	if (hdr->part[i].size_b > 0) {
	    _rbuf_t *rb = &g->rb[g->n_rb++];
	    rbuf_init(rb, (char *)hdr, off, &hdr->part[i]);
	    off += hdr->part[i].size_b;
	}
    }
}

c7_mlog_t c7_mlog_open_r(const char *name)
{
    c7_mlog_t g = c7_calloc(sizeof(*g), 1);
    if (g != NULL) {
	g->recs = c7_deque_create(sizeof(_index_t), NULL);
	if (g->recs != NULL) {
	    g->vbuf = c7_vbuf_new_std();
	    if (g->vbuf != NULL) {
		char *path = mlogpath_x(name, C7_TRUE);
		if (path != NULL) {
		    size_t size_b;
		    _hdr_t *hdr = g->hdr = c7_file_read_x(path, &size_b);
		    free(path);
		    path = NULL;
		    if (hdr != NULL) {
			if (check_hdr(hdr, size_b)) {
			    setup_rbufs(g, hdr);
			    return g;
			}
			free(hdr);
		    }
		}
		c7_vbuf_free(g->vbuf);
	    }
	    c7_deque_destroy(g->recs);
	}
	free(g);
    }

    return NULL;
}


static c7_bool_t default_choice(const c7_mlog_info_t *info, void *__param)
{
    return C7_TRUE;
}

static c7_bool_t prescan(c7_mlog_t g,
			 size_t maxcount,
			 uint32_t order_min,
			 c7_time_t time_us_min,
			 c7_bool_t (*choice)(const c7_mlog_info_t *info, void *__param),
			 void *__param)
{
    _reader_t readers[g->n_rb];
    _desc_t prioq_mem[_PART_CNT+1];
    prioq_base_t prioq = c7_heap_init(prioq_mem);

    for (int i = 0; i < g->n_rb; i++) {
	reader_init(&readers[i], &g->rb[i], i);
	_desc_t desc;
	if (!reader_get(&readers[i], g->vbuf, order_min, time_us_min, choice, __param, &desc)) {
	    return C7_FALSE;
	}
	if (desc.time_us != 0) {
	    prioq_add(&prioq, &desc);
	}
    }

    g->max_sn_size = 0;
    g->max_tn_size = 0;
    c7_deque_reset(g->recs);

    while (maxcount > 0 && c7_heap_count(&prioq) > 0) {
	_desc_t desc = *c7_heap_top(&prioq);
	prioq_remove(&prioq, 0);
	if (c7_deque_push_tail(g->recs, &desc.idx) == NULL) {
	    return C7_FALSE;
	}

	maxcount--;
	if (g->max_sn_size < desc.sn_size) {
	    g->max_sn_size = desc.sn_size;
	}
	if (g->max_tn_size < desc.tn_size) {
	    g->max_tn_size = desc.tn_size;
	}

	if (!reader_get(&readers[desc.idx.part], g->vbuf, order_min, time_us_min, choice, __param, &desc)) {
	    return C7_FALSE;
	}
	if (desc.time_us != 0) {
	    prioq_add(&prioq, &desc);
	}
    }

    return C7_TRUE;
}

c7_bool_t c7_mlog_scan(c7_mlog_t g,
		       size_t maxcount,
		       uint32_t order_min,
		       c7_time_t time_us_min,
		       c7_bool_t (*choice)(const c7_mlog_info_t *info, void *__param),
		       c7_bool_t (*access)(const c7_mlog_info_t *info, void *data, void *__param),
		       void *__param)
{
    maxcount = maxcount ? maxcount : (-1UL - 1);
    maxcount = maxcount < g->hdr->cnt ? maxcount : g->hdr->cnt;
    maxcount++;

    if (choice == NULL) {
	choice = default_choice;
    }

    if (!prescan(g, maxcount, order_min, time_us_min, choice, __param)) {
	return C7_FALSE;
    }

    const _index_t *index;
    c7_deque_foreach_r(g->recs, index) {
	_rbuf_t * const rbuf = &g->rb[index->part];

	_rec_t rec;
	rbuf_get(rbuf, index->addr, sizeof(rec), &rec);

	size_t dsize = rec.size - sizeof(rec);
	void *data = c7_vbuf_get(g->vbuf, dsize);
 	if (data == NULL) {
	    return C7_FALSE;
	}

	rbuf_get(rbuf, index->addr + sizeof(rec), dsize, data);
	c7_mlog_info_t info = make_info(&rec, data);
	if (!(*access)(&info, data, __param)) {
	    return C7_TRUE;
	}
    }

    return C7_TRUE;
}


/*----------------------------------------------------------------------------
                               other operation
----------------------------------------------------------------------------*/

int c7_mlog_thread_name_size(c7_mlog_t g)
{
    return g->max_tn_size;
}

int c7_mlog_source_name_size(c7_mlog_t g)
{
    return g->max_sn_size;
}

const char *c7_mlog_hint(c7_mlog_t g)
{
    return g->hdr->hint;
}
