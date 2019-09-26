/*
 * c7dconf.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <unistd.h>
#include <fcntl.h>
#include <c7dconf.h>
#include <c7file.h>
#include <c7status.h>
#include <c7app.h>


#define _INDEX_LIM	(100)	// C7_DCONF_USER_INDEX_LIM..99: libc7 area
#define _VERSION	(2)	// _INDEX_LIM:100, C7_DCONF_USER_INDEX_LIM:90

typedef struct _dconf_t {
    int32_t version;
    uint32_t mapsize;
    c7_dconf_val_t array[_INDEX_LIM];
    c7_dconf_type_t types[_INDEX_LIM];
} _dconf_t;

typedef struct _mmidx_t {
    c7_dconf_type_t type;
    int str_off;
} _mmidx_t;


static c7_dconf_val_t DefaultArray[_INDEX_LIM];
c7_dconf_val_t *__C7_DCONF_Addr = DefaultArray;


static char *dconfpath_x(const char *name, c7_bool_t exists)
{
    c7_str_t *sbp;
    if (exists)
	sbp = c7_file_special_find(NULL, C7_DCONF_DIR_ENV, name, ".dconf");
    else
	sbp = c7_file_special_path(NULL, C7_DCONF_DIR_ENV, name, ".dconf");
    if (C7_STR_ERR(sbp)) {
	c7_status_add(errno, "cannot create dconf path\n");
	return NULL;
    }
    return c7_strbuf(sbp);
}

static int get_i(const char *env, int defval)
{
    if (getenv(env) == NULL)
	return defval;
    return strtol(getenv(env), NULL, 0);
}


static size_t sizeofhelp(int defc, const c7_dconf_def_t *defv)
{
    size_t n = 0;
    for (int i = 0; i < defc; i++, defv++)
	n += strlen(defv->ident) + strlen(defv->descrip) +1;	// +1: ':'
    n += _INDEX_LIM * 1; 		// all index has '\a'.
    n++;				// last null
    return n;
}

static const c7_dconf_def_t *finddef(int defc, const c7_dconf_def_t *defv, int index)
{
    for (int i = 0; i < defc; i++) {
	if (defv[i].index == index)
	    return &defv[i];
    }
    return NULL;
}

static _dconf_t *mapdconf(const char *name, int defc, const c7_dconf_def_t *defv)
{
    name = dconfpath_x(name, C7_FALSE);
    if (name == NULL)
	return NULL;

    size_t n = sizeofhelp(defc, defv);
    n += sizeof(_dconf_t);
    _dconf_t * const dc = c7_file_mmap_rw(name, &n, C7_TRUE);
    if (dc == NULL)
	return NULL;

    if (__C7_DCONF_Addr != DefaultArray) {
	_dconf_t *dc = (void *)((char *)__C7_DCONF_Addr - offsetof(_dconf_t, array));
	c7_file_munmap(dc, dc->mapsize);
    }
    dc->version = _VERSION;
    dc->mapsize = n;
    __C7_DCONF_Addr = dc->array;

    char *p = (char *)(dc + 1);
    for (int i = 0; i < _INDEX_LIM; i++) {
	const c7_dconf_def_t *def = finddef(defc, defv, i);
	if (def != NULL) {
	    p = c7strcpy_x(p, def->ident);
	    *p++ = ':';
	    p = c7strcpy_x(p, def->descrip);
	    *p++ = '\a';
	} else
	    *p++ = '\a';
    }
    *p = 0;
    return dc;
}

static const c7_dconf_def_t *mergedef(int *defcp, const c7_dconf_def_t *defv)
{
    for (int i = 0; i < *defcp; i++) {
	if (defv[i].index < C7_DCONF_USER_INDEX_BASE ||
	    defv[i].index >= C7_DCONF_USER_INDEX_LIM) {
	    c7_status_add(EINVAL, ": dconf index '%d' for '%s' is out of range\n",
			  defv[i].index, defv[i].ident);
	    return NULL;
	}
    }

    size_t o_size = (*defcp) * sizeof(*defv);
    c7_dconf_def_t c7defs[] = {
	C7_DCONF_DEF_I(C7_DCONF_ECHO, "echo level (default:3)"),
	C7_DCONF_DEF_I(C7_DCONF_MLOG, "mlog level (default:5)"),
	C7_DCONF_DEF_I(C7_DCONF_PREF, "echo/status prefix type (default:0)"),
	C7_DCONF_DEF_I(C7_DCONF_STSSCN_MAX, "statsu scan limitation (default:10)"),
    };
    c7_dconf_def_t *ndefv = c7_sg_malloc(o_size + sizeof(c7defs));
    if (ndefv == NULL) {
	c7echo_err(0, NULL);
	return NULL;
    }
    (void)memcpy(ndefv, defv, o_size);
    (void)memcpy(&ndefv[*defcp], c7defs, sizeof(c7defs));
    *defcp += c7_numberof(c7defs);
    return ndefv;
}

void c7_dconf_init(const char *name, int defc, const c7_dconf_def_t *defv)
{
    c7_sg_push();
    if ((defv = mergedef(&defc, defv)) != NULL) {
	_dconf_t *dc = mapdconf(name, defc, defv);
	if (dc != NULL) {
	    if (c7_dconf_i(C7_DCONF_ECHO) == 0)
		c7_dconf_i_set(C7_DCONF_ECHO, C7_LOG_INF);
	    if (c7_dconf_i(C7_DCONF_MLOG) == 0)
		c7_dconf_i_set(C7_DCONF_MLOG, C7_LOG_DTL);
	    if (c7_dconf_i(C7_DCONF_STSSCN_MAX) == 0)
		c7_dconf_i_set(C7_DCONF_STSSCN_MAX, 10);
	    for (int i = 0; i < defc; i++)
		dc->types[defv[i].index] = defv[i].type;
	}
    }
    c7_sg_pop();
}


static c7_dconf_def_t *loaddconf(const char *name, int *defc)
{
    name = dconfpath_x(name, C7_TRUE);
    if (name == NULL)
	return NULL;

    size_t n = 0;
    _dconf_t * const dc = c7_file_mmap_rw(name, &n, C7_FALSE);
    if (dc == NULL)
	return NULL;

    if (dc->version == _VERSION) {
	__C7_DCONF_Addr = dc->array;
	size_t def_z = sizeof(c7_dconf_def_t) * _INDEX_LIM;
	size_t ma_z = def_z + n - sizeof(*dc);
	c7_dconf_def_t * const def = c7_malloc(ma_z);
	if (def != NULL) {
	    char * const help = (char *)def + def_z;
	    (void)strcpy(help, (char *)(dc + 1));
	    c7_dconf_def_t *d = def;
	    char *p = help;
	    int index = 0;
	    for (; *p != 0; p++, index++) {
		if (*p == '\a')
		    continue;
		d->index = index;
		d->type = dc->types[index];
		d->ident = p;
		p = strchr(p, ':');
		*p++ = 0;
		d->descrip = p;
		p = strchr(p, '\a');
		*p = 0;
		d++;
	    }
	    *defc = (d - def);
	    return def;
	}
    } else
	c7_status_add(errno = EINVAL, ": invalid version: %d (expect: %d)\n",
		      dc->version, _VERSION);
    c7_file_munmap(dc, n);
    return NULL;
}

c7_dconf_def_t *c7_dconf_load(const char *name, int *defc)
{
    c7_sg_push();
    c7_dconf_def_t *def = loaddconf(name, defc);
    c7_sg_pop();
    return def;
}


void __c7_dconf_init(void)
{
    c7_dconf_i_set(C7_DCONF_ECHO, get_i("C7_DCONF_ECHO", C7_LOG_INF));
    c7_dconf_i_set(C7_DCONF_MLOG, get_i("C7_DCONF_MLOG", C7_LOG_DTL));
    c7_dconf_i_set(C7_DCONF_PREF, get_i("C7_DCONF_PREF", 0));
    c7_dconf_i_set(C7_DCONF_STSSCN_MAX, get_i("C7_DCONF_STSSCN_MAX", 10));
}
