/*
 * c7byteorder.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_BYTEORDER_LOADED__
#define __C7_BYTEORDER_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7types.h>


/*
 * byte mask tools
 */

#if defined(__C7_CONFIG_LP64)
# define __c7_u64_(_c)	(_c)
#else
# define __c7_u64_(_c)	(_c##L)	/* long long */
#endif
#define __c7_u64_M16H(v)	(__c7_u64_(0xffff0000ffff0000UL) & (v))
#define __c7_u64_M16L(v)	(__c7_u64_(0x0000ffff0000ffffUL) & (v))
#define __c7_u64_M8H(v)		(__c7_u64_(0xff00ff00ff00ff00UL) & (v))
#define __c7_u64_M8L(v)		(__c7_u64_(0x00ff00ff00ff00ffUL) & (v))
#define __c7_u32_M8H(v)		(0xff00ff00U & (v))
#define __c7_u32_M8L(v)		(0x00ff00ffU & (v))


/*
 * byte swap - inline asm
 */

#if defined(__GNUC__) && defined(__x86_64)

/*
 * gcc inline assembler
 */

# define c7_u32_swap16(_i)						\
    ({ uint32_t v__ = (_i); __asm__("rorl $16,%0":"+r"(v__)); v__; })

# define c7_u16_swap8(_i)						\
    ({ uint16_t v__ = (_i); __asm__("rorw $8,%0":"+r"(v__)); v__; })

# if defined(__x86_64)
#   define c7_u64_rev8(_i,_o)			\
    do {					\
	uint64_t v__ = (_i);			\
	__asm__("bswapq %0":"+r"(v__));		\
	(_o) = v__;				\
    } while (0)
# endif

# define c7_u32_rev8(_i,_o)			\
    do {					\
	uint32_t v__ = (_i);			\
	__asm__("bswapl %0":"+r"(v__));		\
	(_o) = v__;				\
    } while (0)

# define c7_u16_rev8(_i,_o)			\
    do {					\
	uint16_t v__ = (_i);			\
	__asm__("rorw $8,%0":"+r"(v__));	\
	(_o) = v__;				\
    } while (0)

# endif

#elif defined(__SUNPRO_C) && defined(__x86_64) && defined(C7_CONFIG_SOL_IL)

/*
 *   byteswap.il including following definitions are specified as
 *   a part of SUN Pro C compiler argument.
 *
 *	.inline __uukit_u32_swap16_asm,0
 *	movl %edi, %eax
 *	rorl $16, %eax
 *	.end
 *	
 *	.inline __uukit_u16_swap8_asm,0
 *	movzwl %di, %eax
 *	rorw $8, %ax
 *	.end
 *	
 *	.inline __uukit_u64_rev8_asm,0
 *	movq %rdi, %rax
 *	bswapq %rax
 *	.end
 *	
 *	.inline __uukit_u32_rev8_asm,0
 *	movl %edi, %eax
 *	bswapl %eax
 *	.end
 */

uint32_t c7_u32_swap16_asm(uint32_t);
uint16_t c7_u16_swap8_asm(uint16_t);
uint64_t c7_u64_rev8_asm(uint64_t);
uint32_t c7_u32_rev8_asm(uint32_t);
# define c7_u32_swap16(_i)	c7_u32_swap16_asm(_i)
# define c7_u16_swap8(_i)	c7_u16_swap8_asm(_i)
# define c7_u64_rev8(_i,_o)	do _o = c7_u64_rev8_asm(_i); while (0)
# define c7_u32_rev8(_i,_o)	do _o = c7_u32_rev8_asm(_i); while (0)
# define c7_u16_rev8(_i,_o)	do _o = c7_u16_swap8_asm(_i); while (0)

#endif


/*
 * byte swap - C standard operations
 */

#if !defined(c7_u16_swap8)
# define c7_u16_swap8(__u)	(((__u)>>8)|((__u)<<8))
#endif

#if !defined(c7_u16_rev8)
# define c7_u16_rev8(_i,_o)			\
    do {					\
	uint16_t v__ = _i;			\
	_o = c7_u16_swap8(v__);			\
    } while (0)
#endif

#if !defined(c7_u32_swap16)
# define c7_u32_swap16(__u)	(((__u)>>16)|((__u)<<16))
#endif

#if !defined(c7_u32_swap8)
# define c7_u32_swap8(__u)	((__c7_u32_M8H(__u)>>8)|(__c7_u32_M8L(__u)<<8))
#endif

#if !defined(c7_u32_rev8)
# define c7_u32_rev8(_i,_o)			\
    do {					\
	uint32_t v__ = _i;			\
	v__ = c7_u32_swap16(v__);		\
	_o = c7_u32_swap8(v__);			\
    } while (0)
#endif

#if !defined(c7_u64_swap32)
# define c7_u64_swap32(__u)	(((__u)>>32)|((__u)<<32))
#endif

#if !defined(c7_u64_swap16)
# define c7_u64_swap16(__u)	((__c7_u64_M16H(__u)>>16)|(__c7_u64_M16L(__u)<<16))
#endif

#if !defined(c7_u64_swap8)
# define c7_u64_swap8(__u)	((__c7_u64_M8H(__u)>>8)|(__c7_u64_M8L(__u)<<8))
#endif

#if !defined(c7_u64_rev8)
# define c7_u64_rev8(_i,_o)			\
    do {					\
	uint64_t v__ = _i;			\
	v__ = c7_u64_swap32(v__);		\
	v__ = c7_u64_swap16(v__);		\
	_o = c7_u64_swap8(v__);			\
    } while (0)
#endif


#if defined(__cplusplus)
}
#endif
#endif /* c7byteorder.h */
