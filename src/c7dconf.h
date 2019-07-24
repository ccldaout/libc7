/*
 * c7dconf.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_DCONF_H_LOADED__
#define __C7_DCONF_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7types.h>


#define C7_DCONF_DIR_ENV		"C7_DCONF_DIR"

#define C7_DCONF_USER_INDEX_BASE	(0)
#define C7_DCONF_USER_INDEX_LIM		(48)	// not include 48

typedef enum c7_dconf_type_t_ {
    C7_DCONF_TYPE_None,
    C7_DCONF_TYPE_I64,
    C7_DCONF_TYPE_R64
} c7_dconf_type_t;

typedef struct c7_dconf_def_t_ {
    int index;
    c7_dconf_type_t type;
    const char *ident;
    const char *descrip;
} c7_dconf_def_t;

#define C7_DCONF_DEF_I(idxmacro, descrip)			\
    { (idxmacro), C7_DCONF_TYPE_I64, #idxmacro, descrip }
#define C7_DCONF_DEF_R(idxmacro, descrip)			\
    { (idxmacro), C7_DCONF_TYPE_R64, #idxmacro, descrip }
    

enum {
    C7_DCONF_base = C7_DCONF_USER_INDEX_LIM - 1,
    C7_DCONF_ECHO,
    C7_DCONF_MLOG,
    C7_DCONF_PREF,
    C7_DCONF_STSSCN_MAX,
    C7_DCONF_numof
};

typedef union c7_dconf_val_t_ {
    int64_t i64;
    double r64;
} c7_dconf_val_t;

extern c7_dconf_val_t *__C7_DCONF_Addr;

#if !defined(C7_CONFIG_SCA)
# define c7_dconf_i(m)		((int64_t)(__C7_DCONF_Addr[(m)].i64))
# define c7_dconf_i_set(m, v)	(__C7_DCONF_Addr[(m)].i64 = (v))
# define c7_dconf_r(m)		((double)(__C7_DCONF_Addr[(m)].r64))
# define c7_dconf_r_set(m, v)	(__C7_DCONF_Addr[(m)].r64 = (v))
#else
static inline int64_t c7_dconf_i(int index)
{
    return (__C7_DCONF_Addr != NULL) ? __C7_DCONF_Addr[index].i64 : 0;
}

static inline void c7_dconf_i_set(int index, int64_t v)
{
    if (__C7_DCONF_Addr != NULL) {
	__C7_DCONF_Addr[index].i64 = v;
    }
}

static inline double c7_dconf_r(int index)
{
    return (__C7_DCONF_Addr != NULL) ? __C7_DCONF_Addr[index].r64 : 0.0;
}

static inline void c7_dconf_r_set(int index, double v)
{
    if (__C7_DCONF_Addr != NULL) {
	__C7_DCONF_Addr[index].r64 = v;
    }
}
#endif

void c7_dconf_init(const char *name, int defc, const c7_dconf_def_t *defv);
c7_dconf_def_t *c7_dconf_load(const char *name, int *defc);


#if defined(__cplusplus)
}
#endif
#endif /* c7dconf.h */
