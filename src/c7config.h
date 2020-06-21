/*
 * c7config.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef ___C7_CONFIG_H_LOADED__
#define ___C7_CONFIG_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif


#if ((defined(__cplusplus) && (__cplusplus < 201103L)) || \
     (defined(__STDC_VERSION__) && (__STDC_VERSION__ < 199901L)))
# error "libc7 require at least C99"
#endif

#if (defined(_POSIX_C_SOURCE) && (_POSIX_C_SOURCE < 200809L))
# undef _POSIX_C_SOURCE
#endif
#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

#undef  _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED 1

#undef  _XOPEN_SOURCE
#define _XOPEN_SOURCE 1

#undef  _GNU_SOURCE
#define _GNU_SOURCE 1

#if defined(C7_USER_CONFIG_H)
# define __C7_to_string___(x)	#x
# define __C7_to_string__(x)	__C7_to_string___(x)
# include __C7_to_string__(C7_USER_CONFIG_H)
# undef __C7_to_string__
# undef __C7_to_string___
#endif

#if !defined(C7_CONFIG_MEMALIGN)
# define C7_CONFIG_MEMALIGN	16	// 16 is derived by ucontext_t
#endif
#if !defined(C7_CONFIG_UCONTEXT)
# if (defined(__linux) && !defined(__ANDROID__)) || defined(__CYGWIN__)
#   define C7_CONFIG_UCONTEXT	1
# else
#   define C7_CONFIG_UCONTEXT	0
# endif
#endif

#if defined(C7_CONFIG_SCA)
# define __SCA_nullcheck(p)		assert((p) != NULL)
# define __SCA_assert			assert
# undef NDEBUG
# include <assert.h>
#else
# define NDEBUG 1
# define __SCA_nullcheck(p)
# define __SCA_assert(expr)
#endif


#if defined(__cplusplus)
}
#endif
#endif /* c7_config.h */
