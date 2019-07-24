/*
 * c7rbuf.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <c7memory.h>
#include <c7rbuf.h>
#include <string.h>


typedef struct rbuf_t_ {
    rbuf_unit_t size;
    rbuf_unit_t nextaddr;
    rbuf_unit_t loweraddr;
    rbuf_unit_t rewindcnt;
    char data[];
} rbuf_t;


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

static rbuf_unit_t rb_get(rbuf_t *rb, rbuf_unit_t addr, rbuf_unit_t size, void *__ubuf)
{
    const rbuf_unit_t ret_addr = addr + size;
    char *ubuf = __ubuf;
    char *rbuf = (char *)rb->data + (addr % rb->size);
    size_t rrest = rb->size - (addr % rb->size);
	
    while (size > 0) {
	size_t cpsize = (size < rrest) ? size : rrest;
	(void)memcpy(ubuf, rbuf, cpsize);
	ubuf += cpsize;
	size -= cpsize;
	rbuf = rb->data;
	rrest = rb->size;
    }

    return ret_addr;
}

static rbuf_unit_t rb_put(rbuf_t *rb, rbuf_unit_t addr, rbuf_unit_t size, const void *__ubuf)
{
    const rbuf_unit_t ret_addr = addr + size;
    const char *ubuf = __ubuf;
    char *rbuf = (char *)rb->data + (addr % rb->size);
    size_t rrest = rb->size - (addr % rb->size);

    while (size > 0) {
	size_t cpsize = (size < rrest) ? size : rrest;
	(void)memcpy(rbuf, ubuf, cpsize);
	ubuf += cpsize;
	size -= cpsize;
	rbuf = rb->data;
	rrest = rb->size;
    }

    return ret_addr;
}


/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

rbuf_unit_t *c7_rbuf_new(uint32_t size_of_data)
{
    rbuf_unit_t *rba = c7_calloc(sizeof(*rba), C7_RBUF_SIZE(size_of_data));
    if (rba == NULL)
	return NULL;
    
    rbuf_t *rb = (rbuf_t *)rba;
    rb->size = size_of_data;
    rb->nextaddr = size_of_data * 2;
    rb->loweraddr = size_of_data * 2;
    return rba;
}


void c7_rbuf_init(rbuf_unit_t *rba, uint32_t size_of_rball)
{
    rbuf_t *rb = (rbuf_t *)rba;
    size_t size_b = size_of_rball - sizeof(rbuf_t);
    rb->size = size_b;
    rb->nextaddr = size_b * 2;
    rb->loweraddr = size_b * 2;
}


void c7_rbuf_put(rbuf_unit_t *rba, void *data, rbuf_unit_t size_b)
{
    rbuf_t *rb = (rbuf_t *)rba;
    if (size_b + sizeof(size_b)*2 > rb->size ) {
	// TODO: warning to stderr
	return;
    }
 
    rbuf_unit_t addr = rb_put(rb, rb->nextaddr, sizeof(size_b), &size_b);
    addr = rb_put(rb, addr, size_b, data);
    rb->nextaddr = rb_put(rb, addr, sizeof(size_b), &size_b);

    if (rb->loweraddr < rb->nextaddr - rb->size)
	rb->loweraddr = rb->nextaddr - rb->size;
    if (rb->nextaddr > rb->size * 3) {
	rb->rewindcnt++;
	rb->nextaddr -= rb->size;
	rb->loweraddr -= rb->size;
    }
}


void c7_rbuf_putiov(rbuf_unit_t *rba, struct iovec *iov, int ioc)
{
    rbuf_unit_t size_b = 0;
    for (int i = 0; i < ioc; i++)
	size_b += iov[i].iov_len;

    rbuf_t *rb = (rbuf_t *)rba;
    if (size_b + sizeof(size_b)*2 > rb->size ) {
	// TODO: warning to stderr
	return;
    }
 
    rbuf_unit_t addr = rb_put(rb, rb->nextaddr, sizeof(size_b), &size_b);
    for (int i = 0; i < ioc; i++)
	addr = rb_put(rb, addr, iov[i].iov_len, iov[i].iov_base);
    rb->nextaddr = rb_put(rb, addr, sizeof(size_b), &size_b);

    if (rb->loweraddr < rb->nextaddr - rb->size)
	rb->loweraddr = rb->nextaddr - rb->size;
    if (rb->nextaddr > rb->size * 3) {
	rb->rewindcnt++;
	rb->nextaddr -= rb->size;
	rb->loweraddr -= rb->size;
    }
}


void c7_rbuf_clear(rbuf_unit_t *rba)
{
    rbuf_t *rb = (rbuf_t *)rba;
    rb->nextaddr = rb->size * 2;
    rb->loweraddr = rb->size * 2;
}


c7_bool_t c7_rbuf_is_empty(rbuf_unit_t *rba)
{
    rbuf_t *rb = (rbuf_t *)rba;
    return (rb->nextaddr == rb->loweraddr);
}


c7_bool_t c7_rbuf_scan(rbuf_unit_t *rba, unsigned max_call,
		       c7_bool_t (*callback)(void *data, rbuf_unit_t size_b, void *__prm),
		       void *__prm)
{
    rbuf_t *rb = (rbuf_t *)rba;

    if (max_call == 0)
	max_call = -1U;

    rbuf_unit_t size_b;
    rbuf_unit_t addr = rb->nextaddr;

    for (;;) {
	rbuf_unit_t tryaddr = addr - sizeof(size_b);
	if (rb->loweraddr > tryaddr)
	    break;
	(void)rb_get(rb, tryaddr, sizeof(size_b), &size_b);
	tryaddr -= (size_b + sizeof(size_b));
	if (rb->loweraddr > tryaddr)
	    break;
	addr = tryaddr;
	if (--max_call == 0)
	    break;
    }

    c7_vbuf_t vb = c7_vbuf_new_std();
    if (vb == NULL)
	return C7_FALSE;

    c7_bool_t ret = C7_TRUE;
    while (addr < rb->nextaddr) {
	addr = rb_get(rb, addr, sizeof(size_b), &size_b);
	void *data = c7_vbuf_get(vb, size_b);
	if (data == NULL) {
	    ret = C7_FALSE;
	    break;
	}
	addr = rb_get(rb, addr, size_b, data);
	if (!callback(data, size_b, __prm))
	    break;
	addr += sizeof(size_b);
    }
    
    c7_vbuf_free(vb);
    return ret;
}


c7_bool_t c7_rbuf_scan_r(rbuf_unit_t *rba, unsigned max_call,
			 c7_bool_t (*callback)(void *data, rbuf_unit_t size_b, void *__prm),
			 void *__prm)
{
    rbuf_t *rb = (rbuf_t *)rba;

    c7_vbuf_t vb = c7_vbuf_new_std();
    if (vb == NULL)
	return C7_FALSE;

    if (max_call == 0)
	max_call = -1U;

    c7_bool_t ret = C7_TRUE;
    rbuf_unit_t size_b;
    rbuf_unit_t addr = rb->nextaddr - sizeof(size_b);

    for (;;) {
	if (rb->loweraddr > addr)
	    break;
	(void)rb_get(rb, addr, sizeof(size_b), &size_b);
	addr -= size_b;
	if (rb->loweraddr > addr)
	    break;
	void *data = c7_vbuf_get(vb, size_b);
	if (data == NULL) {
	    ret = C7_FALSE;
	    break;
	}
	(void)rb_get(rb, addr, size_b, data);
	if (!callback(data, size_b, __prm))
	    break;
	if (--max_call == 0)
	    break;
	addr -= (sizeof(size_b) * 2);
    }

    c7_vbuf_free(vb);
    return ret;
}
