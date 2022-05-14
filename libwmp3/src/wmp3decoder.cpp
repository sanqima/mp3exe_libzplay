/*
 * libmad - MPEG audio decoder library ( modified version )
 * Copyright (C) 2000-2004 Underbit Technologies, Inc.
 *
 * This is modified version of original libmad.
 * Some parts are excluded from original version, and code is modified
 * to compile on free Borland commandline compiler 5.5
 *
 * Date of modification: 01 April, 2009.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * If you would like to negotiate alternate licensing terms, you may do
 * so by contacting: Underbit Technologies, Inc. <info@underbit.com>
 */


//#include <stdlib.h>
#include <stdio.h>
//#include <ctype.h>
//#include <signal.h>
//#include <math.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <assert.h>

#include <windows.h>
#include "wmp3decoder.h"


// decoder optimization





/* Define to optimize for accuracy over speed. */
#define OPT_ACCURACY



/* Define to optimize for speed over accuracy. */
//#define OPT_SPEED


/* Define to enable a fast subband synthesis approximation optimization. */
//#define OPT_SSO

/* Define to influence a strict interpretation of the ISO/IEC standards, even
   if this is in opposition with best accepted practices. */
//#define OPT_STRICT


#define FPM_DEFAULT

//#define FPM_FLOAT
//#define FPM_INTEL






/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 0

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 0

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 0

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 0

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 0

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 0

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 0

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 0

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 0

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 0

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of a `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1


/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define as `__inline' if that's what the C compiler calls it, or to nothing
   if it is not supported. */
#define inline __inline




/* conditional features */

# if defined(OPT_SPEED) && defined(OPT_ACCURACY)
#  error "cannot optimize for both speed and accuracy"
# endif

# if defined(OPT_SPEED) && !defined(OPT_SSO)
#  define OPT_SSO
# endif

# if defined(HAVE_UNISTD_H) && defined(HAVE_WAITPID) &&  \
    defined(HAVE_FCNTL) && defined(HAVE_PIPE) && defined(HAVE_FORK)
#  define USE_ASYNC
# endif


# if !defined(HAVE_ASSERT_H)
#  if defined(NDEBUG)
#   define assert(x)	/* nothing */
#  else
#   define assert(x)	do { if (!(x)) abort(); } while (0)
#  endif
# endif



#if SIZEOF_INT >= 4
typedef   signed int mad_fixed_t;

typedef   signed int mad_fixed64hi_t;
typedef unsigned int mad_fixed64lo_t;
# else
typedef   signed long mad_fixed_t;

typedef   signed long mad_fixed64hi_t;
typedef unsigned long mad_fixed64lo_t;
# endif

# if defined(_MSC_VER)
#  define mad_fixed64_t  signed __int64
# elif 1 || defined(__GNUC__)
#  define mad_fixed64_t  signed long long
# endif

# if defined(FPM_FLOAT)
typedef double mad_sample_t;
# else
typedef mad_fixed_t mad_sample_t;
# endif

/*
 * Fixed-point format: 0xABBBBBBB
 * A == whole part      (sign + 3 bits)
 * B == fractional part (28 bits)
 *
 * Values are signed two's complement, so the effective range is:
 * 0x80000000 to 0x7fffffff
 *       -8.0 to +7.9999999962747097015380859375
 *
 * The smallest representable value is:
 * 0x00000001 == 0.0000000037252902984619140625 (i.e. about 3.725e-9)
 *
 * 28 bits of fractional accuracy represent about
 * 8.6 digits of decimal accuracy.
 *
 * Fixed-point numbers can be added or subtracted as normal
 * integers, but multiplication requires shifting the 64-bit result
 * from 56 fractional bits back to 28 (and rounding.)
 *
 * Changing the definition of MAD_F_FRACBITS is only partially
 * supported, and must be done with care.
 */



# define mad_f_intpart(x)	((x) >> MAD_F_FRACBITS)
# define mad_f_fracpart(x)	((x) & ((1L << MAD_F_FRACBITS) - 1))
				/* (x should be positive) */

# define mad_f_fromint(x)	((x) << MAD_F_FRACBITS)



# if defined(FPM_FLOAT)
#  error "FPM_FLOAT not yet supported"

#  undef MAD_F
#  define MAD_F(x)		mad_f_todouble(x)

#define mad_f_mul(x, y)	((x) * (y))
#define mad_f_scale64

#  undef ASO_ZEROCHECK

# elif defined(FPM_64BIT)

/*
 * This version should be the most accurate if 64-bit types are supported by
 * the compiler, although it may not be the most efficient.
 */
#  if defined(OPT_ACCURACY)
#   define mad_f_mul(x, y)  \
    ((mad_fixed_t)  \
     ((((mad_fixed64_t) (x) * (y)) +  \
       (1L << (MAD_F_SCALEBITS - 1))) >> MAD_F_SCALEBITS))
#  else
#   define mad_f_mul(x, y)  \
    ((mad_fixed_t) (((mad_fixed64_t) (x) * (y)) >> MAD_F_SCALEBITS))
#  endif

#  define MAD_F_SCALEBITS  MAD_F_FRACBITS

/* --- Intel --------------------------------------------------------------- */

# elif defined(FPM_INTEL)

#  if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4035)  /* no return value */
static __forceinline
mad_fixed_t mad_f_mul_inline(mad_fixed_t x, mad_fixed_t y)
{
  enum {
    fracbits = MAD_F_FRACBITS
  };

  __asm {
    mov eax, x
    imul y
    shrd eax, edx, fracbits
  }

  /* implicit return of eax */
}
#   pragma warning(pop)

#   define mad_f_mul		mad_f_mul_inline
#   define mad_f_scale64
#  else
/*
 * This Intel version is fast and accurate; the disposition of the least
 * significant bit depends on OPT_ACCURACY via mad_f_scale64().
 */
#   define MAD_F_MLX(hi, lo, x, y)  \
    asm ("imull %3"  \
	 : "=a" (lo), "=d" (hi)  \
	 : "%a" (x), "rm" (y)  \
	 : "cc")

#   if defined(OPT_ACCURACY)
/*
 * This gives best accuracy but is not very fast.
 */
#    define MAD_F_MLA(hi, lo, x, y)  \
    ({ mad_fixed64hi_t __hi;  \
       mad_fixed64lo_t __lo;  \
       MAD_F_MLX(__hi, __lo, (x), (y));  \
       asm ("addl %2,%0\n\t"  \
	    "adcl %3,%1"  \
	    : "=rm" (lo), "=rm" (hi)  \
	    : "r" (__lo), "r" (__hi), "0" (lo), "1" (hi)  \
	    : "cc");  \
    })
#   endif  /* OPT_ACCURACY */

#   if defined(OPT_ACCURACY)
/*
 * Surprisingly, this is faster than SHRD followed by ADC.
 */
#    define mad_f_scale64(hi, lo)  \
    ({ mad_fixed64hi_t __hi_;  \
       mad_fixed64lo_t __lo_;  \
       mad_fixed_t __result;  \
       asm ("addl %4,%2\n\t"  \
	    "adcl %5,%3"  \
	    : "=rm" (__lo_), "=rm" (__hi_)  \
	    : "0" (lo), "1" (hi),  \
	      "ir" (1L << (MAD_F_SCALEBITS - 1)), "ir" (0)  \
	    : "cc");  \
       asm ("shrdl %3,%2,%1"  \
	    : "=rm" (__result)  \
	    : "0" (__lo_), "r" (__hi_), "I" (MAD_F_SCALEBITS)  \
	    : "cc");  \
       __result;  \
    })
#   elif defined(OPT_INTEL)
/*
 * Alternate Intel scaling that may or may not perform better.
 */
#    define mad_f_scale64(hi, lo)  \
    ({ mad_fixed_t __result;  \
       asm ("shrl %3,%1\n\t"  \
	    "shll %4,%2\n\t"  \
	    "orl %2,%1"  \
	    : "=rm" (__result)  \
	    : "0" (lo), "r" (hi),  \
	      "I" (MAD_F_SCALEBITS), "I" (32 - MAD_F_SCALEBITS)  \
	    : "cc");  \
       __result;  \
    })
#   else
#    define mad_f_scale64(hi, lo)  \
    ({ mad_fixed_t __result;  \
       asm ("shrdl %3,%2,%1"  \
	    : "=rm" (__result)  \
	    : "0" (lo), "r" (hi), "I" (MAD_F_SCALEBITS)  \
	    : "cc");  \
       __result;  \
    })
#   endif  /* OPT_ACCURACY */

#   define MAD_F_SCALEBITS  MAD_F_FRACBITS
#  endif

/* --- ARM ----------------------------------------------------------------- */

# elif defined(FPM_ARM)

/* 
 * This ARM V4 version is as accurate as FPM_64BIT but much faster. The
 * least significant bit is properly rounded at no CPU cycle cost!
 */
# if 1
/*
 * This is faster than the default implementation via MAD_F_MLX() and
 * mad_f_scale64().
 */
#  define mad_f_mul(x, y)  \
    ({ mad_fixed64hi_t __hi;  \
       mad_fixed64lo_t __lo;  \
       mad_fixed_t __result;  \
       asm ("smull	%0, %1, %3, %4\n\t"  \
	    "movs	%0, %0, lsr %5\n\t"  \
	    "adc	%2, %0, %1, lsl %6"  \
	    : "=&r" (__lo), "=&r" (__hi), "=r" (__result)  \
	    : "%r" (x), "r" (y),  \
	      "M" (MAD_F_SCALEBITS), "M" (32 - MAD_F_SCALEBITS)  \
	    : "cc");  \
       __result;  \
    })
# endif

#  define MAD_F_MLX(hi, lo, x, y)  \
    asm ("smull	%0, %1, %2, %3"  \
	 : "=&r" (lo), "=&r" (hi)  \
	 : "%r" (x), "r" (y))

#  define MAD_F_MLA(hi, lo, x, y)  \
    asm ("smlal	%0, %1, %2, %3"  \
	 : "+r" (lo), "+r" (hi)  \
	 : "%r" (x), "r" (y))

#  define MAD_F_MLN(hi, lo)  \
    asm ("rsbs	%0, %2, #0\n\t"  \
	 "rsc	%1, %3, #0"  \
	 : "=r" (lo), "=r" (hi)  \
	 : "0" (lo), "1" (hi)  \
	 : "cc")

#  define mad_f_scale64(hi, lo)  \
    ({ mad_fixed_t __result;  \
       asm ("movs	%0, %1, lsr %3\n\t"  \
	    "adc	%0, %0, %2, lsl %4"  \
	    : "=&r" (__result)  \
	    : "r" (lo), "r" (hi),  \
	      "M" (MAD_F_SCALEBITS), "M" (32 - MAD_F_SCALEBITS)  \
	    : "cc");  \
       __result;  \
    })

#  define MAD_F_SCALEBITS  MAD_F_FRACBITS

/* --- MIPS ---------------------------------------------------------------- */

# elif defined(FPM_MIPS)

/*
 * This MIPS version is fast and accurate; the disposition of the least
 * significant bit depends on OPT_ACCURACY via mad_f_scale64().
 */
#  define MAD_F_MLX(hi, lo, x, y)  \
    asm ("mult	%2,%3"  \
	 : "=l" (lo), "=h" (hi)  \
	 : "%r" (x), "r" (y))

# if defined(HAVE_MADD_ASM)
#  define MAD_F_MLA(hi, lo, x, y)  \
    asm ("madd	%2,%3"  \
	 : "+l" (lo), "+h" (hi)  \
	 : "%r" (x), "r" (y))
# elif defined(HAVE_MADD16_ASM)
/*
 * This loses significant accuracy due to the 16-bit integer limit in the
 * multiply/accumulate instruction.
 */
#  define MAD_F_ML0(hi, lo, x, y)  \
    asm ("mult	%2,%3"  \
	 : "=l" (lo), "=h" (hi)  \
	 : "%r" ((x) >> 12), "r" ((y) >> 16))
#  define MAD_F_MLA(hi, lo, x, y)  \
    asm ("madd16	%2,%3"  \
	 : "+l" (lo), "+h" (hi)  \
	 : "%r" ((x) >> 12), "r" ((y) >> 16))
#  define MAD_F_MLZ(hi, lo)  ((mad_fixed_t) (lo))
# endif

# if defined(OPT_SPEED)
#  define mad_f_scale64(hi, lo)  \
    ((mad_fixed_t) ((hi) << (32 - MAD_F_SCALEBITS)))
#  define MAD_F_SCALEBITS  MAD_F_FRACBITS
# endif

/* --- SPARC --------------------------------------------------------------- */

# elif defined(FPM_SPARC)

/*
 * This SPARC V8 version is fast and accurate; the disposition of the least
 * significant bit depends on OPT_ACCURACY via mad_f_scale64().
 */
#  define MAD_F_MLX(hi, lo, x, y)  \
    asm ("smul %2, %3, %0\n\t"  \
	 "rd %%y, %1"  \
	 : "=r" (lo), "=r" (hi)  \
	 : "%r" (x), "rI" (y))

/* --- PowerPC ------------------------------------------------------------- */

# elif defined(FPM_PPC)

/*
 * This PowerPC version is fast and accurate; the disposition of the least
 * significant bit depends on OPT_ACCURACY via mad_f_scale64().
 */
#  define MAD_F_MLX(hi, lo, x, y)  \
    do {  \
      asm ("mullw %0,%1,%2"  \
	   : "=r" (lo)  \
	   : "%r" (x), "r" (y));  \
      asm ("mulhw %0,%1,%2"  \
	   : "=r" (hi)  \
	   : "%r" (x), "r" (y));  \
    }  \
    while (0)

#  if defined(OPT_ACCURACY)
/*
 * This gives best accuracy but is not very fast.
 */
#   define MAD_F_MLA(hi, lo, x, y)  \
    ({ mad_fixed64hi_t __hi;  \
       mad_fixed64lo_t __lo;  \
       MAD_F_MLX(__hi, __lo, (x), (y));  \
       asm ("addc %0,%2,%3\n\t"  \
	    "adde %1,%4,%5"  \
	    : "=r" (lo), "=r" (hi)  \
	    : "%r" (lo), "r" (__lo),  \
	      "%r" (hi), "r" (__hi)  \
	    : "xer");  \
    })
#  endif

#  if defined(OPT_ACCURACY)
/*
 * This is slower than the truncating version below it.
 */
#   define mad_f_scale64(hi, lo)  \
    ({ mad_fixed_t __result, __round;  \
       asm ("rotrwi %0,%1,%2"  \
	    : "=r" (__result)  \
	    : "r" (lo), "i" (MAD_F_SCALEBITS));  \
       asm ("extrwi %0,%1,1,0"  \
	    : "=r" (__round)  \
	    : "r" (__result));  \
       asm ("insrwi %0,%1,%2,0"  \
	    : "+r" (__result)  \
	    : "r" (hi), "i" (MAD_F_SCALEBITS));  \
       asm ("add %0,%1,%2"  \
	    : "=r" (__result)  \
	    : "%r" (__result), "r" (__round));  \
       __result;  \
    })
#  else
#   define mad_f_scale64(hi, lo)  \
    ({ mad_fixed_t __result;  \
       asm ("rotrwi %0,%1,%2"  \
	    : "=r" (__result)  \
	    : "r" (lo), "i" (MAD_F_SCALEBITS));  \
       asm ("insrwi %0,%1,%2,0"  \
	    : "+r" (__result)  \
	    : "r" (hi), "i" (MAD_F_SCALEBITS));  \
       __result;  \
    })
#  endif
 
#  define MAD_F_SCALEBITS  MAD_F_FRACBITS

/* --- Default ------------------------------------------------------------- */

# elif defined(FPM_DEFAULT)

/*
 * This version is the most portable but it loses significant accuracy.
 * Furthermore, accuracy is biased against the second argument, so care
 * should be taken when ordering operands.
 *
 * The scale factors are constant as this is not used with SSO.
 *
 * Pre-rounding is required to stay within the limits of compliance.
 */
#  if defined(OPT_SPEED)
#   define mad_f_mul(x, y)	(((x) >> 12) * ((y) >> 16))
#  else
#   define mad_f_mul(x, y)	((((x) + (1L << 11)) >> 12) *  \
				 (((y) + (1L << 15)) >> 16))
#  endif

/* ------------------------------------------------------------------------- */

# else
#  error "no FPM selected"
# endif

/* default implementations */

# if !defined(mad_f_mul)
#  define mad_f_mul(x, y)  \
    ({ register mad_fixed64hi_t __hi;  \
       register mad_fixed64lo_t __lo;  \
       MAD_F_MLX(__hi, __lo, (x), (y));  \
       mad_f_scale64(__hi, __lo);  \
    })
# endif

# if !defined(MAD_F_MLA)
#  define MAD_F_ML0(hi, lo, x, y)	((lo)  = mad_f_mul((x), (y)))
#  define MAD_F_MLA(hi, lo, x, y)	((lo) += mad_f_mul((x), (y)))
#  define MAD_F_MLN(hi, lo)		((lo)  = -(lo))
#  define MAD_F_MLZ(hi, lo)		((void) (hi), (mad_fixed_t) (lo))
# endif

# if !defined(MAD_F_ML0)
#  define MAD_F_ML0(hi, lo, x, y)	MAD_F_MLX((hi), (lo), (x), (y))
# endif

# if !defined(MAD_F_MLN)
#  define MAD_F_MLN(hi, lo)		((hi) = ((lo) = -(lo)) ? ~(hi) : -(hi))
# endif

# if !defined(MAD_F_MLZ)
#  define MAD_F_MLZ(hi, lo)		mad_f_scale64((hi), (lo))
# endif

# if !defined(mad_f_scale64)
#  if defined(OPT_ACCURACY)
#   define mad_f_scale64(hi, lo)  \
    ((((mad_fixed_t)  \
       (((hi) << (32 - (MAD_F_SCALEBITS - 1))) |  \
	((lo) >> (MAD_F_SCALEBITS - 1)))) + 1) >> 1)
#  else
#   define mad_f_scale64(hi, lo)  \
    ((mad_fixed_t)  \
     (((hi) << (32 - MAD_F_SCALEBITS)) |  \
      ((lo) >> MAD_F_SCALEBITS)))
#  endif
#  define MAD_F_SCALEBITS  MAD_F_FRACBITS
# endif

/* C routines */

mad_fixed_t mad_f_abs(mad_fixed_t);
mad_fixed_t mad_f_div(mad_fixed_t, mad_fixed_t);



//*********************** fixed.h END **********************/





// ******************** stream.h  *****************************/





















// ********************* stream.h END ************************/


// *********************** version.h ***************************/



# define MAD_VERSION_MAJOR	0
# define MAD_VERSION_MINOR	15
# define MAD_VERSION_PATCH	1
# define MAD_VERSION_EXTRA	" (beta)"

# define MAD_VERSION_STRINGIZE(str)	#str
# define MAD_VERSION_STRING(num)	MAD_VERSION_STRINGIZE(num)

# define MAD_VERSION		MAD_VERSION_STRING(MAD_VERSION_MAJOR) "."  \
				MAD_VERSION_STRING(MAD_VERSION_MINOR) "."  \
				MAD_VERSION_STRING(MAD_VERSION_PATCH)  \
				MAD_VERSION_EXTRA

# define MAD_PUBLISHYEAR	"2000-2004"
# define MAD_AUTHOR		"Underbit Technologies, Inc."
# define MAD_EMAIL		"info@underbit.com"
 /*
char  mad_version[];
char  mad_copyright[];
char mad_author[];
char  mad_build[];
   */

// ********************* version.h END *******************/



// ************************ frame.h **********************/













# define MAD_NCHANNELS(header)		((header)->mode ? 2 : 1)






enum {
  MAD_PRIVATE_HEADER	= 0x0100,	/* header private bit */
  MAD_PRIVATE_III	= 0x001f	/* Layer III private bits (up to 5) */
};






// ********************* frame.h END ************************/


// *********************** layer12.h ****************************/

int mad_layer_I(struct mad_stream *, struct mad_frame *);
int mad_layer_II(struct mad_stream *, struct mad_frame *);

// ********************** layer12.h END *******************/


// ************************ synth.h ************************/





/* single channel PCM selector */
enum {
  MAD_PCM_CHANNEL_SINGLE = 0
};

/* dual channel PCM selector */
enum {
  MAD_PCM_CHANNEL_DUAL_1 = 0,
  MAD_PCM_CHANNEL_DUAL_2 = 1
};

/* stereo PCM selector */
enum {
  MAD_PCM_CHANNEL_STEREO_LEFT  = 0,
  MAD_PCM_CHANNEL_STEREO_RIGHT = 1
};

/*
void mad_synth_init(struct mad_synth *);

# define mad_synth_finish(synth) 

void mad_synth_mute(struct mad_synth *);

void mad_synth_frame(struct mad_synth *, struct mad_frame const *);
*/

// ******************** synth.h END *********************/






// *********************** huffman.h   **********************/

union huffquad {
  struct {
    unsigned short final  :  1;
    unsigned short bits   :  3;
    unsigned short offset : 12;
  } ptr;
  struct {
    unsigned short final  :  1;
    unsigned short hlen   :  3;
    unsigned short v      :  1;
    unsigned short w      :  1;
    unsigned short x      :  1;
    unsigned short y      :  1;
  } value;
  unsigned short final    :  1;
};

union huffpair {
  struct {
    unsigned short final  :  1;
    unsigned short bits   :  3;
    unsigned short offset : 12;
  } ptr;
  struct {
    unsigned short final  :  1;
    unsigned short hlen   :  3;
    unsigned short x      :  4;
    unsigned short y      :  4;
  } value;
  unsigned short final    :  1;
};

struct hufftable {
  union huffpair const *table;
  unsigned short linbits;
  unsigned short startbits;
};

//union huffquad const *const mad_huff_quad_table[2];
//struct hufftable const mad_huff_pair_table[32];



// ********************** huffman.h END ********************** /


// ********************* layer3.h *******************************/

int mad_layer_III(struct mad_stream *, struct mad_frame *);






# if defined(_MSC_VER)
#  define mad_fixed64_t  signed __int64
# elif 1 || defined(__GNUC__)
#  define mad_fixed64_t  signed long long
# endif


# if defined(FPM_FLOAT)
typedef double mad_sample_t;
# else
typedef mad_fixed_t mad_sample_t;
# endif



# if MAD_F_FRACBITS == 28
#  define MAD_F(x)		((mad_fixed_t) (x##L))
# else
#  if MAD_F_FRACBITS < 28
#   warning "MAD_F_FRACBITS < 28"
#   define MAD_F(x)		((mad_fixed_t)  \
				 (((x##L) +  \
				   (1L << (28 - MAD_F_FRACBITS - 1))) >>  \
				  (28 - MAD_F_FRACBITS)))
#  elif MAD_F_FRACBITS > 28
#   error "MAD_F_FRACBITS > 28 not currently supported"
#   define MAD_F(x)		((mad_fixed_t)  \
				 ((x##L) << (MAD_F_FRACBITS - 28)))
#  endif
# endif

# define MAD_F_MIN		((mad_fixed_t) -0x80000000L)
# define MAD_F_MAX		((mad_fixed_t) +0x7fffffffL)


# define mad_f_tofixed(x)	((mad_fixed_t)  \
				 ((x) * (double) (1L << MAD_F_FRACBITS) + 0.5))
# define mad_f_todouble(x)	((double)  \
				 ((x) / (double) (1L << MAD_F_FRACBITS)))

# define mad_f_intpart(x)	((x) >> MAD_F_FRACBITS)
# define mad_f_fracpart(x)	((x) & ((1L << MAD_F_FRACBITS) - 1))
				/* (x should be positive) */

# define mad_f_fromint(x)	((x) << MAD_F_FRACBITS)

# define mad_f_add(x, y)	((x) + (y))
# define mad_f_sub(x, y)	((x) - (y))

# if defined(FPM_FLOAT)
#  error "FPM_FLOAT not yet supported"

#  undef MAD_F
#  define MAD_F(x)		mad_f_todouble(x)

#  define mad_f_mul(x, y)	((x) * (y))
#  define mad_f_scale64

#  undef ASO_ZEROCHECK

# elif defined(FPM_64BIT)

/*
 * This version should be the most accurate if 64-bit types are supported by
 * the compiler, although it may not be the most efficient.
 */
 
#  if defined(OPT_ACCURACY)
#   define mad_f_mul(x, y)  \
    ((mad_fixed_t)  \
     ((((mad_fixed64_t) (x) * (y)) +  \
       (1L << (MAD_F_SCALEBITS - 1))) >> MAD_F_SCALEBITS))
#  else
#   define mad_f_mul(x, y)  \
    ((mad_fixed_t) (((mad_fixed64_t) (x) * (y)) >> MAD_F_SCALEBITS))
#  endif

#  define MAD_F_SCALEBITS  MAD_F_FRACBITS

/* --- Intel --------------------------------------------------------------- */

# elif defined(FPM_INTEL)

#  if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4035)  /* no return value */


#   pragma warning(pop)

#   define mad_f_mul		mad_f_mul_inline
#   define mad_f_scale64
#  else
/*
 * This Intel version is fast and accurate; the disposition of the least
 * significant bit depends on OPT_ACCURACY via mad_f_scale64().
 */
#   define MAD_F_MLX(hi, lo, x, y)  \
    asm ("imull %3"  \
	 : "=a" (lo), "=d" (hi)  \
	 : "%a" (x), "rm" (y)  \
	 : "cc")

#   if defined(OPT_ACCURACY)
/*
 * This gives best accuracy but is not very fast.
 */
#    define MAD_F_MLA(hi, lo, x, y)  \
    ({ mad_fixed64hi_t __hi;  \
       mad_fixed64lo_t __lo;  \
       MAD_F_MLX(__hi, __lo, (x), (y));  \
       asm ("addl %2,%0\n\t"  \
	    "adcl %3,%1"  \
	    : "=rm" (lo), "=rm" (hi)  \
	    : "r" (__lo), "r" (__hi), "0" (lo), "1" (hi)  \
	    : "cc");  \
    })
#   endif  /* OPT_ACCURACY */

#   if defined(OPT_ACCURACY)
/*
 * Surprisingly, this is faster than SHRD followed by ADC.
 */
#    define mad_f_scale64(hi, lo)  \
    ({ mad_fixed64hi_t __hi_;  \
       mad_fixed64lo_t __lo_;  \
       mad_fixed_t __result;  \
       asm ("addl %4,%2\n\t"  \
	    "adcl %5,%3"  \
	    : "=rm" (__lo_), "=rm" (__hi_)  \
	    : "0" (lo), "1" (hi),  \
	      "ir" (1L << (MAD_F_SCALEBITS - 1)), "ir" (0)  \
	    : "cc");  \
       asm ("shrdl %3,%2,%1"  \
	    : "=rm" (__result)  \
	    : "0" (__lo_), "r" (__hi_), "I" (MAD_F_SCALEBITS)  \
	    : "cc");  \
       __result;  \
    })
#    else
#    define mad_f_scale64(hi, lo)  \
    ({ mad_fixed_t __result;  \
       asm ("shrdl %3,%2,%1"  \
	    : "=rm" (__result)  \
	    : "0" (lo), "r" (hi), "I" (MAD_F_SCALEBITS)  \
	    : "cc");  \
       __result;  \
    })
#   endif  /* OPT_ACCURACY */

#   define MAD_F_SCALEBITS  MAD_F_FRACBITS
#  endif

/* --- ARM ----------------------------------------------------------------- */

# elif defined(FPM_ARM)

/* 
 * This ARM V4 version is as accurate as FPM_64BIT but much faster. The
 * least significant bit is properly rounded at no CPU cycle cost!
 */
# if 1
/*
 * This is faster than the default implementation via MAD_F_MLX() and
 * mad_f_scale64().
 */
#  define mad_f_mul(x, y)  \
    ({ mad_fixed64hi_t __hi;  \
       mad_fixed64lo_t __lo;  \
       mad_fixed_t __result;  \
       asm ("smull	%0, %1, %3, %4\n\t"  \
	    "movs	%0, %0, lsr %5\n\t"  \
	    "adc	%2, %0, %1, lsl %6"  \
	    : "=&r" (__lo), "=&r" (__hi), "=r" (__result)  \
	    : "%r" (x), "r" (y),  \
	      "M" (MAD_F_SCALEBITS), "M" (32 - MAD_F_SCALEBITS)  \
	    : "cc");  \
       __result;  \
    })
# endif

#  define MAD_F_MLX(hi, lo, x, y)  \
    asm ("smull	%0, %1, %2, %3"  \
	 : "=&r" (lo), "=&r" (hi)  \
	 : "%r" (x), "r" (y))

#  define MAD_F_MLA(hi, lo, x, y)  \
    asm ("smlal	%0, %1, %2, %3"  \
	 : "+r" (lo), "+r" (hi)  \
	 : "%r" (x), "r" (y))

#  define MAD_F_MLN(hi, lo)  \
    asm ("rsbs	%0, %2, #0\n\t"  \
	 "rsc	%1, %3, #0"  \
	 : "=r" (lo), "=r" (hi)  \
	 : "0" (lo), "1" (hi)  \
	 : "cc")

#  define mad_f_scale64(hi, lo)  \
    ({ mad_fixed_t __result;  \
       asm ("movs	%0, %1, lsr %3\n\t"  \
	    "adc	%0, %0, %2, lsl %4"  \
	    : "=&r" (__result)  \
	    : "r" (lo), "r" (hi),  \
	      "M" (MAD_F_SCALEBITS), "M" (32 - MAD_F_SCALEBITS)  \
	    : "cc");  \
       __result;  \
    })

#  define MAD_F_SCALEBITS  MAD_F_FRACBITS

/* --- MIPS ---------------------------------------------------------------- */

# elif defined(FPM_MIPS)

/*
 * This MIPS version is fast and accurate; the disposition of the least
 * significant bit depends on OPT_ACCURACY via mad_f_scale64().
 */
#  define MAD_F_MLX(hi, lo, x, y)  \
    asm ("mult	%2,%3"  \
	 : "=l" (lo), "=h" (hi)  \
	 : "%r" (x), "r" (y))

# if defined(HAVE_MADD_ASM)
#  define MAD_F_MLA(hi, lo, x, y)  \
    asm ("madd	%2,%3"  \
	 : "+l" (lo), "+h" (hi)  \
	 : "%r" (x), "r" (y))
# elif defined(HAVE_MADD16_ASM)
/*
 * This loses significant accuracy due to the 16-bit integer limit in the
 * multiply/accumulate instruction.
 */
#  define MAD_F_ML0(hi, lo, x, y)  \
    asm ("mult	%2,%3"  \
	 : "=l" (lo), "=h" (hi)  \
	 : "%r" ((x) >> 12), "r" ((y) >> 16))
#  define MAD_F_MLA(hi, lo, x, y)  \
    asm ("madd16	%2,%3"  \
	 : "+l" (lo), "+h" (hi)  \
	 : "%r" ((x) >> 12), "r" ((y) >> 16))
#  define MAD_F_MLZ(hi, lo)  ((mad_fixed_t) (lo))
# endif

# if defined(OPT_SPEED)
#  define mad_f_scale64(hi, lo)  \
    ((mad_fixed_t) ((hi) << (32 - MAD_F_SCALEBITS)))
#  define MAD_F_SCALEBITS  MAD_F_FRACBITS
# endif

/* --- SPARC --------------------------------------------------------------- */

# elif defined(FPM_SPARC)

/*
 * This SPARC V8 version is fast and accurate; the disposition of the least
 * significant bit depends on OPT_ACCURACY via mad_f_scale64().
 */
#  define MAD_F_MLX(hi, lo, x, y)  \
    asm ("smul %2, %3, %0\n\t"  \
	 "rd %%y, %1"  \
	 : "=r" (lo), "=r" (hi)  \
	 : "%r" (x), "rI" (y))

/* --- PowerPC ------------------------------------------------------------- */

# elif defined(FPM_PPC)

/*
 * This PowerPC version is fast and accurate; the disposition of the least
 * significant bit depends on OPT_ACCURACY via mad_f_scale64().
 */
#  define MAD_F_MLX(hi, lo, x, y)  \
    do {  \
      asm ("mullw %0,%1,%2"  \
	   : "=r" (lo)  \
	   : "%r" (x), "r" (y));  \
      asm ("mulhw %0,%1,%2"  \
	   : "=r" (hi)  \
	   : "%r" (x), "r" (y));  \
    }  \
    while (0)

#  if defined(OPT_ACCURACY)
/*
 * This gives best accuracy but is not very fast.
 */
#   define MAD_F_MLA(hi, lo, x, y)  \
    ({ mad_fixed64hi_t __hi;  \
       mad_fixed64lo_t __lo;  \
       MAD_F_MLX(__hi, __lo, (x), (y));  \
       asm ("addc %0,%2,%3\n\t"  \
	    "adde %1,%4,%5"  \
	    : "=r" (lo), "=r" (hi)  \
	    : "%r" (lo), "r" (__lo),  \
	      "%r" (hi), "r" (__hi)  \
	    : "xer");  \
    })
#  endif

#  if defined(OPT_ACCURACY)
/*
 * This is slower than the truncating version below it.
 */
#   define mad_f_scale64(hi, lo)  \
    ({ mad_fixed_t __result, __round;  \
       asm ("rotrwi %0,%1,%2"  \
	    : "=r" (__result)  \
	    : "r" (lo), "i" (MAD_F_SCALEBITS));  \
       asm ("extrwi %0,%1,1,0"  \
	    : "=r" (__round)  \
	    : "r" (__result));  \
       asm ("insrwi %0,%1,%2,0"  \
	    : "+r" (__result)  \
	    : "r" (hi), "i" (MAD_F_SCALEBITS));  \
       asm ("add %0,%1,%2"  \
	    : "=r" (__result)  \
	    : "%r" (__result), "r" (__round));  \
       __result;  \
    })
#  else
#   define mad_f_scale64(hi, lo)  \
    ({ mad_fixed_t __result;  \
       asm ("rotrwi %0,%1,%2"  \
	    : "=r" (__result)  \
	    : "r" (lo), "i" (MAD_F_SCALEBITS));  \
       asm ("insrwi %0,%1,%2,0"  \
	    : "+r" (__result)  \
	    : "r" (hi), "i" (MAD_F_SCALEBITS));  \
       __result;  \
    })
#  endif

#  define MAD_F_SCALEBITS  MAD_F_FRACBITS

/* --- Default ------------------------------------------------------------- */

# elif defined(FPM_DEFAULT)

/*
 * This version is the most portable but it loses significant accuracy.
 * Furthermore, accuracy is biased against the second argument, so care
 * should be taken when ordering operands.
 *
 * The scale factors are constant as this is not used with SSO.
 *
 * Pre-rounding is required to stay within the limits of compliance.
 */
#  if defined(OPT_SPEED)
#   define mad_f_mul(x, y)	(((x) >> 12) * ((y) >> 16))
#  else
#   define mad_f_mul(x, y)	((((x) + (1L << 11)) >> 12) *  \
				 (((y) + (1L << 15)) >> 16))
#  endif

/* ------------------------------------------------------------------------- */

# else
#  error "no FPM selected"
# endif

/* default implementations */

# if !defined(mad_f_mul)
#  define mad_f_mul(x, y)  \
    ({ register mad_fixed64hi_t __hi;  \
       register mad_fixed64lo_t __lo;  \
       MAD_F_MLX(__hi, __lo, (x), (y));  \
       mad_f_scale64(__hi, __lo);  \
    })
# endif

# if !defined(MAD_F_MLA)
#  define MAD_F_ML0(hi, lo, x, y)	((lo)  = mad_f_mul((x), (y)))
#  define MAD_F_MLA(hi, lo, x, y)	((lo) += mad_f_mul((x), (y)))
#  define MAD_F_MLN(hi, lo)		((lo)  = -(lo))
#  define MAD_F_MLZ(hi, lo)		((void) (hi), (mad_fixed_t) (lo))
# endif

# if !defined(MAD_F_ML0)
#  define MAD_F_ML0(hi, lo, x, y)	MAD_F_MLX((hi), (lo), (x), (y))
# endif

# if !defined(MAD_F_MLN)
#  define MAD_F_MLN(hi, lo)		((hi) = ((lo) = -(lo)) ? ~(hi) : -(hi))
# endif

# if !defined(MAD_F_MLZ)
#  define MAD_F_MLZ(hi, lo)		mad_f_scale64((hi), (lo))
# endif

# if !defined(mad_f_scale64)
#  if defined(OPT_ACCURACY)
#   define mad_f_scale64(hi, lo)  \
    ((((mad_fixed_t)  \
       (((hi) << (32 - (MAD_F_SCALEBITS - 1))) |  \
	((lo) >> (MAD_F_SCALEBITS - 1)))) + 1) >> 1)
#  else
#   define mad_f_scale64(hi, lo)  \
    ((mad_fixed_t)  \
     (((hi) << (32 - MAD_F_SCALEBITS)) |  \
      ((lo) >> MAD_F_SCALEBITS)))
#  endif
#  define MAD_F_SCALEBITS  MAD_F_FRACBITS
# endif

/* C routines */

mad_fixed_t mad_f_abs(mad_fixed_t);
mad_fixed_t mad_f_div(mad_fixed_t, mad_fixed_t);







/* Id: decoder.h,v 1.16 2003/05/27 22:40:36 rob Exp */

# ifndef LIBMAD_DECODER_H
# define LIBMAD_DECODER_H





int mad_decoder_finish(struct mad_decoder *);

# define mad_decoder_options(decoder, opts)  \
    ((void) ((decoder)->options = (opts)))


int mad_decoder_message(struct mad_decoder *, void *, unsigned int *);

# endif


// ********************* mad.h END ***************************/




// ******************** bit.c ********************************/


# ifdef HAVE_LIMITS_H
#  include <limits.h>
# else
#  define CHAR_BIT  8
# endif



/*
 * This is the lookup table for computing the CRC-check word.
 * As described in section 2.4.3.1 and depicted in Figure A.9
 * of ISO/IEC 11172-3, the generator polynomial is:
 *
 * G(X) = X^16 + X^15 + X^2 + 1
 */
static
unsigned short const crc_table[256] = {
  0x0000, 0x8005, 0x800f, 0x000a, 0x801b, 0x001e, 0x0014, 0x8011,
  0x8033, 0x0036, 0x003c, 0x8039, 0x0028, 0x802d, 0x8027, 0x0022,
  0x8063, 0x0066, 0x006c, 0x8069, 0x0078, 0x807d, 0x8077, 0x0072,
  0x0050, 0x8055, 0x805f, 0x005a, 0x804b, 0x004e, 0x0044, 0x8041,
  0x80c3, 0x00c6, 0x00cc, 0x80c9, 0x00d8, 0x80dd, 0x80d7, 0x00d2,
  0x00f0, 0x80f5, 0x80ff, 0x00fa, 0x80eb, 0x00ee, 0x00e4, 0x80e1,
  0x00a0, 0x80a5, 0x80af, 0x00aa, 0x80bb, 0x00be, 0x00b4, 0x80b1,
  0x8093, 0x0096, 0x009c, 0x8099, 0x0088, 0x808d, 0x8087, 0x0082,

  0x8183, 0x0186, 0x018c, 0x8189, 0x0198, 0x819d, 0x8197, 0x0192,
  0x01b0, 0x81b5, 0x81bf, 0x01ba, 0x81ab, 0x01ae, 0x01a4, 0x81a1,
  0x01e0, 0x81e5, 0x81ef, 0x01ea, 0x81fb, 0x01fe, 0x01f4, 0x81f1,
  0x81d3, 0x01d6, 0x01dc, 0x81d9, 0x01c8, 0x81cd, 0x81c7, 0x01c2,
  0x0140, 0x8145, 0x814f, 0x014a, 0x815b, 0x015e, 0x0154, 0x8151,
  0x8173, 0x0176, 0x017c, 0x8179, 0x0168, 0x816d, 0x8167, 0x0162,
  0x8123, 0x0126, 0x012c, 0x8129, 0x0138, 0x813d, 0x8137, 0x0132,
  0x0110, 0x8115, 0x811f, 0x011a, 0x810b, 0x010e, 0x0104, 0x8101,

  0x8303, 0x0306, 0x030c, 0x8309, 0x0318, 0x831d, 0x8317, 0x0312,
  0x0330, 0x8335, 0x833f, 0x033a, 0x832b, 0x032e, 0x0324, 0x8321,
  0x0360, 0x8365, 0x836f, 0x036a, 0x837b, 0x037e, 0x0374, 0x8371,
  0x8353, 0x0356, 0x035c, 0x8359, 0x0348, 0x834d, 0x8347, 0x0342,
  0x03c0, 0x83c5, 0x83cf, 0x03ca, 0x83db, 0x03de, 0x03d4, 0x83d1,
  0x83f3, 0x03f6, 0x03fc, 0x83f9, 0x03e8, 0x83ed, 0x83e7, 0x03e2,
  0x83a3, 0x03a6, 0x03ac, 0x83a9, 0x03b8, 0x83bd, 0x83b7, 0x03b2,
  0x0390, 0x8395, 0x839f, 0x039a, 0x838b, 0x038e, 0x0384, 0x8381,

  0x0280, 0x8285, 0x828f, 0x028a, 0x829b, 0x029e, 0x0294, 0x8291,
  0x82b3, 0x02b6, 0x02bc, 0x82b9, 0x02a8, 0x82ad, 0x82a7, 0x02a2,
  0x82e3, 0x02e6, 0x02ec, 0x82e9, 0x02f8, 0x82fd, 0x82f7, 0x02f2,
  0x02d0, 0x82d5, 0x82df, 0x02da, 0x82cb, 0x02ce, 0x02c4, 0x82c1,
  0x8243, 0x0246, 0x024c, 0x8249, 0x0258, 0x825d, 0x8257, 0x0252,
  0x0270, 0x8275, 0x827f, 0x027a, 0x826b, 0x026e, 0x0264, 0x8261,
  0x0220, 0x8225, 0x822f, 0x022a, 0x823b, 0x023e, 0x0234, 0x8231,
  0x8213, 0x0216, 0x021c, 0x8219, 0x0208, 0x820d, 0x8207, 0x0202
};

# define CRC_POLY  0x8005

/*
 * NAME:	bit->init()
 * DESCRIPTION:	initialize bit pointer struct
 */
void mad_bit_init(struct mad_bitptr *bitptr, unsigned char const *byte)
{
  bitptr->byte  = byte;
  bitptr->cache = 0;
  bitptr->left  = CHAR_BIT;
}

/*
 * NAME:	bit->length()
 * DESCRIPTION:	return number of bits between start and end points
 */
unsigned int mad_bit_length(struct mad_bitptr const *begin,
			    struct mad_bitptr const *end)
{
  return begin->left +
    CHAR_BIT * (end->byte - (begin->byte + 1)) + (CHAR_BIT - end->left);
}

/*
 * NAME:	bit->nextbyte()
 * DESCRIPTION:	return pointer to next unprocessed byte
 */
unsigned char const *mad_bit_nextbyte(struct mad_bitptr const *bitptr)
{
  return bitptr->left == CHAR_BIT ? bitptr->byte : bitptr->byte + 1;
}


unsigned char const *mad_bit_prevbyte(struct mad_bitptr const *bitptr)
{
  return bitptr->left == CHAR_BIT ? bitptr->byte : bitptr->byte - 1;
}



/*
 * NAME:	bit->skip()
 * DESCRIPTION:	advance bit pointer
 */
void mad_bit_skip(struct mad_bitptr *bitptr, unsigned int len)
{
  bitptr->byte += len / CHAR_BIT;
  bitptr->left -= len % CHAR_BIT;

  if (bitptr->left > CHAR_BIT) {
    bitptr->byte++;
    bitptr->left += CHAR_BIT;
  }

  if (bitptr->left < CHAR_BIT)
    bitptr->cache = *bitptr->byte;
}

/*
 * NAME:	bit->read()
 * DESCRIPTION:	read an arbitrary number of bits and return their UIMSBF value
 */
unsigned long mad_bit_read(struct mad_bitptr *bitptr, unsigned int len)
{
  register unsigned long value;

  if (bitptr->left == CHAR_BIT)
    bitptr->cache = *bitptr->byte;

  if (len < bitptr->left) {
    value = (bitptr->cache & ((1 << bitptr->left) - 1)) >>
      (bitptr->left - len);
    bitptr->left -= len;

    return value;
  }

  /* remaining bits in current byte */

  value = bitptr->cache & ((1 << bitptr->left) - 1);
  len  -= bitptr->left;

  bitptr->byte++;
  bitptr->left = CHAR_BIT;

  /* more bytes */

  while (len >= CHAR_BIT) {
    value = (value << CHAR_BIT) | *bitptr->byte++;
    len  -= CHAR_BIT;
  }

  if (len > 0) {
    bitptr->cache = *bitptr->byte;

    value = (value << len) | (bitptr->cache >> (CHAR_BIT - len));
    bitptr->left -= len;
  }

  return value;
}

# if 0
/*
 * NAME:	bit->write()
 * DESCRIPTION:	write an arbitrary number of bits
 */
void mad_bit_write(struct mad_bitptr *bitptr, unsigned int len,
		   unsigned long value)
{
  unsigned char *ptr;

  ptr = (unsigned char *) bitptr->byte;

  /* ... */
}
# endif

/*
 * NAME:	bit->crc()
 * DESCRIPTION:	compute CRC-check word
 */
unsigned short mad_bit_crc(struct mad_bitptr bitptr, unsigned int len,
			   unsigned short init)
{
  register unsigned int crc;

  for (crc = init; len >= 32; len -= 32) {
    register unsigned long data;

    data = mad_bit_read(&bitptr, 32);

    crc = (crc << 8) ^ crc_table[((crc >> 8) ^ (data >> 24)) & 0xff];
    crc = (crc << 8) ^ crc_table[((crc >> 8) ^ (data >> 16)) & 0xff];
    crc = (crc << 8) ^ crc_table[((crc >> 8) ^ (data >>  8)) & 0xff];
    crc = (crc << 8) ^ crc_table[((crc >> 8) ^ (data >>  0)) & 0xff];
  }

  switch (len / 8) {
  case 3: crc = (crc << 8) ^
	    crc_table[((crc >> 8) ^ mad_bit_read(&bitptr, 8)) & 0xff];
  case 2: crc = (crc << 8) ^
	    crc_table[((crc >> 8) ^ mad_bit_read(&bitptr, 8)) & 0xff];
  case 1: crc = (crc << 8) ^
	    crc_table[((crc >> 8) ^ mad_bit_read(&bitptr, 8)) & 0xff];

  len %= 8;

  case 0: break;
  }

  while (len--) {
    register unsigned int msb;

    msb = mad_bit_read(&bitptr, 1) ^ (crc >> 15);

    crc <<= 1;
    if (msb & 1)
      crc ^= CRC_POLY;
  }

  return crc & 0xffff;
}



// ************************** bit.c END ***********************/






// *********************** huffman.c *****************************/ 
# if defined(__GNUC__) ||  \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901)
//#  define PTR(offs, bits)	{ .ptr   = { 0, bits, offs       } }
#  define PTR(offs, bits)	{ { 0, bits, offs } }
//#  define V(v, w, x, y, hlen)	{ .value = { 1, hlen, v, w, x, y } }
#  if defined(WORDS_BIGENDIAN)
#   define V(v, w, x, y, hlen)	{ { 1, hlen, (v << 11) | (w << 10) |  \
                                             (x <<  9) | (y <<  8) } }
#  else
#   define V(v, w, x, y, hlen)	{ { 1, hlen, (v <<  0) | (w <<  1) |  \
                                             (x <<  2) | (y <<  3) } }
#  endif
# else
#  define PTR(offs, bits)	{ { 0, bits, offs } }
#  if defined(WORDS_BIGENDIAN)
#   define V(v, w, x, y, hlen)	{ { 1, hlen, (v << 11) | (w << 10) |  \
                                             (x <<  9) | (y <<  8) } }
#  else
#   define V(v, w, x, y, hlen)	{ { 1, hlen, (v <<  0) | (w <<  1) |  \
                                             (x <<  2) | (y <<  3) } }
#  endif
# endif

static
union huffquad const hufftabA[] = {
  /* 0000 */ PTR(16, 2),
  /* 0001 */ PTR(20, 2),
  /* 0010 */ PTR(24, 1),
  /* 0011 */ PTR(26, 1),
  /* 0100 */ V(0, 0, 1, 0, 4),
  /* 0101 */ V(0, 0, 0, 1, 4),
  /* 0110 */ V(0, 1, 0, 0, 4),
  /* 0111 */ V(1, 0, 0, 0, 4),
  /* 1000 */ V(0, 0, 0, 0, 1),
  /* 1001 */ V(0, 0, 0, 0, 1),
  /* 1010 */ V(0, 0, 0, 0, 1),
  /* 1011 */ V(0, 0, 0, 0, 1),
  /* 1100 */ V(0, 0, 0, 0, 1),
  /* 1101 */ V(0, 0, 0, 0, 1),
  /* 1110 */ V(0, 0, 0, 0, 1),
  /* 1111 */ V(0, 0, 0, 0, 1),

  /* 0000 ... */
  /* 00   */ V(1, 0, 1, 1, 2),	/* 16 */
  /* 01   */ V(1, 1, 1, 1, 2),
  /* 10   */ V(1, 1, 0, 1, 2),
  /* 11   */ V(1, 1, 1, 0, 2),

  /* 0001 ... */
  /* 00   */ V(0, 1, 1, 1, 2),	/* 20 */
  /* 01   */ V(0, 1, 0, 1, 2),
  /* 10   */ V(1, 0, 0, 1, 1),
  /* 11   */ V(1, 0, 0, 1, 1),

  /* 0010 ... */
  /* 0    */ V(0, 1, 1, 0, 1),	/* 24 */
  /* 1    */ V(0, 0, 1, 1, 1),

  /* 0011 ... */
  /* 0    */ V(1, 0, 1, 0, 1),	/* 26 */
  /* 1    */ V(1, 1, 0, 0, 1)
};

static
union huffquad const hufftabB[] = {
  /* 0000 */ V(1, 1, 1, 1, 4),
  /* 0001 */ V(1, 1, 1, 0, 4),
  /* 0010 */ V(1, 1, 0, 1, 4),
  /* 0011 */ V(1, 1, 0, 0, 4),
  /* 0100 */ V(1, 0, 1, 1, 4),
  /* 0101 */ V(1, 0, 1, 0, 4),
  /* 0110 */ V(1, 0, 0, 1, 4),
  /* 0111 */ V(1, 0, 0, 0, 4),
  /* 1000 */ V(0, 1, 1, 1, 4),
  /* 1001 */ V(0, 1, 1, 0, 4),
  /* 1010 */ V(0, 1, 0, 1, 4),
  /* 1011 */ V(0, 1, 0, 0, 4),
  /* 1100 */ V(0, 0, 1, 1, 4),
  /* 1101 */ V(0, 0, 1, 0, 4),
  /* 1110 */ V(0, 0, 0, 1, 4),
  /* 1111 */ V(0, 0, 0, 0, 4)
};

# undef V
# undef PTR

# if defined(__GNUC__) ||  \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901)
//#  define PTR(offs, bits)	{ .ptr   = { 0, bits, offs } }
#  define PTR(offs, bits)	{  { 0, bits, offs } }
//#  define V(x, y, hlen)		{ .value = { 1, hlen, x, y } }
#if defined(WORDS_BIGENDIAN)
#   define V(x, y, hlen)	{ { 1, hlen, (x << 8) | (y << 4) } }
#  else
#   define V(x, y, hlen)	{ { 1, hlen, (x << 0) | (y << 4) } }
#  endif
# else
#  define PTR(offs, bits)	{ { 0, bits, offs } }
#  if defined(WORDS_BIGENDIAN)
#   define V(x, y, hlen)	{ { 1, hlen, (x << 8) | (y << 4) } }
#  else
#   define V(x, y, hlen)	{ { 1, hlen, (x << 0) | (y << 4) } }
#  endif
# endif

static
union huffpair const hufftab0[] = {
  /*      */ V(0, 0, 0)
};

static
union huffpair const hufftab1[] = {
  /* 000  */ V(1, 1, 3),
  /* 001  */ V(0, 1, 3),
  /* 010  */ V(1, 0, 2),
  /* 011  */ V(1, 0, 2),
  /* 100  */ V(0, 0, 1),
  /* 101  */ V(0, 0, 1),
  /* 110  */ V(0, 0, 1),
  /* 111  */ V(0, 0, 1)
};

static
union huffpair const hufftab2[] = {
  /* 000  */ PTR(8, 3),
  /* 001  */ V(1, 1, 3),
  /* 010  */ V(0, 1, 3),
  /* 011  */ V(1, 0, 3),
  /* 100  */ V(0, 0, 1),
  /* 101  */ V(0, 0, 1),
  /* 110  */ V(0, 0, 1),
  /* 111  */ V(0, 0, 1),

  /* 000 ... */
  /* 000  */ V(2, 2, 3),	/* 8 */
  /* 001  */ V(0, 2, 3),
  /* 010  */ V(1, 2, 2),
  /* 011  */ V(1, 2, 2),
  /* 100  */ V(2, 1, 2),
  /* 101  */ V(2, 1, 2),
  /* 110  */ V(2, 0, 2),
  /* 111  */ V(2, 0, 2)
};

static
union huffpair const hufftab3[] = {
  /* 000  */ PTR(8, 3),
  /* 001  */ V(1, 0, 3),
  /* 010  */ V(1, 1, 2),
  /* 011  */ V(1, 1, 2),
  /* 100  */ V(0, 1, 2),
  /* 101  */ V(0, 1, 2),
  /* 110  */ V(0, 0, 2),
  /* 111  */ V(0, 0, 2),

  /* 000 ... */
  /* 000  */ V(2, 2, 3),	/* 8 */
  /* 001  */ V(0, 2, 3),
  /* 010  */ V(1, 2, 2),
  /* 011  */ V(1, 2, 2),
  /* 100  */ V(2, 1, 2),
  /* 101  */ V(2, 1, 2),
  /* 110  */ V(2, 0, 2),
  /* 111  */ V(2, 0, 2)
};

static
union huffpair const hufftab5[] = {
  /* 000  */ PTR(8, 4),
  /* 001  */ V(1, 1, 3),
  /* 010  */ V(0, 1, 3),
  /* 011  */ V(1, 0, 3),
  /* 100  */ V(0, 0, 1),
  /* 101  */ V(0, 0, 1),
  /* 110  */ V(0, 0, 1),
  /* 111  */ V(0, 0, 1),

  /* 000 ... */
  /* 0000 */ PTR(24, 1),	/* 8 */
  /* 0001 */ V(3, 2, 4),
  /* 0010 */ V(3, 1, 3),
  /* 0011 */ V(3, 1, 3),
  /* 0100 */ V(1, 3, 4),
  /* 0101 */ V(0, 3, 4),
  /* 0110 */ V(3, 0, 4),
  /* 0111 */ V(2, 2, 4),
  /* 1000 */ V(1, 2, 3),
  /* 1001 */ V(1, 2, 3),
  /* 1010 */ V(2, 1, 3),
  /* 1011 */ V(2, 1, 3),
  /* 1100 */ V(0, 2, 3),
  /* 1101 */ V(0, 2, 3),
  /* 1110 */ V(2, 0, 3),
  /* 1111 */ V(2, 0, 3),

  /* 000 0000 ... */
  /* 0    */ V(3, 3, 1),	/* 24 */
  /* 1    */ V(2, 3, 1)
};

static
union huffpair const hufftab6[] = {
  /* 0000 */ PTR(16, 3),
  /* 0001 */ PTR(24, 1),
  /* 0010 */ PTR(26, 1),
  /* 0011 */ V(1, 2, 4),
  /* 0100 */ V(2, 1, 4),
  /* 0101 */ V(2, 0, 4),
  /* 0110 */ V(0, 1, 3),
  /* 0111 */ V(0, 1, 3),
  /* 1000 */ V(1, 1, 2),
  /* 1001 */ V(1, 1, 2),
  /* 1010 */ V(1, 1, 2),
  /* 1011 */ V(1, 1, 2),
  /* 1100 */ V(1, 0, 3),
  /* 1101 */ V(1, 0, 3),
  /* 1110 */ V(0, 0, 3),
  /* 1111 */ V(0, 0, 3),

  /* 0000 ... */
  /* 000  */ V(3, 3, 3),	/* 16 */
  /* 001  */ V(0, 3, 3),
  /* 010  */ V(2, 3, 2),
  /* 011  */ V(2, 3, 2),
  /* 100  */ V(3, 2, 2),
  /* 101  */ V(3, 2, 2),
  /* 110  */ V(3, 0, 2),
  /* 111  */ V(3, 0, 2),

  /* 0001 ... */
  /* 0    */ V(1, 3, 1),	/* 24 */
  /* 1    */ V(3, 1, 1),

  /* 0010 ... */
  /* 0    */ V(2, 2, 1),	/* 26 */
  /* 1    */ V(0, 2, 1)
};

static
union huffpair const hufftab7[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 4),
  /* 0010 */ PTR(48, 2),
  /* 0011 */ V(1, 1, 4),
  /* 0100 */ V(0, 1, 3),
  /* 0101 */ V(0, 1, 3),
  /* 0110 */ V(1, 0, 3),
  /* 0111 */ V(1, 0, 3),
  /* 1000 */ V(0, 0, 1),
  /* 1001 */ V(0, 0, 1),
  /* 1010 */ V(0, 0, 1),
  /* 1011 */ V(0, 0, 1),
  /* 1100 */ V(0, 0, 1),
  /* 1101 */ V(0, 0, 1),
  /* 1110 */ V(0, 0, 1),
  /* 1111 */ V(0, 0, 1),

  /* 0000 ... */
  /* 0000 */ PTR(52, 2),	/* 16 */
  /* 0001 */ PTR(56, 1),
  /* 0010 */ PTR(58, 1),
  /* 0011 */ V(1, 5, 4),
  /* 0100 */ V(5, 1, 4),
  /* 0101 */ PTR(60, 1),
  /* 0110 */ V(5, 0, 4),
  /* 0111 */ PTR(62, 1),
  /* 1000 */ V(2, 4, 4),
  /* 1001 */ V(4, 2, 4),
  /* 1010 */ V(1, 4, 3),
  /* 1011 */ V(1, 4, 3),
  /* 1100 */ V(4, 1, 3),
  /* 1101 */ V(4, 1, 3),
  /* 1110 */ V(4, 0, 3),
  /* 1111 */ V(4, 0, 3),

  /* 0001 ... */
  /* 0000 */ V(0, 4, 4),	/* 32 */
  /* 0001 */ V(2, 3, 4),
  /* 0010 */ V(3, 2, 4),
  /* 0011 */ V(0, 3, 4),
  /* 0100 */ V(1, 3, 3),
  /* 0101 */ V(1, 3, 3),
  /* 0110 */ V(3, 1, 3),
  /* 0111 */ V(3, 1, 3),
  /* 1000 */ V(3, 0, 3),
  /* 1001 */ V(3, 0, 3),
  /* 1010 */ V(2, 2, 3),
  /* 1011 */ V(2, 2, 3),
  /* 1100 */ V(1, 2, 2),
  /* 1101 */ V(1, 2, 2),
  /* 1110 */ V(1, 2, 2),
  /* 1111 */ V(1, 2, 2),

  /* 0010 ... */
  /* 00   */ V(2, 1, 1),	/* 48 */
  /* 01   */ V(2, 1, 1),
  /* 10   */ V(0, 2, 2),
  /* 11   */ V(2, 0, 2),

  /* 0000 0000 ... */
  /* 00   */ V(5, 5, 2),	/* 52 */
  /* 01   */ V(4, 5, 2),
  /* 10   */ V(5, 4, 2),
  /* 11   */ V(5, 3, 2),

  /* 0000 0001 ... */
  /* 0    */ V(3, 5, 1),	/* 56 */
  /* 1    */ V(4, 4, 1),

  /* 0000 0010 ... */
  /* 0    */ V(2, 5, 1),	/* 58 */
  /* 1    */ V(5, 2, 1),

  /* 0000 0101 ... */
  /* 0    */ V(0, 5, 1),	/* 60 */
  /* 1    */ V(3, 4, 1),

  /* 0000 0111 ... */
  /* 0    */ V(4, 3, 1),	/* 62 */
  /* 1    */ V(3, 3, 1)
};




static
union huffpair const hufftab8[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 4),
  /* 0010 */ V(1, 2, 4),
  /* 0011 */ V(2, 1, 4),
  /* 0100 */ V(1, 1, 2),
  /* 0101 */ V(1, 1, 2),
  /* 0110 */ V(1, 1, 2),
  /* 0111 */ V(1, 1, 2),
  /* 1000 */ V(0, 1, 3),
  /* 1001 */ V(0, 1, 3),
  /* 1010 */ V(1, 0, 3),
  /* 1011 */ V(1, 0, 3),
  /* 1100 */ V(0, 0, 2),
  /* 1101 */ V(0, 0, 2),
  /* 1110 */ V(0, 0, 2),
  /* 1111 */ V(0, 0, 2),

  /* 0000 ... */
  /* 0000 */ PTR(48, 3),	/* 16 */
  /* 0001 */ PTR(56, 2),
  /* 0010 */ PTR(60, 1),
  /* 0011 */ V(1, 5, 4),
  /* 0100 */ V(5, 1, 4),
  /* 0101 */ PTR(62, 1),
  /* 0110 */ PTR(64, 1),
  /* 0111 */ V(2, 4, 4),
  /* 1000 */ V(4, 2, 4),
  /* 1001 */ V(1, 4, 4),
  /* 1010 */ V(4, 1, 3),
  /* 1011 */ V(4, 1, 3),
  /* 1100 */ V(0, 4, 4),
  /* 1101 */ V(4, 0, 4),
  /* 1110 */ V(2, 3, 4),
  /* 1111 */ V(3, 2, 4),

  /* 0001 ... */
  /* 0000 */ V(1, 3, 4),	/* 32 */
  /* 0001 */ V(3, 1, 4),
  /* 0010 */ V(0, 3, 4),
  /* 0011 */ V(3, 0, 4),
  /* 0100 */ V(2, 2, 2),
  /* 0101 */ V(2, 2, 2),
  /* 0110 */ V(2, 2, 2),
  /* 0111 */ V(2, 2, 2),
  /* 1000 */ V(0, 2, 2),
  /* 1001 */ V(0, 2, 2),
  /* 1010 */ V(0, 2, 2),
  /* 1011 */ V(0, 2, 2),
  /* 1100 */ V(2, 0, 2),
  /* 1101 */ V(2, 0, 2),
  /* 1110 */ V(2, 0, 2),
  /* 1111 */ V(2, 0, 2),

  /* 0000 0000 ... */
  /* 000  */ V(5, 5, 3),	/* 48 */
  /* 001  */ V(5, 4, 3),
  /* 010  */ V(4, 5, 2),
  /* 011  */ V(4, 5, 2),
  /* 100  */ V(5, 3, 1),
  /* 101  */ V(5, 3, 1),
  /* 110  */ V(5, 3, 1),
  /* 111  */ V(5, 3, 1),

  /* 0000 0001 ... */
  /* 00   */ V(3, 5, 2),	/* 56 */
  /* 01   */ V(4, 4, 2),
  /* 10   */ V(2, 5, 1),
  /* 11   */ V(2, 5, 1),

  /* 0000 0010 ... */
  /* 0    */ V(5, 2, 1),	/* 60 */
  /* 1    */ V(0, 5, 1),

  /* 0000 0101 ... */
  /* 0    */ V(3, 4, 1),	/* 62 */
  /* 1    */ V(4, 3, 1),

  /* 0000 0110 ... */
  /* 0    */ V(5, 0, 1),	/* 64 */
  /* 1    */ V(3, 3, 1)
};


static
union huffpair const hufftab9[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 3),
  /* 0010 */ PTR(40, 2),
  /* 0011 */ PTR(44, 2),
  /* 0100 */ PTR(48, 1),
  /* 0101 */ V(1, 2, 4),
  /* 0110 */ V(2, 1, 4),
  /* 0111 */ V(2, 0, 4),
  /* 1000 */ V(1, 1, 3),
  /* 1001 */ V(1, 1, 3),
  /* 1010 */ V(0, 1, 3),
  /* 1011 */ V(0, 1, 3),
  /* 1100 */ V(1, 0, 3),
  /* 1101 */ V(1, 0, 3),
  /* 1110 */ V(0, 0, 3),
  /* 1111 */ V(0, 0, 3),

  /* 0000 ... */
  /* 0000 */ PTR(50, 1),	/* 16 */
  /* 0001 */ V(3, 5, 4),
  /* 0010 */ V(5, 3, 4),
  /* 0011 */ PTR(52, 1),
  /* 0100 */ V(4, 4, 4),
  /* 0101 */ V(2, 5, 4),
  /* 0110 */ V(5, 2, 4),
  /* 0111 */ V(1, 5, 4),
  /* 1000 */ V(5, 1, 3),
  /* 1001 */ V(5, 1, 3),
  /* 1010 */ V(3, 4, 3),
  /* 1011 */ V(3, 4, 3),
  /* 1100 */ V(4, 3, 3),
  /* 1101 */ V(4, 3, 3),
  /* 1110 */ V(5, 0, 4),
  /* 1111 */ V(0, 4, 4),

  /* 0001 ... */
  /* 000  */ V(2, 4, 3),	/* 32 */
  /* 001  */ V(4, 2, 3),
  /* 010  */ V(3, 3, 3),
  /* 011  */ V(4, 0, 3),
  /* 100  */ V(1, 4, 2),
  /* 101  */ V(1, 4, 2),
  /* 110  */ V(4, 1, 2),
  /* 111  */ V(4, 1, 2),

  /* 0010 ... */
  /* 00   */ V(2, 3, 2),	/* 40 */
  /* 01   */ V(3, 2, 2),
  /* 10   */ V(1, 3, 1),
  /* 11   */ V(1, 3, 1),

  /* 0011 ... */
  /* 00   */ V(3, 1, 1),	/* 44 */
  /* 01   */ V(3, 1, 1),
  /* 10   */ V(0, 3, 2),
  /* 11   */ V(3, 0, 2),

  /* 0100 ... */
  /* 0    */ V(2, 2, 1),	/* 48 */
  /* 1    */ V(0, 2, 1),

  /* 0000 0000 ... */
  /* 0    */ V(5, 5, 1),	/* 50 */
  /* 1    */ V(4, 5, 1),

  /* 0000 0011 ... */
  /* 0    */ V(5, 4, 1),	/* 52 */
  /* 1    */ V(0, 5, 1)
};

static
union huffpair const hufftab10[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 4),
  /* 0010 */ PTR(48, 2),
  /* 0011 */ V(1, 1, 4),
  /* 0100 */ V(0, 1, 3),
  /* 0101 */ V(0, 1, 3),
  /* 0110 */ V(1, 0, 3),
  /* 0111 */ V(1, 0, 3),
  /* 1000 */ V(0, 0, 1),
  /* 1001 */ V(0, 0, 1),
  /* 1010 */ V(0, 0, 1),
  /* 1011 */ V(0, 0, 1),
  /* 1100 */ V(0, 0, 1),
  /* 1101 */ V(0, 0, 1),
  /* 1110 */ V(0, 0, 1),
  /* 1111 */ V(0, 0, 1),

  /* 0000 ... */
  /* 0000 */ PTR(52, 3),	/* 16 */
  /* 0001 */ PTR(60, 2),
  /* 0010 */ PTR(64, 3),
  /* 0011 */ PTR(72, 1),
  /* 0100 */ PTR(74, 2),
  /* 0101 */ PTR(78, 2),
  /* 0110 */ PTR(82, 2),
  /* 0111 */ V(1, 7, 4),
  /* 1000 */ V(7, 1, 4),
  /* 1001 */ PTR(86, 1),
  /* 1010 */ PTR(88, 2),
  /* 1011 */ PTR(92, 2),
  /* 1100 */ V(1, 6, 4),
  /* 1101 */ V(6, 1, 4),
  /* 1110 */ V(6, 0, 4),
  /* 1111 */ PTR(96, 1),

  /* 0001 ... */
  /* 0000 */ PTR(98, 1),	/* 32 */
  /* 0001 */ PTR(100, 1),
  /* 0010 */ V(1, 4, 4),
  /* 0011 */ V(4, 1, 4),
  /* 0100 */ V(4, 0, 4),
  /* 0101 */ V(2, 3, 4),
  /* 0110 */ V(3, 2, 4),
  /* 0111 */ V(0, 3, 4),
  /* 1000 */ V(1, 3, 3),
  /* 1001 */ V(1, 3, 3),
  /* 1010 */ V(3, 1, 3),
  /* 1011 */ V(3, 1, 3),
  /* 1100 */ V(3, 0, 3),
  /* 1101 */ V(3, 0, 3),
  /* 1110 */ V(2, 2, 3),
  /* 1111 */ V(2, 2, 3),

  /* 0010 ... */
  /* 00   */ V(1, 2, 2),	/* 48 */
  /* 01   */ V(2, 1, 2),
  /* 10   */ V(0, 2, 2),
  /* 11   */ V(2, 0, 2),

  /* 0000 0000 ... */
  /* 000  */ V(7, 7, 3),	/* 52 */
  /* 001  */ V(6, 7, 3),
  /* 010  */ V(7, 6, 3),
  /* 011  */ V(5, 7, 3),
  /* 100  */ V(7, 5, 3),
  /* 101  */ V(6, 6, 3),
  /* 110  */ V(4, 7, 2),
  /* 111  */ V(4, 7, 2),

  /* 0000 0001 ... */
  /* 00   */ V(7, 4, 2),	/* 60 */
  /* 01   */ V(5, 6, 2),
  /* 10   */ V(6, 5, 2),
  /* 11   */ V(3, 7, 2),

  /* 0000 0010 ... */
  /* 000  */ V(7, 3, 2),	/* 64 */
  /* 001  */ V(7, 3, 2),
  /* 010  */ V(4, 6, 2),
  /* 011  */ V(4, 6, 2),
  /* 100  */ V(5, 5, 3),
  /* 101  */ V(5, 4, 3),
  /* 110  */ V(6, 3, 2),
  /* 111  */ V(6, 3, 2),

  /* 0000 0011 ... */
  /* 0    */ V(2, 7, 1),	/* 72 */
  /* 1    */ V(7, 2, 1),

  /* 0000 0100 ... */
  /* 00   */ V(6, 4, 2),	/* 74 */
  /* 01   */ V(0, 7, 2),
  /* 10   */ V(7, 0, 1),
  /* 11   */ V(7, 0, 1),

  /* 0000 0101 ... */
  /* 00   */ V(6, 2, 1),	/* 78 */
  /* 01   */ V(6, 2, 1),
  /* 10   */ V(4, 5, 2),
  /* 11   */ V(3, 5, 2),

  /* 0000 0110 ... */
  /* 00   */ V(0, 6, 1),	/* 82 */
  /* 01   */ V(0, 6, 1),
  /* 10   */ V(5, 3, 2),
  /* 11   */ V(4, 4, 2),

  /* 0000 1001 ... */
  /* 0    */ V(3, 6, 1),	/* 86 */
  /* 1    */ V(2, 6, 1),

  /* 0000 1010 ... */
  /* 00   */ V(2, 5, 2),	/* 88 */
  /* 01   */ V(5, 2, 2),
  /* 10   */ V(1, 5, 1),
  /* 11   */ V(1, 5, 1),

  /* 0000 1011 ... */
  /* 00   */ V(5, 1, 1),	/* 92 */
  /* 01   */ V(5, 1, 1),
  /* 10   */ V(3, 4, 2),
  /* 11   */ V(4, 3, 2),

  /* 0000 1111 ... */
  /* 0    */ V(0, 5, 1),	/* 96 */
  /* 1    */ V(5, 0, 1),

  /* 0001 0000 ... */
  /* 0    */ V(2, 4, 1),	/* 98 */
  /* 1    */ V(4, 2, 1),

  /* 0001 0001 ... */
  /* 0    */ V(3, 3, 1),	/* 100 */
  /* 1    */ V(0, 4, 1)
};

static
union huffpair const hufftab11[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 4),
  /* 0010 */ PTR(48, 4),
  /* 0011 */ PTR(64, 3),
  /* 0100 */ V(1, 2, 4),
  /* 0101 */ PTR(72, 1),
  /* 0110 */ V(1, 1, 3),
  /* 0111 */ V(1, 1, 3),
  /* 1000 */ V(0, 1, 3),
  /* 1001 */ V(0, 1, 3),
  /* 1010 */ V(1, 0, 3),
  /* 1011 */ V(1, 0, 3),
  /* 1100 */ V(0, 0, 2),
  /* 1101 */ V(0, 0, 2),
  /* 1110 */ V(0, 0, 2),
  /* 1111 */ V(0, 0, 2),

  /* 0000 ... */
  /* 0000 */ PTR(74, 2),	/* 16 */
  /* 0001 */ PTR(78, 3),
  /* 0010 */ PTR(86, 2),
  /* 0011 */ PTR(90, 1),
  /* 0100 */ PTR(92, 2),
  /* 0101 */ V(2, 7, 4),
  /* 0110 */ V(7, 2, 4),
  /* 0111 */ PTR(96, 1),
  /* 1000 */ V(7, 1, 3),
  /* 1001 */ V(7, 1, 3),
  /* 1010 */ V(1, 7, 4),
  /* 1011 */ V(7, 0, 4),
  /* 1100 */ V(3, 6, 4),
  /* 1101 */ V(6, 3, 4),
  /* 1110 */ V(6, 0, 4),
  /* 1111 */ PTR(98, 1),

  /* 0001 ... */
  /* 0000 */ PTR(100, 1),	/* 32 */
  /* 0001 */ V(1, 5, 4),
  /* 0010 */ V(6, 2, 3),
  /* 0011 */ V(6, 2, 3),
  /* 0100 */ V(2, 6, 4),
  /* 0101 */ V(0, 6, 4),
  /* 0110 */ V(1, 6, 3),
  /* 0111 */ V(1, 6, 3),
  /* 1000 */ V(6, 1, 3),
  /* 1001 */ V(6, 1, 3),
  /* 1010 */ V(5, 1, 4),
  /* 1011 */ V(3, 4, 4),
  /* 1100 */ V(5, 0, 4),
  /* 1101 */ PTR(102, 1),
  /* 1110 */ V(2, 4, 4),
  /* 1111 */ V(4, 2, 4),

  /* 0010 ... */
  /* 0000 */ V(1, 4, 4),	/* 48 */
  /* 0001 */ V(4, 1, 4),
  /* 0010 */ V(0, 4, 4),
  /* 0011 */ V(4, 0, 4),
  /* 0100 */ V(2, 3, 3),
  /* 0101 */ V(2, 3, 3),
  /* 0110 */ V(3, 2, 3),
  /* 0111 */ V(3, 2, 3),
  /* 1000 */ V(1, 3, 2),
  /* 1001 */ V(1, 3, 2),
  /* 1010 */ V(1, 3, 2),
  /* 1011 */ V(1, 3, 2),
  /* 1100 */ V(3, 1, 2),
  /* 1101 */ V(3, 1, 2),
  /* 1110 */ V(3, 1, 2),
  /* 1111 */ V(3, 1, 2),

  /* 0011 ... */
  /* 000  */ V(0, 3, 3),	/* 64 */
  /* 001  */ V(3, 0, 3),
  /* 010  */ V(2, 2, 2),
  /* 011  */ V(2, 2, 2),
  /* 100  */ V(2, 1, 1),
  /* 101  */ V(2, 1, 1),
  /* 110  */ V(2, 1, 1),
  /* 111  */ V(2, 1, 1),

  /* 0101 ... */
  /* 0    */ V(0, 2, 1),	/* 72 */
  /* 1    */ V(2, 0, 1),

  /* 0000 0000 ... */
  /* 00   */ V(7, 7, 2),	/* 74 */
  /* 01   */ V(6, 7, 2),
  /* 10   */ V(7, 6, 2),
  /* 11   */ V(7, 5, 2),

  /* 0000 0001 ... */
  /* 000  */ V(6, 6, 2),	/* 78 */
  /* 001  */ V(6, 6, 2),
  /* 010  */ V(4, 7, 2),
  /* 011  */ V(4, 7, 2),
  /* 100  */ V(7, 4, 2),
  /* 101  */ V(7, 4, 2),
  /* 110  */ V(5, 7, 3),
  /* 111  */ V(5, 5, 3),

  /* 0000 0010 ... */
  /* 00   */ V(5, 6, 2),	/* 86 */
  /* 01   */ V(6, 5, 2),
  /* 10   */ V(3, 7, 1),
  /* 11   */ V(3, 7, 1),

  /* 0000 0011 ... */
  /* 0    */ V(7, 3, 1),	/* 90 */
  /* 1    */ V(4, 6, 1),

  /* 0000 0100 ... */
  /* 00   */ V(4, 5, 2),	/* 92 */
  /* 01   */ V(5, 4, 2),
  /* 10   */ V(3, 5, 2),
  /* 11   */ V(5, 3, 2),

  /* 0000 0111 ... */
  /* 0    */ V(6, 4, 1),	/* 96 */
  /* 1    */ V(0, 7, 1),

  /* 0000 1111 ... */
  /* 0    */ V(4, 4, 1),	/* 98 */
  /* 1    */ V(2, 5, 1),

  /* 0001 0000 ... */
  /* 0    */ V(5, 2, 1),	/* 100 */
  /* 1    */ V(0, 5, 1),

  /* 0001 1101 ... */
  /* 0    */ V(4, 3, 1),	/* 102 */
  /* 1    */ V(3, 3, 1)
};

static
union huffpair const hufftab12[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 4),
  /* 0010 */ PTR(48, 4),
  /* 0011 */ PTR(64, 2),
  /* 0100 */ PTR(68, 3),
  /* 0101 */ PTR(76, 1),
  /* 0110 */ V(1, 2, 4),
  /* 0111 */ V(2, 1, 4),
  /* 1000 */ PTR(78, 1),
  /* 1001 */ V(0, 0, 4),
  /* 1010 */ V(1, 1, 3),
  /* 1011 */ V(1, 1, 3),
  /* 1100 */ V(0, 1, 3),
  /* 1101 */ V(0, 1, 3),
  /* 1110 */ V(1, 0, 3),
  /* 1111 */ V(1, 0, 3),

  /* 0000 ... */
  /* 0000 */ PTR(80, 2),	/* 16 */
  /* 0001 */ PTR(84, 1),
  /* 0010 */ PTR(86, 1),
  /* 0011 */ PTR(88, 1),
  /* 0100 */ V(5, 6, 4),
  /* 0101 */ V(3, 7, 4),
  /* 0110 */ PTR(90, 1),
  /* 0111 */ V(2, 7, 4),
  /* 1000 */ V(7, 2, 4),
  /* 1001 */ V(4, 6, 4),
  /* 1010 */ V(6, 4, 4),
  /* 1011 */ V(1, 7, 4),
  /* 1100 */ V(7, 1, 4),
  /* 1101 */ PTR(92, 1),
  /* 1110 */ V(3, 6, 4),
  /* 1111 */ V(6, 3, 4),

  /* 0001 ... */
  /* 0000 */ V(4, 5, 4),	/* 32 */
  /* 0001 */ V(5, 4, 4),
  /* 0010 */ V(4, 4, 4),
  /* 0011 */ PTR(94, 1),
  /* 0100 */ V(2, 6, 3),
  /* 0101 */ V(2, 6, 3),
  /* 0110 */ V(6, 2, 3),
  /* 0111 */ V(6, 2, 3),
  /* 1000 */ V(6, 1, 3),
  /* 1001 */ V(6, 1, 3),
  /* 1010 */ V(1, 6, 4),
  /* 1011 */ V(6, 0, 4),
  /* 1100 */ V(3, 5, 4),
  /* 1101 */ V(5, 3, 4),
  /* 1110 */ V(2, 5, 4),
  /* 1111 */ V(5, 2, 4),

  /* 0010 ... */
  /* 0000 */ V(1, 5, 3),	/* 48 */
  /* 0001 */ V(1, 5, 3),
  /* 0010 */ V(5, 1, 3),
  /* 0011 */ V(5, 1, 3),
  /* 0100 */ V(3, 4, 3),
  /* 0101 */ V(3, 4, 3),
  /* 0110 */ V(4, 3, 3),
  /* 0111 */ V(4, 3, 3),
  /* 1000 */ V(5, 0, 4),
  /* 1001 */ V(0, 4, 4),
  /* 1010 */ V(2, 4, 3),
  /* 1011 */ V(2, 4, 3),
  /* 1100 */ V(4, 2, 3),
  /* 1101 */ V(4, 2, 3),
  /* 1110 */ V(1, 4, 3),
  /* 1111 */ V(1, 4, 3),

  /* 0011 ... */
  /* 00   */ V(3, 3, 2),	/* 64 */
  /* 01   */ V(4, 1, 2),
  /* 10   */ V(2, 3, 2),
  /* 11   */ V(3, 2, 2),

  /* 0100 ... */
  /* 000  */ V(4, 0, 3),	/* 68 */
  /* 001  */ V(0, 3, 3),
  /* 010  */ V(3, 0, 2),
  /* 011  */ V(3, 0, 2),
  /* 100  */ V(1, 3, 1),
  /* 101  */ V(1, 3, 1),
  /* 110  */ V(1, 3, 1),
  /* 111  */ V(1, 3, 1),

  /* 0101 ... */
  /* 0    */ V(3, 1, 1),	/* 76 */
  /* 1    */ V(2, 2, 1),

  /* 1000 ... */
  /* 0    */ V(0, 2, 1),	/* 78 */
  /* 1    */ V(2, 0, 1),

  /* 0000 0000 ... */
  /* 00   */ V(7, 7, 2),	/* 80 */
  /* 01   */ V(6, 7, 2),
  /* 10   */ V(7, 6, 1),
  /* 11   */ V(7, 6, 1),

  /* 0000 0001 ... */
  /* 0    */ V(5, 7, 1),	/* 84 */
  /* 1    */ V(7, 5, 1),

  /* 0000 0010 ... */
  /* 0    */ V(6, 6, 1),	/* 86 */
  /* 1    */ V(4, 7, 1),

  /* 0000 0011 ... */
  /* 0    */ V(7, 4, 1),	/* 88 */
  /* 1    */ V(6, 5, 1),

  /* 0000 0110 ... */
  /* 0    */ V(7, 3, 1),	/* 90 */
  /* 1    */ V(5, 5, 1),

  /* 0000 1101 ... */
  /* 0    */ V(0, 7, 1),	/* 92 */
  /* 1    */ V(7, 0, 1),

  /* 0001 0011 ... */
  /* 0    */ V(0, 6, 1),	/* 94 */
  /* 1    */ V(0, 5, 1)
};

static
union huffpair const hufftab13[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 4),
  /* 0010 */ PTR(48, 4),
  /* 0011 */ PTR(64, 2),
  /* 0100 */ V(1, 1, 4),
  /* 0101 */ V(0, 1, 4),
  /* 0110 */ V(1, 0, 3),
  /* 0111 */ V(1, 0, 3),
  /* 1000 */ V(0, 0, 1),
  /* 1001 */ V(0, 0, 1),
  /* 1010 */ V(0, 0, 1),
  /* 1011 */ V(0, 0, 1),
  /* 1100 */ V(0, 0, 1),
  /* 1101 */ V(0, 0, 1),
  /* 1110 */ V(0, 0, 1),
  /* 1111 */ V(0, 0, 1),

  /* 0000 ... */
  /* 0000 */ PTR(68, 4),	/* 16 */
  /* 0001 */ PTR(84, 4),
  /* 0010 */ PTR(100, 4),
  /* 0011 */ PTR(116, 4),
  /* 0100 */ PTR(132, 4),
  /* 0101 */ PTR(148, 4),
  /* 0110 */ PTR(164, 3),
  /* 0111 */ PTR(172, 3),
  /* 1000 */ PTR(180, 3),
  /* 1001 */ PTR(188, 3),
  /* 1010 */ PTR(196, 3),
  /* 1011 */ PTR(204, 3),
  /* 1100 */ PTR(212, 1),
  /* 1101 */ PTR(214, 2),
  /* 1110 */ PTR(218, 3),
  /* 1111 */ PTR(226, 1),

  /* 0001 ... */
  /* 0000 */ PTR(228, 2),	/* 32 */
  /* 0001 */ PTR(232, 2),
  /* 0010 */ PTR(236, 2),
  /* 0011 */ PTR(240, 2),
  /* 0100 */ V(8, 1, 4),
  /* 0101 */ PTR(244, 1),
  /* 0110 */ PTR(246, 1),
  /* 0111 */ PTR(248, 1),
  /* 1000 */ PTR(250, 2),
  /* 1001 */ PTR(254, 1),
  /* 1010 */ V(1, 5, 4),
  /* 1011 */ V(5, 1, 4),
  /* 1100 */ PTR(256, 1),
  /* 1101 */ PTR(258, 1),
  /* 1110 */ PTR(260, 1),
  /* 1111 */ V(1, 4, 4),

  /* 0010 ... */
  /* 0000 */ V(4, 1, 3),	/* 48 */
  /* 0001 */ V(4, 1, 3),
  /* 0010 */ V(0, 4, 4),
  /* 0011 */ V(4, 0, 4),
  /* 0100 */ V(2, 3, 4),
  /* 0101 */ V(3, 2, 4),
  /* 0110 */ V(1, 3, 3),
  /* 0111 */ V(1, 3, 3),
  /* 1000 */ V(3, 1, 3),
  /* 1001 */ V(3, 1, 3),
  /* 1010 */ V(0, 3, 3),
  /* 1011 */ V(0, 3, 3),
  /* 1100 */ V(3, 0, 3),
  /* 1101 */ V(3, 0, 3),
  /* 1110 */ V(2, 2, 3),
  /* 1111 */ V(2, 2, 3),

  /* 0011 ... */
  /* 00   */ V(1, 2, 2),	/* 64 */
  /* 01   */ V(2, 1, 2),
  /* 10   */ V(0, 2, 2),
  /* 11   */ V(2, 0, 2),

  /* 0000 0000 ... */
  /* 0000 */ PTR(262, 4),	/* 68 */
  /* 0001 */ PTR(278, 4),
  /* 0010 */ PTR(294, 4),
  /* 0011 */ PTR(310, 3),
  /* 0100 */ PTR(318, 2),
  /* 0101 */ PTR(322, 2),
  /* 0110 */ PTR(326, 3),
  /* 0111 */ PTR(334, 2),
  /* 1000 */ PTR(338, 1),
  /* 1001 */ PTR(340, 2),
  /* 1010 */ PTR(344, 2),
  /* 1011 */ PTR(348, 2),
  /* 1100 */ PTR(352, 2),
  /* 1101 */ PTR(356, 2),
  /* 1110 */ V(1, 15, 4),
  /* 1111 */ V(15, 1, 4),

  /* 0000 0001 ... */
  /* 0000 */ V(15, 0, 4),	/* 84 */
  /* 0001 */ PTR(360, 1),
  /* 0010 */ PTR(362, 1),
  /* 0011 */ PTR(364, 1),
  /* 0100 */ V(14, 2, 4),
  /* 0101 */ PTR(366, 1),
  /* 0110 */ V(1, 14, 4),
  /* 0111 */ V(14, 1, 4),
  /* 1000 */ PTR(368, 1),
  /* 1001 */ PTR(370, 1),
  /* 1010 */ PTR(372, 1),
  /* 1011 */ PTR(374, 1),
  /* 1100 */ PTR(376, 1),
  /* 1101 */ PTR(378, 1),
  /* 1110 */ V(12, 6, 4),
  /* 1111 */ V(3, 13, 4),

  /* 0000 0010 ... */
  /* 0000 */ PTR(380, 1),	/* 100 */
  /* 0001 */ V(2, 13, 4),
  /* 0010 */ V(13, 2, 4),
  /* 0011 */ V(1, 13, 4),
  /* 0100 */ V(11, 7, 4),
  /* 0101 */ PTR(382, 1),
  /* 0110 */ PTR(384, 1),
  /* 0111 */ V(12, 3, 4),
  /* 1000 */ PTR(386, 1),
  /* 1001 */ V(4, 11, 4),
  /* 1010 */ V(13, 1, 3),
  /* 1011 */ V(13, 1, 3),
  /* 1100 */ V(0, 13, 4),
  /* 1101 */ V(13, 0, 4),
  /* 1110 */ V(8, 10, 4),
  /* 1111 */ V(10, 8, 4),

  /* 0000 0011 ... */
  /* 0000 */ V(4, 12, 4),	/* 116 */
  /* 0001 */ V(12, 4, 4),
  /* 0010 */ V(6, 11, 4),
  /* 0011 */ V(11, 6, 4),
  /* 0100 */ V(3, 12, 3),
  /* 0101 */ V(3, 12, 3),
  /* 0110 */ V(2, 12, 3),
  /* 0111 */ V(2, 12, 3),
  /* 1000 */ V(12, 2, 3),
  /* 1001 */ V(12, 2, 3),
  /* 1010 */ V(5, 11, 3),
  /* 1011 */ V(5, 11, 3),
  /* 1100 */ V(11, 5, 4),
  /* 1101 */ V(8, 9, 4),
  /* 1110 */ V(1, 12, 3),
  /* 1111 */ V(1, 12, 3),

  /* 0000 0100 ... */
  /* 0000 */ V(12, 1, 3),	/* 132 */
  /* 0001 */ V(12, 1, 3),
  /* 0010 */ V(9, 8, 4),
  /* 0011 */ V(0, 12, 4),
  /* 0100 */ V(12, 0, 3),
  /* 0101 */ V(12, 0, 3),
  /* 0110 */ V(11, 4, 4),
  /* 0111 */ V(6, 10, 4),
  /* 1000 */ V(10, 6, 4),
  /* 1001 */ V(7, 9, 4),
  /* 1010 */ V(3, 11, 3),
  /* 1011 */ V(3, 11, 3),
  /* 1100 */ V(11, 3, 3),
  /* 1101 */ V(11, 3, 3),
  /* 1110 */ V(8, 8, 4),
  /* 1111 */ V(5, 10, 4),

  /* 0000 0101 ... */
  /* 0000 */ V(2, 11, 3),	/* 148 */
  /* 0001 */ V(2, 11, 3),
  /* 0010 */ V(10, 5, 4),
  /* 0011 */ V(6, 9, 4),
  /* 0100 */ V(10, 4, 3),
  /* 0101 */ V(10, 4, 3),
  /* 0110 */ V(7, 8, 4),
  /* 0111 */ V(8, 7, 4),
  /* 1000 */ V(9, 4, 3),
  /* 1001 */ V(9, 4, 3),
  /* 1010 */ V(7, 7, 4),
  /* 1011 */ V(7, 6, 4),
  /* 1100 */ V(11, 2, 2),
  /* 1101 */ V(11, 2, 2),
  /* 1110 */ V(11, 2, 2),
  /* 1111 */ V(11, 2, 2),

  /* 0000 0110 ... */
  /* 000  */ V(1, 11, 2),	/* 164 */
  /* 001  */ V(1, 11, 2),
  /* 010  */ V(11, 1, 2),
  /* 011  */ V(11, 1, 2),
  /* 100  */ V(0, 11, 3),
  /* 101  */ V(11, 0, 3),
  /* 110  */ V(9, 6, 3),
  /* 111  */ V(4, 10, 3),

  /* 0000 0111 ... */
  /* 000  */ V(3, 10, 3),	/* 172 */
  /* 001  */ V(10, 3, 3),
  /* 010  */ V(5, 9, 3),
  /* 011  */ V(9, 5, 3),
  /* 100  */ V(2, 10, 2),
  /* 101  */ V(2, 10, 2),
  /* 110  */ V(10, 2, 2),
  /* 111  */ V(10, 2, 2),

  /* 0000 1000 ... */
  /* 000  */ V(1, 10, 2),	/* 180 */
  /* 001  */ V(1, 10, 2),
  /* 010  */ V(10, 1, 2),
  /* 011  */ V(10, 1, 2),
  /* 100  */ V(0, 10, 3),
  /* 101  */ V(6, 8, 3),
  /* 110  */ V(10, 0, 2),
  /* 111  */ V(10, 0, 2),

  /* 0000 1001 ... */
  /* 000  */ V(8, 6, 3),	/* 188 */
  /* 001  */ V(4, 9, 3),
  /* 010  */ V(9, 3, 2),
  /* 011  */ V(9, 3, 2),
  /* 100  */ V(3, 9, 3),
  /* 101  */ V(5, 8, 3),
  /* 110  */ V(8, 5, 3),
  /* 111  */ V(6, 7, 3),

  /* 0000 1010 ... */
  /* 000  */ V(2, 9, 2),	/* 196 */
  /* 001  */ V(2, 9, 2),
  /* 010  */ V(9, 2, 2),
  /* 011  */ V(9, 2, 2),
  /* 100  */ V(5, 7, 3),
  /* 101  */ V(7, 5, 3),
  /* 110  */ V(3, 8, 2),
  /* 111  */ V(3, 8, 2),

  /* 0000 1011 ... */
  /* 000  */ V(8, 3, 2),	/* 204 */
  /* 001  */ V(8, 3, 2),
  /* 010  */ V(6, 6, 3),
  /* 011  */ V(4, 7, 3),
  /* 100  */ V(7, 4, 3),
  /* 101  */ V(5, 6, 3),
  /* 110  */ V(6, 5, 3),
  /* 111  */ V(7, 3, 3),

  /* 0000 1100 ... */
  /* 0    */ V(1, 9, 1),	/* 212 */
  /* 1    */ V(9, 1, 1),

  /* 0000 1101 ... */
  /* 00   */ V(0, 9, 2),	/* 214 */
  /* 01   */ V(9, 0, 2),
  /* 10   */ V(4, 8, 2),
  /* 11   */ V(8, 4, 2),

  /* 0000 1110 ... */
  /* 000  */ V(7, 2, 2),	/* 218 */
  /* 001  */ V(7, 2, 2),
  /* 010  */ V(4, 6, 3),
  /* 011  */ V(6, 4, 3),
  /* 100  */ V(2, 8, 1),
  /* 101  */ V(2, 8, 1),
  /* 110  */ V(2, 8, 1),
  /* 111  */ V(2, 8, 1),

  /* 0000 1111 ... */
  /* 0    */ V(8, 2, 1),	/* 226 */
  /* 1    */ V(1, 8, 1),

  /* 0001 0000 ... */
  /* 00   */ V(3, 7, 2),	/* 228 */
  /* 01   */ V(2, 7, 2),
  /* 10   */ V(1, 7, 1),
  /* 11   */ V(1, 7, 1),

  /* 0001 0001 ... */
  /* 00   */ V(7, 1, 1),	/* 232 */
  /* 01   */ V(7, 1, 1),
  /* 10   */ V(5, 5, 2),
  /* 11   */ V(0, 7, 2),

  /* 0001 0010 ... */
  /* 00   */ V(7, 0, 2),	/* 236 */
  /* 01   */ V(3, 6, 2),
  /* 10   */ V(6, 3, 2),
  /* 11   */ V(4, 5, 2),

  /* 0001 0011 ... */
  /* 00   */ V(5, 4, 2),	/* 240 */
  /* 01   */ V(2, 6, 2),
  /* 10   */ V(6, 2, 2),
  /* 11   */ V(3, 5, 2),

  /* 0001 0101 ... */
  /* 0    */ V(0, 8, 1),	/* 244 */
  /* 1    */ V(8, 0, 1),

  /* 0001 0110 ... */
  /* 0    */ V(1, 6, 1),	/* 246 */
  /* 1    */ V(6, 1, 1),

  /* 0001 0111 ... */
  /* 0    */ V(0, 6, 1),	/* 248 */
  /* 1    */ V(6, 0, 1),

  /* 0001 1000 ... */
  /* 00   */ V(5, 3, 2),	/* 250 */
  /* 01   */ V(4, 4, 2),
  /* 10   */ V(2, 5, 1),
  /* 11   */ V(2, 5, 1),

  /* 0001 1001 ... */
  /* 0    */ V(5, 2, 1),	/* 254 */
  /* 1    */ V(0, 5, 1),

  /* 0001 1100 ... */
  /* 0    */ V(3, 4, 1),	/* 256 */
  /* 1    */ V(4, 3, 1),

  /* 0001 1101 ... */
  /* 0    */ V(5, 0, 1),	/* 258 */
  /* 1    */ V(2, 4, 1),

  /* 0001 1110 ... */
  /* 0    */ V(4, 2, 1),	/* 260 */
  /* 1    */ V(3, 3, 1),

  /* 0000 0000 0000 ... */
  /* 0000 */ PTR(388, 3),	/* 262 */
  /* 0001 */ V(15, 15, 4),
  /* 0010 */ V(14, 15, 4),
  /* 0011 */ V(13, 15, 4),
  /* 0100 */ V(14, 14, 4),
  /* 0101 */ V(12, 15, 4),
  /* 0110 */ V(13, 14, 4),
  /* 0111 */ V(11, 15, 4),
  /* 1000 */ V(15, 11, 4),
  /* 1001 */ V(12, 14, 4),
  /* 1010 */ V(13, 12, 4),
  /* 1011 */ PTR(396, 1),
  /* 1100 */ V(14, 12, 3),
  /* 1101 */ V(14, 12, 3),
  /* 1110 */ V(13, 13, 3),
  /* 1111 */ V(13, 13, 3),

  /* 0000 0000 0001 ... */
  /* 0000 */ V(15, 10, 4),	/* 278 */
  /* 0001 */ V(12, 13, 4),
  /* 0010 */ V(11, 14, 3),
  /* 0011 */ V(11, 14, 3),
  /* 0100 */ V(14, 11, 3),
  /* 0101 */ V(14, 11, 3),
  /* 0110 */ V(9, 15, 3),
  /* 0111 */ V(9, 15, 3),
  /* 1000 */ V(15, 9, 3),
  /* 1001 */ V(15, 9, 3),
  /* 1010 */ V(14, 10, 3),
  /* 1011 */ V(14, 10, 3),
  /* 1100 */ V(11, 13, 3),
  /* 1101 */ V(11, 13, 3),
  /* 1110 */ V(13, 11, 3),
  /* 1111 */ V(13, 11, 3),

  /* 0000 0000 0010 ... */
  /* 0000 */ V(8, 15, 3),	/* 294 */
  /* 0001 */ V(8, 15, 3),
  /* 0010 */ V(15, 8, 3),
  /* 0011 */ V(15, 8, 3),
  /* 0100 */ V(12, 12, 3),
  /* 0101 */ V(12, 12, 3),
  /* 0110 */ V(10, 14, 4),
  /* 0111 */ V(9, 14, 4),
  /* 1000 */ V(8, 14, 3),
  /* 1001 */ V(8, 14, 3),
  /* 1010 */ V(7, 15, 4),
  /* 1011 */ V(7, 14, 4),
  /* 1100 */ V(15, 7, 2),
  /* 1101 */ V(15, 7, 2),
  /* 1110 */ V(15, 7, 2),
  /* 1111 */ V(15, 7, 2),

  /* 0000 0000 0011 ... */
  /* 000  */ V(13, 10, 2),	/* 310 */
  /* 001  */ V(13, 10, 2),
  /* 010  */ V(10, 13, 3),
  /* 011  */ V(11, 12, 3),
  /* 100  */ V(12, 11, 3),
  /* 101  */ V(15, 6, 3),
  /* 110  */ V(6, 15, 2),
  /* 111  */ V(6, 15, 2),

  /* 0000 0000 0100 ... */
  /* 00   */ V(14, 8, 2),	/* 318 */
  /* 01   */ V(5, 15, 2),
  /* 10   */ V(9, 13, 2),
  /* 11   */ V(13, 9, 2),

  /* 0000 0000 0101 ... */
  /* 00   */ V(15, 5, 2),	/* 322 */
  /* 01   */ V(14, 7, 2),
  /* 10   */ V(10, 12, 2),
  /* 11   */ V(11, 11, 2),

  /* 0000 0000 0110 ... */
  /* 000  */ V(4, 15, 2),	/* 326 */
  /* 001  */ V(4, 15, 2),
  /* 010  */ V(15, 4, 2),
  /* 011  */ V(15, 4, 2),
  /* 100  */ V(12, 10, 3),
  /* 101  */ V(14, 6, 3),
  /* 110  */ V(15, 3, 2),
  /* 111  */ V(15, 3, 2),

  /* 0000 0000 0111 ... */
  /* 00   */ V(3, 15, 1),	/* 334 */
  /* 01   */ V(3, 15, 1),
  /* 10   */ V(8, 13, 2),
  /* 11   */ V(13, 8, 2),

  /* 0000 0000 1000 ... */
  /* 0    */ V(2, 15, 1),	/* 338 */
  /* 1    */ V(15, 2, 1),

  /* 0000 0000 1001 ... */
  /* 00   */ V(6, 14, 2),	/* 340 */
  /* 01   */ V(9, 12, 2),
  /* 10   */ V(0, 15, 1),
  /* 11   */ V(0, 15, 1),

  /* 0000 0000 1010 ... */
  /* 00   */ V(12, 9, 2),	/* 344 */
  /* 01   */ V(5, 14, 2),
  /* 10   */ V(10, 11, 1),
  /* 11   */ V(10, 11, 1),

  /* 0000 0000 1011 ... */
  /* 00   */ V(7, 13, 2),	/* 348 */
  /* 01   */ V(13, 7, 2),
  /* 10   */ V(4, 14, 1),
  /* 11   */ V(4, 14, 1),

  /* 0000 0000 1100 ... */
  /* 00   */ V(12, 8, 2),	/* 352 */
  /* 01   */ V(13, 6, 2),
  /* 10   */ V(3, 14, 1),
  /* 11   */ V(3, 14, 1),

  /* 0000 0000 1101 ... */
  /* 00   */ V(11, 9, 1),	/* 356 */
  /* 01   */ V(11, 9, 1),
  /* 10   */ V(9, 11, 2),
  /* 11   */ V(10, 10, 2),

  /* 0000 0001 0001 ... */
  /* 0    */ V(11, 10, 1),	/* 360 */
  /* 1    */ V(14, 5, 1),

  /* 0000 0001 0010 ... */
  /* 0    */ V(14, 4, 1),	/* 362 */
  /* 1    */ V(8, 12, 1),

  /* 0000 0001 0011 ... */
  /* 0    */ V(6, 13, 1),	/* 364 */
  /* 1    */ V(14, 3, 1),

  /* 0000 0001 0101 ... */
  /* 0    */ V(2, 14, 1),	/* 366 */
  /* 1    */ V(0, 14, 1),

  /* 0000 0001 1000 ... */
  /* 0    */ V(14, 0, 1),	/* 368 */
  /* 1    */ V(5, 13, 1),

  /* 0000 0001 1001 ... */
  /* 0    */ V(13, 5, 1),	/* 370 */
  /* 1    */ V(7, 12, 1),

  /* 0000 0001 1010 ... */
  /* 0    */ V(12, 7, 1),	/* 372 */
  /* 1    */ V(4, 13, 1),

  /* 0000 0001 1011 ... */
  /* 0    */ V(8, 11, 1),	/* 374 */
  /* 1    */ V(11, 8, 1),

  /* 0000 0001 1100 ... */
  /* 0    */ V(13, 4, 1),	/* 376 */
  /* 1    */ V(9, 10, 1),

  /* 0000 0001 1101 ... */
  /* 0    */ V(10, 9, 1),	/* 378 */
  /* 1    */ V(6, 12, 1),

  /* 0000 0010 0000 ... */
  /* 0    */ V(13, 3, 1),	/* 380 */
  /* 1    */ V(7, 11, 1),

  /* 0000 0010 0101 ... */
  /* 0    */ V(5, 12, 1),	/* 382 */
  /* 1    */ V(12, 5, 1),

  /* 0000 0010 0110 ... */
  /* 0    */ V(9, 9, 1),	/* 384 */
  /* 1    */ V(7, 10, 1),

  /* 0000 0010 1000 ... */
  /* 0    */ V(10, 7, 1),	/* 386 */
  /* 1    */ V(9, 7, 1),

  /* 0000 0000 0000 0000 ... */
  /* 000  */ V(15, 14, 3),	/* 388 */
  /* 001  */ V(15, 12, 3),
  /* 010  */ V(15, 13, 2),
  /* 011  */ V(15, 13, 2),
  /* 100  */ V(14, 13, 1),
  /* 101  */ V(14, 13, 1),
  /* 110  */ V(14, 13, 1),
  /* 111  */ V(14, 13, 1),

  /* 0000 0000 0000 1011 ... */
  /* 0    */ V(10, 15, 1),	/* 396 */
  /* 1    */ V(14, 9, 1)
};

static
union huffpair const hufftab15[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 4),
  /* 0010 */ PTR(48, 4),
  /* 0011 */ PTR(64, 4),
  /* 0100 */ PTR(80, 4),
  /* 0101 */ PTR(96, 3),
  /* 0110 */ PTR(104, 3),
  /* 0111 */ PTR(112, 2),
  /* 1000 */ PTR(116, 1),
  /* 1001 */ PTR(118, 1),
  /* 1010 */ V(1, 1, 3),
  /* 1011 */ V(1, 1, 3),
  /* 1100 */ V(0, 1, 4),
  /* 1101 */ V(1, 0, 4),
  /* 1110 */ V(0, 0, 3),
  /* 1111 */ V(0, 0, 3),

  /* 0000 ... */
  /* 0000 */ PTR(120, 4),	/* 16 */
  /* 0001 */ PTR(136, 4),
  /* 0010 */ PTR(152, 4),
  /* 0011 */ PTR(168, 4),
  /* 0100 */ PTR(184, 4),
  /* 0101 */ PTR(200, 3),
  /* 0110 */ PTR(208, 3),
  /* 0111 */ PTR(216, 4),
  /* 1000 */ PTR(232, 3),
  /* 1001 */ PTR(240, 3),
  /* 1010 */ PTR(248, 3),
  /* 1011 */ PTR(256, 3),
  /* 1100 */ PTR(264, 2),
  /* 1101 */ PTR(268, 3),
  /* 1110 */ PTR(276, 3),
  /* 1111 */ PTR(284, 2),

  /* 0001 ... */
  /* 0000 */ PTR(288, 2),	/* 32 */
  /* 0001 */ PTR(292, 2),
  /* 0010 */ PTR(296, 2),
  /* 0011 */ PTR(300, 2),
  /* 0100 */ PTR(304, 2),
  /* 0101 */ PTR(308, 2),
  /* 0110 */ PTR(312, 2),
  /* 0111 */ PTR(316, 2),
  /* 1000 */ PTR(320, 1),
  /* 1001 */ PTR(322, 1),
  /* 1010 */ PTR(324, 1),
  /* 1011 */ PTR(326, 2),
  /* 1100 */ PTR(330, 1),
  /* 1101 */ PTR(332, 1),
  /* 1110 */ PTR(334, 2),
  /* 1111 */ PTR(338, 1),

  /* 0010 ... */
  /* 0000 */ PTR(340, 1),	/* 48 */
  /* 0001 */ PTR(342, 1),
  /* 0010 */ V(9, 1, 4),
  /* 0011 */ PTR(344, 1),
  /* 0100 */ PTR(346, 1),
  /* 0101 */ PTR(348, 1),
  /* 0110 */ PTR(350, 1),
  /* 0111 */ PTR(352, 1),
  /* 1000 */ V(2, 8, 4),
  /* 1001 */ V(8, 2, 4),
  /* 1010 */ V(1, 8, 4),
  /* 1011 */ V(8, 1, 4),
  /* 1100 */ PTR(354, 1),
  /* 1101 */ PTR(356, 1),
  /* 1110 */ PTR(358, 1),
  /* 1111 */ PTR(360, 1),

  /* 0011 ... */
  /* 0000 */ V(2, 7, 4),	/* 64 */
  /* 0001 */ V(7, 2, 4),
  /* 0010 */ V(6, 4, 4),
  /* 0011 */ V(1, 7, 4),
  /* 0100 */ V(5, 5, 4),
  /* 0101 */ V(7, 1, 4),
  /* 0110 */ PTR(362, 1),
  /* 0111 */ V(3, 6, 4),
  /* 1000 */ V(6, 3, 4),
  /* 1001 */ V(4, 5, 4),
  /* 1010 */ V(5, 4, 4),
  /* 1011 */ V(2, 6, 4),
  /* 1100 */ V(6, 2, 4),
  /* 1101 */ V(1, 6, 4),
  /* 1110 */ PTR(364, 1),
  /* 1111 */ V(3, 5, 4),

  /* 0100 ... */
  /* 0000 */ V(6, 1, 3),	/* 80 */
  /* 0001 */ V(6, 1, 3),
  /* 0010 */ V(5, 3, 4),
  /* 0011 */ V(4, 4, 4),
  /* 0100 */ V(2, 5, 3),
  /* 0101 */ V(2, 5, 3),
  /* 0110 */ V(5, 2, 3),
  /* 0111 */ V(5, 2, 3),
  /* 1000 */ V(1, 5, 3),
  /* 1001 */ V(1, 5, 3),
  /* 1010 */ V(5, 1, 3),
  /* 1011 */ V(5, 1, 3),
  /* 1100 */ V(0, 5, 4),
  /* 1101 */ V(5, 0, 4),
  /* 1110 */ V(3, 4, 3),
  /* 1111 */ V(3, 4, 3),

  /* 0101 ... */
  /* 000  */ V(4, 3, 3),	/* 96 */
  /* 001  */ V(2, 4, 3),
  /* 010  */ V(4, 2, 3),
  /* 011  */ V(3, 3, 3),
  /* 100  */ V(4, 1, 2),
  /* 101  */ V(4, 1, 2),
  /* 110  */ V(1, 4, 3),
  /* 111  */ V(0, 4, 3),

  /* 0110 ... */
  /* 000  */ V(2, 3, 2),	/* 104 */
  /* 001  */ V(2, 3, 2),
  /* 010  */ V(3, 2, 2),
  /* 011  */ V(3, 2, 2),
  /* 100  */ V(4, 0, 3),
  /* 101  */ V(0, 3, 3),
  /* 110  */ V(1, 3, 2),
  /* 111  */ V(1, 3, 2),

  /* 0111 ... */
  /* 00   */ V(3, 1, 2),	/* 112 */
  /* 01   */ V(3, 0, 2),
  /* 10   */ V(2, 2, 1),
  /* 11   */ V(2, 2, 1),

  /* 1000 ... */
  /* 0    */ V(1, 2, 1),	/* 116 */
  /* 1    */ V(2, 1, 1),

  /* 1001 ... */
  /* 0    */ V(0, 2, 1),	/* 118 */
  /* 1    */ V(2, 0, 1),

  /* 0000 0000 ... */
  /* 0000 */ PTR(366, 1),	/* 120 */
  /* 0001 */ PTR(368, 1),
  /* 0010 */ V(14, 14, 4),
  /* 0011 */ PTR(370, 1),
  /* 0100 */ PTR(372, 1),
  /* 0101 */ PTR(374, 1),
  /* 0110 */ V(15, 11, 4),
  /* 0111 */ PTR(376, 1),
  /* 1000 */ V(13, 13, 4),
  /* 1001 */ V(10, 15, 4),
  /* 1010 */ V(15, 10, 4),
  /* 1011 */ V(11, 14, 4),
  /* 1100 */ V(14, 11, 4),
  /* 1101 */ V(12, 13, 4),
  /* 1110 */ V(13, 12, 4),
  /* 1111 */ V(9, 15, 4),

  /* 0000 0001 ... */
  /* 0000 */ V(15, 9, 4),	/* 136 */
  /* 0001 */ V(14, 10, 4),
  /* 0010 */ V(11, 13, 4),
  /* 0011 */ V(13, 11, 4),
  /* 0100 */ V(8, 15, 4),
  /* 0101 */ V(15, 8, 4),
  /* 0110 */ V(12, 12, 4),
  /* 0111 */ V(9, 14, 4),
  /* 1000 */ V(14, 9, 4),
  /* 1001 */ V(7, 15, 4),
  /* 1010 */ V(15, 7, 4),
  /* 1011 */ V(10, 13, 4),
  /* 1100 */ V(13, 10, 4),
  /* 1101 */ V(11, 12, 4),
  /* 1110 */ V(6, 15, 4),
  /* 1111 */ PTR(378, 1),

  /* 0000 0010 ... */
  /* 0000 */ V(12, 11, 3),	/* 152 */
  /* 0001 */ V(12, 11, 3),
  /* 0010 */ V(15, 6, 3),
  /* 0011 */ V(15, 6, 3),
  /* 0100 */ V(8, 14, 4),
  /* 0101 */ V(14, 8, 4),
  /* 0110 */ V(5, 15, 4),
  /* 0111 */ V(9, 13, 4),
  /* 1000 */ V(15, 5, 3),
  /* 1001 */ V(15, 5, 3),
  /* 1010 */ V(7, 14, 3),
  /* 1011 */ V(7, 14, 3),
  /* 1100 */ V(14, 7, 3),
  /* 1101 */ V(14, 7, 3),
  /* 1110 */ V(10, 12, 3),
  /* 1111 */ V(10, 12, 3),

  /* 0000 0011 ... */
  /* 0000 */ V(12, 10, 3),	/* 168 */
  /* 0001 */ V(12, 10, 3),
  /* 0010 */ V(11, 11, 3),
  /* 0011 */ V(11, 11, 3),
  /* 0100 */ V(13, 9, 4),
  /* 0101 */ V(8, 13, 4),
  /* 0110 */ V(4, 15, 3),
  /* 0111 */ V(4, 15, 3),
  /* 1000 */ V(15, 4, 3),
  /* 1001 */ V(15, 4, 3),
  /* 1010 */ V(3, 15, 3),
  /* 1011 */ V(3, 15, 3),
  /* 1100 */ V(15, 3, 3),
  /* 1101 */ V(15, 3, 3),
  /* 1110 */ V(13, 8, 3),
  /* 1111 */ V(13, 8, 3),

  /* 0000 0100 ... */
  /* 0000 */ V(14, 6, 3),	/* 184 */
  /* 0001 */ V(14, 6, 3),
  /* 0010 */ V(2, 15, 3),
  /* 0011 */ V(2, 15, 3),
  /* 0100 */ V(15, 2, 3),
  /* 0101 */ V(15, 2, 3),
  /* 0110 */ V(6, 14, 4),
  /* 0111 */ V(15, 0, 4),
  /* 1000 */ V(1, 15, 3),
  /* 1001 */ V(1, 15, 3),
  /* 1010 */ V(15, 1, 3),
  /* 1011 */ V(15, 1, 3),
  /* 1100 */ V(9, 12, 3),
  /* 1101 */ V(9, 12, 3),
  /* 1110 */ V(12, 9, 3),
  /* 1111 */ V(12, 9, 3),

  /* 0000 0101 ... */
  /* 000  */ V(5, 14, 3),	/* 200 */
  /* 001  */ V(10, 11, 3),
  /* 010  */ V(11, 10, 3),
  /* 011  */ V(14, 5, 3),
  /* 100  */ V(7, 13, 3),
  /* 101  */ V(13, 7, 3),
  /* 110  */ V(4, 14, 3),
  /* 111  */ V(14, 4, 3),

  /* 0000 0110 ... */
  /* 000  */ V(8, 12, 3),	/* 208 */
  /* 001  */ V(12, 8, 3),
  /* 010  */ V(3, 14, 3),
  /* 011  */ V(6, 13, 3),
  /* 100  */ V(13, 6, 3),
  /* 101  */ V(14, 3, 3),
  /* 110  */ V(9, 11, 3),
  /* 111  */ V(11, 9, 3),

  /* 0000 0111 ... */
  /* 0000 */ V(2, 14, 3),	/* 216 */
  /* 0001 */ V(2, 14, 3),
  /* 0010 */ V(10, 10, 3),
  /* 0011 */ V(10, 10, 3),
  /* 0100 */ V(14, 2, 3),
  /* 0101 */ V(14, 2, 3),
  /* 0110 */ V(1, 14, 3),
  /* 0111 */ V(1, 14, 3),
  /* 1000 */ V(14, 1, 3),
  /* 1001 */ V(14, 1, 3),
  /* 1010 */ V(0, 14, 4),
  /* 1011 */ V(14, 0, 4),
  /* 1100 */ V(5, 13, 3),
  /* 1101 */ V(5, 13, 3),
  /* 1110 */ V(13, 5, 3),
  /* 1111 */ V(13, 5, 3),

  /* 0000 1000 ... */
  /* 000  */ V(7, 12, 3),	/* 232 */
  /* 001  */ V(12, 7, 3),
  /* 010  */ V(4, 13, 3),
  /* 011  */ V(8, 11, 3),
  /* 100  */ V(13, 4, 2),
  /* 101  */ V(13, 4, 2),
  /* 110  */ V(11, 8, 3),
  /* 111  */ V(9, 10, 3),

  /* 0000 1001 ... */
  /* 000  */ V(10, 9, 3),	/* 240 */
  /* 001  */ V(6, 12, 3),
  /* 010  */ V(12, 6, 3),
  /* 011  */ V(3, 13, 3),
  /* 100  */ V(13, 3, 2),
  /* 101  */ V(13, 3, 2),
  /* 110  */ V(13, 2, 2),
  /* 111  */ V(13, 2, 2),

  /* 0000 1010 ... */
  /* 000  */ V(2, 13, 3),	/* 248 */
  /* 001  */ V(0, 13, 3),
  /* 010  */ V(1, 13, 2),
  /* 011  */ V(1, 13, 2),
  /* 100  */ V(7, 11, 2),
  /* 101  */ V(7, 11, 2),
  /* 110  */ V(11, 7, 2),
  /* 111  */ V(11, 7, 2),

  /* 0000 1011 ... */
  /* 000  */ V(13, 1, 2),	/* 256 */
  /* 001  */ V(13, 1, 2),
  /* 010  */ V(5, 12, 3),
  /* 011  */ V(13, 0, 3),
  /* 100  */ V(12, 5, 2),
  /* 101  */ V(12, 5, 2),
  /* 110  */ V(8, 10, 2),
  /* 111  */ V(8, 10, 2),

  /* 0000 1100 ... */
  /* 00   */ V(10, 8, 2),	/* 264 */
  /* 01   */ V(4, 12, 2),
  /* 10   */ V(12, 4, 2),
  /* 11   */ V(6, 11, 2),

  /* 0000 1101 ... */
  /* 000  */ V(11, 6, 2),	/* 268 */
  /* 001  */ V(11, 6, 2),
  /* 010  */ V(9, 9, 3),
  /* 011  */ V(0, 12, 3),
  /* 100  */ V(3, 12, 2),
  /* 101  */ V(3, 12, 2),
  /* 110  */ V(12, 3, 2),
  /* 111  */ V(12, 3, 2),

  /* 0000 1110 ... */
  /* 000  */ V(7, 10, 2),	/* 276 */
  /* 001  */ V(7, 10, 2),
  /* 010  */ V(10, 7, 2),
  /* 011  */ V(10, 7, 2),
  /* 100  */ V(10, 6, 2),
  /* 101  */ V(10, 6, 2),
  /* 110  */ V(12, 0, 3),
  /* 111  */ V(0, 11, 3),

  /* 0000 1111 ... */
  /* 00   */ V(12, 2, 1),	/* 284 */
  /* 01   */ V(12, 2, 1),
  /* 10   */ V(2, 12, 2),
  /* 11   */ V(5, 11, 2),

  /* 0001 0000 ... */
  /* 00   */ V(11, 5, 2),	/* 288 */
  /* 01   */ V(1, 12, 2),
  /* 10   */ V(8, 9, 2),
  /* 11   */ V(9, 8, 2),

  /* 0001 0001 ... */
  /* 00   */ V(12, 1, 2),	/* 292 */
  /* 01   */ V(4, 11, 2),
  /* 10   */ V(11, 4, 2),
  /* 11   */ V(6, 10, 2),

  /* 0001 0010 ... */
  /* 00   */ V(3, 11, 2),	/* 296 */
  /* 01   */ V(7, 9, 2),
  /* 10   */ V(11, 3, 1),
  /* 11   */ V(11, 3, 1),

  /* 0001 0011 ... */
  /* 00   */ V(9, 7, 2),	/* 300 */
  /* 01   */ V(8, 8, 2),
  /* 10   */ V(2, 11, 2),
  /* 11   */ V(5, 10, 2),

  /* 0001 0100 ... */
  /* 00   */ V(11, 2, 1),	/* 304 */
  /* 01   */ V(11, 2, 1),
  /* 10   */ V(10, 5, 2),
  /* 11   */ V(1, 11, 2),

  /* 0001 0101 ... */
  /* 00   */ V(11, 1, 1),	/* 308 */
  /* 01   */ V(11, 1, 1),
  /* 10   */ V(11, 0, 2),
  /* 11   */ V(6, 9, 2),

  /* 0001 0110 ... */
  /* 00   */ V(9, 6, 2),	/* 312 */
  /* 01   */ V(4, 10, 2),
  /* 10   */ V(10, 4, 2),
  /* 11   */ V(7, 8, 2),

  /* 0001 0111 ... */
  /* 00   */ V(8, 7, 2),	/* 316 */
  /* 01   */ V(3, 10, 2),
  /* 10   */ V(10, 3, 1),
  /* 11   */ V(10, 3, 1),

  /* 0001 1000 ... */
  /* 0    */ V(5, 9, 1),	/* 320 */
  /* 1    */ V(9, 5, 1),

  /* 0001 1001 ... */
  /* 0    */ V(2, 10, 1),	/* 322 */
  /* 1    */ V(10, 2, 1),

  /* 0001 1010 ... */
  /* 0    */ V(1, 10, 1),	/* 324 */
  /* 1    */ V(10, 1, 1),

  /* 0001 1011 ... */
  /* 00   */ V(0, 10, 2),	/* 326 */
  /* 01   */ V(10, 0, 2),
  /* 10   */ V(6, 8, 1),
  /* 11   */ V(6, 8, 1),

  /* 0001 1100 ... */
  /* 0    */ V(8, 6, 1),	/* 330 */
  /* 1    */ V(4, 9, 1),

  /* 0001 1101 ... */
  /* 0    */ V(9, 4, 1),	/* 332 */
  /* 1    */ V(3, 9, 1),

  /* 0001 1110 ... */
  /* 00   */ V(9, 3, 1),	/* 334 */
  /* 01   */ V(9, 3, 1),
  /* 10   */ V(7, 7, 2),
  /* 11   */ V(0, 9, 2),

  /* 0001 1111 ... */
  /* 0    */ V(5, 8, 1),	/* 338 */
  /* 1    */ V(8, 5, 1),

  /* 0010 0000 ... */
  /* 0    */ V(2, 9, 1),	/* 340 */
  /* 1    */ V(6, 7, 1),

  /* 0010 0001 ... */
  /* 0    */ V(7, 6, 1),	/* 342 */
  /* 1    */ V(9, 2, 1),

  /* 0010 0011 ... */
  /* 0    */ V(1, 9, 1),	/* 344 */
  /* 1    */ V(9, 0, 1),

  /* 0010 0100 ... */
  /* 0    */ V(4, 8, 1),	/* 346 */
  /* 1    */ V(8, 4, 1),

  /* 0010 0101 ... */
  /* 0    */ V(5, 7, 1),	/* 348 */
  /* 1    */ V(7, 5, 1),

  /* 0010 0110 ... */
  /* 0    */ V(3, 8, 1),	/* 350 */
  /* 1    */ V(8, 3, 1),

  /* 0010 0111 ... */
  /* 0    */ V(6, 6, 1),	/* 352 */
  /* 1    */ V(4, 7, 1),

  /* 0010 1100 ... */
  /* 0    */ V(7, 4, 1),	/* 354 */
  /* 1    */ V(0, 8, 1),

  /* 0010 1101 ... */
  /* 0    */ V(8, 0, 1),	/* 356 */
  /* 1    */ V(5, 6, 1),

  /* 0010 1110 ... */
  /* 0    */ V(6, 5, 1),	/* 358 */
  /* 1    */ V(3, 7, 1),

  /* 0010 1111 ... */
  /* 0    */ V(7, 3, 1),	/* 360 */
  /* 1    */ V(4, 6, 1),

  /* 0011 0110 ... */
  /* 0    */ V(0, 7, 1),	/* 362 */
  /* 1    */ V(7, 0, 1),

  /* 0011 1110 ... */
  /* 0    */ V(0, 6, 1),	/* 364 */
  /* 1    */ V(6, 0, 1),

  /* 0000 0000 0000 ... */
  /* 0    */ V(15, 15, 1),	/* 366 */
  /* 1    */ V(14, 15, 1),

  /* 0000 0000 0001 ... */
  /* 0    */ V(15, 14, 1),	/* 368 */
  /* 1    */ V(13, 15, 1),

  /* 0000 0000 0011 ... */
  /* 0    */ V(15, 13, 1),	/* 370 */
  /* 1    */ V(12, 15, 1),

  /* 0000 0000 0100 ... */
  /* 0    */ V(15, 12, 1),	/* 372 */
  /* 1    */ V(13, 14, 1),

  /* 0000 0000 0101 ... */
  /* 0    */ V(14, 13, 1),	/* 374 */
  /* 1    */ V(11, 15, 1),

  /* 0000 0000 0111 ... */
  /* 0    */ V(12, 14, 1),	/* 376 */
  /* 1    */ V(14, 12, 1),

  /* 0000 0001 1111 ... */
  /* 0    */ V(10, 14, 1),	/* 378 */
  /* 1    */ V(0, 15, 1)
};

static
union huffpair const hufftab16[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 4),
  /* 0010 */ PTR(48, 4),
  /* 0011 */ PTR(64, 2),
  /* 0100 */ V(1, 1, 4),
  /* 0101 */ V(0, 1, 4),
  /* 0110 */ V(1, 0, 3),
  /* 0111 */ V(1, 0, 3),
  /* 1000 */ V(0, 0, 1),
  /* 1001 */ V(0, 0, 1),
  /* 1010 */ V(0, 0, 1),
  /* 1011 */ V(0, 0, 1),
  /* 1100 */ V(0, 0, 1),
  /* 1101 */ V(0, 0, 1),
  /* 1110 */ V(0, 0, 1),
  /* 1111 */ V(0, 0, 1),

  /* 0000 ... */
  /* 0000 */ PTR(68, 3),	/* 16 */
  /* 0001 */ PTR(76, 3),
  /* 0010 */ PTR(84, 2),
  /* 0011 */ V(15, 15, 4),
  /* 0100 */ PTR(88, 2),
  /* 0101 */ PTR(92, 1),
  /* 0110 */ PTR(94, 4),
  /* 0111 */ V(15, 2, 4),
  /* 1000 */ PTR(110, 1),
  /* 1001 */ V(1, 15, 4),
  /* 1010 */ V(15, 1, 4),
  /* 1011 */ PTR(112, 4),
  /* 1100 */ PTR(128, 4),
  /* 1101 */ PTR(144, 4),
  /* 1110 */ PTR(160, 4),
  /* 1111 */ PTR(176, 4),

  /* 0001 ... */
  /* 0000 */ PTR(192, 4),	/* 32 */
  /* 0001 */ PTR(208, 3),
  /* 0010 */ PTR(216, 3),
  /* 0011 */ PTR(224, 3),
  /* 0100 */ PTR(232, 3),
  /* 0101 */ PTR(240, 3),
  /* 0110 */ PTR(248, 3),
  /* 0111 */ PTR(256, 3),
  /* 1000 */ PTR(264, 2),
  /* 1001 */ PTR(268, 2),
  /* 1010 */ PTR(272, 1),
  /* 1011 */ PTR(274, 2),
  /* 1100 */ PTR(278, 2),
  /* 1101 */ PTR(282, 1),
  /* 1110 */ V(5, 1, 4),
  /* 1111 */ PTR(284, 1),

  /* 0010 ... */
  /* 0000 */ PTR(286, 1),	/* 48 */
  /* 0001 */ PTR(288, 1),
  /* 0010 */ PTR(290, 1),
  /* 0011 */ V(1, 4, 4),
  /* 0100 */ V(4, 1, 4),
  /* 0101 */ PTR(292, 1),
  /* 0110 */ V(2, 3, 4),
  /* 0111 */ V(3, 2, 4),
  /* 1000 */ V(1, 3, 3),
  /* 1001 */ V(1, 3, 3),
  /* 1010 */ V(3, 1, 3),
  /* 1011 */ V(3, 1, 3),
  /* 1100 */ V(0, 3, 4),
  /* 1101 */ V(3, 0, 4),
  /* 1110 */ V(2, 2, 3),
  /* 1111 */ V(2, 2, 3),

  /* 0011 ... */
  /* 00   */ V(1, 2, 2),	/* 64 */
  /* 01   */ V(2, 1, 2),
  /* 10   */ V(0, 2, 2),
  /* 11   */ V(2, 0, 2),

  /* 0000 0000 ... */
  /* 000  */ V(14, 15, 3),	/* 68 */
  /* 001  */ V(15, 14, 3),
  /* 010  */ V(13, 15, 3),
  /* 011  */ V(15, 13, 3),
  /* 100  */ V(12, 15, 3),
  /* 101  */ V(15, 12, 3),
  /* 110  */ V(11, 15, 3),
  /* 111  */ V(15, 11, 3),

  /* 0000 0001 ... */
  /* 000  */ V(10, 15, 2),	/* 76 */
  /* 001  */ V(10, 15, 2),
  /* 010  */ V(15, 10, 3),
  /* 011  */ V(9, 15, 3),
  /* 100  */ V(15, 9, 3),
  /* 101  */ V(15, 8, 3),
  /* 110  */ V(8, 15, 2),
  /* 111  */ V(8, 15, 2),

  /* 0000 0010 ... */
  /* 00   */ V(7, 15, 2),	/* 84 */
  /* 01   */ V(15, 7, 2),
  /* 10   */ V(6, 15, 2),
  /* 11   */ V(15, 6, 2),

  /* 0000 0100 ... */
  /* 00   */ V(5, 15, 2),	/* 88 */
  /* 01   */ V(15, 5, 2),
  /* 10   */ V(4, 15, 1),
  /* 11   */ V(4, 15, 1),

  /* 0000 0101 ... */
  /* 0    */ V(15, 4, 1),	/* 92 */
  /* 1    */ V(15, 3, 1),

  /* 0000 0110 ... */
  /* 0000 */ V(15, 0, 1),	/* 94 */
  /* 0001 */ V(15, 0, 1),
  /* 0010 */ V(15, 0, 1),
  /* 0011 */ V(15, 0, 1),
  /* 0100 */ V(15, 0, 1),
  /* 0101 */ V(15, 0, 1),
  /* 0110 */ V(15, 0, 1),
  /* 0111 */ V(15, 0, 1),
  /* 1000 */ V(3, 15, 2),
  /* 1001 */ V(3, 15, 2),
  /* 1010 */ V(3, 15, 2),
  /* 1011 */ V(3, 15, 2),
  /* 1100 */ PTR(294, 4),
  /* 1101 */ PTR(310, 3),
  /* 1110 */ PTR(318, 3),
  /* 1111 */ PTR(326, 3),

  /* 0000 1000 ... */
  /* 0    */ V(2, 15, 1),	/* 110 */
  /* 1    */ V(0, 15, 1),

  /* 0000 1011 ... */
  /* 0000 */ PTR(334, 2),	/* 112 */
  /* 0001 */ PTR(338, 2),
  /* 0010 */ PTR(342, 2),
  /* 0011 */ PTR(346, 1),
  /* 0100 */ PTR(348, 2),
  /* 0101 */ PTR(352, 2),
  /* 0110 */ PTR(356, 1),
  /* 0111 */ PTR(358, 2),
  /* 1000 */ PTR(362, 2),
  /* 1001 */ PTR(366, 2),
  /* 1010 */ PTR(370, 2),
  /* 1011 */ V(14, 3, 4),
  /* 1100 */ PTR(374, 1),
  /* 1101 */ PTR(376, 1),
  /* 1110 */ PTR(378, 1),
  /* 1111 */ PTR(380, 1),

  /* 0000 1100 ... */
  /* 0000 */ PTR(382, 1),	/* 128 */
  /* 0001 */ PTR(384, 1),
  /* 0010 */ PTR(386, 1),
  /* 0011 */ V(0, 13, 4),
  /* 0100 */ PTR(388, 1),
  /* 0101 */ PTR(390, 1),
  /* 0110 */ PTR(392, 1),
  /* 0111 */ V(3, 12, 4),
  /* 1000 */ PTR(394, 1),
  /* 1001 */ V(1, 12, 4),
  /* 1010 */ V(12, 0, 4),
  /* 1011 */ PTR(396, 1),
  /* 1100 */ V(14, 2, 3),
  /* 1101 */ V(14, 2, 3),
  /* 1110 */ V(2, 14, 4),
  /* 1111 */ V(1, 14, 4),

  /* 0000 1101 ... */
  /* 0000 */ V(13, 3, 4),	/* 144 */
  /* 0001 */ V(2, 13, 4),
  /* 0010 */ V(13, 2, 4),
  /* 0011 */ V(13, 1, 4),
  /* 0100 */ V(3, 11, 4),
  /* 0101 */ PTR(398, 1),
  /* 0110 */ V(1, 13, 3),
  /* 0111 */ V(1, 13, 3),
  /* 1000 */ V(12, 4, 4),
  /* 1001 */ V(6, 11, 4),
  /* 1010 */ V(12, 3, 4),
  /* 1011 */ V(10, 7, 4),
  /* 1100 */ V(2, 12, 3),
  /* 1101 */ V(2, 12, 3),
  /* 1110 */ V(12, 2, 4),
  /* 1111 */ V(11, 5, 4),

  /* 0000 1110 ... */
  /* 0000 */ V(12, 1, 4),	/* 160 */
  /* 0001 */ V(0, 12, 4),
  /* 0010 */ V(4, 11, 4),
  /* 0011 */ V(11, 4, 4),
  /* 0100 */ V(6, 10, 4),
  /* 0101 */ V(10, 6, 4),
  /* 0110 */ V(11, 3, 3),
  /* 0111 */ V(11, 3, 3),
  /* 1000 */ V(5, 10, 4),
  /* 1001 */ V(10, 5, 4),
  /* 1010 */ V(2, 11, 3),
  /* 1011 */ V(2, 11, 3),
  /* 1100 */ V(11, 2, 3),
  /* 1101 */ V(11, 2, 3),
  /* 1110 */ V(1, 11, 3),
  /* 1111 */ V(1, 11, 3),

  /* 0000 1111 ... */
  /* 0000 */ V(11, 1, 3),	/* 176 */
  /* 0001 */ V(11, 1, 3),
  /* 0010 */ V(0, 11, 4),
  /* 0011 */ V(11, 0, 4),
  /* 0100 */ V(6, 9, 4),
  /* 0101 */ V(9, 6, 4),
  /* 0110 */ V(4, 10, 4),
  /* 0111 */ V(10, 4, 4),
  /* 1000 */ V(7, 8, 4),
  /* 1001 */ V(8, 7, 4),
  /* 1010 */ V(10, 3, 3),
  /* 1011 */ V(10, 3, 3),
  /* 1100 */ V(3, 10, 4),
  /* 1101 */ V(5, 9, 4),
  /* 1110 */ V(2, 10, 3),
  /* 1111 */ V(2, 10, 3),

  /* 0001 0000 ... */
  /* 0000 */ V(9, 5, 4),	/* 192 */
  /* 0001 */ V(6, 8, 4),
  /* 0010 */ V(10, 1, 3),
  /* 0011 */ V(10, 1, 3),
  /* 0100 */ V(8, 6, 4),
  /* 0101 */ V(7, 7, 4),
  /* 0110 */ V(9, 4, 3),
  /* 0111 */ V(9, 4, 3),
  /* 1000 */ V(4, 9, 4),
  /* 1001 */ V(5, 7, 4),
  /* 1010 */ V(6, 7, 3),
  /* 1011 */ V(6, 7, 3),
  /* 1100 */ V(10, 2, 2),
  /* 1101 */ V(10, 2, 2),
  /* 1110 */ V(10, 2, 2),
  /* 1111 */ V(10, 2, 2),

  /* 0001 0001 ... */
  /* 000  */ V(1, 10, 2),	/* 208 */
  /* 001  */ V(1, 10, 2),
  /* 010  */ V(0, 10, 3),
  /* 011  */ V(10, 0, 3),
  /* 100  */ V(3, 9, 3),
  /* 101  */ V(9, 3, 3),
  /* 110  */ V(5, 8, 3),
  /* 111  */ V(8, 5, 3),

  /* 0001 0010 ... */
  /* 000  */ V(2, 9, 2),	/* 216 */
  /* 001  */ V(2, 9, 2),
  /* 010  */ V(9, 2, 2),
  /* 011  */ V(9, 2, 2),
  /* 100  */ V(7, 6, 3),
  /* 101  */ V(0, 9, 3),
  /* 110  */ V(1, 9, 2),
  /* 111  */ V(1, 9, 2),

  /* 0001 0011 ... */
  /* 000  */ V(9, 1, 2),	/* 224 */
  /* 001  */ V(9, 1, 2),
  /* 010  */ V(9, 0, 3),
  /* 011  */ V(4, 8, 3),
  /* 100  */ V(8, 4, 3),
  /* 101  */ V(7, 5, 3),
  /* 110  */ V(3, 8, 3),
  /* 111  */ V(8, 3, 3),

  /* 0001 0100 ... */
  /* 000  */ V(6, 6, 3),	/* 232 */
  /* 001  */ V(2, 8, 3),
  /* 010  */ V(8, 2, 2),
  /* 011  */ V(8, 2, 2),
  /* 100  */ V(4, 7, 3),
  /* 101  */ V(7, 4, 3),
  /* 110  */ V(1, 8, 2),
  /* 111  */ V(1, 8, 2),

  /* 0001 0101 ... */
  /* 000  */ V(8, 1, 2),	/* 240 */
  /* 001  */ V(8, 1, 2),
  /* 010  */ V(8, 0, 2),
  /* 011  */ V(8, 0, 2),
  /* 100  */ V(0, 8, 3),
  /* 101  */ V(5, 6, 3),
  /* 110  */ V(3, 7, 2),
  /* 111  */ V(3, 7, 2),

  /* 0001 0110 ... */
  /* 000  */ V(7, 3, 2),	/* 248 */
  /* 001  */ V(7, 3, 2),
  /* 010  */ V(6, 5, 3),
  /* 011  */ V(4, 6, 3),
  /* 100  */ V(2, 7, 2),
  /* 101  */ V(2, 7, 2),
  /* 110  */ V(7, 2, 2),
  /* 111  */ V(7, 2, 2),

  /* 0001 0111 ... */
  /* 000  */ V(6, 4, 3),	/* 256 */
  /* 001  */ V(5, 5, 3),
  /* 010  */ V(0, 7, 2),
  /* 011  */ V(0, 7, 2),
  /* 100  */ V(1, 7, 1),
  /* 101  */ V(1, 7, 1),
  /* 110  */ V(1, 7, 1),
  /* 111  */ V(1, 7, 1),

  /* 0001 1000 ... */
  /* 00   */ V(7, 1, 1),	/* 264  */
  /* 01   */ V(7, 1, 1),
  /* 10   */ V(7, 0, 2),
  /* 11   */ V(3, 6, 2),

  /* 0001 1001 ... */
  /* 00   */ V(6, 3, 2),	/* 268 */
  /* 01   */ V(4, 5, 2),
  /* 10   */ V(5, 4, 2),
  /* 11   */ V(2, 6, 2),

  /* 0001 1010 ... */
  /* 0    */ V(6, 2, 1),	/* 272 */
  /* 1    */ V(1, 6, 1),

  /* 0001 1011 ... */
  /* 00   */ V(6, 1, 1),	/* 274 */
  /* 01   */ V(6, 1, 1),
  /* 10   */ V(0, 6, 2),
  /* 11   */ V(6, 0, 2),

  /* 0001 1100 ... */
  /* 00   */ V(5, 3, 1),	/* 278 */
  /* 01   */ V(5, 3, 1),
  /* 10   */ V(3, 5, 2),
  /* 11   */ V(4, 4, 2),

  /* 0001 1101 ... */
  /* 0    */ V(2, 5, 1),	/* 282 */
  /* 1    */ V(5, 2, 1),

  /* 0001 1111 ... */
  /* 0    */ V(1, 5, 1),	/* 284 */
  /* 1    */ V(0, 5, 1),

  /* 0010 0000 ... */
  /* 0    */ V(3, 4, 1),	/* 286 */
  /* 1    */ V(4, 3, 1),

  /* 0010 0001 ... */
  /* 0    */ V(5, 0, 1),	/* 288 */
  /* 1    */ V(2, 4, 1),

  /* 0010 0010 ... */
  /* 0    */ V(4, 2, 1),	/* 290 */
  /* 1    */ V(3, 3, 1),

  /* 0010 0101 ... */
  /* 0    */ V(0, 4, 1),	/* 292 */
  /* 1    */ V(4, 0, 1),

  /* 0000 0110 1100 ... */
  /* 0000 */ V(12, 14, 4),	/* 294 */
  /* 0001 */ PTR(400, 1),
  /* 0010 */ V(13, 14, 3),
  /* 0011 */ V(13, 14, 3),
  /* 0100 */ V(14, 9, 3),
  /* 0101 */ V(14, 9, 3),
  /* 0110 */ V(14, 10, 4),
  /* 0111 */ V(13, 9, 4),
  /* 1000 */ V(14, 14, 2),
  /* 1001 */ V(14, 14, 2),
  /* 1010 */ V(14, 14, 2),
  /* 1011 */ V(14, 14, 2),
  /* 1100 */ V(14, 13, 3),
  /* 1101 */ V(14, 13, 3),
  /* 1110 */ V(14, 11, 3),
  /* 1111 */ V(14, 11, 3),

  /* 0000 0110 1101 ... */
  /* 000  */ V(11, 14, 2),	/* 310 */
  /* 001  */ V(11, 14, 2),
  /* 010  */ V(12, 13, 2),
  /* 011  */ V(12, 13, 2),
  /* 100  */ V(13, 12, 3),
  /* 101  */ V(13, 11, 3),
  /* 110  */ V(10, 14, 2),
  /* 111  */ V(10, 14, 2),

  /* 0000 0110 1110 ... */
  /* 000  */ V(12, 12, 2),	/* 318 */
  /* 001  */ V(12, 12, 2),
  /* 010  */ V(10, 13, 3),
  /* 011  */ V(13, 10, 3),
  /* 100  */ V(7, 14, 3),
  /* 101  */ V(10, 12, 3),
  /* 110  */ V(12, 10, 2),
  /* 111  */ V(12, 10, 2),

  /* 0000 0110 1111 ... */
  /* 000  */ V(12, 9, 3),	/* 326 */
  /* 001  */ V(7, 13, 3),
  /* 010  */ V(5, 14, 2),
  /* 011  */ V(5, 14, 2),
  /* 100  */ V(11, 13, 1),
  /* 101  */ V(11, 13, 1),
  /* 110  */ V(11, 13, 1),
  /* 111  */ V(11, 13, 1),

  /* 0000 1011 0000 ... */
  /* 00   */ V(9, 14, 1),	/* 334 */
  /* 01   */ V(9, 14, 1),
  /* 10   */ V(11, 12, 2),
  /* 11   */ V(12, 11, 2),

  /* 0000 1011 0001 ... */
  /* 00   */ V(8, 14, 2),	/* 338 */
  /* 01   */ V(14, 8, 2),
  /* 10   */ V(9, 13, 2),
  /* 11   */ V(14, 7, 2),

  /* 0000 1011 0010 ... */
  /* 00   */ V(11, 11, 2),	/* 342 */
  /* 01   */ V(8, 13, 2),
  /* 10   */ V(13, 8, 2),
  /* 11   */ V(6, 14, 2),

  /* 0000 1011 0011 ... */
  /* 0    */ V(14, 6, 1),	/* 346 */
  /* 1    */ V(9, 12, 1),

  /* 0000 1011 0100 ... */
  /* 00   */ V(10, 11, 2),	/* 348 */
  /* 01   */ V(11, 10, 2),
  /* 10   */ V(14, 5, 2),
  /* 11   */ V(13, 7, 2),

  /* 0000 1011 0101 ... */
  /* 00   */ V(4, 14, 1),	/* 352 */
  /* 01   */ V(4, 14, 1),
  /* 10   */ V(14, 4, 2),
  /* 11   */ V(8, 12, 2),

  /* 0000 1011 0110 ... */
  /* 0    */ V(12, 8, 1),	/* 356 */
  /* 1    */ V(3, 14, 1),

  /* 0000 1011 0111 ... */
  /* 00   */ V(6, 13, 1),	/* 358 */
  /* 01   */ V(6, 13, 1),
  /* 10   */ V(13, 6, 2),
  /* 11   */ V(9, 11, 2),

  /* 0000 1011 1000 ... */
  /* 00   */ V(11, 9, 2),	/* 362 */
  /* 01   */ V(10, 10, 2),
  /* 10   */ V(14, 1, 1),
  /* 11   */ V(14, 1, 1),

  /* 0000 1011 1001 ... */
  /* 00   */ V(13, 4, 1),	/* 366 */
  /* 01   */ V(13, 4, 1),
  /* 10   */ V(11, 8, 2),
  /* 11   */ V(10, 9, 2),

  /* 0000 1011 1010 ... */
  /* 00   */ V(7, 11, 1),	/* 370 */
  /* 01   */ V(7, 11, 1),
  /* 10   */ V(11, 7, 2),
  /* 11   */ V(13, 0, 2),

  /* 0000 1011 1100 ... */
  /* 0    */ V(0, 14, 1),	/* 374 */
  /* 1    */ V(14, 0, 1),

  /* 0000 1011 1101 ... */
  /* 0    */ V(5, 13, 1),	/* 376 */
  /* 1    */ V(13, 5, 1),

  /* 0000 1011 1110 ... */
  /* 0    */ V(7, 12, 1),	/* 378 */
  /* 1    */ V(12, 7, 1),

  /* 0000 1011 1111 ... */
  /* 0    */ V(4, 13, 1),	/* 380 */
  /* 1    */ V(8, 11, 1),

  /* 0000 1100 0000 ... */
  /* 0    */ V(9, 10, 1),	/* 382 */
  /* 1    */ V(6, 12, 1),

  /* 0000 1100 0001 ... */
  /* 0    */ V(12, 6, 1),	/* 384 */
  /* 1    */ V(3, 13, 1),

  /* 0000 1100 0010 ... */
  /* 0    */ V(5, 12, 1),	/* 386 */
  /* 1    */ V(12, 5, 1),

  /* 0000 1100 0100 ... */
  /* 0    */ V(8, 10, 1),	/* 388 */
  /* 1    */ V(10, 8, 1),

  /* 0000 1100 0101 ... */
  /* 0    */ V(9, 9, 1),	/* 390 */
  /* 1    */ V(4, 12, 1),

  /* 0000 1100 0110 ... */
  /* 0    */ V(11, 6, 1),	/* 392 */
  /* 1    */ V(7, 10, 1),

  /* 0000 1100 1000 ... */
  /* 0    */ V(5, 11, 1),	/* 394 */
  /* 1    */ V(8, 9, 1),

  /* 0000 1100 1011 ... */
  /* 0    */ V(9, 8, 1),	/* 396 */
  /* 1    */ V(7, 9, 1),

  /* 0000 1101 0101 ... */
  /* 0    */ V(9, 7, 1),	/* 398 */
  /* 1    */ V(8, 8, 1),

  /* 0000 0110 1100 0001 ... */
  /* 0    */ V(14, 12, 1),	/* 400 */
  /* 1    */ V(13, 13, 1)
};

static
union huffpair const hufftab24[] = {
  /* 0000 */ PTR(16, 4),
  /* 0001 */ PTR(32, 4),
  /* 0010 */ PTR(48, 4),
  /* 0011 */ V(15, 15, 4),
  /* 0100 */ PTR(64, 4),
  /* 0101 */ PTR(80, 4),
  /* 0110 */ PTR(96, 4),
  /* 0111 */ PTR(112, 4),
  /* 1000 */ PTR(128, 4),
  /* 1001 */ PTR(144, 4),
  /* 1010 */ PTR(160, 3),
  /* 1011 */ PTR(168, 2),
  /* 1100 */ V(1, 1, 4),
  /* 1101 */ V(0, 1, 4),
  /* 1110 */ V(1, 0, 4),
  /* 1111 */ V(0, 0, 4),

  /* 0000 ... */
  /* 0000 */ V(14, 15, 4),	/* 16 */
  /* 0001 */ V(15, 14, 4),
  /* 0010 */ V(13, 15, 4),
  /* 0011 */ V(15, 13, 4),
  /* 0100 */ V(12, 15, 4),
  /* 0101 */ V(15, 12, 4),
  /* 0110 */ V(11, 15, 4),
  /* 0111 */ V(15, 11, 4),
  /* 1000 */ V(15, 10, 3),
  /* 1001 */ V(15, 10, 3),
  /* 1010 */ V(10, 15, 4),
  /* 1011 */ V(9, 15, 4),
  /* 1100 */ V(15, 9, 3),
  /* 1101 */ V(15, 9, 3),
  /* 1110 */ V(15, 8, 3),
  /* 1111 */ V(15, 8, 3),

  /* 0001 ... */
  /* 0000 */ V(8, 15, 4),	/* 32 */
  /* 0001 */ V(7, 15, 4),
  /* 0010 */ V(15, 7, 3),
  /* 0011 */ V(15, 7, 3),
  /* 0100 */ V(6, 15, 3),
  /* 0101 */ V(6, 15, 3),
  /* 0110 */ V(15, 6, 3),
  /* 0111 */ V(15, 6, 3),
  /* 1000 */ V(5, 15, 3),
  /* 1001 */ V(5, 15, 3),
  /* 1010 */ V(15, 5, 3),
  /* 1011 */ V(15, 5, 3),
  /* 1100 */ V(4, 15, 3),
  /* 1101 */ V(4, 15, 3),
  /* 1110 */ V(15, 4, 3),
  /* 1111 */ V(15, 4, 3),

  /* 0010 ... */
  /* 0000 */ V(3, 15, 3),	/* 48 */
  /* 0001 */ V(3, 15, 3),
  /* 0010 */ V(15, 3, 3),
  /* 0011 */ V(15, 3, 3),
  /* 0100 */ V(2, 15, 3),
  /* 0101 */ V(2, 15, 3),
  /* 0110 */ V(15, 2, 3),
  /* 0111 */ V(15, 2, 3),
  /* 1000 */ V(15, 1, 3),
  /* 1001 */ V(15, 1, 3),
  /* 1010 */ V(1, 15, 4),
  /* 1011 */ V(15, 0, 4),
  /* 1100 */ PTR(172, 3),
  /* 1101 */ PTR(180, 3),
  /* 1110 */ PTR(188, 3),
  /* 1111 */ PTR(196, 3),

  /* 0100 ... */
  /* 0000 */ PTR(204, 4),	/* 64 */
  /* 0001 */ PTR(220, 3),
  /* 0010 */ PTR(228, 3),
  /* 0011 */ PTR(236, 3),
  /* 0100 */ PTR(244, 2),
  /* 0101 */ PTR(248, 2),
  /* 0110 */ PTR(252, 2),
  /* 0111 */ PTR(256, 2),
  /* 1000 */ PTR(260, 2),
  /* 1001 */ PTR(264, 2),
  /* 1010 */ PTR(268, 2),
  /* 1011 */ PTR(272, 2),
  /* 1100 */ PTR(276, 2),
  /* 1101 */ PTR(280, 3),
  /* 1110 */ PTR(288, 2),
  /* 1111 */ PTR(292, 2),

  /* 0101 ... */
  /* 0000 */ PTR(296, 2),	/* 80 */
  /* 0001 */ PTR(300, 3),
  /* 0010 */ PTR(308, 2),
  /* 0011 */ PTR(312, 3),
  /* 0100 */ PTR(320, 1),
  /* 0101 */ PTR(322, 2),
  /* 0110 */ PTR(326, 2),
  /* 0111 */ PTR(330, 1),
  /* 1000 */ PTR(332, 2),
  /* 1001 */ PTR(336, 1),
  /* 1010 */ PTR(338, 1),
  /* 1011 */ PTR(340, 1),
  /* 1100 */ PTR(342, 1),
  /* 1101 */ PTR(344, 1),
  /* 1110 */ PTR(346, 1),
  /* 1111 */ PTR(348, 1),

  /* 0110 ... */
  /* 0000 */ PTR(350, 1),	/* 96 */
  /* 0001 */ PTR(352, 1),
  /* 0010 */ PTR(354, 1),
  /* 0011 */ PTR(356, 1),
  /* 0100 */ PTR(358, 1),
  /* 0101 */ PTR(360, 1),
  /* 0110 */ PTR(362, 1),
  /* 0111 */ PTR(364, 1),
  /* 1000 */ PTR(366, 1),
  /* 1001 */ PTR(368, 1),
  /* 1010 */ PTR(370, 2),
  /* 1011 */ PTR(374, 1),
  /* 1100 */ PTR(376, 2),
  /* 1101 */ V(7, 3, 4),
  /* 1110 */ PTR(380, 1),
  /* 1111 */ V(7, 2, 4),

  /* 0111 ... */
  /* 0000 */ V(4, 6, 4),	/* 112 */
  /* 0001 */ V(6, 4, 4),
  /* 0010 */ V(5, 5, 4),
  /* 0011 */ V(7, 1, 4),
  /* 0100 */ V(3, 6, 4),
  /* 0101 */ V(6, 3, 4),
  /* 0110 */ V(4, 5, 4),
  /* 0111 */ V(5, 4, 4),
  /* 1000 */ V(2, 6, 4),
  /* 1001 */ V(6, 2, 4),
  /* 1010 */ V(1, 6, 4),
  /* 1011 */ V(6, 1, 4),
  /* 1100 */ PTR(382, 1),
  /* 1101 */ V(3, 5, 4),
  /* 1110 */ V(5, 3, 4),
  /* 1111 */ V(4, 4, 4),

  /* 1000 ... */
  /* 0000 */ V(2, 5, 4),	/* 128 */
  /* 0001 */ V(5, 2, 4),
  /* 0010 */ V(1, 5, 4),
  /* 0011 */ PTR(384, 1),
  /* 0100 */ V(5, 1, 3),
  /* 0101 */ V(5, 1, 3),
  /* 0110 */ V(3, 4, 4),
  /* 0111 */ V(4, 3, 4),
  /* 1000 */ V(2, 4, 3),
  /* 1001 */ V(2, 4, 3),
  /* 1010 */ V(4, 2, 3),
  /* 1011 */ V(4, 2, 3),
  /* 1100 */ V(3, 3, 3),
  /* 1101 */ V(3, 3, 3),
  /* 1110 */ V(1, 4, 3),
  /* 1111 */ V(1, 4, 3),

  /* 1001 ... */
  /* 0000 */ V(4, 1, 3),	/* 144 */
  /* 0001 */ V(4, 1, 3),
  /* 0010 */ V(0, 4, 4),
  /* 0011 */ V(4, 0, 4),
  /* 0100 */ V(2, 3, 3),
  /* 0101 */ V(2, 3, 3),
  /* 0110 */ V(3, 2, 3),
  /* 0111 */ V(3, 2, 3),
  /* 1000 */ V(1, 3, 2),
  /* 1001 */ V(1, 3, 2),
  /* 1010 */ V(1, 3, 2),
  /* 1011 */ V(1, 3, 2),
  /* 1100 */ V(3, 1, 2),
  /* 1101 */ V(3, 1, 2),
  /* 1110 */ V(3, 1, 2),
  /* 1111 */ V(3, 1, 2),

  /* 1010 ... */
  /* 000  */ V(0, 3, 3),	/* 160 */
  /* 001  */ V(3, 0, 3),
  /* 010  */ V(2, 2, 2),
  /* 011  */ V(2, 2, 2),
  /* 100  */ V(1, 2, 1),
  /* 101  */ V(1, 2, 1),
  /* 110  */ V(1, 2, 1),
  /* 111  */ V(1, 2, 1),

  /* 1011 ... */
  /* 00   */ V(2, 1, 1),	/* 168 */
  /* 01   */ V(2, 1, 1),
  /* 10   */ V(0, 2, 2),
  /* 11   */ V(2, 0, 2),

  /* 0010 1100 ... */
  /* 000  */ V(0, 15, 1),	/* 172 */
  /* 001  */ V(0, 15, 1),
  /* 010  */ V(0, 15, 1),
  /* 011  */ V(0, 15, 1),
  /* 100  */ V(14, 14, 3),
  /* 101  */ V(13, 14, 3),
  /* 110  */ V(14, 13, 3),
  /* 111  */ V(12, 14, 3),

  /* 0010 1101 ... */
  /* 000  */ V(14, 12, 3),	/* 180 */
  /* 001  */ V(13, 13, 3),
  /* 010  */ V(11, 14, 3),
  /* 011  */ V(14, 11, 3),
  /* 100  */ V(12, 13, 3),
  /* 101  */ V(13, 12, 3),
  /* 110  */ V(10, 14, 3),
  /* 111  */ V(14, 10, 3),

  /* 0010 1110 ... */
  /* 000  */ V(11, 13, 3),	/* 188 */
  /* 001  */ V(13, 11, 3),
  /* 010  */ V(12, 12, 3),
  /* 011  */ V(9, 14, 3),
  /* 100  */ V(14, 9, 3),
  /* 101  */ V(10, 13, 3),
  /* 110  */ V(13, 10, 3),
  /* 111  */ V(11, 12, 3),

  /* 0010 1111 ... */
  /* 000  */ V(12, 11, 3),	/* 196 */
  /* 001  */ V(8, 14, 3),
  /* 010  */ V(14, 8, 3),
  /* 011  */ V(9, 13, 3),
  /* 100  */ V(13, 9, 3),
  /* 101  */ V(7, 14, 3),
  /* 110  */ V(14, 7, 3),
  /* 111  */ V(10, 12, 3),

  /* 0100 0000 ... */
  /* 0000 */ V(12, 10, 3),	/* 204 */
  /* 0001 */ V(12, 10, 3),
  /* 0010 */ V(11, 11, 3),
  /* 0011 */ V(11, 11, 3),
  /* 0100 */ V(8, 13, 3),
  /* 0101 */ V(8, 13, 3),
  /* 0110 */ V(13, 8, 3),
  /* 0111 */ V(13, 8, 3),
  /* 1000 */ V(0, 14, 4),
  /* 1001 */ V(14, 0, 4),
  /* 1010 */ V(0, 13, 3),
  /* 1011 */ V(0, 13, 3),
  /* 1100 */ V(14, 6, 2),
  /* 1101 */ V(14, 6, 2),
  /* 1110 */ V(14, 6, 2),
  /* 1111 */ V(14, 6, 2),

  /* 0100 0001 ... */
  /* 000  */ V(6, 14, 3),	/* 220 */
  /* 001  */ V(9, 12, 3),
  /* 010  */ V(12, 9, 2),
  /* 011  */ V(12, 9, 2),
  /* 100  */ V(5, 14, 2),
  /* 101  */ V(5, 14, 2),
  /* 110  */ V(11, 10, 2),
  /* 111  */ V(11, 10, 2),

  /* 0100 0010 ... */
  /* 000  */ V(14, 5, 2),	/* 228 */
  /* 001  */ V(14, 5, 2),
  /* 010  */ V(10, 11, 3),
  /* 011  */ V(7, 13, 3),
  /* 100  */ V(13, 7, 2),
  /* 101  */ V(13, 7, 2),
  /* 110  */ V(14, 4, 2),
  /* 111  */ V(14, 4, 2),

  /* 0100 0011 ... */
  /* 000  */ V(8, 12, 2),	/* 236 */
  /* 001  */ V(8, 12, 2),
  /* 010  */ V(12, 8, 2),
  /* 011  */ V(12, 8, 2),
  /* 100  */ V(4, 14, 3),
  /* 101  */ V(2, 14, 3),
  /* 110  */ V(3, 14, 2),
  /* 111  */ V(3, 14, 2),

  /* 0100 0100 ... */
  /* 00   */ V(6, 13, 2),	/* 244 */
  /* 01   */ V(13, 6, 2),
  /* 10   */ V(14, 3, 2),
  /* 11   */ V(9, 11, 2),

  /* 0100 0101 ... */
  /* 00   */ V(11, 9, 2),	/* 248 */
  /* 01   */ V(10, 10, 2),
  /* 10   */ V(14, 2, 2),
  /* 11   */ V(1, 14, 2),

  /* 0100 0110 ... */
  /* 00   */ V(14, 1, 2),	/* 252 */
  /* 01   */ V(5, 13, 2),
  /* 10   */ V(13, 5, 2),
  /* 11   */ V(7, 12, 2),

  /* 0100 0111 ... */
  /* 00   */ V(12, 7, 2),	/* 256 */
  /* 01   */ V(4, 13, 2),
  /* 10   */ V(8, 11, 2),
  /* 11   */ V(11, 8, 2),

  /* 0100 1000 ... */
  /* 00   */ V(13, 4, 2),	/* 260 */
  /* 01   */ V(9, 10, 2),
  /* 10   */ V(10, 9, 2),
  /* 11   */ V(6, 12, 2),

  /* 0100 1001 ... */
  /* 00   */ V(12, 6, 2),	/* 264 */
  /* 01   */ V(3, 13, 2),
  /* 10   */ V(13, 3, 2),
  /* 11   */ V(2, 13, 2),

  /* 0100 1010 ... */
  /* 00   */ V(13, 2, 2),	/* 268 */
  /* 01   */ V(1, 13, 2),
  /* 10   */ V(7, 11, 2),
  /* 11   */ V(11, 7, 2),

  /* 0100 1011 ... */
  /* 00   */ V(13, 1, 2),	/* 272 */
  /* 01   */ V(5, 12, 2),
  /* 10   */ V(12, 5, 2),
  /* 11   */ V(8, 10, 2),

  /* 0100 1100 ... */
  /* 00   */ V(10, 8, 2),	/* 276 */
  /* 01   */ V(9, 9, 2),
  /* 10   */ V(4, 12, 2),
  /* 11   */ V(12, 4, 2),

  /* 0100 1101 ... */
  /* 000  */ V(6, 11, 2),	/* 280 */
  /* 001  */ V(6, 11, 2),
  /* 010  */ V(11, 6, 2),
  /* 011  */ V(11, 6, 2),
  /* 100  */ V(13, 0, 3),
  /* 101  */ V(0, 12, 3),
  /* 110  */ V(3, 12, 2),
  /* 111  */ V(3, 12, 2),

  /* 0100 1110 ... */
  /* 00   */ V(12, 3, 2),	/* 288 */
  /* 01   */ V(7, 10, 2),
  /* 10   */ V(10, 7, 2),
  /* 11   */ V(2, 12, 2),

  /* 0100 1111 ... */
  /* 00   */ V(12, 2, 2),	/* 292 */
  /* 01   */ V(5, 11, 2),
  /* 10   */ V(11, 5, 2),
  /* 11   */ V(1, 12, 2),

  /* 0101 0000 ... */
  /* 00   */ V(8, 9, 2),	/* 296 */
  /* 01   */ V(9, 8, 2),
  /* 10   */ V(12, 1, 2),
  /* 11   */ V(4, 11, 2),

  /* 0101 0001 ... */
  /* 000  */ V(12, 0, 3),	/* 300 */
  /* 001  */ V(0, 11, 3),
  /* 010  */ V(3, 11, 2),
  /* 011  */ V(3, 11, 2),
  /* 100  */ V(11, 0, 3),
  /* 101  */ V(0, 10, 3),
  /* 110  */ V(1, 10, 2),
  /* 111  */ V(1, 10, 2),

  /* 0101 0010 ... */
  /* 00   */ V(11, 4, 1),	/* 308 */
  /* 01   */ V(11, 4, 1),
  /* 10   */ V(6, 10, 2),
  /* 11   */ V(10, 6, 2),

  /* 0101 0011 ... */
  /* 000  */ V(7, 9, 2),	/* 312 */
  /* 001  */ V(7, 9, 2),
  /* 010  */ V(9, 7, 2),
  /* 011  */ V(9, 7, 2),
  /* 100  */ V(10, 0, 3),
  /* 101  */ V(0, 9, 3),
  /* 110  */ V(9, 0, 2),
  /* 111  */ V(9, 0, 2),

  /* 0101 0100 ... */
  /* 0    */ V(11, 3, 1),	/* 320 */
  /* 1    */ V(8, 8, 1),

  /* 0101 0101 ... */
  /* 00   */ V(2, 11, 2),	/* 322 */
  /* 01   */ V(5, 10, 2),
  /* 10   */ V(11, 2, 1),
  /* 11   */ V(11, 2, 1),

  /* 0101 0110 ... */
  /* 00   */ V(10, 5, 2),	/* 326 */
  /* 01   */ V(1, 11, 2),
  /* 10   */ V(11, 1, 2),
  /* 11   */ V(6, 9, 2),

  /* 0101 0111 ... */
  /* 0    */ V(9, 6, 1),	/* 330 */
  /* 1    */ V(10, 4, 1),

  /* 0101 1000 ... */
  /* 00   */ V(4, 10, 2),	/* 332 */
  /* 01   */ V(7, 8, 2),
  /* 10   */ V(8, 7, 1),
  /* 11   */ V(8, 7, 1),

  /* 0101 1001 ... */
  /* 0    */ V(3, 10, 1),	/* 336 */
  /* 1    */ V(10, 3, 1),

  /* 0101 1010 ... */
  /* 0    */ V(5, 9, 1),	/* 338 */
  /* 1    */ V(9, 5, 1),

  /* 0101 1011 ... */
  /* 0    */ V(2, 10, 1),	/* 340 */
  /* 1    */ V(10, 2, 1),

  /* 0101 1100 ... */
  /* 0    */ V(10, 1, 1),	/* 342 */
  /* 1    */ V(6, 8, 1),

  /* 0101 1101 ... */
  /* 0    */ V(8, 6, 1),	/* 344 */
  /* 1    */ V(7, 7, 1),

  /* 0101 1110 ... */
  /* 0    */ V(4, 9, 1),	/* 346 */
  /* 1    */ V(9, 4, 1),

  /* 0101 1111 ... */
  /* 0    */ V(3, 9, 1),	/* 348 */
  /* 1    */ V(9, 3, 1),

  /* 0110 0000 ... */
  /* 0    */ V(5, 8, 1),	/* 350 */
  /* 1    */ V(8, 5, 1),

  /* 0110 0001 ... */
  /* 0    */ V(2, 9, 1),	/* 352 */
  /* 1    */ V(6, 7, 1),

  /* 0110 0010 ... */
  /* 0    */ V(7, 6, 1),	/* 354 */
  /* 1    */ V(9, 2, 1),

  /* 0110 0011 ... */
  /* 0    */ V(1, 9, 1),	/* 356 */
  /* 1    */ V(9, 1, 1),

  /* 0110 0100 ... */
  /* 0    */ V(4, 8, 1),	/* 358 */
  /* 1    */ V(8, 4, 1),

  /* 0110 0101 ... */
  /* 0    */ V(5, 7, 1),	/* 360 */
  /* 1    */ V(7, 5, 1),

  /* 0110 0110 ... */
  /* 0    */ V(3, 8, 1),	/* 362 */
  /* 1    */ V(8, 3, 1),

  /* 0110 0111 ... */
  /* 0    */ V(6, 6, 1),	/* 364 */
  /* 1    */ V(2, 8, 1),

  /* 0110 1000 ... */
  /* 0    */ V(8, 2, 1),	/* 366 */
  /* 1    */ V(1, 8, 1),

  /* 0110 1001 ... */
  /* 0    */ V(4, 7, 1),	/* 368 */
  /* 1    */ V(7, 4, 1),

  /* 0110 1010 ... */
  /* 00   */ V(8, 1, 1),	/* 370 */
  /* 01   */ V(8, 1, 1),
  /* 10   */ V(0, 8, 2),
  /* 11   */ V(8, 0, 2),

  /* 0110 1011 ... */
  /* 0    */ V(5, 6, 1),	/* 374 */
  /* 1    */ V(6, 5, 1),

  /* 0110 1100 ... */
  /* 00   */ V(1, 7, 1),	/* 376 */
  /* 01   */ V(1, 7, 1),
  /* 10   */ V(0, 7, 2),
  /* 11   */ V(7, 0, 2),

  /* 0110 1110 ... */
  /* 0    */ V(3, 7, 1),	/* 380  */
  /* 1    */ V(2, 7, 1),

  /* 0111 1100 ... */
  /* 0    */ V(0, 6, 1),	/* 382 */
  /* 1    */ V(6, 0, 1),

  /* 1000 0011 ... */
  /* 0    */ V(0, 5, 1),	/* 384 */
  /* 1    */ V(5, 0, 1)
};

# undef V
# undef PTR

/* external tables */

union huffquad const *const mad_huff_quad_table[2] = { hufftabA, hufftabB };

struct hufftable const mad_huff_pair_table[32] = {
  /*  0 */ { hufftab0,   0, 0 },
  /*  1 */ { hufftab1,   0, 3 },
  /*  2 */ { hufftab2,   0, 3 },
  /*  3 */ { hufftab3,   0, 3 },
  /*  4 */ { 0 /* not used */ },
  /*  5 */ { hufftab5,   0, 3 },
  /*  6 */ { hufftab6,   0, 4 },
  /*  7 */ { hufftab7,   0, 4 },
  /*  8 */ { hufftab8,   0, 4 },
  /*  9 */ { hufftab9,   0, 4 },
  /* 10 */ { hufftab10,  0, 4 },
  /* 11 */ { hufftab11,  0, 4 },
  /* 12 */ { hufftab12,  0, 4 },
  /* 13 */ { hufftab13,  0, 4 },
  /* 14 */ { 0 /* not used */ },
  /* 15 */ { hufftab15,  0, 4 },
  /* 16 */ { hufftab16,  1, 4 },
  /* 17 */ { hufftab16,  2, 4 },
  /* 18 */ { hufftab16,  3, 4 },
  /* 19 */ { hufftab16,  4, 4 },
  /* 20 */ { hufftab16,  6, 4 },
  /* 21 */ { hufftab16,  8, 4 },
  /* 22 */ { hufftab16, 10, 4 },
  /* 23 */ { hufftab16, 13, 4 },
  /* 24 */ { hufftab24,  4, 4 },
  /* 25 */ { hufftab24,  5, 4 },
  /* 26 */ { hufftab24,  6, 4 },
  /* 27 */ { hufftab24,  7, 4 },
  /* 28 */ { hufftab24,  8, 4 },
  /* 29 */ { hufftab24,  9, 4 },
  /* 30 */ { hufftab24, 11, 4 },
  /* 31 */ { hufftab24, 13, 4 }
};


// ***************************** huffman.c END *********************/





/* --- Layer III ----------------------------------------------------------- */

enum {
  count1table_select = 0x01,
  scalefac_scale     = 0x02,
  preflag	     = 0x04,
  mixed_block_flag   = 0x08
};

enum {
  I_STEREO  = 0x1,
  MS_STEREO = 0x2
};

struct channel {
      /* from side info */
      unsigned short part2_3_length;
      unsigned short big_values;
      unsigned short global_gain;
      unsigned short scalefac_compress;

      unsigned char flags;
      unsigned char block_type;
      unsigned char table_select[3];
      unsigned char subblock_gain[3];
      unsigned char region0_count;
      unsigned char region1_count;

      /* from main_data */
      unsigned char scalefac[39];	/* scalefac_l and/or scalefac_s */
} ;

struct granule {
    struct channel ch[2];
} ;


struct sideinfo {
  unsigned int main_data_begin;
  unsigned int private_bits;

  unsigned char scfsi[2];

  struct granule gr[2];
};

/*

struct sideinfo {
  unsigned int main_data_begin;
  unsigned int private_bits;

  unsigned char scfsi[2];

  struct granule {
    struct channel {

      unsigned short part2_3_length;
      unsigned short big_values;
      unsigned short global_gain;
      unsigned short scalefac_compress;

      unsigned char flags;
      unsigned char block_type;
      unsigned char table_select[3];
      unsigned char subblock_gain[3];
      unsigned char region0_count;
      unsigned char region1_count;

      
      unsigned char scalefac[39];
    } ch[2];
  } gr[2];
};

*/
/*
 * scalefactor bit lengths
 * derived from section 2.4.2.7 of ISO/IEC 11172-3
 */
static
struct {
  unsigned char slen1;
  unsigned char slen2;
} const sflen_table[16] = {
  { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 },
  { 3, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 },
  { 2, 1 }, { 2, 2 }, { 2, 3 }, { 3, 1 },
  { 3, 2 }, { 3, 3 }, { 4, 2 }, { 4, 3 }
};

/*
 * number of LSF scalefactor band values
 * derived from section 2.4.3.2 of ISO/IEC 13818-3
 */
static
unsigned char const nsfb_table[6][3][4] = {
  { {  6,  5,  5, 5 },
    {  9,  9,  9, 9 },
    {  6,  9,  9, 9 } },

  { {  6,  5,  7, 3 },
    {  9,  9, 12, 6 },
    {  6,  9, 12, 6 } },

  { { 11, 10,  0, 0 },
    { 18, 18,  0, 0 },
    { 15, 18,  0, 0 } },

  { {  7,  7,  7, 0 },
    { 12, 12, 12, 0 },
    {  6, 15, 12, 0 } },

  { {  6,  6,  6, 3 },
    { 12,  9,  9, 6 },
    {  6, 12,  9, 6 } },

  { {  8,  8,  5, 0 },
    { 15, 12,  9, 0 },
    {  6, 18,  9, 0 } }
};

/*
 * MPEG-1 scalefactor band widths
 * derived from Table B.8 of ISO/IEC 11172-3
 */
static
unsigned char const sfb_48000_long[] = {
   4,  4,  4,  4,  4,  4,  6,  6,  6,   8,  10,
  12, 16, 18, 22, 28, 34, 40, 46, 54,  54, 192
};

static
unsigned char const sfb_44100_long[] = {
   4,  4,  4,  4,  4,  4,  6,  6,  8,   8,  10,
  12, 16, 20, 24, 28, 34, 42, 50, 54,  76, 158
};

static
unsigned char const sfb_32000_long[] = {
   4,  4,  4,  4,  4,  4,  6,  6,  8,  10,  12,
  16, 20, 24, 30, 38, 46, 56, 68, 84, 102,  26
};

static
unsigned char const sfb_48000_short[] = {
   4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  6,
   6,  6,  6,  6,  6, 10, 10, 10, 12, 12, 12, 14, 14,
  14, 16, 16, 16, 20, 20, 20, 26, 26, 26, 66, 66, 66
};

static
unsigned char const sfb_44100_short[] = {
   4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  6,
   6,  6,  8,  8,  8, 10, 10, 10, 12, 12, 12, 14, 14,
  14, 18, 18, 18, 22, 22, 22, 30, 30, 30, 56, 56, 56
};

static
unsigned char const sfb_32000_short[] = {
   4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  6,
   6,  6,  8,  8,  8, 12, 12, 12, 16, 16, 16, 20, 20,
  20, 26, 26, 26, 34, 34, 34, 42, 42, 42, 12, 12, 12
};

static
unsigned char const sfb_48000_mixed[] = {
  /* long */   4,  4,  4,  4,  4,  4,  6,  6,
  /* short */  4,  4,  4,  6,  6,  6,  6,  6,  6, 10,
              10, 10, 12, 12, 12, 14, 14, 14, 16, 16,
              16, 20, 20, 20, 26, 26, 26, 66, 66, 66
};

static
unsigned char const sfb_44100_mixed[] = {
  /* long */   4,  4,  4,  4,  4,  4,  6,  6,
  /* short */  4,  4,  4,  6,  6,  6,  8,  8,  8, 10,
              10, 10, 12, 12, 12, 14, 14, 14, 18, 18,
              18, 22, 22, 22, 30, 30, 30, 56, 56, 56
};

static
unsigned char const sfb_32000_mixed[] = {
  /* long */   4,  4,  4,  4,  4,  4,  6,  6,
  /* short */  4,  4,  4,  6,  6,  6,  8,  8,  8, 12,
              12, 12, 16, 16, 16, 20, 20, 20, 26, 26,
              26, 34, 34, 34, 42, 42, 42, 12, 12, 12
};

/*
 * MPEG-2 scalefactor band widths
 * derived from Table B.2 of ISO/IEC 13818-3
 */
static
unsigned char const sfb_24000_long[] = {
   6,  6,  6,  6,  6,  6,  8, 10, 12,  14,  16,
  18, 22, 26, 32, 38, 46, 54, 62, 70,  76,  36
};

static
unsigned char const sfb_22050_long[] = {
   6,  6,  6,  6,  6,  6,  8, 10, 12,  14,  16,
  20, 24, 28, 32, 38, 46, 52, 60, 68,  58,  54
};

# define sfb_16000_long  sfb_22050_long

static
unsigned char const sfb_24000_short[] = {
   4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  8,
   8,  8, 10, 10, 10, 12, 12, 12, 14, 14, 14, 18, 18,
  18, 24, 24, 24, 32, 32, 32, 44, 44, 44, 12, 12, 12
};

static
unsigned char const sfb_22050_short[] = {
   4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  6,
   6,  6,  8,  8,  8, 10, 10, 10, 14, 14, 14, 18, 18,
  18, 26, 26, 26, 32, 32, 32, 42, 42, 42, 18, 18, 18
};

static
unsigned char const sfb_16000_short[] = {
   4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  8,
   8,  8, 10, 10, 10, 12, 12, 12, 14, 14, 14, 18, 18,
  18, 24, 24, 24, 30, 30, 30, 40, 40, 40, 18, 18, 18
};

static
unsigned char const sfb_24000_mixed[] = {
  /* long */   6,  6,  6,  6,  6,  6,
  /* short */  6,  6,  6,  8,  8,  8, 10, 10, 10, 12,
              12, 12, 14, 14, 14, 18, 18, 18, 24, 24,
              24, 32, 32, 32, 44, 44, 44, 12, 12, 12
};

static
unsigned char const sfb_22050_mixed[] = {
  /* long */   6,  6,  6,  6,  6,  6,
  /* short */  6,  6,  6,  6,  6,  6,  8,  8,  8, 10,
              10, 10, 14, 14, 14, 18, 18, 18, 26, 26,
              26, 32, 32, 32, 42, 42, 42, 18, 18, 18
};

static
unsigned char const sfb_16000_mixed[] = {
  /* long */   6,  6,  6,  6,  6,  6,
  /* short */  6,  6,  6,  8,  8,  8, 10, 10, 10, 12,
              12, 12, 14, 14, 14, 18, 18, 18, 24, 24,
              24, 30, 30, 30, 40, 40, 40, 18, 18, 18
};

/*
 * MPEG 2.5 scalefactor band widths
 * derived from public sources
 */
# define sfb_12000_long  sfb_16000_long
# define sfb_11025_long  sfb_12000_long

static
unsigned char const sfb_8000_long[] = {
  12, 12, 12, 12, 12, 12, 16, 20, 24,  28,  32,
  40, 48, 56, 64, 76, 90,  2,  2,  2,   2,   2
};

# define sfb_12000_short  sfb_16000_short
# define sfb_11025_short  sfb_12000_short

static
unsigned char const sfb_8000_short[] = {
   8,  8,  8,  8,  8,  8,  8,  8,  8, 12, 12, 12, 16,
  16, 16, 20, 20, 20, 24, 24, 24, 28, 28, 28, 36, 36,
  36,  2,  2,  2,  2,  2,  2,  2,  2,  2, 26, 26, 26
};

# define sfb_12000_mixed  sfb_16000_mixed
# define sfb_11025_mixed  sfb_12000_mixed

/* the 8000 Hz short block scalefactor bands do not break after
   the first 36 frequency lines, so this is probably wrong */
static
unsigned char const sfb_8000_mixed[] = {
  /* long */  12, 12, 12,
  /* short */  4,  4,  4,  8,  8,  8, 12, 12, 12, 16, 16, 16,
              20, 20, 20, 24, 24, 24, 28, 28, 28, 36, 36, 36,
               2,  2,  2,  2,  2,  2,  2,  2,  2, 26, 26, 26
};

static
struct {
  unsigned char const *l;
  unsigned char const *s;
  unsigned char const *m;
} const sfbwidth_table[9] = {
  { sfb_48000_long, sfb_48000_short, sfb_48000_mixed },
  { sfb_44100_long, sfb_44100_short, sfb_44100_mixed },
  { sfb_32000_long, sfb_32000_short, sfb_32000_mixed },
  { sfb_24000_long, sfb_24000_short, sfb_24000_mixed },
  { sfb_22050_long, sfb_22050_short, sfb_22050_mixed },
  { sfb_16000_long, sfb_16000_short, sfb_16000_mixed },
  { sfb_12000_long, sfb_12000_short, sfb_12000_mixed },
  { sfb_11025_long, sfb_11025_short, sfb_11025_mixed },
  {  sfb_8000_long,  sfb_8000_short,  sfb_8000_mixed }
};

/*
 * scalefactor band preemphasis (used only when preflag is set)
 * derived from Table B.6 of ISO/IEC 11172-3
 */
static
unsigned char const pretab[22] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0
};

/*
 * table for requantization
 *
 * rq_table[x].mantissa * 2^(rq_table[x].exponent) = x^(4/3)
 */
static
struct fixedfloat {
  unsigned long mantissa  : 27;
  unsigned short exponent :  5;
} const rq_table[8207] = {

  /*    0 */  { MAD_F(0x00000000) /* 0.000000000 */,  0 },
  /*    1 */  { MAD_F(0x04000000) /* 0.250000000 */,  2 },
  /*    2 */  { MAD_F(0x050a28be) /* 0.314980262 */,  3 },
  /*    3 */  { MAD_F(0x0453a5cd) /* 0.270421794 */,  4 },
  /*    4 */  { MAD_F(0x06597fa9) /* 0.396850263 */,  4 },
  /*    5 */  { MAD_F(0x04466275) /* 0.267183742 */,  5 },
  /*    6 */  { MAD_F(0x05738c72) /* 0.340710111 */,  5 },
  /*    7 */  { MAD_F(0x06b1fc81) /* 0.418453696 */,  5 },
  /*    8 */  { MAD_F(0x04000000) /* 0.250000000 */,  6 },
  /*    9 */  { MAD_F(0x04ae20d7) /* 0.292511788 */,  6 },
  /*   10 */  { MAD_F(0x0562d694) /* 0.336630420 */,  6 },
  /*   11 */  { MAD_F(0x061dae96) /* 0.382246578 */,  6 },
  /*   12 */  { MAD_F(0x06de47f4) /* 0.429267841 */,  6 },
  /*   13 */  { MAD_F(0x07a44f7a) /* 0.477614858 */,  6 },
  /*   14 */  { MAD_F(0x0437be65) /* 0.263609310 */,  7 },
  /*   15 */  { MAD_F(0x049fc824) /* 0.289009227 */,  7 },

  /*   16 */  { MAD_F(0x050a28be) /* 0.314980262 */,  7 },
  /*   17 */  { MAD_F(0x0576c6f5) /* 0.341498336 */,  7 },
  /*   18 */  { MAD_F(0x05e58c0b) /* 0.368541759 */,  7 },
  /*   19 */  { MAD_F(0x06566361) /* 0.396090870 */,  7 },
  /*   20 */  { MAD_F(0x06c93a2e) /* 0.424127753 */,  7 },
  /*   21 */  { MAD_F(0x073dff3e) /* 0.452635998 */,  7 },
  /*   22 */  { MAD_F(0x07b4a2bc) /* 0.481600510 */,  7 },
  /*   23 */  { MAD_F(0x04168b05) /* 0.255503674 */,  8 },
  /*   24 */  { MAD_F(0x0453a5cd) /* 0.270421794 */,  8 },
  /*   25 */  { MAD_F(0x04919b6a) /* 0.285548607 */,  8 },
  /*   26 */  { MAD_F(0x04d065fb) /* 0.300878507 */,  8 },
  /*   27 */  { MAD_F(0x05100000) /* 0.316406250 */,  8 },
  /*   28 */  { MAD_F(0x05506451) /* 0.332126919 */,  8 },
  /*   29 */  { MAD_F(0x05918e15) /* 0.348035890 */,  8 },
  /*   30 */  { MAD_F(0x05d378bb) /* 0.364128809 */,  8 },
  /*   31 */  { MAD_F(0x06161ff3) /* 0.380401563 */,  8 },

  /*   32 */  { MAD_F(0x06597fa9) /* 0.396850263 */,  8 },
  /*   33 */  { MAD_F(0x069d9400) /* 0.413471222 */,  8 },
  /*   34 */  { MAD_F(0x06e2594c) /* 0.430260942 */,  8 },
  /*   35 */  { MAD_F(0x0727cc11) /* 0.447216097 */,  8 },
  /*   36 */  { MAD_F(0x076de8fc) /* 0.464333519 */,  8 },
  /*   37 */  { MAD_F(0x07b4ace3) /* 0.481610189 */,  8 },
  /*   38 */  { MAD_F(0x07fc14bf) /* 0.499043224 */,  8 },
  /*   39 */  { MAD_F(0x04220ed7) /* 0.258314934 */,  9 },
  /*   40 */  { MAD_F(0x04466275) /* 0.267183742 */,  9 },
  /*   41 */  { MAD_F(0x046b03e7) /* 0.276126771 */,  9 },
  /*   42 */  { MAD_F(0x048ff1e8) /* 0.285142811 */,  9 },
  /*   43 */  { MAD_F(0x04b52b3f) /* 0.294230696 */,  9 },
  /*   44 */  { MAD_F(0x04daaec0) /* 0.303389310 */,  9 },
  /*   45 */  { MAD_F(0x05007b49) /* 0.312617576 */,  9 },
  /*   46 */  { MAD_F(0x05268fc6) /* 0.321914457 */,  9 },
  /*   47 */  { MAD_F(0x054ceb2a) /* 0.331278957 */,  9 },

  /*   48 */  { MAD_F(0x05738c72) /* 0.340710111 */,  9 },
  /*   49 */  { MAD_F(0x059a72a5) /* 0.350206992 */,  9 },
  /*   50 */  { MAD_F(0x05c19cd3) /* 0.359768701 */,  9 },
  /*   51 */  { MAD_F(0x05e90a12) /* 0.369394372 */,  9 },
  /*   52 */  { MAD_F(0x0610b982) /* 0.379083164 */,  9 },
  /*   53 */  { MAD_F(0x0638aa48) /* 0.388834268 */,  9 },
  /*   54 */  { MAD_F(0x0660db91) /* 0.398646895 */,  9 },
  /*   55 */  { MAD_F(0x06894c90) /* 0.408520284 */,  9 },
  /*   56 */  { MAD_F(0x06b1fc81) /* 0.418453696 */,  9 },
  /*   57 */  { MAD_F(0x06daeaa1) /* 0.428446415 */,  9 },
  /*   58 */  { MAD_F(0x07041636) /* 0.438497744 */,  9 },
  /*   59 */  { MAD_F(0x072d7e8b) /* 0.448607009 */,  9 },
  /*   60 */  { MAD_F(0x075722ef) /* 0.458773552 */,  9 },
  /*   61 */  { MAD_F(0x078102b8) /* 0.468996735 */,  9 },
  /*   62 */  { MAD_F(0x07ab1d3e) /* 0.479275937 */,  9 },
  /*   63 */  { MAD_F(0x07d571e0) /* 0.489610555 */,  9 },

  /*   64 */  { MAD_F(0x04000000) /* 0.250000000 */, 10 },
  /*   65 */  { MAD_F(0x04156381) /* 0.255221850 */, 10 },
  /*   66 */  { MAD_F(0x042ae32a) /* 0.260470548 */, 10 },
  /*   67 */  { MAD_F(0x04407eb1) /* 0.265745823 */, 10 },
  /*   68 */  { MAD_F(0x045635cf) /* 0.271047409 */, 10 },
  /*   69 */  { MAD_F(0x046c083e) /* 0.276375048 */, 10 },
  /*   70 */  { MAD_F(0x0481f5bb) /* 0.281728487 */, 10 },
  /*   71 */  { MAD_F(0x0497fe03) /* 0.287107481 */, 10 },
  /*   72 */  { MAD_F(0x04ae20d7) /* 0.292511788 */, 10 },
  /*   73 */  { MAD_F(0x04c45df6) /* 0.297941173 */, 10 },
  /*   74 */  { MAD_F(0x04dab524) /* 0.303395408 */, 10 },
  /*   75 */  { MAD_F(0x04f12624) /* 0.308874267 */, 10 },
  /*   76 */  { MAD_F(0x0507b0bc) /* 0.314377532 */, 10 },
  /*   77 */  { MAD_F(0x051e54b1) /* 0.319904987 */, 10 },
  /*   78 */  { MAD_F(0x053511cb) /* 0.325456423 */, 10 },
  /*   79 */  { MAD_F(0x054be7d4) /* 0.331031635 */, 10 },

  /*   80 */  { MAD_F(0x0562d694) /* 0.336630420 */, 10 },
  /*   81 */  { MAD_F(0x0579ddd8) /* 0.342252584 */, 10 },
  /*   82 */  { MAD_F(0x0590fd6c) /* 0.347897931 */, 10 },
  /*   83 */  { MAD_F(0x05a8351c) /* 0.353566275 */, 10 },
  /*   84 */  { MAD_F(0x05bf84b8) /* 0.359257429 */, 10 },
  /*   85 */  { MAD_F(0x05d6ec0e) /* 0.364971213 */, 10 },
  /*   86 */  { MAD_F(0x05ee6aef) /* 0.370707448 */, 10 },
  /*   87 */  { MAD_F(0x0606012b) /* 0.376465960 */, 10 },
  /*   88 */  { MAD_F(0x061dae96) /* 0.382246578 */, 10 },
  /*   89 */  { MAD_F(0x06357302) /* 0.388049134 */, 10 },
  /*   90 */  { MAD_F(0x064d4e43) /* 0.393873464 */, 10 },
  /*   91 */  { MAD_F(0x0665402d) /* 0.399719406 */, 10 },
  /*   92 */  { MAD_F(0x067d4896) /* 0.405586801 */, 10 },
  /*   93 */  { MAD_F(0x06956753) /* 0.411475493 */, 10 },
  /*   94 */  { MAD_F(0x06ad9c3d) /* 0.417385331 */, 10 },
  /*   95 */  { MAD_F(0x06c5e72b) /* 0.423316162 */, 10 },

  /*   96 */  { MAD_F(0x06de47f4) /* 0.429267841 */, 10 },
  /*   97 */  { MAD_F(0x06f6be73) /* 0.435240221 */, 10 },
  /*   98 */  { MAD_F(0x070f4a80) /* 0.441233161 */, 10 },
  /*   99 */  { MAD_F(0x0727ebf7) /* 0.447246519 */, 10 },
  /*  100 */  { MAD_F(0x0740a2b2) /* 0.453280160 */, 10 },
  /*  101 */  { MAD_F(0x07596e8d) /* 0.459333946 */, 10 },
  /*  102 */  { MAD_F(0x07724f64) /* 0.465407744 */, 10 },
  /*  103 */  { MAD_F(0x078b4514) /* 0.471501425 */, 10 },
  /*  104 */  { MAD_F(0x07a44f7a) /* 0.477614858 */, 10 },
  /*  105 */  { MAD_F(0x07bd6e75) /* 0.483747918 */, 10 },
  /*  106 */  { MAD_F(0x07d6a1e2) /* 0.489900479 */, 10 },
  /*  107 */  { MAD_F(0x07efe9a1) /* 0.496072418 */, 10 },
  /*  108 */  { MAD_F(0x0404a2c9) /* 0.251131807 */, 11 },
  /*  109 */  { MAD_F(0x04115aca) /* 0.254236974 */, 11 },
  /*  110 */  { MAD_F(0x041e1cc4) /* 0.257351652 */, 11 },
  /*  111 */  { MAD_F(0x042ae8a7) /* 0.260475783 */, 11 },

  /*  112 */  { MAD_F(0x0437be65) /* 0.263609310 */, 11 },
  /*  113 */  { MAD_F(0x04449dee) /* 0.266752177 */, 11 },
  /*  114 */  { MAD_F(0x04518733) /* 0.269904329 */, 11 },
  /*  115 */  { MAD_F(0x045e7a26) /* 0.273065710 */, 11 },
  /*  116 */  { MAD_F(0x046b76b9) /* 0.276236269 */, 11 },
  /*  117 */  { MAD_F(0x04787cdc) /* 0.279415952 */, 11 },
  /*  118 */  { MAD_F(0x04858c83) /* 0.282604707 */, 11 },
  /*  119 */  { MAD_F(0x0492a59f) /* 0.285802482 */, 11 },
  /*  120 */  { MAD_F(0x049fc824) /* 0.289009227 */, 11 },
  /*  121 */  { MAD_F(0x04acf402) /* 0.292224893 */, 11 },
  /*  122 */  { MAD_F(0x04ba292e) /* 0.295449429 */, 11 },
  /*  123 */  { MAD_F(0x04c7679a) /* 0.298682788 */, 11 },
  /*  124 */  { MAD_F(0x04d4af3a) /* 0.301924921 */, 11 },
  /*  125 */  { MAD_F(0x04e20000) /* 0.305175781 */, 11 },
  /*  126 */  { MAD_F(0x04ef59e0) /* 0.308435322 */, 11 },
  /*  127 */  { MAD_F(0x04fcbcce) /* 0.311703498 */, 11 },

  /*  128 */  { MAD_F(0x050a28be) /* 0.314980262 */, 11 },
  /*  129 */  { MAD_F(0x05179da4) /* 0.318265572 */, 11 },
  /*  130 */  { MAD_F(0x05251b73) /* 0.321559381 */, 11 },
  /*  131 */  { MAD_F(0x0532a220) /* 0.324861647 */, 11 },
  /*  132 */  { MAD_F(0x054031a0) /* 0.328172327 */, 11 },
  /*  133 */  { MAD_F(0x054dc9e7) /* 0.331491377 */, 11 },
  /*  134 */  { MAD_F(0x055b6ae9) /* 0.334818756 */, 11 },
  /*  135 */  { MAD_F(0x0569149c) /* 0.338154423 */, 11 },
  /*  136 */  { MAD_F(0x0576c6f5) /* 0.341498336 */, 11 },
  /*  137 */  { MAD_F(0x058481e9) /* 0.344850455 */, 11 },
  /*  138 */  { MAD_F(0x0592456d) /* 0.348210741 */, 11 },
  /*  139 */  { MAD_F(0x05a01176) /* 0.351579152 */, 11 },
  /*  140 */  { MAD_F(0x05ade5fa) /* 0.354955651 */, 11 },
  /*  141 */  { MAD_F(0x05bbc2ef) /* 0.358340200 */, 11 },
  /*  142 */  { MAD_F(0x05c9a84a) /* 0.361732758 */, 11 },
  /*  143 */  { MAD_F(0x05d79601) /* 0.365133291 */, 11 },

  /*  144 */  { MAD_F(0x05e58c0b) /* 0.368541759 */, 11 },
  /*  145 */  { MAD_F(0x05f38a5d) /* 0.371958126 */, 11 },
  /*  146 */  { MAD_F(0x060190ee) /* 0.375382356 */, 11 },
  /*  147 */  { MAD_F(0x060f9fb3) /* 0.378814413 */, 11 },
  /*  148 */  { MAD_F(0x061db6a5) /* 0.382254261 */, 11 },
  /*  149 */  { MAD_F(0x062bd5b8) /* 0.385701865 */, 11 },
  /*  150 */  { MAD_F(0x0639fce4) /* 0.389157191 */, 11 },
  /*  151 */  { MAD_F(0x06482c1f) /* 0.392620204 */, 11 },
  /*  152 */  { MAD_F(0x06566361) /* 0.396090870 */, 11 },
  /*  153 */  { MAD_F(0x0664a2a0) /* 0.399569155 */, 11 },
  /*  154 */  { MAD_F(0x0672e9d4) /* 0.403055027 */, 11 },
  /*  155 */  { MAD_F(0x068138f3) /* 0.406548452 */, 11 },
  /*  156 */  { MAD_F(0x068f8ff5) /* 0.410049398 */, 11 },
  /*  157 */  { MAD_F(0x069deed1) /* 0.413557833 */, 11 },
  /*  158 */  { MAD_F(0x06ac557f) /* 0.417073724 */, 11 },
  /*  159 */  { MAD_F(0x06bac3f6) /* 0.420597041 */, 11 },

  /*  160 */  { MAD_F(0x06c93a2e) /* 0.424127753 */, 11 },
  /*  161 */  { MAD_F(0x06d7b81f) /* 0.427665827 */, 11 },
  /*  162 */  { MAD_F(0x06e63dc0) /* 0.431211234 */, 11 },
  /*  163 */  { MAD_F(0x06f4cb09) /* 0.434763944 */, 11 },
  /*  164 */  { MAD_F(0x07035ff3) /* 0.438323927 */, 11 },
  /*  165 */  { MAD_F(0x0711fc75) /* 0.441891153 */, 11 },
  /*  166 */  { MAD_F(0x0720a087) /* 0.445465593 */, 11 },
  /*  167 */  { MAD_F(0x072f4c22) /* 0.449047217 */, 11 },
  /*  168 */  { MAD_F(0x073dff3e) /* 0.452635998 */, 11 },
  /*  169 */  { MAD_F(0x074cb9d3) /* 0.456231906 */, 11 },
  /*  170 */  { MAD_F(0x075b7bdb) /* 0.459834914 */, 11 },
  /*  171 */  { MAD_F(0x076a454c) /* 0.463444993 */, 11 },
  /*  172 */  { MAD_F(0x07791620) /* 0.467062117 */, 11 },
  /*  173 */  { MAD_F(0x0787ee50) /* 0.470686258 */, 11 },
  /*  174 */  { MAD_F(0x0796cdd4) /* 0.474317388 */, 11 },
  /*  175 */  { MAD_F(0x07a5b4a5) /* 0.477955481 */, 11 },

  /*  176 */  { MAD_F(0x07b4a2bc) /* 0.481600510 */, 11 },
  /*  177 */  { MAD_F(0x07c39812) /* 0.485252449 */, 11 },
  /*  178 */  { MAD_F(0x07d294a0) /* 0.488911273 */, 11 },
  /*  179 */  { MAD_F(0x07e1985f) /* 0.492576954 */, 11 },
  /*  180 */  { MAD_F(0x07f0a348) /* 0.496249468 */, 11 },
  /*  181 */  { MAD_F(0x07ffb554) /* 0.499928790 */, 11 },
  /*  182 */  { MAD_F(0x0407673f) /* 0.251807447 */, 12 },
  /*  183 */  { MAD_F(0x040ef75e) /* 0.253653877 */, 12 },
  /*  184 */  { MAD_F(0x04168b05) /* 0.255503674 */, 12 },
  /*  185 */  { MAD_F(0x041e2230) /* 0.257356825 */, 12 },
  /*  186 */  { MAD_F(0x0425bcdd) /* 0.259213318 */, 12 },
  /*  187 */  { MAD_F(0x042d5b07) /* 0.261073141 */, 12 },
  /*  188 */  { MAD_F(0x0434fcad) /* 0.262936282 */, 12 },
  /*  189 */  { MAD_F(0x043ca1c9) /* 0.264802730 */, 12 },
  /*  190 */  { MAD_F(0x04444a5a) /* 0.266672472 */, 12 },
  /*  191 */  { MAD_F(0x044bf65d) /* 0.268545497 */, 12 },

  /*  192 */  { MAD_F(0x0453a5cd) /* 0.270421794 */, 12 },
  /*  193 */  { MAD_F(0x045b58a9) /* 0.272301352 */, 12 },
  /*  194 */  { MAD_F(0x04630eed) /* 0.274184158 */, 12 },
  /*  195 */  { MAD_F(0x046ac896) /* 0.276070203 */, 12 },
  /*  196 */  { MAD_F(0x047285a2) /* 0.277959474 */, 12 },
  /*  197 */  { MAD_F(0x047a460c) /* 0.279851960 */, 12 },
  /*  198 */  { MAD_F(0x048209d3) /* 0.281747652 */, 12 },
  /*  199 */  { MAD_F(0x0489d0f4) /* 0.283646538 */, 12 },
  /*  200 */  { MAD_F(0x04919b6a) /* 0.285548607 */, 12 },
  /*  201 */  { MAD_F(0x04996935) /* 0.287453849 */, 12 },
  /*  202 */  { MAD_F(0x04a13a50) /* 0.289362253 */, 12 },
  /*  203 */  { MAD_F(0x04a90eba) /* 0.291273810 */, 12 },
  /*  204 */  { MAD_F(0x04b0e66e) /* 0.293188507 */, 12 },
  /*  205 */  { MAD_F(0x04b8c16c) /* 0.295106336 */, 12 },
  /*  206 */  { MAD_F(0x04c09faf) /* 0.297027285 */, 12 },
  /*  207 */  { MAD_F(0x04c88135) /* 0.298951346 */, 12 },

  /*  208 */  { MAD_F(0x04d065fb) /* 0.300878507 */, 12 },
  /*  209 */  { MAD_F(0x04d84dff) /* 0.302808759 */, 12 },
  /*  210 */  { MAD_F(0x04e0393e) /* 0.304742092 */, 12 },
  /*  211 */  { MAD_F(0x04e827b6) /* 0.306678497 */, 12 },
  /*  212 */  { MAD_F(0x04f01963) /* 0.308617963 */, 12 },
  /*  213 */  { MAD_F(0x04f80e44) /* 0.310560480 */, 12 },
  /*  214 */  { MAD_F(0x05000655) /* 0.312506041 */, 12 },
  /*  215 */  { MAD_F(0x05080195) /* 0.314454634 */, 12 },
  /*  216 */  { MAD_F(0x05100000) /* 0.316406250 */, 12 },
  /*  217 */  { MAD_F(0x05180194) /* 0.318360880 */, 12 },
  /*  218 */  { MAD_F(0x0520064f) /* 0.320318516 */, 12 },
  /*  219 */  { MAD_F(0x05280e2d) /* 0.322279147 */, 12 },
  /*  220 */  { MAD_F(0x0530192e) /* 0.324242764 */, 12 },
  /*  221 */  { MAD_F(0x0538274e) /* 0.326209359 */, 12 },
  /*  222 */  { MAD_F(0x0540388a) /* 0.328178922 */, 12 },
  /*  223 */  { MAD_F(0x05484ce2) /* 0.330151445 */, 12 },

  /*  224 */  { MAD_F(0x05506451) /* 0.332126919 */, 12 },
  /*  225 */  { MAD_F(0x05587ed5) /* 0.334105334 */, 12 },
  /*  226 */  { MAD_F(0x05609c6e) /* 0.336086683 */, 12 },
  /*  227 */  { MAD_F(0x0568bd17) /* 0.338070956 */, 12 },
  /*  228 */  { MAD_F(0x0570e0cf) /* 0.340058145 */, 12 },
  /*  229 */  { MAD_F(0x05790793) /* 0.342048241 */, 12 },
  /*  230 */  { MAD_F(0x05813162) /* 0.344041237 */, 12 },
  /*  231 */  { MAD_F(0x05895e39) /* 0.346037122 */, 12 },
  /*  232 */  { MAD_F(0x05918e15) /* 0.348035890 */, 12 },
  /*  233 */  { MAD_F(0x0599c0f4) /* 0.350037532 */, 12 },
  /*  234 */  { MAD_F(0x05a1f6d5) /* 0.352042040 */, 12 },
  /*  235 */  { MAD_F(0x05aa2fb5) /* 0.354049405 */, 12 },
  /*  236 */  { MAD_F(0x05b26b92) /* 0.356059619 */, 12 },
  /*  237 */  { MAD_F(0x05baaa69) /* 0.358072674 */, 12 },
  /*  238 */  { MAD_F(0x05c2ec39) /* 0.360088563 */, 12 },
  /*  239 */  { MAD_F(0x05cb3100) /* 0.362107278 */, 12 },

  /*  240 */  { MAD_F(0x05d378bb) /* 0.364128809 */, 12 },
  /*  241 */  { MAD_F(0x05dbc368) /* 0.366153151 */, 12 },
  /*  242 */  { MAD_F(0x05e41105) /* 0.368180294 */, 12 },
  /*  243 */  { MAD_F(0x05ec6190) /* 0.370210231 */, 12 },
  /*  244 */  { MAD_F(0x05f4b507) /* 0.372242955 */, 12 },
  /*  245 */  { MAD_F(0x05fd0b68) /* 0.374278458 */, 12 },
  /*  246 */  { MAD_F(0x060564b1) /* 0.376316732 */, 12 },
  /*  247 */  { MAD_F(0x060dc0e0) /* 0.378357769 */, 12 },
  /*  248 */  { MAD_F(0x06161ff3) /* 0.380401563 */, 12 },
  /*  249 */  { MAD_F(0x061e81e8) /* 0.382448106 */, 12 },
  /*  250 */  { MAD_F(0x0626e6bc) /* 0.384497391 */, 12 },
  /*  251 */  { MAD_F(0x062f4e6f) /* 0.386549409 */, 12 },
  /*  252 */  { MAD_F(0x0637b8fd) /* 0.388604155 */, 12 },
  /*  253 */  { MAD_F(0x06402666) /* 0.390661620 */, 12 },
  /*  254 */  { MAD_F(0x064896a7) /* 0.392721798 */, 12 },
  /*  255 */  { MAD_F(0x065109be) /* 0.394784681 */, 12 },

  /*  256 */  { MAD_F(0x06597fa9) /* 0.396850263 */, 12 },
  /*  257 */  { MAD_F(0x0661f867) /* 0.398918536 */, 12 },
  /*  258 */  { MAD_F(0x066a73f5) /* 0.400989493 */, 12 },
  /*  259 */  { MAD_F(0x0672f252) /* 0.403063128 */, 12 },
  /*  260 */  { MAD_F(0x067b737c) /* 0.405139433 */, 12 },
  /*  261 */  { MAD_F(0x0683f771) /* 0.407218402 */, 12 },
  /*  262 */  { MAD_F(0x068c7e2f) /* 0.409300027 */, 12 },
  /*  263 */  { MAD_F(0x069507b5) /* 0.411384303 */, 12 },
  /*  264 */  { MAD_F(0x069d9400) /* 0.413471222 */, 12 },
  /*  265 */  { MAD_F(0x06a6230f) /* 0.415560778 */, 12 },
  /*  266 */  { MAD_F(0x06aeb4e0) /* 0.417652964 */, 12 },
  /*  267 */  { MAD_F(0x06b74971) /* 0.419747773 */, 12 },
  /*  268 */  { MAD_F(0x06bfe0c0) /* 0.421845199 */, 12 },
  /*  269 */  { MAD_F(0x06c87acc) /* 0.423945235 */, 12 },
  /*  270 */  { MAD_F(0x06d11794) /* 0.426047876 */, 12 },
  /*  271 */  { MAD_F(0x06d9b714) /* 0.428153114 */, 12 },

  /*  272 */  { MAD_F(0x06e2594c) /* 0.430260942 */, 12 },
  /*  273 */  { MAD_F(0x06eafe3a) /* 0.432371356 */, 12 },
  /*  274 */  { MAD_F(0x06f3a5dc) /* 0.434484348 */, 12 },
  /*  275 */  { MAD_F(0x06fc5030) /* 0.436599912 */, 12 },
  /*  276 */  { MAD_F(0x0704fd35) /* 0.438718042 */, 12 },
  /*  277 */  { MAD_F(0x070dacea) /* 0.440838732 */, 12 },
  /*  278 */  { MAD_F(0x07165f4b) /* 0.442961975 */, 12 },
  /*  279 */  { MAD_F(0x071f1459) /* 0.445087765 */, 12 },
  /*  280 */  { MAD_F(0x0727cc11) /* 0.447216097 */, 12 },
  /*  281 */  { MAD_F(0x07308671) /* 0.449346964 */, 12 },
  /*  282 */  { MAD_F(0x07394378) /* 0.451480360 */, 12 },
  /*  283 */  { MAD_F(0x07420325) /* 0.453616280 */, 12 },
  /*  284 */  { MAD_F(0x074ac575) /* 0.455754717 */, 12 },
  /*  285 */  { MAD_F(0x07538a67) /* 0.457895665 */, 12 },
  /*  286 */  { MAD_F(0x075c51fa) /* 0.460039119 */, 12 },
  /*  287 */  { MAD_F(0x07651c2c) /* 0.462185072 */, 12 },

  /*  288 */  { MAD_F(0x076de8fc) /* 0.464333519 */, 12 },
  /*  289 */  { MAD_F(0x0776b867) /* 0.466484455 */, 12 },
  /*  290 */  { MAD_F(0x077f8a6d) /* 0.468637872 */, 12 },
  /*  291 */  { MAD_F(0x07885f0b) /* 0.470793767 */, 12 },
  /*  292 */  { MAD_F(0x07913641) /* 0.472952132 */, 12 },
  /*  293 */  { MAD_F(0x079a100c) /* 0.475112962 */, 12 },
  /*  294 */  { MAD_F(0x07a2ec6c) /* 0.477276252 */, 12 },
  /*  295 */  { MAD_F(0x07abcb5f) /* 0.479441997 */, 12 },
  /*  296 */  { MAD_F(0x07b4ace3) /* 0.481610189 */, 12 },
  /*  297 */  { MAD_F(0x07bd90f6) /* 0.483780825 */, 12 },
  /*  298 */  { MAD_F(0x07c67798) /* 0.485953899 */, 12 },
  /*  299 */  { MAD_F(0x07cf60c7) /* 0.488129404 */, 12 },
  /*  300 */  { MAD_F(0x07d84c81) /* 0.490307336 */, 12 },
  /*  301 */  { MAD_F(0x07e13ac5) /* 0.492487690 */, 12 },
  /*  302 */  { MAD_F(0x07ea2b92) /* 0.494670459 */, 12 },
  /*  303 */  { MAD_F(0x07f31ee6) /* 0.496855639 */, 12 },

  /*  304 */  { MAD_F(0x07fc14bf) /* 0.499043224 */, 12 },
  /*  305 */  { MAD_F(0x0402868e) /* 0.250616605 */, 13 },
  /*  306 */  { MAD_F(0x040703ff) /* 0.251712795 */, 13 },
  /*  307 */  { MAD_F(0x040b82b0) /* 0.252810180 */, 13 },
  /*  308 */  { MAD_F(0x041002a1) /* 0.253908756 */, 13 },
  /*  309 */  { MAD_F(0x041483d1) /* 0.255008523 */, 13 },
  /*  310 */  { MAD_F(0x04190640) /* 0.256109476 */, 13 },
  /*  311 */  { MAD_F(0x041d89ed) /* 0.257211614 */, 13 },
  /*  312 */  { MAD_F(0x04220ed7) /* 0.258314934 */, 13 },
  /*  313 */  { MAD_F(0x042694fe) /* 0.259419433 */, 13 },
  /*  314 */  { MAD_F(0x042b1c60) /* 0.260525110 */, 13 },
  /*  315 */  { MAD_F(0x042fa4fe) /* 0.261631960 */, 13 },
  /*  316 */  { MAD_F(0x04342ed7) /* 0.262739982 */, 13 },
  /*  317 */  { MAD_F(0x0438b9e9) /* 0.263849174 */, 13 },
  /*  318 */  { MAD_F(0x043d4635) /* 0.264959533 */, 13 },
  /*  319 */  { MAD_F(0x0441d3b9) /* 0.266071056 */, 13 },

  /*  320 */  { MAD_F(0x04466275) /* 0.267183742 */, 13 },
  /*  321 */  { MAD_F(0x044af269) /* 0.268297587 */, 13 },
  /*  322 */  { MAD_F(0x044f8393) /* 0.269412589 */, 13 },
  /*  323 */  { MAD_F(0x045415f3) /* 0.270528746 */, 13 },
  /*  324 */  { MAD_F(0x0458a989) /* 0.271646056 */, 13 },
  /*  325 */  { MAD_F(0x045d3e53) /* 0.272764515 */, 13 },
  /*  326 */  { MAD_F(0x0461d451) /* 0.273884123 */, 13 },
  /*  327 */  { MAD_F(0x04666b83) /* 0.275004875 */, 13 },
  /*  328 */  { MAD_F(0x046b03e7) /* 0.276126771 */, 13 },
  /*  329 */  { MAD_F(0x046f9d7e) /* 0.277249808 */, 13 },
  /*  330 */  { MAD_F(0x04743847) /* 0.278373983 */, 13 },
  /*  331 */  { MAD_F(0x0478d440) /* 0.279499294 */, 13 },
  /*  332 */  { MAD_F(0x047d716a) /* 0.280625739 */, 13 },
  /*  333 */  { MAD_F(0x04820fc3) /* 0.281753315 */, 13 },
  /*  334 */  { MAD_F(0x0486af4c) /* 0.282882021 */, 13 },
  /*  335 */  { MAD_F(0x048b5003) /* 0.284011853 */, 13 },

  /*  336 */  { MAD_F(0x048ff1e8) /* 0.285142811 */, 13 },
  /*  337 */  { MAD_F(0x049494fb) /* 0.286274891 */, 13 },
  /*  338 */  { MAD_F(0x0499393a) /* 0.287408091 */, 13 },
  /*  339 */  { MAD_F(0x049ddea5) /* 0.288542409 */, 13 },
  /*  340 */  { MAD_F(0x04a2853c) /* 0.289677844 */, 13 },
  /*  341 */  { MAD_F(0x04a72cfe) /* 0.290814392 */, 13 },
  /*  342 */  { MAD_F(0x04abd5ea) /* 0.291952051 */, 13 },
  /*  343 */  { MAD_F(0x04b08000) /* 0.293090820 */, 13 },
  /*  344 */  { MAD_F(0x04b52b3f) /* 0.294230696 */, 13 },
  /*  345 */  { MAD_F(0x04b9d7a7) /* 0.295371678 */, 13 },
  /*  346 */  { MAD_F(0x04be8537) /* 0.296513762 */, 13 },
  /*  347 */  { MAD_F(0x04c333ee) /* 0.297656947 */, 13 },
  /*  348 */  { MAD_F(0x04c7e3cc) /* 0.298801231 */, 13 },
  /*  349 */  { MAD_F(0x04cc94d1) /* 0.299946611 */, 13 },
  /*  350 */  { MAD_F(0x04d146fb) /* 0.301093085 */, 13 },
  /*  351 */  { MAD_F(0x04d5fa4b) /* 0.302240653 */, 13 },

  /*  352 */  { MAD_F(0x04daaec0) /* 0.303389310 */, 13 },
  /*  353 */  { MAD_F(0x04df6458) /* 0.304539056 */, 13 },
  /*  354 */  { MAD_F(0x04e41b14) /* 0.305689888 */, 13 },
  /*  355 */  { MAD_F(0x04e8d2f3) /* 0.306841804 */, 13 },
  /*  356 */  { MAD_F(0x04ed8bf5) /* 0.307994802 */, 13 },
  /*  357 */  { MAD_F(0x04f24618) /* 0.309148880 */, 13 },
  /*  358 */  { MAD_F(0x04f7015d) /* 0.310304037 */, 13 },
  /*  359 */  { MAD_F(0x04fbbdc3) /* 0.311460269 */, 13 },
  /*  360 */  { MAD_F(0x05007b49) /* 0.312617576 */, 13 },
  /*  361 */  { MAD_F(0x050539ef) /* 0.313775954 */, 13 },
  /*  362 */  { MAD_F(0x0509f9b4) /* 0.314935403 */, 13 },
  /*  363 */  { MAD_F(0x050eba98) /* 0.316095920 */, 13 },
  /*  364 */  { MAD_F(0x05137c9a) /* 0.317257503 */, 13 },
  /*  365 */  { MAD_F(0x05183fba) /* 0.318420150 */, 13 },
  /*  366 */  { MAD_F(0x051d03f7) /* 0.319583859 */, 13 },
  /*  367 */  { MAD_F(0x0521c950) /* 0.320748629 */, 13 },

  /*  368 */  { MAD_F(0x05268fc6) /* 0.321914457 */, 13 },
  /*  369 */  { MAD_F(0x052b5757) /* 0.323081342 */, 13 },
  /*  370 */  { MAD_F(0x05302003) /* 0.324249281 */, 13 },
  /*  371 */  { MAD_F(0x0534e9ca) /* 0.325418273 */, 13 },
  /*  372 */  { MAD_F(0x0539b4ab) /* 0.326588316 */, 13 },
  /*  373 */  { MAD_F(0x053e80a6) /* 0.327759407 */, 13 },
  /*  374 */  { MAD_F(0x05434db9) /* 0.328931546 */, 13 },
  /*  375 */  { MAD_F(0x05481be5) /* 0.330104730 */, 13 },
  /*  376 */  { MAD_F(0x054ceb2a) /* 0.331278957 */, 13 },
  /*  377 */  { MAD_F(0x0551bb85) /* 0.332454225 */, 13 },
  /*  378 */  { MAD_F(0x05568cf8) /* 0.333630533 */, 13 },
  /*  379 */  { MAD_F(0x055b5f81) /* 0.334807879 */, 13 },
  /*  380 */  { MAD_F(0x05603321) /* 0.335986261 */, 13 },
  /*  381 */  { MAD_F(0x056507d6) /* 0.337165677 */, 13 },
  /*  382 */  { MAD_F(0x0569dda0) /* 0.338346125 */, 13 },
  /*  383 */  { MAD_F(0x056eb47f) /* 0.339527604 */, 13 },

  /*  384 */  { MAD_F(0x05738c72) /* 0.340710111 */, 13 },
  /*  385 */  { MAD_F(0x05786578) /* 0.341893646 */, 13 },
  /*  386 */  { MAD_F(0x057d3f92) /* 0.343078205 */, 13 },
  /*  387 */  { MAD_F(0x05821abf) /* 0.344263788 */, 13 },
  /*  388 */  { MAD_F(0x0586f6fd) /* 0.345450393 */, 13 },
  /*  389 */  { MAD_F(0x058bd44e) /* 0.346638017 */, 13 },
  /*  390 */  { MAD_F(0x0590b2b0) /* 0.347826659 */, 13 },
  /*  391 */  { MAD_F(0x05959222) /* 0.349016318 */, 13 },
  /*  392 */  { MAD_F(0x059a72a5) /* 0.350206992 */, 13 },
  /*  393 */  { MAD_F(0x059f5438) /* 0.351398678 */, 13 },
  /*  394 */  { MAD_F(0x05a436da) /* 0.352591376 */, 13 },
  /*  395 */  { MAD_F(0x05a91a8c) /* 0.353785083 */, 13 },
  /*  396 */  { MAD_F(0x05adff4c) /* 0.354979798 */, 13 },
  /*  397 */  { MAD_F(0x05b2e51a) /* 0.356175519 */, 13 },
  /*  398 */  { MAD_F(0x05b7cbf5) /* 0.357372244 */, 13 },
  /*  399 */  { MAD_F(0x05bcb3de) /* 0.358569972 */, 13 },

  /*  400 */  { MAD_F(0x05c19cd3) /* 0.359768701 */, 13 },
  /*  401 */  { MAD_F(0x05c686d5) /* 0.360968429 */, 13 },
  /*  402 */  { MAD_F(0x05cb71e2) /* 0.362169156 */, 13 },
  /*  403 */  { MAD_F(0x05d05dfb) /* 0.363370878 */, 13 },
  /*  404 */  { MAD_F(0x05d54b1f) /* 0.364573594 */, 13 },
  /*  405 */  { MAD_F(0x05da394d) /* 0.365777304 */, 13 },
  /*  406 */  { MAD_F(0x05df2885) /* 0.366982004 */, 13 },
  /*  407 */  { MAD_F(0x05e418c7) /* 0.368187694 */, 13 },
  /*  408 */  { MAD_F(0x05e90a12) /* 0.369394372 */, 13 },
  /*  409 */  { MAD_F(0x05edfc66) /* 0.370602036 */, 13 },
  /*  410 */  { MAD_F(0x05f2efc2) /* 0.371810684 */, 13 },
  /*  411 */  { MAD_F(0x05f7e426) /* 0.373020316 */, 13 },
  /*  412 */  { MAD_F(0x05fcd992) /* 0.374230929 */, 13 },
  /*  413 */  { MAD_F(0x0601d004) /* 0.375442522 */, 13 },
  /*  414 */  { MAD_F(0x0606c77d) /* 0.376655093 */, 13 },
  /*  415 */  { MAD_F(0x060bbffd) /* 0.377868641 */, 13 },

  /*  416 */  { MAD_F(0x0610b982) /* 0.379083164 */, 13 },
  /*  417 */  { MAD_F(0x0615b40c) /* 0.380298661 */, 13 },
  /*  418 */  { MAD_F(0x061aaf9c) /* 0.381515130 */, 13 },
  /*  419 */  { MAD_F(0x061fac2f) /* 0.382732569 */, 13 },
  /*  420 */  { MAD_F(0x0624a9c7) /* 0.383950977 */, 13 },
  /*  421 */  { MAD_F(0x0629a863) /* 0.385170352 */, 13 },
  /*  422 */  { MAD_F(0x062ea802) /* 0.386390694 */, 13 },
  /*  423 */  { MAD_F(0x0633a8a3) /* 0.387611999 */, 13 },
  /*  424 */  { MAD_F(0x0638aa48) /* 0.388834268 */, 13 },
  /*  425 */  { MAD_F(0x063dacee) /* 0.390057497 */, 13 },
  /*  426 */  { MAD_F(0x0642b096) /* 0.391281687 */, 13 },
  /*  427 */  { MAD_F(0x0647b53f) /* 0.392506834 */, 13 },
  /*  428 */  { MAD_F(0x064cbae9) /* 0.393732939 */, 13 },
  /*  429 */  { MAD_F(0x0651c193) /* 0.394959999 */, 13 },
  /*  430 */  { MAD_F(0x0656c93d) /* 0.396188012 */, 13 },
  /*  431 */  { MAD_F(0x065bd1e7) /* 0.397416978 */, 13 },

  /*  432 */  { MAD_F(0x0660db91) /* 0.398646895 */, 13 },
  /*  433 */  { MAD_F(0x0665e639) /* 0.399877761 */, 13 },
  /*  434 */  { MAD_F(0x066af1df) /* 0.401109575 */, 13 },
  /*  435 */  { MAD_F(0x066ffe84) /* 0.402342335 */, 13 },
  /*  436 */  { MAD_F(0x06750c26) /* 0.403576041 */, 13 },
  /*  437 */  { MAD_F(0x067a1ac6) /* 0.404810690 */, 13 },
  /*  438 */  { MAD_F(0x067f2a62) /* 0.406046281 */, 13 },
  /*  439 */  { MAD_F(0x06843afb) /* 0.407282813 */, 13 },
  /*  440 */  { MAD_F(0x06894c90) /* 0.408520284 */, 13 },
  /*  441 */  { MAD_F(0x068e5f21) /* 0.409758693 */, 13 },
  /*  442 */  { MAD_F(0x069372ae) /* 0.410998038 */, 13 },
  /*  443 */  { MAD_F(0x06988735) /* 0.412238319 */, 13 },
  /*  444 */  { MAD_F(0x069d9cb7) /* 0.413479532 */, 13 },
  /*  445 */  { MAD_F(0x06a2b333) /* 0.414721679 */, 13 },
  /*  446 */  { MAD_F(0x06a7caa9) /* 0.415964756 */, 13 },
  /*  447 */  { MAD_F(0x06ace318) /* 0.417208762 */, 13 },

  /*  448 */  { MAD_F(0x06b1fc81) /* 0.418453696 */, 13 },
  /*  449 */  { MAD_F(0x06b716e2) /* 0.419699557 */, 13 },
  /*  450 */  { MAD_F(0x06bc323b) /* 0.420946343 */, 13 },
  /*  451 */  { MAD_F(0x06c14e8d) /* 0.422194054 */, 13 },
  /*  452 */  { MAD_F(0x06c66bd6) /* 0.423442686 */, 13 },
  /*  453 */  { MAD_F(0x06cb8a17) /* 0.424692240 */, 13 },
  /*  454 */  { MAD_F(0x06d0a94e) /* 0.425942714 */, 13 },
  /*  455 */  { MAD_F(0x06d5c97c) /* 0.427194106 */, 13 },
  /*  456 */  { MAD_F(0x06daeaa1) /* 0.428446415 */, 13 },
  /*  457 */  { MAD_F(0x06e00cbb) /* 0.429699640 */, 13 },
  /*  458 */  { MAD_F(0x06e52fca) /* 0.430953779 */, 13 },
  /*  459 */  { MAD_F(0x06ea53cf) /* 0.432208832 */, 13 },
  /*  460 */  { MAD_F(0x06ef78c8) /* 0.433464796 */, 13 },
  /*  461 */  { MAD_F(0x06f49eb6) /* 0.434721671 */, 13 },
  /*  462 */  { MAD_F(0x06f9c597) /* 0.435979455 */, 13 },
  /*  463 */  { MAD_F(0x06feed6d) /* 0.437238146 */, 13 },

  /*  464 */  { MAD_F(0x07041636) /* 0.438497744 */, 13 },
  /*  465 */  { MAD_F(0x07093ff2) /* 0.439758248 */, 13 },
  /*  466 */  { MAD_F(0x070e6aa0) /* 0.441019655 */, 13 },
  /*  467 */  { MAD_F(0x07139641) /* 0.442281965 */, 13 },
  /*  468 */  { MAD_F(0x0718c2d3) /* 0.443545176 */, 13 },
  /*  469 */  { MAD_F(0x071df058) /* 0.444809288 */, 13 },
  /*  470 */  { MAD_F(0x07231ecd) /* 0.446074298 */, 13 },
  /*  471 */  { MAD_F(0x07284e34) /* 0.447340205 */, 13 },
  /*  472 */  { MAD_F(0x072d7e8b) /* 0.448607009 */, 13 },
  /*  473 */  { MAD_F(0x0732afd2) /* 0.449874708 */, 13 },
  /*  474 */  { MAD_F(0x0737e209) /* 0.451143300 */, 13 },
  /*  475 */  { MAD_F(0x073d1530) /* 0.452412785 */, 13 },
  /*  476 */  { MAD_F(0x07424946) /* 0.453683161 */, 13 },
  /*  477 */  { MAD_F(0x07477e4b) /* 0.454954427 */, 13 },
  /*  478 */  { MAD_F(0x074cb43e) /* 0.456226581 */, 13 },
  /*  479 */  { MAD_F(0x0751eb20) /* 0.457499623 */, 13 },

  /*  480 */  { MAD_F(0x075722ef) /* 0.458773552 */, 13 },
  /*  481 */  { MAD_F(0x075c5bac) /* 0.460048365 */, 13 },
  /*  482 */  { MAD_F(0x07619557) /* 0.461324062 */, 13 },
  /*  483 */  { MAD_F(0x0766cfee) /* 0.462600642 */, 13 },
  /*  484 */  { MAD_F(0x076c0b72) /* 0.463878102 */, 13 },
  /*  485 */  { MAD_F(0x077147e2) /* 0.465156443 */, 13 },
  /*  486 */  { MAD_F(0x0776853e) /* 0.466435663 */, 13 },
  /*  487 */  { MAD_F(0x077bc385) /* 0.467715761 */, 13 },
  /*  488 */  { MAD_F(0x078102b8) /* 0.468996735 */, 13 },
  /*  489 */  { MAD_F(0x078642d6) /* 0.470278584 */, 13 },
  /*  490 */  { MAD_F(0x078b83de) /* 0.471561307 */, 13 },
  /*  491 */  { MAD_F(0x0790c5d1) /* 0.472844904 */, 13 },
  /*  492 */  { MAD_F(0x079608ae) /* 0.474129372 */, 13 },
  /*  493 */  { MAD_F(0x079b4c74) /* 0.475414710 */, 13 },
  /*  494 */  { MAD_F(0x07a09124) /* 0.476700918 */, 13 },
  /*  495 */  { MAD_F(0x07a5d6bd) /* 0.477987994 */, 13 },

  /*  496 */  { MAD_F(0x07ab1d3e) /* 0.479275937 */, 13 },
  /*  497 */  { MAD_F(0x07b064a8) /* 0.480564746 */, 13 },
  /*  498 */  { MAD_F(0x07b5acfb) /* 0.481854420 */, 13 },
  /*  499 */  { MAD_F(0x07baf635) /* 0.483144957 */, 13 },
  /*  500 */  { MAD_F(0x07c04056) /* 0.484436356 */, 13 },
  /*  501 */  { MAD_F(0x07c58b5f) /* 0.485728617 */, 13 },
  /*  502 */  { MAD_F(0x07cad74e) /* 0.487021738 */, 13 },
  /*  503 */  { MAD_F(0x07d02424) /* 0.488315717 */, 13 },
  /*  504 */  { MAD_F(0x07d571e0) /* 0.489610555 */, 13 },
  /*  505 */  { MAD_F(0x07dac083) /* 0.490906249 */, 13 },
  /*  506 */  { MAD_F(0x07e0100a) /* 0.492202799 */, 13 },
  /*  507 */  { MAD_F(0x07e56078) /* 0.493500203 */, 13 },
  /*  508 */  { MAD_F(0x07eab1ca) /* 0.494798460 */, 13 },
  /*  509 */  { MAD_F(0x07f00401) /* 0.496097570 */, 13 },
  /*  510 */  { MAD_F(0x07f5571d) /* 0.497397530 */, 13 },
  /*  511 */  { MAD_F(0x07faab1c) /* 0.498698341 */, 13 },

  /*  512 */  { MAD_F(0x04000000) /* 0.250000000 */, 14 },
  /*  513 */  { MAD_F(0x0402aae3) /* 0.250651254 */, 14 },
  /*  514 */  { MAD_F(0x04055638) /* 0.251302930 */, 14 },
  /*  515 */  { MAD_F(0x040801ff) /* 0.251955030 */, 14 },
  /*  516 */  { MAD_F(0x040aae37) /* 0.252607552 */, 14 },
  /*  517 */  { MAD_F(0x040d5ae0) /* 0.253260495 */, 14 },
  /*  518 */  { MAD_F(0x041007fa) /* 0.253913860 */, 14 },
  /*  519 */  { MAD_F(0x0412b586) /* 0.254567645 */, 14 },
  /*  520 */  { MAD_F(0x04156381) /* 0.255221850 */, 14 },
  /*  521 */  { MAD_F(0x041811ee) /* 0.255876475 */, 14 },
  /*  522 */  { MAD_F(0x041ac0cb) /* 0.256531518 */, 14 },
  /*  523 */  { MAD_F(0x041d7018) /* 0.257186980 */, 14 },
  /*  524 */  { MAD_F(0x04201fd5) /* 0.257842860 */, 14 },
  /*  525 */  { MAD_F(0x0422d003) /* 0.258499157 */, 14 },
  /*  526 */  { MAD_F(0x042580a0) /* 0.259155872 */, 14 },
  /*  527 */  { MAD_F(0x042831ad) /* 0.259813002 */, 14 },

  /*  528 */  { MAD_F(0x042ae32a) /* 0.260470548 */, 14 },
  /*  529 */  { MAD_F(0x042d9516) /* 0.261128510 */, 14 },
  /*  530 */  { MAD_F(0x04304772) /* 0.261786886 */, 14 },
  /*  531 */  { MAD_F(0x0432fa3d) /* 0.262445676 */, 14 },
  /*  532 */  { MAD_F(0x0435ad76) /* 0.263104880 */, 14 },
  /*  533 */  { MAD_F(0x0438611f) /* 0.263764497 */, 14 },
  /*  534 */  { MAD_F(0x043b1536) /* 0.264424527 */, 14 },
  /*  535 */  { MAD_F(0x043dc9bc) /* 0.265084969 */, 14 },
  /*  536 */  { MAD_F(0x04407eb1) /* 0.265745823 */, 14 },
  /*  537 */  { MAD_F(0x04433414) /* 0.266407088 */, 14 },
  /*  538 */  { MAD_F(0x0445e9e5) /* 0.267068763 */, 14 },
  /*  539 */  { MAD_F(0x0448a024) /* 0.267730848 */, 14 },
  /*  540 */  { MAD_F(0x044b56d1) /* 0.268393343 */, 14 },
  /*  541 */  { MAD_F(0x044e0dec) /* 0.269056248 */, 14 },
  /*  542 */  { MAD_F(0x0450c575) /* 0.269719560 */, 14 },
  /*  543 */  { MAD_F(0x04537d6b) /* 0.270383281 */, 14 },

  /*  544 */  { MAD_F(0x045635cf) /* 0.271047409 */, 14 },
  /*  545 */  { MAD_F(0x0458ee9f) /* 0.271711944 */, 14 },
  /*  546 */  { MAD_F(0x045ba7dd) /* 0.272376886 */, 14 },
  /*  547 */  { MAD_F(0x045e6188) /* 0.273042234 */, 14 },
  /*  548 */  { MAD_F(0x04611ba0) /* 0.273707988 */, 14 },
  /*  549 */  { MAD_F(0x0463d625) /* 0.274374147 */, 14 },
  /*  550 */  { MAD_F(0x04669116) /* 0.275040710 */, 14 },
  /*  551 */  { MAD_F(0x04694c74) /* 0.275707677 */, 14 },
  /*  552 */  { MAD_F(0x046c083e) /* 0.276375048 */, 14 },
  /*  553 */  { MAD_F(0x046ec474) /* 0.277042822 */, 14 },
  /*  554 */  { MAD_F(0x04718116) /* 0.277710999 */, 14 },
  /*  555 */  { MAD_F(0x04743e25) /* 0.278379578 */, 14 },
  /*  556 */  { MAD_F(0x0476fb9f) /* 0.279048558 */, 14 },
  /*  557 */  { MAD_F(0x0479b984) /* 0.279717940 */, 14 },
  /*  558 */  { MAD_F(0x047c77d6) /* 0.280387722 */, 14 },
  /*  559 */  { MAD_F(0x047f3693) /* 0.281057905 */, 14 },

  /*  560 */  { MAD_F(0x0481f5bb) /* 0.281728487 */, 14 },
  /*  561 */  { MAD_F(0x0484b54e) /* 0.282399469 */, 14 },
  /*  562 */  { MAD_F(0x0487754c) /* 0.283070849 */, 14 },
  /*  563 */  { MAD_F(0x048a35b6) /* 0.283742628 */, 14 },
  /*  564 */  { MAD_F(0x048cf68a) /* 0.284414805 */, 14 },
  /*  565 */  { MAD_F(0x048fb7c8) /* 0.285087379 */, 14 },
  /*  566 */  { MAD_F(0x04927972) /* 0.285760350 */, 14 },
  /*  567 */  { MAD_F(0x04953b85) /* 0.286433717 */, 14 },
  /*  568 */  { MAD_F(0x0497fe03) /* 0.287107481 */, 14 },
  /*  569 */  { MAD_F(0x049ac0eb) /* 0.287781640 */, 14 },
  /*  570 */  { MAD_F(0x049d843e) /* 0.288456194 */, 14 },
  /*  571 */  { MAD_F(0x04a047fa) /* 0.289131142 */, 14 },
  /*  572 */  { MAD_F(0x04a30c20) /* 0.289806485 */, 14 },
  /*  573 */  { MAD_F(0x04a5d0af) /* 0.290482221 */, 14 },
  /*  574 */  { MAD_F(0x04a895a8) /* 0.291158351 */, 14 },
  /*  575 */  { MAD_F(0x04ab5b0b) /* 0.291834873 */, 14 },

  /*  576 */  { MAD_F(0x04ae20d7) /* 0.292511788 */, 14 },
  /*  577 */  { MAD_F(0x04b0e70c) /* 0.293189094 */, 14 },
  /*  578 */  { MAD_F(0x04b3adaa) /* 0.293866792 */, 14 },
  /*  579 */  { MAD_F(0x04b674b1) /* 0.294544881 */, 14 },
  /*  580 */  { MAD_F(0x04b93c21) /* 0.295223360 */, 14 },
  /*  581 */  { MAD_F(0x04bc03fa) /* 0.295902229 */, 14 },
  /*  582 */  { MAD_F(0x04becc3b) /* 0.296581488 */, 14 },
  /*  583 */  { MAD_F(0x04c194e4) /* 0.297261136 */, 14 },
  /*  584 */  { MAD_F(0x04c45df6) /* 0.297941173 */, 14 },
  /*  585 */  { MAD_F(0x04c72771) /* 0.298621598 */, 14 },
  /*  586 */  { MAD_F(0x04c9f153) /* 0.299302411 */, 14 },
  /*  587 */  { MAD_F(0x04ccbb9d) /* 0.299983611 */, 14 },
  /*  588 */  { MAD_F(0x04cf864f) /* 0.300665198 */, 14 },
  /*  589 */  { MAD_F(0x04d25169) /* 0.301347172 */, 14 },
  /*  590 */  { MAD_F(0x04d51ceb) /* 0.302029532 */, 14 },
  /*  591 */  { MAD_F(0x04d7e8d4) /* 0.302712277 */, 14 },

  /*  592 */  { MAD_F(0x04dab524) /* 0.303395408 */, 14 },
  /*  593 */  { MAD_F(0x04dd81dc) /* 0.304078923 */, 14 },
  /*  594 */  { MAD_F(0x04e04efb) /* 0.304762823 */, 14 },
  /*  595 */  { MAD_F(0x04e31c81) /* 0.305447106 */, 14 },
  /*  596 */  { MAD_F(0x04e5ea6e) /* 0.306131773 */, 14 },
  /*  597 */  { MAD_F(0x04e8b8c2) /* 0.306816823 */, 14 },
  /*  598 */  { MAD_F(0x04eb877c) /* 0.307502256 */, 14 },
  /*  599 */  { MAD_F(0x04ee569d) /* 0.308188071 */, 14 },
  /*  600 */  { MAD_F(0x04f12624) /* 0.308874267 */, 14 },
  /*  601 */  { MAD_F(0x04f3f612) /* 0.309560845 */, 14 },
  /*  602 */  { MAD_F(0x04f6c666) /* 0.310247804 */, 14 },
  /*  603 */  { MAD_F(0x04f99721) /* 0.310935143 */, 14 },
  /*  604 */  { MAD_F(0x04fc6841) /* 0.311622862 */, 14 },
  /*  605 */  { MAD_F(0x04ff39c7) /* 0.312310961 */, 14 },
  /*  606 */  { MAD_F(0x05020bb3) /* 0.312999439 */, 14 },
  /*  607 */  { MAD_F(0x0504de05) /* 0.313688296 */, 14 },

  /*  608 */  { MAD_F(0x0507b0bc) /* 0.314377532 */, 14 },
  /*  609 */  { MAD_F(0x050a83d8) /* 0.315067145 */, 14 },
  /*  610 */  { MAD_F(0x050d575b) /* 0.315757136 */, 14 },
  /*  611 */  { MAD_F(0x05102b42) /* 0.316447504 */, 14 },
  /*  612 */  { MAD_F(0x0512ff8e) /* 0.317138249 */, 14 },
  /*  613 */  { MAD_F(0x0515d440) /* 0.317829370 */, 14 },
  /*  614 */  { MAD_F(0x0518a956) /* 0.318520867 */, 14 },
  /*  615 */  { MAD_F(0x051b7ed1) /* 0.319212739 */, 14 },
  /*  616 */  { MAD_F(0x051e54b1) /* 0.319904987 */, 14 },
  /*  617 */  { MAD_F(0x05212af5) /* 0.320597609 */, 14 },
  /*  618 */  { MAD_F(0x0524019e) /* 0.321290606 */, 14 },
  /*  619 */  { MAD_F(0x0526d8ab) /* 0.321983976 */, 14 },
  /*  620 */  { MAD_F(0x0529b01d) /* 0.322677720 */, 14 },
  /*  621 */  { MAD_F(0x052c87f2) /* 0.323371837 */, 14 },
  /*  622 */  { MAD_F(0x052f602c) /* 0.324066327 */, 14 },
  /*  623 */  { MAD_F(0x053238ca) /* 0.324761189 */, 14 },

  /*  624 */  { MAD_F(0x053511cb) /* 0.325456423 */, 14 },
  /*  625 */  { MAD_F(0x0537eb30) /* 0.326152028 */, 14 },
  /*  626 */  { MAD_F(0x053ac4f9) /* 0.326848005 */, 14 },
  /*  627 */  { MAD_F(0x053d9f25) /* 0.327544352 */, 14 },
  /*  628 */  { MAD_F(0x054079b5) /* 0.328241070 */, 14 },
  /*  629 */  { MAD_F(0x054354a8) /* 0.328938157 */, 14 },
  /*  630 */  { MAD_F(0x05462ffe) /* 0.329635614 */, 14 },
  /*  631 */  { MAD_F(0x05490bb7) /* 0.330333440 */, 14 },
  /*  632 */  { MAD_F(0x054be7d4) /* 0.331031635 */, 14 },
  /*  633 */  { MAD_F(0x054ec453) /* 0.331730198 */, 14 },
  /*  634 */  { MAD_F(0x0551a134) /* 0.332429129 */, 14 },
  /*  635 */  { MAD_F(0x05547e79) /* 0.333128427 */, 14 },
  /*  636 */  { MAD_F(0x05575c20) /* 0.333828093 */, 14 },
  /*  637 */  { MAD_F(0x055a3a2a) /* 0.334528126 */, 14 },
  /*  638 */  { MAD_F(0x055d1896) /* 0.335228525 */, 14 },
  /*  639 */  { MAD_F(0x055ff764) /* 0.335929290 */, 14 },

  /*  640 */  { MAD_F(0x0562d694) /* 0.336630420 */, 14 },
  /*  641 */  { MAD_F(0x0565b627) /* 0.337331916 */, 14 },
  /*  642 */  { MAD_F(0x0568961b) /* 0.338033777 */, 14 },
  /*  643 */  { MAD_F(0x056b7671) /* 0.338736002 */, 14 },
  /*  644 */  { MAD_F(0x056e5729) /* 0.339438592 */, 14 },
  /*  645 */  { MAD_F(0x05713843) /* 0.340141545 */, 14 },
  /*  646 */  { MAD_F(0x057419be) /* 0.340844862 */, 14 },
  /*  647 */  { MAD_F(0x0576fb9a) /* 0.341548541 */, 14 },
  /*  648 */  { MAD_F(0x0579ddd8) /* 0.342252584 */, 14 },
  /*  649 */  { MAD_F(0x057cc077) /* 0.342956988 */, 14 },
  /*  650 */  { MAD_F(0x057fa378) /* 0.343661754 */, 14 },
  /*  651 */  { MAD_F(0x058286d9) /* 0.344366882 */, 14 },
  /*  652 */  { MAD_F(0x05856a9b) /* 0.345072371 */, 14 },
  /*  653 */  { MAD_F(0x05884ebe) /* 0.345778221 */, 14 },
  /*  654 */  { MAD_F(0x058b3342) /* 0.346484431 */, 14 },
  /*  655 */  { MAD_F(0x058e1827) /* 0.347191002 */, 14 },

  /*  656 */  { MAD_F(0x0590fd6c) /* 0.347897931 */, 14 },
  /*  657 */  { MAD_F(0x0593e311) /* 0.348605221 */, 14 },
  /*  658 */  { MAD_F(0x0596c917) /* 0.349312869 */, 14 },
  /*  659 */  { MAD_F(0x0599af7d) /* 0.350020876 */, 14 },
  /*  660 */  { MAD_F(0x059c9643) /* 0.350729240 */, 14 },
  /*  661 */  { MAD_F(0x059f7d6a) /* 0.351437963 */, 14 },
  /*  662 */  { MAD_F(0x05a264f0) /* 0.352147044 */, 14 },
  /*  663 */  { MAD_F(0x05a54cd6) /* 0.352856481 */, 14 },
  /*  664 */  { MAD_F(0x05a8351c) /* 0.353566275 */, 14 },
  /*  665 */  { MAD_F(0x05ab1dc2) /* 0.354276426 */, 14 },
  /*  666 */  { MAD_F(0x05ae06c7) /* 0.354986932 */, 14 },
  /*  667 */  { MAD_F(0x05b0f02b) /* 0.355697795 */, 14 },
  /*  668 */  { MAD_F(0x05b3d9f0) /* 0.356409012 */, 14 },
  /*  669 */  { MAD_F(0x05b6c413) /* 0.357120585 */, 14 },
  /*  670 */  { MAD_F(0x05b9ae95) /* 0.357832512 */, 14 },
  /*  671 */  { MAD_F(0x05bc9977) /* 0.358544794 */, 14 },

  /*  672 */  { MAD_F(0x05bf84b8) /* 0.359257429 */, 14 },
  /*  673 */  { MAD_F(0x05c27057) /* 0.359970419 */, 14 },
  /*  674 */  { MAD_F(0x05c55c56) /* 0.360683761 */, 14 },
  /*  675 */  { MAD_F(0x05c848b3) /* 0.361397456 */, 14 },
  /*  676 */  { MAD_F(0x05cb356e) /* 0.362111504 */, 14 },
  /*  677 */  { MAD_F(0x05ce2289) /* 0.362825904 */, 14 },
  /*  678 */  { MAD_F(0x05d11001) /* 0.363540655 */, 14 },
  /*  679 */  { MAD_F(0x05d3fdd8) /* 0.364255759 */, 14 },
  /*  680 */  { MAD_F(0x05d6ec0e) /* 0.364971213 */, 14 },
  /*  681 */  { MAD_F(0x05d9daa1) /* 0.365687018 */, 14 },
  /*  682 */  { MAD_F(0x05dcc993) /* 0.366403174 */, 14 },
  /*  683 */  { MAD_F(0x05dfb8e2) /* 0.367119680 */, 14 },
  /*  684 */  { MAD_F(0x05e2a890) /* 0.367836535 */, 14 },
  /*  685 */  { MAD_F(0x05e5989b) /* 0.368553740 */, 14 },
  /*  686 */  { MAD_F(0x05e88904) /* 0.369271294 */, 14 },
  /*  687 */  { MAD_F(0x05eb79cb) /* 0.369989197 */, 14 },

  /*  688 */  { MAD_F(0x05ee6aef) /* 0.370707448 */, 14 },
  /*  689 */  { MAD_F(0x05f15c70) /* 0.371426047 */, 14 },
  /*  690 */  { MAD_F(0x05f44e4f) /* 0.372144994 */, 14 },
  /*  691 */  { MAD_F(0x05f7408b) /* 0.372864289 */, 14 },
  /*  692 */  { MAD_F(0x05fa3324) /* 0.373583930 */, 14 },
  /*  693 */  { MAD_F(0x05fd261b) /* 0.374303918 */, 14 },
  /*  694 */  { MAD_F(0x0600196e) /* 0.375024253 */, 14 },
  /*  695 */  { MAD_F(0x06030d1e) /* 0.375744934 */, 14 },
  /*  696 */  { MAD_F(0x0606012b) /* 0.376465960 */, 14 },
  /*  697 */  { MAD_F(0x0608f595) /* 0.377187332 */, 14 },
  /*  698 */  { MAD_F(0x060bea5c) /* 0.377909049 */, 14 },
  /*  699 */  { MAD_F(0x060edf7f) /* 0.378631110 */, 14 },
  /*  700 */  { MAD_F(0x0611d4fe) /* 0.379353516 */, 14 },
  /*  701 */  { MAD_F(0x0614cada) /* 0.380076266 */, 14 },
  /*  702 */  { MAD_F(0x0617c112) /* 0.380799360 */, 14 },
  /*  703 */  { MAD_F(0x061ab7a6) /* 0.381522798 */, 14 },

  /*  704 */  { MAD_F(0x061dae96) /* 0.382246578 */, 14 },
  /*  705 */  { MAD_F(0x0620a5e3) /* 0.382970701 */, 14 },
  /*  706 */  { MAD_F(0x06239d8b) /* 0.383695167 */, 14 },
  /*  707 */  { MAD_F(0x0626958f) /* 0.384419975 */, 14 },
  /*  708 */  { MAD_F(0x06298def) /* 0.385145124 */, 14 },
  /*  709 */  { MAD_F(0x062c86aa) /* 0.385870615 */, 14 },
  /*  710 */  { MAD_F(0x062f7fc1) /* 0.386596448 */, 14 },
  /*  711 */  { MAD_F(0x06327934) /* 0.387322621 */, 14 },
  /*  712 */  { MAD_F(0x06357302) /* 0.388049134 */, 14 },
  /*  713 */  { MAD_F(0x06386d2b) /* 0.388775988 */, 14 },
  /*  714 */  { MAD_F(0x063b67b0) /* 0.389503182 */, 14 },
  /*  715 */  { MAD_F(0x063e6290) /* 0.390230715 */, 14 },
  /*  716 */  { MAD_F(0x06415dcb) /* 0.390958588 */, 14 },
  /*  717 */  { MAD_F(0x06445960) /* 0.391686799 */, 14 },
  /*  718 */  { MAD_F(0x06475551) /* 0.392415349 */, 14 },
  /*  719 */  { MAD_F(0x064a519c) /* 0.393144238 */, 14 },

  /*  720 */  { MAD_F(0x064d4e43) /* 0.393873464 */, 14 },
  /*  721 */  { MAD_F(0x06504b44) /* 0.394603028 */, 14 },
  /*  722 */  { MAD_F(0x0653489f) /* 0.395332930 */, 14 },
  /*  723 */  { MAD_F(0x06564655) /* 0.396063168 */, 14 },
  /*  724 */  { MAD_F(0x06594465) /* 0.396793743 */, 14 },
  /*  725 */  { MAD_F(0x065c42d0) /* 0.397524655 */, 14 },
  /*  726 */  { MAD_F(0x065f4195) /* 0.398255903 */, 14 },
  /*  727 */  { MAD_F(0x066240b4) /* 0.398987487 */, 14 },
  /*  728 */  { MAD_F(0x0665402d) /* 0.399719406 */, 14 },
  /*  729 */  { MAD_F(0x06684000) /* 0.400451660 */, 14 },
  /*  730 */  { MAD_F(0x066b402d) /* 0.401184249 */, 14 },
  /*  731 */  { MAD_F(0x066e40b3) /* 0.401917173 */, 14 },
  /*  732 */  { MAD_F(0x06714194) /* 0.402650431 */, 14 },
  /*  733 */  { MAD_F(0x067442ce) /* 0.403384024 */, 14 },
  /*  734 */  { MAD_F(0x06774462) /* 0.404117949 */, 14 },
  /*  735 */  { MAD_F(0x067a464f) /* 0.404852209 */, 14 },

  /*  736 */  { MAD_F(0x067d4896) /* 0.405586801 */, 14 },
  /*  737 */  { MAD_F(0x06804b36) /* 0.406321726 */, 14 },
  /*  738 */  { MAD_F(0x06834e2f) /* 0.407056983 */, 14 },
  /*  739 */  { MAD_F(0x06865181) /* 0.407792573 */, 14 },
  /*  740 */  { MAD_F(0x0689552c) /* 0.408528495 */, 14 },
  /*  741 */  { MAD_F(0x068c5931) /* 0.409264748 */, 14 },
  /*  742 */  { MAD_F(0x068f5d8e) /* 0.410001332 */, 14 },
  /*  743 */  { MAD_F(0x06926245) /* 0.410738247 */, 14 },
  /*  744 */  { MAD_F(0x06956753) /* 0.411475493 */, 14 },
  /*  745 */  { MAD_F(0x06986cbb) /* 0.412213070 */, 14 },
  /*  746 */  { MAD_F(0x069b727b) /* 0.412950976 */, 14 },
  /*  747 */  { MAD_F(0x069e7894) /* 0.413689213 */, 14 },
  /*  748 */  { MAD_F(0x06a17f05) /* 0.414427779 */, 14 },
  /*  749 */  { MAD_F(0x06a485cf) /* 0.415166674 */, 14 },
  /*  750 */  { MAD_F(0x06a78cf1) /* 0.415905897 */, 14 },
  /*  751 */  { MAD_F(0x06aa946b) /* 0.416645450 */, 14 },

  /*  752 */  { MAD_F(0x06ad9c3d) /* 0.417385331 */, 14 },
  /*  753 */  { MAD_F(0x06b0a468) /* 0.418125540 */, 14 },
  /*  754 */  { MAD_F(0x06b3acea) /* 0.418866076 */, 14 },
  /*  755 */  { MAD_F(0x06b6b5c4) /* 0.419606940 */, 14 },
  /*  756 */  { MAD_F(0x06b9bef6) /* 0.420348132 */, 14 },
  /*  757 */  { MAD_F(0x06bcc880) /* 0.421089650 */, 14 },
  /*  758 */  { MAD_F(0x06bfd261) /* 0.421831494 */, 14 },
  /*  759 */  { MAD_F(0x06c2dc9a) /* 0.422573665 */, 14 },
  /*  760 */  { MAD_F(0x06c5e72b) /* 0.423316162 */, 14 },
  /*  761 */  { MAD_F(0x06c8f213) /* 0.424058985 */, 14 },
  /*  762 */  { MAD_F(0x06cbfd52) /* 0.424802133 */, 14 },
  /*  763 */  { MAD_F(0x06cf08e9) /* 0.425545607 */, 14 },
  /*  764 */  { MAD_F(0x06d214d7) /* 0.426289405 */, 14 },
  /*  765 */  { MAD_F(0x06d5211c) /* 0.427033528 */, 14 },
  /*  766 */  { MAD_F(0x06d82db8) /* 0.427777975 */, 14 },
  /*  767 */  { MAD_F(0x06db3aaa) /* 0.428522746 */, 14 },

  /*  768 */  { MAD_F(0x06de47f4) /* 0.429267841 */, 14 },
  /*  769 */  { MAD_F(0x06e15595) /* 0.430013259 */, 14 },
  /*  770 */  { MAD_F(0x06e4638d) /* 0.430759001 */, 14 },
  /*  771 */  { MAD_F(0x06e771db) /* 0.431505065 */, 14 },
  /*  772 */  { MAD_F(0x06ea807f) /* 0.432251452 */, 14 },
  /*  773 */  { MAD_F(0x06ed8f7b) /* 0.432998162 */, 14 },
  /*  774 */  { MAD_F(0x06f09ecc) /* 0.433745193 */, 14 },
  /*  775 */  { MAD_F(0x06f3ae75) /* 0.434492546 */, 14 },
  /*  776 */  { MAD_F(0x06f6be73) /* 0.435240221 */, 14 },
  /*  777 */  { MAD_F(0x06f9cec8) /* 0.435988217 */, 14 },
  /*  778 */  { MAD_F(0x06fcdf72) /* 0.436736534 */, 14 },
  /*  779 */  { MAD_F(0x06fff073) /* 0.437485172 */, 14 },
  /*  780 */  { MAD_F(0x070301ca) /* 0.438234130 */, 14 },
  /*  781 */  { MAD_F(0x07061377) /* 0.438983408 */, 14 },
  /*  782 */  { MAD_F(0x0709257a) /* 0.439733006 */, 14 },
  /*  783 */  { MAD_F(0x070c37d2) /* 0.440482924 */, 14 },

  /*  784 */  { MAD_F(0x070f4a80) /* 0.441233161 */, 14 },
  /*  785 */  { MAD_F(0x07125d84) /* 0.441983717 */, 14 },
  /*  786 */  { MAD_F(0x071570de) /* 0.442734592 */, 14 },
  /*  787 */  { MAD_F(0x0718848d) /* 0.443485785 */, 14 },
  /*  788 */  { MAD_F(0x071b9891) /* 0.444237296 */, 14 },
  /*  789 */  { MAD_F(0x071eaceb) /* 0.444989126 */, 14 },
  /*  790 */  { MAD_F(0x0721c19a) /* 0.445741273 */, 14 },
  /*  791 */  { MAD_F(0x0724d69e) /* 0.446493738 */, 14 },
  /*  792 */  { MAD_F(0x0727ebf7) /* 0.447246519 */, 14 },
  /*  793 */  { MAD_F(0x072b01a6) /* 0.447999618 */, 14 },
  /*  794 */  { MAD_F(0x072e17a9) /* 0.448753033 */, 14 },
  /*  795 */  { MAD_F(0x07312e01) /* 0.449506765 */, 14 },
  /*  796 */  { MAD_F(0x073444ae) /* 0.450260813 */, 14 },
  /*  797 */  { MAD_F(0x07375bb0) /* 0.451015176 */, 14 },
  /*  798 */  { MAD_F(0x073a7307) /* 0.451769856 */, 14 },
  /*  799 */  { MAD_F(0x073d8ab2) /* 0.452524850 */, 14 },

  /*  800 */  { MAD_F(0x0740a2b2) /* 0.453280160 */, 14 },
  /*  801 */  { MAD_F(0x0743bb06) /* 0.454035784 */, 14 },
  /*  802 */  { MAD_F(0x0746d3af) /* 0.454791723 */, 14 },
  /*  803 */  { MAD_F(0x0749ecac) /* 0.455547976 */, 14 },
  /*  804 */  { MAD_F(0x074d05fe) /* 0.456304543 */, 14 },
  /*  805 */  { MAD_F(0x07501fa3) /* 0.457061423 */, 14 },
  /*  806 */  { MAD_F(0x0753399d) /* 0.457818618 */, 14 },
  /*  807 */  { MAD_F(0x075653eb) /* 0.458576125 */, 14 },
  /*  808 */  { MAD_F(0x07596e8d) /* 0.459333946 */, 14 },
  /*  809 */  { MAD_F(0x075c8983) /* 0.460092079 */, 14 },
  /*  810 */  { MAD_F(0x075fa4cc) /* 0.460850524 */, 14 },
  /*  811 */  { MAD_F(0x0762c06a) /* 0.461609282 */, 14 },
  /*  812 */  { MAD_F(0x0765dc5b) /* 0.462368352 */, 14 },
  /*  813 */  { MAD_F(0x0768f8a0) /* 0.463127733 */, 14 },
  /*  814 */  { MAD_F(0x076c1538) /* 0.463887426 */, 14 },
  /*  815 */  { MAD_F(0x076f3224) /* 0.464647430 */, 14 },

  /*  816 */  { MAD_F(0x07724f64) /* 0.465407744 */, 14 },
  /*  817 */  { MAD_F(0x07756cf7) /* 0.466168370 */, 14 },
  /*  818 */  { MAD_F(0x07788add) /* 0.466929306 */, 14 },
  /*  819 */  { MAD_F(0x077ba916) /* 0.467690552 */, 14 },
  /*  820 */  { MAD_F(0x077ec7a3) /* 0.468452108 */, 14 },
  /*  821 */  { MAD_F(0x0781e683) /* 0.469213973 */, 14 },
  /*  822 */  { MAD_F(0x078505b5) /* 0.469976148 */, 14 },
  /*  823 */  { MAD_F(0x0788253b) /* 0.470738632 */, 14 },
  /*  824 */  { MAD_F(0x078b4514) /* 0.471501425 */, 14 },
  /*  825 */  { MAD_F(0x078e653f) /* 0.472264527 */, 14 },
  /*  826 */  { MAD_F(0x079185be) /* 0.473027937 */, 14 },
  /*  827 */  { MAD_F(0x0794a68f) /* 0.473791655 */, 14 },
  /*  828 */  { MAD_F(0x0797c7b2) /* 0.474555681 */, 14 },
  /*  829 */  { MAD_F(0x079ae929) /* 0.475320014 */, 14 },
  /*  830 */  { MAD_F(0x079e0af1) /* 0.476084655 */, 14 },
  /*  831 */  { MAD_F(0x07a12d0c) /* 0.476849603 */, 14 },

  /*  832 */  { MAD_F(0x07a44f7a) /* 0.477614858 */, 14 },
  /*  833 */  { MAD_F(0x07a7723a) /* 0.478380420 */, 14 },
  /*  834 */  { MAD_F(0x07aa954c) /* 0.479146288 */, 14 },
  /*  835 */  { MAD_F(0x07adb8b0) /* 0.479912463 */, 14 },
  /*  836 */  { MAD_F(0x07b0dc67) /* 0.480678943 */, 14 },
  /*  837 */  { MAD_F(0x07b4006f) /* 0.481445729 */, 14 },
  /*  838 */  { MAD_F(0x07b724ca) /* 0.482212820 */, 14 },
  /*  839 */  { MAD_F(0x07ba4976) /* 0.482980216 */, 14 },
  /*  840 */  { MAD_F(0x07bd6e75) /* 0.483747918 */, 14 },
  /*  841 */  { MAD_F(0x07c093c5) /* 0.484515924 */, 14 },
  /*  842 */  { MAD_F(0x07c3b967) /* 0.485284235 */, 14 },
  /*  843 */  { MAD_F(0x07c6df5a) /* 0.486052849 */, 14 },
  /*  844 */  { MAD_F(0x07ca059f) /* 0.486821768 */, 14 },
  /*  845 */  { MAD_F(0x07cd2c36) /* 0.487590991 */, 14 },
  /*  846 */  { MAD_F(0x07d0531e) /* 0.488360517 */, 14 },
  /*  847 */  { MAD_F(0x07d37a57) /* 0.489130346 */, 14 },

  /*  848 */  { MAD_F(0x07d6a1e2) /* 0.489900479 */, 14 },
  /*  849 */  { MAD_F(0x07d9c9be) /* 0.490670914 */, 14 },
  /*  850 */  { MAD_F(0x07dcf1ec) /* 0.491441651 */, 14 },
  /*  851 */  { MAD_F(0x07e01a6a) /* 0.492212691 */, 14 },
  /*  852 */  { MAD_F(0x07e3433a) /* 0.492984033 */, 14 },
  /*  853 */  { MAD_F(0x07e66c5a) /* 0.493755677 */, 14 },
  /*  854 */  { MAD_F(0x07e995cc) /* 0.494527623 */, 14 },
  /*  855 */  { MAD_F(0x07ecbf8e) /* 0.495299870 */, 14 },
  /*  856 */  { MAD_F(0x07efe9a1) /* 0.496072418 */, 14 },
  /*  857 */  { MAD_F(0x07f31405) /* 0.496845266 */, 14 },
  /*  858 */  { MAD_F(0x07f63eba) /* 0.497618416 */, 14 },
  /*  859 */  { MAD_F(0x07f969c0) /* 0.498391866 */, 14 },
  /*  860 */  { MAD_F(0x07fc9516) /* 0.499165616 */, 14 },
  /*  861 */  { MAD_F(0x07ffc0bc) /* 0.499939666 */, 14 },
  /*  862 */  { MAD_F(0x04017659) /* 0.250357008 */, 15 },
  /*  863 */  { MAD_F(0x04030c7d) /* 0.250744333 */, 15 },

  /*  864 */  { MAD_F(0x0404a2c9) /* 0.251131807 */, 15 },
  /*  865 */  { MAD_F(0x0406393d) /* 0.251519431 */, 15 },
  /*  866 */  { MAD_F(0x0407cfd9) /* 0.251907204 */, 15 },
  /*  867 */  { MAD_F(0x0409669d) /* 0.252295127 */, 15 },
  /*  868 */  { MAD_F(0x040afd89) /* 0.252683198 */, 15 },
  /*  869 */  { MAD_F(0x040c949e) /* 0.253071419 */, 15 },
  /*  870 */  { MAD_F(0x040e2bda) /* 0.253459789 */, 15 },
  /*  871 */  { MAD_F(0x040fc33e) /* 0.253848307 */, 15 },
  /*  872 */  { MAD_F(0x04115aca) /* 0.254236974 */, 15 },
  /*  873 */  { MAD_F(0x0412f27e) /* 0.254625790 */, 15 },
  /*  874 */  { MAD_F(0x04148a5a) /* 0.255014755 */, 15 },
  /*  875 */  { MAD_F(0x0416225d) /* 0.255403867 */, 15 },
  /*  876 */  { MAD_F(0x0417ba89) /* 0.255793128 */, 15 },
  /*  877 */  { MAD_F(0x041952dc) /* 0.256182537 */, 15 },
  /*  878 */  { MAD_F(0x041aeb57) /* 0.256572095 */, 15 },
  /*  879 */  { MAD_F(0x041c83fa) /* 0.256961800 */, 15 },

  /*  880 */  { MAD_F(0x041e1cc4) /* 0.257351652 */, 15 },
  /*  881 */  { MAD_F(0x041fb5b6) /* 0.257741653 */, 15 },
  /*  882 */  { MAD_F(0x04214ed0) /* 0.258131801 */, 15 },
  /*  883 */  { MAD_F(0x0422e811) /* 0.258522097 */, 15 },
  /*  884 */  { MAD_F(0x04248179) /* 0.258912540 */, 15 },
  /*  885 */  { MAD_F(0x04261b0a) /* 0.259303130 */, 15 },
  /*  886 */  { MAD_F(0x0427b4c2) /* 0.259693868 */, 15 },
  /*  887 */  { MAD_F(0x04294ea1) /* 0.260084752 */, 15 },
  /*  888 */  { MAD_F(0x042ae8a7) /* 0.260475783 */, 15 },
  /*  889 */  { MAD_F(0x042c82d6) /* 0.260866961 */, 15 },
  /*  890 */  { MAD_F(0x042e1d2b) /* 0.261258286 */, 15 },
  /*  891 */  { MAD_F(0x042fb7a8) /* 0.261649758 */, 15 },
  /*  892 */  { MAD_F(0x0431524c) /* 0.262041376 */, 15 },
  /*  893 */  { MAD_F(0x0432ed17) /* 0.262433140 */, 15 },
  /*  894 */  { MAD_F(0x0434880a) /* 0.262825051 */, 15 },
  /*  895 */  { MAD_F(0x04362324) /* 0.263217107 */, 15 },

  /*  896 */  { MAD_F(0x0437be65) /* 0.263609310 */, 15 },
  /*  897 */  { MAD_F(0x043959cd) /* 0.264001659 */, 15 },
  /*  898 */  { MAD_F(0x043af55d) /* 0.264394153 */, 15 },
  /*  899 */  { MAD_F(0x043c9113) /* 0.264786794 */, 15 },
  /*  900 */  { MAD_F(0x043e2cf1) /* 0.265179580 */, 15 },
  /*  901 */  { MAD_F(0x043fc8f6) /* 0.265572511 */, 15 },
  /*  902 */  { MAD_F(0x04416522) /* 0.265965588 */, 15 },
  /*  903 */  { MAD_F(0x04430174) /* 0.266358810 */, 15 },
  /*  904 */  { MAD_F(0x04449dee) /* 0.266752177 */, 15 },
  /*  905 */  { MAD_F(0x04463a8f) /* 0.267145689 */, 15 },
  /*  906 */  { MAD_F(0x0447d756) /* 0.267539347 */, 15 },
  /*  907 */  { MAD_F(0x04497445) /* 0.267933149 */, 15 },
  /*  908 */  { MAD_F(0x044b115a) /* 0.268327096 */, 15 },
  /*  909 */  { MAD_F(0x044cae96) /* 0.268721187 */, 15 },
  /*  910 */  { MAD_F(0x044e4bf9) /* 0.269115423 */, 15 },
  /*  911 */  { MAD_F(0x044fe983) /* 0.269509804 */, 15 },

  /*  912 */  { MAD_F(0x04518733) /* 0.269904329 */, 15 },
  /*  913 */  { MAD_F(0x0453250a) /* 0.270298998 */, 15 },
  /*  914 */  { MAD_F(0x0454c308) /* 0.270693811 */, 15 },
  /*  915 */  { MAD_F(0x0456612d) /* 0.271088768 */, 15 },
  /*  916 */  { MAD_F(0x0457ff78) /* 0.271483869 */, 15 },
  /*  917 */  { MAD_F(0x04599dea) /* 0.271879114 */, 15 },
  /*  918 */  { MAD_F(0x045b3c82) /* 0.272274503 */, 15 },
  /*  919 */  { MAD_F(0x045cdb41) /* 0.272670035 */, 15 },
  /*  920 */  { MAD_F(0x045e7a26) /* 0.273065710 */, 15 },
  /*  921 */  { MAD_F(0x04601932) /* 0.273461530 */, 15 },
  /*  922 */  { MAD_F(0x0461b864) /* 0.273857492 */, 15 },
  /*  923 */  { MAD_F(0x046357bd) /* 0.274253597 */, 15 },
  /*  924 */  { MAD_F(0x0464f73c) /* 0.274649846 */, 15 },
  /*  925 */  { MAD_F(0x046696e2) /* 0.275046238 */, 15 },
  /*  926 */  { MAD_F(0x046836ae) /* 0.275442772 */, 15 },
  /*  927 */  { MAD_F(0x0469d6a0) /* 0.275839449 */, 15 },

  /*  928 */  { MAD_F(0x046b76b9) /* 0.276236269 */, 15 },
  /*  929 */  { MAD_F(0x046d16f7) /* 0.276633232 */, 15 },
  /*  930 */  { MAD_F(0x046eb75c) /* 0.277030337 */, 15 },
  /*  931 */  { MAD_F(0x047057e8) /* 0.277427584 */, 15 },
  /*  932 */  { MAD_F(0x0471f899) /* 0.277824973 */, 15 },
  /*  933 */  { MAD_F(0x04739971) /* 0.278222505 */, 15 },
  /*  934 */  { MAD_F(0x04753a6f) /* 0.278620179 */, 15 },
  /*  935 */  { MAD_F(0x0476db92) /* 0.279017995 */, 15 },
  /*  936 */  { MAD_F(0x04787cdc) /* 0.279415952 */, 15 },
  /*  937 */  { MAD_F(0x047a1e4c) /* 0.279814051 */, 15 },
  /*  938 */  { MAD_F(0x047bbfe2) /* 0.280212292 */, 15 },
  /*  939 */  { MAD_F(0x047d619e) /* 0.280610675 */, 15 },
  /*  940 */  { MAD_F(0x047f0380) /* 0.281009199 */, 15 },
  /*  941 */  { MAD_F(0x0480a588) /* 0.281407864 */, 15 },
  /*  942 */  { MAD_F(0x048247b6) /* 0.281806670 */, 15 },
  /*  943 */  { MAD_F(0x0483ea0a) /* 0.282205618 */, 15 },

  /*  944 */  { MAD_F(0x04858c83) /* 0.282604707 */, 15 },
  /*  945 */  { MAD_F(0x04872f22) /* 0.283003936 */, 15 },
  /*  946 */  { MAD_F(0x0488d1e8) /* 0.283403307 */, 15 },
  /*  947 */  { MAD_F(0x048a74d3) /* 0.283802818 */, 15 },
  /*  948 */  { MAD_F(0x048c17e3) /* 0.284202470 */, 15 },
  /*  949 */  { MAD_F(0x048dbb1a) /* 0.284602263 */, 15 },
  /*  950 */  { MAD_F(0x048f5e76) /* 0.285002195 */, 15 },
  /*  951 */  { MAD_F(0x049101f8) /* 0.285402269 */, 15 },
  /*  952 */  { MAD_F(0x0492a59f) /* 0.285802482 */, 15 },
  /*  953 */  { MAD_F(0x0494496c) /* 0.286202836 */, 15 },
  /*  954 */  { MAD_F(0x0495ed5f) /* 0.286603329 */, 15 },
  /*  955 */  { MAD_F(0x04979177) /* 0.287003963 */, 15 },
  /*  956 */  { MAD_F(0x049935b5) /* 0.287404737 */, 15 },
  /*  957 */  { MAD_F(0x049ada19) /* 0.287805650 */, 15 },
  /*  958 */  { MAD_F(0x049c7ea1) /* 0.288206703 */, 15 },
  /*  959 */  { MAD_F(0x049e2350) /* 0.288607895 */, 15 },

  /*  960 */  { MAD_F(0x049fc824) /* 0.289009227 */, 15 },
  /*  961 */  { MAD_F(0x04a16d1d) /* 0.289410699 */, 15 },
  /*  962 */  { MAD_F(0x04a3123b) /* 0.289812309 */, 15 },
  /*  963 */  { MAD_F(0x04a4b77f) /* 0.290214059 */, 15 },
  /*  964 */  { MAD_F(0x04a65ce8) /* 0.290615948 */, 15 },
  /*  965 */  { MAD_F(0x04a80277) /* 0.291017976 */, 15 },
  /*  966 */  { MAD_F(0x04a9a82b) /* 0.291420143 */, 15 },
  /*  967 */  { MAD_F(0x04ab4e04) /* 0.291822449 */, 15 },
  /*  968 */  { MAD_F(0x04acf402) /* 0.292224893 */, 15 },
  /*  969 */  { MAD_F(0x04ae9a26) /* 0.292627476 */, 15 },
  /*  970 */  { MAD_F(0x04b0406e) /* 0.293030197 */, 15 },
  /*  971 */  { MAD_F(0x04b1e6dc) /* 0.293433057 */, 15 },
  /*  972 */  { MAD_F(0x04b38d6f) /* 0.293836055 */, 15 },
  /*  973 */  { MAD_F(0x04b53427) /* 0.294239192 */, 15 },
  /*  974 */  { MAD_F(0x04b6db05) /* 0.294642466 */, 15 },
  /*  975 */  { MAD_F(0x04b88207) /* 0.295045879 */, 15 },

  /*  976 */  { MAD_F(0x04ba292e) /* 0.295449429 */, 15 },
  /*  977 */  { MAD_F(0x04bbd07a) /* 0.295853118 */, 15 },
  /*  978 */  { MAD_F(0x04bd77ec) /* 0.296256944 */, 15 },
  /*  979 */  { MAD_F(0x04bf1f82) /* 0.296660907 */, 15 },
  /*  980 */  { MAD_F(0x04c0c73d) /* 0.297065009 */, 15 },
  /*  981 */  { MAD_F(0x04c26f1d) /* 0.297469248 */, 15 },
  /*  982 */  { MAD_F(0x04c41722) /* 0.297873624 */, 15 },
  /*  983 */  { MAD_F(0x04c5bf4c) /* 0.298278137 */, 15 },
  /*  984 */  { MAD_F(0x04c7679a) /* 0.298682788 */, 15 },
  /*  985 */  { MAD_F(0x04c9100d) /* 0.299087576 */, 15 },
  /*  986 */  { MAD_F(0x04cab8a6) /* 0.299492500 */, 15 },
  /*  987 */  { MAD_F(0x04cc6163) /* 0.299897562 */, 15 },
  /*  988 */  { MAD_F(0x04ce0a44) /* 0.300302761 */, 15 },
  /*  989 */  { MAD_F(0x04cfb34b) /* 0.300708096 */, 15 },
  /*  990 */  { MAD_F(0x04d15c76) /* 0.301113568 */, 15 },
  /*  991 */  { MAD_F(0x04d305c5) /* 0.301519176 */, 15 },

  /*  992 */  { MAD_F(0x04d4af3a) /* 0.301924921 */, 15 },
  /*  993 */  { MAD_F(0x04d658d2) /* 0.302330802 */, 15 },
  /*  994 */  { MAD_F(0x04d80290) /* 0.302736820 */, 15 },
  /*  995 */  { MAD_F(0x04d9ac72) /* 0.303142973 */, 15 },
  /*  996 */  { MAD_F(0x04db5679) /* 0.303549263 */, 15 },
  /*  997 */  { MAD_F(0x04dd00a4) /* 0.303955689 */, 15 },
  /*  998 */  { MAD_F(0x04deaaf3) /* 0.304362251 */, 15 },
  /*  999 */  { MAD_F(0x04e05567) /* 0.304768948 */, 15 },
  /* 1000 */  { MAD_F(0x04e20000) /* 0.305175781 */, 15 },
  /* 1001 */  { MAD_F(0x04e3aabd) /* 0.305582750 */, 15 },
  /* 1002 */  { MAD_F(0x04e5559e) /* 0.305989854 */, 15 },
  /* 1003 */  { MAD_F(0x04e700a3) /* 0.306397094 */, 15 },
  /* 1004 */  { MAD_F(0x04e8abcd) /* 0.306804470 */, 15 },
  /* 1005 */  { MAD_F(0x04ea571c) /* 0.307211980 */, 15 },
  /* 1006 */  { MAD_F(0x04ec028e) /* 0.307619626 */, 15 },
  /* 1007 */  { MAD_F(0x04edae25) /* 0.308027406 */, 15 },

  /* 1008 */  { MAD_F(0x04ef59e0) /* 0.308435322 */, 15 },
  /* 1009 */  { MAD_F(0x04f105bf) /* 0.308843373 */, 15 },
  /* 1010 */  { MAD_F(0x04f2b1c3) /* 0.309251558 */, 15 },
  /* 1011 */  { MAD_F(0x04f45dea) /* 0.309659879 */, 15 },
  /* 1012 */  { MAD_F(0x04f60a36) /* 0.310068333 */, 15 },
  /* 1013 */  { MAD_F(0x04f7b6a6) /* 0.310476923 */, 15 },
  /* 1014 */  { MAD_F(0x04f9633a) /* 0.310885647 */, 15 },
  /* 1015 */  { MAD_F(0x04fb0ff2) /* 0.311294505 */, 15 },
  /* 1016 */  { MAD_F(0x04fcbcce) /* 0.311703498 */, 15 },
  /* 1017 */  { MAD_F(0x04fe69ce) /* 0.312112625 */, 15 },
  /* 1018 */  { MAD_F(0x050016f3) /* 0.312521885 */, 15 },
  /* 1019 */  { MAD_F(0x0501c43b) /* 0.312931280 */, 15 },
  /* 1020 */  { MAD_F(0x050371a7) /* 0.313340809 */, 15 },
  /* 1021 */  { MAD_F(0x05051f37) /* 0.313750472 */, 15 },
  /* 1022 */  { MAD_F(0x0506cceb) /* 0.314160269 */, 15 },
  /* 1023 */  { MAD_F(0x05087ac2) /* 0.314570199 */, 15 },

  /* 1024 */  { MAD_F(0x050a28be) /* 0.314980262 */, 15 },
  /* 1025 */  { MAD_F(0x050bd6de) /* 0.315390460 */, 15 },
  /* 1026 */  { MAD_F(0x050d8521) /* 0.315800790 */, 15 },
  /* 1027 */  { MAD_F(0x050f3388) /* 0.316211255 */, 15 },
  /* 1028 */  { MAD_F(0x0510e213) /* 0.316621852 */, 15 },
  /* 1029 */  { MAD_F(0x051290c2) /* 0.317032582 */, 15 },
  /* 1030 */  { MAD_F(0x05143f94) /* 0.317443446 */, 15 },
  /* 1031 */  { MAD_F(0x0515ee8a) /* 0.317854442 */, 15 },
  /* 1032 */  { MAD_F(0x05179da4) /* 0.318265572 */, 15 },
  /* 1033 */  { MAD_F(0x05194ce1) /* 0.318676834 */, 15 },
  /* 1034 */  { MAD_F(0x051afc42) /* 0.319088229 */, 15 },
  /* 1035 */  { MAD_F(0x051cabc7) /* 0.319499756 */, 15 },
  /* 1036 */  { MAD_F(0x051e5b6f) /* 0.319911417 */, 15 },
  /* 1037 */  { MAD_F(0x05200b3a) /* 0.320323209 */, 15 },
  /* 1038 */  { MAD_F(0x0521bb2a) /* 0.320735134 */, 15 },
  /* 1039 */  { MAD_F(0x05236b3d) /* 0.321147192 */, 15 },

  /* 1040 */  { MAD_F(0x05251b73) /* 0.321559381 */, 15 },
  /* 1041 */  { MAD_F(0x0526cbcd) /* 0.321971703 */, 15 },
  /* 1042 */  { MAD_F(0x05287c4a) /* 0.322384156 */, 15 },
  /* 1043 */  { MAD_F(0x052a2cea) /* 0.322796742 */, 15 },
  /* 1044 */  { MAD_F(0x052bddae) /* 0.323209460 */, 15 },
  /* 1045 */  { MAD_F(0x052d8e96) /* 0.323622309 */, 15 },
  /* 1046 */  { MAD_F(0x052f3fa1) /* 0.324035290 */, 15 },
  /* 1047 */  { MAD_F(0x0530f0cf) /* 0.324448403 */, 15 },
  /* 1048 */  { MAD_F(0x0532a220) /* 0.324861647 */, 15 },
  /* 1049 */  { MAD_F(0x05345395) /* 0.325275023 */, 15 },
  /* 1050 */  { MAD_F(0x0536052d) /* 0.325688530 */, 15 },
  /* 1051 */  { MAD_F(0x0537b6e8) /* 0.326102168 */, 15 },
  /* 1052 */  { MAD_F(0x053968c6) /* 0.326515938 */, 15 },
  /* 1053 */  { MAD_F(0x053b1ac8) /* 0.326929839 */, 15 },
  /* 1054 */  { MAD_F(0x053ccced) /* 0.327343870 */, 15 },
  /* 1055 */  { MAD_F(0x053e7f35) /* 0.327758033 */, 15 },

  /* 1056 */  { MAD_F(0x054031a0) /* 0.328172327 */, 15 },
  /* 1057 */  { MAD_F(0x0541e42e) /* 0.328586751 */, 15 },
  /* 1058 */  { MAD_F(0x054396df) /* 0.329001306 */, 15 },
  /* 1059 */  { MAD_F(0x054549b4) /* 0.329415992 */, 15 },
  /* 1060 */  { MAD_F(0x0546fcab) /* 0.329830808 */, 15 },
  /* 1061 */  { MAD_F(0x0548afc6) /* 0.330245755 */, 15 },
  /* 1062 */  { MAD_F(0x054a6303) /* 0.330660832 */, 15 },
  /* 1063 */  { MAD_F(0x054c1663) /* 0.331076039 */, 15 },
  /* 1064 */  { MAD_F(0x054dc9e7) /* 0.331491377 */, 15 },
  /* 1065 */  { MAD_F(0x054f7d8d) /* 0.331906845 */, 15 },
  /* 1066 */  { MAD_F(0x05513156) /* 0.332322443 */, 15 },
  /* 1067 */  { MAD_F(0x0552e542) /* 0.332738170 */, 15 },
  /* 1068 */  { MAD_F(0x05549951) /* 0.333154028 */, 15 },
  /* 1069 */  { MAD_F(0x05564d83) /* 0.333570016 */, 15 },
  /* 1070 */  { MAD_F(0x055801d8) /* 0.333986133 */, 15 },
  /* 1071 */  { MAD_F(0x0559b64f) /* 0.334402380 */, 15 },

  /* 1072 */  { MAD_F(0x055b6ae9) /* 0.334818756 */, 15 },
  /* 1073 */  { MAD_F(0x055d1fa6) /* 0.335235262 */, 15 },
  /* 1074 */  { MAD_F(0x055ed486) /* 0.335651898 */, 15 },
  /* 1075 */  { MAD_F(0x05608988) /* 0.336068662 */, 15 },
  /* 1076 */  { MAD_F(0x05623ead) /* 0.336485556 */, 15 },
  /* 1077 */  { MAD_F(0x0563f3f5) /* 0.336902579 */, 15 },
  /* 1078 */  { MAD_F(0x0565a960) /* 0.337319732 */, 15 },
  /* 1079 */  { MAD_F(0x05675eed) /* 0.337737013 */, 15 },
  /* 1080 */  { MAD_F(0x0569149c) /* 0.338154423 */, 15 },
  /* 1081 */  { MAD_F(0x056aca6f) /* 0.338571962 */, 15 },
  /* 1082 */  { MAD_F(0x056c8064) /* 0.338989630 */, 15 },
  /* 1083 */  { MAD_F(0x056e367b) /* 0.339407426 */, 15 },
  /* 1084 */  { MAD_F(0x056fecb5) /* 0.339825351 */, 15 },
  /* 1085 */  { MAD_F(0x0571a311) /* 0.340243405 */, 15 },
  /* 1086 */  { MAD_F(0x05735990) /* 0.340661587 */, 15 },
  /* 1087 */  { MAD_F(0x05751032) /* 0.341079898 */, 15 },

  /* 1088 */  { MAD_F(0x0576c6f5) /* 0.341498336 */, 15 },
  /* 1089 */  { MAD_F(0x05787ddc) /* 0.341916903 */, 15 },
  /* 1090 */  { MAD_F(0x057a34e4) /* 0.342335598 */, 15 },
  /* 1091 */  { MAD_F(0x057bec0f) /* 0.342754421 */, 15 },
  /* 1092 */  { MAD_F(0x057da35d) /* 0.343173373 */, 15 },
  /* 1093 */  { MAD_F(0x057f5acc) /* 0.343592452 */, 15 },
  /* 1094 */  { MAD_F(0x0581125e) /* 0.344011659 */, 15 },
  /* 1095 */  { MAD_F(0x0582ca12) /* 0.344430993 */, 15 },
  /* 1096 */  { MAD_F(0x058481e9) /* 0.344850455 */, 15 },
  /* 1097 */  { MAD_F(0x058639e2) /* 0.345270045 */, 15 },
  /* 1098 */  { MAD_F(0x0587f1fd) /* 0.345689763 */, 15 },
  /* 1099 */  { MAD_F(0x0589aa3a) /* 0.346109608 */, 15 },
  /* 1100 */  { MAD_F(0x058b629a) /* 0.346529580 */, 15 },
  /* 1101 */  { MAD_F(0x058d1b1b) /* 0.346949679 */, 15 },
  /* 1102 */  { MAD_F(0x058ed3bf) /* 0.347369906 */, 15 },
  /* 1103 */  { MAD_F(0x05908c85) /* 0.347790260 */, 15 },

  /* 1104 */  { MAD_F(0x0592456d) /* 0.348210741 */, 15 },
  /* 1105 */  { MAD_F(0x0593fe77) /* 0.348631348 */, 15 },
  /* 1106 */  { MAD_F(0x0595b7a3) /* 0.349052083 */, 15 },
  /* 1107 */  { MAD_F(0x059770f1) /* 0.349472945 */, 15 },
  /* 1108 */  { MAD_F(0x05992a61) /* 0.349893933 */, 15 },
  /* 1109 */  { MAD_F(0x059ae3f3) /* 0.350315048 */, 15 },
  /* 1110 */  { MAD_F(0x059c9da8) /* 0.350736290 */, 15 },
  /* 1111 */  { MAD_F(0x059e577e) /* 0.351157658 */, 15 },
  /* 1112 */  { MAD_F(0x05a01176) /* 0.351579152 */, 15 },
  /* 1113 */  { MAD_F(0x05a1cb90) /* 0.352000773 */, 15 },
  /* 1114 */  { MAD_F(0x05a385cc) /* 0.352422520 */, 15 },
  /* 1115 */  { MAD_F(0x05a5402a) /* 0.352844394 */, 15 },
  /* 1116 */  { MAD_F(0x05a6faa9) /* 0.353266393 */, 15 },
  /* 1117 */  { MAD_F(0x05a8b54b) /* 0.353688519 */, 15 },
  /* 1118 */  { MAD_F(0x05aa700e) /* 0.354110771 */, 15 },
  /* 1119 */  { MAD_F(0x05ac2af3) /* 0.354533148 */, 15 },

  /* 1120 */  { MAD_F(0x05ade5fa) /* 0.354955651 */, 15 },
  /* 1121 */  { MAD_F(0x05afa123) /* 0.355378281 */, 15 },
  /* 1122 */  { MAD_F(0x05b15c6d) /* 0.355801035 */, 15 },
  /* 1123 */  { MAD_F(0x05b317d9) /* 0.356223916 */, 15 },
  /* 1124 */  { MAD_F(0x05b4d367) /* 0.356646922 */, 15 },
  /* 1125 */  { MAD_F(0x05b68f16) /* 0.357070053 */, 15 },
  /* 1126 */  { MAD_F(0x05b84ae7) /* 0.357493310 */, 15 },
  /* 1127 */  { MAD_F(0x05ba06da) /* 0.357916692 */, 15 },
  /* 1128 */  { MAD_F(0x05bbc2ef) /* 0.358340200 */, 15 },
  /* 1129 */  { MAD_F(0x05bd7f25) /* 0.358763832 */, 15 },
  /* 1130 */  { MAD_F(0x05bf3b7c) /* 0.359187590 */, 15 },
  /* 1131 */  { MAD_F(0x05c0f7f5) /* 0.359611472 */, 15 },
  /* 1132 */  { MAD_F(0x05c2b490) /* 0.360035480 */, 15 },
  /* 1133 */  { MAD_F(0x05c4714c) /* 0.360459613 */, 15 },
  /* 1134 */  { MAD_F(0x05c62e2a) /* 0.360883870 */, 15 },
  /* 1135 */  { MAD_F(0x05c7eb29) /* 0.361308252 */, 15 },

  /* 1136 */  { MAD_F(0x05c9a84a) /* 0.361732758 */, 15 },
  /* 1137 */  { MAD_F(0x05cb658c) /* 0.362157390 */, 15 },
  /* 1138 */  { MAD_F(0x05cd22ef) /* 0.362582145 */, 15 },
  /* 1139 */  { MAD_F(0x05cee074) /* 0.363007026 */, 15 },
  /* 1140 */  { MAD_F(0x05d09e1b) /* 0.363432030 */, 15 },
  /* 1141 */  { MAD_F(0x05d25be2) /* 0.363857159 */, 15 },
  /* 1142 */  { MAD_F(0x05d419cb) /* 0.364282412 */, 15 },
  /* 1143 */  { MAD_F(0x05d5d7d5) /* 0.364707789 */, 15 },
  /* 1144 */  { MAD_F(0x05d79601) /* 0.365133291 */, 15 },
  /* 1145 */  { MAD_F(0x05d9544e) /* 0.365558916 */, 15 },
  /* 1146 */  { MAD_F(0x05db12bc) /* 0.365984665 */, 15 },
  /* 1147 */  { MAD_F(0x05dcd14c) /* 0.366410538 */, 15 },
  /* 1148 */  { MAD_F(0x05de8ffc) /* 0.366836535 */, 15 },
  /* 1149 */  { MAD_F(0x05e04ece) /* 0.367262655 */, 15 },
  /* 1150 */  { MAD_F(0x05e20dc1) /* 0.367688900 */, 15 },
  /* 1151 */  { MAD_F(0x05e3ccd5) /* 0.368115267 */, 15 },

  /* 1152 */  { MAD_F(0x05e58c0b) /* 0.368541759 */, 15 },
  /* 1153 */  { MAD_F(0x05e74b61) /* 0.368968373 */, 15 },
  /* 1154 */  { MAD_F(0x05e90ad9) /* 0.369395111 */, 15 },
  /* 1155 */  { MAD_F(0x05eaca72) /* 0.369821973 */, 15 },
  /* 1156 */  { MAD_F(0x05ec8a2b) /* 0.370248957 */, 15 },
  /* 1157 */  { MAD_F(0x05ee4a06) /* 0.370676065 */, 15 },
  /* 1158 */  { MAD_F(0x05f00a02) /* 0.371103295 */, 15 },
  /* 1159 */  { MAD_F(0x05f1ca1f) /* 0.371530649 */, 15 },
  /* 1160 */  { MAD_F(0x05f38a5d) /* 0.371958126 */, 15 },
  /* 1161 */  { MAD_F(0x05f54abc) /* 0.372385725 */, 15 },
  /* 1162 */  { MAD_F(0x05f70b3c) /* 0.372813448 */, 15 },
  /* 1163 */  { MAD_F(0x05f8cbdc) /* 0.373241292 */, 15 },
  /* 1164 */  { MAD_F(0x05fa8c9e) /* 0.373669260 */, 15 },
  /* 1165 */  { MAD_F(0x05fc4d81) /* 0.374097350 */, 15 },
  /* 1166 */  { MAD_F(0x05fe0e84) /* 0.374525563 */, 15 },
  /* 1167 */  { MAD_F(0x05ffcfa8) /* 0.374953898 */, 15 },

  /* 1168 */  { MAD_F(0x060190ee) /* 0.375382356 */, 15 },
  /* 1169 */  { MAD_F(0x06035254) /* 0.375810936 */, 15 },
  /* 1170 */  { MAD_F(0x060513da) /* 0.376239638 */, 15 },
  /* 1171 */  { MAD_F(0x0606d582) /* 0.376668462 */, 15 },
  /* 1172 */  { MAD_F(0x0608974a) /* 0.377097408 */, 15 },
  /* 1173 */  { MAD_F(0x060a5934) /* 0.377526476 */, 15 },
  /* 1174 */  { MAD_F(0x060c1b3d) /* 0.377955667 */, 15 },
  /* 1175 */  { MAD_F(0x060ddd68) /* 0.378384979 */, 15 },
  /* 1176 */  { MAD_F(0x060f9fb3) /* 0.378814413 */, 15 },
  /* 1177 */  { MAD_F(0x0611621f) /* 0.379243968 */, 15 },
  /* 1178 */  { MAD_F(0x061324ac) /* 0.379673646 */, 15 },
  /* 1179 */  { MAD_F(0x0614e759) /* 0.380103444 */, 15 },
  /* 1180 */  { MAD_F(0x0616aa27) /* 0.380533365 */, 15 },
  /* 1181 */  { MAD_F(0x06186d16) /* 0.380963407 */, 15 },
  /* 1182 */  { MAD_F(0x061a3025) /* 0.381393570 */, 15 },
  /* 1183 */  { MAD_F(0x061bf354) /* 0.381823855 */, 15 },

  /* 1184 */  { MAD_F(0x061db6a5) /* 0.382254261 */, 15 },
  /* 1185 */  { MAD_F(0x061f7a15) /* 0.382684788 */, 15 },
  /* 1186 */  { MAD_F(0x06213da7) /* 0.383115436 */, 15 },
  /* 1187 */  { MAD_F(0x06230158) /* 0.383546205 */, 15 },
  /* 1188 */  { MAD_F(0x0624c52a) /* 0.383977096 */, 15 },
  /* 1189 */  { MAD_F(0x0626891d) /* 0.384408107 */, 15 },
  /* 1190 */  { MAD_F(0x06284d30) /* 0.384839239 */, 15 },
  /* 1191 */  { MAD_F(0x062a1164) /* 0.385270492 */, 15 },
  /* 1192 */  { MAD_F(0x062bd5b8) /* 0.385701865 */, 15 },
  /* 1193 */  { MAD_F(0x062d9a2c) /* 0.386133359 */, 15 },
  /* 1194 */  { MAD_F(0x062f5ec1) /* 0.386564974 */, 15 },
  /* 1195 */  { MAD_F(0x06312376) /* 0.386996709 */, 15 },
  /* 1196 */  { MAD_F(0x0632e84b) /* 0.387428565 */, 15 },
  /* 1197 */  { MAD_F(0x0634ad41) /* 0.387860541 */, 15 },
  /* 1198 */  { MAD_F(0x06367257) /* 0.388292637 */, 15 },
  /* 1199 */  { MAD_F(0x0638378d) /* 0.388724854 */, 15 },

  /* 1200 */  { MAD_F(0x0639fce4) /* 0.389157191 */, 15 },
  /* 1201 */  { MAD_F(0x063bc25b) /* 0.389589648 */, 15 },
  /* 1202 */  { MAD_F(0x063d87f2) /* 0.390022225 */, 15 },
  /* 1203 */  { MAD_F(0x063f4da9) /* 0.390454922 */, 15 },
  /* 1204 */  { MAD_F(0x06411380) /* 0.390887739 */, 15 },
  /* 1205 */  { MAD_F(0x0642d978) /* 0.391320675 */, 15 },
  /* 1206 */  { MAD_F(0x06449f8f) /* 0.391753732 */, 15 },
  /* 1207 */  { MAD_F(0x064665c7) /* 0.392186908 */, 15 },
  /* 1208 */  { MAD_F(0x06482c1f) /* 0.392620204 */, 15 },
  /* 1209 */  { MAD_F(0x0649f297) /* 0.393053619 */, 15 },
  /* 1210 */  { MAD_F(0x064bb92f) /* 0.393487154 */, 15 },
  /* 1211 */  { MAD_F(0x064d7fe8) /* 0.393920808 */, 15 },
  /* 1212 */  { MAD_F(0x064f46c0) /* 0.394354582 */, 15 },
  /* 1213 */  { MAD_F(0x06510db8) /* 0.394788475 */, 15 },
  /* 1214 */  { MAD_F(0x0652d4d0) /* 0.395222488 */, 15 },
  /* 1215 */  { MAD_F(0x06549c09) /* 0.395656619 */, 15 },

  /* 1216 */  { MAD_F(0x06566361) /* 0.396090870 */, 15 },
  /* 1217 */  { MAD_F(0x06582ad9) /* 0.396525239 */, 15 },
  /* 1218 */  { MAD_F(0x0659f271) /* 0.396959728 */, 15 },
  /* 1219 */  { MAD_F(0x065bba29) /* 0.397394336 */, 15 },
  /* 1220 */  { MAD_F(0x065d8201) /* 0.397829062 */, 15 },
  /* 1221 */  { MAD_F(0x065f49f9) /* 0.398263907 */, 15 },
  /* 1222 */  { MAD_F(0x06611211) /* 0.398698871 */, 15 },
  /* 1223 */  { MAD_F(0x0662da49) /* 0.399133954 */, 15 },
  /* 1224 */  { MAD_F(0x0664a2a0) /* 0.399569155 */, 15 },
  /* 1225 */  { MAD_F(0x06666b17) /* 0.400004475 */, 15 },
  /* 1226 */  { MAD_F(0x066833ae) /* 0.400439913 */, 15 },
  /* 1227 */  { MAD_F(0x0669fc65) /* 0.400875470 */, 15 },
  /* 1228 */  { MAD_F(0x066bc53c) /* 0.401311145 */, 15 },
  /* 1229 */  { MAD_F(0x066d8e32) /* 0.401746938 */, 15 },
  /* 1230 */  { MAD_F(0x066f5748) /* 0.402182850 */, 15 },
  /* 1231 */  { MAD_F(0x0671207e) /* 0.402618879 */, 15 },

  /* 1232 */  { MAD_F(0x0672e9d4) /* 0.403055027 */, 15 },
  /* 1233 */  { MAD_F(0x0674b349) /* 0.403491293 */, 15 },
  /* 1234 */  { MAD_F(0x06767cde) /* 0.403927676 */, 15 },
  /* 1235 */  { MAD_F(0x06784692) /* 0.404364178 */, 15 },
  /* 1236 */  { MAD_F(0x067a1066) /* 0.404800797 */, 15 },
  /* 1237 */  { MAD_F(0x067bda5a) /* 0.405237535 */, 15 },
  /* 1238 */  { MAD_F(0x067da46d) /* 0.405674390 */, 15 },
  /* 1239 */  { MAD_F(0x067f6ea0) /* 0.406111362 */, 15 },
  /* 1240 */  { MAD_F(0x068138f3) /* 0.406548452 */, 15 },
  /* 1241 */  { MAD_F(0x06830365) /* 0.406985660 */, 15 },
  /* 1242 */  { MAD_F(0x0684cdf6) /* 0.407422985 */, 15 },
  /* 1243 */  { MAD_F(0x068698a8) /* 0.407860427 */, 15 },
  /* 1244 */  { MAD_F(0x06886378) /* 0.408297987 */, 15 },
  /* 1245 */  { MAD_F(0x068a2e68) /* 0.408735664 */, 15 },
  /* 1246 */  { MAD_F(0x068bf978) /* 0.409173458 */, 15 },
  /* 1247 */  { MAD_F(0x068dc4a7) /* 0.409611370 */, 15 },

  /* 1248 */  { MAD_F(0x068f8ff5) /* 0.410049398 */, 15 },
  /* 1249 */  { MAD_F(0x06915b63) /* 0.410487544 */, 15 },
  /* 1250 */  { MAD_F(0x069326f0) /* 0.410925806 */, 15 },
  /* 1251 */  { MAD_F(0x0694f29c) /* 0.411364185 */, 15 },
  /* 1252 */  { MAD_F(0x0696be68) /* 0.411802681 */, 15 },
  /* 1253 */  { MAD_F(0x06988a54) /* 0.412241294 */, 15 },
  /* 1254 */  { MAD_F(0x069a565e) /* 0.412680024 */, 15 },
  /* 1255 */  { MAD_F(0x069c2288) /* 0.413118870 */, 15 },
  /* 1256 */  { MAD_F(0x069deed1) /* 0.413557833 */, 15 },
  /* 1257 */  { MAD_F(0x069fbb3a) /* 0.413996912 */, 15 },
  /* 1258 */  { MAD_F(0x06a187c1) /* 0.414436108 */, 15 },
  /* 1259 */  { MAD_F(0x06a35468) /* 0.414875420 */, 15 },
  /* 1260 */  { MAD_F(0x06a5212f) /* 0.415314849 */, 15 },
  /* 1261 */  { MAD_F(0x06a6ee14) /* 0.415754393 */, 15 },
  /* 1262 */  { MAD_F(0x06a8bb18) /* 0.416194054 */, 15 },
  /* 1263 */  { MAD_F(0x06aa883c) /* 0.416633831 */, 15 },

  /* 1264 */  { MAD_F(0x06ac557f) /* 0.417073724 */, 15 },
  /* 1265 */  { MAD_F(0x06ae22e1) /* 0.417513734 */, 15 },
  /* 1266 */  { MAD_F(0x06aff062) /* 0.417953859 */, 15 },
  /* 1267 */  { MAD_F(0x06b1be03) /* 0.418394100 */, 15 },
  /* 1268 */  { MAD_F(0x06b38bc2) /* 0.418834457 */, 15 },
  /* 1269 */  { MAD_F(0x06b559a1) /* 0.419274929 */, 15 },
  /* 1270 */  { MAD_F(0x06b7279e) /* 0.419715518 */, 15 },
  /* 1271 */  { MAD_F(0x06b8f5bb) /* 0.420156222 */, 15 },
  /* 1272 */  { MAD_F(0x06bac3f6) /* 0.420597041 */, 15 },
  /* 1273 */  { MAD_F(0x06bc9251) /* 0.421037977 */, 15 },
  /* 1274 */  { MAD_F(0x06be60cb) /* 0.421479027 */, 15 },
  /* 1275 */  { MAD_F(0x06c02f63) /* 0.421920193 */, 15 },
  /* 1276 */  { MAD_F(0x06c1fe1b) /* 0.422361475 */, 15 },
  /* 1277 */  { MAD_F(0x06c3ccf1) /* 0.422802871 */, 15 },
  /* 1278 */  { MAD_F(0x06c59be7) /* 0.423244383 */, 15 },
  /* 1279 */  { MAD_F(0x06c76afb) /* 0.423686010 */, 15 },

  /* 1280 */  { MAD_F(0x06c93a2e) /* 0.424127753 */, 15 },
  /* 1281 */  { MAD_F(0x06cb0981) /* 0.424569610 */, 15 },
  /* 1282 */  { MAD_F(0x06ccd8f2) /* 0.425011582 */, 15 },
  /* 1283 */  { MAD_F(0x06cea881) /* 0.425453669 */, 15 },
  /* 1284 */  { MAD_F(0x06d07830) /* 0.425895871 */, 15 },
  /* 1285 */  { MAD_F(0x06d247fe) /* 0.426338188 */, 15 },
  /* 1286 */  { MAD_F(0x06d417ea) /* 0.426780620 */, 15 },
  /* 1287 */  { MAD_F(0x06d5e7f5) /* 0.427223166 */, 15 },
  /* 1288 */  { MAD_F(0x06d7b81f) /* 0.427665827 */, 15 },
  /* 1289 */  { MAD_F(0x06d98868) /* 0.428108603 */, 15 },
  /* 1290 */  { MAD_F(0x06db58cf) /* 0.428551493 */, 15 },
  /* 1291 */  { MAD_F(0x06dd2955) /* 0.428994497 */, 15 },
  /* 1292 */  { MAD_F(0x06def9fa) /* 0.429437616 */, 15 },
  /* 1293 */  { MAD_F(0x06e0cabe) /* 0.429880849 */, 15 },
  /* 1294 */  { MAD_F(0x06e29ba0) /* 0.430324197 */, 15 },
  /* 1295 */  { MAD_F(0x06e46ca1) /* 0.430767659 */, 15 },

  /* 1296 */  { MAD_F(0x06e63dc0) /* 0.431211234 */, 15 },
  /* 1297 */  { MAD_F(0x06e80efe) /* 0.431654924 */, 15 },
  /* 1298 */  { MAD_F(0x06e9e05b) /* 0.432098728 */, 15 },
  /* 1299 */  { MAD_F(0x06ebb1d6) /* 0.432542647 */, 15 },
  /* 1300 */  { MAD_F(0x06ed8370) /* 0.432986678 */, 15 },
  /* 1301 */  { MAD_F(0x06ef5529) /* 0.433430824 */, 15 },
  /* 1302 */  { MAD_F(0x06f12700) /* 0.433875084 */, 15 },
  /* 1303 */  { MAD_F(0x06f2f8f5) /* 0.434319457 */, 15 },
  /* 1304 */  { MAD_F(0x06f4cb09) /* 0.434763944 */, 15 },
  /* 1305 */  { MAD_F(0x06f69d3c) /* 0.435208545 */, 15 },
  /* 1306 */  { MAD_F(0x06f86f8d) /* 0.435653259 */, 15 },
  /* 1307 */  { MAD_F(0x06fa41fd) /* 0.436098087 */, 15 },
  /* 1308 */  { MAD_F(0x06fc148b) /* 0.436543029 */, 15 },
  /* 1309 */  { MAD_F(0x06fde737) /* 0.436988083 */, 15 },
  /* 1310 */  { MAD_F(0x06ffba02) /* 0.437433251 */, 15 },
  /* 1311 */  { MAD_F(0x07018ceb) /* 0.437878533 */, 15 },

  /* 1312 */  { MAD_F(0x07035ff3) /* 0.438323927 */, 15 },
  /* 1313 */  { MAD_F(0x07053319) /* 0.438769435 */, 15 },
  /* 1314 */  { MAD_F(0x0707065d) /* 0.439215056 */, 15 },
  /* 1315 */  { MAD_F(0x0708d9c0) /* 0.439660790 */, 15 },
  /* 1316 */  { MAD_F(0x070aad41) /* 0.440106636 */, 15 },
  /* 1317 */  { MAD_F(0x070c80e1) /* 0.440552596 */, 15 },
  /* 1318 */  { MAD_F(0x070e549f) /* 0.440998669 */, 15 },
  /* 1319 */  { MAD_F(0x0710287b) /* 0.441444855 */, 15 },
  /* 1320 */  { MAD_F(0x0711fc75) /* 0.441891153 */, 15 },
  /* 1321 */  { MAD_F(0x0713d08d) /* 0.442337564 */, 15 },
  /* 1322 */  { MAD_F(0x0715a4c4) /* 0.442784088 */, 15 },
  /* 1323 */  { MAD_F(0x07177919) /* 0.443230724 */, 15 },
  /* 1324 */  { MAD_F(0x07194d8c) /* 0.443677473 */, 15 },
  /* 1325 */  { MAD_F(0x071b221e) /* 0.444124334 */, 15 },
  /* 1326 */  { MAD_F(0x071cf6ce) /* 0.444571308 */, 15 },
  /* 1327 */  { MAD_F(0x071ecb9b) /* 0.445018394 */, 15 },

  /* 1328 */  { MAD_F(0x0720a087) /* 0.445465593 */, 15 },
  /* 1329 */  { MAD_F(0x07227591) /* 0.445912903 */, 15 },
  /* 1330 */  { MAD_F(0x07244ab9) /* 0.446360326 */, 15 },
  /* 1331 */  { MAD_F(0x07262000) /* 0.446807861 */, 15 },
  /* 1332 */  { MAD_F(0x0727f564) /* 0.447255509 */, 15 },
  /* 1333 */  { MAD_F(0x0729cae7) /* 0.447703268 */, 15 },
  /* 1334 */  { MAD_F(0x072ba087) /* 0.448151139 */, 15 },
  /* 1335 */  { MAD_F(0x072d7646) /* 0.448599122 */, 15 },
  /* 1336 */  { MAD_F(0x072f4c22) /* 0.449047217 */, 15 },
  /* 1337 */  { MAD_F(0x0731221d) /* 0.449495424 */, 15 },
  /* 1338 */  { MAD_F(0x0732f835) /* 0.449943742 */, 15 },
  /* 1339 */  { MAD_F(0x0734ce6c) /* 0.450392173 */, 15 },
  /* 1340 */  { MAD_F(0x0736a4c1) /* 0.450840715 */, 15 },
  /* 1341 */  { MAD_F(0x07387b33) /* 0.451289368 */, 15 },
  /* 1342 */  { MAD_F(0x073a51c4) /* 0.451738133 */, 15 },
  /* 1343 */  { MAD_F(0x073c2872) /* 0.452187010 */, 15 },

  /* 1344 */  { MAD_F(0x073dff3e) /* 0.452635998 */, 15 },
  /* 1345 */  { MAD_F(0x073fd628) /* 0.453085097 */, 15 },
  /* 1346 */  { MAD_F(0x0741ad30) /* 0.453534308 */, 15 },
  /* 1347 */  { MAD_F(0x07438456) /* 0.453983630 */, 15 },
  /* 1348 */  { MAD_F(0x07455b9a) /* 0.454433063 */, 15 },
  /* 1349 */  { MAD_F(0x074732fc) /* 0.454882607 */, 15 },
  /* 1350 */  { MAD_F(0x07490a7b) /* 0.455332262 */, 15 },
  /* 1351 */  { MAD_F(0x074ae218) /* 0.455782029 */, 15 },
  /* 1352 */  { MAD_F(0x074cb9d3) /* 0.456231906 */, 15 },
  /* 1353 */  { MAD_F(0x074e91ac) /* 0.456681894 */, 15 },
  /* 1354 */  { MAD_F(0x075069a3) /* 0.457131993 */, 15 },
  /* 1355 */  { MAD_F(0x075241b7) /* 0.457582203 */, 15 },
  /* 1356 */  { MAD_F(0x075419e9) /* 0.458032524 */, 15 },
  /* 1357 */  { MAD_F(0x0755f239) /* 0.458482956 */, 15 },
  /* 1358 */  { MAD_F(0x0757caa7) /* 0.458933498 */, 15 },
  /* 1359 */  { MAD_F(0x0759a332) /* 0.459384151 */, 15 },

  /* 1360 */  { MAD_F(0x075b7bdb) /* 0.459834914 */, 15 },
  /* 1361 */  { MAD_F(0x075d54a1) /* 0.460285788 */, 15 },
  /* 1362 */  { MAD_F(0x075f2d85) /* 0.460736772 */, 15 },
  /* 1363 */  { MAD_F(0x07610687) /* 0.461187867 */, 15 },
  /* 1364 */  { MAD_F(0x0762dfa6) /* 0.461639071 */, 15 },
  /* 1365 */  { MAD_F(0x0764b8e3) /* 0.462090387 */, 15 },
  /* 1366 */  { MAD_F(0x0766923e) /* 0.462541812 */, 15 },
  /* 1367 */  { MAD_F(0x07686bb6) /* 0.462993348 */, 15 },
  /* 1368 */  { MAD_F(0x076a454c) /* 0.463444993 */, 15 },
  /* 1369 */  { MAD_F(0x076c1eff) /* 0.463896749 */, 15 },
  /* 1370 */  { MAD_F(0x076df8d0) /* 0.464348615 */, 15 },
  /* 1371 */  { MAD_F(0x076fd2be) /* 0.464800591 */, 15 },
  /* 1372 */  { MAD_F(0x0771acca) /* 0.465252676 */, 15 },
  /* 1373 */  { MAD_F(0x077386f3) /* 0.465704872 */, 15 },
  /* 1374 */  { MAD_F(0x0775613a) /* 0.466157177 */, 15 },
  /* 1375 */  { MAD_F(0x07773b9e) /* 0.466609592 */, 15 },

  /* 1376 */  { MAD_F(0x07791620) /* 0.467062117 */, 15 },
  /* 1377 */  { MAD_F(0x077af0bf) /* 0.467514751 */, 15 },
  /* 1378 */  { MAD_F(0x077ccb7c) /* 0.467967495 */, 15 },
  /* 1379 */  { MAD_F(0x077ea656) /* 0.468420349 */, 15 },
  /* 1380 */  { MAD_F(0x0780814d) /* 0.468873312 */, 15 },
  /* 1381 */  { MAD_F(0x07825c62) /* 0.469326384 */, 15 },
  /* 1382 */  { MAD_F(0x07843794) /* 0.469779566 */, 15 },
  /* 1383 */  { MAD_F(0x078612e3) /* 0.470232857 */, 15 },
  /* 1384 */  { MAD_F(0x0787ee50) /* 0.470686258 */, 15 },
  /* 1385 */  { MAD_F(0x0789c9da) /* 0.471139767 */, 15 },
  /* 1386 */  { MAD_F(0x078ba581) /* 0.471593386 */, 15 },
  /* 1387 */  { MAD_F(0x078d8146) /* 0.472047114 */, 15 },
  /* 1388 */  { MAD_F(0x078f5d28) /* 0.472500951 */, 15 },
  /* 1389 */  { MAD_F(0x07913927) /* 0.472954896 */, 15 },
  /* 1390 */  { MAD_F(0x07931543) /* 0.473408951 */, 15 },
  /* 1391 */  { MAD_F(0x0794f17d) /* 0.473863115 */, 15 },

  /* 1392 */  { MAD_F(0x0796cdd4) /* 0.474317388 */, 15 },
  /* 1393 */  { MAD_F(0x0798aa48) /* 0.474771769 */, 15 },
  /* 1394 */  { MAD_F(0x079a86d9) /* 0.475226259 */, 15 },
  /* 1395 */  { MAD_F(0x079c6388) /* 0.475680858 */, 15 },
  /* 1396 */  { MAD_F(0x079e4053) /* 0.476135565 */, 15 },
  /* 1397 */  { MAD_F(0x07a01d3c) /* 0.476590381 */, 15 },
  /* 1398 */  { MAD_F(0x07a1fa42) /* 0.477045306 */, 15 },
  /* 1399 */  { MAD_F(0x07a3d765) /* 0.477500339 */, 15 },
  /* 1400 */  { MAD_F(0x07a5b4a5) /* 0.477955481 */, 15 },
  /* 1401 */  { MAD_F(0x07a79202) /* 0.478410731 */, 15 },
  /* 1402 */  { MAD_F(0x07a96f7d) /* 0.478866089 */, 15 },
  /* 1403 */  { MAD_F(0x07ab4d14) /* 0.479321555 */, 15 },
  /* 1404 */  { MAD_F(0x07ad2ac8) /* 0.479777130 */, 15 },
  /* 1405 */  { MAD_F(0x07af089a) /* 0.480232813 */, 15 },
  /* 1406 */  { MAD_F(0x07b0e688) /* 0.480688604 */, 15 },
  /* 1407 */  { MAD_F(0x07b2c494) /* 0.481144503 */, 15 },

  /* 1408 */  { MAD_F(0x07b4a2bc) /* 0.481600510 */, 15 },
  /* 1409 */  { MAD_F(0x07b68102) /* 0.482056625 */, 15 },
  /* 1410 */  { MAD_F(0x07b85f64) /* 0.482512848 */, 15 },
  /* 1411 */  { MAD_F(0x07ba3de4) /* 0.482969179 */, 15 },
  /* 1412 */  { MAD_F(0x07bc1c80) /* 0.483425618 */, 15 },
  /* 1413 */  { MAD_F(0x07bdfb39) /* 0.483882164 */, 15 },
  /* 1414 */  { MAD_F(0x07bfda0f) /* 0.484338818 */, 15 },
  /* 1415 */  { MAD_F(0x07c1b902) /* 0.484795580 */, 15 },
  /* 1416 */  { MAD_F(0x07c39812) /* 0.485252449 */, 15 },
  /* 1417 */  { MAD_F(0x07c5773f) /* 0.485709426 */, 15 },
  /* 1418 */  { MAD_F(0x07c75689) /* 0.486166511 */, 15 },
  /* 1419 */  { MAD_F(0x07c935ef) /* 0.486623703 */, 15 },
  /* 1420 */  { MAD_F(0x07cb1573) /* 0.487081002 */, 15 },
  /* 1421 */  { MAD_F(0x07ccf513) /* 0.487538409 */, 15 },
  /* 1422 */  { MAD_F(0x07ced4d0) /* 0.487995923 */, 15 },
  /* 1423 */  { MAD_F(0x07d0b4aa) /* 0.488453544 */, 15 },

  /* 1424 */  { MAD_F(0x07d294a0) /* 0.488911273 */, 15 },
  /* 1425 */  { MAD_F(0x07d474b3) /* 0.489369108 */, 15 },
  /* 1426 */  { MAD_F(0x07d654e4) /* 0.489827051 */, 15 },
  /* 1427 */  { MAD_F(0x07d83530) /* 0.490285101 */, 15 },
  /* 1428 */  { MAD_F(0x07da159a) /* 0.490743258 */, 15 },
  /* 1429 */  { MAD_F(0x07dbf620) /* 0.491201522 */, 15 },
  /* 1430 */  { MAD_F(0x07ddd6c3) /* 0.491659892 */, 15 },
  /* 1431 */  { MAD_F(0x07dfb783) /* 0.492118370 */, 15 },
  /* 1432 */  { MAD_F(0x07e1985f) /* 0.492576954 */, 15 },
  /* 1433 */  { MAD_F(0x07e37958) /* 0.493035645 */, 15 },
  /* 1434 */  { MAD_F(0x07e55a6e) /* 0.493494443 */, 15 },
  /* 1435 */  { MAD_F(0x07e73ba0) /* 0.493953348 */, 15 },
  /* 1436 */  { MAD_F(0x07e91cef) /* 0.494412359 */, 15 },
  /* 1437 */  { MAD_F(0x07eafe5a) /* 0.494871476 */, 15 },
  /* 1438 */  { MAD_F(0x07ecdfe2) /* 0.495330701 */, 15 },
  /* 1439 */  { MAD_F(0x07eec187) /* 0.495790031 */, 15 },

  /* 1440 */  { MAD_F(0x07f0a348) /* 0.496249468 */, 15 },
  /* 1441 */  { MAD_F(0x07f28526) /* 0.496709012 */, 15 },
  /* 1442 */  { MAD_F(0x07f46720) /* 0.497168662 */, 15 },
  /* 1443 */  { MAD_F(0x07f64937) /* 0.497628418 */, 15 },
  /* 1444 */  { MAD_F(0x07f82b6a) /* 0.498088280 */, 15 },
  /* 1445 */  { MAD_F(0x07fa0dba) /* 0.498548248 */, 15 },
  /* 1446 */  { MAD_F(0x07fbf026) /* 0.499008323 */, 15 },
  /* 1447 */  { MAD_F(0x07fdd2af) /* 0.499468503 */, 15 },
  /* 1448 */  { MAD_F(0x07ffb554) /* 0.499928790 */, 15 },
  /* 1449 */  { MAD_F(0x0400cc0b) /* 0.250194591 */, 16 },
  /* 1450 */  { MAD_F(0x0401bd7a) /* 0.250424840 */, 16 },
  /* 1451 */  { MAD_F(0x0402aef7) /* 0.250655143 */, 16 },
  /* 1452 */  { MAD_F(0x0403a083) /* 0.250885498 */, 16 },
  /* 1453 */  { MAD_F(0x0404921c) /* 0.251115906 */, 16 },
  /* 1454 */  { MAD_F(0x040583c4) /* 0.251346367 */, 16 },
  /* 1455 */  { MAD_F(0x0406757a) /* 0.251576880 */, 16 },

  /* 1456 */  { MAD_F(0x0407673f) /* 0.251807447 */, 16 },
  /* 1457 */  { MAD_F(0x04085911) /* 0.252038066 */, 16 },
  /* 1458 */  { MAD_F(0x04094af1) /* 0.252268738 */, 16 },
  /* 1459 */  { MAD_F(0x040a3ce0) /* 0.252499463 */, 16 },
  /* 1460 */  { MAD_F(0x040b2edd) /* 0.252730240 */, 16 },
  /* 1461 */  { MAD_F(0x040c20e8) /* 0.252961071 */, 16 },
  /* 1462 */  { MAD_F(0x040d1301) /* 0.253191953 */, 16 },
  /* 1463 */  { MAD_F(0x040e0529) /* 0.253422889 */, 16 },
  /* 1464 */  { MAD_F(0x040ef75e) /* 0.253653877 */, 16 },
  /* 1465 */  { MAD_F(0x040fe9a1) /* 0.253884918 */, 16 },
  /* 1466 */  { MAD_F(0x0410dbf3) /* 0.254116011 */, 16 },
  /* 1467 */  { MAD_F(0x0411ce53) /* 0.254347157 */, 16 },
  /* 1468 */  { MAD_F(0x0412c0c1) /* 0.254578356 */, 16 },
  /* 1469 */  { MAD_F(0x0413b33d) /* 0.254809606 */, 16 },
  /* 1470 */  { MAD_F(0x0414a5c7) /* 0.255040910 */, 16 },
  /* 1471 */  { MAD_F(0x0415985f) /* 0.255272266 */, 16 },

  /* 1472 */  { MAD_F(0x04168b05) /* 0.255503674 */, 16 },
  /* 1473 */  { MAD_F(0x04177db9) /* 0.255735135 */, 16 },
  /* 1474 */  { MAD_F(0x0418707c) /* 0.255966648 */, 16 },
  /* 1475 */  { MAD_F(0x0419634c) /* 0.256198213 */, 16 },
  /* 1476 */  { MAD_F(0x041a562a) /* 0.256429831 */, 16 },
  /* 1477 */  { MAD_F(0x041b4917) /* 0.256661501 */, 16 },
  /* 1478 */  { MAD_F(0x041c3c11) /* 0.256893223 */, 16 },
  /* 1479 */  { MAD_F(0x041d2f1a) /* 0.257124998 */, 16 },
  /* 1480 */  { MAD_F(0x041e2230) /* 0.257356825 */, 16 },
  /* 1481 */  { MAD_F(0x041f1555) /* 0.257588704 */, 16 },
  /* 1482 */  { MAD_F(0x04200888) /* 0.257820635 */, 16 },
  /* 1483 */  { MAD_F(0x0420fbc8) /* 0.258052619 */, 16 },
  /* 1484 */  { MAD_F(0x0421ef17) /* 0.258284654 */, 16 },
  /* 1485 */  { MAD_F(0x0422e273) /* 0.258516742 */, 16 },
  /* 1486 */  { MAD_F(0x0423d5de) /* 0.258748882 */, 16 },
  /* 1487 */  { MAD_F(0x0424c956) /* 0.258981074 */, 16 },

  /* 1488 */  { MAD_F(0x0425bcdd) /* 0.259213318 */, 16 },
  /* 1489 */  { MAD_F(0x0426b071) /* 0.259445614 */, 16 },
  /* 1490 */  { MAD_F(0x0427a414) /* 0.259677962 */, 16 },
  /* 1491 */  { MAD_F(0x042897c4) /* 0.259910362 */, 16 },
  /* 1492 */  { MAD_F(0x04298b83) /* 0.260142814 */, 16 },
  /* 1493 */  { MAD_F(0x042a7f4f) /* 0.260375318 */, 16 },
  /* 1494 */  { MAD_F(0x042b7329) /* 0.260607874 */, 16 },
  /* 1495 */  { MAD_F(0x042c6711) /* 0.260840481 */, 16 },
  /* 1496 */  { MAD_F(0x042d5b07) /* 0.261073141 */, 16 },
  /* 1497 */  { MAD_F(0x042e4f0b) /* 0.261305852 */, 16 },
  /* 1498 */  { MAD_F(0x042f431d) /* 0.261538616 */, 16 },
  /* 1499 */  { MAD_F(0x0430373d) /* 0.261771431 */, 16 },
  /* 1500 */  { MAD_F(0x04312b6b) /* 0.262004297 */, 16 },
  /* 1501 */  { MAD_F(0x04321fa6) /* 0.262237216 */, 16 },
  /* 1502 */  { MAD_F(0x043313f0) /* 0.262470186 */, 16 },
  /* 1503 */  { MAD_F(0x04340847) /* 0.262703208 */, 16 },

  /* 1504 */  { MAD_F(0x0434fcad) /* 0.262936282 */, 16 },
  /* 1505 */  { MAD_F(0x0435f120) /* 0.263169407 */, 16 },
  /* 1506 */  { MAD_F(0x0436e5a1) /* 0.263402584 */, 16 },
  /* 1507 */  { MAD_F(0x0437da2f) /* 0.263635813 */, 16 },
  /* 1508 */  { MAD_F(0x0438cecc) /* 0.263869093 */, 16 },
  /* 1509 */  { MAD_F(0x0439c377) /* 0.264102425 */, 16 },
  /* 1510 */  { MAD_F(0x043ab82f) /* 0.264335808 */, 16 },
  /* 1511 */  { MAD_F(0x043bacf5) /* 0.264569243 */, 16 },
  /* 1512 */  { MAD_F(0x043ca1c9) /* 0.264802730 */, 16 },
  /* 1513 */  { MAD_F(0x043d96ab) /* 0.265036267 */, 16 },
  /* 1514 */  { MAD_F(0x043e8b9b) /* 0.265269857 */, 16 },
  /* 1515 */  { MAD_F(0x043f8098) /* 0.265503498 */, 16 },
  /* 1516 */  { MAD_F(0x044075a3) /* 0.265737190 */, 16 },
  /* 1517 */  { MAD_F(0x04416abc) /* 0.265970933 */, 16 },
  /* 1518 */  { MAD_F(0x04425fe3) /* 0.266204728 */, 16 },
  /* 1519 */  { MAD_F(0x04435518) /* 0.266438574 */, 16 },

  /* 1520 */  { MAD_F(0x04444a5a) /* 0.266672472 */, 16 },
  /* 1521 */  { MAD_F(0x04453fab) /* 0.266906421 */, 16 },
  /* 1522 */  { MAD_F(0x04463508) /* 0.267140421 */, 16 },
  /* 1523 */  { MAD_F(0x04472a74) /* 0.267374472 */, 16 },
  /* 1524 */  { MAD_F(0x04481fee) /* 0.267608575 */, 16 },
  /* 1525 */  { MAD_F(0x04491575) /* 0.267842729 */, 16 },
  /* 1526 */  { MAD_F(0x044a0b0a) /* 0.268076934 */, 16 },
  /* 1527 */  { MAD_F(0x044b00ac) /* 0.268311190 */, 16 },
  /* 1528 */  { MAD_F(0x044bf65d) /* 0.268545497 */, 16 },
  /* 1529 */  { MAD_F(0x044cec1b) /* 0.268779856 */, 16 },
  /* 1530 */  { MAD_F(0x044de1e7) /* 0.269014265 */, 16 },
  /* 1531 */  { MAD_F(0x044ed7c0) /* 0.269248726 */, 16 },
  /* 1532 */  { MAD_F(0x044fcda8) /* 0.269483238 */, 16 },
  /* 1533 */  { MAD_F(0x0450c39c) /* 0.269717800 */, 16 },
  /* 1534 */  { MAD_F(0x0451b99f) /* 0.269952414 */, 16 },
  /* 1535 */  { MAD_F(0x0452afaf) /* 0.270187079 */, 16 },

  /* 1536 */  { MAD_F(0x0453a5cd) /* 0.270421794 */, 16 },
  /* 1537 */  { MAD_F(0x04549bf9) /* 0.270656561 */, 16 },
  /* 1538 */  { MAD_F(0x04559232) /* 0.270891379 */, 16 },
  /* 1539 */  { MAD_F(0x04568879) /* 0.271126247 */, 16 },
  /* 1540 */  { MAD_F(0x04577ece) /* 0.271361166 */, 16 },
  /* 1541 */  { MAD_F(0x04587530) /* 0.271596136 */, 16 },
  /* 1542 */  { MAD_F(0x04596ba0) /* 0.271831157 */, 16 },
  /* 1543 */  { MAD_F(0x045a621e) /* 0.272066229 */, 16 },
  /* 1544 */  { MAD_F(0x045b58a9) /* 0.272301352 */, 16 },
  /* 1545 */  { MAD_F(0x045c4f42) /* 0.272536525 */, 16 },
  /* 1546 */  { MAD_F(0x045d45e9) /* 0.272771749 */, 16 },
  /* 1547 */  { MAD_F(0x045e3c9d) /* 0.273007024 */, 16 },
  /* 1548 */  { MAD_F(0x045f335e) /* 0.273242350 */, 16 },
  /* 1549 */  { MAD_F(0x04602a2e) /* 0.273477726 */, 16 },
  /* 1550 */  { MAD_F(0x0461210b) /* 0.273713153 */, 16 },
  /* 1551 */  { MAD_F(0x046217f5) /* 0.273948630 */, 16 },

  /* 1552 */  { MAD_F(0x04630eed) /* 0.274184158 */, 16 },
  /* 1553 */  { MAD_F(0x046405f3) /* 0.274419737 */, 16 },
  /* 1554 */  { MAD_F(0x0464fd06) /* 0.274655366 */, 16 },
  /* 1555 */  { MAD_F(0x0465f427) /* 0.274891046 */, 16 },
  /* 1556 */  { MAD_F(0x0466eb55) /* 0.275126776 */, 16 },
  /* 1557 */  { MAD_F(0x0467e291) /* 0.275362557 */, 16 },
  /* 1558 */  { MAD_F(0x0468d9db) /* 0.275598389 */, 16 },
  /* 1559 */  { MAD_F(0x0469d132) /* 0.275834270 */, 16 },
  /* 1560 */  { MAD_F(0x046ac896) /* 0.276070203 */, 16 },
  /* 1561 */  { MAD_F(0x046bc009) /* 0.276306185 */, 16 },
  /* 1562 */  { MAD_F(0x046cb788) /* 0.276542218 */, 16 },
  /* 1563 */  { MAD_F(0x046daf15) /* 0.276778302 */, 16 },
  /* 1564 */  { MAD_F(0x046ea6b0) /* 0.277014435 */, 16 },
  /* 1565 */  { MAD_F(0x046f9e58) /* 0.277250619 */, 16 },
  /* 1566 */  { MAD_F(0x0470960e) /* 0.277486854 */, 16 },
  /* 1567 */  { MAD_F(0x04718dd1) /* 0.277723139 */, 16 },

  /* 1568 */  { MAD_F(0x047285a2) /* 0.277959474 */, 16 },
  /* 1569 */  { MAD_F(0x04737d80) /* 0.278195859 */, 16 },
  /* 1570 */  { MAD_F(0x0474756c) /* 0.278432294 */, 16 },
  /* 1571 */  { MAD_F(0x04756d65) /* 0.278668780 */, 16 },
  /* 1572 */  { MAD_F(0x0476656b) /* 0.278905316 */, 16 },
  /* 1573 */  { MAD_F(0x04775d7f) /* 0.279141902 */, 16 },
  /* 1574 */  { MAD_F(0x047855a1) /* 0.279378538 */, 16 },
  /* 1575 */  { MAD_F(0x04794dd0) /* 0.279615224 */, 16 },
  /* 1576 */  { MAD_F(0x047a460c) /* 0.279851960 */, 16 },
  /* 1577 */  { MAD_F(0x047b3e56) /* 0.280088747 */, 16 },
  /* 1578 */  { MAD_F(0x047c36ae) /* 0.280325583 */, 16 },
  /* 1579 */  { MAD_F(0x047d2f12) /* 0.280562470 */, 16 },
  /* 1580 */  { MAD_F(0x047e2784) /* 0.280799406 */, 16 },
  /* 1581 */  { MAD_F(0x047f2004) /* 0.281036393 */, 16 },
  /* 1582 */  { MAD_F(0x04801891) /* 0.281273429 */, 16 },
  /* 1583 */  { MAD_F(0x0481112b) /* 0.281510516 */, 16 },

  /* 1584 */  { MAD_F(0x048209d3) /* 0.281747652 */, 16 },
  /* 1585 */  { MAD_F(0x04830288) /* 0.281984838 */, 16 },
  /* 1586 */  { MAD_F(0x0483fb4b) /* 0.282222075 */, 16 },
  /* 1587 */  { MAD_F(0x0484f41b) /* 0.282459361 */, 16 },
  /* 1588 */  { MAD_F(0x0485ecf8) /* 0.282696697 */, 16 },
  /* 1589 */  { MAD_F(0x0486e5e3) /* 0.282934082 */, 16 },
  /* 1590 */  { MAD_F(0x0487dedb) /* 0.283171518 */, 16 },
  /* 1591 */  { MAD_F(0x0488d7e1) /* 0.283409003 */, 16 },
  /* 1592 */  { MAD_F(0x0489d0f4) /* 0.283646538 */, 16 },
  /* 1593 */  { MAD_F(0x048aca14) /* 0.283884123 */, 16 },
  /* 1594 */  { MAD_F(0x048bc341) /* 0.284121757 */, 16 },
  /* 1595 */  { MAD_F(0x048cbc7c) /* 0.284359441 */, 16 },
  /* 1596 */  { MAD_F(0x048db5c4) /* 0.284597175 */, 16 },
  /* 1597 */  { MAD_F(0x048eaf1a) /* 0.284834959 */, 16 },
  /* 1598 */  { MAD_F(0x048fa87d) /* 0.285072792 */, 16 },
  /* 1599 */  { MAD_F(0x0490a1ed) /* 0.285310675 */, 16 },

  /* 1600 */  { MAD_F(0x04919b6a) /* 0.285548607 */, 16 },
  /* 1601 */  { MAD_F(0x049294f5) /* 0.285786589 */, 16 },
  /* 1602 */  { MAD_F(0x04938e8d) /* 0.286024621 */, 16 },
  /* 1603 */  { MAD_F(0x04948833) /* 0.286262702 */, 16 },
  /* 1604 */  { MAD_F(0x049581e5) /* 0.286500832 */, 16 },
  /* 1605 */  { MAD_F(0x04967ba5) /* 0.286739012 */, 16 },
  /* 1606 */  { MAD_F(0x04977573) /* 0.286977242 */, 16 },
  /* 1607 */  { MAD_F(0x04986f4d) /* 0.287215521 */, 16 },
  /* 1608 */  { MAD_F(0x04996935) /* 0.287453849 */, 16 },
  /* 1609 */  { MAD_F(0x049a632a) /* 0.287692227 */, 16 },
  /* 1610 */  { MAD_F(0x049b5d2c) /* 0.287930654 */, 16 },
  /* 1611 */  { MAD_F(0x049c573c) /* 0.288169131 */, 16 },
  /* 1612 */  { MAD_F(0x049d5159) /* 0.288407657 */, 16 },
  /* 1613 */  { MAD_F(0x049e4b83) /* 0.288646232 */, 16 },
  /* 1614 */  { MAD_F(0x049f45ba) /* 0.288884857 */, 16 },
  /* 1615 */  { MAD_F(0x04a03ffe) /* 0.289123530 */, 16 },

  /* 1616 */  { MAD_F(0x04a13a50) /* 0.289362253 */, 16 },
  /* 1617 */  { MAD_F(0x04a234af) /* 0.289601026 */, 16 },
  /* 1618 */  { MAD_F(0x04a32f1b) /* 0.289839847 */, 16 },
  /* 1619 */  { MAD_F(0x04a42995) /* 0.290078718 */, 16 },
  /* 1620 */  { MAD_F(0x04a5241b) /* 0.290317638 */, 16 },
  /* 1621 */  { MAD_F(0x04a61eaf) /* 0.290556607 */, 16 },
  /* 1622 */  { MAD_F(0x04a71950) /* 0.290795626 */, 16 },
  /* 1623 */  { MAD_F(0x04a813fe) /* 0.291034693 */, 16 },
  /* 1624 */  { MAD_F(0x04a90eba) /* 0.291273810 */, 16 },
  /* 1625 */  { MAD_F(0x04aa0982) /* 0.291512975 */, 16 },
  /* 1626 */  { MAD_F(0x04ab0458) /* 0.291752190 */, 16 },
  /* 1627 */  { MAD_F(0x04abff3b) /* 0.291991453 */, 16 },
  /* 1628 */  { MAD_F(0x04acfa2b) /* 0.292230766 */, 16 },
  /* 1629 */  { MAD_F(0x04adf528) /* 0.292470128 */, 16 },
  /* 1630 */  { MAD_F(0x04aef032) /* 0.292709539 */, 16 },
  /* 1631 */  { MAD_F(0x04afeb4a) /* 0.292948998 */, 16 },

  /* 1632 */  { MAD_F(0x04b0e66e) /* 0.293188507 */, 16 },
  /* 1633 */  { MAD_F(0x04b1e1a0) /* 0.293428065 */, 16 },
  /* 1634 */  { MAD_F(0x04b2dcdf) /* 0.293667671 */, 16 },
  /* 1635 */  { MAD_F(0x04b3d82b) /* 0.293907326 */, 16 },
  /* 1636 */  { MAD_F(0x04b4d384) /* 0.294147031 */, 16 },
  /* 1637 */  { MAD_F(0x04b5ceea) /* 0.294386784 */, 16 },
  /* 1638 */  { MAD_F(0x04b6ca5e) /* 0.294626585 */, 16 },
  /* 1639 */  { MAD_F(0x04b7c5de) /* 0.294866436 */, 16 },
  /* 1640 */  { MAD_F(0x04b8c16c) /* 0.295106336 */, 16 },
  /* 1641 */  { MAD_F(0x04b9bd06) /* 0.295346284 */, 16 },
  /* 1642 */  { MAD_F(0x04bab8ae) /* 0.295586281 */, 16 },
  /* 1643 */  { MAD_F(0x04bbb463) /* 0.295826327 */, 16 },
  /* 1644 */  { MAD_F(0x04bcb024) /* 0.296066421 */, 16 },
  /* 1645 */  { MAD_F(0x04bdabf3) /* 0.296306564 */, 16 },
  /* 1646 */  { MAD_F(0x04bea7cf) /* 0.296546756 */, 16 },
  /* 1647 */  { MAD_F(0x04bfa3b8) /* 0.296786996 */, 16 },

  /* 1648 */  { MAD_F(0x04c09faf) /* 0.297027285 */, 16 },
  /* 1649 */  { MAD_F(0x04c19bb2) /* 0.297267623 */, 16 },
  /* 1650 */  { MAD_F(0x04c297c2) /* 0.297508009 */, 16 },
  /* 1651 */  { MAD_F(0x04c393df) /* 0.297748444 */, 16 },
  /* 1652 */  { MAD_F(0x04c49009) /* 0.297988927 */, 16 },
  /* 1653 */  { MAD_F(0x04c58c41) /* 0.298229459 */, 16 },
  /* 1654 */  { MAD_F(0x04c68885) /* 0.298470039 */, 16 },
  /* 1655 */  { MAD_F(0x04c784d6) /* 0.298710668 */, 16 },
  /* 1656 */  { MAD_F(0x04c88135) /* 0.298951346 */, 16 },
  /* 1657 */  { MAD_F(0x04c97da0) /* 0.299192071 */, 16 },
  /* 1658 */  { MAD_F(0x04ca7a18) /* 0.299432846 */, 16 },
  /* 1659 */  { MAD_F(0x04cb769e) /* 0.299673668 */, 16 },
  /* 1660 */  { MAD_F(0x04cc7330) /* 0.299914539 */, 16 },
  /* 1661 */  { MAD_F(0x04cd6fcf) /* 0.300155459 */, 16 },
  /* 1662 */  { MAD_F(0x04ce6c7b) /* 0.300396426 */, 16 },
  /* 1663 */  { MAD_F(0x04cf6935) /* 0.300637443 */, 16 },

  /* 1664 */  { MAD_F(0x04d065fb) /* 0.300878507 */, 16 },
  /* 1665 */  { MAD_F(0x04d162ce) /* 0.301119620 */, 16 },
  /* 1666 */  { MAD_F(0x04d25fae) /* 0.301360781 */, 16 },
  /* 1667 */  { MAD_F(0x04d35c9b) /* 0.301601990 */, 16 },
  /* 1668 */  { MAD_F(0x04d45995) /* 0.301843247 */, 16 },
  /* 1669 */  { MAD_F(0x04d5569c) /* 0.302084553 */, 16 },
  /* 1670 */  { MAD_F(0x04d653b0) /* 0.302325907 */, 16 },
  /* 1671 */  { MAD_F(0x04d750d1) /* 0.302567309 */, 16 },
  /* 1672 */  { MAD_F(0x04d84dff) /* 0.302808759 */, 16 },
  /* 1673 */  { MAD_F(0x04d94b3a) /* 0.303050257 */, 16 },
  /* 1674 */  { MAD_F(0x04da4881) /* 0.303291804 */, 16 },
  /* 1675 */  { MAD_F(0x04db45d6) /* 0.303533399 */, 16 },
  /* 1676 */  { MAD_F(0x04dc4337) /* 0.303775041 */, 16 },
  /* 1677 */  { MAD_F(0x04dd40a6) /* 0.304016732 */, 16 },
  /* 1678 */  { MAD_F(0x04de3e21) /* 0.304258471 */, 16 },
  /* 1679 */  { MAD_F(0x04df3ba9) /* 0.304500257 */, 16 },

  /* 1680 */  { MAD_F(0x04e0393e) /* 0.304742092 */, 16 },
  /* 1681 */  { MAD_F(0x04e136e0) /* 0.304983975 */, 16 },
  /* 1682 */  { MAD_F(0x04e2348f) /* 0.305225906 */, 16 },
  /* 1683 */  { MAD_F(0x04e3324b) /* 0.305467885 */, 16 },
  /* 1684 */  { MAD_F(0x04e43013) /* 0.305709911 */, 16 },
  /* 1685 */  { MAD_F(0x04e52de9) /* 0.305951986 */, 16 },
  /* 1686 */  { MAD_F(0x04e62bcb) /* 0.306194108 */, 16 },
  /* 1687 */  { MAD_F(0x04e729ba) /* 0.306436279 */, 16 },
  /* 1688 */  { MAD_F(0x04e827b6) /* 0.306678497 */, 16 },
  /* 1689 */  { MAD_F(0x04e925bf) /* 0.306920763 */, 16 },
  /* 1690 */  { MAD_F(0x04ea23d4) /* 0.307163077 */, 16 },
  /* 1691 */  { MAD_F(0x04eb21f7) /* 0.307405438 */, 16 },
  /* 1692 */  { MAD_F(0x04ec2026) /* 0.307647848 */, 16 },
  /* 1693 */  { MAD_F(0x04ed1e62) /* 0.307890305 */, 16 },
  /* 1694 */  { MAD_F(0x04ee1cab) /* 0.308132810 */, 16 },
  /* 1695 */  { MAD_F(0x04ef1b01) /* 0.308375362 */, 16 },

  /* 1696 */  { MAD_F(0x04f01963) /* 0.308617963 */, 16 },
  /* 1697 */  { MAD_F(0x04f117d3) /* 0.308860611 */, 16 },
  /* 1698 */  { MAD_F(0x04f2164f) /* 0.309103306 */, 16 },
  /* 1699 */  { MAD_F(0x04f314d8) /* 0.309346050 */, 16 },
  /* 1700 */  { MAD_F(0x04f4136d) /* 0.309588841 */, 16 },
  /* 1701 */  { MAD_F(0x04f51210) /* 0.309831679 */, 16 },
  /* 1702 */  { MAD_F(0x04f610bf) /* 0.310074565 */, 16 },
  /* 1703 */  { MAD_F(0x04f70f7b) /* 0.310317499 */, 16 },
  /* 1704 */  { MAD_F(0x04f80e44) /* 0.310560480 */, 16 },
  /* 1705 */  { MAD_F(0x04f90d19) /* 0.310803509 */, 16 },
  /* 1706 */  { MAD_F(0x04fa0bfc) /* 0.311046586 */, 16 },
  /* 1707 */  { MAD_F(0x04fb0aeb) /* 0.311289710 */, 16 },
  /* 1708 */  { MAD_F(0x04fc09e7) /* 0.311532881 */, 16 },
  /* 1709 */  { MAD_F(0x04fd08ef) /* 0.311776100 */, 16 },
  /* 1710 */  { MAD_F(0x04fe0805) /* 0.312019366 */, 16 },
  /* 1711 */  { MAD_F(0x04ff0727) /* 0.312262680 */, 16 },

  /* 1712 */  { MAD_F(0x05000655) /* 0.312506041 */, 16 },
  /* 1713 */  { MAD_F(0x05010591) /* 0.312749449 */, 16 },
  /* 1714 */  { MAD_F(0x050204d9) /* 0.312992905 */, 16 },
  /* 1715 */  { MAD_F(0x0503042e) /* 0.313236408 */, 16 },
  /* 1716 */  { MAD_F(0x0504038f) /* 0.313479959 */, 16 },
  /* 1717 */  { MAD_F(0x050502fe) /* 0.313723556 */, 16 },
  /* 1718 */  { MAD_F(0x05060279) /* 0.313967202 */, 16 },
  /* 1719 */  { MAD_F(0x05070200) /* 0.314210894 */, 16 },
  /* 1720 */  { MAD_F(0x05080195) /* 0.314454634 */, 16 },
  /* 1721 */  { MAD_F(0x05090136) /* 0.314698420 */, 16 },
  /* 1722 */  { MAD_F(0x050a00e3) /* 0.314942255 */, 16 },
  /* 1723 */  { MAD_F(0x050b009e) /* 0.315186136 */, 16 },
  /* 1724 */  { MAD_F(0x050c0065) /* 0.315430064 */, 16 },
  /* 1725 */  { MAD_F(0x050d0039) /* 0.315674040 */, 16 },
  /* 1726 */  { MAD_F(0x050e0019) /* 0.315918063 */, 16 },
  /* 1727 */  { MAD_F(0x050f0006) /* 0.316162133 */, 16 },

  /* 1728 */  { MAD_F(0x05100000) /* 0.316406250 */, 16 },
  /* 1729 */  { MAD_F(0x05110006) /* 0.316650414 */, 16 },
  /* 1730 */  { MAD_F(0x05120019) /* 0.316894625 */, 16 },
  /* 1731 */  { MAD_F(0x05130039) /* 0.317138884 */, 16 },
  /* 1732 */  { MAD_F(0x05140065) /* 0.317383189 */, 16 },
  /* 1733 */  { MAD_F(0x0515009e) /* 0.317627541 */, 16 },
  /* 1734 */  { MAD_F(0x051600e3) /* 0.317871941 */, 16 },
  /* 1735 */  { MAD_F(0x05170135) /* 0.318116387 */, 16 },
  /* 1736 */  { MAD_F(0x05180194) /* 0.318360880 */, 16 },
  /* 1737 */  { MAD_F(0x051901ff) /* 0.318605421 */, 16 },
  /* 1738 */  { MAD_F(0x051a0277) /* 0.318850008 */, 16 },
  /* 1739 */  { MAD_F(0x051b02fc) /* 0.319094642 */, 16 },
  /* 1740 */  { MAD_F(0x051c038d) /* 0.319339323 */, 16 },
  /* 1741 */  { MAD_F(0x051d042a) /* 0.319584051 */, 16 },
  /* 1742 */  { MAD_F(0x051e04d4) /* 0.319828826 */, 16 },
  /* 1743 */  { MAD_F(0x051f058b) /* 0.320073647 */, 16 },

  /* 1744 */  { MAD_F(0x0520064f) /* 0.320318516 */, 16 },
  /* 1745 */  { MAD_F(0x0521071f) /* 0.320563431 */, 16 },
  /* 1746 */  { MAD_F(0x052207fb) /* 0.320808393 */, 16 },
  /* 1747 */  { MAD_F(0x052308e4) /* 0.321053402 */, 16 },
  /* 1748 */  { MAD_F(0x052409da) /* 0.321298457 */, 16 },
  /* 1749 */  { MAD_F(0x05250adc) /* 0.321543560 */, 16 },
  /* 1750 */  { MAD_F(0x05260bea) /* 0.321788709 */, 16 },
  /* 1751 */  { MAD_F(0x05270d06) /* 0.322033904 */, 16 },
  /* 1752 */  { MAD_F(0x05280e2d) /* 0.322279147 */, 16 },
  /* 1753 */  { MAD_F(0x05290f62) /* 0.322524436 */, 16 },
  /* 1754 */  { MAD_F(0x052a10a3) /* 0.322769771 */, 16 },
  /* 1755 */  { MAD_F(0x052b11f0) /* 0.323015154 */, 16 },
  /* 1756 */  { MAD_F(0x052c134a) /* 0.323260583 */, 16 },
  /* 1757 */  { MAD_F(0x052d14b0) /* 0.323506058 */, 16 },
  /* 1758 */  { MAD_F(0x052e1623) /* 0.323751580 */, 16 },
  /* 1759 */  { MAD_F(0x052f17a2) /* 0.323997149 */, 16 },

  /* 1760 */  { MAD_F(0x0530192e) /* 0.324242764 */, 16 },
  /* 1761 */  { MAD_F(0x05311ac6) /* 0.324488426 */, 16 },
  /* 1762 */  { MAD_F(0x05321c6b) /* 0.324734134 */, 16 },
  /* 1763 */  { MAD_F(0x05331e1c) /* 0.324979889 */, 16 },
  /* 1764 */  { MAD_F(0x05341fda) /* 0.325225690 */, 16 },
  /* 1765 */  { MAD_F(0x053521a4) /* 0.325471538 */, 16 },
  /* 1766 */  { MAD_F(0x0536237b) /* 0.325717432 */, 16 },
  /* 1767 */  { MAD_F(0x0537255e) /* 0.325963372 */, 16 },
  /* 1768 */  { MAD_F(0x0538274e) /* 0.326209359 */, 16 },
  /* 1769 */  { MAD_F(0x0539294a) /* 0.326455392 */, 16 },
  /* 1770 */  { MAD_F(0x053a2b52) /* 0.326701472 */, 16 },
  /* 1771 */  { MAD_F(0x053b2d67) /* 0.326947598 */, 16 },
  /* 1772 */  { MAD_F(0x053c2f89) /* 0.327193770 */, 16 },
  /* 1773 */  { MAD_F(0x053d31b6) /* 0.327439989 */, 16 },
  /* 1774 */  { MAD_F(0x053e33f1) /* 0.327686254 */, 16 },
  /* 1775 */  { MAD_F(0x053f3637) /* 0.327932565 */, 16 },

  /* 1776 */  { MAD_F(0x0540388a) /* 0.328178922 */, 16 },
  /* 1777 */  { MAD_F(0x05413aea) /* 0.328425326 */, 16 },
  /* 1778 */  { MAD_F(0x05423d56) /* 0.328671776 */, 16 },
  /* 1779 */  { MAD_F(0x05433fce) /* 0.328918272 */, 16 },
  /* 1780 */  { MAD_F(0x05444253) /* 0.329164814 */, 16 },
  /* 1781 */  { MAD_F(0x054544e4) /* 0.329411403 */, 16 },
  /* 1782 */  { MAD_F(0x05464781) /* 0.329658038 */, 16 },
  /* 1783 */  { MAD_F(0x05474a2b) /* 0.329904718 */, 16 },
  /* 1784 */  { MAD_F(0x05484ce2) /* 0.330151445 */, 16 },
  /* 1785 */  { MAD_F(0x05494fa4) /* 0.330398218 */, 16 },
  /* 1786 */  { MAD_F(0x054a5273) /* 0.330645037 */, 16 },
  /* 1787 */  { MAD_F(0x054b554e) /* 0.330891903 */, 16 },
  /* 1788 */  { MAD_F(0x054c5836) /* 0.331138814 */, 16 },
  /* 1789 */  { MAD_F(0x054d5b2a) /* 0.331385771 */, 16 },
  /* 1790 */  { MAD_F(0x054e5e2b) /* 0.331632774 */, 16 },
  /* 1791 */  { MAD_F(0x054f6138) /* 0.331879824 */, 16 },

  /* 1792 */  { MAD_F(0x05506451) /* 0.332126919 */, 16 },
  /* 1793 */  { MAD_F(0x05516776) /* 0.332374060 */, 16 },
  /* 1794 */  { MAD_F(0x05526aa8) /* 0.332621247 */, 16 },
  /* 1795 */  { MAD_F(0x05536de6) /* 0.332868480 */, 16 },
  /* 1796 */  { MAD_F(0x05547131) /* 0.333115759 */, 16 },
  /* 1797 */  { MAD_F(0x05557487) /* 0.333363084 */, 16 },
  /* 1798 */  { MAD_F(0x055677ea) /* 0.333610455 */, 16 },
  /* 1799 */  { MAD_F(0x05577b5a) /* 0.333857872 */, 16 },
  /* 1800 */  { MAD_F(0x05587ed5) /* 0.334105334 */, 16 },
  /* 1801 */  { MAD_F(0x0559825e) /* 0.334352843 */, 16 },
  /* 1802 */  { MAD_F(0x055a85f2) /* 0.334600397 */, 16 },
  /* 1803 */  { MAD_F(0x055b8992) /* 0.334847997 */, 16 },
  /* 1804 */  { MAD_F(0x055c8d3f) /* 0.335095642 */, 16 },
  /* 1805 */  { MAD_F(0x055d90f9) /* 0.335343334 */, 16 },
  /* 1806 */  { MAD_F(0x055e94be) /* 0.335591071 */, 16 },
  /* 1807 */  { MAD_F(0x055f9890) /* 0.335838854 */, 16 },

  /* 1808 */  { MAD_F(0x05609c6e) /* 0.336086683 */, 16 },
  /* 1809 */  { MAD_F(0x0561a058) /* 0.336334557 */, 16 },
  /* 1810 */  { MAD_F(0x0562a44f) /* 0.336582477 */, 16 },
  /* 1811 */  { MAD_F(0x0563a851) /* 0.336830443 */, 16 },
  /* 1812 */  { MAD_F(0x0564ac60) /* 0.337078454 */, 16 },
  /* 1813 */  { MAD_F(0x0565b07c) /* 0.337326511 */, 16 },
  /* 1814 */  { MAD_F(0x0566b4a3) /* 0.337574614 */, 16 },
  /* 1815 */  { MAD_F(0x0567b8d7) /* 0.337822762 */, 16 },
  /* 1816 */  { MAD_F(0x0568bd17) /* 0.338070956 */, 16 },
  /* 1817 */  { MAD_F(0x0569c163) /* 0.338319195 */, 16 },
  /* 1818 */  { MAD_F(0x056ac5bc) /* 0.338567480 */, 16 },
  /* 1819 */  { MAD_F(0x056bca20) /* 0.338815811 */, 16 },
  /* 1820 */  { MAD_F(0x056cce91) /* 0.339064186 */, 16 },
  /* 1821 */  { MAD_F(0x056dd30e) /* 0.339312608 */, 16 },
  /* 1822 */  { MAD_F(0x056ed798) /* 0.339561075 */, 16 },
  /* 1823 */  { MAD_F(0x056fdc2d) /* 0.339809587 */, 16 },

  /* 1824 */  { MAD_F(0x0570e0cf) /* 0.340058145 */, 16 },
  /* 1825 */  { MAD_F(0x0571e57d) /* 0.340306748 */, 16 },
  /* 1826 */  { MAD_F(0x0572ea37) /* 0.340555397 */, 16 },
  /* 1827 */  { MAD_F(0x0573eefd) /* 0.340804091 */, 16 },
  /* 1828 */  { MAD_F(0x0574f3d0) /* 0.341052830 */, 16 },
  /* 1829 */  { MAD_F(0x0575f8ae) /* 0.341301615 */, 16 },
  /* 1830 */  { MAD_F(0x0576fd99) /* 0.341550445 */, 16 },
  /* 1831 */  { MAD_F(0x05780290) /* 0.341799321 */, 16 },
  /* 1832 */  { MAD_F(0x05790793) /* 0.342048241 */, 16 },
  /* 1833 */  { MAD_F(0x057a0ca3) /* 0.342297207 */, 16 },
  /* 1834 */  { MAD_F(0x057b11be) /* 0.342546219 */, 16 },
  /* 1835 */  { MAD_F(0x057c16e6) /* 0.342795275 */, 16 },
  /* 1836 */  { MAD_F(0x057d1c1a) /* 0.343044377 */, 16 },
  /* 1837 */  { MAD_F(0x057e2159) /* 0.343293524 */, 16 },
  /* 1838 */  { MAD_F(0x057f26a6) /* 0.343542717 */, 16 },
  /* 1839 */  { MAD_F(0x05802bfe) /* 0.343791954 */, 16 },

  /* 1840 */  { MAD_F(0x05813162) /* 0.344041237 */, 16 },
  /* 1841 */  { MAD_F(0x058236d2) /* 0.344290564 */, 16 },
  /* 1842 */  { MAD_F(0x05833c4f) /* 0.344539937 */, 16 },
  /* 1843 */  { MAD_F(0x058441d8) /* 0.344789356 */, 16 },
  /* 1844 */  { MAD_F(0x0585476c) /* 0.345038819 */, 16 },
  /* 1845 */  { MAD_F(0x05864d0d) /* 0.345288327 */, 16 },
  /* 1846 */  { MAD_F(0x058752ba) /* 0.345537880 */, 16 },
  /* 1847 */  { MAD_F(0x05885873) /* 0.345787479 */, 16 },
  /* 1848 */  { MAD_F(0x05895e39) /* 0.346037122 */, 16 },
  /* 1849 */  { MAD_F(0x058a640a) /* 0.346286811 */, 16 },
  /* 1850 */  { MAD_F(0x058b69e7) /* 0.346536545 */, 16 },
  /* 1851 */  { MAD_F(0x058c6fd1) /* 0.346786323 */, 16 },
  /* 1852 */  { MAD_F(0x058d75c6) /* 0.347036147 */, 16 },
  /* 1853 */  { MAD_F(0x058e7bc8) /* 0.347286015 */, 16 },
  /* 1854 */  { MAD_F(0x058f81d5) /* 0.347535929 */, 16 },
  /* 1855 */  { MAD_F(0x059087ef) /* 0.347785887 */, 16 },

  /* 1856 */  { MAD_F(0x05918e15) /* 0.348035890 */, 16 },
  /* 1857 */  { MAD_F(0x05929447) /* 0.348285939 */, 16 },
  /* 1858 */  { MAD_F(0x05939a84) /* 0.348536032 */, 16 },
  /* 1859 */  { MAD_F(0x0594a0ce) /* 0.348786170 */, 16 },
  /* 1860 */  { MAD_F(0x0595a724) /* 0.349036353 */, 16 },
  /* 1861 */  { MAD_F(0x0596ad86) /* 0.349286580 */, 16 },
  /* 1862 */  { MAD_F(0x0597b3f4) /* 0.349536853 */, 16 },
  /* 1863 */  { MAD_F(0x0598ba6e) /* 0.349787170 */, 16 },
  /* 1864 */  { MAD_F(0x0599c0f4) /* 0.350037532 */, 16 },
  /* 1865 */  { MAD_F(0x059ac786) /* 0.350287939 */, 16 },
  /* 1866 */  { MAD_F(0x059bce25) /* 0.350538391 */, 16 },
  /* 1867 */  { MAD_F(0x059cd4cf) /* 0.350788887 */, 16 },
  /* 1868 */  { MAD_F(0x059ddb85) /* 0.351039428 */, 16 },
  /* 1869 */  { MAD_F(0x059ee247) /* 0.351290014 */, 16 },
  /* 1870 */  { MAD_F(0x059fe915) /* 0.351540645 */, 16 },
  /* 1871 */  { MAD_F(0x05a0efef) /* 0.351791320 */, 16 },

  /* 1872 */  { MAD_F(0x05a1f6d5) /* 0.352042040 */, 16 },
  /* 1873 */  { MAD_F(0x05a2fdc7) /* 0.352292804 */, 16 },
  /* 1874 */  { MAD_F(0x05a404c5) /* 0.352543613 */, 16 },
  /* 1875 */  { MAD_F(0x05a50bcf) /* 0.352794467 */, 16 },
  /* 1876 */  { MAD_F(0x05a612e5) /* 0.353045365 */, 16 },
  /* 1877 */  { MAD_F(0x05a71a07) /* 0.353296308 */, 16 },
  /* 1878 */  { MAD_F(0x05a82135) /* 0.353547296 */, 16 },
  /* 1879 */  { MAD_F(0x05a9286f) /* 0.353798328 */, 16 },
  /* 1880 */  { MAD_F(0x05aa2fb5) /* 0.354049405 */, 16 },
  /* 1881 */  { MAD_F(0x05ab3707) /* 0.354300526 */, 16 },
  /* 1882 */  { MAD_F(0x05ac3e65) /* 0.354551691 */, 16 },
  /* 1883 */  { MAD_F(0x05ad45ce) /* 0.354802901 */, 16 },
  /* 1884 */  { MAD_F(0x05ae4d44) /* 0.355054156 */, 16 },
  /* 1885 */  { MAD_F(0x05af54c6) /* 0.355305455 */, 16 },
  /* 1886 */  { MAD_F(0x05b05c53) /* 0.355556799 */, 16 },
  /* 1887 */  { MAD_F(0x05b163ed) /* 0.355808187 */, 16 },

  /* 1888 */  { MAD_F(0x05b26b92) /* 0.356059619 */, 16 },
  /* 1889 */  { MAD_F(0x05b37343) /* 0.356311096 */, 16 },
  /* 1890 */  { MAD_F(0x05b47b00) /* 0.356562617 */, 16 },
  /* 1891 */  { MAD_F(0x05b582c9) /* 0.356814182 */, 16 },
  /* 1892 */  { MAD_F(0x05b68a9e) /* 0.357065792 */, 16 },
  /* 1893 */  { MAD_F(0x05b7927f) /* 0.357317446 */, 16 },
  /* 1894 */  { MAD_F(0x05b89a6c) /* 0.357569145 */, 16 },
  /* 1895 */  { MAD_F(0x05b9a265) /* 0.357820887 */, 16 },
  /* 1896 */  { MAD_F(0x05baaa69) /* 0.358072674 */, 16 },
  /* 1897 */  { MAD_F(0x05bbb27a) /* 0.358324506 */, 16 },
  /* 1898 */  { MAD_F(0x05bcba96) /* 0.358576381 */, 16 },
  /* 1899 */  { MAD_F(0x05bdc2be) /* 0.358828301 */, 16 },
  /* 1900 */  { MAD_F(0x05becaf2) /* 0.359080265 */, 16 },
  /* 1901 */  { MAD_F(0x05bfd332) /* 0.359332273 */, 16 },
  /* 1902 */  { MAD_F(0x05c0db7e) /* 0.359584326 */, 16 },
  /* 1903 */  { MAD_F(0x05c1e3d6) /* 0.359836423 */, 16 },

  /* 1904 */  { MAD_F(0x05c2ec39) /* 0.360088563 */, 16 },
  /* 1905 */  { MAD_F(0x05c3f4a9) /* 0.360340748 */, 16 },
  /* 1906 */  { MAD_F(0x05c4fd24) /* 0.360592977 */, 16 },
  /* 1907 */  { MAD_F(0x05c605ab) /* 0.360845251 */, 16 },
  /* 1908 */  { MAD_F(0x05c70e3e) /* 0.361097568 */, 16 },
  /* 1909 */  { MAD_F(0x05c816dd) /* 0.361349929 */, 16 },
  /* 1910 */  { MAD_F(0x05c91f87) /* 0.361602335 */, 16 },
  /* 1911 */  { MAD_F(0x05ca283e) /* 0.361854784 */, 16 },
  /* 1912 */  { MAD_F(0x05cb3100) /* 0.362107278 */, 16 },
  /* 1913 */  { MAD_F(0x05cc39ce) /* 0.362359815 */, 16 },
  /* 1914 */  { MAD_F(0x05cd42a8) /* 0.362612397 */, 16 },
  /* 1915 */  { MAD_F(0x05ce4b8d) /* 0.362865022 */, 16 },
  /* 1916 */  { MAD_F(0x05cf547f) /* 0.363117692 */, 16 },
  /* 1917 */  { MAD_F(0x05d05d7c) /* 0.363370405 */, 16 },
  /* 1918 */  { MAD_F(0x05d16685) /* 0.363623163 */, 16 },
  /* 1919 */  { MAD_F(0x05d26f9a) /* 0.363875964 */, 16 },

  /* 1920 */  { MAD_F(0x05d378bb) /* 0.364128809 */, 16 },
  /* 1921 */  { MAD_F(0x05d481e7) /* 0.364381698 */, 16 },
  /* 1922 */  { MAD_F(0x05d58b1f) /* 0.364634632 */, 16 },
  /* 1923 */  { MAD_F(0x05d69463) /* 0.364887608 */, 16 },
  /* 1924 */  { MAD_F(0x05d79db3) /* 0.365140629 */, 16 },
  /* 1925 */  { MAD_F(0x05d8a70f) /* 0.365393694 */, 16 },
  /* 1926 */  { MAD_F(0x05d9b076) /* 0.365646802 */, 16 },
  /* 1927 */  { MAD_F(0x05dab9e9) /* 0.365899955 */, 16 },
  /* 1928 */  { MAD_F(0x05dbc368) /* 0.366153151 */, 16 },
  /* 1929 */  { MAD_F(0x05dcccf2) /* 0.366406390 */, 16 },
  /* 1930 */  { MAD_F(0x05ddd689) /* 0.366659674 */, 16 },
  /* 1931 */  { MAD_F(0x05dee02b) /* 0.366913001 */, 16 },
  /* 1932 */  { MAD_F(0x05dfe9d8) /* 0.367166372 */, 16 },
  /* 1933 */  { MAD_F(0x05e0f392) /* 0.367419787 */, 16 },
  /* 1934 */  { MAD_F(0x05e1fd57) /* 0.367673246 */, 16 },
  /* 1935 */  { MAD_F(0x05e30728) /* 0.367926748 */, 16 },

  /* 1936 */  { MAD_F(0x05e41105) /* 0.368180294 */, 16 },
  /* 1937 */  { MAD_F(0x05e51aed) /* 0.368433883 */, 16 },
  /* 1938 */  { MAD_F(0x05e624e1) /* 0.368687517 */, 16 },
  /* 1939 */  { MAD_F(0x05e72ee1) /* 0.368941193 */, 16 },
  /* 1940 */  { MAD_F(0x05e838ed) /* 0.369194914 */, 16 },
  /* 1941 */  { MAD_F(0x05e94304) /* 0.369448678 */, 16 },
  /* 1942 */  { MAD_F(0x05ea4d27) /* 0.369702485 */, 16 },
  /* 1943 */  { MAD_F(0x05eb5756) /* 0.369956336 */, 16 },
  /* 1944 */  { MAD_F(0x05ec6190) /* 0.370210231 */, 16 },
  /* 1945 */  { MAD_F(0x05ed6bd6) /* 0.370464169 */, 16 },
  /* 1946 */  { MAD_F(0x05ee7628) /* 0.370718151 */, 16 },
  /* 1947 */  { MAD_F(0x05ef8085) /* 0.370972177 */, 16 },
  /* 1948 */  { MAD_F(0x05f08aee) /* 0.371226245 */, 16 },
  /* 1949 */  { MAD_F(0x05f19563) /* 0.371480358 */, 16 },
  /* 1950 */  { MAD_F(0x05f29fe3) /* 0.371734513 */, 16 },
  /* 1951 */  { MAD_F(0x05f3aa6f) /* 0.371988712 */, 16 },

  /* 1952 */  { MAD_F(0x05f4b507) /* 0.372242955 */, 16 },
  /* 1953 */  { MAD_F(0x05f5bfab) /* 0.372497241 */, 16 },
  /* 1954 */  { MAD_F(0x05f6ca5a) /* 0.372751570 */, 16 },
  /* 1955 */  { MAD_F(0x05f7d514) /* 0.373005943 */, 16 },
  /* 1956 */  { MAD_F(0x05f8dfdb) /* 0.373260359 */, 16 },
  /* 1957 */  { MAD_F(0x05f9eaad) /* 0.373514819 */, 16 },
  /* 1958 */  { MAD_F(0x05faf58a) /* 0.373769322 */, 16 },
  /* 1959 */  { MAD_F(0x05fc0073) /* 0.374023868 */, 16 },
  /* 1960 */  { MAD_F(0x05fd0b68) /* 0.374278458 */, 16 },
  /* 1961 */  { MAD_F(0x05fe1669) /* 0.374533091 */, 16 },
  /* 1962 */  { MAD_F(0x05ff2175) /* 0.374787767 */, 16 },
  /* 1963 */  { MAD_F(0x06002c8d) /* 0.375042486 */, 16 },
  /* 1964 */  { MAD_F(0x060137b0) /* 0.375297249 */, 16 },
  /* 1965 */  { MAD_F(0x060242df) /* 0.375552055 */, 16 },
  /* 1966 */  { MAD_F(0x06034e19) /* 0.375806904 */, 16 },
  /* 1967 */  { MAD_F(0x0604595f) /* 0.376061796 */, 16 },

  /* 1968 */  { MAD_F(0x060564b1) /* 0.376316732 */, 16 },
  /* 1969 */  { MAD_F(0x0606700f) /* 0.376571710 */, 16 },
  /* 1970 */  { MAD_F(0x06077b77) /* 0.376826732 */, 16 },
  /* 1971 */  { MAD_F(0x060886ec) /* 0.377081797 */, 16 },
  /* 1972 */  { MAD_F(0x0609926c) /* 0.377336905 */, 16 },
  /* 1973 */  { MAD_F(0x060a9df8) /* 0.377592057 */, 16 },
  /* 1974 */  { MAD_F(0x060ba98f) /* 0.377847251 */, 16 },
  /* 1975 */  { MAD_F(0x060cb532) /* 0.378102489 */, 16 },
  /* 1976 */  { MAD_F(0x060dc0e0) /* 0.378357769 */, 16 },
  /* 1977 */  { MAD_F(0x060ecc9a) /* 0.378613093 */, 16 },
  /* 1978 */  { MAD_F(0x060fd860) /* 0.378868460 */, 16 },
  /* 1979 */  { MAD_F(0x0610e431) /* 0.379123870 */, 16 },
  /* 1980 */  { MAD_F(0x0611f00d) /* 0.379379322 */, 16 },
  /* 1981 */  { MAD_F(0x0612fbf5) /* 0.379634818 */, 16 },
  /* 1982 */  { MAD_F(0x061407e9) /* 0.379890357 */, 16 },
  /* 1983 */  { MAD_F(0x061513e8) /* 0.380145939 */, 16 },

  /* 1984 */  { MAD_F(0x06161ff3) /* 0.380401563 */, 16 },
  /* 1985 */  { MAD_F(0x06172c09) /* 0.380657231 */, 16 },
  /* 1986 */  { MAD_F(0x0618382b) /* 0.380912942 */, 16 },
  /* 1987 */  { MAD_F(0x06194458) /* 0.381168695 */, 16 },
  /* 1988 */  { MAD_F(0x061a5091) /* 0.381424492 */, 16 },
  /* 1989 */  { MAD_F(0x061b5cd5) /* 0.381680331 */, 16 },
  /* 1990 */  { MAD_F(0x061c6925) /* 0.381936213 */, 16 },
  /* 1991 */  { MAD_F(0x061d7581) /* 0.382192138 */, 16 },
  /* 1992 */  { MAD_F(0x061e81e8) /* 0.382448106 */, 16 },
  /* 1993 */  { MAD_F(0x061f8e5a) /* 0.382704117 */, 16 },
  /* 1994 */  { MAD_F(0x06209ad8) /* 0.382960171 */, 16 },
  /* 1995 */  { MAD_F(0x0621a761) /* 0.383216267 */, 16 },
  /* 1996 */  { MAD_F(0x0622b3f6) /* 0.383472406 */, 16 },
  /* 1997 */  { MAD_F(0x0623c096) /* 0.383728588 */, 16 },
  /* 1998 */  { MAD_F(0x0624cd42) /* 0.383984813 */, 16 },
  /* 1999 */  { MAD_F(0x0625d9f9) /* 0.384241080 */, 16 },

  /* 2000 */  { MAD_F(0x0626e6bc) /* 0.384497391 */, 16 },
  /* 2001 */  { MAD_F(0x0627f38a) /* 0.384753744 */, 16 },
  /* 2002 */  { MAD_F(0x06290064) /* 0.385010139 */, 16 },
  /* 2003 */  { MAD_F(0x062a0d49) /* 0.385266578 */, 16 },
  /* 2004 */  { MAD_F(0x062b1a3a) /* 0.385523059 */, 16 },
  /* 2005 */  { MAD_F(0x062c2736) /* 0.385779582 */, 16 },
  /* 2006 */  { MAD_F(0x062d343d) /* 0.386036149 */, 16 },
  /* 2007 */  { MAD_F(0x062e4150) /* 0.386292758 */, 16 },
  /* 2008 */  { MAD_F(0x062f4e6f) /* 0.386549409 */, 16 },
  /* 2009 */  { MAD_F(0x06305b99) /* 0.386806104 */, 16 },
  /* 2010 */  { MAD_F(0x063168ce) /* 0.387062840 */, 16 },
  /* 2011 */  { MAD_F(0x0632760f) /* 0.387319620 */, 16 },
  /* 2012 */  { MAD_F(0x0633835b) /* 0.387576442 */, 16 },
  /* 2013 */  { MAD_F(0x063490b2) /* 0.387833306 */, 16 },
  /* 2014 */  { MAD_F(0x06359e15) /* 0.388090213 */, 16 },
  /* 2015 */  { MAD_F(0x0636ab83) /* 0.388347163 */, 16 },

  /* 2016 */  { MAD_F(0x0637b8fd) /* 0.388604155 */, 16 },
  /* 2017 */  { MAD_F(0x0638c682) /* 0.388861190 */, 16 },
  /* 2018 */  { MAD_F(0x0639d413) /* 0.389118267 */, 16 },
  /* 2019 */  { MAD_F(0x063ae1af) /* 0.389375386 */, 16 },
  /* 2020 */  { MAD_F(0x063bef56) /* 0.389632548 */, 16 },
  /* 2021 */  { MAD_F(0x063cfd09) /* 0.389889752 */, 16 },
  /* 2022 */  { MAD_F(0x063e0ac7) /* 0.390146999 */, 16 },
  /* 2023 */  { MAD_F(0x063f1891) /* 0.390404289 */, 16 },
  /* 2024 */  { MAD_F(0x06402666) /* 0.390661620 */, 16 },
  /* 2025 */  { MAD_F(0x06413446) /* 0.390918994 */, 16 },
  /* 2026 */  { MAD_F(0x06424232) /* 0.391176411 */, 16 },
  /* 2027 */  { MAD_F(0x06435029) /* 0.391433869 */, 16 },
  /* 2028 */  { MAD_F(0x06445e2b) /* 0.391691371 */, 16 },
  /* 2029 */  { MAD_F(0x06456c39) /* 0.391948914 */, 16 },
  /* 2030 */  { MAD_F(0x06467a52) /* 0.392206500 */, 16 },
  /* 2031 */  { MAD_F(0x06478877) /* 0.392464128 */, 16 },

  /* 2032 */  { MAD_F(0x064896a7) /* 0.392721798 */, 16 },
  /* 2033 */  { MAD_F(0x0649a4e2) /* 0.392979511 */, 16 },
  /* 2034 */  { MAD_F(0x064ab328) /* 0.393237266 */, 16 },
  /* 2035 */  { MAD_F(0x064bc17a) /* 0.393495063 */, 16 },
  /* 2036 */  { MAD_F(0x064ccfd8) /* 0.393752902 */, 16 },
  /* 2037 */  { MAD_F(0x064dde40) /* 0.394010784 */, 16 },
  /* 2038 */  { MAD_F(0x064eecb4) /* 0.394268707 */, 16 },
  /* 2039 */  { MAD_F(0x064ffb33) /* 0.394526673 */, 16 },
  /* 2040 */  { MAD_F(0x065109be) /* 0.394784681 */, 16 },
  /* 2041 */  { MAD_F(0x06521854) /* 0.395042732 */, 16 },
  /* 2042 */  { MAD_F(0x065326f5) /* 0.395300824 */, 16 },
  /* 2043 */  { MAD_F(0x065435a1) /* 0.395558959 */, 16 },
  /* 2044 */  { MAD_F(0x06554459) /* 0.395817135 */, 16 },
  /* 2045 */  { MAD_F(0x0656531c) /* 0.396075354 */, 16 },
  /* 2046 */  { MAD_F(0x065761ea) /* 0.396333615 */, 16 },
  /* 2047 */  { MAD_F(0x065870c4) /* 0.396591918 */, 16 },

  /* 2048 */  { MAD_F(0x06597fa9) /* 0.396850263 */, 16 },
  /* 2049 */  { MAD_F(0x065a8e99) /* 0.397108650 */, 16 },
  /* 2050 */  { MAD_F(0x065b9d95) /* 0.397367079 */, 16 },
  /* 2051 */  { MAD_F(0x065cac9c) /* 0.397625550 */, 16 },
  /* 2052 */  { MAD_F(0x065dbbae) /* 0.397884063 */, 16 },
  /* 2053 */  { MAD_F(0x065ecacb) /* 0.398142619 */, 16 },
  /* 2054 */  { MAD_F(0x065fd9f4) /* 0.398401216 */, 16 },
  /* 2055 */  { MAD_F(0x0660e928) /* 0.398659855 */, 16 },
  /* 2056 */  { MAD_F(0x0661f867) /* 0.398918536 */, 16 },
  /* 2057 */  { MAD_F(0x066307b1) /* 0.399177259 */, 16 },
  /* 2058 */  { MAD_F(0x06641707) /* 0.399436024 */, 16 },
  /* 2059 */  { MAD_F(0x06652668) /* 0.399694831 */, 16 },
  /* 2060 */  { MAD_F(0x066635d4) /* 0.399953679 */, 16 },
  /* 2061 */  { MAD_F(0x0667454c) /* 0.400212570 */, 16 },
  /* 2062 */  { MAD_F(0x066854ce) /* 0.400471503 */, 16 },
  /* 2063 */  { MAD_F(0x0669645c) /* 0.400730477 */, 16 },

  /* 2064 */  { MAD_F(0x066a73f5) /* 0.400989493 */, 16 },
  /* 2065 */  { MAD_F(0x066b839a) /* 0.401248551 */, 16 },
  /* 2066 */  { MAD_F(0x066c9349) /* 0.401507651 */, 16 },
  /* 2067 */  { MAD_F(0x066da304) /* 0.401766793 */, 16 },
  /* 2068 */  { MAD_F(0x066eb2ca) /* 0.402025976 */, 16 },
  /* 2069 */  { MAD_F(0x066fc29b) /* 0.402285202 */, 16 },
  /* 2070 */  { MAD_F(0x0670d278) /* 0.402544469 */, 16 },
  /* 2071 */  { MAD_F(0x0671e25f) /* 0.402803777 */, 16 },
  /* 2072 */  { MAD_F(0x0672f252) /* 0.403063128 */, 16 },
  /* 2073 */  { MAD_F(0x06740250) /* 0.403322520 */, 16 },
  /* 2074 */  { MAD_F(0x0675125a) /* 0.403581954 */, 16 },
  /* 2075 */  { MAD_F(0x0676226e) /* 0.403841430 */, 16 },
  /* 2076 */  { MAD_F(0x0677328e) /* 0.404100947 */, 16 },
  /* 2077 */  { MAD_F(0x067842b9) /* 0.404360506 */, 16 },
  /* 2078 */  { MAD_F(0x067952ef) /* 0.404620107 */, 16 },
  /* 2079 */  { MAD_F(0x067a6330) /* 0.404879749 */, 16 },

  /* 2080 */  { MAD_F(0x067b737c) /* 0.405139433 */, 16 },
  /* 2081 */  { MAD_F(0x067c83d4) /* 0.405399159 */, 16 },
  /* 2082 */  { MAD_F(0x067d9436) /* 0.405658926 */, 16 },
  /* 2083 */  { MAD_F(0x067ea4a4) /* 0.405918735 */, 16 },
  /* 2084 */  { MAD_F(0x067fb51d) /* 0.406178585 */, 16 },
  /* 2085 */  { MAD_F(0x0680c5a2) /* 0.406438477 */, 16 },
  /* 2086 */  { MAD_F(0x0681d631) /* 0.406698410 */, 16 },
  /* 2087 */  { MAD_F(0x0682e6cb) /* 0.406958385 */, 16 },
  /* 2088 */  { MAD_F(0x0683f771) /* 0.407218402 */, 16 },
  /* 2089 */  { MAD_F(0x06850822) /* 0.407478460 */, 16 },
  /* 2090 */  { MAD_F(0x068618de) /* 0.407738559 */, 16 },
  /* 2091 */  { MAD_F(0x068729a5) /* 0.407998700 */, 16 },
  /* 2092 */  { MAD_F(0x06883a77) /* 0.408258883 */, 16 },
  /* 2093 */  { MAD_F(0x06894b55) /* 0.408519107 */, 16 },
  /* 2094 */  { MAD_F(0x068a5c3d) /* 0.408779372 */, 16 },
  /* 2095 */  { MAD_F(0x068b6d31) /* 0.409039679 */, 16 },

  /* 2096 */  { MAD_F(0x068c7e2f) /* 0.409300027 */, 16 },
  /* 2097 */  { MAD_F(0x068d8f39) /* 0.409560417 */, 16 },
  /* 2098 */  { MAD_F(0x068ea04e) /* 0.409820848 */, 16 },
  /* 2099 */  { MAD_F(0x068fb16e) /* 0.410081321 */, 16 },
  /* 2100 */  { MAD_F(0x0690c299) /* 0.410341834 */, 16 },
  /* 2101 */  { MAD_F(0x0691d3cf) /* 0.410602390 */, 16 },
  /* 2102 */  { MAD_F(0x0692e511) /* 0.410862986 */, 16 },
  /* 2103 */  { MAD_F(0x0693f65d) /* 0.411123624 */, 16 },
  /* 2104 */  { MAD_F(0x069507b5) /* 0.411384303 */, 16 },
  /* 2105 */  { MAD_F(0x06961917) /* 0.411645024 */, 16 },
  /* 2106 */  { MAD_F(0x06972a85) /* 0.411905785 */, 16 },
  /* 2107 */  { MAD_F(0x06983bfe) /* 0.412166588 */, 16 },
  /* 2108 */  { MAD_F(0x06994d82) /* 0.412427433 */, 16 },
  /* 2109 */  { MAD_F(0x069a5f11) /* 0.412688318 */, 16 },
  /* 2110 */  { MAD_F(0x069b70ab) /* 0.412949245 */, 16 },
  /* 2111 */  { MAD_F(0x069c8250) /* 0.413210213 */, 16 },

  /* 2112 */  { MAD_F(0x069d9400) /* 0.413471222 */, 16 },
  /* 2113 */  { MAD_F(0x069ea5bb) /* 0.413732273 */, 16 },
  /* 2114 */  { MAD_F(0x069fb781) /* 0.413993364 */, 16 },
  /* 2115 */  { MAD_F(0x06a0c953) /* 0.414254497 */, 16 },
  /* 2116 */  { MAD_F(0x06a1db2f) /* 0.414515671 */, 16 },
  /* 2117 */  { MAD_F(0x06a2ed16) /* 0.414776886 */, 16 },
  /* 2118 */  { MAD_F(0x06a3ff09) /* 0.415038142 */, 16 },
  /* 2119 */  { MAD_F(0x06a51106) /* 0.415299440 */, 16 },
  /* 2120 */  { MAD_F(0x06a6230f) /* 0.415560778 */, 16 },
  /* 2121 */  { MAD_F(0x06a73522) /* 0.415822157 */, 16 },
  /* 2122 */  { MAD_F(0x06a84741) /* 0.416083578 */, 16 },
  /* 2123 */  { MAD_F(0x06a9596a) /* 0.416345040 */, 16 },
  /* 2124 */  { MAD_F(0x06aa6b9f) /* 0.416606542 */, 16 },
  /* 2125 */  { MAD_F(0x06ab7ddf) /* 0.416868086 */, 16 },
  /* 2126 */  { MAD_F(0x06ac9029) /* 0.417129671 */, 16 },
  /* 2127 */  { MAD_F(0x06ada27f) /* 0.417391297 */, 16 },

  /* 2128 */  { MAD_F(0x06aeb4e0) /* 0.417652964 */, 16 },
  /* 2129 */  { MAD_F(0x06afc74b) /* 0.417914672 */, 16 },
  /* 2130 */  { MAD_F(0x06b0d9c2) /* 0.418176420 */, 16 },
  /* 2131 */  { MAD_F(0x06b1ec43) /* 0.418438210 */, 16 },
  /* 2132 */  { MAD_F(0x06b2fed0) /* 0.418700041 */, 16 },
  /* 2133 */  { MAD_F(0x06b41168) /* 0.418961912 */, 16 },
  /* 2134 */  { MAD_F(0x06b5240a) /* 0.419223825 */, 16 },
  /* 2135 */  { MAD_F(0x06b636b8) /* 0.419485778 */, 16 },
  /* 2136 */  { MAD_F(0x06b74971) /* 0.419747773 */, 16 },
  /* 2137 */  { MAD_F(0x06b85c34) /* 0.420009808 */, 16 },
  /* 2138 */  { MAD_F(0x06b96f03) /* 0.420271884 */, 16 },
  /* 2139 */  { MAD_F(0x06ba81dc) /* 0.420534001 */, 16 },
  /* 2140 */  { MAD_F(0x06bb94c1) /* 0.420796159 */, 16 },
  /* 2141 */  { MAD_F(0x06bca7b0) /* 0.421058358 */, 16 },
  /* 2142 */  { MAD_F(0x06bdbaaa) /* 0.421320597 */, 16 },
  /* 2143 */  { MAD_F(0x06becdb0) /* 0.421582878 */, 16 },

  /* 2144 */  { MAD_F(0x06bfe0c0) /* 0.421845199 */, 16 },
  /* 2145 */  { MAD_F(0x06c0f3db) /* 0.422107561 */, 16 },
  /* 2146 */  { MAD_F(0x06c20702) /* 0.422369964 */, 16 },
  /* 2147 */  { MAD_F(0x06c31a33) /* 0.422632407 */, 16 },
  /* 2148 */  { MAD_F(0x06c42d6f) /* 0.422894891 */, 16 },
  /* 2149 */  { MAD_F(0x06c540b6) /* 0.423157416 */, 16 },
  /* 2150 */  { MAD_F(0x06c65408) /* 0.423419982 */, 16 },
  /* 2151 */  { MAD_F(0x06c76765) /* 0.423682588 */, 16 },
  /* 2152 */  { MAD_F(0x06c87acc) /* 0.423945235 */, 16 },
  /* 2153 */  { MAD_F(0x06c98e3f) /* 0.424207923 */, 16 },
  /* 2154 */  { MAD_F(0x06caa1bd) /* 0.424470652 */, 16 },
  /* 2155 */  { MAD_F(0x06cbb545) /* 0.424733421 */, 16 },
  /* 2156 */  { MAD_F(0x06ccc8d9) /* 0.424996230 */, 16 },
  /* 2157 */  { MAD_F(0x06cddc77) /* 0.425259081 */, 16 },
  /* 2158 */  { MAD_F(0x06cef020) /* 0.425521972 */, 16 },
  /* 2159 */  { MAD_F(0x06d003d4) /* 0.425784903 */, 16 },

  /* 2160 */  { MAD_F(0x06d11794) /* 0.426047876 */, 16 },
  /* 2161 */  { MAD_F(0x06d22b5e) /* 0.426310889 */, 16 },
  /* 2162 */  { MAD_F(0x06d33f32) /* 0.426573942 */, 16 },
  /* 2163 */  { MAD_F(0x06d45312) /* 0.426837036 */, 16 },
  /* 2164 */  { MAD_F(0x06d566fd) /* 0.427100170 */, 16 },
  /* 2165 */  { MAD_F(0x06d67af2) /* 0.427363345 */, 16 },
  /* 2166 */  { MAD_F(0x06d78ef3) /* 0.427626561 */, 16 },
  /* 2167 */  { MAD_F(0x06d8a2fe) /* 0.427889817 */, 16 },
  /* 2168 */  { MAD_F(0x06d9b714) /* 0.428153114 */, 16 },
  /* 2169 */  { MAD_F(0x06dacb35) /* 0.428416451 */, 16 },
  /* 2170 */  { MAD_F(0x06dbdf61) /* 0.428679828 */, 16 },
  /* 2171 */  { MAD_F(0x06dcf398) /* 0.428943246 */, 16 },
  /* 2172 */  { MAD_F(0x06de07d9) /* 0.429206704 */, 16 },
  /* 2173 */  { MAD_F(0x06df1c26) /* 0.429470203 */, 16 },
  /* 2174 */  { MAD_F(0x06e0307d) /* 0.429733743 */, 16 },
  /* 2175 */  { MAD_F(0x06e144df) /* 0.429997322 */, 16 },

  /* 2176 */  { MAD_F(0x06e2594c) /* 0.430260942 */, 16 },
  /* 2177 */  { MAD_F(0x06e36dc4) /* 0.430524603 */, 16 },
  /* 2178 */  { MAD_F(0x06e48246) /* 0.430788304 */, 16 },
  /* 2179 */  { MAD_F(0x06e596d4) /* 0.431052045 */, 16 },
  /* 2180 */  { MAD_F(0x06e6ab6c) /* 0.431315826 */, 16 },
  /* 2181 */  { MAD_F(0x06e7c00f) /* 0.431579648 */, 16 },
  /* 2182 */  { MAD_F(0x06e8d4bd) /* 0.431843511 */, 16 },
  /* 2183 */  { MAD_F(0x06e9e976) /* 0.432107413 */, 16 },
  /* 2184 */  { MAD_F(0x06eafe3a) /* 0.432371356 */, 16 },
  /* 2185 */  { MAD_F(0x06ec1308) /* 0.432635339 */, 16 },
  /* 2186 */  { MAD_F(0x06ed27e2) /* 0.432899362 */, 16 },
  /* 2187 */  { MAD_F(0x06ee3cc6) /* 0.433163426 */, 16 },
  /* 2188 */  { MAD_F(0x06ef51b4) /* 0.433427530 */, 16 },
  /* 2189 */  { MAD_F(0x06f066ae) /* 0.433691674 */, 16 },
  /* 2190 */  { MAD_F(0x06f17bb3) /* 0.433955859 */, 16 },
  /* 2191 */  { MAD_F(0x06f290c2) /* 0.434220083 */, 16 },

  /* 2192 */  { MAD_F(0x06f3a5dc) /* 0.434484348 */, 16 },
  /* 2193 */  { MAD_F(0x06f4bb01) /* 0.434748653 */, 16 },
  /* 2194 */  { MAD_F(0x06f5d030) /* 0.435012998 */, 16 },
  /* 2195 */  { MAD_F(0x06f6e56b) /* 0.435277383 */, 16 },
  /* 2196 */  { MAD_F(0x06f7fab0) /* 0.435541809 */, 16 },
  /* 2197 */  { MAD_F(0x06f91000) /* 0.435806274 */, 16 },
  /* 2198 */  { MAD_F(0x06fa255a) /* 0.436070780 */, 16 },
  /* 2199 */  { MAD_F(0x06fb3ac0) /* 0.436335326 */, 16 },
  /* 2200 */  { MAD_F(0x06fc5030) /* 0.436599912 */, 16 },
  /* 2201 */  { MAD_F(0x06fd65ab) /* 0.436864538 */, 16 },
  /* 2202 */  { MAD_F(0x06fe7b31) /* 0.437129204 */, 16 },
  /* 2203 */  { MAD_F(0x06ff90c2) /* 0.437393910 */, 16 },
  /* 2204 */  { MAD_F(0x0700a65d) /* 0.437658657 */, 16 },
  /* 2205 */  { MAD_F(0x0701bc03) /* 0.437923443 */, 16 },
  /* 2206 */  { MAD_F(0x0702d1b4) /* 0.438188269 */, 16 },
  /* 2207 */  { MAD_F(0x0703e76f) /* 0.438453136 */, 16 },

  /* 2208 */  { MAD_F(0x0704fd35) /* 0.438718042 */, 16 },
  /* 2209 */  { MAD_F(0x07061306) /* 0.438982988 */, 16 },
  /* 2210 */  { MAD_F(0x070728e2) /* 0.439247975 */, 16 },
  /* 2211 */  { MAD_F(0x07083ec9) /* 0.439513001 */, 16 },
  /* 2212 */  { MAD_F(0x070954ba) /* 0.439778067 */, 16 },
  /* 2213 */  { MAD_F(0x070a6ab6) /* 0.440043173 */, 16 },
  /* 2214 */  { MAD_F(0x070b80bc) /* 0.440308320 */, 16 },
  /* 2215 */  { MAD_F(0x070c96ce) /* 0.440573506 */, 16 },
  /* 2216 */  { MAD_F(0x070dacea) /* 0.440838732 */, 16 },
  /* 2217 */  { MAD_F(0x070ec310) /* 0.441103997 */, 16 },
  /* 2218 */  { MAD_F(0x070fd942) /* 0.441369303 */, 16 },
  /* 2219 */  { MAD_F(0x0710ef7e) /* 0.441634649 */, 16 },
  /* 2220 */  { MAD_F(0x071205c5) /* 0.441900034 */, 16 },
  /* 2221 */  { MAD_F(0x07131c17) /* 0.442165460 */, 16 },
  /* 2222 */  { MAD_F(0x07143273) /* 0.442430925 */, 16 },
  /* 2223 */  { MAD_F(0x071548da) /* 0.442696430 */, 16 },

  /* 2224 */  { MAD_F(0x07165f4b) /* 0.442961975 */, 16 },
  /* 2225 */  { MAD_F(0x071775c8) /* 0.443227559 */, 16 },
  /* 2226 */  { MAD_F(0x07188c4f) /* 0.443493184 */, 16 },
  /* 2227 */  { MAD_F(0x0719a2e0) /* 0.443758848 */, 16 },
  /* 2228 */  { MAD_F(0x071ab97d) /* 0.444024552 */, 16 },
  /* 2229 */  { MAD_F(0x071bd024) /* 0.444290296 */, 16 },
  /* 2230 */  { MAD_F(0x071ce6d6) /* 0.444556079 */, 16 },
  /* 2231 */  { MAD_F(0x071dfd92) /* 0.444821902 */, 16 },
  /* 2232 */  { MAD_F(0x071f1459) /* 0.445087765 */, 16 },
  /* 2233 */  { MAD_F(0x07202b2b) /* 0.445353668 */, 16 },
  /* 2234 */  { MAD_F(0x07214207) /* 0.445619610 */, 16 },
  /* 2235 */  { MAD_F(0x072258ee) /* 0.445885592 */, 16 },
  /* 2236 */  { MAD_F(0x07236fe0) /* 0.446151614 */, 16 },
  /* 2237 */  { MAD_F(0x072486dc) /* 0.446417675 */, 16 },
  /* 2238 */  { MAD_F(0x07259de3) /* 0.446683776 */, 16 },
  /* 2239 */  { MAD_F(0x0726b4f4) /* 0.446949917 */, 16 },

  /* 2240 */  { MAD_F(0x0727cc11) /* 0.447216097 */, 16 },
  /* 2241 */  { MAD_F(0x0728e338) /* 0.447482317 */, 16 },
  /* 2242 */  { MAD_F(0x0729fa69) /* 0.447748576 */, 16 },
  /* 2243 */  { MAD_F(0x072b11a5) /* 0.448014875 */, 16 },
  /* 2244 */  { MAD_F(0x072c28ec) /* 0.448281214 */, 16 },
  /* 2245 */  { MAD_F(0x072d403d) /* 0.448547592 */, 16 },
  /* 2246 */  { MAD_F(0x072e5799) /* 0.448814010 */, 16 },
  /* 2247 */  { MAD_F(0x072f6f00) /* 0.449080467 */, 16 },
  /* 2248 */  { MAD_F(0x07308671) /* 0.449346964 */, 16 },
  /* 2249 */  { MAD_F(0x07319ded) /* 0.449613501 */, 16 },
  /* 2250 */  { MAD_F(0x0732b573) /* 0.449880076 */, 16 },
  /* 2251 */  { MAD_F(0x0733cd04) /* 0.450146692 */, 16 },
  /* 2252 */  { MAD_F(0x0734e4a0) /* 0.450413347 */, 16 },
  /* 2253 */  { MAD_F(0x0735fc46) /* 0.450680041 */, 16 },
  /* 2254 */  { MAD_F(0x073713f7) /* 0.450946775 */, 16 },
  /* 2255 */  { MAD_F(0x07382bb2) /* 0.451213548 */, 16 },

  /* 2256 */  { MAD_F(0x07394378) /* 0.451480360 */, 16 },
  /* 2257 */  { MAD_F(0x073a5b49) /* 0.451747213 */, 16 },
  /* 2258 */  { MAD_F(0x073b7324) /* 0.452014104 */, 16 },
  /* 2259 */  { MAD_F(0x073c8b0a) /* 0.452281035 */, 16 },
  /* 2260 */  { MAD_F(0x073da2fa) /* 0.452548005 */, 16 },
  /* 2261 */  { MAD_F(0x073ebaf5) /* 0.452815015 */, 16 },
  /* 2262 */  { MAD_F(0x073fd2fa) /* 0.453082064 */, 16 },
  /* 2263 */  { MAD_F(0x0740eb0a) /* 0.453349152 */, 16 },
  /* 2264 */  { MAD_F(0x07420325) /* 0.453616280 */, 16 },
  /* 2265 */  { MAD_F(0x07431b4a) /* 0.453883447 */, 16 },
  /* 2266 */  { MAD_F(0x0744337a) /* 0.454150653 */, 16 },
  /* 2267 */  { MAD_F(0x07454bb4) /* 0.454417899 */, 16 },
  /* 2268 */  { MAD_F(0x074663f8) /* 0.454685184 */, 16 },
  /* 2269 */  { MAD_F(0x07477c48) /* 0.454952508 */, 16 },
  /* 2270 */  { MAD_F(0x074894a2) /* 0.455219872 */, 16 },
  /* 2271 */  { MAD_F(0x0749ad06) /* 0.455487275 */, 16 },

  /* 2272 */  { MAD_F(0x074ac575) /* 0.455754717 */, 16 },
  /* 2273 */  { MAD_F(0x074bddee) /* 0.456022198 */, 16 },
  /* 2274 */  { MAD_F(0x074cf672) /* 0.456289719 */, 16 },
  /* 2275 */  { MAD_F(0x074e0f01) /* 0.456557278 */, 16 },
  /* 2276 */  { MAD_F(0x074f279a) /* 0.456824877 */, 16 },
  /* 2277 */  { MAD_F(0x0750403e) /* 0.457092516 */, 16 },
  /* 2278 */  { MAD_F(0x075158ec) /* 0.457360193 */, 16 },
  /* 2279 */  { MAD_F(0x075271a4) /* 0.457627909 */, 16 },
  /* 2280 */  { MAD_F(0x07538a67) /* 0.457895665 */, 16 },
  /* 2281 */  { MAD_F(0x0754a335) /* 0.458163460 */, 16 },
  /* 2282 */  { MAD_F(0x0755bc0d) /* 0.458431294 */, 16 },
  /* 2283 */  { MAD_F(0x0756d4f0) /* 0.458699167 */, 16 },
  /* 2284 */  { MAD_F(0x0757eddd) /* 0.458967079 */, 16 },
  /* 2285 */  { MAD_F(0x075906d5) /* 0.459235030 */, 16 },
  /* 2286 */  { MAD_F(0x075a1fd7) /* 0.459503021 */, 16 },
  /* 2287 */  { MAD_F(0x075b38e3) /* 0.459771050 */, 16 },

  /* 2288 */  { MAD_F(0x075c51fa) /* 0.460039119 */, 16 },
  /* 2289 */  { MAD_F(0x075d6b1c) /* 0.460307226 */, 16 },
  /* 2290 */  { MAD_F(0x075e8448) /* 0.460575373 */, 16 },
  /* 2291 */  { MAD_F(0x075f9d7f) /* 0.460843559 */, 16 },
  /* 2292 */  { MAD_F(0x0760b6c0) /* 0.461111783 */, 16 },
  /* 2293 */  { MAD_F(0x0761d00b) /* 0.461380047 */, 16 },
  /* 2294 */  { MAD_F(0x0762e961) /* 0.461648350 */, 16 },
  /* 2295 */  { MAD_F(0x076402c1) /* 0.461916691 */, 16 },
  /* 2296 */  { MAD_F(0x07651c2c) /* 0.462185072 */, 16 },
  /* 2297 */  { MAD_F(0x076635a2) /* 0.462453492 */, 16 },
  /* 2298 */  { MAD_F(0x07674f22) /* 0.462721950 */, 16 },
  /* 2299 */  { MAD_F(0x076868ac) /* 0.462990448 */, 16 },
  /* 2300 */  { MAD_F(0x07698240) /* 0.463258984 */, 16 },
  /* 2301 */  { MAD_F(0x076a9be0) /* 0.463527560 */, 16 },
  /* 2302 */  { MAD_F(0x076bb589) /* 0.463796174 */, 16 },
  /* 2303 */  { MAD_F(0x076ccf3d) /* 0.464064827 */, 16 },

  /* 2304 */  { MAD_F(0x076de8fc) /* 0.464333519 */, 16 },
  /* 2305 */  { MAD_F(0x076f02c5) /* 0.464602250 */, 16 },
  /* 2306 */  { MAD_F(0x07701c98) /* 0.464871020 */, 16 },
  /* 2307 */  { MAD_F(0x07713676) /* 0.465139829 */, 16 },
  /* 2308 */  { MAD_F(0x0772505e) /* 0.465408676 */, 16 },
  /* 2309 */  { MAD_F(0x07736a51) /* 0.465677563 */, 16 },
  /* 2310 */  { MAD_F(0x0774844e) /* 0.465946488 */, 16 },
  /* 2311 */  { MAD_F(0x07759e55) /* 0.466215452 */, 16 },
  /* 2312 */  { MAD_F(0x0776b867) /* 0.466484455 */, 16 },
  /* 2313 */  { MAD_F(0x0777d283) /* 0.466753496 */, 16 },
  /* 2314 */  { MAD_F(0x0778ecaa) /* 0.467022577 */, 16 },
  /* 2315 */  { MAD_F(0x077a06db) /* 0.467291696 */, 16 },
  /* 2316 */  { MAD_F(0x077b2117) /* 0.467560854 */, 16 },
  /* 2317 */  { MAD_F(0x077c3b5d) /* 0.467830050 */, 16 },
  /* 2318 */  { MAD_F(0x077d55ad) /* 0.468099285 */, 16 },
  /* 2319 */  { MAD_F(0x077e7008) /* 0.468368560 */, 16 },

  /* 2320 */  { MAD_F(0x077f8a6d) /* 0.468637872 */, 16 },
  /* 2321 */  { MAD_F(0x0780a4dc) /* 0.468907224 */, 16 },
  /* 2322 */  { MAD_F(0x0781bf56) /* 0.469176614 */, 16 },
  /* 2323 */  { MAD_F(0x0782d9da) /* 0.469446043 */, 16 },
  /* 2324 */  { MAD_F(0x0783f469) /* 0.469715510 */, 16 },
  /* 2325 */  { MAD_F(0x07850f02) /* 0.469985016 */, 16 },
  /* 2326 */  { MAD_F(0x078629a5) /* 0.470254561 */, 16 },
  /* 2327 */  { MAD_F(0x07874453) /* 0.470524145 */, 16 },
  /* 2328 */  { MAD_F(0x07885f0b) /* 0.470793767 */, 16 },
  /* 2329 */  { MAD_F(0x078979ce) /* 0.471063427 */, 16 },
  /* 2330 */  { MAD_F(0x078a949a) /* 0.471333126 */, 16 },
  /* 2331 */  { MAD_F(0x078baf72) /* 0.471602864 */, 16 },
  /* 2332 */  { MAD_F(0x078cca53) /* 0.471872641 */, 16 },
  /* 2333 */  { MAD_F(0x078de53f) /* 0.472142456 */, 16 },
  /* 2334 */  { MAD_F(0x078f0035) /* 0.472412309 */, 16 },
  /* 2335 */  { MAD_F(0x07901b36) /* 0.472682201 */, 16 },

  /* 2336 */  { MAD_F(0x07913641) /* 0.472952132 */, 16 },
  /* 2337 */  { MAD_F(0x07925156) /* 0.473222101 */, 16 },
  /* 2338 */  { MAD_F(0x07936c76) /* 0.473492108 */, 16 },
  /* 2339 */  { MAD_F(0x079487a0) /* 0.473762155 */, 16 },
  /* 2340 */  { MAD_F(0x0795a2d4) /* 0.474032239 */, 16 },
  /* 2341 */  { MAD_F(0x0796be13) /* 0.474302362 */, 16 },
  /* 2342 */  { MAD_F(0x0797d95c) /* 0.474572524 */, 16 },
  /* 2343 */  { MAD_F(0x0798f4af) /* 0.474842724 */, 16 },
  /* 2344 */  { MAD_F(0x079a100c) /* 0.475112962 */, 16 },
  /* 2345 */  { MAD_F(0x079b2b74) /* 0.475383239 */, 16 },
  /* 2346 */  { MAD_F(0x079c46e7) /* 0.475653554 */, 16 },
  /* 2347 */  { MAD_F(0x079d6263) /* 0.475923908 */, 16 },
  /* 2348 */  { MAD_F(0x079e7dea) /* 0.476194300 */, 16 },
  /* 2349 */  { MAD_F(0x079f997b) /* 0.476464731 */, 16 },
  /* 2350 */  { MAD_F(0x07a0b516) /* 0.476735200 */, 16 },
  /* 2351 */  { MAD_F(0x07a1d0bc) /* 0.477005707 */, 16 },

  /* 2352 */  { MAD_F(0x07a2ec6c) /* 0.477276252 */, 16 },
  /* 2353 */  { MAD_F(0x07a40827) /* 0.477546836 */, 16 },
  /* 2354 */  { MAD_F(0x07a523eb) /* 0.477817459 */, 16 },
  /* 2355 */  { MAD_F(0x07a63fba) /* 0.478088119 */, 16 },
  /* 2356 */  { MAD_F(0x07a75b93) /* 0.478358818 */, 16 },
  /* 2357 */  { MAD_F(0x07a87777) /* 0.478629555 */, 16 },
  /* 2358 */  { MAD_F(0x07a99364) /* 0.478900331 */, 16 },
  /* 2359 */  { MAD_F(0x07aaaf5c) /* 0.479171145 */, 16 },
  /* 2360 */  { MAD_F(0x07abcb5f) /* 0.479441997 */, 16 },
  /* 2361 */  { MAD_F(0x07ace76b) /* 0.479712887 */, 16 },
  /* 2362 */  { MAD_F(0x07ae0382) /* 0.479983816 */, 16 },
  /* 2363 */  { MAD_F(0x07af1fa3) /* 0.480254782 */, 16 },
  /* 2364 */  { MAD_F(0x07b03bcf) /* 0.480525787 */, 16 },
  /* 2365 */  { MAD_F(0x07b15804) /* 0.480796831 */, 16 },
  /* 2366 */  { MAD_F(0x07b27444) /* 0.481067912 */, 16 },
  /* 2367 */  { MAD_F(0x07b3908e) /* 0.481339032 */, 16 },

  /* 2368 */  { MAD_F(0x07b4ace3) /* 0.481610189 */, 16 },
  /* 2369 */  { MAD_F(0x07b5c941) /* 0.481881385 */, 16 },
  /* 2370 */  { MAD_F(0x07b6e5aa) /* 0.482152620 */, 16 },
  /* 2371 */  { MAD_F(0x07b8021d) /* 0.482423892 */, 16 },
  /* 2372 */  { MAD_F(0x07b91e9b) /* 0.482695202 */, 16 },
  /* 2373 */  { MAD_F(0x07ba3b22) /* 0.482966551 */, 16 },
  /* 2374 */  { MAD_F(0x07bb57b4) /* 0.483237938 */, 16 },
  /* 2375 */  { MAD_F(0x07bc7450) /* 0.483509362 */, 16 },
  /* 2376 */  { MAD_F(0x07bd90f6) /* 0.483780825 */, 16 },
  /* 2377 */  { MAD_F(0x07beada7) /* 0.484052326 */, 16 },
  /* 2378 */  { MAD_F(0x07bfca61) /* 0.484323865 */, 16 },
  /* 2379 */  { MAD_F(0x07c0e726) /* 0.484595443 */, 16 },
  /* 2380 */  { MAD_F(0x07c203f5) /* 0.484867058 */, 16 },
  /* 2381 */  { MAD_F(0x07c320cf) /* 0.485138711 */, 16 },
  /* 2382 */  { MAD_F(0x07c43db2) /* 0.485410402 */, 16 },
  /* 2383 */  { MAD_F(0x07c55aa0) /* 0.485682131 */, 16 },

  /* 2384 */  { MAD_F(0x07c67798) /* 0.485953899 */, 16 },
  /* 2385 */  { MAD_F(0x07c7949a) /* 0.486225704 */, 16 },
  /* 2386 */  { MAD_F(0x07c8b1a7) /* 0.486497547 */, 16 },
  /* 2387 */  { MAD_F(0x07c9cebd) /* 0.486769429 */, 16 },
  /* 2388 */  { MAD_F(0x07caebde) /* 0.487041348 */, 16 },
  /* 2389 */  { MAD_F(0x07cc0909) /* 0.487313305 */, 16 },
  /* 2390 */  { MAD_F(0x07cd263e) /* 0.487585300 */, 16 },
  /* 2391 */  { MAD_F(0x07ce437d) /* 0.487857333 */, 16 },
  /* 2392 */  { MAD_F(0x07cf60c7) /* 0.488129404 */, 16 },
  /* 2393 */  { MAD_F(0x07d07e1b) /* 0.488401513 */, 16 },
  /* 2394 */  { MAD_F(0x07d19b79) /* 0.488673660 */, 16 },
  /* 2395 */  { MAD_F(0x07d2b8e1) /* 0.488945845 */, 16 },
  /* 2396 */  { MAD_F(0x07d3d653) /* 0.489218067 */, 16 },
  /* 2397 */  { MAD_F(0x07d4f3cf) /* 0.489490328 */, 16 },
  /* 2398 */  { MAD_F(0x07d61156) /* 0.489762626 */, 16 },
  /* 2399 */  { MAD_F(0x07d72ee6) /* 0.490034962 */, 16 },

  /* 2400 */  { MAD_F(0x07d84c81) /* 0.490307336 */, 16 },
  /* 2401 */  { MAD_F(0x07d96a26) /* 0.490579748 */, 16 },
  /* 2402 */  { MAD_F(0x07da87d5) /* 0.490852198 */, 16 },
  /* 2403 */  { MAD_F(0x07dba58f) /* 0.491124686 */, 16 },
  /* 2404 */  { MAD_F(0x07dcc352) /* 0.491397211 */, 16 },
  /* 2405 */  { MAD_F(0x07dde120) /* 0.491669774 */, 16 },
  /* 2406 */  { MAD_F(0x07defef7) /* 0.491942375 */, 16 },
  /* 2407 */  { MAD_F(0x07e01cd9) /* 0.492215014 */, 16 },
  /* 2408 */  { MAD_F(0x07e13ac5) /* 0.492487690 */, 16 },
  /* 2409 */  { MAD_F(0x07e258bc) /* 0.492760404 */, 16 },
  /* 2410 */  { MAD_F(0x07e376bc) /* 0.493033156 */, 16 },
  /* 2411 */  { MAD_F(0x07e494c6) /* 0.493305946 */, 16 },
  /* 2412 */  { MAD_F(0x07e5b2db) /* 0.493578773 */, 16 },
  /* 2413 */  { MAD_F(0x07e6d0f9) /* 0.493851638 */, 16 },
  /* 2414 */  { MAD_F(0x07e7ef22) /* 0.494124541 */, 16 },
  /* 2415 */  { MAD_F(0x07e90d55) /* 0.494397481 */, 16 },

  /* 2416 */  { MAD_F(0x07ea2b92) /* 0.494670459 */, 16 },
  /* 2417 */  { MAD_F(0x07eb49d9) /* 0.494943475 */, 16 },
  /* 2418 */  { MAD_F(0x07ec682a) /* 0.495216529 */, 16 },
  /* 2419 */  { MAD_F(0x07ed8686) /* 0.495489620 */, 16 },
  /* 2420 */  { MAD_F(0x07eea4eb) /* 0.495762748 */, 16 },
  /* 2421 */  { MAD_F(0x07efc35b) /* 0.496035915 */, 16 },
  /* 2422 */  { MAD_F(0x07f0e1d4) /* 0.496309119 */, 16 },
  /* 2423 */  { MAD_F(0x07f20058) /* 0.496582360 */, 16 },
  /* 2424 */  { MAD_F(0x07f31ee6) /* 0.496855639 */, 16 },
  /* 2425 */  { MAD_F(0x07f43d7e) /* 0.497128956 */, 16 },
  /* 2426 */  { MAD_F(0x07f55c20) /* 0.497402310 */, 16 },
  /* 2427 */  { MAD_F(0x07f67acc) /* 0.497675702 */, 16 },
  /* 2428 */  { MAD_F(0x07f79982) /* 0.497949132 */, 16 },
  /* 2429 */  { MAD_F(0x07f8b842) /* 0.498222598 */, 16 },
  /* 2430 */  { MAD_F(0x07f9d70c) /* 0.498496103 */, 16 },
  /* 2431 */  { MAD_F(0x07faf5e1) /* 0.498769645 */, 16 },

  /* 2432 */  { MAD_F(0x07fc14bf) /* 0.499043224 */, 16 },
  /* 2433 */  { MAD_F(0x07fd33a8) /* 0.499316841 */, 16 },
  /* 2434 */  { MAD_F(0x07fe529a) /* 0.499590496 */, 16 },
  /* 2435 */  { MAD_F(0x07ff7197) /* 0.499864188 */, 16 },
  /* 2436 */  { MAD_F(0x0400484f) /* 0.250068959 */, 17 },
  /* 2437 */  { MAD_F(0x0400d7d7) /* 0.250205842 */, 17 },
  /* 2438 */  { MAD_F(0x04016764) /* 0.250342744 */, 17 },
  /* 2439 */  { MAD_F(0x0401f6f7) /* 0.250479665 */, 17 },
  /* 2440 */  { MAD_F(0x0402868e) /* 0.250616605 */, 17 },
  /* 2441 */  { MAD_F(0x0403162b) /* 0.250753563 */, 17 },
  /* 2442 */  { MAD_F(0x0403a5cc) /* 0.250890540 */, 17 },
  /* 2443 */  { MAD_F(0x04043573) /* 0.251027536 */, 17 },
  /* 2444 */  { MAD_F(0x0404c51e) /* 0.251164550 */, 17 },
  /* 2445 */  { MAD_F(0x040554cf) /* 0.251301583 */, 17 },
  /* 2446 */  { MAD_F(0x0405e484) /* 0.251438635 */, 17 },
  /* 2447 */  { MAD_F(0x0406743f) /* 0.251575706 */, 17 },

  /* 2448 */  { MAD_F(0x040703ff) /* 0.251712795 */, 17 },
  /* 2449 */  { MAD_F(0x040793c3) /* 0.251849903 */, 17 },
  /* 2450 */  { MAD_F(0x0408238d) /* 0.251987029 */, 17 },
  /* 2451 */  { MAD_F(0x0408b35b) /* 0.252124174 */, 17 },
  /* 2452 */  { MAD_F(0x0409432f) /* 0.252261338 */, 17 },
  /* 2453 */  { MAD_F(0x0409d308) /* 0.252398520 */, 17 },
  /* 2454 */  { MAD_F(0x040a62e5) /* 0.252535721 */, 17 },
  /* 2455 */  { MAD_F(0x040af2c8) /* 0.252672941 */, 17 },
  /* 2456 */  { MAD_F(0x040b82b0) /* 0.252810180 */, 17 },
  /* 2457 */  { MAD_F(0x040c129c) /* 0.252947436 */, 17 },
  /* 2458 */  { MAD_F(0x040ca28e) /* 0.253084712 */, 17 },
  /* 2459 */  { MAD_F(0x040d3284) /* 0.253222006 */, 17 },
  /* 2460 */  { MAD_F(0x040dc280) /* 0.253359319 */, 17 },
  /* 2461 */  { MAD_F(0x040e5281) /* 0.253496651 */, 17 },
  /* 2462 */  { MAD_F(0x040ee286) /* 0.253634001 */, 17 },
  /* 2463 */  { MAD_F(0x040f7291) /* 0.253771369 */, 17 },

  /* 2464 */  { MAD_F(0x041002a1) /* 0.253908756 */, 17 },
  /* 2465 */  { MAD_F(0x041092b5) /* 0.254046162 */, 17 },
  /* 2466 */  { MAD_F(0x041122cf) /* 0.254183587 */, 17 },
  /* 2467 */  { MAD_F(0x0411b2ed) /* 0.254321030 */, 17 },
  /* 2468 */  { MAD_F(0x04124311) /* 0.254458491 */, 17 },
  /* 2469 */  { MAD_F(0x0412d339) /* 0.254595971 */, 17 },
  /* 2470 */  { MAD_F(0x04136367) /* 0.254733470 */, 17 },
  /* 2471 */  { MAD_F(0x0413f399) /* 0.254870987 */, 17 },
  /* 2472 */  { MAD_F(0x041483d1) /* 0.255008523 */, 17 },
  /* 2473 */  { MAD_F(0x0415140d) /* 0.255146077 */, 17 },
  /* 2474 */  { MAD_F(0x0415a44f) /* 0.255283650 */, 17 },
  /* 2475 */  { MAD_F(0x04163495) /* 0.255421241 */, 17 },
  /* 2476 */  { MAD_F(0x0416c4e1) /* 0.255558851 */, 17 },
  /* 2477 */  { MAD_F(0x04175531) /* 0.255696480 */, 17 },
  /* 2478 */  { MAD_F(0x0417e586) /* 0.255834127 */, 17 },
  /* 2479 */  { MAD_F(0x041875e1) /* 0.255971792 */, 17 },

  /* 2480 */  { MAD_F(0x04190640) /* 0.256109476 */, 17 },
  /* 2481 */  { MAD_F(0x041996a4) /* 0.256247179 */, 17 },
  /* 2482 */  { MAD_F(0x041a270d) /* 0.256384900 */, 17 },
  /* 2483 */  { MAD_F(0x041ab77b) /* 0.256522639 */, 17 },
  /* 2484 */  { MAD_F(0x041b47ef) /* 0.256660397 */, 17 },
  /* 2485 */  { MAD_F(0x041bd867) /* 0.256798174 */, 17 },
  /* 2486 */  { MAD_F(0x041c68e4) /* 0.256935969 */, 17 },
  /* 2487 */  { MAD_F(0x041cf966) /* 0.257073782 */, 17 },
  /* 2488 */  { MAD_F(0x041d89ed) /* 0.257211614 */, 17 },
  /* 2489 */  { MAD_F(0x041e1a79) /* 0.257349465 */, 17 },
  /* 2490 */  { MAD_F(0x041eab0a) /* 0.257487334 */, 17 },
  /* 2491 */  { MAD_F(0x041f3b9f) /* 0.257625221 */, 17 },
  /* 2492 */  { MAD_F(0x041fcc3a) /* 0.257763127 */, 17 },
  /* 2493 */  { MAD_F(0x04205cda) /* 0.257901051 */, 17 },
  /* 2494 */  { MAD_F(0x0420ed7f) /* 0.258038994 */, 17 },
  /* 2495 */  { MAD_F(0x04217e28) /* 0.258176955 */, 17 },

  /* 2496 */  { MAD_F(0x04220ed7) /* 0.258314934 */, 17 },
  /* 2497 */  { MAD_F(0x04229f8a) /* 0.258452932 */, 17 },
  /* 2498 */  { MAD_F(0x04233043) /* 0.258590948 */, 17 },
  /* 2499 */  { MAD_F(0x0423c100) /* 0.258728983 */, 17 },
  /* 2500 */  { MAD_F(0x042451c3) /* 0.258867036 */, 17 },
  /* 2501 */  { MAD_F(0x0424e28a) /* 0.259005108 */, 17 },
  /* 2502 */  { MAD_F(0x04257356) /* 0.259143198 */, 17 },
  /* 2503 */  { MAD_F(0x04260428) /* 0.259281307 */, 17 },
  /* 2504 */  { MAD_F(0x042694fe) /* 0.259419433 */, 17 },
  /* 2505 */  { MAD_F(0x042725d9) /* 0.259557579 */, 17 },
  /* 2506 */  { MAD_F(0x0427b6b9) /* 0.259695742 */, 17 },
  /* 2507 */  { MAD_F(0x0428479e) /* 0.259833924 */, 17 },
  /* 2508 */  { MAD_F(0x0428d888) /* 0.259972124 */, 17 },
  /* 2509 */  { MAD_F(0x04296976) /* 0.260110343 */, 17 },
  /* 2510 */  { MAD_F(0x0429fa6a) /* 0.260248580 */, 17 },
  /* 2511 */  { MAD_F(0x042a8b63) /* 0.260386836 */, 17 },

  /* 2512 */  { MAD_F(0x042b1c60) /* 0.260525110 */, 17 },
  /* 2513 */  { MAD_F(0x042bad63) /* 0.260663402 */, 17 },
  /* 2514 */  { MAD_F(0x042c3e6a) /* 0.260801712 */, 17 },
  /* 2515 */  { MAD_F(0x042ccf77) /* 0.260940041 */, 17 },
  /* 2516 */  { MAD_F(0x042d6088) /* 0.261078388 */, 17 },
  /* 2517 */  { MAD_F(0x042df19e) /* 0.261216754 */, 17 },
  /* 2518 */  { MAD_F(0x042e82b9) /* 0.261355137 */, 17 },
  /* 2519 */  { MAD_F(0x042f13d9) /* 0.261493540 */, 17 },
  /* 2520 */  { MAD_F(0x042fa4fe) /* 0.261631960 */, 17 },
  /* 2521 */  { MAD_F(0x04303628) /* 0.261770399 */, 17 },
  /* 2522 */  { MAD_F(0x0430c757) /* 0.261908856 */, 17 },
  /* 2523 */  { MAD_F(0x0431588b) /* 0.262047331 */, 17 },
  /* 2524 */  { MAD_F(0x0431e9c3) /* 0.262185825 */, 17 },
  /* 2525 */  { MAD_F(0x04327b01) /* 0.262324337 */, 17 },
  /* 2526 */  { MAD_F(0x04330c43) /* 0.262462867 */, 17 },
  /* 2527 */  { MAD_F(0x04339d8a) /* 0.262601416 */, 17 },

  /* 2528 */  { MAD_F(0x04342ed7) /* 0.262739982 */, 17 },
  /* 2529 */  { MAD_F(0x0434c028) /* 0.262878568 */, 17 },
  /* 2530 */  { MAD_F(0x0435517e) /* 0.263017171 */, 17 },
  /* 2531 */  { MAD_F(0x0435e2d9) /* 0.263155792 */, 17 },
  /* 2532 */  { MAD_F(0x04367439) /* 0.263294432 */, 17 },
  /* 2533 */  { MAD_F(0x0437059e) /* 0.263433090 */, 17 },
  /* 2534 */  { MAD_F(0x04379707) /* 0.263571767 */, 17 },
  /* 2535 */  { MAD_F(0x04382876) /* 0.263710461 */, 17 },
  /* 2536 */  { MAD_F(0x0438b9e9) /* 0.263849174 */, 17 },
  /* 2537 */  { MAD_F(0x04394b61) /* 0.263987905 */, 17 },
  /* 2538 */  { MAD_F(0x0439dcdf) /* 0.264126655 */, 17 },
  /* 2539 */  { MAD_F(0x043a6e61) /* 0.264265422 */, 17 },
  /* 2540 */  { MAD_F(0x043affe8) /* 0.264404208 */, 17 },
  /* 2541 */  { MAD_F(0x043b9174) /* 0.264543012 */, 17 },
  /* 2542 */  { MAD_F(0x043c2305) /* 0.264681834 */, 17 },
  /* 2543 */  { MAD_F(0x043cb49a) /* 0.264820674 */, 17 },

  /* 2544 */  { MAD_F(0x043d4635) /* 0.264959533 */, 17 },
  /* 2545 */  { MAD_F(0x043dd7d4) /* 0.265098410 */, 17 },
  /* 2546 */  { MAD_F(0x043e6979) /* 0.265237305 */, 17 },
  /* 2547 */  { MAD_F(0x043efb22) /* 0.265376218 */, 17 },
  /* 2548 */  { MAD_F(0x043f8cd0) /* 0.265515149 */, 17 },
  /* 2549 */  { MAD_F(0x04401e83) /* 0.265654099 */, 17 },
  /* 2550 */  { MAD_F(0x0440b03b) /* 0.265793066 */, 17 },
  /* 2551 */  { MAD_F(0x044141f7) /* 0.265932052 */, 17 },
  /* 2552 */  { MAD_F(0x0441d3b9) /* 0.266071056 */, 17 },
  /* 2553 */  { MAD_F(0x04426580) /* 0.266210078 */, 17 },
  /* 2554 */  { MAD_F(0x0442f74b) /* 0.266349119 */, 17 },
  /* 2555 */  { MAD_F(0x0443891b) /* 0.266488177 */, 17 },
  /* 2556 */  { MAD_F(0x04441af0) /* 0.266627254 */, 17 },
  /* 2557 */  { MAD_F(0x0444acca) /* 0.266766349 */, 17 },
  /* 2558 */  { MAD_F(0x04453ea9) /* 0.266905462 */, 17 },
  /* 2559 */  { MAD_F(0x0445d08d) /* 0.267044593 */, 17 },

  /* 2560 */  { MAD_F(0x04466275) /* 0.267183742 */, 17 },
  /* 2561 */  { MAD_F(0x0446f463) /* 0.267322909 */, 17 },
  /* 2562 */  { MAD_F(0x04478655) /* 0.267462094 */, 17 },
  /* 2563 */  { MAD_F(0x0448184c) /* 0.267601298 */, 17 },
  /* 2564 */  { MAD_F(0x0448aa48) /* 0.267740519 */, 17 },
  /* 2565 */  { MAD_F(0x04493c49) /* 0.267879759 */, 17 },
  /* 2566 */  { MAD_F(0x0449ce4f) /* 0.268019017 */, 17 },
  /* 2567 */  { MAD_F(0x044a6059) /* 0.268158293 */, 17 },
  /* 2568 */  { MAD_F(0x044af269) /* 0.268297587 */, 17 },
  /* 2569 */  { MAD_F(0x044b847d) /* 0.268436899 */, 17 },
  /* 2570 */  { MAD_F(0x044c1696) /* 0.268576229 */, 17 },
  /* 2571 */  { MAD_F(0x044ca8b4) /* 0.268715577 */, 17 },
  /* 2572 */  { MAD_F(0x044d3ad7) /* 0.268854943 */, 17 },
  /* 2573 */  { MAD_F(0x044dccff) /* 0.268994328 */, 17 },
  /* 2574 */  { MAD_F(0x044e5f2b) /* 0.269133730 */, 17 },
  /* 2575 */  { MAD_F(0x044ef15d) /* 0.269273150 */, 17 },

  /* 2576 */  { MAD_F(0x044f8393) /* 0.269412589 */, 17 },
  /* 2577 */  { MAD_F(0x045015ce) /* 0.269552045 */, 17 },
  /* 2578 */  { MAD_F(0x0450a80e) /* 0.269691520 */, 17 },
  /* 2579 */  { MAD_F(0x04513a53) /* 0.269831013 */, 17 },
  /* 2580 */  { MAD_F(0x0451cc9c) /* 0.269970523 */, 17 },
  /* 2581 */  { MAD_F(0x04525eeb) /* 0.270110052 */, 17 },
  /* 2582 */  { MAD_F(0x0452f13e) /* 0.270249599 */, 17 },
  /* 2583 */  { MAD_F(0x04538396) /* 0.270389163 */, 17 },
  /* 2584 */  { MAD_F(0x045415f3) /* 0.270528746 */, 17 },
  /* 2585 */  { MAD_F(0x0454a855) /* 0.270668347 */, 17 },
  /* 2586 */  { MAD_F(0x04553abb) /* 0.270807965 */, 17 },
  /* 2587 */  { MAD_F(0x0455cd27) /* 0.270947602 */, 17 },
  /* 2588 */  { MAD_F(0x04565f97) /* 0.271087257 */, 17 },
  /* 2589 */  { MAD_F(0x0456f20c) /* 0.271226930 */, 17 },
  /* 2590 */  { MAD_F(0x04578486) /* 0.271366620 */, 17 },
  /* 2591 */  { MAD_F(0x04581705) /* 0.271506329 */, 17 },

  /* 2592 */  { MAD_F(0x0458a989) /* 0.271646056 */, 17 },
  /* 2593 */  { MAD_F(0x04593c11) /* 0.271785800 */, 17 },
  /* 2594 */  { MAD_F(0x0459ce9e) /* 0.271925563 */, 17 },
  /* 2595 */  { MAD_F(0x045a6130) /* 0.272065343 */, 17 },
  /* 2596 */  { MAD_F(0x045af3c7) /* 0.272205142 */, 17 },
  /* 2597 */  { MAD_F(0x045b8663) /* 0.272344958 */, 17 },
  /* 2598 */  { MAD_F(0x045c1903) /* 0.272484793 */, 17 },
  /* 2599 */  { MAD_F(0x045caba9) /* 0.272624645 */, 17 },
  /* 2600 */  { MAD_F(0x045d3e53) /* 0.272764515 */, 17 },
  /* 2601 */  { MAD_F(0x045dd102) /* 0.272904403 */, 17 },
  /* 2602 */  { MAD_F(0x045e63b6) /* 0.273044310 */, 17 },
  /* 2603 */  { MAD_F(0x045ef66e) /* 0.273184234 */, 17 },
  /* 2604 */  { MAD_F(0x045f892b) /* 0.273324176 */, 17 },
  /* 2605 */  { MAD_F(0x04601bee) /* 0.273464136 */, 17 },
  /* 2606 */  { MAD_F(0x0460aeb5) /* 0.273604113 */, 17 },
  /* 2607 */  { MAD_F(0x04614180) /* 0.273744109 */, 17 },

  /* 2608 */  { MAD_F(0x0461d451) /* 0.273884123 */, 17 },
  /* 2609 */  { MAD_F(0x04626727) /* 0.274024154 */, 17 },
  /* 2610 */  { MAD_F(0x0462fa01) /* 0.274164204 */, 17 },
  /* 2611 */  { MAD_F(0x04638ce0) /* 0.274304271 */, 17 },
  /* 2612 */  { MAD_F(0x04641fc4) /* 0.274444356 */, 17 },
  /* 2613 */  { MAD_F(0x0464b2ac) /* 0.274584459 */, 17 },
  /* 2614 */  { MAD_F(0x0465459a) /* 0.274724580 */, 17 },
  /* 2615 */  { MAD_F(0x0465d88c) /* 0.274864719 */, 17 },
  /* 2616 */  { MAD_F(0x04666b83) /* 0.275004875 */, 17 },
  /* 2617 */  { MAD_F(0x0466fe7f) /* 0.275145050 */, 17 },
  /* 2618 */  { MAD_F(0x0467917f) /* 0.275285242 */, 17 },
  /* 2619 */  { MAD_F(0x04682485) /* 0.275425452 */, 17 },
  /* 2620 */  { MAD_F(0x0468b78f) /* 0.275565681 */, 17 },
  /* 2621 */  { MAD_F(0x04694a9e) /* 0.275705926 */, 17 },
  /* 2622 */  { MAD_F(0x0469ddb2) /* 0.275846190 */, 17 },
  /* 2623 */  { MAD_F(0x046a70ca) /* 0.275986472 */, 17 },

  /* 2624 */  { MAD_F(0x046b03e7) /* 0.276126771 */, 17 },
  /* 2625 */  { MAD_F(0x046b970a) /* 0.276267088 */, 17 },
  /* 2626 */  { MAD_F(0x046c2a31) /* 0.276407423 */, 17 },
  /* 2627 */  { MAD_F(0x046cbd5c) /* 0.276547776 */, 17 },
  /* 2628 */  { MAD_F(0x046d508d) /* 0.276688147 */, 17 },
  /* 2629 */  { MAD_F(0x046de3c2) /* 0.276828535 */, 17 },
  /* 2630 */  { MAD_F(0x046e76fc) /* 0.276968942 */, 17 },
  /* 2631 */  { MAD_F(0x046f0a3b) /* 0.277109366 */, 17 },
  /* 2632 */  { MAD_F(0x046f9d7e) /* 0.277249808 */, 17 },
  /* 2633 */  { MAD_F(0x047030c7) /* 0.277390267 */, 17 },
  /* 2634 */  { MAD_F(0x0470c414) /* 0.277530745 */, 17 },
  /* 2635 */  { MAD_F(0x04715766) /* 0.277671240 */, 17 },
  /* 2636 */  { MAD_F(0x0471eabc) /* 0.277811753 */, 17 },
  /* 2637 */  { MAD_F(0x04727e18) /* 0.277952284 */, 17 },
  /* 2638 */  { MAD_F(0x04731178) /* 0.278092832 */, 17 },
  /* 2639 */  { MAD_F(0x0473a4dd) /* 0.278233399 */, 17 },

  /* 2640 */  { MAD_F(0x04743847) /* 0.278373983 */, 17 },
  /* 2641 */  { MAD_F(0x0474cbb5) /* 0.278514584 */, 17 },
  /* 2642 */  { MAD_F(0x04755f29) /* 0.278655204 */, 17 },
  /* 2643 */  { MAD_F(0x0475f2a1) /* 0.278795841 */, 17 },
  /* 2644 */  { MAD_F(0x0476861d) /* 0.278936496 */, 17 },
  /* 2645 */  { MAD_F(0x0477199f) /* 0.279077169 */, 17 },
  /* 2646 */  { MAD_F(0x0477ad25) /* 0.279217860 */, 17 },
  /* 2647 */  { MAD_F(0x047840b0) /* 0.279358568 */, 17 },
  /* 2648 */  { MAD_F(0x0478d440) /* 0.279499294 */, 17 },
  /* 2649 */  { MAD_F(0x047967d5) /* 0.279640037 */, 17 },
  /* 2650 */  { MAD_F(0x0479fb6e) /* 0.279780799 */, 17 },
  /* 2651 */  { MAD_F(0x047a8f0c) /* 0.279921578 */, 17 },
  /* 2652 */  { MAD_F(0x047b22af) /* 0.280062375 */, 17 },
  /* 2653 */  { MAD_F(0x047bb657) /* 0.280203189 */, 17 },
  /* 2654 */  { MAD_F(0x047c4a03) /* 0.280344021 */, 17 },
  /* 2655 */  { MAD_F(0x047cddb4) /* 0.280484871 */, 17 },

  /* 2656 */  { MAD_F(0x047d716a) /* 0.280625739 */, 17 },
  /* 2657 */  { MAD_F(0x047e0524) /* 0.280766624 */, 17 },
  /* 2658 */  { MAD_F(0x047e98e4) /* 0.280907527 */, 17 },
  /* 2659 */  { MAD_F(0x047f2ca8) /* 0.281048447 */, 17 },
  /* 2660 */  { MAD_F(0x047fc071) /* 0.281189385 */, 17 },
  /* 2661 */  { MAD_F(0x0480543e) /* 0.281330341 */, 17 },
  /* 2662 */  { MAD_F(0x0480e811) /* 0.281471315 */, 17 },
  /* 2663 */  { MAD_F(0x04817be8) /* 0.281612306 */, 17 },
  /* 2664 */  { MAD_F(0x04820fc3) /* 0.281753315 */, 17 },
  /* 2665 */  { MAD_F(0x0482a3a4) /* 0.281894341 */, 17 },
  /* 2666 */  { MAD_F(0x04833789) /* 0.282035386 */, 17 },
  /* 2667 */  { MAD_F(0x0483cb73) /* 0.282176447 */, 17 },
  /* 2668 */  { MAD_F(0x04845f62) /* 0.282317527 */, 17 },
  /* 2669 */  { MAD_F(0x0484f355) /* 0.282458624 */, 17 },
  /* 2670 */  { MAD_F(0x0485874d) /* 0.282599738 */, 17 },
  /* 2671 */  { MAD_F(0x04861b4a) /* 0.282740871 */, 17 },

  /* 2672 */  { MAD_F(0x0486af4c) /* 0.282882021 */, 17 },
  /* 2673 */  { MAD_F(0x04874352) /* 0.283023188 */, 17 },
  /* 2674 */  { MAD_F(0x0487d75d) /* 0.283164373 */, 17 },
  /* 2675 */  { MAD_F(0x04886b6d) /* 0.283305576 */, 17 },
  /* 2676 */  { MAD_F(0x0488ff82) /* 0.283446796 */, 17 },
  /* 2677 */  { MAD_F(0x0489939b) /* 0.283588034 */, 17 },
  /* 2678 */  { MAD_F(0x048a27b9) /* 0.283729290 */, 17 },
  /* 2679 */  { MAD_F(0x048abbdc) /* 0.283870563 */, 17 },
  /* 2680 */  { MAD_F(0x048b5003) /* 0.284011853 */, 17 },
  /* 2681 */  { MAD_F(0x048be42f) /* 0.284153161 */, 17 },
  /* 2682 */  { MAD_F(0x048c7860) /* 0.284294487 */, 17 },
  /* 2683 */  { MAD_F(0x048d0c96) /* 0.284435831 */, 17 },
  /* 2684 */  { MAD_F(0x048da0d0) /* 0.284577192 */, 17 },
  /* 2685 */  { MAD_F(0x048e350f) /* 0.284718570 */, 17 },
  /* 2686 */  { MAD_F(0x048ec953) /* 0.284859966 */, 17 },
  /* 2687 */  { MAD_F(0x048f5d9b) /* 0.285001380 */, 17 },

  /* 2688 */  { MAD_F(0x048ff1e8) /* 0.285142811 */, 17 },
  /* 2689 */  { MAD_F(0x0490863a) /* 0.285284259 */, 17 },
  /* 2690 */  { MAD_F(0x04911a91) /* 0.285425726 */, 17 },
  /* 2691 */  { MAD_F(0x0491aeec) /* 0.285567209 */, 17 },
  /* 2692 */  { MAD_F(0x0492434c) /* 0.285708711 */, 17 },
  /* 2693 */  { MAD_F(0x0492d7b0) /* 0.285850229 */, 17 },
  /* 2694 */  { MAD_F(0x04936c1a) /* 0.285991766 */, 17 },
  /* 2695 */  { MAD_F(0x04940088) /* 0.286133319 */, 17 },
  /* 2696 */  { MAD_F(0x049494fb) /* 0.286274891 */, 17 },
  /* 2697 */  { MAD_F(0x04952972) /* 0.286416480 */, 17 },
  /* 2698 */  { MAD_F(0x0495bdee) /* 0.286558086 */, 17 },
  /* 2699 */  { MAD_F(0x0496526f) /* 0.286699710 */, 17 },
  /* 2700 */  { MAD_F(0x0496e6f5) /* 0.286841351 */, 17 },
  /* 2701 */  { MAD_F(0x04977b7f) /* 0.286983010 */, 17 },
  /* 2702 */  { MAD_F(0x0498100e) /* 0.287124686 */, 17 },
  /* 2703 */  { MAD_F(0x0498a4a1) /* 0.287266380 */, 17 },

  /* 2704 */  { MAD_F(0x0499393a) /* 0.287408091 */, 17 },
  /* 2705 */  { MAD_F(0x0499cdd7) /* 0.287549820 */, 17 },
  /* 2706 */  { MAD_F(0x049a6278) /* 0.287691566 */, 17 },
  /* 2707 */  { MAD_F(0x049af71f) /* 0.287833330 */, 17 },
  /* 2708 */  { MAD_F(0x049b8bca) /* 0.287975111 */, 17 },
  /* 2709 */  { MAD_F(0x049c207a) /* 0.288116909 */, 17 },
  /* 2710 */  { MAD_F(0x049cb52e) /* 0.288258725 */, 17 },
  /* 2711 */  { MAD_F(0x049d49e7) /* 0.288400559 */, 17 },
  /* 2712 */  { MAD_F(0x049ddea5) /* 0.288542409 */, 17 },
  /* 2713 */  { MAD_F(0x049e7367) /* 0.288684278 */, 17 },
  /* 2714 */  { MAD_F(0x049f082f) /* 0.288826163 */, 17 },
  /* 2715 */  { MAD_F(0x049f9cfa) /* 0.288968067 */, 17 },
  /* 2716 */  { MAD_F(0x04a031cb) /* 0.289109987 */, 17 },
  /* 2717 */  { MAD_F(0x04a0c6a0) /* 0.289251925 */, 17 },
  /* 2718 */  { MAD_F(0x04a15b7a) /* 0.289393881 */, 17 },
  /* 2719 */  { MAD_F(0x04a1f059) /* 0.289535854 */, 17 },

  /* 2720 */  { MAD_F(0x04a2853c) /* 0.289677844 */, 17 },
  /* 2721 */  { MAD_F(0x04a31a24) /* 0.289819851 */, 17 },
  /* 2722 */  { MAD_F(0x04a3af10) /* 0.289961876 */, 17 },
  /* 2723 */  { MAD_F(0x04a44401) /* 0.290103919 */, 17 },
  /* 2724 */  { MAD_F(0x04a4d8f7) /* 0.290245979 */, 17 },
  /* 2725 */  { MAD_F(0x04a56df2) /* 0.290388056 */, 17 },
  /* 2726 */  { MAD_F(0x04a602f1) /* 0.290530150 */, 17 },
  /* 2727 */  { MAD_F(0x04a697f5) /* 0.290672262 */, 17 },
  /* 2728 */  { MAD_F(0x04a72cfe) /* 0.290814392 */, 17 },
  /* 2729 */  { MAD_F(0x04a7c20b) /* 0.290956538 */, 17 },
  /* 2730 */  { MAD_F(0x04a8571d) /* 0.291098703 */, 17 },
  /* 2731 */  { MAD_F(0x04a8ec33) /* 0.291240884 */, 17 },
  /* 2732 */  { MAD_F(0x04a9814e) /* 0.291383083 */, 17 },
  /* 2733 */  { MAD_F(0x04aa166e) /* 0.291525299 */, 17 },
  /* 2734 */  { MAD_F(0x04aaab93) /* 0.291667532 */, 17 },
  /* 2735 */  { MAD_F(0x04ab40bc) /* 0.291809783 */, 17 },

  /* 2736 */  { MAD_F(0x04abd5ea) /* 0.291952051 */, 17 },
  /* 2737 */  { MAD_F(0x04ac6b1c) /* 0.292094337 */, 17 },
  /* 2738 */  { MAD_F(0x04ad0053) /* 0.292236640 */, 17 },
  /* 2739 */  { MAD_F(0x04ad958f) /* 0.292378960 */, 17 },
  /* 2740 */  { MAD_F(0x04ae2ad0) /* 0.292521297 */, 17 },
  /* 2741 */  { MAD_F(0x04aec015) /* 0.292663652 */, 17 },
  /* 2742 */  { MAD_F(0x04af555e) /* 0.292806024 */, 17 },
  /* 2743 */  { MAD_F(0x04afeaad) /* 0.292948414 */, 17 },
  /* 2744 */  { MAD_F(0x04b08000) /* 0.293090820 */, 17 },
  /* 2745 */  { MAD_F(0x04b11557) /* 0.293233244 */, 17 },
  /* 2746 */  { MAD_F(0x04b1aab4) /* 0.293375686 */, 17 },
  /* 2747 */  { MAD_F(0x04b24015) /* 0.293518144 */, 17 },
  /* 2748 */  { MAD_F(0x04b2d57a) /* 0.293660620 */, 17 },
  /* 2749 */  { MAD_F(0x04b36ae4) /* 0.293803113 */, 17 },
  /* 2750 */  { MAD_F(0x04b40053) /* 0.293945624 */, 17 },
  /* 2751 */  { MAD_F(0x04b495c7) /* 0.294088151 */, 17 },

  /* 2752 */  { MAD_F(0x04b52b3f) /* 0.294230696 */, 17 },
  /* 2753 */  { MAD_F(0x04b5c0bc) /* 0.294373259 */, 17 },
  /* 2754 */  { MAD_F(0x04b6563d) /* 0.294515838 */, 17 },
  /* 2755 */  { MAD_F(0x04b6ebc3) /* 0.294658435 */, 17 },
  /* 2756 */  { MAD_F(0x04b7814e) /* 0.294801049 */, 17 },
  /* 2757 */  { MAD_F(0x04b816dd) /* 0.294943680 */, 17 },
  /* 2758 */  { MAD_F(0x04b8ac71) /* 0.295086329 */, 17 },
  /* 2759 */  { MAD_F(0x04b9420a) /* 0.295228995 */, 17 },
  /* 2760 */  { MAD_F(0x04b9d7a7) /* 0.295371678 */, 17 },
  /* 2761 */  { MAD_F(0x04ba6d49) /* 0.295514378 */, 17 },
  /* 2762 */  { MAD_F(0x04bb02ef) /* 0.295657095 */, 17 },
  /* 2763 */  { MAD_F(0x04bb989a) /* 0.295799830 */, 17 },
  /* 2764 */  { MAD_F(0x04bc2e4a) /* 0.295942582 */, 17 },
  /* 2765 */  { MAD_F(0x04bcc3fe) /* 0.296085351 */, 17 },
  /* 2766 */  { MAD_F(0x04bd59b7) /* 0.296228138 */, 17 },
  /* 2767 */  { MAD_F(0x04bdef74) /* 0.296370941 */, 17 },

  /* 2768 */  { MAD_F(0x04be8537) /* 0.296513762 */, 17 },
  /* 2769 */  { MAD_F(0x04bf1afd) /* 0.296656600 */, 17 },
  /* 2770 */  { MAD_F(0x04bfb0c9) /* 0.296799455 */, 17 },
  /* 2771 */  { MAD_F(0x04c04699) /* 0.296942327 */, 17 },
  /* 2772 */  { MAD_F(0x04c0dc6d) /* 0.297085217 */, 17 },
  /* 2773 */  { MAD_F(0x04c17247) /* 0.297228124 */, 17 },
  /* 2774 */  { MAD_F(0x04c20824) /* 0.297371048 */, 17 },
  /* 2775 */  { MAD_F(0x04c29e07) /* 0.297513989 */, 17 },
  /* 2776 */  { MAD_F(0x04c333ee) /* 0.297656947 */, 17 },
  /* 2777 */  { MAD_F(0x04c3c9da) /* 0.297799922 */, 17 },
  /* 2778 */  { MAD_F(0x04c45fca) /* 0.297942915 */, 17 },
  /* 2779 */  { MAD_F(0x04c4f5bf) /* 0.298085925 */, 17 },
  /* 2780 */  { MAD_F(0x04c58bb8) /* 0.298228951 */, 17 },
  /* 2781 */  { MAD_F(0x04c621b6) /* 0.298371996 */, 17 },
  /* 2782 */  { MAD_F(0x04c6b7b9) /* 0.298515057 */, 17 },
  /* 2783 */  { MAD_F(0x04c74dc0) /* 0.298658135 */, 17 },

  /* 2784 */  { MAD_F(0x04c7e3cc) /* 0.298801231 */, 17 },
  /* 2785 */  { MAD_F(0x04c879dd) /* 0.298944343 */, 17 },
  /* 2786 */  { MAD_F(0x04c90ff2) /* 0.299087473 */, 17 },
  /* 2787 */  { MAD_F(0x04c9a60c) /* 0.299230620 */, 17 },
  /* 2788 */  { MAD_F(0x04ca3c2a) /* 0.299373784 */, 17 },
  /* 2789 */  { MAD_F(0x04cad24d) /* 0.299516965 */, 17 },
  /* 2790 */  { MAD_F(0x04cb6874) /* 0.299660163 */, 17 },
  /* 2791 */  { MAD_F(0x04cbfea0) /* 0.299803378 */, 17 },
  /* 2792 */  { MAD_F(0x04cc94d1) /* 0.299946611 */, 17 },
  /* 2793 */  { MAD_F(0x04cd2b06) /* 0.300089860 */, 17 },
  /* 2794 */  { MAD_F(0x04cdc140) /* 0.300233127 */, 17 },
  /* 2795 */  { MAD_F(0x04ce577f) /* 0.300376411 */, 17 },
  /* 2796 */  { MAD_F(0x04ceedc2) /* 0.300519711 */, 17 },
  /* 2797 */  { MAD_F(0x04cf8409) /* 0.300663029 */, 17 },
  /* 2798 */  { MAD_F(0x04d01a55) /* 0.300806364 */, 17 },
  /* 2799 */  { MAD_F(0x04d0b0a6) /* 0.300949716 */, 17 },

  /* 2800 */  { MAD_F(0x04d146fb) /* 0.301093085 */, 17 },
  /* 2801 */  { MAD_F(0x04d1dd55) /* 0.301236472 */, 17 },
  /* 2802 */  { MAD_F(0x04d273b4) /* 0.301379875 */, 17 },
  /* 2803 */  { MAD_F(0x04d30a17) /* 0.301523295 */, 17 },
  /* 2804 */  { MAD_F(0x04d3a07f) /* 0.301666733 */, 17 },
  /* 2805 */  { MAD_F(0x04d436eb) /* 0.301810187 */, 17 },
  /* 2806 */  { MAD_F(0x04d4cd5c) /* 0.301953659 */, 17 },
  /* 2807 */  { MAD_F(0x04d563d1) /* 0.302097147 */, 17 },
  /* 2808 */  { MAD_F(0x04d5fa4b) /* 0.302240653 */, 17 },
  /* 2809 */  { MAD_F(0x04d690ca) /* 0.302384175 */, 17 },
  /* 2810 */  { MAD_F(0x04d7274d) /* 0.302527715 */, 17 },
  /* 2811 */  { MAD_F(0x04d7bdd5) /* 0.302671271 */, 17 },
  /* 2812 */  { MAD_F(0x04d85461) /* 0.302814845 */, 17 },
  /* 2813 */  { MAD_F(0x04d8eaf2) /* 0.302958436 */, 17 },
  /* 2814 */  { MAD_F(0x04d98187) /* 0.303102044 */, 17 },
  /* 2815 */  { MAD_F(0x04da1821) /* 0.303245668 */, 17 },

  /* 2816 */  { MAD_F(0x04daaec0) /* 0.303389310 */, 17 },
  /* 2817 */  { MAD_F(0x04db4563) /* 0.303532969 */, 17 },
  /* 2818 */  { MAD_F(0x04dbdc0a) /* 0.303676645 */, 17 },
  /* 2819 */  { MAD_F(0x04dc72b7) /* 0.303820337 */, 17 },
  /* 2820 */  { MAD_F(0x04dd0967) /* 0.303964047 */, 17 },
  /* 2821 */  { MAD_F(0x04dda01d) /* 0.304107774 */, 17 },
  /* 2822 */  { MAD_F(0x04de36d7) /* 0.304251517 */, 17 },
  /* 2823 */  { MAD_F(0x04decd95) /* 0.304395278 */, 17 },
  /* 2824 */  { MAD_F(0x04df6458) /* 0.304539056 */, 17 },
  /* 2825 */  { MAD_F(0x04dffb20) /* 0.304682850 */, 17 },
  /* 2826 */  { MAD_F(0x04e091ec) /* 0.304826662 */, 17 },
  /* 2827 */  { MAD_F(0x04e128bc) /* 0.304970491 */, 17 },
  /* 2828 */  { MAD_F(0x04e1bf92) /* 0.305114336 */, 17 },
  /* 2829 */  { MAD_F(0x04e2566b) /* 0.305258199 */, 17 },
  /* 2830 */  { MAD_F(0x04e2ed4a) /* 0.305402078 */, 17 },
  /* 2831 */  { MAD_F(0x04e3842d) /* 0.305545974 */, 17 },

  /* 2832 */  { MAD_F(0x04e41b14) /* 0.305689888 */, 17 },
  /* 2833 */  { MAD_F(0x04e4b200) /* 0.305833818 */, 17 },
  /* 2834 */  { MAD_F(0x04e548f1) /* 0.305977765 */, 17 },
  /* 2835 */  { MAD_F(0x04e5dfe6) /* 0.306121729 */, 17 },
  /* 2836 */  { MAD_F(0x04e676df) /* 0.306265710 */, 17 },
  /* 2837 */  { MAD_F(0x04e70dde) /* 0.306409708 */, 17 },
  /* 2838 */  { MAD_F(0x04e7a4e0) /* 0.306553723 */, 17 },
  /* 2839 */  { MAD_F(0x04e83be7) /* 0.306697755 */, 17 },
  /* 2840 */  { MAD_F(0x04e8d2f3) /* 0.306841804 */, 17 },
  /* 2841 */  { MAD_F(0x04e96a04) /* 0.306985869 */, 17 },
  /* 2842 */  { MAD_F(0x04ea0118) /* 0.307129952 */, 17 },
  /* 2843 */  { MAD_F(0x04ea9832) /* 0.307274051 */, 17 },
  /* 2844 */  { MAD_F(0x04eb2f50) /* 0.307418168 */, 17 },
  /* 2845 */  { MAD_F(0x04ebc672) /* 0.307562301 */, 17 },
  /* 2846 */  { MAD_F(0x04ec5d99) /* 0.307706451 */, 17 },
  /* 2847 */  { MAD_F(0x04ecf4c5) /* 0.307850618 */, 17 },

  /* 2848 */  { MAD_F(0x04ed8bf5) /* 0.307994802 */, 17 },
  /* 2849 */  { MAD_F(0x04ee2329) /* 0.308139003 */, 17 },
  /* 2850 */  { MAD_F(0x04eeba63) /* 0.308283220 */, 17 },
  /* 2851 */  { MAD_F(0x04ef51a0) /* 0.308427455 */, 17 },
  /* 2852 */  { MAD_F(0x04efe8e2) /* 0.308571706 */, 17 },
  /* 2853 */  { MAD_F(0x04f08029) /* 0.308715974 */, 17 },
  /* 2854 */  { MAD_F(0x04f11774) /* 0.308860260 */, 17 },
  /* 2855 */  { MAD_F(0x04f1aec4) /* 0.309004561 */, 17 },
  /* 2856 */  { MAD_F(0x04f24618) /* 0.309148880 */, 17 },
  /* 2857 */  { MAD_F(0x04f2dd71) /* 0.309293216 */, 17 },
  /* 2858 */  { MAD_F(0x04f374cf) /* 0.309437568 */, 17 },
  /* 2859 */  { MAD_F(0x04f40c30) /* 0.309581938 */, 17 },
  /* 2860 */  { MAD_F(0x04f4a397) /* 0.309726324 */, 17 },
  /* 2861 */  { MAD_F(0x04f53b02) /* 0.309870727 */, 17 },
  /* 2862 */  { MAD_F(0x04f5d271) /* 0.310015147 */, 17 },
  /* 2863 */  { MAD_F(0x04f669e5) /* 0.310159583 */, 17 },

  /* 2864 */  { MAD_F(0x04f7015d) /* 0.310304037 */, 17 },
  /* 2865 */  { MAD_F(0x04f798da) /* 0.310448507 */, 17 },
  /* 2866 */  { MAD_F(0x04f8305c) /* 0.310592994 */, 17 },
  /* 2867 */  { MAD_F(0x04f8c7e2) /* 0.310737498 */, 17 },
  /* 2868 */  { MAD_F(0x04f95f6c) /* 0.310882018 */, 17 },
  /* 2869 */  { MAD_F(0x04f9f6fb) /* 0.311026556 */, 17 },
  /* 2870 */  { MAD_F(0x04fa8e8f) /* 0.311171110 */, 17 },
  /* 2871 */  { MAD_F(0x04fb2627) /* 0.311315681 */, 17 },
  /* 2872 */  { MAD_F(0x04fbbdc3) /* 0.311460269 */, 17 },
  /* 2873 */  { MAD_F(0x04fc5564) /* 0.311604874 */, 17 },
  /* 2874 */  { MAD_F(0x04fced0a) /* 0.311749495 */, 17 },
  /* 2875 */  { MAD_F(0x04fd84b4) /* 0.311894133 */, 17 },
  /* 2876 */  { MAD_F(0x04fe1c62) /* 0.312038788 */, 17 },
  /* 2877 */  { MAD_F(0x04feb415) /* 0.312183460 */, 17 },
  /* 2878 */  { MAD_F(0x04ff4bcd) /* 0.312328148 */, 17 },
  /* 2879 */  { MAD_F(0x04ffe389) /* 0.312472854 */, 17 },

  /* 2880 */  { MAD_F(0x05007b49) /* 0.312617576 */, 17 },
  /* 2881 */  { MAD_F(0x0501130e) /* 0.312762314 */, 17 },
  /* 2882 */  { MAD_F(0x0501aad8) /* 0.312907070 */, 17 },
  /* 2883 */  { MAD_F(0x050242a6) /* 0.313051842 */, 17 },
  /* 2884 */  { MAD_F(0x0502da78) /* 0.313196631 */, 17 },
  /* 2885 */  { MAD_F(0x0503724f) /* 0.313341437 */, 17 },
  /* 2886 */  { MAD_F(0x05040a2b) /* 0.313486259 */, 17 },
  /* 2887 */  { MAD_F(0x0504a20b) /* 0.313631098 */, 17 },
  /* 2888 */  { MAD_F(0x050539ef) /* 0.313775954 */, 17 },
  /* 2889 */  { MAD_F(0x0505d1d8) /* 0.313920827 */, 17 },
  /* 2890 */  { MAD_F(0x050669c5) /* 0.314065716 */, 17 },
  /* 2891 */  { MAD_F(0x050701b7) /* 0.314210622 */, 17 },
  /* 2892 */  { MAD_F(0x050799ae) /* 0.314355545 */, 17 },
  /* 2893 */  { MAD_F(0x050831a9) /* 0.314500484 */, 17 },
  /* 2894 */  { MAD_F(0x0508c9a8) /* 0.314645440 */, 17 },
  /* 2895 */  { MAD_F(0x050961ac) /* 0.314790413 */, 17 },

  /* 2896 */  { MAD_F(0x0509f9b4) /* 0.314935403 */, 17 },
  /* 2897 */  { MAD_F(0x050a91c1) /* 0.315080409 */, 17 },
  /* 2898 */  { MAD_F(0x050b29d2) /* 0.315225432 */, 17 },
  /* 2899 */  { MAD_F(0x050bc1e8) /* 0.315370472 */, 17 },
  /* 2900 */  { MAD_F(0x050c5a02) /* 0.315515528 */, 17 },
  /* 2901 */  { MAD_F(0x050cf221) /* 0.315660601 */, 17 },
  /* 2902 */  { MAD_F(0x050d8a44) /* 0.315805690 */, 17 },
  /* 2903 */  { MAD_F(0x050e226c) /* 0.315950797 */, 17 },
  /* 2904 */  { MAD_F(0x050eba98) /* 0.316095920 */, 17 },
  /* 2905 */  { MAD_F(0x050f52c9) /* 0.316241059 */, 17 },
  /* 2906 */  { MAD_F(0x050feafe) /* 0.316386216 */, 17 },
  /* 2907 */  { MAD_F(0x05108337) /* 0.316531388 */, 17 },
  /* 2908 */  { MAD_F(0x05111b75) /* 0.316676578 */, 17 },
  /* 2909 */  { MAD_F(0x0511b3b8) /* 0.316821784 */, 17 },
  /* 2910 */  { MAD_F(0x05124bff) /* 0.316967007 */, 17 },
  /* 2911 */  { MAD_F(0x0512e44a) /* 0.317112247 */, 17 },

  /* 2912 */  { MAD_F(0x05137c9a) /* 0.317257503 */, 17 },
  /* 2913 */  { MAD_F(0x051414ee) /* 0.317402775 */, 17 },
  /* 2914 */  { MAD_F(0x0514ad47) /* 0.317548065 */, 17 },
  /* 2915 */  { MAD_F(0x051545a5) /* 0.317693371 */, 17 },
  /* 2916 */  { MAD_F(0x0515de06) /* 0.317838693 */, 17 },
  /* 2917 */  { MAD_F(0x0516766d) /* 0.317984033 */, 17 },
  /* 2918 */  { MAD_F(0x05170ed7) /* 0.318129388 */, 17 },
  /* 2919 */  { MAD_F(0x0517a746) /* 0.318274761 */, 17 },
  /* 2920 */  { MAD_F(0x05183fba) /* 0.318420150 */, 17 },
  /* 2921 */  { MAD_F(0x0518d832) /* 0.318565555 */, 17 },
  /* 2922 */  { MAD_F(0x051970ae) /* 0.318710978 */, 17 },
  /* 2923 */  { MAD_F(0x051a092f) /* 0.318856416 */, 17 },
  /* 2924 */  { MAD_F(0x051aa1b5) /* 0.319001872 */, 17 },
  /* 2925 */  { MAD_F(0x051b3a3f) /* 0.319147344 */, 17 },
  /* 2926 */  { MAD_F(0x051bd2cd) /* 0.319292832 */, 17 },
  /* 2927 */  { MAD_F(0x051c6b60) /* 0.319438338 */, 17 },

  /* 2928 */  { MAD_F(0x051d03f7) /* 0.319583859 */, 17 },
  /* 2929 */  { MAD_F(0x051d9c92) /* 0.319729398 */, 17 },
  /* 2930 */  { MAD_F(0x051e3532) /* 0.319874952 */, 17 },
  /* 2931 */  { MAD_F(0x051ecdd7) /* 0.320020524 */, 17 },
  /* 2932 */  { MAD_F(0x051f6680) /* 0.320166112 */, 17 },
  /* 2933 */  { MAD_F(0x051fff2d) /* 0.320311716 */, 17 },
  /* 2934 */  { MAD_F(0x052097df) /* 0.320457337 */, 17 },
  /* 2935 */  { MAD_F(0x05213095) /* 0.320602975 */, 17 },
  /* 2936 */  { MAD_F(0x0521c950) /* 0.320748629 */, 17 },
  /* 2937 */  { MAD_F(0x0522620f) /* 0.320894300 */, 17 },
  /* 2938 */  { MAD_F(0x0522fad3) /* 0.321039987 */, 17 },
  /* 2939 */  { MAD_F(0x0523939b) /* 0.321185691 */, 17 },
  /* 2940 */  { MAD_F(0x05242c68) /* 0.321331411 */, 17 },
  /* 2941 */  { MAD_F(0x0524c538) /* 0.321477148 */, 17 },
  /* 2942 */  { MAD_F(0x05255e0e) /* 0.321622901 */, 17 },
  /* 2943 */  { MAD_F(0x0525f6e8) /* 0.321768671 */, 17 },

  /* 2944 */  { MAD_F(0x05268fc6) /* 0.321914457 */, 17 },
  /* 2945 */  { MAD_F(0x052728a9) /* 0.322060260 */, 17 },
  /* 2946 */  { MAD_F(0x0527c190) /* 0.322206079 */, 17 },
  /* 2947 */  { MAD_F(0x05285a7b) /* 0.322351915 */, 17 },
  /* 2948 */  { MAD_F(0x0528f36b) /* 0.322497768 */, 17 },
  /* 2949 */  { MAD_F(0x05298c5f) /* 0.322643636 */, 17 },
  /* 2950 */  { MAD_F(0x052a2558) /* 0.322789522 */, 17 },
  /* 2951 */  { MAD_F(0x052abe55) /* 0.322935424 */, 17 },
  /* 2952 */  { MAD_F(0x052b5757) /* 0.323081342 */, 17 },
  /* 2953 */  { MAD_F(0x052bf05d) /* 0.323227277 */, 17 },
  /* 2954 */  { MAD_F(0x052c8968) /* 0.323373228 */, 17 },
  /* 2955 */  { MAD_F(0x052d2277) /* 0.323519196 */, 17 },
  /* 2956 */  { MAD_F(0x052dbb8a) /* 0.323665180 */, 17 },
  /* 2957 */  { MAD_F(0x052e54a2) /* 0.323811180 */, 17 },
  /* 2958 */  { MAD_F(0x052eedbe) /* 0.323957197 */, 17 },
  /* 2959 */  { MAD_F(0x052f86de) /* 0.324103231 */, 17 },

  /* 2960 */  { MAD_F(0x05302003) /* 0.324249281 */, 17 },
  /* 2961 */  { MAD_F(0x0530b92d) /* 0.324395347 */, 17 },
  /* 2962 */  { MAD_F(0x0531525b) /* 0.324541430 */, 17 },
  /* 2963 */  { MAD_F(0x0531eb8d) /* 0.324687530 */, 17 },
  /* 2964 */  { MAD_F(0x053284c4) /* 0.324833646 */, 17 },
  /* 2965 */  { MAD_F(0x05331dff) /* 0.324979778 */, 17 },
  /* 2966 */  { MAD_F(0x0533b73e) /* 0.325125926 */, 17 },
  /* 2967 */  { MAD_F(0x05345082) /* 0.325272091 */, 17 },
  /* 2968 */  { MAD_F(0x0534e9ca) /* 0.325418273 */, 17 },
  /* 2969 */  { MAD_F(0x05358317) /* 0.325564471 */, 17 },
  /* 2970 */  { MAD_F(0x05361c68) /* 0.325710685 */, 17 },
  /* 2971 */  { MAD_F(0x0536b5be) /* 0.325856916 */, 17 },
  /* 2972 */  { MAD_F(0x05374f17) /* 0.326003163 */, 17 },
  /* 2973 */  { MAD_F(0x0537e876) /* 0.326149427 */, 17 },
  /* 2974 */  { MAD_F(0x053881d9) /* 0.326295707 */, 17 },
  /* 2975 */  { MAD_F(0x05391b40) /* 0.326442003 */, 17 },

  /* 2976 */  { MAD_F(0x0539b4ab) /* 0.326588316 */, 17 },
  /* 2977 */  { MAD_F(0x053a4e1b) /* 0.326734645 */, 17 },
  /* 2978 */  { MAD_F(0x053ae78f) /* 0.326880990 */, 17 },
  /* 2979 */  { MAD_F(0x053b8108) /* 0.327027352 */, 17 },
  /* 2980 */  { MAD_F(0x053c1a85) /* 0.327173730 */, 17 },
  /* 2981 */  { MAD_F(0x053cb407) /* 0.327320125 */, 17 },
  /* 2982 */  { MAD_F(0x053d4d8d) /* 0.327466536 */, 17 },
  /* 2983 */  { MAD_F(0x053de717) /* 0.327612963 */, 17 },
  /* 2984 */  { MAD_F(0x053e80a6) /* 0.327759407 */, 17 },
  /* 2985 */  { MAD_F(0x053f1a39) /* 0.327905867 */, 17 },
  /* 2986 */  { MAD_F(0x053fb3d0) /* 0.328052344 */, 17 },
  /* 2987 */  { MAD_F(0x05404d6c) /* 0.328198837 */, 17 },
  /* 2988 */  { MAD_F(0x0540e70c) /* 0.328345346 */, 17 },
  /* 2989 */  { MAD_F(0x054180b1) /* 0.328491871 */, 17 },
  /* 2990 */  { MAD_F(0x05421a5a) /* 0.328638413 */, 17 },
  /* 2991 */  { MAD_F(0x0542b407) /* 0.328784971 */, 17 },

  /* 2992 */  { MAD_F(0x05434db9) /* 0.328931546 */, 17 },
  /* 2993 */  { MAD_F(0x0543e76f) /* 0.329078137 */, 17 },
  /* 2994 */  { MAD_F(0x0544812a) /* 0.329224744 */, 17 },
  /* 2995 */  { MAD_F(0x05451ae9) /* 0.329371367 */, 17 },
  /* 2996 */  { MAD_F(0x0545b4ac) /* 0.329518007 */, 17 },
  /* 2997 */  { MAD_F(0x05464e74) /* 0.329664663 */, 17 },
  /* 2998 */  { MAD_F(0x0546e840) /* 0.329811336 */, 17 },
  /* 2999 */  { MAD_F(0x05478211) /* 0.329958024 */, 17 },
  /* 3000 */  { MAD_F(0x05481be5) /* 0.330104730 */, 17 },
  /* 3001 */  { MAD_F(0x0548b5bf) /* 0.330251451 */, 17 },
  /* 3002 */  { MAD_F(0x05494f9c) /* 0.330398189 */, 17 },
  /* 3003 */  { MAD_F(0x0549e97e) /* 0.330544943 */, 17 },
  /* 3004 */  { MAD_F(0x054a8364) /* 0.330691713 */, 17 },
  /* 3005 */  { MAD_F(0x054b1d4f) /* 0.330838499 */, 17 },
  /* 3006 */  { MAD_F(0x054bb73e) /* 0.330985302 */, 17 },
  /* 3007 */  { MAD_F(0x054c5132) /* 0.331132121 */, 17 },

  /* 3008 */  { MAD_F(0x054ceb2a) /* 0.331278957 */, 17 },
  /* 3009 */  { MAD_F(0x054d8526) /* 0.331425808 */, 17 },
  /* 3010 */  { MAD_F(0x054e1f26) /* 0.331572676 */, 17 },
  /* 3011 */  { MAD_F(0x054eb92b) /* 0.331719560 */, 17 },
  /* 3012 */  { MAD_F(0x054f5334) /* 0.331866461 */, 17 },
  /* 3013 */  { MAD_F(0x054fed42) /* 0.332013377 */, 17 },
  /* 3014 */  { MAD_F(0x05508754) /* 0.332160310 */, 17 },
  /* 3015 */  { MAD_F(0x0551216b) /* 0.332307260 */, 17 },
  /* 3016 */  { MAD_F(0x0551bb85) /* 0.332454225 */, 17 },
  /* 3017 */  { MAD_F(0x055255a4) /* 0.332601207 */, 17 },
  /* 3018 */  { MAD_F(0x0552efc8) /* 0.332748205 */, 17 },
  /* 3019 */  { MAD_F(0x055389f0) /* 0.332895219 */, 17 },
  /* 3020 */  { MAD_F(0x0554241c) /* 0.333042249 */, 17 },
  /* 3021 */  { MAD_F(0x0554be4c) /* 0.333189296 */, 17 },
  /* 3022 */  { MAD_F(0x05555881) /* 0.333336359 */, 17 },
  /* 3023 */  { MAD_F(0x0555f2ba) /* 0.333483438 */, 17 },

  /* 3024 */  { MAD_F(0x05568cf8) /* 0.333630533 */, 17 },
  /* 3025 */  { MAD_F(0x0557273a) /* 0.333777645 */, 17 },
  /* 3026 */  { MAD_F(0x0557c180) /* 0.333924772 */, 17 },
  /* 3027 */  { MAD_F(0x05585bcb) /* 0.334071916 */, 17 },
  /* 3028 */  { MAD_F(0x0558f61a) /* 0.334219076 */, 17 },
  /* 3029 */  { MAD_F(0x0559906d) /* 0.334366253 */, 17 },
  /* 3030 */  { MAD_F(0x055a2ac5) /* 0.334513445 */, 17 },
  /* 3031 */  { MAD_F(0x055ac521) /* 0.334660654 */, 17 },
  /* 3032 */  { MAD_F(0x055b5f81) /* 0.334807879 */, 17 },
  /* 3033 */  { MAD_F(0x055bf9e6) /* 0.334955120 */, 17 },
  /* 3034 */  { MAD_F(0x055c944f) /* 0.335102377 */, 17 },
  /* 3035 */  { MAD_F(0x055d2ebd) /* 0.335249651 */, 17 },
  /* 3036 */  { MAD_F(0x055dc92e) /* 0.335396941 */, 17 },
  /* 3037 */  { MAD_F(0x055e63a5) /* 0.335544246 */, 17 },
  /* 3038 */  { MAD_F(0x055efe1f) /* 0.335691568 */, 17 },
  /* 3039 */  { MAD_F(0x055f989e) /* 0.335838906 */, 17 },

  /* 3040 */  { MAD_F(0x05603321) /* 0.335986261 */, 17 },
  /* 3041 */  { MAD_F(0x0560cda8) /* 0.336133631 */, 17 },
  /* 3042 */  { MAD_F(0x05616834) /* 0.336281018 */, 17 },
  /* 3043 */  { MAD_F(0x056202c4) /* 0.336428421 */, 17 },
  /* 3044 */  { MAD_F(0x05629d59) /* 0.336575840 */, 17 },
  /* 3045 */  { MAD_F(0x056337f2) /* 0.336723275 */, 17 },
  /* 3046 */  { MAD_F(0x0563d28f) /* 0.336870726 */, 17 },
  /* 3047 */  { MAD_F(0x05646d30) /* 0.337018193 */, 17 },
  /* 3048 */  { MAD_F(0x056507d6) /* 0.337165677 */, 17 },
  /* 3049 */  { MAD_F(0x0565a280) /* 0.337313176 */, 17 },
  /* 3050 */  { MAD_F(0x05663d2f) /* 0.337460692 */, 17 },
  /* 3051 */  { MAD_F(0x0566d7e1) /* 0.337608224 */, 17 },
  /* 3052 */  { MAD_F(0x05677298) /* 0.337755772 */, 17 },
  /* 3053 */  { MAD_F(0x05680d54) /* 0.337903336 */, 17 },
  /* 3054 */  { MAD_F(0x0568a814) /* 0.338050916 */, 17 },
  /* 3055 */  { MAD_F(0x056942d8) /* 0.338198513 */, 17 },

  /* 3056 */  { MAD_F(0x0569dda0) /* 0.338346125 */, 17 },
  /* 3057 */  { MAD_F(0x056a786d) /* 0.338493753 */, 17 },
  /* 3058 */  { MAD_F(0x056b133e) /* 0.338641398 */, 17 },
  /* 3059 */  { MAD_F(0x056bae13) /* 0.338789059 */, 17 },
  /* 3060 */  { MAD_F(0x056c48ed) /* 0.338936736 */, 17 },
  /* 3061 */  { MAD_F(0x056ce3cb) /* 0.339084429 */, 17 },
  /* 3062 */  { MAD_F(0x056d7ead) /* 0.339232138 */, 17 },
  /* 3063 */  { MAD_F(0x056e1994) /* 0.339379863 */, 17 },
  /* 3064 */  { MAD_F(0x056eb47f) /* 0.339527604 */, 17 },
  /* 3065 */  { MAD_F(0x056f4f6e) /* 0.339675361 */, 17 },
  /* 3066 */  { MAD_F(0x056fea62) /* 0.339823134 */, 17 },
  /* 3067 */  { MAD_F(0x0570855a) /* 0.339970924 */, 17 },
  /* 3068 */  { MAD_F(0x05712056) /* 0.340118729 */, 17 },
  /* 3069 */  { MAD_F(0x0571bb56) /* 0.340266550 */, 17 },
  /* 3070 */  { MAD_F(0x0572565b) /* 0.340414388 */, 17 },
  /* 3071 */  { MAD_F(0x0572f164) /* 0.340562242 */, 17 },

  /* 3072 */  { MAD_F(0x05738c72) /* 0.340710111 */, 17 },
  /* 3073 */  { MAD_F(0x05742784) /* 0.340857997 */, 17 },
  /* 3074 */  { MAD_F(0x0574c29a) /* 0.341005899 */, 17 },
  /* 3075 */  { MAD_F(0x05755db4) /* 0.341153816 */, 17 },
  /* 3076 */  { MAD_F(0x0575f8d3) /* 0.341301750 */, 17 },
  /* 3077 */  { MAD_F(0x057693f6) /* 0.341449700 */, 17 },
  /* 3078 */  { MAD_F(0x05772f1d) /* 0.341597666 */, 17 },
  /* 3079 */  { MAD_F(0x0577ca49) /* 0.341745648 */, 17 },
  /* 3080 */  { MAD_F(0x05786578) /* 0.341893646 */, 17 },
  /* 3081 */  { MAD_F(0x057900ad) /* 0.342041659 */, 17 },
  /* 3082 */  { MAD_F(0x05799be5) /* 0.342189689 */, 17 },
  /* 3083 */  { MAD_F(0x057a3722) /* 0.342337735 */, 17 },
  /* 3084 */  { MAD_F(0x057ad263) /* 0.342485797 */, 17 },
  /* 3085 */  { MAD_F(0x057b6da8) /* 0.342633875 */, 17 },
  /* 3086 */  { MAD_F(0x057c08f2) /* 0.342781969 */, 17 },
  /* 3087 */  { MAD_F(0x057ca440) /* 0.342930079 */, 17 },

  /* 3088 */  { MAD_F(0x057d3f92) /* 0.343078205 */, 17 },
  /* 3089 */  { MAD_F(0x057ddae9) /* 0.343226347 */, 17 },
  /* 3090 */  { MAD_F(0x057e7644) /* 0.343374505 */, 17 },
  /* 3091 */  { MAD_F(0x057f11a3) /* 0.343522679 */, 17 },
  /* 3092 */  { MAD_F(0x057fad06) /* 0.343670869 */, 17 },
  /* 3093 */  { MAD_F(0x0580486e) /* 0.343819075 */, 17 },
  /* 3094 */  { MAD_F(0x0580e3da) /* 0.343967296 */, 17 },
  /* 3095 */  { MAD_F(0x05817f4a) /* 0.344115534 */, 17 },
  /* 3096 */  { MAD_F(0x05821abf) /* 0.344263788 */, 17 },
  /* 3097 */  { MAD_F(0x0582b638) /* 0.344412058 */, 17 },
  /* 3098 */  { MAD_F(0x058351b5) /* 0.344560343 */, 17 },
  /* 3099 */  { MAD_F(0x0583ed36) /* 0.344708645 */, 17 },
  /* 3100 */  { MAD_F(0x058488bc) /* 0.344856963 */, 17 },
  /* 3101 */  { MAD_F(0x05852446) /* 0.345005296 */, 17 },
  /* 3102 */  { MAD_F(0x0585bfd4) /* 0.345153646 */, 17 },
  /* 3103 */  { MAD_F(0x05865b67) /* 0.345302011 */, 17 },

  /* 3104 */  { MAD_F(0x0586f6fd) /* 0.345450393 */, 17 },
  /* 3105 */  { MAD_F(0x05879298) /* 0.345598790 */, 17 },
  /* 3106 */  { MAD_F(0x05882e38) /* 0.345747203 */, 17 },
  /* 3107 */  { MAD_F(0x0588c9dc) /* 0.345895632 */, 17 },
  /* 3108 */  { MAD_F(0x05896583) /* 0.346044077 */, 17 },
  /* 3109 */  { MAD_F(0x058a0130) /* 0.346192538 */, 17 },
  /* 3110 */  { MAD_F(0x058a9ce0) /* 0.346341015 */, 17 },
  /* 3111 */  { MAD_F(0x058b3895) /* 0.346489508 */, 17 },
  /* 3112 */  { MAD_F(0x058bd44e) /* 0.346638017 */, 17 },
  /* 3113 */  { MAD_F(0x058c700b) /* 0.346786542 */, 17 },
  /* 3114 */  { MAD_F(0x058d0bcd) /* 0.346935082 */, 17 },
  /* 3115 */  { MAD_F(0x058da793) /* 0.347083639 */, 17 },
  /* 3116 */  { MAD_F(0x058e435d) /* 0.347232211 */, 17 },
  /* 3117 */  { MAD_F(0x058edf2b) /* 0.347380799 */, 17 },
  /* 3118 */  { MAD_F(0x058f7afe) /* 0.347529403 */, 17 },
  /* 3119 */  { MAD_F(0x059016d5) /* 0.347678023 */, 17 },

  /* 3120 */  { MAD_F(0x0590b2b0) /* 0.347826659 */, 17 },
  /* 3121 */  { MAD_F(0x05914e8f) /* 0.347975311 */, 17 },
  /* 3122 */  { MAD_F(0x0591ea73) /* 0.348123979 */, 17 },
  /* 3123 */  { MAD_F(0x0592865b) /* 0.348272662 */, 17 },
  /* 3124 */  { MAD_F(0x05932247) /* 0.348421362 */, 17 },
  /* 3125 */  { MAD_F(0x0593be37) /* 0.348570077 */, 17 },
  /* 3126 */  { MAD_F(0x05945a2c) /* 0.348718808 */, 17 },
  /* 3127 */  { MAD_F(0x0594f625) /* 0.348867555 */, 17 },
  /* 3128 */  { MAD_F(0x05959222) /* 0.349016318 */, 17 },
  /* 3129 */  { MAD_F(0x05962e24) /* 0.349165097 */, 17 },
  /* 3130 */  { MAD_F(0x0596ca2a) /* 0.349313892 */, 17 },
  /* 3131 */  { MAD_F(0x05976634) /* 0.349462702 */, 17 },
  /* 3132 */  { MAD_F(0x05980242) /* 0.349611528 */, 17 },
  /* 3133 */  { MAD_F(0x05989e54) /* 0.349760370 */, 17 },
  /* 3134 */  { MAD_F(0x05993a6b) /* 0.349909228 */, 17 },
  /* 3135 */  { MAD_F(0x0599d686) /* 0.350058102 */, 17 },

  /* 3136 */  { MAD_F(0x059a72a5) /* 0.350206992 */, 17 },
  /* 3137 */  { MAD_F(0x059b0ec9) /* 0.350355897 */, 17 },
  /* 3138 */  { MAD_F(0x059baaf1) /* 0.350504818 */, 17 },
  /* 3139 */  { MAD_F(0x059c471d) /* 0.350653756 */, 17 },
  /* 3140 */  { MAD_F(0x059ce34d) /* 0.350802708 */, 17 },
  /* 3141 */  { MAD_F(0x059d7f81) /* 0.350951677 */, 17 },
  /* 3142 */  { MAD_F(0x059e1bba) /* 0.351100662 */, 17 },
  /* 3143 */  { MAD_F(0x059eb7f7) /* 0.351249662 */, 17 },
  /* 3144 */  { MAD_F(0x059f5438) /* 0.351398678 */, 17 },
  /* 3145 */  { MAD_F(0x059ff07e) /* 0.351547710 */, 17 },
  /* 3146 */  { MAD_F(0x05a08cc7) /* 0.351696758 */, 17 },
  /* 3147 */  { MAD_F(0x05a12915) /* 0.351845821 */, 17 },
  /* 3148 */  { MAD_F(0x05a1c567) /* 0.351994901 */, 17 },
  /* 3149 */  { MAD_F(0x05a261be) /* 0.352143996 */, 17 },
  /* 3150 */  { MAD_F(0x05a2fe18) /* 0.352293107 */, 17 },
  /* 3151 */  { MAD_F(0x05a39a77) /* 0.352442233 */, 17 },

  /* 3152 */  { MAD_F(0x05a436da) /* 0.352591376 */, 17 },
  /* 3153 */  { MAD_F(0x05a4d342) /* 0.352740534 */, 17 },
  /* 3154 */  { MAD_F(0x05a56fad) /* 0.352889708 */, 17 },
  /* 3155 */  { MAD_F(0x05a60c1d) /* 0.353038898 */, 17 },
  /* 3156 */  { MAD_F(0x05a6a891) /* 0.353188103 */, 17 },
  /* 3157 */  { MAD_F(0x05a7450a) /* 0.353337325 */, 17 },
  /* 3158 */  { MAD_F(0x05a7e186) /* 0.353486562 */, 17 },
  /* 3159 */  { MAD_F(0x05a87e07) /* 0.353635814 */, 17 },
  /* 3160 */  { MAD_F(0x05a91a8c) /* 0.353785083 */, 17 },
  /* 3161 */  { MAD_F(0x05a9b715) /* 0.353934367 */, 17 },
  /* 3162 */  { MAD_F(0x05aa53a2) /* 0.354083667 */, 17 },
  /* 3163 */  { MAD_F(0x05aaf034) /* 0.354232983 */, 17 },
  /* 3164 */  { MAD_F(0x05ab8cca) /* 0.354382314 */, 17 },
  /* 3165 */  { MAD_F(0x05ac2964) /* 0.354531662 */, 17 },
  /* 3166 */  { MAD_F(0x05acc602) /* 0.354681025 */, 17 },
  /* 3167 */  { MAD_F(0x05ad62a5) /* 0.354830403 */, 17 },

  /* 3168 */  { MAD_F(0x05adff4c) /* 0.354979798 */, 17 },
  /* 3169 */  { MAD_F(0x05ae9bf7) /* 0.355129208 */, 17 },
  /* 3170 */  { MAD_F(0x05af38a6) /* 0.355278634 */, 17 },
  /* 3171 */  { MAD_F(0x05afd559) /* 0.355428075 */, 17 },
  /* 3172 */  { MAD_F(0x05b07211) /* 0.355577533 */, 17 },
  /* 3173 */  { MAD_F(0x05b10ecd) /* 0.355727006 */, 17 },
  /* 3174 */  { MAD_F(0x05b1ab8d) /* 0.355876494 */, 17 },
  /* 3175 */  { MAD_F(0x05b24851) /* 0.356025999 */, 17 },
  /* 3176 */  { MAD_F(0x05b2e51a) /* 0.356175519 */, 17 },
  /* 3177 */  { MAD_F(0x05b381e6) /* 0.356325054 */, 17 },
  /* 3178 */  { MAD_F(0x05b41eb7) /* 0.356474606 */, 17 },
  /* 3179 */  { MAD_F(0x05b4bb8c) /* 0.356624173 */, 17 },
  /* 3180 */  { MAD_F(0x05b55866) /* 0.356773756 */, 17 },
  /* 3181 */  { MAD_F(0x05b5f543) /* 0.356923354 */, 17 },
  /* 3182 */  { MAD_F(0x05b69225) /* 0.357072969 */, 17 },
  /* 3183 */  { MAD_F(0x05b72f0b) /* 0.357222598 */, 17 },

  /* 3184 */  { MAD_F(0x05b7cbf5) /* 0.357372244 */, 17 },
  /* 3185 */  { MAD_F(0x05b868e3) /* 0.357521905 */, 17 },
  /* 3186 */  { MAD_F(0x05b905d6) /* 0.357671582 */, 17 },
  /* 3187 */  { MAD_F(0x05b9a2cd) /* 0.357821275 */, 17 },
  /* 3188 */  { MAD_F(0x05ba3fc8) /* 0.357970983 */, 17 },
  /* 3189 */  { MAD_F(0x05badcc7) /* 0.358120707 */, 17 },
  /* 3190 */  { MAD_F(0x05bb79ca) /* 0.358270446 */, 17 },
  /* 3191 */  { MAD_F(0x05bc16d2) /* 0.358420201 */, 17 },
  /* 3192 */  { MAD_F(0x05bcb3de) /* 0.358569972 */, 17 },
  /* 3193 */  { MAD_F(0x05bd50ee) /* 0.358719758 */, 17 },
  /* 3194 */  { MAD_F(0x05bdee02) /* 0.358869560 */, 17 },
  /* 3195 */  { MAD_F(0x05be8b1a) /* 0.359019378 */, 17 },
  /* 3196 */  { MAD_F(0x05bf2837) /* 0.359169211 */, 17 },
  /* 3197 */  { MAD_F(0x05bfc558) /* 0.359319060 */, 17 },
  /* 3198 */  { MAD_F(0x05c0627d) /* 0.359468925 */, 17 },
  /* 3199 */  { MAD_F(0x05c0ffa6) /* 0.359618805 */, 17 },

  /* 3200 */  { MAD_F(0x05c19cd3) /* 0.359768701 */, 17 },
  /* 3201 */  { MAD_F(0x05c23a05) /* 0.359918612 */, 17 },
  /* 3202 */  { MAD_F(0x05c2d73a) /* 0.360068540 */, 17 },
  /* 3203 */  { MAD_F(0x05c37474) /* 0.360218482 */, 17 },
  /* 3204 */  { MAD_F(0x05c411b2) /* 0.360368440 */, 17 },
  /* 3205 */  { MAD_F(0x05c4aef5) /* 0.360518414 */, 17 },
  /* 3206 */  { MAD_F(0x05c54c3b) /* 0.360668404 */, 17 },
  /* 3207 */  { MAD_F(0x05c5e986) /* 0.360818409 */, 17 },
  /* 3208 */  { MAD_F(0x05c686d5) /* 0.360968429 */, 17 },
  /* 3209 */  { MAD_F(0x05c72428) /* 0.361118466 */, 17 },
  /* 3210 */  { MAD_F(0x05c7c17f) /* 0.361268517 */, 17 },
  /* 3211 */  { MAD_F(0x05c85eda) /* 0.361418585 */, 17 },
  /* 3212 */  { MAD_F(0x05c8fc3a) /* 0.361568668 */, 17 },
  /* 3213 */  { MAD_F(0x05c9999e) /* 0.361718766 */, 17 },
  /* 3214 */  { MAD_F(0x05ca3706) /* 0.361868881 */, 17 },
  /* 3215 */  { MAD_F(0x05cad472) /* 0.362019010 */, 17 },

  /* 3216 */  { MAD_F(0x05cb71e2) /* 0.362169156 */, 17 },
  /* 3217 */  { MAD_F(0x05cc0f57) /* 0.362319316 */, 17 },
  /* 3218 */  { MAD_F(0x05ccaccf) /* 0.362469493 */, 17 },
  /* 3219 */  { MAD_F(0x05cd4a4c) /* 0.362619685 */, 17 },
  /* 3220 */  { MAD_F(0x05cde7cd) /* 0.362769892 */, 17 },
  /* 3221 */  { MAD_F(0x05ce8552) /* 0.362920115 */, 17 },
  /* 3222 */  { MAD_F(0x05cf22dc) /* 0.363070354 */, 17 },
  /* 3223 */  { MAD_F(0x05cfc069) /* 0.363220608 */, 17 },
  /* 3224 */  { MAD_F(0x05d05dfb) /* 0.363370878 */, 17 },
  /* 3225 */  { MAD_F(0x05d0fb91) /* 0.363521163 */, 17 },
  /* 3226 */  { MAD_F(0x05d1992b) /* 0.363671464 */, 17 },
  /* 3227 */  { MAD_F(0x05d236c9) /* 0.363821780 */, 17 },
  /* 3228 */  { MAD_F(0x05d2d46c) /* 0.363972112 */, 17 },
  /* 3229 */  { MAD_F(0x05d37212) /* 0.364122459 */, 17 },
  /* 3230 */  { MAD_F(0x05d40fbd) /* 0.364272822 */, 17 },
  /* 3231 */  { MAD_F(0x05d4ad6c) /* 0.364423200 */, 17 },

  /* 3232 */  { MAD_F(0x05d54b1f) /* 0.364573594 */, 17 },
  /* 3233 */  { MAD_F(0x05d5e8d6) /* 0.364724004 */, 17 },
  /* 3234 */  { MAD_F(0x05d68691) /* 0.364874429 */, 17 },
  /* 3235 */  { MAD_F(0x05d72451) /* 0.365024869 */, 17 },
  /* 3236 */  { MAD_F(0x05d7c215) /* 0.365175325 */, 17 },
  /* 3237 */  { MAD_F(0x05d85fdc) /* 0.365325796 */, 17 },
  /* 3238 */  { MAD_F(0x05d8fda8) /* 0.365476283 */, 17 },
  /* 3239 */  { MAD_F(0x05d99b79) /* 0.365626786 */, 17 },
  /* 3240 */  { MAD_F(0x05da394d) /* 0.365777304 */, 17 },
  /* 3241 */  { MAD_F(0x05dad726) /* 0.365927837 */, 17 },
  /* 3242 */  { MAD_F(0x05db7502) /* 0.366078386 */, 17 },
  /* 3243 */  { MAD_F(0x05dc12e3) /* 0.366228950 */, 17 },
  /* 3244 */  { MAD_F(0x05dcb0c8) /* 0.366379530 */, 17 },
  /* 3245 */  { MAD_F(0x05dd4eb1) /* 0.366530125 */, 17 },
  /* 3246 */  { MAD_F(0x05ddec9e) /* 0.366680736 */, 17 },
  /* 3247 */  { MAD_F(0x05de8a90) /* 0.366831362 */, 17 },

  /* 3248 */  { MAD_F(0x05df2885) /* 0.366982004 */, 17 },
  /* 3249 */  { MAD_F(0x05dfc67f) /* 0.367132661 */, 17 },
  /* 3250 */  { MAD_F(0x05e0647d) /* 0.367283334 */, 17 },
  /* 3251 */  { MAD_F(0x05e1027f) /* 0.367434022 */, 17 },
  /* 3252 */  { MAD_F(0x05e1a085) /* 0.367584725 */, 17 },
  /* 3253 */  { MAD_F(0x05e23e8f) /* 0.367735444 */, 17 },
  /* 3254 */  { MAD_F(0x05e2dc9e) /* 0.367886179 */, 17 },
  /* 3255 */  { MAD_F(0x05e37ab0) /* 0.368036929 */, 17 },
  /* 3256 */  { MAD_F(0x05e418c7) /* 0.368187694 */, 17 },
  /* 3257 */  { MAD_F(0x05e4b6e2) /* 0.368338475 */, 17 },
  /* 3258 */  { MAD_F(0x05e55501) /* 0.368489271 */, 17 },
  /* 3259 */  { MAD_F(0x05e5f324) /* 0.368640082 */, 17 },
  /* 3260 */  { MAD_F(0x05e6914c) /* 0.368790909 */, 17 },
  /* 3261 */  { MAD_F(0x05e72f77) /* 0.368941752 */, 17 },
  /* 3262 */  { MAD_F(0x05e7cda7) /* 0.369092610 */, 17 },
  /* 3263 */  { MAD_F(0x05e86bda) /* 0.369243483 */, 17 },

  /* 3264 */  { MAD_F(0x05e90a12) /* 0.369394372 */, 17 },
  /* 3265 */  { MAD_F(0x05e9a84e) /* 0.369545276 */, 17 },
  /* 3266 */  { MAD_F(0x05ea468e) /* 0.369696195 */, 17 },
  /* 3267 */  { MAD_F(0x05eae4d3) /* 0.369847130 */, 17 },
  /* 3268 */  { MAD_F(0x05eb831b) /* 0.369998080 */, 17 },
  /* 3269 */  { MAD_F(0x05ec2168) /* 0.370149046 */, 17 },
  /* 3270 */  { MAD_F(0x05ecbfb8) /* 0.370300027 */, 17 },
  /* 3271 */  { MAD_F(0x05ed5e0d) /* 0.370451024 */, 17 },
  /* 3272 */  { MAD_F(0x05edfc66) /* 0.370602036 */, 17 },
  /* 3273 */  { MAD_F(0x05ee9ac3) /* 0.370753063 */, 17 },
  /* 3274 */  { MAD_F(0x05ef3924) /* 0.370904105 */, 17 },
  /* 3275 */  { MAD_F(0x05efd78a) /* 0.371055163 */, 17 },
  /* 3276 */  { MAD_F(0x05f075f3) /* 0.371206237 */, 17 },
  /* 3277 */  { MAD_F(0x05f11461) /* 0.371357326 */, 17 },
  /* 3278 */  { MAD_F(0x05f1b2d3) /* 0.371508430 */, 17 },
  /* 3279 */  { MAD_F(0x05f25148) /* 0.371659549 */, 17 },

  /* 3280 */  { MAD_F(0x05f2efc2) /* 0.371810684 */, 17 },
  /* 3281 */  { MAD_F(0x05f38e40) /* 0.371961834 */, 17 },
  /* 3282 */  { MAD_F(0x05f42cc3) /* 0.372113000 */, 17 },
  /* 3283 */  { MAD_F(0x05f4cb49) /* 0.372264181 */, 17 },
  /* 3284 */  { MAD_F(0x05f569d3) /* 0.372415377 */, 17 },
  /* 3285 */  { MAD_F(0x05f60862) /* 0.372566589 */, 17 },
  /* 3286 */  { MAD_F(0x05f6a6f5) /* 0.372717816 */, 17 },
  /* 3287 */  { MAD_F(0x05f7458b) /* 0.372869058 */, 17 },
  /* 3288 */  { MAD_F(0x05f7e426) /* 0.373020316 */, 17 },
  /* 3289 */  { MAD_F(0x05f882c5) /* 0.373171589 */, 17 },
  /* 3290 */  { MAD_F(0x05f92169) /* 0.373322877 */, 17 },
  /* 3291 */  { MAD_F(0x05f9c010) /* 0.373474181 */, 17 },
  /* 3292 */  { MAD_F(0x05fa5ebb) /* 0.373625500 */, 17 },
  /* 3293 */  { MAD_F(0x05fafd6b) /* 0.373776834 */, 17 },
  /* 3294 */  { MAD_F(0x05fb9c1e) /* 0.373928184 */, 17 },
  /* 3295 */  { MAD_F(0x05fc3ad6) /* 0.374079549 */, 17 },

  /* 3296 */  { MAD_F(0x05fcd992) /* 0.374230929 */, 17 },
  /* 3297 */  { MAD_F(0x05fd7852) /* 0.374382325 */, 17 },
  /* 3298 */  { MAD_F(0x05fe1716) /* 0.374533735 */, 17 },
  /* 3299 */  { MAD_F(0x05feb5de) /* 0.374685162 */, 17 },
  /* 3300 */  { MAD_F(0x05ff54aa) /* 0.374836603 */, 17 },
  /* 3301 */  { MAD_F(0x05fff37b) /* 0.374988060 */, 17 },
  /* 3302 */  { MAD_F(0x0600924f) /* 0.375139532 */, 17 },
  /* 3303 */  { MAD_F(0x06013128) /* 0.375291019 */, 17 },
  /* 3304 */  { MAD_F(0x0601d004) /* 0.375442522 */, 17 },
  /* 3305 */  { MAD_F(0x06026ee5) /* 0.375594040 */, 17 },
  /* 3306 */  { MAD_F(0x06030dca) /* 0.375745573 */, 17 },
  /* 3307 */  { MAD_F(0x0603acb3) /* 0.375897122 */, 17 },
  /* 3308 */  { MAD_F(0x06044ba0) /* 0.376048685 */, 17 },
  /* 3309 */  { MAD_F(0x0604ea91) /* 0.376200265 */, 17 },
  /* 3310 */  { MAD_F(0x06058987) /* 0.376351859 */, 17 },
  /* 3311 */  { MAD_F(0x06062880) /* 0.376503468 */, 17 },

  /* 3312 */  { MAD_F(0x0606c77d) /* 0.376655093 */, 17 },
  /* 3313 */  { MAD_F(0x0607667f) /* 0.376806733 */, 17 },
  /* 3314 */  { MAD_F(0x06080585) /* 0.376958389 */, 17 },
  /* 3315 */  { MAD_F(0x0608a48f) /* 0.377110059 */, 17 },
  /* 3316 */  { MAD_F(0x0609439c) /* 0.377261745 */, 17 },
  /* 3317 */  { MAD_F(0x0609e2ae) /* 0.377413446 */, 17 },
  /* 3318 */  { MAD_F(0x060a81c4) /* 0.377565163 */, 17 },
  /* 3319 */  { MAD_F(0x060b20df) /* 0.377716894 */, 17 },
  /* 3320 */  { MAD_F(0x060bbffd) /* 0.377868641 */, 17 },
  /* 3321 */  { MAD_F(0x060c5f1f) /* 0.378020403 */, 17 },
  /* 3322 */  { MAD_F(0x060cfe46) /* 0.378172181 */, 17 },
  /* 3323 */  { MAD_F(0x060d9d70) /* 0.378323973 */, 17 },
  /* 3324 */  { MAD_F(0x060e3c9f) /* 0.378475781 */, 17 },
  /* 3325 */  { MAD_F(0x060edbd1) /* 0.378627604 */, 17 },
  /* 3326 */  { MAD_F(0x060f7b08) /* 0.378779442 */, 17 },
  /* 3327 */  { MAD_F(0x06101a43) /* 0.378931296 */, 17 },

  /* 3328 */  { MAD_F(0x0610b982) /* 0.379083164 */, 17 },
  /* 3329 */  { MAD_F(0x061158c5) /* 0.379235048 */, 17 },
  /* 3330 */  { MAD_F(0x0611f80c) /* 0.379386947 */, 17 },
  /* 3331 */  { MAD_F(0x06129757) /* 0.379538862 */, 17 },
  /* 3332 */  { MAD_F(0x061336a6) /* 0.379690791 */, 17 },
  /* 3333 */  { MAD_F(0x0613d5fa) /* 0.379842736 */, 17 },
  /* 3334 */  { MAD_F(0x06147551) /* 0.379994696 */, 17 },
  /* 3335 */  { MAD_F(0x061514ad) /* 0.380146671 */, 17 },
  /* 3336 */  { MAD_F(0x0615b40c) /* 0.380298661 */, 17 },
  /* 3337 */  { MAD_F(0x06165370) /* 0.380450666 */, 17 },
  /* 3338 */  { MAD_F(0x0616f2d8) /* 0.380602687 */, 17 },
  /* 3339 */  { MAD_F(0x06179243) /* 0.380754723 */, 17 },
  /* 3340 */  { MAD_F(0x061831b3) /* 0.380906774 */, 17 },
  /* 3341 */  { MAD_F(0x0618d127) /* 0.381058840 */, 17 },
  /* 3342 */  { MAD_F(0x0619709f) /* 0.381210921 */, 17 },
  /* 3343 */  { MAD_F(0x061a101b) /* 0.381363018 */, 17 },

  /* 3344 */  { MAD_F(0x061aaf9c) /* 0.381515130 */, 17 },
  /* 3345 */  { MAD_F(0x061b4f20) /* 0.381667257 */, 17 },
  /* 3346 */  { MAD_F(0x061beea8) /* 0.381819399 */, 17 },
  /* 3347 */  { MAD_F(0x061c8e34) /* 0.381971556 */, 17 },
  /* 3348 */  { MAD_F(0x061d2dc5) /* 0.382123728 */, 17 },
  /* 3349 */  { MAD_F(0x061dcd59) /* 0.382275916 */, 17 },
  /* 3350 */  { MAD_F(0x061e6cf2) /* 0.382428118 */, 17 },
  /* 3351 */  { MAD_F(0x061f0c8f) /* 0.382580336 */, 17 },
  /* 3352 */  { MAD_F(0x061fac2f) /* 0.382732569 */, 17 },
  /* 3353 */  { MAD_F(0x06204bd4) /* 0.382884817 */, 17 },
  /* 3354 */  { MAD_F(0x0620eb7d) /* 0.383037080 */, 17 },
  /* 3355 */  { MAD_F(0x06218b2a) /* 0.383189358 */, 17 },
  /* 3356 */  { MAD_F(0x06222adb) /* 0.383341652 */, 17 },
  /* 3357 */  { MAD_F(0x0622ca90) /* 0.383493960 */, 17 },
  /* 3358 */  { MAD_F(0x06236a49) /* 0.383646284 */, 17 },
  /* 3359 */  { MAD_F(0x06240a06) /* 0.383798623 */, 17 },

  /* 3360 */  { MAD_F(0x0624a9c7) /* 0.383950977 */, 17 },
  /* 3361 */  { MAD_F(0x0625498d) /* 0.384103346 */, 17 },
  /* 3362 */  { MAD_F(0x0625e956) /* 0.384255730 */, 17 },
  /* 3363 */  { MAD_F(0x06268923) /* 0.384408129 */, 17 },
  /* 3364 */  { MAD_F(0x062728f5) /* 0.384560544 */, 17 },
  /* 3365 */  { MAD_F(0x0627c8ca) /* 0.384712973 */, 17 },
  /* 3366 */  { MAD_F(0x062868a4) /* 0.384865418 */, 17 },
  /* 3367 */  { MAD_F(0x06290881) /* 0.385017878 */, 17 },
  /* 3368 */  { MAD_F(0x0629a863) /* 0.385170352 */, 17 },
  /* 3369 */  { MAD_F(0x062a4849) /* 0.385322842 */, 17 },
  /* 3370 */  { MAD_F(0x062ae832) /* 0.385475347 */, 17 },
  /* 3371 */  { MAD_F(0x062b8820) /* 0.385627867 */, 17 },
  /* 3372 */  { MAD_F(0x062c2812) /* 0.385780402 */, 17 },
  /* 3373 */  { MAD_F(0x062cc808) /* 0.385932953 */, 17 },
  /* 3374 */  { MAD_F(0x062d6802) /* 0.386085518 */, 17 },
  /* 3375 */  { MAD_F(0x062e0800) /* 0.386238098 */, 17 },

  /* 3376 */  { MAD_F(0x062ea802) /* 0.386390694 */, 17 },
  /* 3377 */  { MAD_F(0x062f4808) /* 0.386543304 */, 17 },
  /* 3378 */  { MAD_F(0x062fe812) /* 0.386695930 */, 17 },
  /* 3379 */  { MAD_F(0x06308820) /* 0.386848570 */, 17 },
  /* 3380 */  { MAD_F(0x06312832) /* 0.387001226 */, 17 },
  /* 3381 */  { MAD_F(0x0631c849) /* 0.387153897 */, 17 },
  /* 3382 */  { MAD_F(0x06326863) /* 0.387306582 */, 17 },
  /* 3383 */  { MAD_F(0x06330881) /* 0.387459283 */, 17 },
  /* 3384 */  { MAD_F(0x0633a8a3) /* 0.387611999 */, 17 },
  /* 3385 */  { MAD_F(0x063448ca) /* 0.387764730 */, 17 },
  /* 3386 */  { MAD_F(0x0634e8f4) /* 0.387917476 */, 17 },
  /* 3387 */  { MAD_F(0x06358923) /* 0.388070237 */, 17 },
  /* 3388 */  { MAD_F(0x06362955) /* 0.388223013 */, 17 },
  /* 3389 */  { MAD_F(0x0636c98c) /* 0.388375804 */, 17 },
  /* 3390 */  { MAD_F(0x063769c6) /* 0.388528610 */, 17 },
  /* 3391 */  { MAD_F(0x06380a05) /* 0.388681431 */, 17 },

  /* 3392 */  { MAD_F(0x0638aa48) /* 0.388834268 */, 17 },
  /* 3393 */  { MAD_F(0x06394a8e) /* 0.388987119 */, 17 },
  /* 3394 */  { MAD_F(0x0639ead9) /* 0.389139985 */, 17 },
  /* 3395 */  { MAD_F(0x063a8b28) /* 0.389292866 */, 17 },
  /* 3396 */  { MAD_F(0x063b2b7b) /* 0.389445762 */, 17 },
  /* 3397 */  { MAD_F(0x063bcbd1) /* 0.389598674 */, 17 },
  /* 3398 */  { MAD_F(0x063c6c2c) /* 0.389751600 */, 17 },
  /* 3399 */  { MAD_F(0x063d0c8b) /* 0.389904541 */, 17 },
  /* 3400 */  { MAD_F(0x063dacee) /* 0.390057497 */, 17 },
  /* 3401 */  { MAD_F(0x063e4d55) /* 0.390210468 */, 17 },
  /* 3402 */  { MAD_F(0x063eedc0) /* 0.390363455 */, 17 },
  /* 3403 */  { MAD_F(0x063f8e2f) /* 0.390516456 */, 17 },
  /* 3404 */  { MAD_F(0x06402ea2) /* 0.390669472 */, 17 },
  /* 3405 */  { MAD_F(0x0640cf19) /* 0.390822503 */, 17 },
  /* 3406 */  { MAD_F(0x06416f94) /* 0.390975549 */, 17 },
  /* 3407 */  { MAD_F(0x06421013) /* 0.391128611 */, 17 },

  /* 3408 */  { MAD_F(0x0642b096) /* 0.391281687 */, 17 },
  /* 3409 */  { MAD_F(0x0643511d) /* 0.391434778 */, 17 },
  /* 3410 */  { MAD_F(0x0643f1a8) /* 0.391587884 */, 17 },
  /* 3411 */  { MAD_F(0x06449237) /* 0.391741005 */, 17 },
  /* 3412 */  { MAD_F(0x064532ca) /* 0.391894141 */, 17 },
  /* 3413 */  { MAD_F(0x0645d361) /* 0.392047292 */, 17 },
  /* 3414 */  { MAD_F(0x064673fc) /* 0.392200458 */, 17 },
  /* 3415 */  { MAD_F(0x0647149c) /* 0.392353638 */, 17 },
  /* 3416 */  { MAD_F(0x0647b53f) /* 0.392506834 */, 17 },
  /* 3417 */  { MAD_F(0x064855e6) /* 0.392660045 */, 17 },
  /* 3418 */  { MAD_F(0x0648f691) /* 0.392813271 */, 17 },
  /* 3419 */  { MAD_F(0x06499740) /* 0.392966511 */, 17 },
  /* 3420 */  { MAD_F(0x064a37f4) /* 0.393119767 */, 17 },
  /* 3421 */  { MAD_F(0x064ad8ab) /* 0.393273038 */, 17 },
  /* 3422 */  { MAD_F(0x064b7966) /* 0.393426323 */, 17 },
  /* 3423 */  { MAD_F(0x064c1a25) /* 0.393579623 */, 17 },

  /* 3424 */  { MAD_F(0x064cbae9) /* 0.393732939 */, 17 },
  /* 3425 */  { MAD_F(0x064d5bb0) /* 0.393886269 */, 17 },
  /* 3426 */  { MAD_F(0x064dfc7b) /* 0.394039614 */, 17 },
  /* 3427 */  { MAD_F(0x064e9d4b) /* 0.394192974 */, 17 },
  /* 3428 */  { MAD_F(0x064f3e1e) /* 0.394346349 */, 17 },
  /* 3429 */  { MAD_F(0x064fdef5) /* 0.394499739 */, 17 },
  /* 3430 */  { MAD_F(0x06507fd0) /* 0.394653144 */, 17 },
  /* 3431 */  { MAD_F(0x065120b0) /* 0.394806564 */, 17 },
  /* 3432 */  { MAD_F(0x0651c193) /* 0.394959999 */, 17 },
  /* 3433 */  { MAD_F(0x0652627a) /* 0.395113448 */, 17 },
  /* 3434 */  { MAD_F(0x06530366) /* 0.395266913 */, 17 },
  /* 3435 */  { MAD_F(0x0653a455) /* 0.395420392 */, 17 },
  /* 3436 */  { MAD_F(0x06544548) /* 0.395573886 */, 17 },
  /* 3437 */  { MAD_F(0x0654e640) /* 0.395727395 */, 17 },
  /* 3438 */  { MAD_F(0x0655873b) /* 0.395880919 */, 17 },
  /* 3439 */  { MAD_F(0x0656283a) /* 0.396034458 */, 17 },

  /* 3440 */  { MAD_F(0x0656c93d) /* 0.396188012 */, 17 },
  /* 3441 */  { MAD_F(0x06576a45) /* 0.396341581 */, 17 },
  /* 3442 */  { MAD_F(0x06580b50) /* 0.396495164 */, 17 },
  /* 3443 */  { MAD_F(0x0658ac5f) /* 0.396648763 */, 17 },
  /* 3444 */  { MAD_F(0x06594d73) /* 0.396802376 */, 17 },
  /* 3445 */  { MAD_F(0x0659ee8a) /* 0.396956004 */, 17 },
  /* 3446 */  { MAD_F(0x065a8fa5) /* 0.397109647 */, 17 },
  /* 3447 */  { MAD_F(0x065b30c4) /* 0.397263305 */, 17 },
  /* 3448 */  { MAD_F(0x065bd1e7) /* 0.397416978 */, 17 },
  /* 3449 */  { MAD_F(0x065c730f) /* 0.397570666 */, 17 },
  /* 3450 */  { MAD_F(0x065d143a) /* 0.397724368 */, 17 },
  /* 3451 */  { MAD_F(0x065db569) /* 0.397878085 */, 17 },
  /* 3452 */  { MAD_F(0x065e569c) /* 0.398031818 */, 17 },
  /* 3453 */  { MAD_F(0x065ef7d3) /* 0.398185565 */, 17 },
  /* 3454 */  { MAD_F(0x065f990e) /* 0.398339326 */, 17 },
  /* 3455 */  { MAD_F(0x06603a4e) /* 0.398493103 */, 17 },

  /* 3456 */  { MAD_F(0x0660db91) /* 0.398646895 */, 17 },
  /* 3457 */  { MAD_F(0x06617cd8) /* 0.398800701 */, 17 },
  /* 3458 */  { MAD_F(0x06621e23) /* 0.398954522 */, 17 },
  /* 3459 */  { MAD_F(0x0662bf72) /* 0.399108358 */, 17 },
  /* 3460 */  { MAD_F(0x066360c5) /* 0.399262209 */, 17 },
  /* 3461 */  { MAD_F(0x0664021c) /* 0.399416075 */, 17 },
  /* 3462 */  { MAD_F(0x0664a377) /* 0.399569955 */, 17 },
  /* 3463 */  { MAD_F(0x066544d6) /* 0.399723851 */, 17 },
  /* 3464 */  { MAD_F(0x0665e639) /* 0.399877761 */, 17 },
  /* 3465 */  { MAD_F(0x066687a0) /* 0.400031686 */, 17 },
  /* 3466 */  { MAD_F(0x0667290b) /* 0.400185625 */, 17 },
  /* 3467 */  { MAD_F(0x0667ca79) /* 0.400339580 */, 17 },
  /* 3468 */  { MAD_F(0x06686bec) /* 0.400493549 */, 17 },
  /* 3469 */  { MAD_F(0x06690d63) /* 0.400647534 */, 17 },
  /* 3470 */  { MAD_F(0x0669aede) /* 0.400801533 */, 17 },
  /* 3471 */  { MAD_F(0x066a505d) /* 0.400955546 */, 17 },

  /* 3472 */  { MAD_F(0x066af1df) /* 0.401109575 */, 17 },
  /* 3473 */  { MAD_F(0x066b9366) /* 0.401263618 */, 17 },
  /* 3474 */  { MAD_F(0x066c34f1) /* 0.401417676 */, 17 },
  /* 3475 */  { MAD_F(0x066cd67f) /* 0.401571749 */, 17 },
  /* 3476 */  { MAD_F(0x066d7812) /* 0.401725837 */, 17 },
  /* 3477 */  { MAD_F(0x066e19a9) /* 0.401879939 */, 17 },
  /* 3478 */  { MAD_F(0x066ebb43) /* 0.402034056 */, 17 },
  /* 3479 */  { MAD_F(0x066f5ce2) /* 0.402188188 */, 17 },
  /* 3480 */  { MAD_F(0x066ffe84) /* 0.402342335 */, 17 },
  /* 3481 */  { MAD_F(0x0670a02a) /* 0.402496497 */, 17 },
  /* 3482 */  { MAD_F(0x067141d5) /* 0.402650673 */, 17 },
  /* 3483 */  { MAD_F(0x0671e383) /* 0.402804864 */, 17 },
  /* 3484 */  { MAD_F(0x06728535) /* 0.402959070 */, 17 },
  /* 3485 */  { MAD_F(0x067326ec) /* 0.403113291 */, 17 },
  /* 3486 */  { MAD_F(0x0673c8a6) /* 0.403267526 */, 17 },
  /* 3487 */  { MAD_F(0x06746a64) /* 0.403421776 */, 17 },

  /* 3488 */  { MAD_F(0x06750c26) /* 0.403576041 */, 17 },
  /* 3489 */  { MAD_F(0x0675adec) /* 0.403730320 */, 17 },
  /* 3490 */  { MAD_F(0x06764fb6) /* 0.403884615 */, 17 },
  /* 3491 */  { MAD_F(0x0676f184) /* 0.404038924 */, 17 },
  /* 3492 */  { MAD_F(0x06779356) /* 0.404193247 */, 17 },
  /* 3493 */  { MAD_F(0x0678352c) /* 0.404347586 */, 17 },
  /* 3494 */  { MAD_F(0x0678d706) /* 0.404501939 */, 17 },
  /* 3495 */  { MAD_F(0x067978e4) /* 0.404656307 */, 17 },
  /* 3496 */  { MAD_F(0x067a1ac6) /* 0.404810690 */, 17 },
  /* 3497 */  { MAD_F(0x067abcac) /* 0.404965087 */, 17 },
  /* 3498 */  { MAD_F(0x067b5e95) /* 0.405119499 */, 17 },
  /* 3499 */  { MAD_F(0x067c0083) /* 0.405273926 */, 17 },
  /* 3500 */  { MAD_F(0x067ca275) /* 0.405428368 */, 17 },
  /* 3501 */  { MAD_F(0x067d446a) /* 0.405582824 */, 17 },
  /* 3502 */  { MAD_F(0x067de664) /* 0.405737295 */, 17 },
  /* 3503 */  { MAD_F(0x067e8861) /* 0.405891781 */, 17 },

  /* 3504 */  { MAD_F(0x067f2a62) /* 0.406046281 */, 17 },
  /* 3505 */  { MAD_F(0x067fcc68) /* 0.406200796 */, 17 },
  /* 3506 */  { MAD_F(0x06806e71) /* 0.406355326 */, 17 },
  /* 3507 */  { MAD_F(0x0681107e) /* 0.406509870 */, 17 },
  /* 3508 */  { MAD_F(0x0681b28f) /* 0.406664429 */, 17 },
  /* 3509 */  { MAD_F(0x068254a4) /* 0.406819003 */, 17 },
  /* 3510 */  { MAD_F(0x0682f6bd) /* 0.406973592 */, 17 },
  /* 3511 */  { MAD_F(0x068398da) /* 0.407128195 */, 17 },
  /* 3512 */  { MAD_F(0x06843afb) /* 0.407282813 */, 17 },
  /* 3513 */  { MAD_F(0x0684dd20) /* 0.407437445 */, 17 },
  /* 3514 */  { MAD_F(0x06857f49) /* 0.407592093 */, 17 },
  /* 3515 */  { MAD_F(0x06862176) /* 0.407746754 */, 17 },
  /* 3516 */  { MAD_F(0x0686c3a6) /* 0.407901431 */, 17 },
  /* 3517 */  { MAD_F(0x068765db) /* 0.408056122 */, 17 },
  /* 3518 */  { MAD_F(0x06880814) /* 0.408210828 */, 17 },
  /* 3519 */  { MAD_F(0x0688aa50) /* 0.408365549 */, 17 },

  /* 3520 */  { MAD_F(0x06894c90) /* 0.408520284 */, 17 },
  /* 3521 */  { MAD_F(0x0689eed5) /* 0.408675034 */, 17 },
  /* 3522 */  { MAD_F(0x068a911d) /* 0.408829798 */, 17 },
  /* 3523 */  { MAD_F(0x068b3369) /* 0.408984577 */, 17 },
  /* 3524 */  { MAD_F(0x068bd5b9) /* 0.409139371 */, 17 },
  /* 3525 */  { MAD_F(0x068c780e) /* 0.409294180 */, 17 },
  /* 3526 */  { MAD_F(0x068d1a66) /* 0.409449003 */, 17 },
  /* 3527 */  { MAD_F(0x068dbcc1) /* 0.409603840 */, 17 },
  /* 3528 */  { MAD_F(0x068e5f21) /* 0.409758693 */, 17 },
  /* 3529 */  { MAD_F(0x068f0185) /* 0.409913560 */, 17 },
  /* 3530 */  { MAD_F(0x068fa3ed) /* 0.410068441 */, 17 },
  /* 3531 */  { MAD_F(0x06904658) /* 0.410223338 */, 17 },
  /* 3532 */  { MAD_F(0x0690e8c8) /* 0.410378249 */, 17 },
  /* 3533 */  { MAD_F(0x06918b3c) /* 0.410533174 */, 17 },
  /* 3534 */  { MAD_F(0x06922db3) /* 0.410688114 */, 17 },
  /* 3535 */  { MAD_F(0x0692d02e) /* 0.410843069 */, 17 },

  /* 3536 */  { MAD_F(0x069372ae) /* 0.410998038 */, 17 },
  /* 3537 */  { MAD_F(0x06941531) /* 0.411153022 */, 17 },
  /* 3538 */  { MAD_F(0x0694b7b8) /* 0.411308021 */, 17 },
  /* 3539 */  { MAD_F(0x06955a43) /* 0.411463034 */, 17 },
  /* 3540 */  { MAD_F(0x0695fcd2) /* 0.411618062 */, 17 },
  /* 3541 */  { MAD_F(0x06969f65) /* 0.411773104 */, 17 },
  /* 3542 */  { MAD_F(0x069741fb) /* 0.411928161 */, 17 },
  /* 3543 */  { MAD_F(0x0697e496) /* 0.412083232 */, 17 },
  /* 3544 */  { MAD_F(0x06988735) /* 0.412238319 */, 17 },
  /* 3545 */  { MAD_F(0x069929d7) /* 0.412393419 */, 17 },
  /* 3546 */  { MAD_F(0x0699cc7e) /* 0.412548535 */, 17 },
  /* 3547 */  { MAD_F(0x069a6f28) /* 0.412703664 */, 17 },
  /* 3548 */  { MAD_F(0x069b11d6) /* 0.412858809 */, 17 },
  /* 3549 */  { MAD_F(0x069bb489) /* 0.413013968 */, 17 },
  /* 3550 */  { MAD_F(0x069c573f) /* 0.413169142 */, 17 },
  /* 3551 */  { MAD_F(0x069cf9f9) /* 0.413324330 */, 17 },

  /* 3552 */  { MAD_F(0x069d9cb7) /* 0.413479532 */, 17 },
  /* 3553 */  { MAD_F(0x069e3f78) /* 0.413634750 */, 17 },
  /* 3554 */  { MAD_F(0x069ee23e) /* 0.413789982 */, 17 },
  /* 3555 */  { MAD_F(0x069f8508) /* 0.413945228 */, 17 },
  /* 3556 */  { MAD_F(0x06a027d5) /* 0.414100489 */, 17 },
  /* 3557 */  { MAD_F(0x06a0caa7) /* 0.414255765 */, 17 },
  /* 3558 */  { MAD_F(0x06a16d7c) /* 0.414411055 */, 17 },
  /* 3559 */  { MAD_F(0x06a21055) /* 0.414566359 */, 17 },
  /* 3560 */  { MAD_F(0x06a2b333) /* 0.414721679 */, 17 },
  /* 3561 */  { MAD_F(0x06a35614) /* 0.414877012 */, 17 },
  /* 3562 */  { MAD_F(0x06a3f8f9) /* 0.415032361 */, 17 },
  /* 3563 */  { MAD_F(0x06a49be2) /* 0.415187723 */, 17 },
  /* 3564 */  { MAD_F(0x06a53ece) /* 0.415343101 */, 17 },
  /* 3565 */  { MAD_F(0x06a5e1bf) /* 0.415498493 */, 17 },
  /* 3566 */  { MAD_F(0x06a684b4) /* 0.415653899 */, 17 },
  /* 3567 */  { MAD_F(0x06a727ac) /* 0.415809320 */, 17 },

  /* 3568 */  { MAD_F(0x06a7caa9) /* 0.415964756 */, 17 },
  /* 3569 */  { MAD_F(0x06a86da9) /* 0.416120206 */, 17 },
  /* 3570 */  { MAD_F(0x06a910ad) /* 0.416275670 */, 17 },
  /* 3571 */  { MAD_F(0x06a9b3b5) /* 0.416431149 */, 17 },
  /* 3572 */  { MAD_F(0x06aa56c1) /* 0.416586643 */, 17 },
  /* 3573 */  { MAD_F(0x06aaf9d1) /* 0.416742151 */, 17 },
  /* 3574 */  { MAD_F(0x06ab9ce5) /* 0.416897673 */, 17 },
  /* 3575 */  { MAD_F(0x06ac3ffc) /* 0.417053210 */, 17 },
  /* 3576 */  { MAD_F(0x06ace318) /* 0.417208762 */, 17 },
  /* 3577 */  { MAD_F(0x06ad8637) /* 0.417364328 */, 17 },
  /* 3578 */  { MAD_F(0x06ae295b) /* 0.417519909 */, 17 },
  /* 3579 */  { MAD_F(0x06aecc82) /* 0.417675504 */, 17 },
  /* 3580 */  { MAD_F(0x06af6fad) /* 0.417831113 */, 17 },
  /* 3581 */  { MAD_F(0x06b012dc) /* 0.417986737 */, 17 },
  /* 3582 */  { MAD_F(0x06b0b60f) /* 0.418142376 */, 17 },
  /* 3583 */  { MAD_F(0x06b15946) /* 0.418298029 */, 17 },

  /* 3584 */  { MAD_F(0x06b1fc81) /* 0.418453696 */, 17 },
  /* 3585 */  { MAD_F(0x06b29fbf) /* 0.418609378 */, 17 },
  /* 3586 */  { MAD_F(0x06b34302) /* 0.418765075 */, 17 },
  /* 3587 */  { MAD_F(0x06b3e648) /* 0.418920786 */, 17 },
  /* 3588 */  { MAD_F(0x06b48992) /* 0.419076511 */, 17 },
  /* 3589 */  { MAD_F(0x06b52ce0) /* 0.419232251 */, 17 },
  /* 3590 */  { MAD_F(0x06b5d032) /* 0.419388005 */, 17 },
  /* 3591 */  { MAD_F(0x06b67388) /* 0.419543774 */, 17 },
  /* 3592 */  { MAD_F(0x06b716e2) /* 0.419699557 */, 17 },
  /* 3593 */  { MAD_F(0x06b7ba3f) /* 0.419855355 */, 17 },
  /* 3594 */  { MAD_F(0x06b85da1) /* 0.420011167 */, 17 },
  /* 3595 */  { MAD_F(0x06b90106) /* 0.420166994 */, 17 },
  /* 3596 */  { MAD_F(0x06b9a470) /* 0.420322835 */, 17 },
  /* 3597 */  { MAD_F(0x06ba47dd) /* 0.420478690 */, 17 },
  /* 3598 */  { MAD_F(0x06baeb4e) /* 0.420634560 */, 17 },
  /* 3599 */  { MAD_F(0x06bb8ec3) /* 0.420790445 */, 17 },

  /* 3600 */  { MAD_F(0x06bc323b) /* 0.420946343 */, 17 },
  /* 3601 */  { MAD_F(0x06bcd5b8) /* 0.421102257 */, 17 },
  /* 3602 */  { MAD_F(0x06bd7939) /* 0.421258184 */, 17 },
  /* 3603 */  { MAD_F(0x06be1cbd) /* 0.421414127 */, 17 },
  /* 3604 */  { MAD_F(0x06bec045) /* 0.421570083 */, 17 },
  /* 3605 */  { MAD_F(0x06bf63d1) /* 0.421726054 */, 17 },
  /* 3606 */  { MAD_F(0x06c00761) /* 0.421882040 */, 17 },
  /* 3607 */  { MAD_F(0x06c0aaf5) /* 0.422038039 */, 17 },
  /* 3608 */  { MAD_F(0x06c14e8d) /* 0.422194054 */, 17 },
  /* 3609 */  { MAD_F(0x06c1f229) /* 0.422350082 */, 17 },
  /* 3610 */  { MAD_F(0x06c295c8) /* 0.422506125 */, 17 },
  /* 3611 */  { MAD_F(0x06c3396c) /* 0.422662183 */, 17 },
  /* 3612 */  { MAD_F(0x06c3dd13) /* 0.422818255 */, 17 },
  /* 3613 */  { MAD_F(0x06c480be) /* 0.422974341 */, 17 },
  /* 3614 */  { MAD_F(0x06c5246d) /* 0.423130442 */, 17 },
  /* 3615 */  { MAD_F(0x06c5c820) /* 0.423286557 */, 17 },

  /* 3616 */  { MAD_F(0x06c66bd6) /* 0.423442686 */, 17 },
  /* 3617 */  { MAD_F(0x06c70f91) /* 0.423598830 */, 17 },
  /* 3618 */  { MAD_F(0x06c7b34f) /* 0.423754988 */, 17 },
  /* 3619 */  { MAD_F(0x06c85712) /* 0.423911161 */, 17 },
  /* 3620 */  { MAD_F(0x06c8fad8) /* 0.424067348 */, 17 },
  /* 3621 */  { MAD_F(0x06c99ea2) /* 0.424223550 */, 17 },
  /* 3622 */  { MAD_F(0x06ca4270) /* 0.424379765 */, 17 },
  /* 3623 */  { MAD_F(0x06cae641) /* 0.424535996 */, 17 },
  /* 3624 */  { MAD_F(0x06cb8a17) /* 0.424692240 */, 17 },
  /* 3625 */  { MAD_F(0x06cc2df0) /* 0.424848499 */, 17 },
  /* 3626 */  { MAD_F(0x06ccd1ce) /* 0.425004772 */, 17 },
  /* 3627 */  { MAD_F(0x06cd75af) /* 0.425161060 */, 17 },
  /* 3628 */  { MAD_F(0x06ce1994) /* 0.425317362 */, 17 },
  /* 3629 */  { MAD_F(0x06cebd7d) /* 0.425473678 */, 17 },
  /* 3630 */  { MAD_F(0x06cf6169) /* 0.425630009 */, 17 },
  /* 3631 */  { MAD_F(0x06d0055a) /* 0.425786354 */, 17 },

  /* 3632 */  { MAD_F(0x06d0a94e) /* 0.425942714 */, 17 },
  /* 3633 */  { MAD_F(0x06d14d47) /* 0.426099088 */, 17 },
  /* 3634 */  { MAD_F(0x06d1f143) /* 0.426255476 */, 17 },
  /* 3635 */  { MAD_F(0x06d29543) /* 0.426411878 */, 17 },
  /* 3636 */  { MAD_F(0x06d33947) /* 0.426568295 */, 17 },
  /* 3637 */  { MAD_F(0x06d3dd4e) /* 0.426724726 */, 17 },
  /* 3638 */  { MAD_F(0x06d4815a) /* 0.426881172 */, 17 },
  /* 3639 */  { MAD_F(0x06d52569) /* 0.427037632 */, 17 },
  /* 3640 */  { MAD_F(0x06d5c97c) /* 0.427194106 */, 17 },
  /* 3641 */  { MAD_F(0x06d66d93) /* 0.427350594 */, 17 },
  /* 3642 */  { MAD_F(0x06d711ae) /* 0.427507097 */, 17 },
  /* 3643 */  { MAD_F(0x06d7b5cd) /* 0.427663614 */, 17 },
  /* 3644 */  { MAD_F(0x06d859f0) /* 0.427820146 */, 17 },
  /* 3645 */  { MAD_F(0x06d8fe16) /* 0.427976692 */, 17 },
  /* 3646 */  { MAD_F(0x06d9a240) /* 0.428133252 */, 17 },
  /* 3647 */  { MAD_F(0x06da466f) /* 0.428289826 */, 17 },

  /* 3648 */  { MAD_F(0x06daeaa1) /* 0.428446415 */, 17 },
  /* 3649 */  { MAD_F(0x06db8ed6) /* 0.428603018 */, 17 },
  /* 3650 */  { MAD_F(0x06dc3310) /* 0.428759635 */, 17 },
  /* 3651 */  { MAD_F(0x06dcd74d) /* 0.428916267 */, 17 },
  /* 3652 */  { MAD_F(0x06dd7b8f) /* 0.429072913 */, 17 },
  /* 3653 */  { MAD_F(0x06de1fd4) /* 0.429229573 */, 17 },
  /* 3654 */  { MAD_F(0x06dec41d) /* 0.429386248 */, 17 },
  /* 3655 */  { MAD_F(0x06df686a) /* 0.429542937 */, 17 },
  /* 3656 */  { MAD_F(0x06e00cbb) /* 0.429699640 */, 17 },
  /* 3657 */  { MAD_F(0x06e0b10f) /* 0.429856357 */, 17 },
  /* 3658 */  { MAD_F(0x06e15567) /* 0.430013089 */, 17 },
  /* 3659 */  { MAD_F(0x06e1f9c4) /* 0.430169835 */, 17 },
  /* 3660 */  { MAD_F(0x06e29e24) /* 0.430326595 */, 17 },
  /* 3661 */  { MAD_F(0x06e34287) /* 0.430483370 */, 17 },
  /* 3662 */  { MAD_F(0x06e3e6ef) /* 0.430640159 */, 17 },
  /* 3663 */  { MAD_F(0x06e48b5b) /* 0.430796962 */, 17 },

  /* 3664 */  { MAD_F(0x06e52fca) /* 0.430953779 */, 17 },
  /* 3665 */  { MAD_F(0x06e5d43d) /* 0.431110611 */, 17 },
  /* 3666 */  { MAD_F(0x06e678b4) /* 0.431267457 */, 17 },
  /* 3667 */  { MAD_F(0x06e71d2f) /* 0.431424317 */, 17 },
  /* 3668 */  { MAD_F(0x06e7c1ae) /* 0.431581192 */, 17 },
  /* 3669 */  { MAD_F(0x06e86630) /* 0.431738080 */, 17 },
  /* 3670 */  { MAD_F(0x06e90ab7) /* 0.431894983 */, 17 },
  /* 3671 */  { MAD_F(0x06e9af41) /* 0.432051900 */, 17 },
  /* 3672 */  { MAD_F(0x06ea53cf) /* 0.432208832 */, 17 },
  /* 3673 */  { MAD_F(0x06eaf860) /* 0.432365778 */, 17 },
  /* 3674 */  { MAD_F(0x06eb9cf6) /* 0.432522737 */, 17 },
  /* 3675 */  { MAD_F(0x06ec418f) /* 0.432679712 */, 17 },
  /* 3676 */  { MAD_F(0x06ece62d) /* 0.432836700 */, 17 },
  /* 3677 */  { MAD_F(0x06ed8ace) /* 0.432993703 */, 17 },
  /* 3678 */  { MAD_F(0x06ee2f73) /* 0.433150720 */, 17 },
  /* 3679 */  { MAD_F(0x06eed41b) /* 0.433307751 */, 17 },

  /* 3680 */  { MAD_F(0x06ef78c8) /* 0.433464796 */, 17 },
  /* 3681 */  { MAD_F(0x06f01d78) /* 0.433621856 */, 17 },
  /* 3682 */  { MAD_F(0x06f0c22c) /* 0.433778929 */, 17 },
  /* 3683 */  { MAD_F(0x06f166e4) /* 0.433936017 */, 17 },
  /* 3684 */  { MAD_F(0x06f20ba0) /* 0.434093120 */, 17 },
  /* 3685 */  { MAD_F(0x06f2b060) /* 0.434250236 */, 17 },
  /* 3686 */  { MAD_F(0x06f35523) /* 0.434407367 */, 17 },
  /* 3687 */  { MAD_F(0x06f3f9eb) /* 0.434564512 */, 17 },
  /* 3688 */  { MAD_F(0x06f49eb6) /* 0.434721671 */, 17 },
  /* 3689 */  { MAD_F(0x06f54385) /* 0.434878844 */, 17 },
  /* 3690 */  { MAD_F(0x06f5e857) /* 0.435036032 */, 17 },
  /* 3691 */  { MAD_F(0x06f68d2e) /* 0.435193233 */, 17 },
  /* 3692 */  { MAD_F(0x06f73208) /* 0.435350449 */, 17 },
  /* 3693 */  { MAD_F(0x06f7d6e6) /* 0.435507679 */, 17 },
  /* 3694 */  { MAD_F(0x06f87bc8) /* 0.435664924 */, 17 },
  /* 3695 */  { MAD_F(0x06f920ae) /* 0.435822182 */, 17 },

  /* 3696 */  { MAD_F(0x06f9c597) /* 0.435979455 */, 17 },
  /* 3697 */  { MAD_F(0x06fa6a85) /* 0.436136741 */, 17 },
  /* 3698 */  { MAD_F(0x06fb0f76) /* 0.436294042 */, 17 },
  /* 3699 */  { MAD_F(0x06fbb46b) /* 0.436451358 */, 17 },
  /* 3700 */  { MAD_F(0x06fc5964) /* 0.436608687 */, 17 },
  /* 3701 */  { MAD_F(0x06fcfe60) /* 0.436766031 */, 17 },
  /* 3702 */  { MAD_F(0x06fda361) /* 0.436923388 */, 17 },
  /* 3703 */  { MAD_F(0x06fe4865) /* 0.437080760 */, 17 },
  /* 3704 */  { MAD_F(0x06feed6d) /* 0.437238146 */, 17 },
  /* 3705 */  { MAD_F(0x06ff9279) /* 0.437395547 */, 17 },
  /* 3706 */  { MAD_F(0x07003788) /* 0.437552961 */, 17 },
  /* 3707 */  { MAD_F(0x0700dc9c) /* 0.437710389 */, 17 },
  /* 3708 */  { MAD_F(0x070181b3) /* 0.437867832 */, 17 },
  /* 3709 */  { MAD_F(0x070226ce) /* 0.438025289 */, 17 },
  /* 3710 */  { MAD_F(0x0702cbed) /* 0.438182760 */, 17 },
  /* 3711 */  { MAD_F(0x0703710f) /* 0.438340245 */, 17 },

  /* 3712 */  { MAD_F(0x07041636) /* 0.438497744 */, 17 },
  /* 3713 */  { MAD_F(0x0704bb60) /* 0.438655258 */, 17 },
  /* 3714 */  { MAD_F(0x0705608e) /* 0.438812785 */, 17 },
  /* 3715 */  { MAD_F(0x070605c0) /* 0.438970327 */, 17 },
  /* 3716 */  { MAD_F(0x0706aaf5) /* 0.439127883 */, 17 },
  /* 3717 */  { MAD_F(0x0707502f) /* 0.439285453 */, 17 },
  /* 3718 */  { MAD_F(0x0707f56c) /* 0.439443037 */, 17 },
  /* 3719 */  { MAD_F(0x07089aad) /* 0.439600635 */, 17 },
  /* 3720 */  { MAD_F(0x07093ff2) /* 0.439758248 */, 17 },
  /* 3721 */  { MAD_F(0x0709e53a) /* 0.439915874 */, 17 },
  /* 3722 */  { MAD_F(0x070a8a86) /* 0.440073515 */, 17 },
  /* 3723 */  { MAD_F(0x070b2fd7) /* 0.440231170 */, 17 },
  /* 3724 */  { MAD_F(0x070bd52a) /* 0.440388839 */, 17 },
  /* 3725 */  { MAD_F(0x070c7a82) /* 0.440546522 */, 17 },
  /* 3726 */  { MAD_F(0x070d1fde) /* 0.440704219 */, 17 },
  /* 3727 */  { MAD_F(0x070dc53d) /* 0.440861930 */, 17 },

  /* 3728 */  { MAD_F(0x070e6aa0) /* 0.441019655 */, 17 },
  /* 3729 */  { MAD_F(0x070f1007) /* 0.441177395 */, 17 },
  /* 3730 */  { MAD_F(0x070fb571) /* 0.441335148 */, 17 },
  /* 3731 */  { MAD_F(0x07105ae0) /* 0.441492916 */, 17 },
  /* 3732 */  { MAD_F(0x07110052) /* 0.441650697 */, 17 },
  /* 3733 */  { MAD_F(0x0711a5c8) /* 0.441808493 */, 17 },
  /* 3734 */  { MAD_F(0x07124b42) /* 0.441966303 */, 17 },
  /* 3735 */  { MAD_F(0x0712f0bf) /* 0.442124127 */, 17 },
  /* 3736 */  { MAD_F(0x07139641) /* 0.442281965 */, 17 },
  /* 3737 */  { MAD_F(0x07143bc6) /* 0.442439817 */, 17 },
  /* 3738 */  { MAD_F(0x0714e14f) /* 0.442597683 */, 17 },
  /* 3739 */  { MAD_F(0x071586db) /* 0.442755564 */, 17 },
  /* 3740 */  { MAD_F(0x07162c6c) /* 0.442913458 */, 17 },
  /* 3741 */  { MAD_F(0x0716d200) /* 0.443071366 */, 17 },
  /* 3742 */  { MAD_F(0x07177798) /* 0.443229289 */, 17 },
  /* 3743 */  { MAD_F(0x07181d34) /* 0.443387226 */, 17 },

  /* 3744 */  { MAD_F(0x0718c2d3) /* 0.443545176 */, 17 },
  /* 3745 */  { MAD_F(0x07196877) /* 0.443703141 */, 17 },
  /* 3746 */  { MAD_F(0x071a0e1e) /* 0.443861120 */, 17 },
  /* 3747 */  { MAD_F(0x071ab3c9) /* 0.444019113 */, 17 },
  /* 3748 */  { MAD_F(0x071b5977) /* 0.444177119 */, 17 },
  /* 3749 */  { MAD_F(0x071bff2a) /* 0.444335140 */, 17 },
  /* 3750 */  { MAD_F(0x071ca4e0) /* 0.444493175 */, 17 },
  /* 3751 */  { MAD_F(0x071d4a9a) /* 0.444651224 */, 17 },
  /* 3752 */  { MAD_F(0x071df058) /* 0.444809288 */, 17 },
  /* 3753 */  { MAD_F(0x071e9619) /* 0.444967365 */, 17 },
  /* 3754 */  { MAD_F(0x071f3bde) /* 0.445125456 */, 17 },
  /* 3755 */  { MAD_F(0x071fe1a8) /* 0.445283561 */, 17 },
  /* 3756 */  { MAD_F(0x07208774) /* 0.445441680 */, 17 },
  /* 3757 */  { MAD_F(0x07212d45) /* 0.445599814 */, 17 },
  /* 3758 */  { MAD_F(0x0721d319) /* 0.445757961 */, 17 },
  /* 3759 */  { MAD_F(0x072278f1) /* 0.445916122 */, 17 },

  /* 3760 */  { MAD_F(0x07231ecd) /* 0.446074298 */, 17 },
  /* 3761 */  { MAD_F(0x0723c4ad) /* 0.446232487 */, 17 },
  /* 3762 */  { MAD_F(0x07246a90) /* 0.446390690 */, 17 },
  /* 3763 */  { MAD_F(0x07251077) /* 0.446548908 */, 17 },
  /* 3764 */  { MAD_F(0x0725b662) /* 0.446707139 */, 17 },
  /* 3765 */  { MAD_F(0x07265c51) /* 0.446865385 */, 17 },
  /* 3766 */  { MAD_F(0x07270244) /* 0.447023644 */, 17 },
  /* 3767 */  { MAD_F(0x0727a83a) /* 0.447181918 */, 17 },
  /* 3768 */  { MAD_F(0x07284e34) /* 0.447340205 */, 17 },
  /* 3769 */  { MAD_F(0x0728f431) /* 0.447498507 */, 17 },
  /* 3770 */  { MAD_F(0x07299a33) /* 0.447656822 */, 17 },
  /* 3771 */  { MAD_F(0x072a4038) /* 0.447815152 */, 17 },
  /* 3772 */  { MAD_F(0x072ae641) /* 0.447973495 */, 17 },
  /* 3773 */  { MAD_F(0x072b8c4e) /* 0.448131853 */, 17 },
  /* 3774 */  { MAD_F(0x072c325e) /* 0.448290224 */, 17 },
  /* 3775 */  { MAD_F(0x072cd873) /* 0.448448609 */, 17 },

  /* 3776 */  { MAD_F(0x072d7e8b) /* 0.448607009 */, 17 },
  /* 3777 */  { MAD_F(0x072e24a7) /* 0.448765422 */, 17 },
  /* 3778 */  { MAD_F(0x072ecac6) /* 0.448923850 */, 17 },
  /* 3779 */  { MAD_F(0x072f70e9) /* 0.449082291 */, 17 },
  /* 3780 */  { MAD_F(0x07301710) /* 0.449240746 */, 17 },
  /* 3781 */  { MAD_F(0x0730bd3b) /* 0.449399216 */, 17 },
  /* 3782 */  { MAD_F(0x0731636a) /* 0.449557699 */, 17 },
  /* 3783 */  { MAD_F(0x0732099c) /* 0.449716196 */, 17 },
  /* 3784 */  { MAD_F(0x0732afd2) /* 0.449874708 */, 17 },
  /* 3785 */  { MAD_F(0x0733560c) /* 0.450033233 */, 17 },
  /* 3786 */  { MAD_F(0x0733fc49) /* 0.450191772 */, 17 },
  /* 3787 */  { MAD_F(0x0734a28b) /* 0.450350325 */, 17 },
  /* 3788 */  { MAD_F(0x073548d0) /* 0.450508892 */, 17 },
  /* 3789 */  { MAD_F(0x0735ef18) /* 0.450667473 */, 17 },
  /* 3790 */  { MAD_F(0x07369565) /* 0.450826068 */, 17 },
  /* 3791 */  { MAD_F(0x07373bb5) /* 0.450984677 */, 17 },

  /* 3792 */  { MAD_F(0x0737e209) /* 0.451143300 */, 17 },
  /* 3793 */  { MAD_F(0x07388861) /* 0.451301937 */, 17 },
  /* 3794 */  { MAD_F(0x07392ebc) /* 0.451460588 */, 17 },
  /* 3795 */  { MAD_F(0x0739d51c) /* 0.451619252 */, 17 },
  /* 3796 */  { MAD_F(0x073a7b7f) /* 0.451777931 */, 17 },
  /* 3797 */  { MAD_F(0x073b21e5) /* 0.451936623 */, 17 },
  /* 3798 */  { MAD_F(0x073bc850) /* 0.452095330 */, 17 },
  /* 3799 */  { MAD_F(0x073c6ebe) /* 0.452254050 */, 17 },
  /* 3800 */  { MAD_F(0x073d1530) /* 0.452412785 */, 17 },
  /* 3801 */  { MAD_F(0x073dbba6) /* 0.452571533 */, 17 },
  /* 3802 */  { MAD_F(0x073e621f) /* 0.452730295 */, 17 },
  /* 3803 */  { MAD_F(0x073f089c) /* 0.452889071 */, 17 },
  /* 3804 */  { MAD_F(0x073faf1d) /* 0.453047861 */, 17 },
  /* 3805 */  { MAD_F(0x074055a2) /* 0.453206665 */, 17 },
  /* 3806 */  { MAD_F(0x0740fc2a) /* 0.453365483 */, 17 },
  /* 3807 */  { MAD_F(0x0741a2b6) /* 0.453524315 */, 17 },

  /* 3808 */  { MAD_F(0x07424946) /* 0.453683161 */, 17 },
  /* 3809 */  { MAD_F(0x0742efd9) /* 0.453842020 */, 17 },
  /* 3810 */  { MAD_F(0x07439671) /* 0.454000894 */, 17 },
  /* 3811 */  { MAD_F(0x07443d0c) /* 0.454159781 */, 17 },
  /* 3812 */  { MAD_F(0x0744e3aa) /* 0.454318683 */, 17 },
  /* 3813 */  { MAD_F(0x07458a4d) /* 0.454477598 */, 17 },
  /* 3814 */  { MAD_F(0x074630f3) /* 0.454636527 */, 17 },
  /* 3815 */  { MAD_F(0x0746d79d) /* 0.454795470 */, 17 },
  /* 3816 */  { MAD_F(0x07477e4b) /* 0.454954427 */, 17 },
  /* 3817 */  { MAD_F(0x074824fc) /* 0.455113397 */, 17 },
  /* 3818 */  { MAD_F(0x0748cbb1) /* 0.455272382 */, 17 },
  /* 3819 */  { MAD_F(0x0749726a) /* 0.455431381 */, 17 },
  /* 3820 */  { MAD_F(0x074a1927) /* 0.455590393 */, 17 },
  /* 3821 */  { MAD_F(0x074abfe7) /* 0.455749419 */, 17 },
  /* 3822 */  { MAD_F(0x074b66ab) /* 0.455908459 */, 17 },
  /* 3823 */  { MAD_F(0x074c0d73) /* 0.456067513 */, 17 },

  /* 3824 */  { MAD_F(0x074cb43e) /* 0.456226581 */, 17 },
  /* 3825 */  { MAD_F(0x074d5b0d) /* 0.456385663 */, 17 },
  /* 3826 */  { MAD_F(0x074e01e0) /* 0.456544759 */, 17 },
  /* 3827 */  { MAD_F(0x074ea8b7) /* 0.456703868 */, 17 },
  /* 3828 */  { MAD_F(0x074f4f91) /* 0.456862992 */, 17 },
  /* 3829 */  { MAD_F(0x074ff66f) /* 0.457022129 */, 17 },
  /* 3830 */  { MAD_F(0x07509d51) /* 0.457181280 */, 17 },
  /* 3831 */  { MAD_F(0x07514437) /* 0.457340445 */, 17 },
  /* 3832 */  { MAD_F(0x0751eb20) /* 0.457499623 */, 17 },
  /* 3833 */  { MAD_F(0x0752920d) /* 0.457658816 */, 17 },
  /* 3834 */  { MAD_F(0x075338fd) /* 0.457818022 */, 17 },
  /* 3835 */  { MAD_F(0x0753dff2) /* 0.457977243 */, 17 },
  /* 3836 */  { MAD_F(0x075486ea) /* 0.458136477 */, 17 },
  /* 3837 */  { MAD_F(0x07552de6) /* 0.458295725 */, 17 },
  /* 3838 */  { MAD_F(0x0755d4e5) /* 0.458454987 */, 17 },
  /* 3839 */  { MAD_F(0x07567be8) /* 0.458614262 */, 17 },

  /* 3840 */  { MAD_F(0x075722ef) /* 0.458773552 */, 17 },
  /* 3841 */  { MAD_F(0x0757c9fa) /* 0.458932855 */, 17 },
  /* 3842 */  { MAD_F(0x07587108) /* 0.459092172 */, 17 },
  /* 3843 */  { MAD_F(0x0759181a) /* 0.459251503 */, 17 },
  /* 3844 */  { MAD_F(0x0759bf30) /* 0.459410848 */, 17 },
  /* 3845 */  { MAD_F(0x075a664a) /* 0.459570206 */, 17 },
  /* 3846 */  { MAD_F(0x075b0d67) /* 0.459729579 */, 17 },
  /* 3847 */  { MAD_F(0x075bb488) /* 0.459888965 */, 17 },
  /* 3848 */  { MAD_F(0x075c5bac) /* 0.460048365 */, 17 },
  /* 3849 */  { MAD_F(0x075d02d5) /* 0.460207779 */, 17 },
  /* 3850 */  { MAD_F(0x075daa01) /* 0.460367206 */, 17 },
  /* 3851 */  { MAD_F(0x075e5130) /* 0.460526648 */, 17 },
  /* 3852 */  { MAD_F(0x075ef864) /* 0.460686103 */, 17 },
  /* 3853 */  { MAD_F(0x075f9f9b) /* 0.460845572 */, 17 },
  /* 3854 */  { MAD_F(0x076046d6) /* 0.461005055 */, 17 },
  /* 3855 */  { MAD_F(0x0760ee14) /* 0.461164552 */, 17 },

  /* 3856 */  { MAD_F(0x07619557) /* 0.461324062 */, 17 },
  /* 3857 */  { MAD_F(0x07623c9d) /* 0.461483586 */, 17 },
  /* 3858 */  { MAD_F(0x0762e3e6) /* 0.461643124 */, 17 },
  /* 3859 */  { MAD_F(0x07638b34) /* 0.461802676 */, 17 },
  /* 3860 */  { MAD_F(0x07643285) /* 0.461962242 */, 17 },
  /* 3861 */  { MAD_F(0x0764d9d9) /* 0.462121821 */, 17 },
  /* 3862 */  { MAD_F(0x07658132) /* 0.462281414 */, 17 },
  /* 3863 */  { MAD_F(0x0766288e) /* 0.462441021 */, 17 },
  /* 3864 */  { MAD_F(0x0766cfee) /* 0.462600642 */, 17 },
  /* 3865 */  { MAD_F(0x07677751) /* 0.462760276 */, 17 },
  /* 3866 */  { MAD_F(0x07681eb9) /* 0.462919924 */, 17 },
  /* 3867 */  { MAD_F(0x0768c624) /* 0.463079586 */, 17 },
  /* 3868 */  { MAD_F(0x07696d92) /* 0.463239262 */, 17 },
  /* 3869 */  { MAD_F(0x076a1505) /* 0.463398951 */, 17 },
  /* 3870 */  { MAD_F(0x076abc7b) /* 0.463558655 */, 17 },
  /* 3871 */  { MAD_F(0x076b63f4) /* 0.463718372 */, 17 },

  /* 3872 */  { MAD_F(0x076c0b72) /* 0.463878102 */, 17 },
  /* 3873 */  { MAD_F(0x076cb2f3) /* 0.464037847 */, 17 },
  /* 3874 */  { MAD_F(0x076d5a78) /* 0.464197605 */, 17 },
  /* 3875 */  { MAD_F(0x076e0200) /* 0.464357377 */, 17 },
  /* 3876 */  { MAD_F(0x076ea98c) /* 0.464517163 */, 17 },
  /* 3877 */  { MAD_F(0x076f511c) /* 0.464676962 */, 17 },
  /* 3878 */  { MAD_F(0x076ff8b0) /* 0.464836776 */, 17 },
  /* 3879 */  { MAD_F(0x0770a047) /* 0.464996603 */, 17 },
  /* 3880 */  { MAD_F(0x077147e2) /* 0.465156443 */, 17 },
  /* 3881 */  { MAD_F(0x0771ef80) /* 0.465316298 */, 17 },
  /* 3882 */  { MAD_F(0x07729723) /* 0.465476166 */, 17 },
  /* 3883 */  { MAD_F(0x07733ec9) /* 0.465636048 */, 17 },
  /* 3884 */  { MAD_F(0x0773e672) /* 0.465795943 */, 17 },
  /* 3885 */  { MAD_F(0x07748e20) /* 0.465955853 */, 17 },
  /* 3886 */  { MAD_F(0x077535d1) /* 0.466115776 */, 17 },
  /* 3887 */  { MAD_F(0x0775dd85) /* 0.466275713 */, 17 },

  /* 3888 */  { MAD_F(0x0776853e) /* 0.466435663 */, 17 },
  /* 3889 */  { MAD_F(0x07772cfa) /* 0.466595627 */, 17 },
  /* 3890 */  { MAD_F(0x0777d4ba) /* 0.466755605 */, 17 },
  /* 3891 */  { MAD_F(0x07787c7d) /* 0.466915597 */, 17 },
  /* 3892 */  { MAD_F(0x07792444) /* 0.467075602 */, 17 },
  /* 3893 */  { MAD_F(0x0779cc0f) /* 0.467235621 */, 17 },
  /* 3894 */  { MAD_F(0x077a73dd) /* 0.467395654 */, 17 },
  /* 3895 */  { MAD_F(0x077b1baf) /* 0.467555701 */, 17 },
  /* 3896 */  { MAD_F(0x077bc385) /* 0.467715761 */, 17 },
  /* 3897 */  { MAD_F(0x077c6b5f) /* 0.467875835 */, 17 },
  /* 3898 */  { MAD_F(0x077d133c) /* 0.468035922 */, 17 },
  /* 3899 */  { MAD_F(0x077dbb1d) /* 0.468196023 */, 17 },
  /* 3900 */  { MAD_F(0x077e6301) /* 0.468356138 */, 17 },
  /* 3901 */  { MAD_F(0x077f0ae9) /* 0.468516267 */, 17 },
  /* 3902 */  { MAD_F(0x077fb2d5) /* 0.468676409 */, 17 },
  /* 3903 */  { MAD_F(0x07805ac5) /* 0.468836565 */, 17 },

  /* 3904 */  { MAD_F(0x078102b8) /* 0.468996735 */, 17 },
  /* 3905 */  { MAD_F(0x0781aaaf) /* 0.469156918 */, 17 },
  /* 3906 */  { MAD_F(0x078252aa) /* 0.469317115 */, 17 },
  /* 3907 */  { MAD_F(0x0782faa8) /* 0.469477326 */, 17 },
  /* 3908 */  { MAD_F(0x0783a2aa) /* 0.469637550 */, 17 },
  /* 3909 */  { MAD_F(0x07844aaf) /* 0.469797788 */, 17 },
  /* 3910 */  { MAD_F(0x0784f2b8) /* 0.469958040 */, 17 },
  /* 3911 */  { MAD_F(0x07859ac5) /* 0.470118305 */, 17 },
  /* 3912 */  { MAD_F(0x078642d6) /* 0.470278584 */, 17 },
  /* 3913 */  { MAD_F(0x0786eaea) /* 0.470438877 */, 17 },
  /* 3914 */  { MAD_F(0x07879302) /* 0.470599183 */, 17 },
  /* 3915 */  { MAD_F(0x07883b1e) /* 0.470759503 */, 17 },
  /* 3916 */  { MAD_F(0x0788e33d) /* 0.470919836 */, 17 },
  /* 3917 */  { MAD_F(0x07898b60) /* 0.471080184 */, 17 },
  /* 3918 */  { MAD_F(0x078a3386) /* 0.471240545 */, 17 },
  /* 3919 */  { MAD_F(0x078adbb0) /* 0.471400919 */, 17 },

  /* 3920 */  { MAD_F(0x078b83de) /* 0.471561307 */, 17 },
  /* 3921 */  { MAD_F(0x078c2c10) /* 0.471721709 */, 17 },
  /* 3922 */  { MAD_F(0x078cd445) /* 0.471882125 */, 17 },
  /* 3923 */  { MAD_F(0x078d7c7e) /* 0.472042554 */, 17 },
  /* 3924 */  { MAD_F(0x078e24ba) /* 0.472202996 */, 17 },
  /* 3925 */  { MAD_F(0x078eccfb) /* 0.472363453 */, 17 },
  /* 3926 */  { MAD_F(0x078f753e) /* 0.472523923 */, 17 },
  /* 3927 */  { MAD_F(0x07901d86) /* 0.472684406 */, 17 },
  /* 3928 */  { MAD_F(0x0790c5d1) /* 0.472844904 */, 17 },
  /* 3929 */  { MAD_F(0x07916e20) /* 0.473005414 */, 17 },
  /* 3930 */  { MAD_F(0x07921672) /* 0.473165939 */, 17 },
  /* 3931 */  { MAD_F(0x0792bec8) /* 0.473326477 */, 17 },
  /* 3932 */  { MAD_F(0x07936722) /* 0.473487029 */, 17 },
  /* 3933 */  { MAD_F(0x07940f80) /* 0.473647594 */, 17 },
  /* 3934 */  { MAD_F(0x0794b7e1) /* 0.473808173 */, 17 },
  /* 3935 */  { MAD_F(0x07956045) /* 0.473968765 */, 17 },

  /* 3936 */  { MAD_F(0x079608ae) /* 0.474129372 */, 17 },
  /* 3937 */  { MAD_F(0x0796b11a) /* 0.474289991 */, 17 },
  /* 3938 */  { MAD_F(0x0797598a) /* 0.474450625 */, 17 },
  /* 3939 */  { MAD_F(0x079801fd) /* 0.474611272 */, 17 },
  /* 3940 */  { MAD_F(0x0798aa74) /* 0.474771932 */, 17 },
  /* 3941 */  { MAD_F(0x079952ee) /* 0.474932606 */, 17 },
  /* 3942 */  { MAD_F(0x0799fb6d) /* 0.475093294 */, 17 },
  /* 3943 */  { MAD_F(0x079aa3ef) /* 0.475253995 */, 17 },
  /* 3944 */  { MAD_F(0x079b4c74) /* 0.475414710 */, 17 },
  /* 3945 */  { MAD_F(0x079bf4fd) /* 0.475575439 */, 17 },
  /* 3946 */  { MAD_F(0x079c9d8a) /* 0.475736181 */, 17 },
  /* 3947 */  { MAD_F(0x079d461b) /* 0.475896936 */, 17 },
  /* 3948 */  { MAD_F(0x079deeaf) /* 0.476057705 */, 17 },
  /* 3949 */  { MAD_F(0x079e9747) /* 0.476218488 */, 17 },
  /* 3950 */  { MAD_F(0x079f3fe2) /* 0.476379285 */, 17 },
  /* 3951 */  { MAD_F(0x079fe881) /* 0.476540095 */, 17 },

  /* 3952 */  { MAD_F(0x07a09124) /* 0.476700918 */, 17 },
  /* 3953 */  { MAD_F(0x07a139ca) /* 0.476861755 */, 17 },
  /* 3954 */  { MAD_F(0x07a1e274) /* 0.477022606 */, 17 },
  /* 3955 */  { MAD_F(0x07a28b22) /* 0.477183470 */, 17 },
  /* 3956 */  { MAD_F(0x07a333d3) /* 0.477344348 */, 17 },
  /* 3957 */  { MAD_F(0x07a3dc88) /* 0.477505239 */, 17 },
  /* 3958 */  { MAD_F(0x07a48541) /* 0.477666144 */, 17 },
  /* 3959 */  { MAD_F(0x07a52dfd) /* 0.477827062 */, 17 },
  /* 3960 */  { MAD_F(0x07a5d6bd) /* 0.477987994 */, 17 },
  /* 3961 */  { MAD_F(0x07a67f80) /* 0.478148940 */, 17 },
  /* 3962 */  { MAD_F(0x07a72847) /* 0.478309899 */, 17 },
  /* 3963 */  { MAD_F(0x07a7d112) /* 0.478470871 */, 17 },
  /* 3964 */  { MAD_F(0x07a879e1) /* 0.478631857 */, 17 },
  /* 3965 */  { MAD_F(0x07a922b3) /* 0.478792857 */, 17 },
  /* 3966 */  { MAD_F(0x07a9cb88) /* 0.478953870 */, 17 },
  /* 3967 */  { MAD_F(0x07aa7462) /* 0.479114897 */, 17 },

  /* 3968 */  { MAD_F(0x07ab1d3e) /* 0.479275937 */, 17 },
  /* 3969 */  { MAD_F(0x07abc61f) /* 0.479436991 */, 17 },
  /* 3970 */  { MAD_F(0x07ac6f03) /* 0.479598058 */, 17 },
  /* 3971 */  { MAD_F(0x07ad17eb) /* 0.479759139 */, 17 },
  /* 3972 */  { MAD_F(0x07adc0d6) /* 0.479920233 */, 17 },
  /* 3973 */  { MAD_F(0x07ae69c6) /* 0.480081341 */, 17 },
  /* 3974 */  { MAD_F(0x07af12b8) /* 0.480242463 */, 17 },
  /* 3975 */  { MAD_F(0x07afbbaf) /* 0.480403598 */, 17 },
  /* 3976 */  { MAD_F(0x07b064a8) /* 0.480564746 */, 17 },
  /* 3977 */  { MAD_F(0x07b10da6) /* 0.480725908 */, 17 },
  /* 3978 */  { MAD_F(0x07b1b6a7) /* 0.480887083 */, 17 },
  /* 3979 */  { MAD_F(0x07b25fac) /* 0.481048272 */, 17 },
  /* 3980 */  { MAD_F(0x07b308b5) /* 0.481209475 */, 17 },
  /* 3981 */  { MAD_F(0x07b3b1c1) /* 0.481370691 */, 17 },
  /* 3982 */  { MAD_F(0x07b45ad0) /* 0.481531920 */, 17 },
  /* 3983 */  { MAD_F(0x07b503e4) /* 0.481693163 */, 17 },

  /* 3984 */  { MAD_F(0x07b5acfb) /* 0.481854420 */, 17 },
  /* 3985 */  { MAD_F(0x07b65615) /* 0.482015690 */, 17 },
  /* 3986 */  { MAD_F(0x07b6ff33) /* 0.482176973 */, 17 },
  /* 3987 */  { MAD_F(0x07b7a855) /* 0.482338270 */, 17 },
  /* 3988 */  { MAD_F(0x07b8517b) /* 0.482499580 */, 17 },
  /* 3989 */  { MAD_F(0x07b8faa4) /* 0.482660904 */, 17 },
  /* 3990 */  { MAD_F(0x07b9a3d0) /* 0.482822242 */, 17 },
  /* 3991 */  { MAD_F(0x07ba4d01) /* 0.482983592 */, 17 },
  /* 3992 */  { MAD_F(0x07baf635) /* 0.483144957 */, 17 },
  /* 3993 */  { MAD_F(0x07bb9f6c) /* 0.483306335 */, 17 },
  /* 3994 */  { MAD_F(0x07bc48a7) /* 0.483467726 */, 17 },
  /* 3995 */  { MAD_F(0x07bcf1e6) /* 0.483629131 */, 17 },
  /* 3996 */  { MAD_F(0x07bd9b28) /* 0.483790549 */, 17 },
  /* 3997 */  { MAD_F(0x07be446e) /* 0.483951980 */, 17 },
  /* 3998 */  { MAD_F(0x07beedb8) /* 0.484113426 */, 17 },
  /* 3999 */  { MAD_F(0x07bf9705) /* 0.484274884 */, 17 },

  /* 4000 */  { MAD_F(0x07c04056) /* 0.484436356 */, 17 },
  /* 4001 */  { MAD_F(0x07c0e9aa) /* 0.484597842 */, 17 },
  /* 4002 */  { MAD_F(0x07c19302) /* 0.484759341 */, 17 },
  /* 4003 */  { MAD_F(0x07c23c5e) /* 0.484920853 */, 17 },
  /* 4004 */  { MAD_F(0x07c2e5bd) /* 0.485082379 */, 17 },
  /* 4005 */  { MAD_F(0x07c38f20) /* 0.485243918 */, 17 },
  /* 4006 */  { MAD_F(0x07c43887) /* 0.485405471 */, 17 },
  /* 4007 */  { MAD_F(0x07c4e1f1) /* 0.485567037 */, 17 },
  /* 4008 */  { MAD_F(0x07c58b5f) /* 0.485728617 */, 17 },
  /* 4009 */  { MAD_F(0x07c634d0) /* 0.485890210 */, 17 },
  /* 4010 */  { MAD_F(0x07c6de45) /* 0.486051817 */, 17 },
  /* 4011 */  { MAD_F(0x07c787bd) /* 0.486213436 */, 17 },
  /* 4012 */  { MAD_F(0x07c83139) /* 0.486375070 */, 17 },
  /* 4013 */  { MAD_F(0x07c8dab9) /* 0.486536717 */, 17 },
  /* 4014 */  { MAD_F(0x07c9843c) /* 0.486698377 */, 17 },
  /* 4015 */  { MAD_F(0x07ca2dc3) /* 0.486860051 */, 17 },

  /* 4016 */  { MAD_F(0x07cad74e) /* 0.487021738 */, 17 },
  /* 4017 */  { MAD_F(0x07cb80dc) /* 0.487183438 */, 17 },
  /* 4018 */  { MAD_F(0x07cc2a6e) /* 0.487345152 */, 17 },
  /* 4019 */  { MAD_F(0x07ccd403) /* 0.487506879 */, 17 },
  /* 4020 */  { MAD_F(0x07cd7d9c) /* 0.487668620 */, 17 },
  /* 4021 */  { MAD_F(0x07ce2739) /* 0.487830374 */, 17 },
  /* 4022 */  { MAD_F(0x07ced0d9) /* 0.487992142 */, 17 },
  /* 4023 */  { MAD_F(0x07cf7a7d) /* 0.488153923 */, 17 },
  /* 4024 */  { MAD_F(0x07d02424) /* 0.488315717 */, 17 },
  /* 4025 */  { MAD_F(0x07d0cdcf) /* 0.488477525 */, 17 },
  /* 4026 */  { MAD_F(0x07d1777e) /* 0.488639346 */, 17 },
  /* 4027 */  { MAD_F(0x07d22130) /* 0.488801181 */, 17 },
  /* 4028 */  { MAD_F(0x07d2cae5) /* 0.488963029 */, 17 },
  /* 4029 */  { MAD_F(0x07d3749f) /* 0.489124890 */, 17 },
  /* 4030 */  { MAD_F(0x07d41e5c) /* 0.489286765 */, 17 },
  /* 4031 */  { MAD_F(0x07d4c81c) /* 0.489448653 */, 17 },

  /* 4032 */  { MAD_F(0x07d571e0) /* 0.489610555 */, 17 },
  /* 4033 */  { MAD_F(0x07d61ba8) /* 0.489772470 */, 17 },
  /* 4034 */  { MAD_F(0x07d6c573) /* 0.489934398 */, 17 },
  /* 4035 */  { MAD_F(0x07d76f42) /* 0.490096340 */, 17 },
  /* 4036 */  { MAD_F(0x07d81915) /* 0.490258295 */, 17 },
  /* 4037 */  { MAD_F(0x07d8c2eb) /* 0.490420263 */, 17 },
  /* 4038 */  { MAD_F(0x07d96cc4) /* 0.490582245 */, 17 },
  /* 4039 */  { MAD_F(0x07da16a2) /* 0.490744240 */, 17 },
  /* 4040 */  { MAD_F(0x07dac083) /* 0.490906249 */, 17 },
  /* 4041 */  { MAD_F(0x07db6a67) /* 0.491068271 */, 17 },
  /* 4042 */  { MAD_F(0x07dc144f) /* 0.491230306 */, 17 },
  /* 4043 */  { MAD_F(0x07dcbe3b) /* 0.491392355 */, 17 },
  /* 4044 */  { MAD_F(0x07dd682a) /* 0.491554417 */, 17 },
  /* 4045 */  { MAD_F(0x07de121d) /* 0.491716492 */, 17 },
  /* 4046 */  { MAD_F(0x07debc13) /* 0.491878581 */, 17 },
  /* 4047 */  { MAD_F(0x07df660d) /* 0.492040683 */, 17 },

  /* 4048 */  { MAD_F(0x07e0100a) /* 0.492202799 */, 17 },
  /* 4049 */  { MAD_F(0x07e0ba0c) /* 0.492364928 */, 17 },
  /* 4050 */  { MAD_F(0x07e16410) /* 0.492527070 */, 17 },
  /* 4051 */  { MAD_F(0x07e20e19) /* 0.492689225 */, 17 },
  /* 4052 */  { MAD_F(0x07e2b824) /* 0.492851394 */, 17 },
  /* 4053 */  { MAD_F(0x07e36234) /* 0.493013576 */, 17 },
  /* 4054 */  { MAD_F(0x07e40c47) /* 0.493175772 */, 17 },
  /* 4055 */  { MAD_F(0x07e4b65e) /* 0.493337981 */, 17 },
  /* 4056 */  { MAD_F(0x07e56078) /* 0.493500203 */, 17 },
  /* 4057 */  { MAD_F(0x07e60a95) /* 0.493662438 */, 17 },
  /* 4058 */  { MAD_F(0x07e6b4b7) /* 0.493824687 */, 17 },
  /* 4059 */  { MAD_F(0x07e75edc) /* 0.493986949 */, 17 },
  /* 4060 */  { MAD_F(0x07e80904) /* 0.494149225 */, 17 },
  /* 4061 */  { MAD_F(0x07e8b330) /* 0.494311514 */, 17 },
  /* 4062 */  { MAD_F(0x07e95d60) /* 0.494473816 */, 17 },
  /* 4063 */  { MAD_F(0x07ea0793) /* 0.494636131 */, 17 },

  /* 4064 */  { MAD_F(0x07eab1ca) /* 0.494798460 */, 17 },
  /* 4065 */  { MAD_F(0x07eb5c04) /* 0.494960802 */, 17 },
  /* 4066 */  { MAD_F(0x07ec0642) /* 0.495123158 */, 17 },
  /* 4067 */  { MAD_F(0x07ecb084) /* 0.495285526 */, 17 },
  /* 4068 */  { MAD_F(0x07ed5ac9) /* 0.495447908 */, 17 },
  /* 4069 */  { MAD_F(0x07ee0512) /* 0.495610304 */, 17 },
  /* 4070 */  { MAD_F(0x07eeaf5e) /* 0.495772712 */, 17 },
  /* 4071 */  { MAD_F(0x07ef59ae) /* 0.495935134 */, 17 },
  /* 4072 */  { MAD_F(0x07f00401) /* 0.496097570 */, 17 },
  /* 4073 */  { MAD_F(0x07f0ae58) /* 0.496260018 */, 17 },
  /* 4074 */  { MAD_F(0x07f158b3) /* 0.496422480 */, 17 },
  /* 4075 */  { MAD_F(0x07f20311) /* 0.496584955 */, 17 },
  /* 4076 */  { MAD_F(0x07f2ad72) /* 0.496747444 */, 17 },
  /* 4077 */  { MAD_F(0x07f357d8) /* 0.496909945 */, 17 },
  /* 4078 */  { MAD_F(0x07f40240) /* 0.497072460 */, 17 },
  /* 4079 */  { MAD_F(0x07f4acad) /* 0.497234989 */, 17 },

  /* 4080 */  { MAD_F(0x07f5571d) /* 0.497397530 */, 17 },
  /* 4081 */  { MAD_F(0x07f60190) /* 0.497560085 */, 17 },
  /* 4082 */  { MAD_F(0x07f6ac07) /* 0.497722653 */, 17 },
  /* 4083 */  { MAD_F(0x07f75682) /* 0.497885235 */, 17 },
  /* 4084 */  { MAD_F(0x07f80100) /* 0.498047829 */, 17 },
  /* 4085 */  { MAD_F(0x07f8ab82) /* 0.498210437 */, 17 },
  /* 4086 */  { MAD_F(0x07f95607) /* 0.498373058 */, 17 },
  /* 4087 */  { MAD_F(0x07fa0090) /* 0.498535693 */, 17 },
  /* 4088 */  { MAD_F(0x07faab1c) /* 0.498698341 */, 17 },
  /* 4089 */  { MAD_F(0x07fb55ac) /* 0.498861002 */, 17 },
  /* 4090 */  { MAD_F(0x07fc0040) /* 0.499023676 */, 17 },
  /* 4091 */  { MAD_F(0x07fcaad7) /* 0.499186364 */, 17 },
  /* 4092 */  { MAD_F(0x07fd5572) /* 0.499349064 */, 17 },
  /* 4093 */  { MAD_F(0x07fe0010) /* 0.499511778 */, 17 },
  /* 4094 */  { MAD_F(0x07feaab2) /* 0.499674506 */, 17 },
  /* 4095 */  { MAD_F(0x07ff5557) /* 0.499837246 */, 17 },

  /* 4096 */  { MAD_F(0x04000000) /* 0.250000000 */, 18 },
  /* 4097 */  { MAD_F(0x04005556) /* 0.250081384 */, 18 },
  /* 4098 */  { MAD_F(0x0400aaae) /* 0.250162774 */, 18 },
  /* 4099 */  { MAD_F(0x04010008) /* 0.250244170 */, 18 },
  /* 4100 */  { MAD_F(0x04015563) /* 0.250325574 */, 18 },
  /* 4101 */  { MAD_F(0x0401aac1) /* 0.250406984 */, 18 },
  /* 4102 */  { MAD_F(0x04020020) /* 0.250488400 */, 18 },
  /* 4103 */  { MAD_F(0x04025581) /* 0.250569824 */, 18 },
  /* 4104 */  { MAD_F(0x0402aae3) /* 0.250651254 */, 18 },
  /* 4105 */  { MAD_F(0x04030048) /* 0.250732690 */, 18 },
  /* 4106 */  { MAD_F(0x040355ae) /* 0.250814133 */, 18 },
  /* 4107 */  { MAD_F(0x0403ab16) /* 0.250895583 */, 18 },
  /* 4108 */  { MAD_F(0x04040080) /* 0.250977039 */, 18 },
  /* 4109 */  { MAD_F(0x040455eb) /* 0.251058502 */, 18 },
  /* 4110 */  { MAD_F(0x0404ab59) /* 0.251139971 */, 18 },
  /* 4111 */  { MAD_F(0x040500c8) /* 0.251221448 */, 18 },

  /* 4112 */  { MAD_F(0x04055638) /* 0.251302930 */, 18 },
  /* 4113 */  { MAD_F(0x0405abab) /* 0.251384420 */, 18 },
  /* 4114 */  { MAD_F(0x0406011f) /* 0.251465916 */, 18 },
  /* 4115 */  { MAD_F(0x04065696) /* 0.251547418 */, 18 },
  /* 4116 */  { MAD_F(0x0406ac0e) /* 0.251628927 */, 18 },
  /* 4117 */  { MAD_F(0x04070187) /* 0.251710443 */, 18 },
  /* 4118 */  { MAD_F(0x04075703) /* 0.251791965 */, 18 },
  /* 4119 */  { MAD_F(0x0407ac80) /* 0.251873494 */, 18 },
  /* 4120 */  { MAD_F(0x040801ff) /* 0.251955030 */, 18 },
  /* 4121 */  { MAD_F(0x04085780) /* 0.252036572 */, 18 },
  /* 4122 */  { MAD_F(0x0408ad02) /* 0.252118121 */, 18 },
  /* 4123 */  { MAD_F(0x04090287) /* 0.252199676 */, 18 },
  /* 4124 */  { MAD_F(0x0409580d) /* 0.252281238 */, 18 },
  /* 4125 */  { MAD_F(0x0409ad95) /* 0.252362807 */, 18 },
  /* 4126 */  { MAD_F(0x040a031e) /* 0.252444382 */, 18 },
  /* 4127 */  { MAD_F(0x040a58aa) /* 0.252525963 */, 18 },

  /* 4128 */  { MAD_F(0x040aae37) /* 0.252607552 */, 18 },
  /* 4129 */  { MAD_F(0x040b03c6) /* 0.252689147 */, 18 },
  /* 4130 */  { MAD_F(0x040b5957) /* 0.252770748 */, 18 },
  /* 4131 */  { MAD_F(0x040baee9) /* 0.252852356 */, 18 },
  /* 4132 */  { MAD_F(0x040c047e) /* 0.252933971 */, 18 },
  /* 4133 */  { MAD_F(0x040c5a14) /* 0.253015592 */, 18 },
  /* 4134 */  { MAD_F(0x040cafab) /* 0.253097220 */, 18 },
  /* 4135 */  { MAD_F(0x040d0545) /* 0.253178854 */, 18 },
  /* 4136 */  { MAD_F(0x040d5ae0) /* 0.253260495 */, 18 },
  /* 4137 */  { MAD_F(0x040db07d) /* 0.253342143 */, 18 },
  /* 4138 */  { MAD_F(0x040e061c) /* 0.253423797 */, 18 },
  /* 4139 */  { MAD_F(0x040e5bbd) /* 0.253505457 */, 18 },
  /* 4140 */  { MAD_F(0x040eb15f) /* 0.253587125 */, 18 },
  /* 4141 */  { MAD_F(0x040f0703) /* 0.253668799 */, 18 },
  /* 4142 */  { MAD_F(0x040f5ca9) /* 0.253750479 */, 18 },
  /* 4143 */  { MAD_F(0x040fb251) /* 0.253832166 */, 18 },

  /* 4144 */  { MAD_F(0x041007fa) /* 0.253913860 */, 18 },
  /* 4145 */  { MAD_F(0x04105da6) /* 0.253995560 */, 18 },
  /* 4146 */  { MAD_F(0x0410b353) /* 0.254077266 */, 18 },
  /* 4147 */  { MAD_F(0x04110901) /* 0.254158980 */, 18 },
  /* 4148 */  { MAD_F(0x04115eb2) /* 0.254240700 */, 18 },
  /* 4149 */  { MAD_F(0x0411b464) /* 0.254322426 */, 18 },
  /* 4150 */  { MAD_F(0x04120a18) /* 0.254404159 */, 18 },
  /* 4151 */  { MAD_F(0x04125fce) /* 0.254485899 */, 18 },
  /* 4152 */  { MAD_F(0x0412b586) /* 0.254567645 */, 18 },
  /* 4153 */  { MAD_F(0x04130b3f) /* 0.254649397 */, 18 },
  /* 4154 */  { MAD_F(0x041360fa) /* 0.254731157 */, 18 },
  /* 4155 */  { MAD_F(0x0413b6b7) /* 0.254812922 */, 18 },
  /* 4156 */  { MAD_F(0x04140c75) /* 0.254894695 */, 18 },
  /* 4157 */  { MAD_F(0x04146236) /* 0.254976474 */, 18 },
  /* 4158 */  { MAD_F(0x0414b7f8) /* 0.255058259 */, 18 },
  /* 4159 */  { MAD_F(0x04150dbc) /* 0.255140051 */, 18 },

  /* 4160 */  { MAD_F(0x04156381) /* 0.255221850 */, 18 },
  /* 4161 */  { MAD_F(0x0415b949) /* 0.255303655 */, 18 },
  /* 4162 */  { MAD_F(0x04160f12) /* 0.255385467 */, 18 },
  /* 4163 */  { MAD_F(0x041664dd) /* 0.255467285 */, 18 },
  /* 4164 */  { MAD_F(0x0416baaa) /* 0.255549110 */, 18 },
  /* 4165 */  { MAD_F(0x04171078) /* 0.255630941 */, 18 },
  /* 4166 */  { MAD_F(0x04176648) /* 0.255712779 */, 18 },
  /* 4167 */  { MAD_F(0x0417bc1a) /* 0.255794624 */, 18 },
  /* 4168 */  { MAD_F(0x041811ee) /* 0.255876475 */, 18 },
  /* 4169 */  { MAD_F(0x041867c3) /* 0.255958332 */, 18 },
  /* 4170 */  { MAD_F(0x0418bd9b) /* 0.256040196 */, 18 },
  /* 4171 */  { MAD_F(0x04191374) /* 0.256122067 */, 18 },
  /* 4172 */  { MAD_F(0x0419694e) /* 0.256203944 */, 18 },
  /* 4173 */  { MAD_F(0x0419bf2b) /* 0.256285828 */, 18 },
  /* 4174 */  { MAD_F(0x041a1509) /* 0.256367718 */, 18 },
  /* 4175 */  { MAD_F(0x041a6ae9) /* 0.256449615 */, 18 },

  /* 4176 */  { MAD_F(0x041ac0cb) /* 0.256531518 */, 18 },
  /* 4177 */  { MAD_F(0x041b16ae) /* 0.256613428 */, 18 },
  /* 4178 */  { MAD_F(0x041b6c94) /* 0.256695344 */, 18 },
  /* 4179 */  { MAD_F(0x041bc27b) /* 0.256777267 */, 18 },
  /* 4180 */  { MAD_F(0x041c1863) /* 0.256859197 */, 18 },
  /* 4181 */  { MAD_F(0x041c6e4e) /* 0.256941133 */, 18 },
  /* 4182 */  { MAD_F(0x041cc43a) /* 0.257023076 */, 18 },
  /* 4183 */  { MAD_F(0x041d1a28) /* 0.257105025 */, 18 },
  /* 4184 */  { MAD_F(0x041d7018) /* 0.257186980 */, 18 },
  /* 4185 */  { MAD_F(0x041dc60a) /* 0.257268942 */, 18 },
  /* 4186 */  { MAD_F(0x041e1bfd) /* 0.257350911 */, 18 },
  /* 4187 */  { MAD_F(0x041e71f2) /* 0.257432886 */, 18 },
  /* 4188 */  { MAD_F(0x041ec7e9) /* 0.257514868 */, 18 },
  /* 4189 */  { MAD_F(0x041f1de1) /* 0.257596856 */, 18 },
  /* 4190 */  { MAD_F(0x041f73dc) /* 0.257678851 */, 18 },
  /* 4191 */  { MAD_F(0x041fc9d8) /* 0.257760852 */, 18 },

  /* 4192 */  { MAD_F(0x04201fd5) /* 0.257842860 */, 18 },
  /* 4193 */  { MAD_F(0x042075d5) /* 0.257924875 */, 18 },
  /* 4194 */  { MAD_F(0x0420cbd6) /* 0.258006895 */, 18 },
  /* 4195 */  { MAD_F(0x042121d9) /* 0.258088923 */, 18 },
  /* 4196 */  { MAD_F(0x042177de) /* 0.258170957 */, 18 },
  /* 4197 */  { MAD_F(0x0421cde5) /* 0.258252997 */, 18 },
  /* 4198 */  { MAD_F(0x042223ed) /* 0.258335044 */, 18 },
  /* 4199 */  { MAD_F(0x042279f7) /* 0.258417097 */, 18 },
  /* 4200 */  { MAD_F(0x0422d003) /* 0.258499157 */, 18 },
  /* 4201 */  { MAD_F(0x04232611) /* 0.258581224 */, 18 },
  /* 4202 */  { MAD_F(0x04237c20) /* 0.258663297 */, 18 },
  /* 4203 */  { MAD_F(0x0423d231) /* 0.258745376 */, 18 },
  /* 4204 */  { MAD_F(0x04242844) /* 0.258827462 */, 18 },
  /* 4205 */  { MAD_F(0x04247e58) /* 0.258909555 */, 18 },
  /* 4206 */  { MAD_F(0x0424d46e) /* 0.258991654 */, 18 },
  /* 4207 */  { MAD_F(0x04252a87) /* 0.259073760 */, 18 },

  /* 4208 */  { MAD_F(0x042580a0) /* 0.259155872 */, 18 },
  /* 4209 */  { MAD_F(0x0425d6bc) /* 0.259237990 */, 18 },
  /* 4210 */  { MAD_F(0x04262cd9) /* 0.259320115 */, 18 },
  /* 4211 */  { MAD_F(0x042682f8) /* 0.259402247 */, 18 },
  /* 4212 */  { MAD_F(0x0426d919) /* 0.259484385 */, 18 },
  /* 4213 */  { MAD_F(0x04272f3b) /* 0.259566529 */, 18 },
  /* 4214 */  { MAD_F(0x04278560) /* 0.259648680 */, 18 },
  /* 4215 */  { MAD_F(0x0427db86) /* 0.259730838 */, 18 },
  /* 4216 */  { MAD_F(0x042831ad) /* 0.259813002 */, 18 },
  /* 4217 */  { MAD_F(0x042887d7) /* 0.259895173 */, 18 },
  /* 4218 */  { MAD_F(0x0428de02) /* 0.259977350 */, 18 },
  /* 4219 */  { MAD_F(0x0429342f) /* 0.260059533 */, 18 },
  /* 4220 */  { MAD_F(0x04298a5e) /* 0.260141723 */, 18 },
  /* 4221 */  { MAD_F(0x0429e08e) /* 0.260223920 */, 18 },
  /* 4222 */  { MAD_F(0x042a36c0) /* 0.260306123 */, 18 },
  /* 4223 */  { MAD_F(0x042a8cf4) /* 0.260388332 */, 18 },

  /* 4224 */  { MAD_F(0x042ae32a) /* 0.260470548 */, 18 },
  /* 4225 */  { MAD_F(0x042b3962) /* 0.260552771 */, 18 },
  /* 4226 */  { MAD_F(0x042b8f9b) /* 0.260635000 */, 18 },
  /* 4227 */  { MAD_F(0x042be5d6) /* 0.260717235 */, 18 },
  /* 4228 */  { MAD_F(0x042c3c12) /* 0.260799477 */, 18 },
  /* 4229 */  { MAD_F(0x042c9251) /* 0.260881725 */, 18 },
  /* 4230 */  { MAD_F(0x042ce891) /* 0.260963980 */, 18 },
  /* 4231 */  { MAD_F(0x042d3ed3) /* 0.261046242 */, 18 },
  /* 4232 */  { MAD_F(0x042d9516) /* 0.261128510 */, 18 },
  /* 4233 */  { MAD_F(0x042deb5c) /* 0.261210784 */, 18 },
  /* 4234 */  { MAD_F(0x042e41a3) /* 0.261293065 */, 18 },
  /* 4235 */  { MAD_F(0x042e97ec) /* 0.261375352 */, 18 },
  /* 4236 */  { MAD_F(0x042eee36) /* 0.261457646 */, 18 },
  /* 4237 */  { MAD_F(0x042f4482) /* 0.261539946 */, 18 },
  /* 4238 */  { MAD_F(0x042f9ad1) /* 0.261622253 */, 18 },
  /* 4239 */  { MAD_F(0x042ff120) /* 0.261704566 */, 18 },

  /* 4240 */  { MAD_F(0x04304772) /* 0.261786886 */, 18 },
  /* 4241 */  { MAD_F(0x04309dc5) /* 0.261869212 */, 18 },
  /* 4242 */  { MAD_F(0x0430f41a) /* 0.261951545 */, 18 },
  /* 4243 */  { MAD_F(0x04314a71) /* 0.262033884 */, 18 },
  /* 4244 */  { MAD_F(0x0431a0c9) /* 0.262116229 */, 18 },
  /* 4245 */  { MAD_F(0x0431f723) /* 0.262198581 */, 18 },
  /* 4246 */  { MAD_F(0x04324d7f) /* 0.262280940 */, 18 },
  /* 4247 */  { MAD_F(0x0432a3dd) /* 0.262363305 */, 18 },
  /* 4248 */  { MAD_F(0x0432fa3d) /* 0.262445676 */, 18 },
  /* 4249 */  { MAD_F(0x0433509e) /* 0.262528054 */, 18 },
  /* 4250 */  { MAD_F(0x0433a701) /* 0.262610438 */, 18 },
  /* 4251 */  { MAD_F(0x0433fd65) /* 0.262692829 */, 18 },
  /* 4252 */  { MAD_F(0x043453cc) /* 0.262775227 */, 18 },
  /* 4253 */  { MAD_F(0x0434aa34) /* 0.262857630 */, 18 },
  /* 4254 */  { MAD_F(0x0435009d) /* 0.262940040 */, 18 },
  /* 4255 */  { MAD_F(0x04355709) /* 0.263022457 */, 18 },

  /* 4256 */  { MAD_F(0x0435ad76) /* 0.263104880 */, 18 },
  /* 4257 */  { MAD_F(0x043603e5) /* 0.263187310 */, 18 },
  /* 4258 */  { MAD_F(0x04365a56) /* 0.263269746 */, 18 },
  /* 4259 */  { MAD_F(0x0436b0c9) /* 0.263352188 */, 18 },
  /* 4260 */  { MAD_F(0x0437073d) /* 0.263434637 */, 18 },
  /* 4261 */  { MAD_F(0x04375db3) /* 0.263517093 */, 18 },
  /* 4262 */  { MAD_F(0x0437b42a) /* 0.263599554 */, 18 },
  /* 4263 */  { MAD_F(0x04380aa4) /* 0.263682023 */, 18 },
  /* 4264 */  { MAD_F(0x0438611f) /* 0.263764497 */, 18 },
  /* 4265 */  { MAD_F(0x0438b79c) /* 0.263846979 */, 18 },
  /* 4266 */  { MAD_F(0x04390e1a) /* 0.263929466 */, 18 },
  /* 4267 */  { MAD_F(0x0439649b) /* 0.264011960 */, 18 },
  /* 4268 */  { MAD_F(0x0439bb1d) /* 0.264094461 */, 18 },
  /* 4269 */  { MAD_F(0x043a11a1) /* 0.264176968 */, 18 },
  /* 4270 */  { MAD_F(0x043a6826) /* 0.264259481 */, 18 },
  /* 4271 */  { MAD_F(0x043abead) /* 0.264342001 */, 18 },

  /* 4272 */  { MAD_F(0x043b1536) /* 0.264424527 */, 18 },
  /* 4273 */  { MAD_F(0x043b6bc1) /* 0.264507060 */, 18 },
  /* 4274 */  { MAD_F(0x043bc24d) /* 0.264589599 */, 18 },
  /* 4275 */  { MAD_F(0x043c18dc) /* 0.264672145 */, 18 },
  /* 4276 */  { MAD_F(0x043c6f6c) /* 0.264754697 */, 18 },
  /* 4277 */  { MAD_F(0x043cc5fd) /* 0.264837255 */, 18 },
  /* 4278 */  { MAD_F(0x043d1c91) /* 0.264919820 */, 18 },
  /* 4279 */  { MAD_F(0x043d7326) /* 0.265002392 */, 18 },
  /* 4280 */  { MAD_F(0x043dc9bc) /* 0.265084969 */, 18 },
  /* 4281 */  { MAD_F(0x043e2055) /* 0.265167554 */, 18 },
  /* 4282 */  { MAD_F(0x043e76ef) /* 0.265250144 */, 18 },
  /* 4283 */  { MAD_F(0x043ecd8b) /* 0.265332741 */, 18 },
  /* 4284 */  { MAD_F(0x043f2429) /* 0.265415345 */, 18 },
  /* 4285 */  { MAD_F(0x043f7ac8) /* 0.265497955 */, 18 },
  /* 4286 */  { MAD_F(0x043fd169) /* 0.265580571 */, 18 },
  /* 4287 */  { MAD_F(0x0440280c) /* 0.265663194 */, 18 },

  /* 4288 */  { MAD_F(0x04407eb1) /* 0.265745823 */, 18 },
  /* 4289 */  { MAD_F(0x0440d557) /* 0.265828459 */, 18 },
  /* 4290 */  { MAD_F(0x04412bff) /* 0.265911101 */, 18 },
  /* 4291 */  { MAD_F(0x044182a9) /* 0.265993749 */, 18 },
  /* 4292 */  { MAD_F(0x0441d955) /* 0.266076404 */, 18 },
  /* 4293 */  { MAD_F(0x04423002) /* 0.266159065 */, 18 },
  /* 4294 */  { MAD_F(0x044286b1) /* 0.266241733 */, 18 },
  /* 4295 */  { MAD_F(0x0442dd61) /* 0.266324407 */, 18 },
  /* 4296 */  { MAD_F(0x04433414) /* 0.266407088 */, 18 },
  /* 4297 */  { MAD_F(0x04438ac8) /* 0.266489775 */, 18 },
  /* 4298 */  { MAD_F(0x0443e17e) /* 0.266572468 */, 18 },
  /* 4299 */  { MAD_F(0x04443835) /* 0.266655168 */, 18 },
  /* 4300 */  { MAD_F(0x04448eef) /* 0.266737874 */, 18 },
  /* 4301 */  { MAD_F(0x0444e5aa) /* 0.266820587 */, 18 },
  /* 4302 */  { MAD_F(0x04453c66) /* 0.266903306 */, 18 },
  /* 4303 */  { MAD_F(0x04459325) /* 0.266986031 */, 18 },

  /* 4304 */  { MAD_F(0x0445e9e5) /* 0.267068763 */, 18 },
  /* 4305 */  { MAD_F(0x044640a7) /* 0.267151501 */, 18 },
  /* 4306 */  { MAD_F(0x0446976a) /* 0.267234246 */, 18 },
  /* 4307 */  { MAD_F(0x0446ee30) /* 0.267316997 */, 18 },
  /* 4308 */  { MAD_F(0x044744f7) /* 0.267399755 */, 18 },
  /* 4309 */  { MAD_F(0x04479bc0) /* 0.267482518 */, 18 },
  /* 4310 */  { MAD_F(0x0447f28a) /* 0.267565289 */, 18 },
  /* 4311 */  { MAD_F(0x04484956) /* 0.267648065 */, 18 },
  /* 4312 */  { MAD_F(0x0448a024) /* 0.267730848 */, 18 },
  /* 4313 */  { MAD_F(0x0448f6f4) /* 0.267813638 */, 18 },
  /* 4314 */  { MAD_F(0x04494dc5) /* 0.267896434 */, 18 },
  /* 4315 */  { MAD_F(0x0449a498) /* 0.267979236 */, 18 },
  /* 4316 */  { MAD_F(0x0449fb6d) /* 0.268062045 */, 18 },
  /* 4317 */  { MAD_F(0x044a5243) /* 0.268144860 */, 18 },
  /* 4318 */  { MAD_F(0x044aa91c) /* 0.268227681 */, 18 },
  /* 4319 */  { MAD_F(0x044afff6) /* 0.268310509 */, 18 },

  /* 4320 */  { MAD_F(0x044b56d1) /* 0.268393343 */, 18 },
  /* 4321 */  { MAD_F(0x044badaf) /* 0.268476184 */, 18 },
  /* 4322 */  { MAD_F(0x044c048e) /* 0.268559031 */, 18 },
  /* 4323 */  { MAD_F(0x044c5b6f) /* 0.268641885 */, 18 },
  /* 4324 */  { MAD_F(0x044cb251) /* 0.268724744 */, 18 },
  /* 4325 */  { MAD_F(0x044d0935) /* 0.268807611 */, 18 },
  /* 4326 */  { MAD_F(0x044d601b) /* 0.268890483 */, 18 },
  /* 4327 */  { MAD_F(0x044db703) /* 0.268973362 */, 18 },
  /* 4328 */  { MAD_F(0x044e0dec) /* 0.269056248 */, 18 },
  /* 4329 */  { MAD_F(0x044e64d7) /* 0.269139139 */, 18 },
  /* 4330 */  { MAD_F(0x044ebbc4) /* 0.269222037 */, 18 },
  /* 4331 */  { MAD_F(0x044f12b3) /* 0.269304942 */, 18 },
  /* 4332 */  { MAD_F(0x044f69a3) /* 0.269387853 */, 18 },
  /* 4333 */  { MAD_F(0x044fc095) /* 0.269470770 */, 18 },
  /* 4334 */  { MAD_F(0x04501788) /* 0.269553694 */, 18 },
  /* 4335 */  { MAD_F(0x04506e7e) /* 0.269636624 */, 18 },

  /* 4336 */  { MAD_F(0x0450c575) /* 0.269719560 */, 18 },
  /* 4337 */  { MAD_F(0x04511c6e) /* 0.269802503 */, 18 },
  /* 4338 */  { MAD_F(0x04517368) /* 0.269885452 */, 18 },
  /* 4339 */  { MAD_F(0x0451ca64) /* 0.269968408 */, 18 },
  /* 4340 */  { MAD_F(0x04522162) /* 0.270051370 */, 18 },
  /* 4341 */  { MAD_F(0x04527862) /* 0.270134338 */, 18 },
  /* 4342 */  { MAD_F(0x0452cf63) /* 0.270217312 */, 18 },
  /* 4343 */  { MAD_F(0x04532666) /* 0.270300293 */, 18 },
  /* 4344 */  { MAD_F(0x04537d6b) /* 0.270383281 */, 18 },
  /* 4345 */  { MAD_F(0x0453d472) /* 0.270466275 */, 18 },
  /* 4346 */  { MAD_F(0x04542b7a) /* 0.270549275 */, 18 },
  /* 4347 */  { MAD_F(0x04548284) /* 0.270632281 */, 18 },
  /* 4348 */  { MAD_F(0x0454d98f) /* 0.270715294 */, 18 },
  /* 4349 */  { MAD_F(0x0455309c) /* 0.270798313 */, 18 },
  /* 4350 */  { MAD_F(0x045587ab) /* 0.270881339 */, 18 },
  /* 4351 */  { MAD_F(0x0455debc) /* 0.270964371 */, 18 },

  /* 4352 */  { MAD_F(0x045635cf) /* 0.271047409 */, 18 },
  /* 4353 */  { MAD_F(0x04568ce3) /* 0.271130454 */, 18 },
  /* 4354 */  { MAD_F(0x0456e3f9) /* 0.271213505 */, 18 },
  /* 4355 */  { MAD_F(0x04573b10) /* 0.271296562 */, 18 },
  /* 4356 */  { MAD_F(0x04579229) /* 0.271379626 */, 18 },
  /* 4357 */  { MAD_F(0x0457e944) /* 0.271462696 */, 18 },
  /* 4358 */  { MAD_F(0x04584061) /* 0.271545772 */, 18 },
  /* 4359 */  { MAD_F(0x0458977f) /* 0.271628855 */, 18 },
  /* 4360 */  { MAD_F(0x0458ee9f) /* 0.271711944 */, 18 },
  /* 4361 */  { MAD_F(0x045945c1) /* 0.271795040 */, 18 },
  /* 4362 */  { MAD_F(0x04599ce5) /* 0.271878142 */, 18 },
  /* 4363 */  { MAD_F(0x0459f40a) /* 0.271961250 */, 18 },
  /* 4364 */  { MAD_F(0x045a4b31) /* 0.272044365 */, 18 },
  /* 4365 */  { MAD_F(0x045aa259) /* 0.272127486 */, 18 },
  /* 4366 */  { MAD_F(0x045af984) /* 0.272210613 */, 18 },
  /* 4367 */  { MAD_F(0x045b50b0) /* 0.272293746 */, 18 },

  /* 4368 */  { MAD_F(0x045ba7dd) /* 0.272376886 */, 18 },
  /* 4369 */  { MAD_F(0x045bff0d) /* 0.272460033 */, 18 },
  /* 4370 */  { MAD_F(0x045c563e) /* 0.272543185 */, 18 },
  /* 4371 */  { MAD_F(0x045cad71) /* 0.272626344 */, 18 },
  /* 4372 */  { MAD_F(0x045d04a5) /* 0.272709510 */, 18 },
  /* 4373 */  { MAD_F(0x045d5bdc) /* 0.272792681 */, 18 },
  /* 4374 */  { MAD_F(0x045db313) /* 0.272875859 */, 18 },
  /* 4375 */  { MAD_F(0x045e0a4d) /* 0.272959044 */, 18 },
  /* 4376 */  { MAD_F(0x045e6188) /* 0.273042234 */, 18 },
  /* 4377 */  { MAD_F(0x045eb8c5) /* 0.273125431 */, 18 },
  /* 4378 */  { MAD_F(0x045f1004) /* 0.273208635 */, 18 },
  /* 4379 */  { MAD_F(0x045f6745) /* 0.273291844 */, 18 },
  /* 4380 */  { MAD_F(0x045fbe87) /* 0.273375060 */, 18 },
  /* 4381 */  { MAD_F(0x046015cb) /* 0.273458283 */, 18 },
  /* 4382 */  { MAD_F(0x04606d10) /* 0.273541511 */, 18 },
  /* 4383 */  { MAD_F(0x0460c457) /* 0.273624747 */, 18 },

  /* 4384 */  { MAD_F(0x04611ba0) /* 0.273707988 */, 18 },
  /* 4385 */  { MAD_F(0x046172eb) /* 0.273791236 */, 18 },
  /* 4386 */  { MAD_F(0x0461ca37) /* 0.273874490 */, 18 },
  /* 4387 */  { MAD_F(0x04622185) /* 0.273957750 */, 18 },
  /* 4388 */  { MAD_F(0x046278d5) /* 0.274041017 */, 18 },
  /* 4389 */  { MAD_F(0x0462d026) /* 0.274124290 */, 18 },
  /* 4390 */  { MAD_F(0x0463277a) /* 0.274207569 */, 18 },
  /* 4391 */  { MAD_F(0x04637ece) /* 0.274290855 */, 18 },
  /* 4392 */  { MAD_F(0x0463d625) /* 0.274374147 */, 18 },
  /* 4393 */  { MAD_F(0x04642d7d) /* 0.274457445 */, 18 },
  /* 4394 */  { MAD_F(0x046484d7) /* 0.274540749 */, 18 },
  /* 4395 */  { MAD_F(0x0464dc33) /* 0.274624060 */, 18 },
  /* 4396 */  { MAD_F(0x04653390) /* 0.274707378 */, 18 },
  /* 4397 */  { MAD_F(0x04658aef) /* 0.274790701 */, 18 },
  /* 4398 */  { MAD_F(0x0465e250) /* 0.274874031 */, 18 },
  /* 4399 */  { MAD_F(0x046639b2) /* 0.274957367 */, 18 },

  /* 4400 */  { MAD_F(0x04669116) /* 0.275040710 */, 18 },
  /* 4401 */  { MAD_F(0x0466e87c) /* 0.275124059 */, 18 },
  /* 4402 */  { MAD_F(0x04673fe3) /* 0.275207414 */, 18 },
  /* 4403 */  { MAD_F(0x0467974d) /* 0.275290775 */, 18 },
  /* 4404 */  { MAD_F(0x0467eeb7) /* 0.275374143 */, 18 },
  /* 4405 */  { MAD_F(0x04684624) /* 0.275457517 */, 18 },
  /* 4406 */  { MAD_F(0x04689d92) /* 0.275540897 */, 18 },
  /* 4407 */  { MAD_F(0x0468f502) /* 0.275624284 */, 18 },
  /* 4408 */  { MAD_F(0x04694c74) /* 0.275707677 */, 18 },
  /* 4409 */  { MAD_F(0x0469a3e7) /* 0.275791076 */, 18 },
  /* 4410 */  { MAD_F(0x0469fb5c) /* 0.275874482 */, 18 },
  /* 4411 */  { MAD_F(0x046a52d3) /* 0.275957894 */, 18 },
  /* 4412 */  { MAD_F(0x046aaa4b) /* 0.276041312 */, 18 },
  /* 4413 */  { MAD_F(0x046b01c5) /* 0.276124737 */, 18 },
  /* 4414 */  { MAD_F(0x046b5941) /* 0.276208167 */, 18 },
  /* 4415 */  { MAD_F(0x046bb0bf) /* 0.276291605 */, 18 },

  /* 4416 */  { MAD_F(0x046c083e) /* 0.276375048 */, 18 },
  /* 4417 */  { MAD_F(0x046c5fbf) /* 0.276458498 */, 18 },
  /* 4418 */  { MAD_F(0x046cb741) /* 0.276541954 */, 18 },
  /* 4419 */  { MAD_F(0x046d0ec5) /* 0.276625416 */, 18 },
  /* 4420 */  { MAD_F(0x046d664b) /* 0.276708885 */, 18 },
  /* 4421 */  { MAD_F(0x046dbdd3) /* 0.276792360 */, 18 },
  /* 4422 */  { MAD_F(0x046e155c) /* 0.276875841 */, 18 },
  /* 4423 */  { MAD_F(0x046e6ce7) /* 0.276959328 */, 18 },
  /* 4424 */  { MAD_F(0x046ec474) /* 0.277042822 */, 18 },
  /* 4425 */  { MAD_F(0x046f1c02) /* 0.277126322 */, 18 },
  /* 4426 */  { MAD_F(0x046f7392) /* 0.277209829 */, 18 },
  /* 4427 */  { MAD_F(0x046fcb24) /* 0.277293341 */, 18 },
  /* 4428 */  { MAD_F(0x047022b8) /* 0.277376860 */, 18 },
  /* 4429 */  { MAD_F(0x04707a4d) /* 0.277460385 */, 18 },
  /* 4430 */  { MAD_F(0x0470d1e4) /* 0.277543917 */, 18 },
  /* 4431 */  { MAD_F(0x0471297c) /* 0.277627455 */, 18 },

  /* 4432 */  { MAD_F(0x04718116) /* 0.277710999 */, 18 },
  /* 4433 */  { MAD_F(0x0471d8b2) /* 0.277794549 */, 18 },
  /* 4434 */  { MAD_F(0x04723050) /* 0.277878106 */, 18 },
  /* 4435 */  { MAD_F(0x047287ef) /* 0.277961669 */, 18 },
  /* 4436 */  { MAD_F(0x0472df90) /* 0.278045238 */, 18 },
  /* 4437 */  { MAD_F(0x04733733) /* 0.278128813 */, 18 },
  /* 4438 */  { MAD_F(0x04738ed7) /* 0.278212395 */, 18 },
  /* 4439 */  { MAD_F(0x0473e67d) /* 0.278295983 */, 18 },
  /* 4440 */  { MAD_F(0x04743e25) /* 0.278379578 */, 18 },
  /* 4441 */  { MAD_F(0x047495ce) /* 0.278463178 */, 18 },
  /* 4442 */  { MAD_F(0x0474ed79) /* 0.278546785 */, 18 },
  /* 4443 */  { MAD_F(0x04754526) /* 0.278630398 */, 18 },
  /* 4444 */  { MAD_F(0x04759cd4) /* 0.278714018 */, 18 },
  /* 4445 */  { MAD_F(0x0475f484) /* 0.278797643 */, 18 },
  /* 4446 */  { MAD_F(0x04764c36) /* 0.278881275 */, 18 },
  /* 4447 */  { MAD_F(0x0476a3ea) /* 0.278964914 */, 18 },

  /* 4448 */  { MAD_F(0x0476fb9f) /* 0.279048558 */, 18 },
  /* 4449 */  { MAD_F(0x04775356) /* 0.279132209 */, 18 },
  /* 4450 */  { MAD_F(0x0477ab0e) /* 0.279215866 */, 18 },
  /* 4451 */  { MAD_F(0x047802c8) /* 0.279299529 */, 18 },
  /* 4452 */  { MAD_F(0x04785a84) /* 0.279383199 */, 18 },
  /* 4453 */  { MAD_F(0x0478b242) /* 0.279466875 */, 18 },
  /* 4454 */  { MAD_F(0x04790a01) /* 0.279550557 */, 18 },
  /* 4455 */  { MAD_F(0x047961c2) /* 0.279634245 */, 18 },
  /* 4456 */  { MAD_F(0x0479b984) /* 0.279717940 */, 18 },
  /* 4457 */  { MAD_F(0x047a1149) /* 0.279801641 */, 18 },
  /* 4458 */  { MAD_F(0x047a690f) /* 0.279885348 */, 18 },
  /* 4459 */  { MAD_F(0x047ac0d6) /* 0.279969061 */, 18 },
  /* 4460 */  { MAD_F(0x047b18a0) /* 0.280052781 */, 18 },
  /* 4461 */  { MAD_F(0x047b706b) /* 0.280136507 */, 18 },
  /* 4462 */  { MAD_F(0x047bc837) /* 0.280220239 */, 18 },
  /* 4463 */  { MAD_F(0x047c2006) /* 0.280303978 */, 18 },

  /* 4464 */  { MAD_F(0x047c77d6) /* 0.280387722 */, 18 },
  /* 4465 */  { MAD_F(0x047ccfa8) /* 0.280471473 */, 18 },
  /* 4466 */  { MAD_F(0x047d277b) /* 0.280555230 */, 18 },
  /* 4467 */  { MAD_F(0x047d7f50) /* 0.280638994 */, 18 },
  /* 4468 */  { MAD_F(0x047dd727) /* 0.280722764 */, 18 },
  /* 4469 */  { MAD_F(0x047e2eff) /* 0.280806540 */, 18 },
  /* 4470 */  { MAD_F(0x047e86d9) /* 0.280890322 */, 18 },
  /* 4471 */  { MAD_F(0x047edeb5) /* 0.280974110 */, 18 },
  /* 4472 */  { MAD_F(0x047f3693) /* 0.281057905 */, 18 },
  /* 4473 */  { MAD_F(0x047f8e72) /* 0.281141706 */, 18 },
  /* 4474 */  { MAD_F(0x047fe653) /* 0.281225513 */, 18 },
  /* 4475 */  { MAD_F(0x04803e35) /* 0.281309326 */, 18 },
  /* 4476 */  { MAD_F(0x04809619) /* 0.281393146 */, 18 },
  /* 4477 */  { MAD_F(0x0480edff) /* 0.281476972 */, 18 },
  /* 4478 */  { MAD_F(0x048145e7) /* 0.281560804 */, 18 },
  /* 4479 */  { MAD_F(0x04819dd0) /* 0.281644643 */, 18 },

  /* 4480 */  { MAD_F(0x0481f5bb) /* 0.281728487 */, 18 },
  /* 4481 */  { MAD_F(0x04824da7) /* 0.281812338 */, 18 },
  /* 4482 */  { MAD_F(0x0482a595) /* 0.281896195 */, 18 },
  /* 4483 */  { MAD_F(0x0482fd85) /* 0.281980059 */, 18 },
  /* 4484 */  { MAD_F(0x04835577) /* 0.282063928 */, 18 },
  /* 4485 */  { MAD_F(0x0483ad6a) /* 0.282147804 */, 18 },
  /* 4486 */  { MAD_F(0x0484055f) /* 0.282231686 */, 18 },
  /* 4487 */  { MAD_F(0x04845d56) /* 0.282315574 */, 18 },
  /* 4488 */  { MAD_F(0x0484b54e) /* 0.282399469 */, 18 },
  /* 4489 */  { MAD_F(0x04850d48) /* 0.282483370 */, 18 },
  /* 4490 */  { MAD_F(0x04856544) /* 0.282567277 */, 18 },
  /* 4491 */  { MAD_F(0x0485bd41) /* 0.282651190 */, 18 },
  /* 4492 */  { MAD_F(0x04861540) /* 0.282735109 */, 18 },
  /* 4493 */  { MAD_F(0x04866d40) /* 0.282819035 */, 18 },
  /* 4494 */  { MAD_F(0x0486c543) /* 0.282902967 */, 18 },
  /* 4495 */  { MAD_F(0x04871d47) /* 0.282986905 */, 18 },

  /* 4496 */  { MAD_F(0x0487754c) /* 0.283070849 */, 18 },
  /* 4497 */  { MAD_F(0x0487cd54) /* 0.283154800 */, 18 },
  /* 4498 */  { MAD_F(0x0488255d) /* 0.283238757 */, 18 },
  /* 4499 */  { MAD_F(0x04887d67) /* 0.283322720 */, 18 },
  /* 4500 */  { MAD_F(0x0488d574) /* 0.283406689 */, 18 },
  /* 4501 */  { MAD_F(0x04892d82) /* 0.283490665 */, 18 },
  /* 4502 */  { MAD_F(0x04898591) /* 0.283574646 */, 18 },
  /* 4503 */  { MAD_F(0x0489dda3) /* 0.283658634 */, 18 },
  /* 4504 */  { MAD_F(0x048a35b6) /* 0.283742628 */, 18 },
  /* 4505 */  { MAD_F(0x048a8dca) /* 0.283826629 */, 18 },
  /* 4506 */  { MAD_F(0x048ae5e1) /* 0.283910635 */, 18 },
  /* 4507 */  { MAD_F(0x048b3df9) /* 0.283994648 */, 18 },
  /* 4508 */  { MAD_F(0x048b9612) /* 0.284078667 */, 18 },
  /* 4509 */  { MAD_F(0x048bee2e) /* 0.284162692 */, 18 },
  /* 4510 */  { MAD_F(0x048c464b) /* 0.284246723 */, 18 },
  /* 4511 */  { MAD_F(0x048c9e69) /* 0.284330761 */, 18 },

  /* 4512 */  { MAD_F(0x048cf68a) /* 0.284414805 */, 18 },
  /* 4513 */  { MAD_F(0x048d4eac) /* 0.284498855 */, 18 },
  /* 4514 */  { MAD_F(0x048da6cf) /* 0.284582911 */, 18 },
  /* 4515 */  { MAD_F(0x048dfef5) /* 0.284666974 */, 18 },
  /* 4516 */  { MAD_F(0x048e571c) /* 0.284751042 */, 18 },
  /* 4517 */  { MAD_F(0x048eaf44) /* 0.284835117 */, 18 },
  /* 4518 */  { MAD_F(0x048f076f) /* 0.284919198 */, 18 },
  /* 4519 */  { MAD_F(0x048f5f9b) /* 0.285003285 */, 18 },
  /* 4520 */  { MAD_F(0x048fb7c8) /* 0.285087379 */, 18 },
  /* 4521 */  { MAD_F(0x04900ff8) /* 0.285171479 */, 18 },
  /* 4522 */  { MAD_F(0x04906829) /* 0.285255584 */, 18 },
  /* 4523 */  { MAD_F(0x0490c05b) /* 0.285339697 */, 18 },
  /* 4524 */  { MAD_F(0x04911890) /* 0.285423815 */, 18 },
  /* 4525 */  { MAD_F(0x049170c6) /* 0.285507939 */, 18 },
  /* 4526 */  { MAD_F(0x0491c8fd) /* 0.285592070 */, 18 },
  /* 4527 */  { MAD_F(0x04922137) /* 0.285676207 */, 18 },

  /* 4528 */  { MAD_F(0x04927972) /* 0.285760350 */, 18 },
  /* 4529 */  { MAD_F(0x0492d1ae) /* 0.285844499 */, 18 },
  /* 4530 */  { MAD_F(0x049329ed) /* 0.285928655 */, 18 },
  /* 4531 */  { MAD_F(0x0493822c) /* 0.286012816 */, 18 },
  /* 4532 */  { MAD_F(0x0493da6e) /* 0.286096984 */, 18 },
  /* 4533 */  { MAD_F(0x049432b1) /* 0.286181158 */, 18 },
  /* 4534 */  { MAD_F(0x04948af6) /* 0.286265338 */, 18 },
  /* 4535 */  { MAD_F(0x0494e33d) /* 0.286349525 */, 18 },
  /* 4536 */  { MAD_F(0x04953b85) /* 0.286433717 */, 18 },
  /* 4537 */  { MAD_F(0x049593cf) /* 0.286517916 */, 18 },
  /* 4538 */  { MAD_F(0x0495ec1b) /* 0.286602121 */, 18 },
  /* 4539 */  { MAD_F(0x04964468) /* 0.286686332 */, 18 },
  /* 4540 */  { MAD_F(0x04969cb7) /* 0.286770550 */, 18 },
  /* 4541 */  { MAD_F(0x0496f508) /* 0.286854773 */, 18 },
  /* 4542 */  { MAD_F(0x04974d5a) /* 0.286939003 */, 18 },
  /* 4543 */  { MAD_F(0x0497a5ae) /* 0.287023239 */, 18 },

  /* 4544 */  { MAD_F(0x0497fe03) /* 0.287107481 */, 18 },
  /* 4545 */  { MAD_F(0x0498565a) /* 0.287191729 */, 18 },
  /* 4546 */  { MAD_F(0x0498aeb3) /* 0.287275983 */, 18 },
  /* 4547 */  { MAD_F(0x0499070e) /* 0.287360244 */, 18 },
  /* 4548 */  { MAD_F(0x04995f6a) /* 0.287444511 */, 18 },
  /* 4549 */  { MAD_F(0x0499b7c8) /* 0.287528784 */, 18 },
  /* 4550 */  { MAD_F(0x049a1027) /* 0.287613063 */, 18 },
  /* 4551 */  { MAD_F(0x049a6889) /* 0.287697348 */, 18 },
  /* 4552 */  { MAD_F(0x049ac0eb) /* 0.287781640 */, 18 },
  /* 4553 */  { MAD_F(0x049b1950) /* 0.287865937 */, 18 },
  /* 4554 */  { MAD_F(0x049b71b6) /* 0.287950241 */, 18 },
  /* 4555 */  { MAD_F(0x049bca1e) /* 0.288034551 */, 18 },
  /* 4556 */  { MAD_F(0x049c2287) /* 0.288118867 */, 18 },
  /* 4557 */  { MAD_F(0x049c7af2) /* 0.288203190 */, 18 },
  /* 4558 */  { MAD_F(0x049cd35f) /* 0.288287518 */, 18 },
  /* 4559 */  { MAD_F(0x049d2bce) /* 0.288371853 */, 18 },

  /* 4560 */  { MAD_F(0x049d843e) /* 0.288456194 */, 18 },
  /* 4561 */  { MAD_F(0x049ddcaf) /* 0.288540541 */, 18 },
  /* 4562 */  { MAD_F(0x049e3523) /* 0.288624894 */, 18 },
  /* 4563 */  { MAD_F(0x049e8d98) /* 0.288709253 */, 18 },
  /* 4564 */  { MAD_F(0x049ee60e) /* 0.288793619 */, 18 },
  /* 4565 */  { MAD_F(0x049f3e87) /* 0.288877990 */, 18 },
  /* 4566 */  { MAD_F(0x049f9701) /* 0.288962368 */, 18 },
  /* 4567 */  { MAD_F(0x049fef7c) /* 0.289046752 */, 18 },
  /* 4568 */  { MAD_F(0x04a047fa) /* 0.289131142 */, 18 },
  /* 4569 */  { MAD_F(0x04a0a079) /* 0.289215538 */, 18 },
  /* 4570 */  { MAD_F(0x04a0f8f9) /* 0.289299941 */, 18 },
  /* 4571 */  { MAD_F(0x04a1517c) /* 0.289384349 */, 18 },
  /* 4572 */  { MAD_F(0x04a1a9ff) /* 0.289468764 */, 18 },
  /* 4573 */  { MAD_F(0x04a20285) /* 0.289553185 */, 18 },
  /* 4574 */  { MAD_F(0x04a25b0c) /* 0.289637612 */, 18 },
  /* 4575 */  { MAD_F(0x04a2b395) /* 0.289722045 */, 18 },

  /* 4576 */  { MAD_F(0x04a30c20) /* 0.289806485 */, 18 },
  /* 4577 */  { MAD_F(0x04a364ac) /* 0.289890930 */, 18 },
  /* 4578 */  { MAD_F(0x04a3bd3a) /* 0.289975382 */, 18 },
  /* 4579 */  { MAD_F(0x04a415c9) /* 0.290059840 */, 18 },
  /* 4580 */  { MAD_F(0x04a46e5a) /* 0.290144304 */, 18 },
  /* 4581 */  { MAD_F(0x04a4c6ed) /* 0.290228774 */, 18 },
  /* 4582 */  { MAD_F(0x04a51f81) /* 0.290313250 */, 18 },
  /* 4583 */  { MAD_F(0x04a57818) /* 0.290397733 */, 18 },
  /* 4584 */  { MAD_F(0x04a5d0af) /* 0.290482221 */, 18 },
  /* 4585 */  { MAD_F(0x04a62949) /* 0.290566716 */, 18 },
  /* 4586 */  { MAD_F(0x04a681e4) /* 0.290651217 */, 18 },
  /* 4587 */  { MAD_F(0x04a6da80) /* 0.290735724 */, 18 },
  /* 4588 */  { MAD_F(0x04a7331f) /* 0.290820237 */, 18 },
  /* 4589 */  { MAD_F(0x04a78bbf) /* 0.290904756 */, 18 },
  /* 4590 */  { MAD_F(0x04a7e460) /* 0.290989281 */, 18 },
  /* 4591 */  { MAD_F(0x04a83d03) /* 0.291073813 */, 18 },

  /* 4592 */  { MAD_F(0x04a895a8) /* 0.291158351 */, 18 },
  /* 4593 */  { MAD_F(0x04a8ee4f) /* 0.291242894 */, 18 },
  /* 4594 */  { MAD_F(0x04a946f7) /* 0.291327444 */, 18 },
  /* 4595 */  { MAD_F(0x04a99fa1) /* 0.291412001 */, 18 },
  /* 4596 */  { MAD_F(0x04a9f84c) /* 0.291496563 */, 18 },
  /* 4597 */  { MAD_F(0x04aa50fa) /* 0.291581131 */, 18 },
  /* 4598 */  { MAD_F(0x04aaa9a8) /* 0.291665706 */, 18 },
  /* 4599 */  { MAD_F(0x04ab0259) /* 0.291750286 */, 18 },
  /* 4600 */  { MAD_F(0x04ab5b0b) /* 0.291834873 */, 18 },
  /* 4601 */  { MAD_F(0x04abb3bf) /* 0.291919466 */, 18 },
  /* 4602 */  { MAD_F(0x04ac0c74) /* 0.292004065 */, 18 },
  /* 4603 */  { MAD_F(0x04ac652b) /* 0.292088670 */, 18 },
  /* 4604 */  { MAD_F(0x04acbde4) /* 0.292173281 */, 18 },
  /* 4605 */  { MAD_F(0x04ad169e) /* 0.292257899 */, 18 },
  /* 4606 */  { MAD_F(0x04ad6f5a) /* 0.292342522 */, 18 },
  /* 4607 */  { MAD_F(0x04adc818) /* 0.292427152 */, 18 },

  /* 4608 */  { MAD_F(0x04ae20d7) /* 0.292511788 */, 18 },
  /* 4609 */  { MAD_F(0x04ae7998) /* 0.292596430 */, 18 },
  /* 4610 */  { MAD_F(0x04aed25a) /* 0.292681078 */, 18 },
  /* 4611 */  { MAD_F(0x04af2b1e) /* 0.292765732 */, 18 },
  /* 4612 */  { MAD_F(0x04af83e4) /* 0.292850392 */, 18 },
  /* 4613 */  { MAD_F(0x04afdcac) /* 0.292935058 */, 18 },
  /* 4614 */  { MAD_F(0x04b03575) /* 0.293019731 */, 18 },
  /* 4615 */  { MAD_F(0x04b08e40) /* 0.293104409 */, 18 },
  /* 4616 */  { MAD_F(0x04b0e70c) /* 0.293189094 */, 18 },
  /* 4617 */  { MAD_F(0x04b13fda) /* 0.293273785 */, 18 },
  /* 4618 */  { MAD_F(0x04b198aa) /* 0.293358482 */, 18 },
  /* 4619 */  { MAD_F(0x04b1f17b) /* 0.293443185 */, 18 },
  /* 4620 */  { MAD_F(0x04b24a4e) /* 0.293527894 */, 18 },
  /* 4621 */  { MAD_F(0x04b2a322) /* 0.293612609 */, 18 },
  /* 4622 */  { MAD_F(0x04b2fbf9) /* 0.293697331 */, 18 },
  /* 4623 */  { MAD_F(0x04b354d1) /* 0.293782058 */, 18 },

  /* 4624 */  { MAD_F(0x04b3adaa) /* 0.293866792 */, 18 },
  /* 4625 */  { MAD_F(0x04b40685) /* 0.293951532 */, 18 },
  /* 4626 */  { MAD_F(0x04b45f62) /* 0.294036278 */, 18 },
  /* 4627 */  { MAD_F(0x04b4b840) /* 0.294121029 */, 18 },
  /* 4628 */  { MAD_F(0x04b51120) /* 0.294205788 */, 18 },
  /* 4629 */  { MAD_F(0x04b56a02) /* 0.294290552 */, 18 },
  /* 4630 */  { MAD_F(0x04b5c2e6) /* 0.294375322 */, 18 },
  /* 4631 */  { MAD_F(0x04b61bcb) /* 0.294460098 */, 18 },
  /* 4632 */  { MAD_F(0x04b674b1) /* 0.294544881 */, 18 },
  /* 4633 */  { MAD_F(0x04b6cd99) /* 0.294629669 */, 18 },
  /* 4634 */  { MAD_F(0x04b72683) /* 0.294714464 */, 18 },
  /* 4635 */  { MAD_F(0x04b77f6f) /* 0.294799265 */, 18 },
  /* 4636 */  { MAD_F(0x04b7d85c) /* 0.294884072 */, 18 },
  /* 4637 */  { MAD_F(0x04b8314b) /* 0.294968885 */, 18 },
  /* 4638 */  { MAD_F(0x04b88a3b) /* 0.295053704 */, 18 },
  /* 4639 */  { MAD_F(0x04b8e32d) /* 0.295138529 */, 18 },

  /* 4640 */  { MAD_F(0x04b93c21) /* 0.295223360 */, 18 },
  /* 4641 */  { MAD_F(0x04b99516) /* 0.295308197 */, 18 },
  /* 4642 */  { MAD_F(0x04b9ee0d) /* 0.295393041 */, 18 },
  /* 4643 */  { MAD_F(0x04ba4706) /* 0.295477890 */, 18 },
  /* 4644 */  { MAD_F(0x04baa000) /* 0.295562746 */, 18 },
  /* 4645 */  { MAD_F(0x04baf8fc) /* 0.295647608 */, 18 },
  /* 4646 */  { MAD_F(0x04bb51fa) /* 0.295732476 */, 18 },
  /* 4647 */  { MAD_F(0x04bbaaf9) /* 0.295817349 */, 18 },
  /* 4648 */  { MAD_F(0x04bc03fa) /* 0.295902229 */, 18 },
  /* 4649 */  { MAD_F(0x04bc5cfc) /* 0.295987115 */, 18 },
  /* 4650 */  { MAD_F(0x04bcb600) /* 0.296072008 */, 18 },
  /* 4651 */  { MAD_F(0x04bd0f06) /* 0.296156906 */, 18 },
  /* 4652 */  { MAD_F(0x04bd680d) /* 0.296241810 */, 18 },
  /* 4653 */  { MAD_F(0x04bdc116) /* 0.296326721 */, 18 },
  /* 4654 */  { MAD_F(0x04be1a21) /* 0.296411637 */, 18 },
  /* 4655 */  { MAD_F(0x04be732d) /* 0.296496560 */, 18 },

  /* 4656 */  { MAD_F(0x04becc3b) /* 0.296581488 */, 18 },
  /* 4657 */  { MAD_F(0x04bf254a) /* 0.296666423 */, 18 },
  /* 4658 */  { MAD_F(0x04bf7e5b) /* 0.296751364 */, 18 },
  /* 4659 */  { MAD_F(0x04bfd76e) /* 0.296836311 */, 18 },
  /* 4660 */  { MAD_F(0x04c03083) /* 0.296921264 */, 18 },
  /* 4661 */  { MAD_F(0x04c08999) /* 0.297006223 */, 18 },
  /* 4662 */  { MAD_F(0x04c0e2b0) /* 0.297091188 */, 18 },
  /* 4663 */  { MAD_F(0x04c13bca) /* 0.297176159 */, 18 },
  /* 4664 */  { MAD_F(0x04c194e4) /* 0.297261136 */, 18 },
  /* 4665 */  { MAD_F(0x04c1ee01) /* 0.297346120 */, 18 },
  /* 4666 */  { MAD_F(0x04c2471f) /* 0.297431109 */, 18 },
  /* 4667 */  { MAD_F(0x04c2a03f) /* 0.297516105 */, 18 },
  /* 4668 */  { MAD_F(0x04c2f960) /* 0.297601106 */, 18 },
  /* 4669 */  { MAD_F(0x04c35283) /* 0.297686114 */, 18 },
  /* 4670 */  { MAD_F(0x04c3aba8) /* 0.297771128 */, 18 },
  /* 4671 */  { MAD_F(0x04c404ce) /* 0.297856147 */, 18 },

  /* 4672 */  { MAD_F(0x04c45df6) /* 0.297941173 */, 18 },
  /* 4673 */  { MAD_F(0x04c4b720) /* 0.298026205 */, 18 },
  /* 4674 */  { MAD_F(0x04c5104b) /* 0.298111243 */, 18 },
  /* 4675 */  { MAD_F(0x04c56978) /* 0.298196287 */, 18 },
  /* 4676 */  { MAD_F(0x04c5c2a7) /* 0.298281337 */, 18 },
  /* 4677 */  { MAD_F(0x04c61bd7) /* 0.298366393 */, 18 },
  /* 4678 */  { MAD_F(0x04c67508) /* 0.298451456 */, 18 },
  /* 4679 */  { MAD_F(0x04c6ce3c) /* 0.298536524 */, 18 },
  /* 4680 */  { MAD_F(0x04c72771) /* 0.298621598 */, 18 },
  /* 4681 */  { MAD_F(0x04c780a7) /* 0.298706679 */, 18 },
  /* 4682 */  { MAD_F(0x04c7d9df) /* 0.298791765 */, 18 },
  /* 4683 */  { MAD_F(0x04c83319) /* 0.298876858 */, 18 },
  /* 4684 */  { MAD_F(0x04c88c55) /* 0.298961956 */, 18 },
  /* 4685 */  { MAD_F(0x04c8e592) /* 0.299047061 */, 18 },
  /* 4686 */  { MAD_F(0x04c93ed1) /* 0.299132172 */, 18 },
  /* 4687 */  { MAD_F(0x04c99811) /* 0.299217288 */, 18 },

  /* 4688 */  { MAD_F(0x04c9f153) /* 0.299302411 */, 18 },
  /* 4689 */  { MAD_F(0x04ca4a97) /* 0.299387540 */, 18 },
  /* 4690 */  { MAD_F(0x04caa3dc) /* 0.299472675 */, 18 },
  /* 4691 */  { MAD_F(0x04cafd23) /* 0.299557816 */, 18 },
  /* 4692 */  { MAD_F(0x04cb566b) /* 0.299642963 */, 18 },
  /* 4693 */  { MAD_F(0x04cbafb5) /* 0.299728116 */, 18 },
  /* 4694 */  { MAD_F(0x04cc0901) /* 0.299813275 */, 18 },
  /* 4695 */  { MAD_F(0x04cc624e) /* 0.299898440 */, 18 },
  /* 4696 */  { MAD_F(0x04ccbb9d) /* 0.299983611 */, 18 },
  /* 4697 */  { MAD_F(0x04cd14ee) /* 0.300068789 */, 18 },
  /* 4698 */  { MAD_F(0x04cd6e40) /* 0.300153972 */, 18 },
  /* 4699 */  { MAD_F(0x04cdc794) /* 0.300239161 */, 18 },
  /* 4700 */  { MAD_F(0x04ce20e9) /* 0.300324357 */, 18 },
  /* 4701 */  { MAD_F(0x04ce7a40) /* 0.300409558 */, 18 },
  /* 4702 */  { MAD_F(0x04ced399) /* 0.300494765 */, 18 },
  /* 4703 */  { MAD_F(0x04cf2cf3) /* 0.300579979 */, 18 },

  /* 4704 */  { MAD_F(0x04cf864f) /* 0.300665198 */, 18 },
  /* 4705 */  { MAD_F(0x04cfdfad) /* 0.300750424 */, 18 },
  /* 4706 */  { MAD_F(0x04d0390c) /* 0.300835656 */, 18 },
  /* 4707 */  { MAD_F(0x04d0926d) /* 0.300920893 */, 18 },
  /* 4708 */  { MAD_F(0x04d0ebcf) /* 0.301006137 */, 18 },
  /* 4709 */  { MAD_F(0x04d14533) /* 0.301091387 */, 18 },
  /* 4710 */  { MAD_F(0x04d19e99) /* 0.301176643 */, 18 },
  /* 4711 */  { MAD_F(0x04d1f800) /* 0.301261904 */, 18 },
  /* 4712 */  { MAD_F(0x04d25169) /* 0.301347172 */, 18 },
  /* 4713 */  { MAD_F(0x04d2aad4) /* 0.301432446 */, 18 },
  /* 4714 */  { MAD_F(0x04d30440) /* 0.301517726 */, 18 },
  /* 4715 */  { MAD_F(0x04d35dae) /* 0.301603012 */, 18 },
  /* 4716 */  { MAD_F(0x04d3b71d) /* 0.301688304 */, 18 },
  /* 4717 */  { MAD_F(0x04d4108e) /* 0.301773602 */, 18 },
  /* 4718 */  { MAD_F(0x04d46a01) /* 0.301858906 */, 18 },
  /* 4719 */  { MAD_F(0x04d4c375) /* 0.301944216 */, 18 },

  /* 4720 */  { MAD_F(0x04d51ceb) /* 0.302029532 */, 18 },
  /* 4721 */  { MAD_F(0x04d57662) /* 0.302114854 */, 18 },
  /* 4722 */  { MAD_F(0x04d5cfdb) /* 0.302200182 */, 18 },
  /* 4723 */  { MAD_F(0x04d62956) /* 0.302285516 */, 18 },
  /* 4724 */  { MAD_F(0x04d682d2) /* 0.302370856 */, 18 },
  /* 4725 */  { MAD_F(0x04d6dc50) /* 0.302456203 */, 18 },
  /* 4726 */  { MAD_F(0x04d735d0) /* 0.302541555 */, 18 },
  /* 4727 */  { MAD_F(0x04d78f51) /* 0.302626913 */, 18 },
  /* 4728 */  { MAD_F(0x04d7e8d4) /* 0.302712277 */, 18 },
  /* 4729 */  { MAD_F(0x04d84258) /* 0.302797648 */, 18 },
  /* 4730 */  { MAD_F(0x04d89bde) /* 0.302883024 */, 18 },
  /* 4731 */  { MAD_F(0x04d8f566) /* 0.302968406 */, 18 },
  /* 4732 */  { MAD_F(0x04d94eef) /* 0.303053794 */, 18 },
  /* 4733 */  { MAD_F(0x04d9a87a) /* 0.303139189 */, 18 },
  /* 4734 */  { MAD_F(0x04da0207) /* 0.303224589 */, 18 },
  /* 4735 */  { MAD_F(0x04da5b95) /* 0.303309995 */, 18 },

  /* 4736 */  { MAD_F(0x04dab524) /* 0.303395408 */, 18 },
  /* 4737 */  { MAD_F(0x04db0eb6) /* 0.303480826 */, 18 },
  /* 4738 */  { MAD_F(0x04db6849) /* 0.303566251 */, 18 },
  /* 4739 */  { MAD_F(0x04dbc1dd) /* 0.303651681 */, 18 },
  /* 4740 */  { MAD_F(0x04dc1b73) /* 0.303737117 */, 18 },
  /* 4741 */  { MAD_F(0x04dc750b) /* 0.303822560 */, 18 },
  /* 4742 */  { MAD_F(0x04dccea5) /* 0.303908008 */, 18 },
  /* 4743 */  { MAD_F(0x04dd2840) /* 0.303993463 */, 18 },
  /* 4744 */  { MAD_F(0x04dd81dc) /* 0.304078923 */, 18 },
  /* 4745 */  { MAD_F(0x04dddb7a) /* 0.304164390 */, 18 },
  /* 4746 */  { MAD_F(0x04de351a) /* 0.304249862 */, 18 },
  /* 4747 */  { MAD_F(0x04de8ebc) /* 0.304335340 */, 18 },
  /* 4748 */  { MAD_F(0x04dee85f) /* 0.304420825 */, 18 },
  /* 4749 */  { MAD_F(0x04df4203) /* 0.304506315 */, 18 },
  /* 4750 */  { MAD_F(0x04df9baa) /* 0.304591812 */, 18 },
  /* 4751 */  { MAD_F(0x04dff552) /* 0.304677314 */, 18 },

  /* 4752 */  { MAD_F(0x04e04efb) /* 0.304762823 */, 18 },
  /* 4753 */  { MAD_F(0x04e0a8a6) /* 0.304848337 */, 18 },
  /* 4754 */  { MAD_F(0x04e10253) /* 0.304933858 */, 18 },
  /* 4755 */  { MAD_F(0x04e15c01) /* 0.305019384 */, 18 },
  /* 4756 */  { MAD_F(0x04e1b5b1) /* 0.305104917 */, 18 },
  /* 4757 */  { MAD_F(0x04e20f63) /* 0.305190455 */, 18 },
  /* 4758 */  { MAD_F(0x04e26916) /* 0.305275999 */, 18 },
  /* 4759 */  { MAD_F(0x04e2c2cb) /* 0.305361550 */, 18 },
  /* 4760 */  { MAD_F(0x04e31c81) /* 0.305447106 */, 18 },
  /* 4761 */  { MAD_F(0x04e37639) /* 0.305532669 */, 18 },
  /* 4762 */  { MAD_F(0x04e3cff3) /* 0.305618237 */, 18 },
  /* 4763 */  { MAD_F(0x04e429ae) /* 0.305703811 */, 18 },
  /* 4764 */  { MAD_F(0x04e4836b) /* 0.305789392 */, 18 },
  /* 4765 */  { MAD_F(0x04e4dd29) /* 0.305874978 */, 18 },
  /* 4766 */  { MAD_F(0x04e536e9) /* 0.305960571 */, 18 },
  /* 4767 */  { MAD_F(0x04e590ab) /* 0.306046169 */, 18 },

  /* 4768 */  { MAD_F(0x04e5ea6e) /* 0.306131773 */, 18 },
  /* 4769 */  { MAD_F(0x04e64433) /* 0.306217383 */, 18 },
  /* 4770 */  { MAD_F(0x04e69df9) /* 0.306303000 */, 18 },
  /* 4771 */  { MAD_F(0x04e6f7c1) /* 0.306388622 */, 18 },
  /* 4772 */  { MAD_F(0x04e7518b) /* 0.306474250 */, 18 },
  /* 4773 */  { MAD_F(0x04e7ab56) /* 0.306559885 */, 18 },
  /* 4774 */  { MAD_F(0x04e80523) /* 0.306645525 */, 18 },
  /* 4775 */  { MAD_F(0x04e85ef2) /* 0.306731171 */, 18 },
  /* 4776 */  { MAD_F(0x04e8b8c2) /* 0.306816823 */, 18 },
  /* 4777 */  { MAD_F(0x04e91293) /* 0.306902481 */, 18 },
  /* 4778 */  { MAD_F(0x04e96c67) /* 0.306988145 */, 18 },
  /* 4779 */  { MAD_F(0x04e9c63b) /* 0.307073816 */, 18 },
  /* 4780 */  { MAD_F(0x04ea2012) /* 0.307159492 */, 18 },
  /* 4781 */  { MAD_F(0x04ea79ea) /* 0.307245174 */, 18 },
  /* 4782 */  { MAD_F(0x04ead3c4) /* 0.307330862 */, 18 },
  /* 4783 */  { MAD_F(0x04eb2d9f) /* 0.307416556 */, 18 },

  /* 4784 */  { MAD_F(0x04eb877c) /* 0.307502256 */, 18 },
  /* 4785 */  { MAD_F(0x04ebe15b) /* 0.307587962 */, 18 },
  /* 4786 */  { MAD_F(0x04ec3b3b) /* 0.307673674 */, 18 },
  /* 4787 */  { MAD_F(0x04ec951c) /* 0.307759392 */, 18 },
  /* 4788 */  { MAD_F(0x04ecef00) /* 0.307845115 */, 18 },
  /* 4789 */  { MAD_F(0x04ed48e5) /* 0.307930845 */, 18 },
  /* 4790 */  { MAD_F(0x04eda2cb) /* 0.308016581 */, 18 },
  /* 4791 */  { MAD_F(0x04edfcb3) /* 0.308102323 */, 18 },
  /* 4792 */  { MAD_F(0x04ee569d) /* 0.308188071 */, 18 },
  /* 4793 */  { MAD_F(0x04eeb088) /* 0.308273824 */, 18 },
  /* 4794 */  { MAD_F(0x04ef0a75) /* 0.308359584 */, 18 },
  /* 4795 */  { MAD_F(0x04ef6464) /* 0.308445350 */, 18 },
  /* 4796 */  { MAD_F(0x04efbe54) /* 0.308531121 */, 18 },
  /* 4797 */  { MAD_F(0x04f01846) /* 0.308616899 */, 18 },
  /* 4798 */  { MAD_F(0x04f07239) /* 0.308702682 */, 18 },
  /* 4799 */  { MAD_F(0x04f0cc2e) /* 0.308788472 */, 18 },

  /* 4800 */  { MAD_F(0x04f12624) /* 0.308874267 */, 18 },
  /* 4801 */  { MAD_F(0x04f1801d) /* 0.308960068 */, 18 },
  /* 4802 */  { MAD_F(0x04f1da16) /* 0.309045876 */, 18 },
  /* 4803 */  { MAD_F(0x04f23412) /* 0.309131689 */, 18 },
  /* 4804 */  { MAD_F(0x04f28e0f) /* 0.309217508 */, 18 },
  /* 4805 */  { MAD_F(0x04f2e80d) /* 0.309303334 */, 18 },
  /* 4806 */  { MAD_F(0x04f3420d) /* 0.309389165 */, 18 },
  /* 4807 */  { MAD_F(0x04f39c0f) /* 0.309475002 */, 18 },
  /* 4808 */  { MAD_F(0x04f3f612) /* 0.309560845 */, 18 },
  /* 4809 */  { MAD_F(0x04f45017) /* 0.309646694 */, 18 },
  /* 4810 */  { MAD_F(0x04f4aa1e) /* 0.309732549 */, 18 },
  /* 4811 */  { MAD_F(0x04f50426) /* 0.309818410 */, 18 },
  /* 4812 */  { MAD_F(0x04f55e30) /* 0.309904277 */, 18 },
  /* 4813 */  { MAD_F(0x04f5b83b) /* 0.309990150 */, 18 },
  /* 4814 */  { MAD_F(0x04f61248) /* 0.310076028 */, 18 },
  /* 4815 */  { MAD_F(0x04f66c56) /* 0.310161913 */, 18 },

  /* 4816 */  { MAD_F(0x04f6c666) /* 0.310247804 */, 18 },
  /* 4817 */  { MAD_F(0x04f72078) /* 0.310333700 */, 18 },
  /* 4818 */  { MAD_F(0x04f77a8b) /* 0.310419603 */, 18 },
  /* 4819 */  { MAD_F(0x04f7d4a0) /* 0.310505511 */, 18 },
  /* 4820 */  { MAD_F(0x04f82eb7) /* 0.310591426 */, 18 },
  /* 4821 */  { MAD_F(0x04f888cf) /* 0.310677346 */, 18 },
  /* 4822 */  { MAD_F(0x04f8e2e9) /* 0.310763272 */, 18 },
  /* 4823 */  { MAD_F(0x04f93d04) /* 0.310849205 */, 18 },
  /* 4824 */  { MAD_F(0x04f99721) /* 0.310935143 */, 18 },
  /* 4825 */  { MAD_F(0x04f9f13f) /* 0.311021087 */, 18 },
  /* 4826 */  { MAD_F(0x04fa4b5f) /* 0.311107037 */, 18 },
  /* 4827 */  { MAD_F(0x04faa581) /* 0.311192993 */, 18 },
  /* 4828 */  { MAD_F(0x04faffa4) /* 0.311278955 */, 18 },
  /* 4829 */  { MAD_F(0x04fb59c9) /* 0.311364923 */, 18 },
  /* 4830 */  { MAD_F(0x04fbb3ef) /* 0.311450897 */, 18 },
  /* 4831 */  { MAD_F(0x04fc0e17) /* 0.311536877 */, 18 },

  /* 4832 */  { MAD_F(0x04fc6841) /* 0.311622862 */, 18 },
  /* 4833 */  { MAD_F(0x04fcc26c) /* 0.311708854 */, 18 },
  /* 4834 */  { MAD_F(0x04fd1c99) /* 0.311794851 */, 18 },
  /* 4835 */  { MAD_F(0x04fd76c7) /* 0.311880855 */, 18 },
  /* 4836 */  { MAD_F(0x04fdd0f7) /* 0.311966864 */, 18 },
  /* 4837 */  { MAD_F(0x04fe2b29) /* 0.312052880 */, 18 },
  /* 4838 */  { MAD_F(0x04fe855c) /* 0.312138901 */, 18 },
  /* 4839 */  { MAD_F(0x04fedf91) /* 0.312224928 */, 18 },
  /* 4840 */  { MAD_F(0x04ff39c7) /* 0.312310961 */, 18 },
  /* 4841 */  { MAD_F(0x04ff93ff) /* 0.312397000 */, 18 },
  /* 4842 */  { MAD_F(0x04ffee38) /* 0.312483045 */, 18 },
  /* 4843 */  { MAD_F(0x05004874) /* 0.312569096 */, 18 },
  /* 4844 */  { MAD_F(0x0500a2b0) /* 0.312655153 */, 18 },
  /* 4845 */  { MAD_F(0x0500fcef) /* 0.312741216 */, 18 },
  /* 4846 */  { MAD_F(0x0501572e) /* 0.312827284 */, 18 },
  /* 4847 */  { MAD_F(0x0501b170) /* 0.312913359 */, 18 },

  /* 4848 */  { MAD_F(0x05020bb3) /* 0.312999439 */, 18 },
  /* 4849 */  { MAD_F(0x050265f8) /* 0.313085526 */, 18 },
  /* 4850 */  { MAD_F(0x0502c03e) /* 0.313171618 */, 18 },
  /* 4851 */  { MAD_F(0x05031a86) /* 0.313257716 */, 18 },
  /* 4852 */  { MAD_F(0x050374cf) /* 0.313343820 */, 18 },
  /* 4853 */  { MAD_F(0x0503cf1a) /* 0.313429931 */, 18 },
  /* 4854 */  { MAD_F(0x05042967) /* 0.313516047 */, 18 },
  /* 4855 */  { MAD_F(0x050483b5) /* 0.313602168 */, 18 },
  /* 4856 */  { MAD_F(0x0504de05) /* 0.313688296 */, 18 },
  /* 4857 */  { MAD_F(0x05053856) /* 0.313774430 */, 18 },
  /* 4858 */  { MAD_F(0x050592a9) /* 0.313860570 */, 18 },
  /* 4859 */  { MAD_F(0x0505ecfd) /* 0.313946715 */, 18 },
  /* 4860 */  { MAD_F(0x05064754) /* 0.314032867 */, 18 },
  /* 4861 */  { MAD_F(0x0506a1ab) /* 0.314119024 */, 18 },
  /* 4862 */  { MAD_F(0x0506fc04) /* 0.314205187 */, 18 },
  /* 4863 */  { MAD_F(0x0507565f) /* 0.314291357 */, 18 },

  /* 4864 */  { MAD_F(0x0507b0bc) /* 0.314377532 */, 18 },
  /* 4865 */  { MAD_F(0x05080b1a) /* 0.314463713 */, 18 },
  /* 4866 */  { MAD_F(0x05086579) /* 0.314549900 */, 18 },
  /* 4867 */  { MAD_F(0x0508bfdb) /* 0.314636092 */, 18 },
  /* 4868 */  { MAD_F(0x05091a3d) /* 0.314722291 */, 18 },
  /* 4869 */  { MAD_F(0x050974a2) /* 0.314808496 */, 18 },
  /* 4870 */  { MAD_F(0x0509cf08) /* 0.314894706 */, 18 },
  /* 4871 */  { MAD_F(0x050a296f) /* 0.314980923 */, 18 },
  /* 4872 */  { MAD_F(0x050a83d8) /* 0.315067145 */, 18 },
  /* 4873 */  { MAD_F(0x050ade43) /* 0.315153373 */, 18 },
  /* 4874 */  { MAD_F(0x050b38af) /* 0.315239607 */, 18 },
  /* 4875 */  { MAD_F(0x050b931d) /* 0.315325847 */, 18 },
  /* 4876 */  { MAD_F(0x050bed8d) /* 0.315412093 */, 18 },
  /* 4877 */  { MAD_F(0x050c47fe) /* 0.315498345 */, 18 },
  /* 4878 */  { MAD_F(0x050ca271) /* 0.315584603 */, 18 },
  /* 4879 */  { MAD_F(0x050cfce5) /* 0.315670866 */, 18 },

  /* 4880 */  { MAD_F(0x050d575b) /* 0.315757136 */, 18 },
  /* 4881 */  { MAD_F(0x050db1d2) /* 0.315843411 */, 18 },
  /* 4882 */  { MAD_F(0x050e0c4b) /* 0.315929693 */, 18 },
  /* 4883 */  { MAD_F(0x050e66c5) /* 0.316015980 */, 18 },
  /* 4884 */  { MAD_F(0x050ec141) /* 0.316102273 */, 18 },
  /* 4885 */  { MAD_F(0x050f1bbf) /* 0.316188572 */, 18 },
  /* 4886 */  { MAD_F(0x050f763e) /* 0.316274877 */, 18 },
  /* 4887 */  { MAD_F(0x050fd0bf) /* 0.316361187 */, 18 },
  /* 4888 */  { MAD_F(0x05102b42) /* 0.316447504 */, 18 },
  /* 4889 */  { MAD_F(0x051085c6) /* 0.316533826 */, 18 },
  /* 4890 */  { MAD_F(0x0510e04b) /* 0.316620155 */, 18 },
  /* 4891 */  { MAD_F(0x05113ad3) /* 0.316706489 */, 18 },
  /* 4892 */  { MAD_F(0x0511955b) /* 0.316792829 */, 18 },
  /* 4893 */  { MAD_F(0x0511efe6) /* 0.316879175 */, 18 },
  /* 4894 */  { MAD_F(0x05124a72) /* 0.316965527 */, 18 },
  /* 4895 */  { MAD_F(0x0512a4ff) /* 0.317051885 */, 18 },

  /* 4896 */  { MAD_F(0x0512ff8e) /* 0.317138249 */, 18 },
  /* 4897 */  { MAD_F(0x05135a1f) /* 0.317224618 */, 18 },
  /* 4898 */  { MAD_F(0x0513b4b1) /* 0.317310994 */, 18 },
  /* 4899 */  { MAD_F(0x05140f45) /* 0.317397375 */, 18 },
  /* 4900 */  { MAD_F(0x051469da) /* 0.317483762 */, 18 },
  /* 4901 */  { MAD_F(0x0514c471) /* 0.317570155 */, 18 },
  /* 4902 */  { MAD_F(0x05151f0a) /* 0.317656554 */, 18 },
  /* 4903 */  { MAD_F(0x051579a4) /* 0.317742959 */, 18 },
  /* 4904 */  { MAD_F(0x0515d440) /* 0.317829370 */, 18 },
  /* 4905 */  { MAD_F(0x05162edd) /* 0.317915786 */, 18 },
  /* 4906 */  { MAD_F(0x0516897c) /* 0.318002209 */, 18 },
  /* 4907 */  { MAD_F(0x0516e41c) /* 0.318088637 */, 18 },
  /* 4908 */  { MAD_F(0x05173ebe) /* 0.318175071 */, 18 },
  /* 4909 */  { MAD_F(0x05179962) /* 0.318261511 */, 18 },
  /* 4910 */  { MAD_F(0x0517f407) /* 0.318347957 */, 18 },
  /* 4911 */  { MAD_F(0x05184eae) /* 0.318434409 */, 18 },

  /* 4912 */  { MAD_F(0x0518a956) /* 0.318520867 */, 18 },
  /* 4913 */  { MAD_F(0x05190400) /* 0.318607330 */, 18 },
  /* 4914 */  { MAD_F(0x05195eab) /* 0.318693800 */, 18 },
  /* 4915 */  { MAD_F(0x0519b958) /* 0.318780275 */, 18 },
  /* 4916 */  { MAD_F(0x051a1407) /* 0.318866756 */, 18 },
  /* 4917 */  { MAD_F(0x051a6eb7) /* 0.318953243 */, 18 },
  /* 4918 */  { MAD_F(0x051ac969) /* 0.319039736 */, 18 },
  /* 4919 */  { MAD_F(0x051b241c) /* 0.319126235 */, 18 },
  /* 4920 */  { MAD_F(0x051b7ed1) /* 0.319212739 */, 18 },
  /* 4921 */  { MAD_F(0x051bd987) /* 0.319299250 */, 18 },
  /* 4922 */  { MAD_F(0x051c3440) /* 0.319385766 */, 18 },
  /* 4923 */  { MAD_F(0x051c8ef9) /* 0.319472288 */, 18 },
  /* 4924 */  { MAD_F(0x051ce9b4) /* 0.319558816 */, 18 },
  /* 4925 */  { MAD_F(0x051d4471) /* 0.319645350 */, 18 },
  /* 4926 */  { MAD_F(0x051d9f2f) /* 0.319731890 */, 18 },
  /* 4927 */  { MAD_F(0x051df9ef) /* 0.319818435 */, 18 },

  /* 4928 */  { MAD_F(0x051e54b1) /* 0.319904987 */, 18 },
  /* 4929 */  { MAD_F(0x051eaf74) /* 0.319991544 */, 18 },
  /* 4930 */  { MAD_F(0x051f0a38) /* 0.320078107 */, 18 },
  /* 4931 */  { MAD_F(0x051f64ff) /* 0.320164676 */, 18 },
  /* 4932 */  { MAD_F(0x051fbfc6) /* 0.320251251 */, 18 },
  /* 4933 */  { MAD_F(0x05201a90) /* 0.320337832 */, 18 },
  /* 4934 */  { MAD_F(0x0520755b) /* 0.320424419 */, 18 },
  /* 4935 */  { MAD_F(0x0520d027) /* 0.320511011 */, 18 },
  /* 4936 */  { MAD_F(0x05212af5) /* 0.320597609 */, 18 },
  /* 4937 */  { MAD_F(0x052185c5) /* 0.320684213 */, 18 },
  /* 4938 */  { MAD_F(0x0521e096) /* 0.320770823 */, 18 },
  /* 4939 */  { MAD_F(0x05223b69) /* 0.320857439 */, 18 },
  /* 4940 */  { MAD_F(0x0522963d) /* 0.320944061 */, 18 },
  /* 4941 */  { MAD_F(0x0522f113) /* 0.321030688 */, 18 },
  /* 4942 */  { MAD_F(0x05234bea) /* 0.321117322 */, 18 },
  /* 4943 */  { MAD_F(0x0523a6c3) /* 0.321203961 */, 18 },

  /* 4944 */  { MAD_F(0x0524019e) /* 0.321290606 */, 18 },
  /* 4945 */  { MAD_F(0x05245c7a) /* 0.321377257 */, 18 },
  /* 4946 */  { MAD_F(0x0524b758) /* 0.321463913 */, 18 },
  /* 4947 */  { MAD_F(0x05251237) /* 0.321550576 */, 18 },
  /* 4948 */  { MAD_F(0x05256d18) /* 0.321637244 */, 18 },
  /* 4949 */  { MAD_F(0x0525c7fb) /* 0.321723919 */, 18 },
  /* 4950 */  { MAD_F(0x052622df) /* 0.321810599 */, 18 },
  /* 4951 */  { MAD_F(0x05267dc4) /* 0.321897285 */, 18 },
  /* 4952 */  { MAD_F(0x0526d8ab) /* 0.321983976 */, 18 },
  /* 4953 */  { MAD_F(0x05273394) /* 0.322070674 */, 18 },
  /* 4954 */  { MAD_F(0x05278e7e) /* 0.322157377 */, 18 },
  /* 4955 */  { MAD_F(0x0527e96a) /* 0.322244087 */, 18 },
  /* 4956 */  { MAD_F(0x05284457) /* 0.322330802 */, 18 },
  /* 4957 */  { MAD_F(0x05289f46) /* 0.322417523 */, 18 },
  /* 4958 */  { MAD_F(0x0528fa37) /* 0.322504249 */, 18 },
  /* 4959 */  { MAD_F(0x05295529) /* 0.322590982 */, 18 },

  /* 4960 */  { MAD_F(0x0529b01d) /* 0.322677720 */, 18 },
  /* 4961 */  { MAD_F(0x052a0b12) /* 0.322764465 */, 18 },
  /* 4962 */  { MAD_F(0x052a6609) /* 0.322851215 */, 18 },
  /* 4963 */  { MAD_F(0x052ac101) /* 0.322937971 */, 18 },
  /* 4964 */  { MAD_F(0x052b1bfb) /* 0.323024732 */, 18 },
  /* 4965 */  { MAD_F(0x052b76f7) /* 0.323111500 */, 18 },
  /* 4966 */  { MAD_F(0x052bd1f4) /* 0.323198273 */, 18 },
  /* 4967 */  { MAD_F(0x052c2cf2) /* 0.323285052 */, 18 },
  /* 4968 */  { MAD_F(0x052c87f2) /* 0.323371837 */, 18 },
  /* 4969 */  { MAD_F(0x052ce2f4) /* 0.323458628 */, 18 },
  /* 4970 */  { MAD_F(0x052d3df7) /* 0.323545425 */, 18 },
  /* 4971 */  { MAD_F(0x052d98fc) /* 0.323632227 */, 18 },
  /* 4972 */  { MAD_F(0x052df403) /* 0.323719036 */, 18 },
  /* 4973 */  { MAD_F(0x052e4f0b) /* 0.323805850 */, 18 },
  /* 4974 */  { MAD_F(0x052eaa14) /* 0.323892670 */, 18 },
  /* 4975 */  { MAD_F(0x052f051f) /* 0.323979496 */, 18 },

  /* 4976 */  { MAD_F(0x052f602c) /* 0.324066327 */, 18 },
  /* 4977 */  { MAD_F(0x052fbb3a) /* 0.324153165 */, 18 },
  /* 4978 */  { MAD_F(0x0530164a) /* 0.324240008 */, 18 },
  /* 4979 */  { MAD_F(0x0530715b) /* 0.324326857 */, 18 },
  /* 4980 */  { MAD_F(0x0530cc6e) /* 0.324413712 */, 18 },
  /* 4981 */  { MAD_F(0x05312783) /* 0.324500572 */, 18 },
  /* 4982 */  { MAD_F(0x05318299) /* 0.324587439 */, 18 },
  /* 4983 */  { MAD_F(0x0531ddb0) /* 0.324674311 */, 18 },
  /* 4984 */  { MAD_F(0x053238ca) /* 0.324761189 */, 18 },
  /* 4985 */  { MAD_F(0x053293e4) /* 0.324848073 */, 18 },
  /* 4986 */  { MAD_F(0x0532ef01) /* 0.324934963 */, 18 },
  /* 4987 */  { MAD_F(0x05334a1e) /* 0.325021858 */, 18 },
  /* 4988 */  { MAD_F(0x0533a53e) /* 0.325108760 */, 18 },
  /* 4989 */  { MAD_F(0x0534005f) /* 0.325195667 */, 18 },
  /* 4990 */  { MAD_F(0x05345b81) /* 0.325282580 */, 18 },
  /* 4991 */  { MAD_F(0x0534b6a5) /* 0.325369498 */, 18 },

  /* 4992 */  { MAD_F(0x053511cb) /* 0.325456423 */, 18 },
  /* 4993 */  { MAD_F(0x05356cf2) /* 0.325543353 */, 18 },
  /* 4994 */  { MAD_F(0x0535c81b) /* 0.325630290 */, 18 },
  /* 4995 */  { MAD_F(0x05362345) /* 0.325717232 */, 18 },
  /* 4996 */  { MAD_F(0x05367e71) /* 0.325804179 */, 18 },
  /* 4997 */  { MAD_F(0x0536d99f) /* 0.325891133 */, 18 },
  /* 4998 */  { MAD_F(0x053734ce) /* 0.325978092 */, 18 },
  /* 4999 */  { MAD_F(0x05378ffe) /* 0.326065057 */, 18 },
  /* 5000 */  { MAD_F(0x0537eb30) /* 0.326152028 */, 18 },
  /* 5001 */  { MAD_F(0x05384664) /* 0.326239005 */, 18 },
  /* 5002 */  { MAD_F(0x0538a199) /* 0.326325988 */, 18 },
  /* 5003 */  { MAD_F(0x0538fcd0) /* 0.326412976 */, 18 },
  /* 5004 */  { MAD_F(0x05395808) /* 0.326499970 */, 18 },
  /* 5005 */  { MAD_F(0x0539b342) /* 0.326586970 */, 18 },
  /* 5006 */  { MAD_F(0x053a0e7d) /* 0.326673976 */, 18 },
  /* 5007 */  { MAD_F(0x053a69ba) /* 0.326760988 */, 18 },

  /* 5008 */  { MAD_F(0x053ac4f9) /* 0.326848005 */, 18 },
  /* 5009 */  { MAD_F(0x053b2039) /* 0.326935028 */, 18 },
  /* 5010 */  { MAD_F(0x053b7b7b) /* 0.327022057 */, 18 },
  /* 5011 */  { MAD_F(0x053bd6be) /* 0.327109092 */, 18 },
  /* 5012 */  { MAD_F(0x053c3203) /* 0.327196132 */, 18 },
  /* 5013 */  { MAD_F(0x053c8d49) /* 0.327283178 */, 18 },
  /* 5014 */  { MAD_F(0x053ce891) /* 0.327370231 */, 18 },
  /* 5015 */  { MAD_F(0x053d43da) /* 0.327457288 */, 18 },
  /* 5016 */  { MAD_F(0x053d9f25) /* 0.327544352 */, 18 },
  /* 5017 */  { MAD_F(0x053dfa72) /* 0.327631421 */, 18 },
  /* 5018 */  { MAD_F(0x053e55c0) /* 0.327718497 */, 18 },
  /* 5019 */  { MAD_F(0x053eb10f) /* 0.327805578 */, 18 },
  /* 5020 */  { MAD_F(0x053f0c61) /* 0.327892665 */, 18 },
  /* 5021 */  { MAD_F(0x053f67b3) /* 0.327979757 */, 18 },
  /* 5022 */  { MAD_F(0x053fc308) /* 0.328066855 */, 18 },
  /* 5023 */  { MAD_F(0x05401e5e) /* 0.328153960 */, 18 },

  /* 5024 */  { MAD_F(0x054079b5) /* 0.328241070 */, 18 },
  /* 5025 */  { MAD_F(0x0540d50e) /* 0.328328185 */, 18 },
  /* 5026 */  { MAD_F(0x05413068) /* 0.328415307 */, 18 },
  /* 5027 */  { MAD_F(0x05418bc4) /* 0.328502434 */, 18 },
  /* 5028 */  { MAD_F(0x0541e722) /* 0.328589567 */, 18 },
  /* 5029 */  { MAD_F(0x05424281) /* 0.328676706 */, 18 },
  /* 5030 */  { MAD_F(0x05429de2) /* 0.328763850 */, 18 },
  /* 5031 */  { MAD_F(0x0542f944) /* 0.328851001 */, 18 },
  /* 5032 */  { MAD_F(0x054354a8) /* 0.328938157 */, 18 },
  /* 5033 */  { MAD_F(0x0543b00d) /* 0.329025319 */, 18 },
  /* 5034 */  { MAD_F(0x05440b74) /* 0.329112486 */, 18 },
  /* 5035 */  { MAD_F(0x054466dd) /* 0.329199660 */, 18 },
  /* 5036 */  { MAD_F(0x0544c247) /* 0.329286839 */, 18 },
  /* 5037 */  { MAD_F(0x05451db2) /* 0.329374024 */, 18 },
  /* 5038 */  { MAD_F(0x0545791f) /* 0.329461215 */, 18 },
  /* 5039 */  { MAD_F(0x0545d48e) /* 0.329548411 */, 18 },

  /* 5040 */  { MAD_F(0x05462ffe) /* 0.329635614 */, 18 },
  /* 5041 */  { MAD_F(0x05468b70) /* 0.329722822 */, 18 },
  /* 5042 */  { MAD_F(0x0546e6e3) /* 0.329810036 */, 18 },
  /* 5043 */  { MAD_F(0x05474258) /* 0.329897255 */, 18 },
  /* 5044 */  { MAD_F(0x05479dce) /* 0.329984481 */, 18 },
  /* 5045 */  { MAD_F(0x0547f946) /* 0.330071712 */, 18 },
  /* 5046 */  { MAD_F(0x054854c0) /* 0.330158949 */, 18 },
  /* 5047 */  { MAD_F(0x0548b03b) /* 0.330246191 */, 18 },
  /* 5048 */  { MAD_F(0x05490bb7) /* 0.330333440 */, 18 },
  /* 5049 */  { MAD_F(0x05496735) /* 0.330420694 */, 18 },
  /* 5050 */  { MAD_F(0x0549c2b5) /* 0.330507954 */, 18 },
  /* 5051 */  { MAD_F(0x054a1e36) /* 0.330595220 */, 18 },
  /* 5052 */  { MAD_F(0x054a79b9) /* 0.330682491 */, 18 },
  /* 5053 */  { MAD_F(0x054ad53d) /* 0.330769768 */, 18 },
  /* 5054 */  { MAD_F(0x054b30c3) /* 0.330857051 */, 18 },
  /* 5055 */  { MAD_F(0x054b8c4b) /* 0.330944340 */, 18 },

  /* 5056 */  { MAD_F(0x054be7d4) /* 0.331031635 */, 18 },
  /* 5057 */  { MAD_F(0x054c435e) /* 0.331118935 */, 18 },
  /* 5058 */  { MAD_F(0x054c9eea) /* 0.331206241 */, 18 },
  /* 5059 */  { MAD_F(0x054cfa78) /* 0.331293553 */, 18 },
  /* 5060 */  { MAD_F(0x054d5607) /* 0.331380870 */, 18 },
  /* 5061 */  { MAD_F(0x054db197) /* 0.331468193 */, 18 },
  /* 5062 */  { MAD_F(0x054e0d2a) /* 0.331555522 */, 18 },
  /* 5063 */  { MAD_F(0x054e68bd) /* 0.331642857 */, 18 },
  /* 5064 */  { MAD_F(0x054ec453) /* 0.331730198 */, 18 },
  /* 5065 */  { MAD_F(0x054f1fe9) /* 0.331817544 */, 18 },
  /* 5066 */  { MAD_F(0x054f7b82) /* 0.331904896 */, 18 },
  /* 5067 */  { MAD_F(0x054fd71c) /* 0.331992254 */, 18 },
  /* 5068 */  { MAD_F(0x055032b7) /* 0.332079617 */, 18 },
  /* 5069 */  { MAD_F(0x05508e54) /* 0.332166986 */, 18 },
  /* 5070 */  { MAD_F(0x0550e9f3) /* 0.332254361 */, 18 },
  /* 5071 */  { MAD_F(0x05514593) /* 0.332341742 */, 18 },

  /* 5072 */  { MAD_F(0x0551a134) /* 0.332429129 */, 18 },
  /* 5073 */  { MAD_F(0x0551fcd8) /* 0.332516521 */, 18 },
  /* 5074 */  { MAD_F(0x0552587c) /* 0.332603919 */, 18 },
  /* 5075 */  { MAD_F(0x0552b423) /* 0.332691323 */, 18 },
  /* 5076 */  { MAD_F(0x05530fca) /* 0.332778732 */, 18 },
  /* 5077 */  { MAD_F(0x05536b74) /* 0.332866147 */, 18 },
  /* 5078 */  { MAD_F(0x0553c71f) /* 0.332953568 */, 18 },
  /* 5079 */  { MAD_F(0x055422cb) /* 0.333040995 */, 18 },
  /* 5080 */  { MAD_F(0x05547e79) /* 0.333128427 */, 18 },
  /* 5081 */  { MAD_F(0x0554da29) /* 0.333215865 */, 18 },
  /* 5082 */  { MAD_F(0x055535da) /* 0.333303309 */, 18 },
  /* 5083 */  { MAD_F(0x0555918c) /* 0.333390759 */, 18 },
  /* 5084 */  { MAD_F(0x0555ed40) /* 0.333478214 */, 18 },
  /* 5085 */  { MAD_F(0x055648f6) /* 0.333565675 */, 18 },
  /* 5086 */  { MAD_F(0x0556a4ad) /* 0.333653142 */, 18 },
  /* 5087 */  { MAD_F(0x05570066) /* 0.333740615 */, 18 },

  /* 5088 */  { MAD_F(0x05575c20) /* 0.333828093 */, 18 },
  /* 5089 */  { MAD_F(0x0557b7dc) /* 0.333915577 */, 18 },
  /* 5090 */  { MAD_F(0x05581399) /* 0.334003067 */, 18 },
  /* 5091 */  { MAD_F(0x05586f58) /* 0.334090562 */, 18 },
  /* 5092 */  { MAD_F(0x0558cb19) /* 0.334178063 */, 18 },
  /* 5093 */  { MAD_F(0x055926db) /* 0.334265570 */, 18 },
  /* 5094 */  { MAD_F(0x0559829e) /* 0.334353083 */, 18 },
  /* 5095 */  { MAD_F(0x0559de63) /* 0.334440601 */, 18 },
  /* 5096 */  { MAD_F(0x055a3a2a) /* 0.334528126 */, 18 },
  /* 5097 */  { MAD_F(0x055a95f2) /* 0.334615655 */, 18 },
  /* 5098 */  { MAD_F(0x055af1bb) /* 0.334703191 */, 18 },
  /* 5099 */  { MAD_F(0x055b4d87) /* 0.334790732 */, 18 },
  /* 5100 */  { MAD_F(0x055ba953) /* 0.334878279 */, 18 },
  /* 5101 */  { MAD_F(0x055c0522) /* 0.334965832 */, 18 },
  /* 5102 */  { MAD_F(0x055c60f1) /* 0.335053391 */, 18 },
  /* 5103 */  { MAD_F(0x055cbcc3) /* 0.335140955 */, 18 },

  /* 5104 */  { MAD_F(0x055d1896) /* 0.335228525 */, 18 },
  /* 5105 */  { MAD_F(0x055d746a) /* 0.335316100 */, 18 },
  /* 5106 */  { MAD_F(0x055dd040) /* 0.335403682 */, 18 },
  /* 5107 */  { MAD_F(0x055e2c17) /* 0.335491269 */, 18 },
  /* 5108 */  { MAD_F(0x055e87f0) /* 0.335578861 */, 18 },
  /* 5109 */  { MAD_F(0x055ee3cb) /* 0.335666460 */, 18 },
  /* 5110 */  { MAD_F(0x055f3fa7) /* 0.335754064 */, 18 },
  /* 5111 */  { MAD_F(0x055f9b85) /* 0.335841674 */, 18 },
  /* 5112 */  { MAD_F(0x055ff764) /* 0.335929290 */, 18 },
  /* 5113 */  { MAD_F(0x05605344) /* 0.336016911 */, 18 },
  /* 5114 */  { MAD_F(0x0560af27) /* 0.336104538 */, 18 },
  /* 5115 */  { MAD_F(0x05610b0a) /* 0.336192171 */, 18 },
  /* 5116 */  { MAD_F(0x056166f0) /* 0.336279809 */, 18 },
  /* 5117 */  { MAD_F(0x0561c2d7) /* 0.336367453 */, 18 },
  /* 5118 */  { MAD_F(0x05621ebf) /* 0.336455103 */, 18 },
  /* 5119 */  { MAD_F(0x05627aa9) /* 0.336542759 */, 18 },

  /* 5120 */  { MAD_F(0x0562d694) /* 0.336630420 */, 18 },
  /* 5121 */  { MAD_F(0x05633281) /* 0.336718087 */, 18 },
  /* 5122 */  { MAD_F(0x05638e70) /* 0.336805760 */, 18 },
  /* 5123 */  { MAD_F(0x0563ea60) /* 0.336893439 */, 18 },
  /* 5124 */  { MAD_F(0x05644651) /* 0.336981123 */, 18 },
  /* 5125 */  { MAD_F(0x0564a244) /* 0.337068813 */, 18 },
  /* 5126 */  { MAD_F(0x0564fe39) /* 0.337156508 */, 18 },
  /* 5127 */  { MAD_F(0x05655a2f) /* 0.337244209 */, 18 },
  /* 5128 */  { MAD_F(0x0565b627) /* 0.337331916 */, 18 },
  /* 5129 */  { MAD_F(0x05661220) /* 0.337419629 */, 18 },
  /* 5130 */  { MAD_F(0x05666e1a) /* 0.337507347 */, 18 },
  /* 5131 */  { MAD_F(0x0566ca17) /* 0.337595071 */, 18 },
  /* 5132 */  { MAD_F(0x05672614) /* 0.337682801 */, 18 },
  /* 5133 */  { MAD_F(0x05678214) /* 0.337770537 */, 18 },
  /* 5134 */  { MAD_F(0x0567de15) /* 0.337858278 */, 18 },
  /* 5135 */  { MAD_F(0x05683a17) /* 0.337946025 */, 18 },

  /* 5136 */  { MAD_F(0x0568961b) /* 0.338033777 */, 18 },
  /* 5137 */  { MAD_F(0x0568f220) /* 0.338121535 */, 18 },
  /* 5138 */  { MAD_F(0x05694e27) /* 0.338209299 */, 18 },
  /* 5139 */  { MAD_F(0x0569aa30) /* 0.338297069 */, 18 },
  /* 5140 */  { MAD_F(0x056a063a) /* 0.338384844 */, 18 },
  /* 5141 */  { MAD_F(0x056a6245) /* 0.338472625 */, 18 },
  /* 5142 */  { MAD_F(0x056abe52) /* 0.338560412 */, 18 },
  /* 5143 */  { MAD_F(0x056b1a61) /* 0.338648204 */, 18 },
  /* 5144 */  { MAD_F(0x056b7671) /* 0.338736002 */, 18 },
  /* 5145 */  { MAD_F(0x056bd283) /* 0.338823806 */, 18 },
  /* 5146 */  { MAD_F(0x056c2e96) /* 0.338911616 */, 18 },
  /* 5147 */  { MAD_F(0x056c8aab) /* 0.338999431 */, 18 },
  /* 5148 */  { MAD_F(0x056ce6c1) /* 0.339087252 */, 18 },
  /* 5149 */  { MAD_F(0x056d42d9) /* 0.339175078 */, 18 },
  /* 5150 */  { MAD_F(0x056d9ef2) /* 0.339262910 */, 18 },
  /* 5151 */  { MAD_F(0x056dfb0d) /* 0.339350748 */, 18 },

  /* 5152 */  { MAD_F(0x056e5729) /* 0.339438592 */, 18 },
  /* 5153 */  { MAD_F(0x056eb347) /* 0.339526441 */, 18 },
  /* 5154 */  { MAD_F(0x056f0f66) /* 0.339614296 */, 18 },
  /* 5155 */  { MAD_F(0x056f6b87) /* 0.339702157 */, 18 },
  /* 5156 */  { MAD_F(0x056fc7aa) /* 0.339790023 */, 18 },
  /* 5157 */  { MAD_F(0x057023cd) /* 0.339877895 */, 18 },
  /* 5158 */  { MAD_F(0x05707ff3) /* 0.339965773 */, 18 },
  /* 5159 */  { MAD_F(0x0570dc1a) /* 0.340053656 */, 18 },
  /* 5160 */  { MAD_F(0x05713843) /* 0.340141545 */, 18 },
  /* 5161 */  { MAD_F(0x0571946d) /* 0.340229440 */, 18 },
  /* 5162 */  { MAD_F(0x0571f098) /* 0.340317340 */, 18 },
  /* 5163 */  { MAD_F(0x05724cc5) /* 0.340405246 */, 18 },
  /* 5164 */  { MAD_F(0x0572a8f4) /* 0.340493158 */, 18 },
  /* 5165 */  { MAD_F(0x05730524) /* 0.340581075 */, 18 },
  /* 5166 */  { MAD_F(0x05736156) /* 0.340668999 */, 18 },
  /* 5167 */  { MAD_F(0x0573bd89) /* 0.340756927 */, 18 },

  /* 5168 */  { MAD_F(0x057419be) /* 0.340844862 */, 18 },
  /* 5169 */  { MAD_F(0x057475f4) /* 0.340932802 */, 18 },
  /* 5170 */  { MAD_F(0x0574d22c) /* 0.341020748 */, 18 },
  /* 5171 */  { MAD_F(0x05752e65) /* 0.341108699 */, 18 },
  /* 5172 */  { MAD_F(0x05758aa0) /* 0.341196656 */, 18 },
  /* 5173 */  { MAD_F(0x0575e6dc) /* 0.341284619 */, 18 },
  /* 5174 */  { MAD_F(0x0576431a) /* 0.341372587 */, 18 },
  /* 5175 */  { MAD_F(0x05769f59) /* 0.341460562 */, 18 },
  /* 5176 */  { MAD_F(0x0576fb9a) /* 0.341548541 */, 18 },
  /* 5177 */  { MAD_F(0x057757dd) /* 0.341636527 */, 18 },
  /* 5178 */  { MAD_F(0x0577b421) /* 0.341724518 */, 18 },
  /* 5179 */  { MAD_F(0x05781066) /* 0.341812515 */, 18 },
  /* 5180 */  { MAD_F(0x05786cad) /* 0.341900517 */, 18 },
  /* 5181 */  { MAD_F(0x0578c8f5) /* 0.341988525 */, 18 },
  /* 5182 */  { MAD_F(0x0579253f) /* 0.342076539 */, 18 },
  /* 5183 */  { MAD_F(0x0579818b) /* 0.342164558 */, 18 },

  /* 5184 */  { MAD_F(0x0579ddd8) /* 0.342252584 */, 18 },
  /* 5185 */  { MAD_F(0x057a3a27) /* 0.342340614 */, 18 },
  /* 5186 */  { MAD_F(0x057a9677) /* 0.342428651 */, 18 },
  /* 5187 */  { MAD_F(0x057af2c8) /* 0.342516693 */, 18 },
  /* 5188 */  { MAD_F(0x057b4f1c) /* 0.342604741 */, 18 },
  /* 5189 */  { MAD_F(0x057bab70) /* 0.342692794 */, 18 },
  /* 5190 */  { MAD_F(0x057c07c6) /* 0.342780853 */, 18 },
  /* 5191 */  { MAD_F(0x057c641e) /* 0.342868918 */, 18 },
  /* 5192 */  { MAD_F(0x057cc077) /* 0.342956988 */, 18 },
  /* 5193 */  { MAD_F(0x057d1cd2) /* 0.343045064 */, 18 },
  /* 5194 */  { MAD_F(0x057d792e) /* 0.343133146 */, 18 },
  /* 5195 */  { MAD_F(0x057dd58c) /* 0.343221233 */, 18 },
  /* 5196 */  { MAD_F(0x057e31eb) /* 0.343309326 */, 18 },
  /* 5197 */  { MAD_F(0x057e8e4c) /* 0.343397425 */, 18 },
  /* 5198 */  { MAD_F(0x057eeaae) /* 0.343485529 */, 18 },
  /* 5199 */  { MAD_F(0x057f4712) /* 0.343573639 */, 18 },

  /* 5200 */  { MAD_F(0x057fa378) /* 0.343661754 */, 18 },
  /* 5201 */  { MAD_F(0x057fffde) /* 0.343749876 */, 18 },
  /* 5202 */  { MAD_F(0x05805c47) /* 0.343838003 */, 18 },
  /* 5203 */  { MAD_F(0x0580b8b1) /* 0.343926135 */, 18 },
  /* 5204 */  { MAD_F(0x0581151c) /* 0.344014273 */, 18 },
  /* 5205 */  { MAD_F(0x05817189) /* 0.344102417 */, 18 },
  /* 5206 */  { MAD_F(0x0581cdf7) /* 0.344190566 */, 18 },
  /* 5207 */  { MAD_F(0x05822a67) /* 0.344278722 */, 18 },
  /* 5208 */  { MAD_F(0x058286d9) /* 0.344366882 */, 18 },
  /* 5209 */  { MAD_F(0x0582e34c) /* 0.344455049 */, 18 },
  /* 5210 */  { MAD_F(0x05833fc0) /* 0.344543221 */, 18 },
  /* 5211 */  { MAD_F(0x05839c36) /* 0.344631398 */, 18 },
  /* 5212 */  { MAD_F(0x0583f8ae) /* 0.344719582 */, 18 },
  /* 5213 */  { MAD_F(0x05845527) /* 0.344807771 */, 18 },
  /* 5214 */  { MAD_F(0x0584b1a1) /* 0.344895965 */, 18 },
  /* 5215 */  { MAD_F(0x05850e1e) /* 0.344984165 */, 18 },

  /* 5216 */  { MAD_F(0x05856a9b) /* 0.345072371 */, 18 },
  /* 5217 */  { MAD_F(0x0585c71a) /* 0.345160583 */, 18 },
  /* 5218 */  { MAD_F(0x0586239b) /* 0.345248800 */, 18 },
  /* 5219 */  { MAD_F(0x0586801d) /* 0.345337023 */, 18 },
  /* 5220 */  { MAD_F(0x0586dca1) /* 0.345425251 */, 18 },
  /* 5221 */  { MAD_F(0x05873926) /* 0.345513485 */, 18 },
  /* 5222 */  { MAD_F(0x058795ac) /* 0.345601725 */, 18 },
  /* 5223 */  { MAD_F(0x0587f235) /* 0.345689970 */, 18 },
  /* 5224 */  { MAD_F(0x05884ebe) /* 0.345778221 */, 18 },
  /* 5225 */  { MAD_F(0x0588ab49) /* 0.345866478 */, 18 },
  /* 5226 */  { MAD_F(0x058907d6) /* 0.345954740 */, 18 },
  /* 5227 */  { MAD_F(0x05896464) /* 0.346043008 */, 18 },
  /* 5228 */  { MAD_F(0x0589c0f4) /* 0.346131281 */, 18 },
  /* 5229 */  { MAD_F(0x058a1d85) /* 0.346219560 */, 18 },
  /* 5230 */  { MAD_F(0x058a7a18) /* 0.346307845 */, 18 },
  /* 5231 */  { MAD_F(0x058ad6ac) /* 0.346396135 */, 18 },

  /* 5232 */  { MAD_F(0x058b3342) /* 0.346484431 */, 18 },
  /* 5233 */  { MAD_F(0x058b8fd9) /* 0.346572733 */, 18 },
  /* 5234 */  { MAD_F(0x058bec72) /* 0.346661040 */, 18 },
  /* 5235 */  { MAD_F(0x058c490c) /* 0.346749353 */, 18 },
  /* 5236 */  { MAD_F(0x058ca5a8) /* 0.346837671 */, 18 },
  /* 5237 */  { MAD_F(0x058d0246) /* 0.346925996 */, 18 },
  /* 5238 */  { MAD_F(0x058d5ee4) /* 0.347014325 */, 18 },
  /* 5239 */  { MAD_F(0x058dbb85) /* 0.347102661 */, 18 },
  /* 5240 */  { MAD_F(0x058e1827) /* 0.347191002 */, 18 },
  /* 5241 */  { MAD_F(0x058e74ca) /* 0.347279348 */, 18 },
  /* 5242 */  { MAD_F(0x058ed16f) /* 0.347367700 */, 18 },
  /* 5243 */  { MAD_F(0x058f2e15) /* 0.347456058 */, 18 },
  /* 5244 */  { MAD_F(0x058f8abd) /* 0.347544422 */, 18 },
  /* 5245 */  { MAD_F(0x058fe766) /* 0.347632791 */, 18 },
  /* 5246 */  { MAD_F(0x05904411) /* 0.347721165 */, 18 },
  /* 5247 */  { MAD_F(0x0590a0be) /* 0.347809546 */, 18 },

  /* 5248 */  { MAD_F(0x0590fd6c) /* 0.347897931 */, 18 },
  /* 5249 */  { MAD_F(0x05915a1b) /* 0.347986323 */, 18 },
  /* 5250 */  { MAD_F(0x0591b6cc) /* 0.348074720 */, 18 },
  /* 5251 */  { MAD_F(0x0592137e) /* 0.348163123 */, 18 },
  /* 5252 */  { MAD_F(0x05927032) /* 0.348251531 */, 18 },
  /* 5253 */  { MAD_F(0x0592cce8) /* 0.348339945 */, 18 },
  /* 5254 */  { MAD_F(0x0593299f) /* 0.348428365 */, 18 },
  /* 5255 */  { MAD_F(0x05938657) /* 0.348516790 */, 18 },
  /* 5256 */  { MAD_F(0x0593e311) /* 0.348605221 */, 18 },
  /* 5257 */  { MAD_F(0x05943fcd) /* 0.348693657 */, 18 },
  /* 5258 */  { MAD_F(0x05949c8a) /* 0.348782099 */, 18 },
  /* 5259 */  { MAD_F(0x0594f948) /* 0.348870547 */, 18 },
  /* 5260 */  { MAD_F(0x05955608) /* 0.348959000 */, 18 },
  /* 5261 */  { MAD_F(0x0595b2ca) /* 0.349047459 */, 18 },
  /* 5262 */  { MAD_F(0x05960f8c) /* 0.349135923 */, 18 },
  /* 5263 */  { MAD_F(0x05966c51) /* 0.349224393 */, 18 },

  /* 5264 */  { MAD_F(0x0596c917) /* 0.349312869 */, 18 },
  /* 5265 */  { MAD_F(0x059725de) /* 0.349401350 */, 18 },
  /* 5266 */  { MAD_F(0x059782a7) /* 0.349489837 */, 18 },
  /* 5267 */  { MAD_F(0x0597df72) /* 0.349578329 */, 18 },
  /* 5268 */  { MAD_F(0x05983c3e) /* 0.349666827 */, 18 },
  /* 5269 */  { MAD_F(0x0598990c) /* 0.349755331 */, 18 },
  /* 5270 */  { MAD_F(0x0598f5db) /* 0.349843840 */, 18 },
  /* 5271 */  { MAD_F(0x059952ab) /* 0.349932355 */, 18 },
  /* 5272 */  { MAD_F(0x0599af7d) /* 0.350020876 */, 18 },
  /* 5273 */  { MAD_F(0x059a0c51) /* 0.350109402 */, 18 },
  /* 5274 */  { MAD_F(0x059a6926) /* 0.350197933 */, 18 },
  /* 5275 */  { MAD_F(0x059ac5fc) /* 0.350286470 */, 18 },
  /* 5276 */  { MAD_F(0x059b22d4) /* 0.350375013 */, 18 },
  /* 5277 */  { MAD_F(0x059b7fae) /* 0.350463562 */, 18 },
  /* 5278 */  { MAD_F(0x059bdc89) /* 0.350552116 */, 18 },
  /* 5279 */  { MAD_F(0x059c3965) /* 0.350640675 */, 18 },

  /* 5280 */  { MAD_F(0x059c9643) /* 0.350729240 */, 18 },
  /* 5281 */  { MAD_F(0x059cf323) /* 0.350817811 */, 18 },
  /* 5282 */  { MAD_F(0x059d5004) /* 0.350906388 */, 18 },
  /* 5283 */  { MAD_F(0x059dace6) /* 0.350994970 */, 18 },
  /* 5284 */  { MAD_F(0x059e09cb) /* 0.351083557 */, 18 },
  /* 5285 */  { MAD_F(0x059e66b0) /* 0.351172150 */, 18 },
  /* 5286 */  { MAD_F(0x059ec397) /* 0.351260749 */, 18 },
  /* 5287 */  { MAD_F(0x059f2080) /* 0.351349353 */, 18 },
  /* 5288 */  { MAD_F(0x059f7d6a) /* 0.351437963 */, 18 },
  /* 5289 */  { MAD_F(0x059fda55) /* 0.351526579 */, 18 },
  /* 5290 */  { MAD_F(0x05a03742) /* 0.351615200 */, 18 },
  /* 5291 */  { MAD_F(0x05a09431) /* 0.351703827 */, 18 },
  /* 5292 */  { MAD_F(0x05a0f121) /* 0.351792459 */, 18 },
  /* 5293 */  { MAD_F(0x05a14e12) /* 0.351881097 */, 18 },
  /* 5294 */  { MAD_F(0x05a1ab05) /* 0.351969740 */, 18 },
  /* 5295 */  { MAD_F(0x05a207fa) /* 0.352058389 */, 18 },

  /* 5296 */  { MAD_F(0x05a264f0) /* 0.352147044 */, 18 },
  /* 5297 */  { MAD_F(0x05a2c1e7) /* 0.352235704 */, 18 },
  /* 5298 */  { MAD_F(0x05a31ee1) /* 0.352324369 */, 18 },
  /* 5299 */  { MAD_F(0x05a37bdb) /* 0.352413041 */, 18 },
  /* 5300 */  { MAD_F(0x05a3d8d7) /* 0.352501718 */, 18 },
  /* 5301 */  { MAD_F(0x05a435d5) /* 0.352590400 */, 18 },
  /* 5302 */  { MAD_F(0x05a492d4) /* 0.352679088 */, 18 },
  /* 5303 */  { MAD_F(0x05a4efd4) /* 0.352767782 */, 18 },
  /* 5304 */  { MAD_F(0x05a54cd6) /* 0.352856481 */, 18 },
  /* 5305 */  { MAD_F(0x05a5a9da) /* 0.352945186 */, 18 },
  /* 5306 */  { MAD_F(0x05a606df) /* 0.353033896 */, 18 },
  /* 5307 */  { MAD_F(0x05a663e5) /* 0.353122612 */, 18 },
  /* 5308 */  { MAD_F(0x05a6c0ed) /* 0.353211333 */, 18 },
  /* 5309 */  { MAD_F(0x05a71df7) /* 0.353300061 */, 18 },
  /* 5310 */  { MAD_F(0x05a77b02) /* 0.353388793 */, 18 },
  /* 5311 */  { MAD_F(0x05a7d80e) /* 0.353477531 */, 18 },

  /* 5312 */  { MAD_F(0x05a8351c) /* 0.353566275 */, 18 },
  /* 5313 */  { MAD_F(0x05a8922c) /* 0.353655024 */, 18 },
  /* 5314 */  { MAD_F(0x05a8ef3c) /* 0.353743779 */, 18 },
  /* 5315 */  { MAD_F(0x05a94c4f) /* 0.353832540 */, 18 },
  /* 5316 */  { MAD_F(0x05a9a963) /* 0.353921306 */, 18 },
  /* 5317 */  { MAD_F(0x05aa0678) /* 0.354010077 */, 18 },
  /* 5318 */  { MAD_F(0x05aa638f) /* 0.354098855 */, 18 },
  /* 5319 */  { MAD_F(0x05aac0a8) /* 0.354187637 */, 18 },
  /* 5320 */  { MAD_F(0x05ab1dc2) /* 0.354276426 */, 18 },
  /* 5321 */  { MAD_F(0x05ab7add) /* 0.354365220 */, 18 },
  /* 5322 */  { MAD_F(0x05abd7fa) /* 0.354454019 */, 18 },
  /* 5323 */  { MAD_F(0x05ac3518) /* 0.354542824 */, 18 },
  /* 5324 */  { MAD_F(0x05ac9238) /* 0.354631635 */, 18 },
  /* 5325 */  { MAD_F(0x05acef5a) /* 0.354720451 */, 18 },
  /* 5326 */  { MAD_F(0x05ad4c7d) /* 0.354809272 */, 18 },
  /* 5327 */  { MAD_F(0x05ada9a1) /* 0.354898100 */, 18 },

  /* 5328 */  { MAD_F(0x05ae06c7) /* 0.354986932 */, 18 },
  /* 5329 */  { MAD_F(0x05ae63ee) /* 0.355075771 */, 18 },
  /* 5330 */  { MAD_F(0x05aec117) /* 0.355164615 */, 18 },
  /* 5331 */  { MAD_F(0x05af1e41) /* 0.355253464 */, 18 },
  /* 5332 */  { MAD_F(0x05af7b6d) /* 0.355342319 */, 18 },
  /* 5333 */  { MAD_F(0x05afd89b) /* 0.355431180 */, 18 },
  /* 5334 */  { MAD_F(0x05b035c9) /* 0.355520046 */, 18 },
  /* 5335 */  { MAD_F(0x05b092fa) /* 0.355608917 */, 18 },
  /* 5336 */  { MAD_F(0x05b0f02b) /* 0.355697795 */, 18 },
  /* 5337 */  { MAD_F(0x05b14d5f) /* 0.355786677 */, 18 },
  /* 5338 */  { MAD_F(0x05b1aa94) /* 0.355875566 */, 18 },
  /* 5339 */  { MAD_F(0x05b207ca) /* 0.355964460 */, 18 },
  /* 5340 */  { MAD_F(0x05b26502) /* 0.356053359 */, 18 },
  /* 5341 */  { MAD_F(0x05b2c23b) /* 0.356142264 */, 18 },
  /* 5342 */  { MAD_F(0x05b31f76) /* 0.356231175 */, 18 },
  /* 5343 */  { MAD_F(0x05b37cb2) /* 0.356320091 */, 18 },

  /* 5344 */  { MAD_F(0x05b3d9f0) /* 0.356409012 */, 18 },
  /* 5345 */  { MAD_F(0x05b4372f) /* 0.356497940 */, 18 },
  /* 5346 */  { MAD_F(0x05b4946f) /* 0.356586872 */, 18 },
  /* 5347 */  { MAD_F(0x05b4f1b2) /* 0.356675811 */, 18 },
  /* 5348 */  { MAD_F(0x05b54ef5) /* 0.356764754 */, 18 },
  /* 5349 */  { MAD_F(0x05b5ac3a) /* 0.356853704 */, 18 },
  /* 5350 */  { MAD_F(0x05b60981) /* 0.356942659 */, 18 },
  /* 5351 */  { MAD_F(0x05b666c9) /* 0.357031619 */, 18 },
  /* 5352 */  { MAD_F(0x05b6c413) /* 0.357120585 */, 18 },
  /* 5353 */  { MAD_F(0x05b7215e) /* 0.357209557 */, 18 },
  /* 5354 */  { MAD_F(0x05b77eab) /* 0.357298534 */, 18 },
  /* 5355 */  { MAD_F(0x05b7dbf9) /* 0.357387516 */, 18 },
  /* 5356 */  { MAD_F(0x05b83948) /* 0.357476504 */, 18 },
  /* 5357 */  { MAD_F(0x05b89699) /* 0.357565498 */, 18 },
  /* 5358 */  { MAD_F(0x05b8f3ec) /* 0.357654497 */, 18 },
  /* 5359 */  { MAD_F(0x05b95140) /* 0.357743502 */, 18 },

  /* 5360 */  { MAD_F(0x05b9ae95) /* 0.357832512 */, 18 },
  /* 5361 */  { MAD_F(0x05ba0bec) /* 0.357921528 */, 18 },
  /* 5362 */  { MAD_F(0x05ba6945) /* 0.358010550 */, 18 },
  /* 5363 */  { MAD_F(0x05bac69f) /* 0.358099576 */, 18 },
  /* 5364 */  { MAD_F(0x05bb23fa) /* 0.358188609 */, 18 },
  /* 5365 */  { MAD_F(0x05bb8157) /* 0.358277647 */, 18 },
  /* 5366 */  { MAD_F(0x05bbdeb6) /* 0.358366690 */, 18 },
  /* 5367 */  { MAD_F(0x05bc3c16) /* 0.358455739 */, 18 },
  /* 5368 */  { MAD_F(0x05bc9977) /* 0.358544794 */, 18 },
  /* 5369 */  { MAD_F(0x05bcf6da) /* 0.358633854 */, 18 },
  /* 5370 */  { MAD_F(0x05bd543e) /* 0.358722920 */, 18 },
  /* 5371 */  { MAD_F(0x05bdb1a4) /* 0.358811991 */, 18 },
  /* 5372 */  { MAD_F(0x05be0f0b) /* 0.358901067 */, 18 },
  /* 5373 */  { MAD_F(0x05be6c74) /* 0.358990150 */, 18 },
  /* 5374 */  { MAD_F(0x05bec9df) /* 0.359079237 */, 18 },
  /* 5375 */  { MAD_F(0x05bf274a) /* 0.359168331 */, 18 },

  /* 5376 */  { MAD_F(0x05bf84b8) /* 0.359257429 */, 18 },
  /* 5377 */  { MAD_F(0x05bfe226) /* 0.359346534 */, 18 },
  /* 5378 */  { MAD_F(0x05c03f97) /* 0.359435644 */, 18 },
  /* 5379 */  { MAD_F(0x05c09d08) /* 0.359524759 */, 18 },
  /* 5380 */  { MAD_F(0x05c0fa7c) /* 0.359613880 */, 18 },
  /* 5381 */  { MAD_F(0x05c157f0) /* 0.359703006 */, 18 },
  /* 5382 */  { MAD_F(0x05c1b566) /* 0.359792138 */, 18 },
  /* 5383 */  { MAD_F(0x05c212de) /* 0.359881276 */, 18 },
  /* 5384 */  { MAD_F(0x05c27057) /* 0.359970419 */, 18 },
  /* 5385 */  { MAD_F(0x05c2cdd2) /* 0.360059567 */, 18 },
  /* 5386 */  { MAD_F(0x05c32b4e) /* 0.360148721 */, 18 },
  /* 5387 */  { MAD_F(0x05c388cb) /* 0.360237881 */, 18 },
  /* 5388 */  { MAD_F(0x05c3e64b) /* 0.360327046 */, 18 },
  /* 5389 */  { MAD_F(0x05c443cb) /* 0.360416216 */, 18 },
  /* 5390 */  { MAD_F(0x05c4a14d) /* 0.360505392 */, 18 },
  /* 5391 */  { MAD_F(0x05c4fed1) /* 0.360594574 */, 18 },

  /* 5392 */  { MAD_F(0x05c55c56) /* 0.360683761 */, 18 },
  /* 5393 */  { MAD_F(0x05c5b9dc) /* 0.360772953 */, 18 },
  /* 5394 */  { MAD_F(0x05c61764) /* 0.360862152 */, 18 },
  /* 5395 */  { MAD_F(0x05c674ed) /* 0.360951355 */, 18 },
  /* 5396 */  { MAD_F(0x05c6d278) /* 0.361040564 */, 18 },
  /* 5397 */  { MAD_F(0x05c73005) /* 0.361129779 */, 18 },
  /* 5398 */  { MAD_F(0x05c78d93) /* 0.361218999 */, 18 },
  /* 5399 */  { MAD_F(0x05c7eb22) /* 0.361308225 */, 18 },
  /* 5400 */  { MAD_F(0x05c848b3) /* 0.361397456 */, 18 },
  /* 5401 */  { MAD_F(0x05c8a645) /* 0.361486693 */, 18 },
  /* 5402 */  { MAD_F(0x05c903d9) /* 0.361575935 */, 18 },
  /* 5403 */  { MAD_F(0x05c9616e) /* 0.361665183 */, 18 },
  /* 5404 */  { MAD_F(0x05c9bf05) /* 0.361754436 */, 18 },
  /* 5405 */  { MAD_F(0x05ca1c9d) /* 0.361843695 */, 18 },
  /* 5406 */  { MAD_F(0x05ca7a37) /* 0.361932959 */, 18 },
  /* 5407 */  { MAD_F(0x05cad7d2) /* 0.362022229 */, 18 },

  /* 5408 */  { MAD_F(0x05cb356e) /* 0.362111504 */, 18 },
  /* 5409 */  { MAD_F(0x05cb930d) /* 0.362200785 */, 18 },
  /* 5410 */  { MAD_F(0x05cbf0ac) /* 0.362290071 */, 18 },
  /* 5411 */  { MAD_F(0x05cc4e4d) /* 0.362379362 */, 18 },
  /* 5412 */  { MAD_F(0x05ccabf0) /* 0.362468660 */, 18 },
  /* 5413 */  { MAD_F(0x05cd0994) /* 0.362557962 */, 18 },
  /* 5414 */  { MAD_F(0x05cd6739) /* 0.362647271 */, 18 },
  /* 5415 */  { MAD_F(0x05cdc4e0) /* 0.362736584 */, 18 },
  /* 5416 */  { MAD_F(0x05ce2289) /* 0.362825904 */, 18 },
  /* 5417 */  { MAD_F(0x05ce8033) /* 0.362915228 */, 18 },
  /* 5418 */  { MAD_F(0x05ceddde) /* 0.363004559 */, 18 },
  /* 5419 */  { MAD_F(0x05cf3b8b) /* 0.363093894 */, 18 },
  /* 5420 */  { MAD_F(0x05cf9939) /* 0.363183236 */, 18 },
  /* 5421 */  { MAD_F(0x05cff6e9) /* 0.363272582 */, 18 },
  /* 5422 */  { MAD_F(0x05d0549a) /* 0.363361935 */, 18 },
  /* 5423 */  { MAD_F(0x05d0b24d) /* 0.363451292 */, 18 },

  /* 5424 */  { MAD_F(0x05d11001) /* 0.363540655 */, 18 },
  /* 5425 */  { MAD_F(0x05d16db7) /* 0.363630024 */, 18 },
  /* 5426 */  { MAD_F(0x05d1cb6e) /* 0.363719398 */, 18 },
  /* 5427 */  { MAD_F(0x05d22927) /* 0.363808778 */, 18 },
  /* 5428 */  { MAD_F(0x05d286e1) /* 0.363898163 */, 18 },
  /* 5429 */  { MAD_F(0x05d2e49d) /* 0.363987554 */, 18 },
  /* 5430 */  { MAD_F(0x05d3425a) /* 0.364076950 */, 18 },
  /* 5431 */  { MAD_F(0x05d3a018) /* 0.364166352 */, 18 },
  /* 5432 */  { MAD_F(0x05d3fdd8) /* 0.364255759 */, 18 },
  /* 5433 */  { MAD_F(0x05d45b9a) /* 0.364345171 */, 18 },
  /* 5434 */  { MAD_F(0x05d4b95d) /* 0.364434589 */, 18 },
  /* 5435 */  { MAD_F(0x05d51721) /* 0.364524013 */, 18 },
  /* 5436 */  { MAD_F(0x05d574e7) /* 0.364613442 */, 18 },
  /* 5437 */  { MAD_F(0x05d5d2af) /* 0.364702877 */, 18 },
  /* 5438 */  { MAD_F(0x05d63078) /* 0.364792317 */, 18 },
  /* 5439 */  { MAD_F(0x05d68e42) /* 0.364881762 */, 18 },

  /* 5440 */  { MAD_F(0x05d6ec0e) /* 0.364971213 */, 18 },
  /* 5441 */  { MAD_F(0x05d749db) /* 0.365060669 */, 18 },
  /* 5442 */  { MAD_F(0x05d7a7aa) /* 0.365150131 */, 18 },
  /* 5443 */  { MAD_F(0x05d8057a) /* 0.365239599 */, 18 },
  /* 5444 */  { MAD_F(0x05d8634c) /* 0.365329072 */, 18 },
  /* 5445 */  { MAD_F(0x05d8c11f) /* 0.365418550 */, 18 },
  /* 5446 */  { MAD_F(0x05d91ef4) /* 0.365508034 */, 18 },
  /* 5447 */  { MAD_F(0x05d97cca) /* 0.365597523 */, 18 },
  /* 5448 */  { MAD_F(0x05d9daa1) /* 0.365687018 */, 18 },
  /* 5449 */  { MAD_F(0x05da387a) /* 0.365776518 */, 18 },
  /* 5450 */  { MAD_F(0x05da9655) /* 0.365866024 */, 18 },
  /* 5451 */  { MAD_F(0x05daf431) /* 0.365955536 */, 18 },
  /* 5452 */  { MAD_F(0x05db520e) /* 0.366045052 */, 18 },
  /* 5453 */  { MAD_F(0x05dbafed) /* 0.366134574 */, 18 },
  /* 5454 */  { MAD_F(0x05dc0dce) /* 0.366224102 */, 18 },
  /* 5455 */  { MAD_F(0x05dc6baf) /* 0.366313635 */, 18 },

  /* 5456 */  { MAD_F(0x05dcc993) /* 0.366403174 */, 18 },
  /* 5457 */  { MAD_F(0x05dd2778) /* 0.366492718 */, 18 },
  /* 5458 */  { MAD_F(0x05dd855e) /* 0.366582267 */, 18 },
  /* 5459 */  { MAD_F(0x05dde346) /* 0.366671822 */, 18 },
  /* 5460 */  { MAD_F(0x05de412f) /* 0.366761383 */, 18 },
  /* 5461 */  { MAD_F(0x05de9f1a) /* 0.366850949 */, 18 },
  /* 5462 */  { MAD_F(0x05defd06) /* 0.366940520 */, 18 },
  /* 5463 */  { MAD_F(0x05df5af3) /* 0.367030097 */, 18 },
  /* 5464 */  { MAD_F(0x05dfb8e2) /* 0.367119680 */, 18 },
  /* 5465 */  { MAD_F(0x05e016d3) /* 0.367209267 */, 18 },
  /* 5466 */  { MAD_F(0x05e074c5) /* 0.367298861 */, 18 },
  /* 5467 */  { MAD_F(0x05e0d2b8) /* 0.367388459 */, 18 },
  /* 5468 */  { MAD_F(0x05e130ad) /* 0.367478064 */, 18 },
  /* 5469 */  { MAD_F(0x05e18ea4) /* 0.367567673 */, 18 },
  /* 5470 */  { MAD_F(0x05e1ec9c) /* 0.367657288 */, 18 },
  /* 5471 */  { MAD_F(0x05e24a95) /* 0.367746909 */, 18 },

  /* 5472 */  { MAD_F(0x05e2a890) /* 0.367836535 */, 18 },
  /* 5473 */  { MAD_F(0x05e3068c) /* 0.367926167 */, 18 },
  /* 5474 */  { MAD_F(0x05e3648a) /* 0.368015804 */, 18 },
  /* 5475 */  { MAD_F(0x05e3c289) /* 0.368105446 */, 18 },
  /* 5476 */  { MAD_F(0x05e4208a) /* 0.368195094 */, 18 },
  /* 5477 */  { MAD_F(0x05e47e8c) /* 0.368284747 */, 18 },
  /* 5478 */  { MAD_F(0x05e4dc8f) /* 0.368374406 */, 18 },
  /* 5479 */  { MAD_F(0x05e53a94) /* 0.368464070 */, 18 },
  /* 5480 */  { MAD_F(0x05e5989b) /* 0.368553740 */, 18 },
  /* 5481 */  { MAD_F(0x05e5f6a3) /* 0.368643415 */, 18 },
  /* 5482 */  { MAD_F(0x05e654ac) /* 0.368733096 */, 18 },
  /* 5483 */  { MAD_F(0x05e6b2b7) /* 0.368822782 */, 18 },
  /* 5484 */  { MAD_F(0x05e710c4) /* 0.368912473 */, 18 },
  /* 5485 */  { MAD_F(0x05e76ed2) /* 0.369002170 */, 18 },
  /* 5486 */  { MAD_F(0x05e7cce1) /* 0.369091873 */, 18 },
  /* 5487 */  { MAD_F(0x05e82af2) /* 0.369181581 */, 18 },

  /* 5488 */  { MAD_F(0x05e88904) /* 0.369271294 */, 18 },
  /* 5489 */  { MAD_F(0x05e8e718) /* 0.369361013 */, 18 },
  /* 5490 */  { MAD_F(0x05e9452d) /* 0.369450737 */, 18 },
  /* 5491 */  { MAD_F(0x05e9a343) /* 0.369540467 */, 18 },
  /* 5492 */  { MAD_F(0x05ea015c) /* 0.369630202 */, 18 },
  /* 5493 */  { MAD_F(0x05ea5f75) /* 0.369719942 */, 18 },
  /* 5494 */  { MAD_F(0x05eabd90) /* 0.369809688 */, 18 },
  /* 5495 */  { MAD_F(0x05eb1bad) /* 0.369899440 */, 18 },
  /* 5496 */  { MAD_F(0x05eb79cb) /* 0.369989197 */, 18 },
  /* 5497 */  { MAD_F(0x05ebd7ea) /* 0.370078959 */, 18 },
  /* 5498 */  { MAD_F(0x05ec360b) /* 0.370168727 */, 18 },
  /* 5499 */  { MAD_F(0x05ec942d) /* 0.370258500 */, 18 },
  /* 5500 */  { MAD_F(0x05ecf251) /* 0.370348279 */, 18 },
  /* 5501 */  { MAD_F(0x05ed5076) /* 0.370438063 */, 18 },
  /* 5502 */  { MAD_F(0x05edae9d) /* 0.370527853 */, 18 },
  /* 5503 */  { MAD_F(0x05ee0cc5) /* 0.370617648 */, 18 },

  /* 5504 */  { MAD_F(0x05ee6aef) /* 0.370707448 */, 18 },
  /* 5505 */  { MAD_F(0x05eec91a) /* 0.370797254 */, 18 },
  /* 5506 */  { MAD_F(0x05ef2746) /* 0.370887065 */, 18 },
  /* 5507 */  { MAD_F(0x05ef8574) /* 0.370976882 */, 18 },
  /* 5508 */  { MAD_F(0x05efe3a4) /* 0.371066704 */, 18 },
  /* 5509 */  { MAD_F(0x05f041d5) /* 0.371156532 */, 18 },
  /* 5510 */  { MAD_F(0x05f0a007) /* 0.371246365 */, 18 },
  /* 5511 */  { MAD_F(0x05f0fe3b) /* 0.371336203 */, 18 },
  /* 5512 */  { MAD_F(0x05f15c70) /* 0.371426047 */, 18 },
  /* 5513 */  { MAD_F(0x05f1baa7) /* 0.371515897 */, 18 },
  /* 5514 */  { MAD_F(0x05f218df) /* 0.371605751 */, 18 },
  /* 5515 */  { MAD_F(0x05f27719) /* 0.371695612 */, 18 },
  /* 5516 */  { MAD_F(0x05f2d554) /* 0.371785477 */, 18 },
  /* 5517 */  { MAD_F(0x05f33390) /* 0.371875348 */, 18 },
  /* 5518 */  { MAD_F(0x05f391cf) /* 0.371965225 */, 18 },
  /* 5519 */  { MAD_F(0x05f3f00e) /* 0.372055107 */, 18 },

  /* 5520 */  { MAD_F(0x05f44e4f) /* 0.372144994 */, 18 },
  /* 5521 */  { MAD_F(0x05f4ac91) /* 0.372234887 */, 18 },
  /* 5522 */  { MAD_F(0x05f50ad5) /* 0.372324785 */, 18 },
  /* 5523 */  { MAD_F(0x05f5691b) /* 0.372414689 */, 18 },
  /* 5524 */  { MAD_F(0x05f5c761) /* 0.372504598 */, 18 },
  /* 5525 */  { MAD_F(0x05f625aa) /* 0.372594513 */, 18 },
  /* 5526 */  { MAD_F(0x05f683f3) /* 0.372684433 */, 18 },
  /* 5527 */  { MAD_F(0x05f6e23f) /* 0.372774358 */, 18 },
  /* 5528 */  { MAD_F(0x05f7408b) /* 0.372864289 */, 18 },
  /* 5529 */  { MAD_F(0x05f79ed9) /* 0.372954225 */, 18 },
  /* 5530 */  { MAD_F(0x05f7fd29) /* 0.373044167 */, 18 },
  /* 5531 */  { MAD_F(0x05f85b7a) /* 0.373134114 */, 18 },
  /* 5532 */  { MAD_F(0x05f8b9cc) /* 0.373224066 */, 18 },
  /* 5533 */  { MAD_F(0x05f91820) /* 0.373314024 */, 18 },
  /* 5534 */  { MAD_F(0x05f97675) /* 0.373403987 */, 18 },
  /* 5535 */  { MAD_F(0x05f9d4cc) /* 0.373493956 */, 18 },

  /* 5536 */  { MAD_F(0x05fa3324) /* 0.373583930 */, 18 },
  /* 5537 */  { MAD_F(0x05fa917e) /* 0.373673910 */, 18 },
  /* 5538 */  { MAD_F(0x05faefd9) /* 0.373763895 */, 18 },
  /* 5539 */  { MAD_F(0x05fb4e36) /* 0.373853885 */, 18 },
  /* 5540 */  { MAD_F(0x05fbac94) /* 0.373943881 */, 18 },
  /* 5541 */  { MAD_F(0x05fc0af3) /* 0.374033882 */, 18 },
  /* 5542 */  { MAD_F(0x05fc6954) /* 0.374123889 */, 18 },
  /* 5543 */  { MAD_F(0x05fcc7b7) /* 0.374213901 */, 18 },
  /* 5544 */  { MAD_F(0x05fd261b) /* 0.374303918 */, 18 },
  /* 5545 */  { MAD_F(0x05fd8480) /* 0.374393941 */, 18 },
  /* 5546 */  { MAD_F(0x05fde2e7) /* 0.374483970 */, 18 },
  /* 5547 */  { MAD_F(0x05fe414f) /* 0.374574003 */, 18 },
  /* 5548 */  { MAD_F(0x05fe9fb9) /* 0.374664042 */, 18 },
  /* 5549 */  { MAD_F(0x05fefe24) /* 0.374754087 */, 18 },
  /* 5550 */  { MAD_F(0x05ff5c91) /* 0.374844137 */, 18 },
  /* 5551 */  { MAD_F(0x05ffbaff) /* 0.374934192 */, 18 },

  /* 5552 */  { MAD_F(0x0600196e) /* 0.375024253 */, 18 },
  /* 5553 */  { MAD_F(0x060077df) /* 0.375114319 */, 18 },
  /* 5554 */  { MAD_F(0x0600d651) /* 0.375204391 */, 18 },
  /* 5555 */  { MAD_F(0x060134c5) /* 0.375294468 */, 18 },
  /* 5556 */  { MAD_F(0x0601933b) /* 0.375384550 */, 18 },
  /* 5557 */  { MAD_F(0x0601f1b1) /* 0.375474638 */, 18 },
  /* 5558 */  { MAD_F(0x0602502a) /* 0.375564731 */, 18 },
  /* 5559 */  { MAD_F(0x0602aea3) /* 0.375654830 */, 18 },
  /* 5560 */  { MAD_F(0x06030d1e) /* 0.375744934 */, 18 },
  /* 5561 */  { MAD_F(0x06036b9b) /* 0.375835043 */, 18 },
  /* 5562 */  { MAD_F(0x0603ca19) /* 0.375925158 */, 18 },
  /* 5563 */  { MAD_F(0x06042898) /* 0.376015278 */, 18 },
  /* 5564 */  { MAD_F(0x06048719) /* 0.376105404 */, 18 },
  /* 5565 */  { MAD_F(0x0604e59c) /* 0.376195535 */, 18 },
  /* 5566 */  { MAD_F(0x0605441f) /* 0.376285671 */, 18 },
  /* 5567 */  { MAD_F(0x0605a2a5) /* 0.376375813 */, 18 },

  /* 5568 */  { MAD_F(0x0606012b) /* 0.376465960 */, 18 },
  /* 5569 */  { MAD_F(0x06065fb4) /* 0.376556113 */, 18 },
  /* 5570 */  { MAD_F(0x0606be3d) /* 0.376646271 */, 18 },
  /* 5571 */  { MAD_F(0x06071cc8) /* 0.376736434 */, 18 },
  /* 5572 */  { MAD_F(0x06077b55) /* 0.376826603 */, 18 },
  /* 5573 */  { MAD_F(0x0607d9e3) /* 0.376916777 */, 18 },
  /* 5574 */  { MAD_F(0x06083872) /* 0.377006957 */, 18 },
  /* 5575 */  { MAD_F(0x06089703) /* 0.377097141 */, 18 },
  /* 5576 */  { MAD_F(0x0608f595) /* 0.377187332 */, 18 },
  /* 5577 */  { MAD_F(0x06095429) /* 0.377277528 */, 18 },
  /* 5578 */  { MAD_F(0x0609b2be) /* 0.377367729 */, 18 },
  /* 5579 */  { MAD_F(0x060a1155) /* 0.377457935 */, 18 },
  /* 5580 */  { MAD_F(0x060a6fed) /* 0.377548147 */, 18 },
  /* 5581 */  { MAD_F(0x060ace86) /* 0.377638364 */, 18 },
  /* 5582 */  { MAD_F(0x060b2d21) /* 0.377728587 */, 18 },
  /* 5583 */  { MAD_F(0x060b8bbe) /* 0.377818815 */, 18 },

  /* 5584 */  { MAD_F(0x060bea5c) /* 0.377909049 */, 18 },
  /* 5585 */  { MAD_F(0x060c48fb) /* 0.377999288 */, 18 },
  /* 5586 */  { MAD_F(0x060ca79c) /* 0.378089532 */, 18 },
  /* 5587 */  { MAD_F(0x060d063e) /* 0.378179781 */, 18 },
  /* 5588 */  { MAD_F(0x060d64e1) /* 0.378270036 */, 18 },
  /* 5589 */  { MAD_F(0x060dc387) /* 0.378360297 */, 18 },
  /* 5590 */  { MAD_F(0x060e222d) /* 0.378450563 */, 18 },
  /* 5591 */  { MAD_F(0x060e80d5) /* 0.378540834 */, 18 },
  /* 5592 */  { MAD_F(0x060edf7f) /* 0.378631110 */, 18 },
  /* 5593 */  { MAD_F(0x060f3e29) /* 0.378721392 */, 18 },
  /* 5594 */  { MAD_F(0x060f9cd6) /* 0.378811680 */, 18 },
  /* 5595 */  { MAD_F(0x060ffb83) /* 0.378901972 */, 18 },
  /* 5596 */  { MAD_F(0x06105a33) /* 0.378992270 */, 18 },
  /* 5597 */  { MAD_F(0x0610b8e3) /* 0.379082574 */, 18 },
  /* 5598 */  { MAD_F(0x06111795) /* 0.379172883 */, 18 },
  /* 5599 */  { MAD_F(0x06117649) /* 0.379263197 */, 18 },

  /* 5600 */  { MAD_F(0x0611d4fe) /* 0.379353516 */, 18 },
  /* 5601 */  { MAD_F(0x061233b4) /* 0.379443841 */, 18 },
  /* 5602 */  { MAD_F(0x0612926c) /* 0.379534172 */, 18 },
  /* 5603 */  { MAD_F(0x0612f125) /* 0.379624507 */, 18 },
  /* 5604 */  { MAD_F(0x06134fe0) /* 0.379714848 */, 18 },
  /* 5605 */  { MAD_F(0x0613ae9c) /* 0.379805195 */, 18 },
  /* 5606 */  { MAD_F(0x06140d5a) /* 0.379895547 */, 18 },
  /* 5607 */  { MAD_F(0x06146c19) /* 0.379985904 */, 18 },
  /* 5608 */  { MAD_F(0x0614cada) /* 0.380076266 */, 18 },
  /* 5609 */  { MAD_F(0x0615299c) /* 0.380166634 */, 18 },
  /* 5610 */  { MAD_F(0x0615885f) /* 0.380257008 */, 18 },
  /* 5611 */  { MAD_F(0x0615e724) /* 0.380347386 */, 18 },
  /* 5612 */  { MAD_F(0x061645ea) /* 0.380437770 */, 18 },
  /* 5613 */  { MAD_F(0x0616a4b2) /* 0.380528160 */, 18 },
  /* 5614 */  { MAD_F(0x0617037b) /* 0.380618555 */, 18 },
  /* 5615 */  { MAD_F(0x06176246) /* 0.380708955 */, 18 },

  /* 5616 */  { MAD_F(0x0617c112) /* 0.380799360 */, 18 },
  /* 5617 */  { MAD_F(0x06181fdf) /* 0.380889771 */, 18 },
  /* 5618 */  { MAD_F(0x06187eae) /* 0.380980187 */, 18 },
  /* 5619 */  { MAD_F(0x0618dd7e) /* 0.381070609 */, 18 },
  /* 5620 */  { MAD_F(0x06193c50) /* 0.381161036 */, 18 },
  /* 5621 */  { MAD_F(0x06199b24) /* 0.381251468 */, 18 },
  /* 5622 */  { MAD_F(0x0619f9f8) /* 0.381341906 */, 18 },
  /* 5623 */  { MAD_F(0x061a58ce) /* 0.381432349 */, 18 },
  /* 5624 */  { MAD_F(0x061ab7a6) /* 0.381522798 */, 18 },
  /* 5625 */  { MAD_F(0x061b167f) /* 0.381613251 */, 18 },
  /* 5626 */  { MAD_F(0x061b7559) /* 0.381703711 */, 18 },
  /* 5627 */  { MAD_F(0x061bd435) /* 0.381794175 */, 18 },
  /* 5628 */  { MAD_F(0x061c3313) /* 0.381884645 */, 18 },
  /* 5629 */  { MAD_F(0x061c91f1) /* 0.381975120 */, 18 },
  /* 5630 */  { MAD_F(0x061cf0d2) /* 0.382065601 */, 18 },
  /* 5631 */  { MAD_F(0x061d4fb3) /* 0.382156087 */, 18 },

  /* 5632 */  { MAD_F(0x061dae96) /* 0.382246578 */, 18 },
  /* 5633 */  { MAD_F(0x061e0d7b) /* 0.382337075 */, 18 },
  /* 5634 */  { MAD_F(0x061e6c61) /* 0.382427577 */, 18 },
  /* 5635 */  { MAD_F(0x061ecb48) /* 0.382518084 */, 18 },
  /* 5636 */  { MAD_F(0x061f2a31) /* 0.382608597 */, 18 },
  /* 5637 */  { MAD_F(0x061f891b) /* 0.382699115 */, 18 },
  /* 5638 */  { MAD_F(0x061fe807) /* 0.382789638 */, 18 },
  /* 5639 */  { MAD_F(0x062046f4) /* 0.382880167 */, 18 },
  /* 5640 */  { MAD_F(0x0620a5e3) /* 0.382970701 */, 18 },
  /* 5641 */  { MAD_F(0x062104d3) /* 0.383061241 */, 18 },
  /* 5642 */  { MAD_F(0x062163c4) /* 0.383151786 */, 18 },
  /* 5643 */  { MAD_F(0x0621c2b7) /* 0.383242336 */, 18 },
  /* 5644 */  { MAD_F(0x062221ab) /* 0.383332891 */, 18 },
  /* 5645 */  { MAD_F(0x062280a1) /* 0.383423452 */, 18 },
  /* 5646 */  { MAD_F(0x0622df98) /* 0.383514018 */, 18 },
  /* 5647 */  { MAD_F(0x06233e91) /* 0.383604590 */, 18 },

  /* 5648 */  { MAD_F(0x06239d8b) /* 0.383695167 */, 18 },
  /* 5649 */  { MAD_F(0x0623fc86) /* 0.383785749 */, 18 },
  /* 5650 */  { MAD_F(0x06245b83) /* 0.383876337 */, 18 },
  /* 5651 */  { MAD_F(0x0624ba82) /* 0.383966930 */, 18 },
  /* 5652 */  { MAD_F(0x06251981) /* 0.384057528 */, 18 },
  /* 5653 */  { MAD_F(0x06257883) /* 0.384148132 */, 18 },
  /* 5654 */  { MAD_F(0x0625d785) /* 0.384238741 */, 18 },
  /* 5655 */  { MAD_F(0x06263689) /* 0.384329355 */, 18 },
  /* 5656 */  { MAD_F(0x0626958f) /* 0.384419975 */, 18 },
  /* 5657 */  { MAD_F(0x0626f496) /* 0.384510600 */, 18 },
  /* 5658 */  { MAD_F(0x0627539e) /* 0.384601230 */, 18 },
  /* 5659 */  { MAD_F(0x0627b2a8) /* 0.384691866 */, 18 },
  /* 5660 */  { MAD_F(0x062811b3) /* 0.384782507 */, 18 },
  /* 5661 */  { MAD_F(0x062870c0) /* 0.384873153 */, 18 },
  /* 5662 */  { MAD_F(0x0628cfce) /* 0.384963805 */, 18 },
  /* 5663 */  { MAD_F(0x06292ede) /* 0.385054462 */, 18 },

  /* 5664 */  { MAD_F(0x06298def) /* 0.385145124 */, 18 },
  /* 5665 */  { MAD_F(0x0629ed01) /* 0.385235792 */, 18 },
  /* 5666 */  { MAD_F(0x062a4c15) /* 0.385326465 */, 18 },
  /* 5667 */  { MAD_F(0x062aab2a) /* 0.385417143 */, 18 },
  /* 5668 */  { MAD_F(0x062b0a41) /* 0.385507827 */, 18 },
  /* 5669 */  { MAD_F(0x062b6959) /* 0.385598516 */, 18 },
  /* 5670 */  { MAD_F(0x062bc873) /* 0.385689211 */, 18 },
  /* 5671 */  { MAD_F(0x062c278e) /* 0.385779910 */, 18 },
  /* 5672 */  { MAD_F(0x062c86aa) /* 0.385870615 */, 18 },
  /* 5673 */  { MAD_F(0x062ce5c8) /* 0.385961326 */, 18 },
  /* 5674 */  { MAD_F(0x062d44e8) /* 0.386052041 */, 18 },
  /* 5675 */  { MAD_F(0x062da408) /* 0.386142762 */, 18 },
  /* 5676 */  { MAD_F(0x062e032a) /* 0.386233489 */, 18 },
  /* 5677 */  { MAD_F(0x062e624e) /* 0.386324221 */, 18 },
  /* 5678 */  { MAD_F(0x062ec173) /* 0.386414958 */, 18 },
  /* 5679 */  { MAD_F(0x062f209a) /* 0.386505700 */, 18 },

  /* 5680 */  { MAD_F(0x062f7fc1) /* 0.386596448 */, 18 },
  /* 5681 */  { MAD_F(0x062fdeeb) /* 0.386687201 */, 18 },
  /* 5682 */  { MAD_F(0x06303e16) /* 0.386777959 */, 18 },
  /* 5683 */  { MAD_F(0x06309d42) /* 0.386868723 */, 18 },
  /* 5684 */  { MAD_F(0x0630fc6f) /* 0.386959492 */, 18 },
  /* 5685 */  { MAD_F(0x06315b9e) /* 0.387050266 */, 18 },
  /* 5686 */  { MAD_F(0x0631bacf) /* 0.387141045 */, 18 },
  /* 5687 */  { MAD_F(0x06321a01) /* 0.387231830 */, 18 },
  /* 5688 */  { MAD_F(0x06327934) /* 0.387322621 */, 18 },
  /* 5689 */  { MAD_F(0x0632d869) /* 0.387413416 */, 18 },
  /* 5690 */  { MAD_F(0x0633379f) /* 0.387504217 */, 18 },
  /* 5691 */  { MAD_F(0x063396d7) /* 0.387595023 */, 18 },
  /* 5692 */  { MAD_F(0x0633f610) /* 0.387685835 */, 18 },
  /* 5693 */  { MAD_F(0x0634554a) /* 0.387776652 */, 18 },
  /* 5694 */  { MAD_F(0x0634b486) /* 0.387867474 */, 18 },
  /* 5695 */  { MAD_F(0x063513c3) /* 0.387958301 */, 18 },

  /* 5696 */  { MAD_F(0x06357302) /* 0.388049134 */, 18 },
  /* 5697 */  { MAD_F(0x0635d242) /* 0.388139972 */, 18 },
  /* 5698 */  { MAD_F(0x06363184) /* 0.388230816 */, 18 },
  /* 5699 */  { MAD_F(0x063690c7) /* 0.388321665 */, 18 },
  /* 5700 */  { MAD_F(0x0636f00b) /* 0.388412519 */, 18 },
  /* 5701 */  { MAD_F(0x06374f51) /* 0.388503378 */, 18 },
  /* 5702 */  { MAD_F(0x0637ae99) /* 0.388594243 */, 18 },
  /* 5703 */  { MAD_F(0x06380de1) /* 0.388685113 */, 18 },
  /* 5704 */  { MAD_F(0x06386d2b) /* 0.388775988 */, 18 },
  /* 5705 */  { MAD_F(0x0638cc77) /* 0.388866869 */, 18 },
  /* 5706 */  { MAD_F(0x06392bc4) /* 0.388957755 */, 18 },
  /* 5707 */  { MAD_F(0x06398b12) /* 0.389048646 */, 18 },
  /* 5708 */  { MAD_F(0x0639ea62) /* 0.389139542 */, 18 },
  /* 5709 */  { MAD_F(0x063a49b4) /* 0.389230444 */, 18 },
  /* 5710 */  { MAD_F(0x063aa906) /* 0.389321352 */, 18 },
  /* 5711 */  { MAD_F(0x063b085a) /* 0.389412264 */, 18 },

  /* 5712 */  { MAD_F(0x063b67b0) /* 0.389503182 */, 18 },
  /* 5713 */  { MAD_F(0x063bc707) /* 0.389594105 */, 18 },
  /* 5714 */  { MAD_F(0x063c265f) /* 0.389685033 */, 18 },
  /* 5715 */  { MAD_F(0x063c85b9) /* 0.389775967 */, 18 },
  /* 5716 */  { MAD_F(0x063ce514) /* 0.389866906 */, 18 },
  /* 5717 */  { MAD_F(0x063d4471) /* 0.389957850 */, 18 },
  /* 5718 */  { MAD_F(0x063da3cf) /* 0.390048800 */, 18 },
  /* 5719 */  { MAD_F(0x063e032f) /* 0.390139755 */, 18 },
  /* 5720 */  { MAD_F(0x063e6290) /* 0.390230715 */, 18 },
  /* 5721 */  { MAD_F(0x063ec1f2) /* 0.390321681 */, 18 },
  /* 5722 */  { MAD_F(0x063f2156) /* 0.390412651 */, 18 },
  /* 5723 */  { MAD_F(0x063f80bb) /* 0.390503628 */, 18 },
  /* 5724 */  { MAD_F(0x063fe022) /* 0.390594609 */, 18 },
  /* 5725 */  { MAD_F(0x06403f8a) /* 0.390685596 */, 18 },
  /* 5726 */  { MAD_F(0x06409ef3) /* 0.390776588 */, 18 },
  /* 5727 */  { MAD_F(0x0640fe5e) /* 0.390867585 */, 18 },

  /* 5728 */  { MAD_F(0x06415dcb) /* 0.390958588 */, 18 },
  /* 5729 */  { MAD_F(0x0641bd38) /* 0.391049596 */, 18 },
  /* 5730 */  { MAD_F(0x06421ca7) /* 0.391140609 */, 18 },
  /* 5731 */  { MAD_F(0x06427c18) /* 0.391231627 */, 18 },
  /* 5732 */  { MAD_F(0x0642db8a) /* 0.391322651 */, 18 },
  /* 5733 */  { MAD_F(0x06433afd) /* 0.391413680 */, 18 },
  /* 5734 */  { MAD_F(0x06439a72) /* 0.391504714 */, 18 },
  /* 5735 */  { MAD_F(0x0643f9e9) /* 0.391595754 */, 18 },
  /* 5736 */  { MAD_F(0x06445960) /* 0.391686799 */, 18 },
  /* 5737 */  { MAD_F(0x0644b8d9) /* 0.391777849 */, 18 },
  /* 5738 */  { MAD_F(0x06451854) /* 0.391868905 */, 18 },
  /* 5739 */  { MAD_F(0x064577d0) /* 0.391959966 */, 18 },
  /* 5740 */  { MAD_F(0x0645d74d) /* 0.392051032 */, 18 },
  /* 5741 */  { MAD_F(0x064636cc) /* 0.392142103 */, 18 },
  /* 5742 */  { MAD_F(0x0646964c) /* 0.392233180 */, 18 },
  /* 5743 */  { MAD_F(0x0646f5ce) /* 0.392324262 */, 18 },

  /* 5744 */  { MAD_F(0x06475551) /* 0.392415349 */, 18 },
  /* 5745 */  { MAD_F(0x0647b4d5) /* 0.392506442 */, 18 },
  /* 5746 */  { MAD_F(0x0648145b) /* 0.392597540 */, 18 },
  /* 5747 */  { MAD_F(0x064873e3) /* 0.392688643 */, 18 },
  /* 5748 */  { MAD_F(0x0648d36b) /* 0.392779751 */, 18 },
  /* 5749 */  { MAD_F(0x064932f6) /* 0.392870865 */, 18 },
  /* 5750 */  { MAD_F(0x06499281) /* 0.392961984 */, 18 },
  /* 5751 */  { MAD_F(0x0649f20e) /* 0.393053108 */, 18 },
  /* 5752 */  { MAD_F(0x064a519c) /* 0.393144238 */, 18 },
  /* 5753 */  { MAD_F(0x064ab12c) /* 0.393235372 */, 18 },
  /* 5754 */  { MAD_F(0x064b10be) /* 0.393326513 */, 18 },
  /* 5755 */  { MAD_F(0x064b7050) /* 0.393417658 */, 18 },
  /* 5756 */  { MAD_F(0x064bcfe4) /* 0.393508809 */, 18 },
  /* 5757 */  { MAD_F(0x064c2f7a) /* 0.393599965 */, 18 },
  /* 5758 */  { MAD_F(0x064c8f11) /* 0.393691126 */, 18 },
  /* 5759 */  { MAD_F(0x064ceea9) /* 0.393782292 */, 18 },

  /* 5760 */  { MAD_F(0x064d4e43) /* 0.393873464 */, 18 },
  /* 5761 */  { MAD_F(0x064dadde) /* 0.393964641 */, 18 },
  /* 5762 */  { MAD_F(0x064e0d7a) /* 0.394055823 */, 18 },
  /* 5763 */  { MAD_F(0x064e6d18) /* 0.394147011 */, 18 },
  /* 5764 */  { MAD_F(0x064eccb8) /* 0.394238204 */, 18 },
  /* 5765 */  { MAD_F(0x064f2c59) /* 0.394329402 */, 18 },
  /* 5766 */  { MAD_F(0x064f8bfb) /* 0.394420605 */, 18 },
  /* 5767 */  { MAD_F(0x064feb9e) /* 0.394511814 */, 18 },
  /* 5768 */  { MAD_F(0x06504b44) /* 0.394603028 */, 18 },
  /* 5769 */  { MAD_F(0x0650aaea) /* 0.394694247 */, 18 },
  /* 5770 */  { MAD_F(0x06510a92) /* 0.394785472 */, 18 },
  /* 5771 */  { MAD_F(0x06516a3b) /* 0.394876702 */, 18 },
  /* 5772 */  { MAD_F(0x0651c9e6) /* 0.394967937 */, 18 },
  /* 5773 */  { MAD_F(0x06522992) /* 0.395059177 */, 18 },
  /* 5774 */  { MAD_F(0x06528940) /* 0.395150423 */, 18 },
  /* 5775 */  { MAD_F(0x0652e8ef) /* 0.395241673 */, 18 },

  /* 5776 */  { MAD_F(0x0653489f) /* 0.395332930 */, 18 },
  /* 5777 */  { MAD_F(0x0653a851) /* 0.395424191 */, 18 },
  /* 5778 */  { MAD_F(0x06540804) /* 0.395515458 */, 18 },
  /* 5779 */  { MAD_F(0x065467b9) /* 0.395606730 */, 18 },
  /* 5780 */  { MAD_F(0x0654c76f) /* 0.395698007 */, 18 },
  /* 5781 */  { MAD_F(0x06552726) /* 0.395789289 */, 18 },
  /* 5782 */  { MAD_F(0x065586df) /* 0.395880577 */, 18 },
  /* 5783 */  { MAD_F(0x0655e699) /* 0.395971870 */, 18 },
  /* 5784 */  { MAD_F(0x06564655) /* 0.396063168 */, 18 },
  /* 5785 */  { MAD_F(0x0656a612) /* 0.396154472 */, 18 },
  /* 5786 */  { MAD_F(0x065705d0) /* 0.396245780 */, 18 },
  /* 5787 */  { MAD_F(0x06576590) /* 0.396337094 */, 18 },
  /* 5788 */  { MAD_F(0x0657c552) /* 0.396428414 */, 18 },
  /* 5789 */  { MAD_F(0x06582514) /* 0.396519738 */, 18 },
  /* 5790 */  { MAD_F(0x065884d9) /* 0.396611068 */, 18 },
  /* 5791 */  { MAD_F(0x0658e49e) /* 0.396702403 */, 18 },

  /* 5792 */  { MAD_F(0x06594465) /* 0.396793743 */, 18 },
  /* 5793 */  { MAD_F(0x0659a42e) /* 0.396885089 */, 18 },
  /* 5794 */  { MAD_F(0x065a03f7) /* 0.396976440 */, 18 },
  /* 5795 */  { MAD_F(0x065a63c3) /* 0.397067796 */, 18 },
  /* 5796 */  { MAD_F(0x065ac38f) /* 0.397159157 */, 18 },
  /* 5797 */  { MAD_F(0x065b235d) /* 0.397250524 */, 18 },
  /* 5798 */  { MAD_F(0x065b832d) /* 0.397341896 */, 18 },
  /* 5799 */  { MAD_F(0x065be2fe) /* 0.397433273 */, 18 },
  /* 5800 */  { MAD_F(0x065c42d0) /* 0.397524655 */, 18 },
  /* 5801 */  { MAD_F(0x065ca2a3) /* 0.397616043 */, 18 },
  /* 5802 */  { MAD_F(0x065d0279) /* 0.397707436 */, 18 },
  /* 5803 */  { MAD_F(0x065d624f) /* 0.397798834 */, 18 },
  /* 5804 */  { MAD_F(0x065dc227) /* 0.397890237 */, 18 },
  /* 5805 */  { MAD_F(0x065e2200) /* 0.397981646 */, 18 },
  /* 5806 */  { MAD_F(0x065e81db) /* 0.398073059 */, 18 },
  /* 5807 */  { MAD_F(0x065ee1b7) /* 0.398164479 */, 18 },

  /* 5808 */  { MAD_F(0x065f4195) /* 0.398255903 */, 18 },
  /* 5809 */  { MAD_F(0x065fa174) /* 0.398347333 */, 18 },
  /* 5810 */  { MAD_F(0x06600154) /* 0.398438767 */, 18 },
  /* 5811 */  { MAD_F(0x06606136) /* 0.398530207 */, 18 },
  /* 5812 */  { MAD_F(0x0660c119) /* 0.398621653 */, 18 },
  /* 5813 */  { MAD_F(0x066120fd) /* 0.398713103 */, 18 },
  /* 5814 */  { MAD_F(0x066180e3) /* 0.398804559 */, 18 },
  /* 5815 */  { MAD_F(0x0661e0cb) /* 0.398896020 */, 18 },
  /* 5816 */  { MAD_F(0x066240b4) /* 0.398987487 */, 18 },
  /* 5817 */  { MAD_F(0x0662a09e) /* 0.399078958 */, 18 },
  /* 5818 */  { MAD_F(0x06630089) /* 0.399170435 */, 18 },
  /* 5819 */  { MAD_F(0x06636077) /* 0.399261917 */, 18 },
  /* 5820 */  { MAD_F(0x0663c065) /* 0.399353404 */, 18 },
  /* 5821 */  { MAD_F(0x06642055) /* 0.399444897 */, 18 },
  /* 5822 */  { MAD_F(0x06648046) /* 0.399536395 */, 18 },
  /* 5823 */  { MAD_F(0x0664e039) /* 0.399627898 */, 18 },

  /* 5824 */  { MAD_F(0x0665402d) /* 0.399719406 */, 18 },
  /* 5825 */  { MAD_F(0x0665a022) /* 0.399810919 */, 18 },
  /* 5826 */  { MAD_F(0x06660019) /* 0.399902438 */, 18 },
  /* 5827 */  { MAD_F(0x06666011) /* 0.399993962 */, 18 },
  /* 5828 */  { MAD_F(0x0666c00b) /* 0.400085491 */, 18 },
  /* 5829 */  { MAD_F(0x06672006) /* 0.400177026 */, 18 },
  /* 5830 */  { MAD_F(0x06678003) /* 0.400268565 */, 18 },
  /* 5831 */  { MAD_F(0x0667e000) /* 0.400360110 */, 18 },
  /* 5832 */  { MAD_F(0x06684000) /* 0.400451660 */, 18 },
  /* 5833 */  { MAD_F(0x0668a000) /* 0.400543216 */, 18 },
  /* 5834 */  { MAD_F(0x06690003) /* 0.400634776 */, 18 },
  /* 5835 */  { MAD_F(0x06696006) /* 0.400726342 */, 18 },
  /* 5836 */  { MAD_F(0x0669c00b) /* 0.400817913 */, 18 },
  /* 5837 */  { MAD_F(0x066a2011) /* 0.400909489 */, 18 },
  /* 5838 */  { MAD_F(0x066a8019) /* 0.401001071 */, 18 },
  /* 5839 */  { MAD_F(0x066ae022) /* 0.401092657 */, 18 },

  /* 5840 */  { MAD_F(0x066b402d) /* 0.401184249 */, 18 },
  /* 5841 */  { MAD_F(0x066ba039) /* 0.401275847 */, 18 },
  /* 5842 */  { MAD_F(0x066c0046) /* 0.401367449 */, 18 },
  /* 5843 */  { MAD_F(0x066c6055) /* 0.401459057 */, 18 },
  /* 5844 */  { MAD_F(0x066cc065) /* 0.401550670 */, 18 },
  /* 5845 */  { MAD_F(0x066d2076) /* 0.401642288 */, 18 },
  /* 5846 */  { MAD_F(0x066d8089) /* 0.401733911 */, 18 },
  /* 5847 */  { MAD_F(0x066de09e) /* 0.401825540 */, 18 },
  /* 5848 */  { MAD_F(0x066e40b3) /* 0.401917173 */, 18 },
  /* 5849 */  { MAD_F(0x066ea0cb) /* 0.402008812 */, 18 },
  /* 5850 */  { MAD_F(0x066f00e3) /* 0.402100457 */, 18 },
  /* 5851 */  { MAD_F(0x066f60fd) /* 0.402192106 */, 18 },
  /* 5852 */  { MAD_F(0x066fc118) /* 0.402283761 */, 18 },
  /* 5853 */  { MAD_F(0x06702135) /* 0.402375420 */, 18 },
  /* 5854 */  { MAD_F(0x06708153) /* 0.402467086 */, 18 },
  /* 5855 */  { MAD_F(0x0670e173) /* 0.402558756 */, 18 },

  /* 5856 */  { MAD_F(0x06714194) /* 0.402650431 */, 18 },
  /* 5857 */  { MAD_F(0x0671a1b6) /* 0.402742112 */, 18 },
  /* 5858 */  { MAD_F(0x067201da) /* 0.402833798 */, 18 },
  /* 5859 */  { MAD_F(0x067261ff) /* 0.402925489 */, 18 },
  /* 5860 */  { MAD_F(0x0672c226) /* 0.403017186 */, 18 },
  /* 5861 */  { MAD_F(0x0673224e) /* 0.403108887 */, 18 },
  /* 5862 */  { MAD_F(0x06738277) /* 0.403200594 */, 18 },
  /* 5863 */  { MAD_F(0x0673e2a2) /* 0.403292306 */, 18 },
  /* 5864 */  { MAD_F(0x067442ce) /* 0.403384024 */, 18 },
  /* 5865 */  { MAD_F(0x0674a2fc) /* 0.403475746 */, 18 },
  /* 5866 */  { MAD_F(0x0675032b) /* 0.403567474 */, 18 },
  /* 5867 */  { MAD_F(0x0675635b) /* 0.403659207 */, 18 },
  /* 5868 */  { MAD_F(0x0675c38d) /* 0.403750945 */, 18 },
  /* 5869 */  { MAD_F(0x067623c0) /* 0.403842688 */, 18 },
  /* 5870 */  { MAD_F(0x067683f4) /* 0.403934437 */, 18 },
  /* 5871 */  { MAD_F(0x0676e42a) /* 0.404026190 */, 18 },

  /* 5872 */  { MAD_F(0x06774462) /* 0.404117949 */, 18 },
  /* 5873 */  { MAD_F(0x0677a49b) /* 0.404209714 */, 18 },
  /* 5874 */  { MAD_F(0x067804d5) /* 0.404301483 */, 18 },
  /* 5875 */  { MAD_F(0x06786510) /* 0.404393258 */, 18 },
  /* 5876 */  { MAD_F(0x0678c54d) /* 0.404485037 */, 18 },
  /* 5877 */  { MAD_F(0x0679258c) /* 0.404576822 */, 18 },
  /* 5878 */  { MAD_F(0x067985cb) /* 0.404668613 */, 18 },
  /* 5879 */  { MAD_F(0x0679e60c) /* 0.404760408 */, 18 },
  /* 5880 */  { MAD_F(0x067a464f) /* 0.404852209 */, 18 },
  /* 5881 */  { MAD_F(0x067aa693) /* 0.404944014 */, 18 },
  /* 5882 */  { MAD_F(0x067b06d8) /* 0.405035825 */, 18 },
  /* 5883 */  { MAD_F(0x067b671f) /* 0.405127642 */, 18 },
  /* 5884 */  { MAD_F(0x067bc767) /* 0.405219463 */, 18 },
  /* 5885 */  { MAD_F(0x067c27b1) /* 0.405311290 */, 18 },
  /* 5886 */  { MAD_F(0x067c87fc) /* 0.405403122 */, 18 },
  /* 5887 */  { MAD_F(0x067ce848) /* 0.405494959 */, 18 },

  /* 5888 */  { MAD_F(0x067d4896) /* 0.405586801 */, 18 },
  /* 5889 */  { MAD_F(0x067da8e5) /* 0.405678648 */, 18 },
  /* 5890 */  { MAD_F(0x067e0935) /* 0.405770501 */, 18 },
  /* 5891 */  { MAD_F(0x067e6987) /* 0.405862359 */, 18 },
  /* 5892 */  { MAD_F(0x067ec9da) /* 0.405954222 */, 18 },
  /* 5893 */  { MAD_F(0x067f2a2f) /* 0.406046090 */, 18 },
  /* 5894 */  { MAD_F(0x067f8a85) /* 0.406137963 */, 18 },
  /* 5895 */  { MAD_F(0x067feadd) /* 0.406229842 */, 18 },
  /* 5896 */  { MAD_F(0x06804b36) /* 0.406321726 */, 18 },
  /* 5897 */  { MAD_F(0x0680ab90) /* 0.406413615 */, 18 },
  /* 5898 */  { MAD_F(0x06810beb) /* 0.406505509 */, 18 },
  /* 5899 */  { MAD_F(0x06816c49) /* 0.406597408 */, 18 },
  /* 5900 */  { MAD_F(0x0681cca7) /* 0.406689313 */, 18 },
  /* 5901 */  { MAD_F(0x06822d07) /* 0.406781223 */, 18 },
  /* 5902 */  { MAD_F(0x06828d68) /* 0.406873138 */, 18 },
  /* 5903 */  { MAD_F(0x0682edcb) /* 0.406965058 */, 18 },

  /* 5904 */  { MAD_F(0x06834e2f) /* 0.407056983 */, 18 },
  /* 5905 */  { MAD_F(0x0683ae94) /* 0.407148914 */, 18 },
  /* 5906 */  { MAD_F(0x06840efb) /* 0.407240850 */, 18 },
  /* 5907 */  { MAD_F(0x06846f63) /* 0.407332791 */, 18 },
  /* 5908 */  { MAD_F(0x0684cfcd) /* 0.407424737 */, 18 },
  /* 5909 */  { MAD_F(0x06853038) /* 0.407516688 */, 18 },
  /* 5910 */  { MAD_F(0x068590a4) /* 0.407608645 */, 18 },
  /* 5911 */  { MAD_F(0x0685f112) /* 0.407700606 */, 18 },
  /* 5912 */  { MAD_F(0x06865181) /* 0.407792573 */, 18 },
  /* 5913 */  { MAD_F(0x0686b1f2) /* 0.407884545 */, 18 },
  /* 5914 */  { MAD_F(0x06871264) /* 0.407976522 */, 18 },
  /* 5915 */  { MAD_F(0x068772d7) /* 0.408068505 */, 18 },
  /* 5916 */  { MAD_F(0x0687d34c) /* 0.408160492 */, 18 },
  /* 5917 */  { MAD_F(0x068833c2) /* 0.408252485 */, 18 },
  /* 5918 */  { MAD_F(0x06889439) /* 0.408344483 */, 18 },
  /* 5919 */  { MAD_F(0x0688f4b2) /* 0.408436486 */, 18 },

  /* 5920 */  { MAD_F(0x0689552c) /* 0.408528495 */, 18 },
  /* 5921 */  { MAD_F(0x0689b5a8) /* 0.408620508 */, 18 },
  /* 5922 */  { MAD_F(0x068a1625) /* 0.408712527 */, 18 },
  /* 5923 */  { MAD_F(0x068a76a4) /* 0.408804551 */, 18 },
  /* 5924 */  { MAD_F(0x068ad724) /* 0.408896580 */, 18 },
  /* 5925 */  { MAD_F(0x068b37a5) /* 0.408988614 */, 18 },
  /* 5926 */  { MAD_F(0x068b9827) /* 0.409080653 */, 18 },
  /* 5927 */  { MAD_F(0x068bf8ac) /* 0.409172698 */, 18 },
  /* 5928 */  { MAD_F(0x068c5931) /* 0.409264748 */, 18 },
  /* 5929 */  { MAD_F(0x068cb9b8) /* 0.409356803 */, 18 },
  /* 5930 */  { MAD_F(0x068d1a40) /* 0.409448863 */, 18 },
  /* 5931 */  { MAD_F(0x068d7aca) /* 0.409540928 */, 18 },
  /* 5932 */  { MAD_F(0x068ddb54) /* 0.409632999 */, 18 },
  /* 5933 */  { MAD_F(0x068e3be1) /* 0.409725074 */, 18 },
  /* 5934 */  { MAD_F(0x068e9c6f) /* 0.409817155 */, 18 },
  /* 5935 */  { MAD_F(0x068efcfe) /* 0.409909241 */, 18 },

  /* 5936 */  { MAD_F(0x068f5d8e) /* 0.410001332 */, 18 },
  /* 5937 */  { MAD_F(0x068fbe20) /* 0.410093428 */, 18 },
  /* 5938 */  { MAD_F(0x06901eb4) /* 0.410185530 */, 18 },
  /* 5939 */  { MAD_F(0x06907f48) /* 0.410277637 */, 18 },
  /* 5940 */  { MAD_F(0x0690dfde) /* 0.410369748 */, 18 },
  /* 5941 */  { MAD_F(0x06914076) /* 0.410461865 */, 18 },
  /* 5942 */  { MAD_F(0x0691a10f) /* 0.410553988 */, 18 },
  /* 5943 */  { MAD_F(0x069201a9) /* 0.410646115 */, 18 },
  /* 5944 */  { MAD_F(0x06926245) /* 0.410738247 */, 18 },
  /* 5945 */  { MAD_F(0x0692c2e2) /* 0.410830385 */, 18 },
  /* 5946 */  { MAD_F(0x06932380) /* 0.410922528 */, 18 },
  /* 5947 */  { MAD_F(0x06938420) /* 0.411014676 */, 18 },
  /* 5948 */  { MAD_F(0x0693e4c1) /* 0.411106829 */, 18 },
  /* 5949 */  { MAD_F(0x06944563) /* 0.411198987 */, 18 },
  /* 5950 */  { MAD_F(0x0694a607) /* 0.411291151 */, 18 },
  /* 5951 */  { MAD_F(0x069506ad) /* 0.411383320 */, 18 },

  /* 5952 */  { MAD_F(0x06956753) /* 0.411475493 */, 18 },
  /* 5953 */  { MAD_F(0x0695c7fc) /* 0.411567672 */, 18 },
  /* 5954 */  { MAD_F(0x069628a5) /* 0.411659857 */, 18 },
  /* 5955 */  { MAD_F(0x06968950) /* 0.411752046 */, 18 },
  /* 5956 */  { MAD_F(0x0696e9fc) /* 0.411844240 */, 18 },
  /* 5957 */  { MAD_F(0x06974aaa) /* 0.411936440 */, 18 },
  /* 5958 */  { MAD_F(0x0697ab59) /* 0.412028645 */, 18 },
  /* 5959 */  { MAD_F(0x06980c09) /* 0.412120855 */, 18 },
  /* 5960 */  { MAD_F(0x06986cbb) /* 0.412213070 */, 18 },
  /* 5961 */  { MAD_F(0x0698cd6e) /* 0.412305290 */, 18 },
  /* 5962 */  { MAD_F(0x06992e23) /* 0.412397516 */, 18 },
  /* 5963 */  { MAD_F(0x06998ed9) /* 0.412489746 */, 18 },
  /* 5964 */  { MAD_F(0x0699ef90) /* 0.412581982 */, 18 },
  /* 5965 */  { MAD_F(0x069a5049) /* 0.412674223 */, 18 },
  /* 5966 */  { MAD_F(0x069ab103) /* 0.412766469 */, 18 },
  /* 5967 */  { MAD_F(0x069b11bf) /* 0.412858720 */, 18 },

  /* 5968 */  { MAD_F(0x069b727b) /* 0.412950976 */, 18 },
  /* 5969 */  { MAD_F(0x069bd33a) /* 0.413043238 */, 18 },
  /* 5970 */  { MAD_F(0x069c33f9) /* 0.413135505 */, 18 },
  /* 5971 */  { MAD_F(0x069c94ba) /* 0.413227776 */, 18 },
  /* 5972 */  { MAD_F(0x069cf57d) /* 0.413320053 */, 18 },
  /* 5973 */  { MAD_F(0x069d5641) /* 0.413412335 */, 18 },
  /* 5974 */  { MAD_F(0x069db706) /* 0.413504623 */, 18 },
  /* 5975 */  { MAD_F(0x069e17cc) /* 0.413596915 */, 18 },
  /* 5976 */  { MAD_F(0x069e7894) /* 0.413689213 */, 18 },
  /* 5977 */  { MAD_F(0x069ed95e) /* 0.413781515 */, 18 },
  /* 5978 */  { MAD_F(0x069f3a28) /* 0.413873823 */, 18 },
  /* 5979 */  { MAD_F(0x069f9af4) /* 0.413966136 */, 18 },
  /* 5980 */  { MAD_F(0x069ffbc2) /* 0.414058454 */, 18 },
  /* 5981 */  { MAD_F(0x06a05c91) /* 0.414150778 */, 18 },
  /* 5982 */  { MAD_F(0x06a0bd61) /* 0.414243106 */, 18 },
  /* 5983 */  { MAD_F(0x06a11e32) /* 0.414335440 */, 18 },

  /* 5984 */  { MAD_F(0x06a17f05) /* 0.414427779 */, 18 },
  /* 5985 */  { MAD_F(0x06a1dfda) /* 0.414520122 */, 18 },
  /* 5986 */  { MAD_F(0x06a240b0) /* 0.414612471 */, 18 },
  /* 5987 */  { MAD_F(0x06a2a187) /* 0.414704826 */, 18 },
  /* 5988 */  { MAD_F(0x06a3025f) /* 0.414797185 */, 18 },
  /* 5989 */  { MAD_F(0x06a36339) /* 0.414889549 */, 18 },
  /* 5990 */  { MAD_F(0x06a3c414) /* 0.414981919 */, 18 },
  /* 5991 */  { MAD_F(0x06a424f1) /* 0.415074294 */, 18 },
  /* 5992 */  { MAD_F(0x06a485cf) /* 0.415166674 */, 18 },
  /* 5993 */  { MAD_F(0x06a4e6ae) /* 0.415259059 */, 18 },
  /* 5994 */  { MAD_F(0x06a5478f) /* 0.415351449 */, 18 },
  /* 5995 */  { MAD_F(0x06a5a871) /* 0.415443844 */, 18 },
  /* 5996 */  { MAD_F(0x06a60955) /* 0.415536244 */, 18 },
  /* 5997 */  { MAD_F(0x06a66a3a) /* 0.415628650 */, 18 },
  /* 5998 */  { MAD_F(0x06a6cb20) /* 0.415721061 */, 18 },
  /* 5999 */  { MAD_F(0x06a72c08) /* 0.415813476 */, 18 },

  /* 6000 */  { MAD_F(0x06a78cf1) /* 0.415905897 */, 18 },
  /* 6001 */  { MAD_F(0x06a7eddb) /* 0.415998324 */, 18 },
  /* 6002 */  { MAD_F(0x06a84ec7) /* 0.416090755 */, 18 },
  /* 6003 */  { MAD_F(0x06a8afb4) /* 0.416183191 */, 18 },
  /* 6004 */  { MAD_F(0x06a910a3) /* 0.416275633 */, 18 },
  /* 6005 */  { MAD_F(0x06a97193) /* 0.416368079 */, 18 },
  /* 6006 */  { MAD_F(0x06a9d284) /* 0.416460531 */, 18 },
  /* 6007 */  { MAD_F(0x06aa3377) /* 0.416552988 */, 18 },
  /* 6008 */  { MAD_F(0x06aa946b) /* 0.416645450 */, 18 },
  /* 6009 */  { MAD_F(0x06aaf561) /* 0.416737917 */, 18 },
  /* 6010 */  { MAD_F(0x06ab5657) /* 0.416830389 */, 18 },
  /* 6011 */  { MAD_F(0x06abb750) /* 0.416922867 */, 18 },
  /* 6012 */  { MAD_F(0x06ac1849) /* 0.417015349 */, 18 },
  /* 6013 */  { MAD_F(0x06ac7944) /* 0.417107837 */, 18 },
  /* 6014 */  { MAD_F(0x06acda41) /* 0.417200330 */, 18 },
  /* 6015 */  { MAD_F(0x06ad3b3e) /* 0.417292828 */, 18 },

  /* 6016 */  { MAD_F(0x06ad9c3d) /* 0.417385331 */, 18 },
  /* 6017 */  { MAD_F(0x06adfd3e) /* 0.417477839 */, 18 },
  /* 6018 */  { MAD_F(0x06ae5e40) /* 0.417570352 */, 18 },
  /* 6019 */  { MAD_F(0x06aebf43) /* 0.417662871 */, 18 },
  /* 6020 */  { MAD_F(0x06af2047) /* 0.417755394 */, 18 },
  /* 6021 */  { MAD_F(0x06af814d) /* 0.417847923 */, 18 },
  /* 6022 */  { MAD_F(0x06afe255) /* 0.417940457 */, 18 },
  /* 6023 */  { MAD_F(0x06b0435e) /* 0.418032996 */, 18 },
  /* 6024 */  { MAD_F(0x06b0a468) /* 0.418125540 */, 18 },
  /* 6025 */  { MAD_F(0x06b10573) /* 0.418218089 */, 18 },
  /* 6026 */  { MAD_F(0x06b16680) /* 0.418310643 */, 18 },
  /* 6027 */  { MAD_F(0x06b1c78e) /* 0.418403203 */, 18 },
  /* 6028 */  { MAD_F(0x06b2289e) /* 0.418495767 */, 18 },
  /* 6029 */  { MAD_F(0x06b289af) /* 0.418588337 */, 18 },
  /* 6030 */  { MAD_F(0x06b2eac1) /* 0.418680911 */, 18 },
  /* 6031 */  { MAD_F(0x06b34bd5) /* 0.418773491 */, 18 },

  /* 6032 */  { MAD_F(0x06b3acea) /* 0.418866076 */, 18 },
  /* 6033 */  { MAD_F(0x06b40e00) /* 0.418958666 */, 18 },
  /* 6034 */  { MAD_F(0x06b46f18) /* 0.419051262 */, 18 },
  /* 6035 */  { MAD_F(0x06b4d031) /* 0.419143862 */, 18 },
  /* 6036 */  { MAD_F(0x06b5314c) /* 0.419236467 */, 18 },
  /* 6037 */  { MAD_F(0x06b59268) /* 0.419329078 */, 18 },
  /* 6038 */  { MAD_F(0x06b5f385) /* 0.419421694 */, 18 },
  /* 6039 */  { MAD_F(0x06b654a4) /* 0.419514314 */, 18 },
  /* 6040 */  { MAD_F(0x06b6b5c4) /* 0.419606940 */, 18 },
  /* 6041 */  { MAD_F(0x06b716e6) /* 0.419699571 */, 18 },
  /* 6042 */  { MAD_F(0x06b77808) /* 0.419792208 */, 18 },
  /* 6043 */  { MAD_F(0x06b7d92d) /* 0.419884849 */, 18 },
  /* 6044 */  { MAD_F(0x06b83a52) /* 0.419977495 */, 18 },
  /* 6045 */  { MAD_F(0x06b89b79) /* 0.420070147 */, 18 },
  /* 6046 */  { MAD_F(0x06b8fca1) /* 0.420162803 */, 18 },
  /* 6047 */  { MAD_F(0x06b95dcb) /* 0.420255465 */, 18 },

  /* 6048 */  { MAD_F(0x06b9bef6) /* 0.420348132 */, 18 },
  /* 6049 */  { MAD_F(0x06ba2023) /* 0.420440803 */, 18 },
  /* 6050 */  { MAD_F(0x06ba8150) /* 0.420533481 */, 18 },
  /* 6051 */  { MAD_F(0x06bae280) /* 0.420626163 */, 18 },
  /* 6052 */  { MAD_F(0x06bb43b0) /* 0.420718850 */, 18 },
  /* 6053 */  { MAD_F(0x06bba4e2) /* 0.420811542 */, 18 },
  /* 6054 */  { MAD_F(0x06bc0615) /* 0.420904240 */, 18 },
  /* 6055 */  { MAD_F(0x06bc674a) /* 0.420996942 */, 18 },
  /* 6056 */  { MAD_F(0x06bcc880) /* 0.421089650 */, 18 },
  /* 6057 */  { MAD_F(0x06bd29b7) /* 0.421182362 */, 18 },
  /* 6058 */  { MAD_F(0x06bd8af0) /* 0.421275080 */, 18 },
  /* 6059 */  { MAD_F(0x06bdec2a) /* 0.421367803 */, 18 },
  /* 6060 */  { MAD_F(0x06be4d66) /* 0.421460531 */, 18 },
  /* 6061 */  { MAD_F(0x06beaea3) /* 0.421553264 */, 18 },
  /* 6062 */  { MAD_F(0x06bf0fe1) /* 0.421646003 */, 18 },
  /* 6063 */  { MAD_F(0x06bf7120) /* 0.421738746 */, 18 },

  /* 6064 */  { MAD_F(0x06bfd261) /* 0.421831494 */, 18 },
  /* 6065 */  { MAD_F(0x06c033a4) /* 0.421924248 */, 18 },
  /* 6066 */  { MAD_F(0x06c094e7) /* 0.422017007 */, 18 },
  /* 6067 */  { MAD_F(0x06c0f62c) /* 0.422109770 */, 18 },
  /* 6068 */  { MAD_F(0x06c15773) /* 0.422202539 */, 18 },
  /* 6069 */  { MAD_F(0x06c1b8bb) /* 0.422295313 */, 18 },
  /* 6070 */  { MAD_F(0x06c21a04) /* 0.422388092 */, 18 },
  /* 6071 */  { MAD_F(0x06c27b4e) /* 0.422480876 */, 18 },
  /* 6072 */  { MAD_F(0x06c2dc9a) /* 0.422573665 */, 18 },
  /* 6073 */  { MAD_F(0x06c33de8) /* 0.422666460 */, 18 },
  /* 6074 */  { MAD_F(0x06c39f36) /* 0.422759259 */, 18 },
  /* 6075 */  { MAD_F(0x06c40086) /* 0.422852064 */, 18 },
  /* 6076 */  { MAD_F(0x06c461d8) /* 0.422944873 */, 18 },
  /* 6077 */  { MAD_F(0x06c4c32a) /* 0.423037688 */, 18 },
  /* 6078 */  { MAD_F(0x06c5247f) /* 0.423130508 */, 18 },
  /* 6079 */  { MAD_F(0x06c585d4) /* 0.423223333 */, 18 },

  /* 6080 */  { MAD_F(0x06c5e72b) /* 0.423316162 */, 18 },
  /* 6081 */  { MAD_F(0x06c64883) /* 0.423408997 */, 18 },
  /* 6082 */  { MAD_F(0x06c6a9dd) /* 0.423501838 */, 18 },
  /* 6083 */  { MAD_F(0x06c70b38) /* 0.423594683 */, 18 },
  /* 6084 */  { MAD_F(0x06c76c94) /* 0.423687533 */, 18 },
  /* 6085 */  { MAD_F(0x06c7cdf2) /* 0.423780389 */, 18 },
  /* 6086 */  { MAD_F(0x06c82f51) /* 0.423873249 */, 18 },
  /* 6087 */  { MAD_F(0x06c890b1) /* 0.423966115 */, 18 },
  /* 6088 */  { MAD_F(0x06c8f213) /* 0.424058985 */, 18 },
  /* 6089 */  { MAD_F(0x06c95376) /* 0.424151861 */, 18 },
  /* 6090 */  { MAD_F(0x06c9b4da) /* 0.424244742 */, 18 },
  /* 6091 */  { MAD_F(0x06ca1640) /* 0.424337628 */, 18 },
  /* 6092 */  { MAD_F(0x06ca77a8) /* 0.424430519 */, 18 },
  /* 6093 */  { MAD_F(0x06cad910) /* 0.424523415 */, 18 },
  /* 6094 */  { MAD_F(0x06cb3a7a) /* 0.424616316 */, 18 },
  /* 6095 */  { MAD_F(0x06cb9be5) /* 0.424709222 */, 18 },

  /* 6096 */  { MAD_F(0x06cbfd52) /* 0.424802133 */, 18 },
  /* 6097 */  { MAD_F(0x06cc5ec0) /* 0.424895050 */, 18 },
  /* 6098 */  { MAD_F(0x06ccc030) /* 0.424987971 */, 18 },
  /* 6099 */  { MAD_F(0x06cd21a0) /* 0.425080898 */, 18 },
  /* 6100 */  { MAD_F(0x06cd8313) /* 0.425173829 */, 18 },
  /* 6101 */  { MAD_F(0x06cde486) /* 0.425266766 */, 18 },
  /* 6102 */  { MAD_F(0x06ce45fb) /* 0.425359708 */, 18 },
  /* 6103 */  { MAD_F(0x06cea771) /* 0.425452655 */, 18 },
  /* 6104 */  { MAD_F(0x06cf08e9) /* 0.425545607 */, 18 },
  /* 6105 */  { MAD_F(0x06cf6a62) /* 0.425638564 */, 18 },
  /* 6106 */  { MAD_F(0x06cfcbdc) /* 0.425731526 */, 18 },
  /* 6107 */  { MAD_F(0x06d02d58) /* 0.425824493 */, 18 },
  /* 6108 */  { MAD_F(0x06d08ed5) /* 0.425917465 */, 18 },
  /* 6109 */  { MAD_F(0x06d0f053) /* 0.426010443 */, 18 },
  /* 6110 */  { MAD_F(0x06d151d3) /* 0.426103425 */, 18 },
  /* 6111 */  { MAD_F(0x06d1b354) /* 0.426196412 */, 18 },

  /* 6112 */  { MAD_F(0x06d214d7) /* 0.426289405 */, 18 },
  /* 6113 */  { MAD_F(0x06d2765a) /* 0.426382403 */, 18 },
  /* 6114 */  { MAD_F(0x06d2d7e0) /* 0.426475405 */, 18 },
  /* 6115 */  { MAD_F(0x06d33966) /* 0.426568413 */, 18 },
  /* 6116 */  { MAD_F(0x06d39aee) /* 0.426661426 */, 18 },
  /* 6117 */  { MAD_F(0x06d3fc77) /* 0.426754444 */, 18 },
  /* 6118 */  { MAD_F(0x06d45e02) /* 0.426847467 */, 18 },
  /* 6119 */  { MAD_F(0x06d4bf8e) /* 0.426940495 */, 18 },
  /* 6120 */  { MAD_F(0x06d5211c) /* 0.427033528 */, 18 },
  /* 6121 */  { MAD_F(0x06d582aa) /* 0.427126566 */, 18 },
  /* 6122 */  { MAD_F(0x06d5e43a) /* 0.427219609 */, 18 },
  /* 6123 */  { MAD_F(0x06d645cc) /* 0.427312657 */, 18 },
  /* 6124 */  { MAD_F(0x06d6a75f) /* 0.427405711 */, 18 },
  /* 6125 */  { MAD_F(0x06d708f3) /* 0.427498769 */, 18 },
  /* 6126 */  { MAD_F(0x06d76a88) /* 0.427591833 */, 18 },
  /* 6127 */  { MAD_F(0x06d7cc1f) /* 0.427684901 */, 18 },

  /* 6128 */  { MAD_F(0x06d82db8) /* 0.427777975 */, 18 },
  /* 6129 */  { MAD_F(0x06d88f51) /* 0.427871054 */, 18 },
  /* 6130 */  { MAD_F(0x06d8f0ec) /* 0.427964137 */, 18 },
  /* 6131 */  { MAD_F(0x06d95288) /* 0.428057226 */, 18 },
  /* 6132 */  { MAD_F(0x06d9b426) /* 0.428150320 */, 18 },
  /* 6133 */  { MAD_F(0x06da15c5) /* 0.428243419 */, 18 },
  /* 6134 */  { MAD_F(0x06da7766) /* 0.428336523 */, 18 },
  /* 6135 */  { MAD_F(0x06dad907) /* 0.428429632 */, 18 },
  /* 6136 */  { MAD_F(0x06db3aaa) /* 0.428522746 */, 18 },
  /* 6137 */  { MAD_F(0x06db9c4f) /* 0.428615865 */, 18 },
  /* 6138 */  { MAD_F(0x06dbfdf5) /* 0.428708989 */, 18 },
  /* 6139 */  { MAD_F(0x06dc5f9c) /* 0.428802119 */, 18 },
  /* 6140 */  { MAD_F(0x06dcc145) /* 0.428895253 */, 18 },
  /* 6141 */  { MAD_F(0x06dd22ee) /* 0.428988392 */, 18 },
  /* 6142 */  { MAD_F(0x06dd849a) /* 0.429081537 */, 18 },
  /* 6143 */  { MAD_F(0x06dde646) /* 0.429174686 */, 18 },

  /* 6144 */  { MAD_F(0x06de47f4) /* 0.429267841 */, 18 },
  /* 6145 */  { MAD_F(0x06dea9a4) /* 0.429361001 */, 18 },
  /* 6146 */  { MAD_F(0x06df0b54) /* 0.429454165 */, 18 },
  /* 6147 */  { MAD_F(0x06df6d06) /* 0.429547335 */, 18 },
  /* 6148 */  { MAD_F(0x06dfceba) /* 0.429640510 */, 18 },
  /* 6149 */  { MAD_F(0x06e0306f) /* 0.429733690 */, 18 },
  /* 6150 */  { MAD_F(0x06e09225) /* 0.429826874 */, 18 },
  /* 6151 */  { MAD_F(0x06e0f3dc) /* 0.429920064 */, 18 },
  /* 6152 */  { MAD_F(0x06e15595) /* 0.430013259 */, 18 },
  /* 6153 */  { MAD_F(0x06e1b74f) /* 0.430106459 */, 18 },
  /* 6154 */  { MAD_F(0x06e2190b) /* 0.430199664 */, 18 },
  /* 6155 */  { MAD_F(0x06e27ac8) /* 0.430292875 */, 18 },
  /* 6156 */  { MAD_F(0x06e2dc86) /* 0.430386090 */, 18 },
  /* 6157 */  { MAD_F(0x06e33e46) /* 0.430479310 */, 18 },
  /* 6158 */  { MAD_F(0x06e3a007) /* 0.430572535 */, 18 },
  /* 6159 */  { MAD_F(0x06e401c9) /* 0.430665765 */, 18 },

  /* 6160 */  { MAD_F(0x06e4638d) /* 0.430759001 */, 18 },
  /* 6161 */  { MAD_F(0x06e4c552) /* 0.430852241 */, 18 },
  /* 6162 */  { MAD_F(0x06e52718) /* 0.430945487 */, 18 },
  /* 6163 */  { MAD_F(0x06e588e0) /* 0.431038737 */, 18 },
  /* 6164 */  { MAD_F(0x06e5eaa9) /* 0.431131993 */, 18 },
  /* 6165 */  { MAD_F(0x06e64c73) /* 0.431225253 */, 18 },
  /* 6166 */  { MAD_F(0x06e6ae3f) /* 0.431318519 */, 18 },
  /* 6167 */  { MAD_F(0x06e7100c) /* 0.431411790 */, 18 },
  /* 6168 */  { MAD_F(0x06e771db) /* 0.431505065 */, 18 },
  /* 6169 */  { MAD_F(0x06e7d3ab) /* 0.431598346 */, 18 },
  /* 6170 */  { MAD_F(0x06e8357c) /* 0.431691632 */, 18 },
  /* 6171 */  { MAD_F(0x06e8974e) /* 0.431784923 */, 18 },
  /* 6172 */  { MAD_F(0x06e8f922) /* 0.431878218 */, 18 },
  /* 6173 */  { MAD_F(0x06e95af8) /* 0.431971519 */, 18 },
  /* 6174 */  { MAD_F(0x06e9bcce) /* 0.432064825 */, 18 },
  /* 6175 */  { MAD_F(0x06ea1ea6) /* 0.432158136 */, 18 },

  /* 6176 */  { MAD_F(0x06ea807f) /* 0.432251452 */, 18 },
  /* 6177 */  { MAD_F(0x06eae25a) /* 0.432344773 */, 18 },
  /* 6178 */  { MAD_F(0x06eb4436) /* 0.432438099 */, 18 },
  /* 6179 */  { MAD_F(0x06eba614) /* 0.432531431 */, 18 },
  /* 6180 */  { MAD_F(0x06ec07f2) /* 0.432624767 */, 18 },
  /* 6181 */  { MAD_F(0x06ec69d2) /* 0.432718108 */, 18 },
  /* 6182 */  { MAD_F(0x06eccbb4) /* 0.432811454 */, 18 },
  /* 6183 */  { MAD_F(0x06ed2d97) /* 0.432904805 */, 18 },
  /* 6184 */  { MAD_F(0x06ed8f7b) /* 0.432998162 */, 18 },
  /* 6185 */  { MAD_F(0x06edf160) /* 0.433091523 */, 18 },
  /* 6186 */  { MAD_F(0x06ee5347) /* 0.433184889 */, 18 },
  /* 6187 */  { MAD_F(0x06eeb52f) /* 0.433278261 */, 18 },
  /* 6188 */  { MAD_F(0x06ef1719) /* 0.433371637 */, 18 },
  /* 6189 */  { MAD_F(0x06ef7904) /* 0.433465019 */, 18 },
  /* 6190 */  { MAD_F(0x06efdaf0) /* 0.433558405 */, 18 },
  /* 6191 */  { MAD_F(0x06f03cde) /* 0.433651797 */, 18 },

  /* 6192 */  { MAD_F(0x06f09ecc) /* 0.433745193 */, 18 },
  /* 6193 */  { MAD_F(0x06f100bd) /* 0.433838595 */, 18 },
  /* 6194 */  { MAD_F(0x06f162ae) /* 0.433932001 */, 18 },
  /* 6195 */  { MAD_F(0x06f1c4a1) /* 0.434025413 */, 18 },
  /* 6196 */  { MAD_F(0x06f22696) /* 0.434118830 */, 18 },
  /* 6197 */  { MAD_F(0x06f2888b) /* 0.434212251 */, 18 },
  /* 6198 */  { MAD_F(0x06f2ea82) /* 0.434305678 */, 18 },
  /* 6199 */  { MAD_F(0x06f34c7b) /* 0.434399110 */, 18 },
  /* 6200 */  { MAD_F(0x06f3ae75) /* 0.434492546 */, 18 },
  /* 6201 */  { MAD_F(0x06f41070) /* 0.434585988 */, 18 },
  /* 6202 */  { MAD_F(0x06f4726c) /* 0.434679435 */, 18 },
  /* 6203 */  { MAD_F(0x06f4d46a) /* 0.434772887 */, 18 },
  /* 6204 */  { MAD_F(0x06f53669) /* 0.434866344 */, 18 },
  /* 6205 */  { MAD_F(0x06f59869) /* 0.434959806 */, 18 },
  /* 6206 */  { MAD_F(0x06f5fa6b) /* 0.435053272 */, 18 },
  /* 6207 */  { MAD_F(0x06f65c6e) /* 0.435146744 */, 18 },

  /* 6208 */  { MAD_F(0x06f6be73) /* 0.435240221 */, 18 },
  /* 6209 */  { MAD_F(0x06f72079) /* 0.435333703 */, 18 },
  /* 6210 */  { MAD_F(0x06f78280) /* 0.435427190 */, 18 },
  /* 6211 */  { MAD_F(0x06f7e489) /* 0.435520682 */, 18 },
  /* 6212 */  { MAD_F(0x06f84693) /* 0.435614179 */, 18 },
  /* 6213 */  { MAD_F(0x06f8a89e) /* 0.435707681 */, 18 },
  /* 6214 */  { MAD_F(0x06f90aaa) /* 0.435801188 */, 18 },
  /* 6215 */  { MAD_F(0x06f96cb8) /* 0.435894700 */, 18 },
  /* 6216 */  { MAD_F(0x06f9cec8) /* 0.435988217 */, 18 },
  /* 6217 */  { MAD_F(0x06fa30d8) /* 0.436081739 */, 18 },
  /* 6218 */  { MAD_F(0x06fa92ea) /* 0.436175266 */, 18 },
  /* 6219 */  { MAD_F(0x06faf4fe) /* 0.436268799 */, 18 },
  /* 6220 */  { MAD_F(0x06fb5712) /* 0.436362336 */, 18 },
  /* 6221 */  { MAD_F(0x06fbb928) /* 0.436455878 */, 18 },
  /* 6222 */  { MAD_F(0x06fc1b40) /* 0.436549425 */, 18 },
  /* 6223 */  { MAD_F(0x06fc7d58) /* 0.436642977 */, 18 },

  /* 6224 */  { MAD_F(0x06fcdf72) /* 0.436736534 */, 18 },
  /* 6225 */  { MAD_F(0x06fd418e) /* 0.436830096 */, 18 },
  /* 6226 */  { MAD_F(0x06fda3ab) /* 0.436923664 */, 18 },
  /* 6227 */  { MAD_F(0x06fe05c9) /* 0.437017236 */, 18 },
  /* 6228 */  { MAD_F(0x06fe67e8) /* 0.437110813 */, 18 },
  /* 6229 */  { MAD_F(0x06feca09) /* 0.437204395 */, 18 },
  /* 6230 */  { MAD_F(0x06ff2c2b) /* 0.437297982 */, 18 },
  /* 6231 */  { MAD_F(0x06ff8e4f) /* 0.437391575 */, 18 },
  /* 6232 */  { MAD_F(0x06fff073) /* 0.437485172 */, 18 },
  /* 6233 */  { MAD_F(0x0700529a) /* 0.437578774 */, 18 },
  /* 6234 */  { MAD_F(0x0700b4c1) /* 0.437672381 */, 18 },
  /* 6235 */  { MAD_F(0x070116ea) /* 0.437765994 */, 18 },
  /* 6236 */  { MAD_F(0x07017914) /* 0.437859611 */, 18 },
  /* 6237 */  { MAD_F(0x0701db40) /* 0.437953233 */, 18 },
  /* 6238 */  { MAD_F(0x07023d6c) /* 0.438046860 */, 18 },
  /* 6239 */  { MAD_F(0x07029f9b) /* 0.438140493 */, 18 },

  /* 6240 */  { MAD_F(0x070301ca) /* 0.438234130 */, 18 },
  /* 6241 */  { MAD_F(0x070363fb) /* 0.438327772 */, 18 },
  /* 6242 */  { MAD_F(0x0703c62d) /* 0.438421419 */, 18 },
  /* 6243 */  { MAD_F(0x07042861) /* 0.438515072 */, 18 },
  /* 6244 */  { MAD_F(0x07048a96) /* 0.438608729 */, 18 },
  /* 6245 */  { MAD_F(0x0704eccc) /* 0.438702391 */, 18 },
  /* 6246 */  { MAD_F(0x07054f04) /* 0.438796059 */, 18 },
  /* 6247 */  { MAD_F(0x0705b13d) /* 0.438889731 */, 18 },
  /* 6248 */  { MAD_F(0x07061377) /* 0.438983408 */, 18 },
  /* 6249 */  { MAD_F(0x070675b3) /* 0.439077090 */, 18 },
  /* 6250 */  { MAD_F(0x0706d7f0) /* 0.439170778 */, 18 },
  /* 6251 */  { MAD_F(0x07073a2e) /* 0.439264470 */, 18 },
  /* 6252 */  { MAD_F(0x07079c6e) /* 0.439358167 */, 18 },
  /* 6253 */  { MAD_F(0x0707feaf) /* 0.439451869 */, 18 },
  /* 6254 */  { MAD_F(0x070860f1) /* 0.439545577 */, 18 },
  /* 6255 */  { MAD_F(0x0708c335) /* 0.439639289 */, 18 },

  /* 6256 */  { MAD_F(0x0709257a) /* 0.439733006 */, 18 },
  /* 6257 */  { MAD_F(0x070987c0) /* 0.439826728 */, 18 },
  /* 6258 */  { MAD_F(0x0709ea08) /* 0.439920456 */, 18 },
  /* 6259 */  { MAD_F(0x070a4c51) /* 0.440014188 */, 18 },
  /* 6260 */  { MAD_F(0x070aae9b) /* 0.440107925 */, 18 },
  /* 6261 */  { MAD_F(0x070b10e7) /* 0.440201667 */, 18 },
  /* 6262 */  { MAD_F(0x070b7334) /* 0.440295414 */, 18 },
  /* 6263 */  { MAD_F(0x070bd583) /* 0.440389167 */, 18 },
  /* 6264 */  { MAD_F(0x070c37d2) /* 0.440482924 */, 18 },
  /* 6265 */  { MAD_F(0x070c9a23) /* 0.440576686 */, 18 },
  /* 6266 */  { MAD_F(0x070cfc76) /* 0.440670453 */, 18 },
  /* 6267 */  { MAD_F(0x070d5eca) /* 0.440764225 */, 18 },
  /* 6268 */  { MAD_F(0x070dc11f) /* 0.440858002 */, 18 },
  /* 6269 */  { MAD_F(0x070e2375) /* 0.440951784 */, 18 },
  /* 6270 */  { MAD_F(0x070e85cd) /* 0.441045572 */, 18 },
  /* 6271 */  { MAD_F(0x070ee826) /* 0.441139364 */, 18 },

  /* 6272 */  { MAD_F(0x070f4a80) /* 0.441233161 */, 18 },
  /* 6273 */  { MAD_F(0x070facdc) /* 0.441326963 */, 18 },
  /* 6274 */  { MAD_F(0x07100f39) /* 0.441420770 */, 18 },
  /* 6275 */  { MAD_F(0x07107198) /* 0.441514582 */, 18 },
  /* 6276 */  { MAD_F(0x0710d3f8) /* 0.441608399 */, 18 },
  /* 6277 */  { MAD_F(0x07113659) /* 0.441702221 */, 18 },
  /* 6278 */  { MAD_F(0x071198bb) /* 0.441796048 */, 18 },
  /* 6279 */  { MAD_F(0x0711fb1f) /* 0.441889880 */, 18 },
  /* 6280 */  { MAD_F(0x07125d84) /* 0.441983717 */, 18 },
  /* 6281 */  { MAD_F(0x0712bfeb) /* 0.442077559 */, 18 },
  /* 6282 */  { MAD_F(0x07132253) /* 0.442171406 */, 18 },
  /* 6283 */  { MAD_F(0x071384bc) /* 0.442265257 */, 18 },
  /* 6284 */  { MAD_F(0x0713e726) /* 0.442359114 */, 18 },
  /* 6285 */  { MAD_F(0x07144992) /* 0.442452976 */, 18 },
  /* 6286 */  { MAD_F(0x0714abff) /* 0.442546843 */, 18 },
  /* 6287 */  { MAD_F(0x07150e6e) /* 0.442640715 */, 18 },

  /* 6288 */  { MAD_F(0x071570de) /* 0.442734592 */, 18 },
  /* 6289 */  { MAD_F(0x0715d34f) /* 0.442828473 */, 18 },
  /* 6290 */  { MAD_F(0x071635c1) /* 0.442922360 */, 18 },
  /* 6291 */  { MAD_F(0x07169835) /* 0.443016252 */, 18 },
  /* 6292 */  { MAD_F(0x0716faaa) /* 0.443110148 */, 18 },
  /* 6293 */  { MAD_F(0x07175d21) /* 0.443204050 */, 18 },
  /* 6294 */  { MAD_F(0x0717bf99) /* 0.443297957 */, 18 },
  /* 6295 */  { MAD_F(0x07182212) /* 0.443391868 */, 18 },
  /* 6296 */  { MAD_F(0x0718848d) /* 0.443485785 */, 18 },
  /* 6297 */  { MAD_F(0x0718e709) /* 0.443579706 */, 18 },
  /* 6298 */  { MAD_F(0x07194986) /* 0.443673633 */, 18 },
  /* 6299 */  { MAD_F(0x0719ac04) /* 0.443767564 */, 18 },
  /* 6300 */  { MAD_F(0x071a0e84) /* 0.443861501 */, 18 },
  /* 6301 */  { MAD_F(0x071a7105) /* 0.443955442 */, 18 },
  /* 6302 */  { MAD_F(0x071ad388) /* 0.444049389 */, 18 },
  /* 6303 */  { MAD_F(0x071b360c) /* 0.444143340 */, 18 },

  /* 6304 */  { MAD_F(0x071b9891) /* 0.444237296 */, 18 },
  /* 6305 */  { MAD_F(0x071bfb18) /* 0.444331258 */, 18 },
  /* 6306 */  { MAD_F(0x071c5d9f) /* 0.444425224 */, 18 },
  /* 6307 */  { MAD_F(0x071cc029) /* 0.444519195 */, 18 },
  /* 6308 */  { MAD_F(0x071d22b3) /* 0.444613171 */, 18 },
  /* 6309 */  { MAD_F(0x071d853f) /* 0.444707153 */, 18 },
  /* 6310 */  { MAD_F(0x071de7cc) /* 0.444801139 */, 18 },
  /* 6311 */  { MAD_F(0x071e4a5b) /* 0.444895130 */, 18 },
  /* 6312 */  { MAD_F(0x071eaceb) /* 0.444989126 */, 18 },
  /* 6313 */  { MAD_F(0x071f0f7c) /* 0.445083127 */, 18 },
  /* 6314 */  { MAD_F(0x071f720e) /* 0.445177133 */, 18 },
  /* 6315 */  { MAD_F(0x071fd4a2) /* 0.445271144 */, 18 },
  /* 6316 */  { MAD_F(0x07203737) /* 0.445365160 */, 18 },
  /* 6317 */  { MAD_F(0x072099ce) /* 0.445459181 */, 18 },
  /* 6318 */  { MAD_F(0x0720fc66) /* 0.445553206 */, 18 },
  /* 6319 */  { MAD_F(0x07215eff) /* 0.445647237 */, 18 },

  /* 6320 */  { MAD_F(0x0721c19a) /* 0.445741273 */, 18 },
  /* 6321 */  { MAD_F(0x07222436) /* 0.445835314 */, 18 },
  /* 6322 */  { MAD_F(0x072286d3) /* 0.445929359 */, 18 },
  /* 6323 */  { MAD_F(0x0722e971) /* 0.446023410 */, 18 },
  /* 6324 */  { MAD_F(0x07234c11) /* 0.446117466 */, 18 },
  /* 6325 */  { MAD_F(0x0723aeb2) /* 0.446211526 */, 18 },
  /* 6326 */  { MAD_F(0x07241155) /* 0.446305592 */, 18 },
  /* 6327 */  { MAD_F(0x072473f9) /* 0.446399662 */, 18 },
  /* 6328 */  { MAD_F(0x0724d69e) /* 0.446493738 */, 18 },
  /* 6329 */  { MAD_F(0x07253944) /* 0.446587818 */, 18 },
  /* 6330 */  { MAD_F(0x07259bec) /* 0.446681903 */, 18 },
  /* 6331 */  { MAD_F(0x0725fe95) /* 0.446775994 */, 18 },
  /* 6332 */  { MAD_F(0x07266140) /* 0.446870089 */, 18 },
  /* 6333 */  { MAD_F(0x0726c3ec) /* 0.446964189 */, 18 },
  /* 6334 */  { MAD_F(0x07272699) /* 0.447058294 */, 18 },
  /* 6335 */  { MAD_F(0x07278947) /* 0.447152404 */, 18 },

  /* 6336 */  { MAD_F(0x0727ebf7) /* 0.447246519 */, 18 },
  /* 6337 */  { MAD_F(0x07284ea8) /* 0.447340639 */, 18 },
  /* 6338 */  { MAD_F(0x0728b15b) /* 0.447434764 */, 18 },
  /* 6339 */  { MAD_F(0x0729140f) /* 0.447528894 */, 18 },
  /* 6340 */  { MAD_F(0x072976c4) /* 0.447623029 */, 18 },
  /* 6341 */  { MAD_F(0x0729d97a) /* 0.447717169 */, 18 },
  /* 6342 */  { MAD_F(0x072a3c32) /* 0.447811314 */, 18 },
  /* 6343 */  { MAD_F(0x072a9eeb) /* 0.447905463 */, 18 },
  /* 6344 */  { MAD_F(0x072b01a6) /* 0.447999618 */, 18 },
  /* 6345 */  { MAD_F(0x072b6461) /* 0.448093778 */, 18 },
  /* 6346 */  { MAD_F(0x072bc71e) /* 0.448187942 */, 18 },
  /* 6347 */  { MAD_F(0x072c29dd) /* 0.448282112 */, 18 },
  /* 6348 */  { MAD_F(0x072c8c9d) /* 0.448376286 */, 18 },
  /* 6349 */  { MAD_F(0x072cef5e) /* 0.448470466 */, 18 },
  /* 6350 */  { MAD_F(0x072d5220) /* 0.448564650 */, 18 },
  /* 6351 */  { MAD_F(0x072db4e4) /* 0.448658839 */, 18 },

  /* 6352 */  { MAD_F(0x072e17a9) /* 0.448753033 */, 18 },
  /* 6353 */  { MAD_F(0x072e7a6f) /* 0.448847233 */, 18 },
  /* 6354 */  { MAD_F(0x072edd37) /* 0.448941437 */, 18 },
  /* 6355 */  { MAD_F(0x072f4000) /* 0.449035646 */, 18 },
  /* 6356 */  { MAD_F(0x072fa2ca) /* 0.449129860 */, 18 },
  /* 6357 */  { MAD_F(0x07300596) /* 0.449224079 */, 18 },
  /* 6358 */  { MAD_F(0x07306863) /* 0.449318303 */, 18 },
  /* 6359 */  { MAD_F(0x0730cb32) /* 0.449412531 */, 18 },
  /* 6360 */  { MAD_F(0x07312e01) /* 0.449506765 */, 18 },
  /* 6361 */  { MAD_F(0x073190d2) /* 0.449601004 */, 18 },
  /* 6362 */  { MAD_F(0x0731f3a5) /* 0.449695247 */, 18 },
  /* 6363 */  { MAD_F(0x07325678) /* 0.449789496 */, 18 },
  /* 6364 */  { MAD_F(0x0732b94d) /* 0.449883749 */, 18 },
  /* 6365 */  { MAD_F(0x07331c23) /* 0.449978008 */, 18 },
  /* 6366 */  { MAD_F(0x07337efb) /* 0.450072271 */, 18 },
  /* 6367 */  { MAD_F(0x0733e1d4) /* 0.450166540 */, 18 },

  /* 6368 */  { MAD_F(0x073444ae) /* 0.450260813 */, 18 },
  /* 6369 */  { MAD_F(0x0734a78a) /* 0.450355091 */, 18 },
  /* 6370 */  { MAD_F(0x07350a67) /* 0.450449374 */, 18 },
  /* 6371 */  { MAD_F(0x07356d45) /* 0.450543662 */, 18 },
  /* 6372 */  { MAD_F(0x0735d025) /* 0.450637955 */, 18 },
  /* 6373 */  { MAD_F(0x07363306) /* 0.450732253 */, 18 },
  /* 6374 */  { MAD_F(0x073695e8) /* 0.450826556 */, 18 },
  /* 6375 */  { MAD_F(0x0736f8cb) /* 0.450920864 */, 18 },
  /* 6376 */  { MAD_F(0x07375bb0) /* 0.451015176 */, 18 },
  /* 6377 */  { MAD_F(0x0737be96) /* 0.451109494 */, 18 },
  /* 6378 */  { MAD_F(0x0738217e) /* 0.451203817 */, 18 },
  /* 6379 */  { MAD_F(0x07388467) /* 0.451298144 */, 18 },
  /* 6380 */  { MAD_F(0x0738e751) /* 0.451392477 */, 18 },
  /* 6381 */  { MAD_F(0x07394a3d) /* 0.451486814 */, 18 },
  /* 6382 */  { MAD_F(0x0739ad29) /* 0.451581156 */, 18 },
  /* 6383 */  { MAD_F(0x073a1017) /* 0.451675503 */, 18 },

  /* 6384 */  { MAD_F(0x073a7307) /* 0.451769856 */, 18 },
  /* 6385 */  { MAD_F(0x073ad5f8) /* 0.451864213 */, 18 },
  /* 6386 */  { MAD_F(0x073b38ea) /* 0.451958575 */, 18 },
  /* 6387 */  { MAD_F(0x073b9bdd) /* 0.452052942 */, 18 },
  /* 6388 */  { MAD_F(0x073bfed2) /* 0.452147313 */, 18 },
  /* 6389 */  { MAD_F(0x073c61c8) /* 0.452241690 */, 18 },
  /* 6390 */  { MAD_F(0x073cc4bf) /* 0.452336072 */, 18 },
  /* 6391 */  { MAD_F(0x073d27b8) /* 0.452430458 */, 18 },
  /* 6392 */  { MAD_F(0x073d8ab2) /* 0.452524850 */, 18 },
  /* 6393 */  { MAD_F(0x073dedae) /* 0.452619246 */, 18 },
  /* 6394 */  { MAD_F(0x073e50aa) /* 0.452713648 */, 18 },
  /* 6395 */  { MAD_F(0x073eb3a8) /* 0.452808054 */, 18 },
  /* 6396 */  { MAD_F(0x073f16a8) /* 0.452902465 */, 18 },
  /* 6397 */  { MAD_F(0x073f79a8) /* 0.452996882 */, 18 },
  /* 6398 */  { MAD_F(0x073fdcaa) /* 0.453091303 */, 18 },
  /* 6399 */  { MAD_F(0x07403fad) /* 0.453185729 */, 18 },

  /* 6400 */  { MAD_F(0x0740a2b2) /* 0.453280160 */, 18 },
  /* 6401 */  { MAD_F(0x074105b8) /* 0.453374595 */, 18 },
  /* 6402 */  { MAD_F(0x074168bf) /* 0.453469036 */, 18 },
  /* 6403 */  { MAD_F(0x0741cbc8) /* 0.453563482 */, 18 },
  /* 6404 */  { MAD_F(0x07422ed2) /* 0.453657932 */, 18 },
  /* 6405 */  { MAD_F(0x074291dd) /* 0.453752388 */, 18 },
  /* 6406 */  { MAD_F(0x0742f4e9) /* 0.453846848 */, 18 },
  /* 6407 */  { MAD_F(0x074357f7) /* 0.453941314 */, 18 },
  /* 6408 */  { MAD_F(0x0743bb06) /* 0.454035784 */, 18 },
  /* 6409 */  { MAD_F(0x07441e17) /* 0.454130259 */, 18 },
  /* 6410 */  { MAD_F(0x07448129) /* 0.454224739 */, 18 },
  /* 6411 */  { MAD_F(0x0744e43c) /* 0.454319224 */, 18 },
  /* 6412 */  { MAD_F(0x07454750) /* 0.454413714 */, 18 },
  /* 6413 */  { MAD_F(0x0745aa66) /* 0.454508209 */, 18 },
  /* 6414 */  { MAD_F(0x07460d7d) /* 0.454602708 */, 18 },
  /* 6415 */  { MAD_F(0x07467095) /* 0.454697213 */, 18 },

  /* 6416 */  { MAD_F(0x0746d3af) /* 0.454791723 */, 18 },
  /* 6417 */  { MAD_F(0x074736ca) /* 0.454886237 */, 18 },
  /* 6418 */  { MAD_F(0x074799e7) /* 0.454980756 */, 18 },
  /* 6419 */  { MAD_F(0x0747fd04) /* 0.455075281 */, 18 },
  /* 6420 */  { MAD_F(0x07486023) /* 0.455169810 */, 18 },
  /* 6421 */  { MAD_F(0x0748c344) /* 0.455264344 */, 18 },
  /* 6422 */  { MAD_F(0x07492665) /* 0.455358883 */, 18 },
  /* 6423 */  { MAD_F(0x07498988) /* 0.455453427 */, 18 },
  /* 6424 */  { MAD_F(0x0749ecac) /* 0.455547976 */, 18 },
  /* 6425 */  { MAD_F(0x074a4fd2) /* 0.455642529 */, 18 },
  /* 6426 */  { MAD_F(0x074ab2f9) /* 0.455737088 */, 18 },
  /* 6427 */  { MAD_F(0x074b1621) /* 0.455831652 */, 18 },
  /* 6428 */  { MAD_F(0x074b794b) /* 0.455926220 */, 18 },
  /* 6429 */  { MAD_F(0x074bdc75) /* 0.456020793 */, 18 },
  /* 6430 */  { MAD_F(0x074c3fa1) /* 0.456115372 */, 18 },
  /* 6431 */  { MAD_F(0x074ca2cf) /* 0.456209955 */, 18 },

  /* 6432 */  { MAD_F(0x074d05fe) /* 0.456304543 */, 18 },
  /* 6433 */  { MAD_F(0x074d692e) /* 0.456399136 */, 18 },
  /* 6434 */  { MAD_F(0x074dcc5f) /* 0.456493733 */, 18 },
  /* 6435 */  { MAD_F(0x074e2f92) /* 0.456588336 */, 18 },
  /* 6436 */  { MAD_F(0x074e92c6) /* 0.456682944 */, 18 },
  /* 6437 */  { MAD_F(0x074ef5fb) /* 0.456777556 */, 18 },
  /* 6438 */  { MAD_F(0x074f5932) /* 0.456872174 */, 18 },
  /* 6439 */  { MAD_F(0x074fbc6a) /* 0.456966796 */, 18 },
  /* 6440 */  { MAD_F(0x07501fa3) /* 0.457061423 */, 18 },
  /* 6441 */  { MAD_F(0x075082de) /* 0.457156056 */, 18 },
  /* 6442 */  { MAD_F(0x0750e61a) /* 0.457250693 */, 18 },
  /* 6443 */  { MAD_F(0x07514957) /* 0.457345335 */, 18 },
  /* 6444 */  { MAD_F(0x0751ac96) /* 0.457439981 */, 18 },
  /* 6445 */  { MAD_F(0x07520fd6) /* 0.457534633 */, 18 },
  /* 6446 */  { MAD_F(0x07527317) /* 0.457629290 */, 18 },
  /* 6447 */  { MAD_F(0x0752d659) /* 0.457723951 */, 18 },

  /* 6448 */  { MAD_F(0x0753399d) /* 0.457818618 */, 18 },
  /* 6449 */  { MAD_F(0x07539ce2) /* 0.457913289 */, 18 },
  /* 6450 */  { MAD_F(0x07540029) /* 0.458007965 */, 18 },
  /* 6451 */  { MAD_F(0x07546371) /* 0.458102646 */, 18 },
  /* 6452 */  { MAD_F(0x0754c6ba) /* 0.458197332 */, 18 },
  /* 6453 */  { MAD_F(0x07552a04) /* 0.458292023 */, 18 },
  /* 6454 */  { MAD_F(0x07558d50) /* 0.458386719 */, 18 },
  /* 6455 */  { MAD_F(0x0755f09d) /* 0.458481420 */, 18 },
  /* 6456 */  { MAD_F(0x075653eb) /* 0.458576125 */, 18 },
  /* 6457 */  { MAD_F(0x0756b73b) /* 0.458670836 */, 18 },
  /* 6458 */  { MAD_F(0x07571a8c) /* 0.458765551 */, 18 },
  /* 6459 */  { MAD_F(0x07577dde) /* 0.458860271 */, 18 },
  /* 6460 */  { MAD_F(0x0757e131) /* 0.458954996 */, 18 },
  /* 6461 */  { MAD_F(0x07584486) /* 0.459049726 */, 18 },
  /* 6462 */  { MAD_F(0x0758a7dd) /* 0.459144461 */, 18 },
  /* 6463 */  { MAD_F(0x07590b34) /* 0.459239201 */, 18 },

  /* 6464 */  { MAD_F(0x07596e8d) /* 0.459333946 */, 18 },
  /* 6465 */  { MAD_F(0x0759d1e7) /* 0.459428695 */, 18 },
  /* 6466 */  { MAD_F(0x075a3542) /* 0.459523450 */, 18 },
  /* 6467 */  { MAD_F(0x075a989f) /* 0.459618209 */, 18 },
  /* 6468 */  { MAD_F(0x075afbfd) /* 0.459712973 */, 18 },
  /* 6469 */  { MAD_F(0x075b5f5d) /* 0.459807742 */, 18 },
  /* 6470 */  { MAD_F(0x075bc2bd) /* 0.459902516 */, 18 },
  /* 6471 */  { MAD_F(0x075c261f) /* 0.459997295 */, 18 },
  /* 6472 */  { MAD_F(0x075c8983) /* 0.460092079 */, 18 },
  /* 6473 */  { MAD_F(0x075cece7) /* 0.460186867 */, 18 },
  /* 6474 */  { MAD_F(0x075d504d) /* 0.460281661 */, 18 },
  /* 6475 */  { MAD_F(0x075db3b5) /* 0.460376459 */, 18 },
  /* 6476 */  { MAD_F(0x075e171d) /* 0.460471262 */, 18 },
  /* 6477 */  { MAD_F(0x075e7a87) /* 0.460566071 */, 18 },
  /* 6478 */  { MAD_F(0x075eddf2) /* 0.460660884 */, 18 },
  /* 6479 */  { MAD_F(0x075f415f) /* 0.460755701 */, 18 },

  /* 6480 */  { MAD_F(0x075fa4cc) /* 0.460850524 */, 18 },
  /* 6481 */  { MAD_F(0x0760083b) /* 0.460945352 */, 18 },
  /* 6482 */  { MAD_F(0x07606bac) /* 0.461040184 */, 18 },
  /* 6483 */  { MAD_F(0x0760cf1e) /* 0.461135022 */, 18 },
  /* 6484 */  { MAD_F(0x07613291) /* 0.461229864 */, 18 },
  /* 6485 */  { MAD_F(0x07619605) /* 0.461324711 */, 18 },
  /* 6486 */  { MAD_F(0x0761f97b) /* 0.461419563 */, 18 },
  /* 6487 */  { MAD_F(0x07625cf2) /* 0.461514420 */, 18 },
  /* 6488 */  { MAD_F(0x0762c06a) /* 0.461609282 */, 18 },
  /* 6489 */  { MAD_F(0x076323e3) /* 0.461704149 */, 18 },
  /* 6490 */  { MAD_F(0x0763875e) /* 0.461799020 */, 18 },
  /* 6491 */  { MAD_F(0x0763eadb) /* 0.461893897 */, 18 },
  /* 6492 */  { MAD_F(0x07644e58) /* 0.461988778 */, 18 },
  /* 6493 */  { MAD_F(0x0764b1d7) /* 0.462083664 */, 18 },
  /* 6494 */  { MAD_F(0x07651557) /* 0.462178555 */, 18 },
  /* 6495 */  { MAD_F(0x076578d8) /* 0.462273451 */, 18 },

  /* 6496 */  { MAD_F(0x0765dc5b) /* 0.462368352 */, 18 },
  /* 6497 */  { MAD_F(0x07663fdf) /* 0.462463257 */, 18 },
  /* 6498 */  { MAD_F(0x0766a364) /* 0.462558168 */, 18 },
  /* 6499 */  { MAD_F(0x076706eb) /* 0.462653083 */, 18 },
  /* 6500 */  { MAD_F(0x07676a73) /* 0.462748003 */, 18 },
  /* 6501 */  { MAD_F(0x0767cdfc) /* 0.462842928 */, 18 },
  /* 6502 */  { MAD_F(0x07683187) /* 0.462937858 */, 18 },
  /* 6503 */  { MAD_F(0x07689513) /* 0.463032793 */, 18 },
  /* 6504 */  { MAD_F(0x0768f8a0) /* 0.463127733 */, 18 },
  /* 6505 */  { MAD_F(0x07695c2e) /* 0.463222678 */, 18 },
  /* 6506 */  { MAD_F(0x0769bfbe) /* 0.463317627 */, 18 },
  /* 6507 */  { MAD_F(0x076a234f) /* 0.463412581 */, 18 },
  /* 6508 */  { MAD_F(0x076a86e2) /* 0.463507540 */, 18 },
  /* 6509 */  { MAD_F(0x076aea75) /* 0.463602504 */, 18 },
  /* 6510 */  { MAD_F(0x076b4e0a) /* 0.463697473 */, 18 },
  /* 6511 */  { MAD_F(0x076bb1a1) /* 0.463792447 */, 18 },

  /* 6512 */  { MAD_F(0x076c1538) /* 0.463887426 */, 18 },
  /* 6513 */  { MAD_F(0x076c78d1) /* 0.463982409 */, 18 },
  /* 6514 */  { MAD_F(0x076cdc6c) /* 0.464077398 */, 18 },
  /* 6515 */  { MAD_F(0x076d4007) /* 0.464172391 */, 18 },
  /* 6516 */  { MAD_F(0x076da3a4) /* 0.464267389 */, 18 },
  /* 6517 */  { MAD_F(0x076e0742) /* 0.464362392 */, 18 },
  /* 6518 */  { MAD_F(0x076e6ae2) /* 0.464457399 */, 18 },
  /* 6519 */  { MAD_F(0x076ece82) /* 0.464552412 */, 18 },
  /* 6520 */  { MAD_F(0x076f3224) /* 0.464647430 */, 18 },
  /* 6521 */  { MAD_F(0x076f95c8) /* 0.464742452 */, 18 },
  /* 6522 */  { MAD_F(0x076ff96c) /* 0.464837479 */, 18 },
  /* 6523 */  { MAD_F(0x07705d12) /* 0.464932511 */, 18 },
  /* 6524 */  { MAD_F(0x0770c0ba) /* 0.465027548 */, 18 },
  /* 6525 */  { MAD_F(0x07712462) /* 0.465122590 */, 18 },
  /* 6526 */  { MAD_F(0x0771880c) /* 0.465217637 */, 18 },
  /* 6527 */  { MAD_F(0x0771ebb7) /* 0.465312688 */, 18 },

  /* 6528 */  { MAD_F(0x07724f64) /* 0.465407744 */, 18 },
  /* 6529 */  { MAD_F(0x0772b312) /* 0.465502806 */, 18 },
  /* 6530 */  { MAD_F(0x077316c1) /* 0.465597872 */, 18 },
  /* 6531 */  { MAD_F(0x07737a71) /* 0.465692943 */, 18 },
  /* 6532 */  { MAD_F(0x0773de23) /* 0.465788018 */, 18 },
  /* 6533 */  { MAD_F(0x077441d6) /* 0.465883099 */, 18 },
  /* 6534 */  { MAD_F(0x0774a58a) /* 0.465978184 */, 18 },
  /* 6535 */  { MAD_F(0x07750940) /* 0.466073275 */, 18 },
  /* 6536 */  { MAD_F(0x07756cf7) /* 0.466168370 */, 18 },
  /* 6537 */  { MAD_F(0x0775d0af) /* 0.466263470 */, 18 },
  /* 6538 */  { MAD_F(0x07763468) /* 0.466358575 */, 18 },
  /* 6539 */  { MAD_F(0x07769823) /* 0.466453684 */, 18 },
  /* 6540 */  { MAD_F(0x0776fbdf) /* 0.466548799 */, 18 },
  /* 6541 */  { MAD_F(0x07775f9d) /* 0.466643918 */, 18 },
  /* 6542 */  { MAD_F(0x0777c35c) /* 0.466739043 */, 18 },
  /* 6543 */  { MAD_F(0x0778271c) /* 0.466834172 */, 18 },

  /* 6544 */  { MAD_F(0x07788add) /* 0.466929306 */, 18 },
  /* 6545 */  { MAD_F(0x0778ee9f) /* 0.467024445 */, 18 },
  /* 6546 */  { MAD_F(0x07795263) /* 0.467119588 */, 18 },
  /* 6547 */  { MAD_F(0x0779b629) /* 0.467214737 */, 18 },
  /* 6548 */  { MAD_F(0x077a19ef) /* 0.467309890 */, 18 },
  /* 6549 */  { MAD_F(0x077a7db7) /* 0.467405048 */, 18 },
  /* 6550 */  { MAD_F(0x077ae180) /* 0.467500211 */, 18 },
  /* 6551 */  { MAD_F(0x077b454b) /* 0.467595379 */, 18 },
  /* 6552 */  { MAD_F(0x077ba916) /* 0.467690552 */, 18 },
  /* 6553 */  { MAD_F(0x077c0ce3) /* 0.467785729 */, 18 },
  /* 6554 */  { MAD_F(0x077c70b2) /* 0.467880912 */, 18 },
  /* 6555 */  { MAD_F(0x077cd481) /* 0.467976099 */, 18 },
  /* 6556 */  { MAD_F(0x077d3852) /* 0.468071291 */, 18 },
  /* 6557 */  { MAD_F(0x077d9c24) /* 0.468166488 */, 18 },
  /* 6558 */  { MAD_F(0x077dfff8) /* 0.468261690 */, 18 },
  /* 6559 */  { MAD_F(0x077e63cd) /* 0.468356896 */, 18 },

  /* 6560 */  { MAD_F(0x077ec7a3) /* 0.468452108 */, 18 },
  /* 6561 */  { MAD_F(0x077f2b7a) /* 0.468547324 */, 18 },
  /* 6562 */  { MAD_F(0x077f8f53) /* 0.468642545 */, 18 },
  /* 6563 */  { MAD_F(0x077ff32d) /* 0.468737771 */, 18 },
  /* 6564 */  { MAD_F(0x07805708) /* 0.468833002 */, 18 },
  /* 6565 */  { MAD_F(0x0780bae5) /* 0.468928237 */, 18 },
  /* 6566 */  { MAD_F(0x07811ec3) /* 0.469023478 */, 18 },
  /* 6567 */  { MAD_F(0x078182a2) /* 0.469118723 */, 18 },
  /* 6568 */  { MAD_F(0x0781e683) /* 0.469213973 */, 18 },
  /* 6569 */  { MAD_F(0x07824a64) /* 0.469309228 */, 18 },
  /* 6570 */  { MAD_F(0x0782ae47) /* 0.469404488 */, 18 },
  /* 6571 */  { MAD_F(0x0783122c) /* 0.469499752 */, 18 },
  /* 6572 */  { MAD_F(0x07837612) /* 0.469595022 */, 18 },
  /* 6573 */  { MAD_F(0x0783d9f9) /* 0.469690296 */, 18 },
  /* 6574 */  { MAD_F(0x07843de1) /* 0.469785575 */, 18 },
  /* 6575 */  { MAD_F(0x0784a1ca) /* 0.469880859 */, 18 },

  /* 6576 */  { MAD_F(0x078505b5) /* 0.469976148 */, 18 },
  /* 6577 */  { MAD_F(0x078569a2) /* 0.470071442 */, 18 },
  /* 6578 */  { MAD_F(0x0785cd8f) /* 0.470166740 */, 18 },
  /* 6579 */  { MAD_F(0x0786317e) /* 0.470262043 */, 18 },
  /* 6580 */  { MAD_F(0x0786956e) /* 0.470357351 */, 18 },
  /* 6581 */  { MAD_F(0x0786f95f) /* 0.470452664 */, 18 },
  /* 6582 */  { MAD_F(0x07875d52) /* 0.470547982 */, 18 },
  /* 6583 */  { MAD_F(0x0787c146) /* 0.470643305 */, 18 },
  /* 6584 */  { MAD_F(0x0788253b) /* 0.470738632 */, 18 },
  /* 6585 */  { MAD_F(0x07888932) /* 0.470833964 */, 18 },
  /* 6586 */  { MAD_F(0x0788ed2a) /* 0.470929301 */, 18 },
  /* 6587 */  { MAD_F(0x07895123) /* 0.471024643 */, 18 },
  /* 6588 */  { MAD_F(0x0789b51d) /* 0.471119990 */, 18 },
  /* 6589 */  { MAD_F(0x078a1919) /* 0.471215341 */, 18 },
  /* 6590 */  { MAD_F(0x078a7d16) /* 0.471310698 */, 18 },
  /* 6591 */  { MAD_F(0x078ae114) /* 0.471406059 */, 18 },

  /* 6592 */  { MAD_F(0x078b4514) /* 0.471501425 */, 18 },
  /* 6593 */  { MAD_F(0x078ba915) /* 0.471596796 */, 18 },
  /* 6594 */  { MAD_F(0x078c0d17) /* 0.471692171 */, 18 },
  /* 6595 */  { MAD_F(0x078c711a) /* 0.471787552 */, 18 },
  /* 6596 */  { MAD_F(0x078cd51f) /* 0.471882937 */, 18 },
  /* 6597 */  { MAD_F(0x078d3925) /* 0.471978327 */, 18 },
  /* 6598 */  { MAD_F(0x078d9d2d) /* 0.472073722 */, 18 },
  /* 6599 */  { MAD_F(0x078e0135) /* 0.472169122 */, 18 },
  /* 6600 */  { MAD_F(0x078e653f) /* 0.472264527 */, 18 },
  /* 6601 */  { MAD_F(0x078ec94b) /* 0.472359936 */, 18 },
  /* 6602 */  { MAD_F(0x078f2d57) /* 0.472455350 */, 18 },
  /* 6603 */  { MAD_F(0x078f9165) /* 0.472550769 */, 18 },
  /* 6604 */  { MAD_F(0x078ff574) /* 0.472646193 */, 18 },
  /* 6605 */  { MAD_F(0x07905985) /* 0.472741622 */, 18 },
  /* 6606 */  { MAD_F(0x0790bd96) /* 0.472837055 */, 18 },
  /* 6607 */  { MAD_F(0x079121a9) /* 0.472932493 */, 18 },

  /* 6608 */  { MAD_F(0x079185be) /* 0.473027937 */, 18 },
  /* 6609 */  { MAD_F(0x0791e9d3) /* 0.473123384 */, 18 },
  /* 6610 */  { MAD_F(0x07924dea) /* 0.473218837 */, 18 },
  /* 6611 */  { MAD_F(0x0792b202) /* 0.473314295 */, 18 },
  /* 6612 */  { MAD_F(0x0793161c) /* 0.473409757 */, 18 },
  /* 6613 */  { MAD_F(0x07937a37) /* 0.473505224 */, 18 },
  /* 6614 */  { MAD_F(0x0793de53) /* 0.473600696 */, 18 },
  /* 6615 */  { MAD_F(0x07944270) /* 0.473696173 */, 18 },
  /* 6616 */  { MAD_F(0x0794a68f) /* 0.473791655 */, 18 },
  /* 6617 */  { MAD_F(0x07950aaf) /* 0.473887141 */, 18 },
  /* 6618 */  { MAD_F(0x07956ed0) /* 0.473982632 */, 18 },
  /* 6619 */  { MAD_F(0x0795d2f2) /* 0.474078128 */, 18 },
  /* 6620 */  { MAD_F(0x07963716) /* 0.474173629 */, 18 },
  /* 6621 */  { MAD_F(0x07969b3b) /* 0.474269135 */, 18 },
  /* 6622 */  { MAD_F(0x0796ff62) /* 0.474364645 */, 18 },
  /* 6623 */  { MAD_F(0x07976389) /* 0.474460161 */, 18 },

  /* 6624 */  { MAD_F(0x0797c7b2) /* 0.474555681 */, 18 },
  /* 6625 */  { MAD_F(0x07982bdd) /* 0.474651205 */, 18 },
  /* 6626 */  { MAD_F(0x07989008) /* 0.474746735 */, 18 },
  /* 6627 */  { MAD_F(0x0798f435) /* 0.474842270 */, 18 },
  /* 6628 */  { MAD_F(0x07995863) /* 0.474937809 */, 18 },
  /* 6629 */  { MAD_F(0x0799bc92) /* 0.475033353 */, 18 },
  /* 6630 */  { MAD_F(0x079a20c3) /* 0.475128902 */, 18 },
  /* 6631 */  { MAD_F(0x079a84f5) /* 0.475224456 */, 18 },
  /* 6632 */  { MAD_F(0x079ae929) /* 0.475320014 */, 18 },
  /* 6633 */  { MAD_F(0x079b4d5d) /* 0.475415578 */, 18 },
  /* 6634 */  { MAD_F(0x079bb193) /* 0.475511146 */, 18 },
  /* 6635 */  { MAD_F(0x079c15ca) /* 0.475606719 */, 18 },
  /* 6636 */  { MAD_F(0x079c7a03) /* 0.475702296 */, 18 },
  /* 6637 */  { MAD_F(0x079cde3c) /* 0.475797879 */, 18 },
  /* 6638 */  { MAD_F(0x079d4277) /* 0.475893466 */, 18 },
  /* 6639 */  { MAD_F(0x079da6b4) /* 0.475989058 */, 18 },

  /* 6640 */  { MAD_F(0x079e0af1) /* 0.476084655 */, 18 },
  /* 6641 */  { MAD_F(0x079e6f30) /* 0.476180257 */, 18 },
  /* 6642 */  { MAD_F(0x079ed370) /* 0.476275863 */, 18 },
  /* 6643 */  { MAD_F(0x079f37b2) /* 0.476371475 */, 18 },
  /* 6644 */  { MAD_F(0x079f9bf5) /* 0.476467091 */, 18 },
  /* 6645 */  { MAD_F(0x07a00039) /* 0.476562712 */, 18 },
  /* 6646 */  { MAD_F(0x07a0647e) /* 0.476658338 */, 18 },
  /* 6647 */  { MAD_F(0x07a0c8c5) /* 0.476753968 */, 18 },
  /* 6648 */  { MAD_F(0x07a12d0c) /* 0.476849603 */, 18 },
  /* 6649 */  { MAD_F(0x07a19156) /* 0.476945243 */, 18 },
  /* 6650 */  { MAD_F(0x07a1f5a0) /* 0.477040888 */, 18 },
  /* 6651 */  { MAD_F(0x07a259ec) /* 0.477136538 */, 18 },
  /* 6652 */  { MAD_F(0x07a2be39) /* 0.477232193 */, 18 },
  /* 6653 */  { MAD_F(0x07a32287) /* 0.477327852 */, 18 },
  /* 6654 */  { MAD_F(0x07a386d7) /* 0.477423516 */, 18 },
  /* 6655 */  { MAD_F(0x07a3eb28) /* 0.477519185 */, 18 },

  /* 6656 */  { MAD_F(0x07a44f7a) /* 0.477614858 */, 18 },
  /* 6657 */  { MAD_F(0x07a4b3ce) /* 0.477710537 */, 18 },
  /* 6658 */  { MAD_F(0x07a51822) /* 0.477806220 */, 18 },
  /* 6659 */  { MAD_F(0x07a57c78) /* 0.477901908 */, 18 },
  /* 6660 */  { MAD_F(0x07a5e0d0) /* 0.477997601 */, 18 },
  /* 6661 */  { MAD_F(0x07a64528) /* 0.478093299 */, 18 },
  /* 6662 */  { MAD_F(0x07a6a982) /* 0.478189001 */, 18 },
  /* 6663 */  { MAD_F(0x07a70ddd) /* 0.478284708 */, 18 },
  /* 6664 */  { MAD_F(0x07a7723a) /* 0.478380420 */, 18 },
  /* 6665 */  { MAD_F(0x07a7d698) /* 0.478476137 */, 18 },
  /* 6666 */  { MAD_F(0x07a83af7) /* 0.478571858 */, 18 },
  /* 6667 */  { MAD_F(0x07a89f57) /* 0.478667585 */, 18 },
  /* 6668 */  { MAD_F(0x07a903b9) /* 0.478763316 */, 18 },
  /* 6669 */  { MAD_F(0x07a9681c) /* 0.478859052 */, 18 },
  /* 6670 */  { MAD_F(0x07a9cc80) /* 0.478954793 */, 18 },
  /* 6671 */  { MAD_F(0x07aa30e5) /* 0.479050538 */, 18 },

  /* 6672 */  { MAD_F(0x07aa954c) /* 0.479146288 */, 18 },
  /* 6673 */  { MAD_F(0x07aaf9b4) /* 0.479242043 */, 18 },
  /* 6674 */  { MAD_F(0x07ab5e1e) /* 0.479337803 */, 18 },
  /* 6675 */  { MAD_F(0x07abc288) /* 0.479433568 */, 18 },
  /* 6676 */  { MAD_F(0x07ac26f4) /* 0.479529337 */, 18 },
  /* 6677 */  { MAD_F(0x07ac8b61) /* 0.479625111 */, 18 },
  /* 6678 */  { MAD_F(0x07acefd0) /* 0.479720890 */, 18 },
  /* 6679 */  { MAD_F(0x07ad543f) /* 0.479816674 */, 18 },
  /* 6680 */  { MAD_F(0x07adb8b0) /* 0.479912463 */, 18 },
  /* 6681 */  { MAD_F(0x07ae1d23) /* 0.480008256 */, 18 },
  /* 6682 */  { MAD_F(0x07ae8196) /* 0.480104054 */, 18 },
  /* 6683 */  { MAD_F(0x07aee60b) /* 0.480199857 */, 18 },
  /* 6684 */  { MAD_F(0x07af4a81) /* 0.480295664 */, 18 },
  /* 6685 */  { MAD_F(0x07afaef9) /* 0.480391477 */, 18 },
  /* 6686 */  { MAD_F(0x07b01372) /* 0.480487294 */, 18 },
  /* 6687 */  { MAD_F(0x07b077ec) /* 0.480583116 */, 18 },

  /* 6688 */  { MAD_F(0x07b0dc67) /* 0.480678943 */, 18 },
  /* 6689 */  { MAD_F(0x07b140e4) /* 0.480774774 */, 18 },
  /* 6690 */  { MAD_F(0x07b1a561) /* 0.480870611 */, 18 },
  /* 6691 */  { MAD_F(0x07b209e1) /* 0.480966452 */, 18 },
  /* 6692 */  { MAD_F(0x07b26e61) /* 0.481062298 */, 18 },
  /* 6693 */  { MAD_F(0x07b2d2e3) /* 0.481158148 */, 18 },
  /* 6694 */  { MAD_F(0x07b33766) /* 0.481254004 */, 18 },
  /* 6695 */  { MAD_F(0x07b39bea) /* 0.481349864 */, 18 },
  /* 6696 */  { MAD_F(0x07b4006f) /* 0.481445729 */, 18 },
  /* 6697 */  { MAD_F(0x07b464f6) /* 0.481541598 */, 18 },
  /* 6698 */  { MAD_F(0x07b4c97e) /* 0.481637473 */, 18 },
  /* 6699 */  { MAD_F(0x07b52e08) /* 0.481733352 */, 18 },
  /* 6700 */  { MAD_F(0x07b59292) /* 0.481829236 */, 18 },
  /* 6701 */  { MAD_F(0x07b5f71e) /* 0.481925125 */, 18 },
  /* 6702 */  { MAD_F(0x07b65bac) /* 0.482021019 */, 18 },
  /* 6703 */  { MAD_F(0x07b6c03a) /* 0.482116917 */, 18 },

  /* 6704 */  { MAD_F(0x07b724ca) /* 0.482212820 */, 18 },
  /* 6705 */  { MAD_F(0x07b7895b) /* 0.482308728 */, 18 },
  /* 6706 */  { MAD_F(0x07b7eded) /* 0.482404640 */, 18 },
  /* 6707 */  { MAD_F(0x07b85281) /* 0.482500558 */, 18 },
  /* 6708 */  { MAD_F(0x07b8b716) /* 0.482596480 */, 18 },
  /* 6709 */  { MAD_F(0x07b91bac) /* 0.482692407 */, 18 },
  /* 6710 */  { MAD_F(0x07b98044) /* 0.482788339 */, 18 },
  /* 6711 */  { MAD_F(0x07b9e4dc) /* 0.482884275 */, 18 },
  /* 6712 */  { MAD_F(0x07ba4976) /* 0.482980216 */, 18 },
  /* 6713 */  { MAD_F(0x07baae12) /* 0.483076162 */, 18 },
  /* 6714 */  { MAD_F(0x07bb12ae) /* 0.483172113 */, 18 },
  /* 6715 */  { MAD_F(0x07bb774c) /* 0.483268069 */, 18 },
  /* 6716 */  { MAD_F(0x07bbdbeb) /* 0.483364029 */, 18 },
  /* 6717 */  { MAD_F(0x07bc408c) /* 0.483459994 */, 18 },
  /* 6718 */  { MAD_F(0x07bca52d) /* 0.483555964 */, 18 },
  /* 6719 */  { MAD_F(0x07bd09d0) /* 0.483651939 */, 18 },

  /* 6720 */  { MAD_F(0x07bd6e75) /* 0.483747918 */, 18 },
  /* 6721 */  { MAD_F(0x07bdd31a) /* 0.483843902 */, 18 },
  /* 6722 */  { MAD_F(0x07be37c1) /* 0.483939891 */, 18 },
  /* 6723 */  { MAD_F(0x07be9c69) /* 0.484035885 */, 18 },
  /* 6724 */  { MAD_F(0x07bf0113) /* 0.484131883 */, 18 },
  /* 6725 */  { MAD_F(0x07bf65bd) /* 0.484227886 */, 18 },
  /* 6726 */  { MAD_F(0x07bfca69) /* 0.484323894 */, 18 },
  /* 6727 */  { MAD_F(0x07c02f16) /* 0.484419907 */, 18 },
  /* 6728 */  { MAD_F(0x07c093c5) /* 0.484515924 */, 18 },
  /* 6729 */  { MAD_F(0x07c0f875) /* 0.484611946 */, 18 },
  /* 6730 */  { MAD_F(0x07c15d26) /* 0.484707973 */, 18 },
  /* 6731 */  { MAD_F(0x07c1c1d8) /* 0.484804005 */, 18 },
  /* 6732 */  { MAD_F(0x07c2268b) /* 0.484900041 */, 18 },
  /* 6733 */  { MAD_F(0x07c28b40) /* 0.484996083 */, 18 },
  /* 6734 */  { MAD_F(0x07c2eff6) /* 0.485092128 */, 18 },
  /* 6735 */  { MAD_F(0x07c354ae) /* 0.485188179 */, 18 },

  /* 6736 */  { MAD_F(0x07c3b967) /* 0.485284235 */, 18 },
  /* 6737 */  { MAD_F(0x07c41e21) /* 0.485380295 */, 18 },
  /* 6738 */  { MAD_F(0x07c482dc) /* 0.485476360 */, 18 },
  /* 6739 */  { MAD_F(0x07c4e798) /* 0.485572430 */, 18 },
  /* 6740 */  { MAD_F(0x07c54c56) /* 0.485668504 */, 18 },
  /* 6741 */  { MAD_F(0x07c5b115) /* 0.485764583 */, 18 },
  /* 6742 */  { MAD_F(0x07c615d6) /* 0.485860667 */, 18 },
  /* 6743 */  { MAD_F(0x07c67a97) /* 0.485956756 */, 18 },
  /* 6744 */  { MAD_F(0x07c6df5a) /* 0.486052849 */, 18 },
  /* 6745 */  { MAD_F(0x07c7441e) /* 0.486148948 */, 18 },
  /* 6746 */  { MAD_F(0x07c7a8e4) /* 0.486245051 */, 18 },
  /* 6747 */  { MAD_F(0x07c80daa) /* 0.486341158 */, 18 },
  /* 6748 */  { MAD_F(0x07c87272) /* 0.486437271 */, 18 },
  /* 6749 */  { MAD_F(0x07c8d73c) /* 0.486533388 */, 18 },
  /* 6750 */  { MAD_F(0x07c93c06) /* 0.486629510 */, 18 },
  /* 6751 */  { MAD_F(0x07c9a0d2) /* 0.486725637 */, 18 },

  /* 6752 */  { MAD_F(0x07ca059f) /* 0.486821768 */, 18 },
  /* 6753 */  { MAD_F(0x07ca6a6d) /* 0.486917905 */, 18 },
  /* 6754 */  { MAD_F(0x07cacf3d) /* 0.487014045 */, 18 },
  /* 6755 */  { MAD_F(0x07cb340e) /* 0.487110191 */, 18 },
  /* 6756 */  { MAD_F(0x07cb98e0) /* 0.487206342 */, 18 },
  /* 6757 */  { MAD_F(0x07cbfdb4) /* 0.487302497 */, 18 },
  /* 6758 */  { MAD_F(0x07cc6288) /* 0.487398657 */, 18 },
  /* 6759 */  { MAD_F(0x07ccc75e) /* 0.487494821 */, 18 },
  /* 6760 */  { MAD_F(0x07cd2c36) /* 0.487590991 */, 18 },
  /* 6761 */  { MAD_F(0x07cd910e) /* 0.487687165 */, 18 },
  /* 6762 */  { MAD_F(0x07cdf5e8) /* 0.487783344 */, 18 },
  /* 6763 */  { MAD_F(0x07ce5ac3) /* 0.487879528 */, 18 },
  /* 6764 */  { MAD_F(0x07cebfa0) /* 0.487975716 */, 18 },
  /* 6765 */  { MAD_F(0x07cf247d) /* 0.488071909 */, 18 },
  /* 6766 */  { MAD_F(0x07cf895c) /* 0.488168107 */, 18 },
  /* 6767 */  { MAD_F(0x07cfee3c) /* 0.488264310 */, 18 },

  /* 6768 */  { MAD_F(0x07d0531e) /* 0.488360517 */, 18 },
  /* 6769 */  { MAD_F(0x07d0b801) /* 0.488456729 */, 18 },
  /* 6770 */  { MAD_F(0x07d11ce5) /* 0.488552946 */, 18 },
  /* 6771 */  { MAD_F(0x07d181ca) /* 0.488649167 */, 18 },
  /* 6772 */  { MAD_F(0x07d1e6b0) /* 0.488745394 */, 18 },
  /* 6773 */  { MAD_F(0x07d24b98) /* 0.488841625 */, 18 },
  /* 6774 */  { MAD_F(0x07d2b081) /* 0.488937860 */, 18 },
  /* 6775 */  { MAD_F(0x07d3156c) /* 0.489034101 */, 18 },
  /* 6776 */  { MAD_F(0x07d37a57) /* 0.489130346 */, 18 },
  /* 6777 */  { MAD_F(0x07d3df44) /* 0.489226596 */, 18 },
  /* 6778 */  { MAD_F(0x07d44432) /* 0.489322851 */, 18 },
  /* 6779 */  { MAD_F(0x07d4a922) /* 0.489419110 */, 18 },
  /* 6780 */  { MAD_F(0x07d50e13) /* 0.489515375 */, 18 },
  /* 6781 */  { MAD_F(0x07d57305) /* 0.489611643 */, 18 },
  /* 6782 */  { MAD_F(0x07d5d7f8) /* 0.489707917 */, 18 },
  /* 6783 */  { MAD_F(0x07d63cec) /* 0.489804195 */, 18 },

  /* 6784 */  { MAD_F(0x07d6a1e2) /* 0.489900479 */, 18 },
  /* 6785 */  { MAD_F(0x07d706d9) /* 0.489996766 */, 18 },
  /* 6786 */  { MAD_F(0x07d76bd2) /* 0.490093059 */, 18 },
  /* 6787 */  { MAD_F(0x07d7d0cb) /* 0.490189356 */, 18 },
  /* 6788 */  { MAD_F(0x07d835c6) /* 0.490285658 */, 18 },
  /* 6789 */  { MAD_F(0x07d89ac2) /* 0.490381965 */, 18 },
  /* 6790 */  { MAD_F(0x07d8ffc0) /* 0.490478277 */, 18 },
  /* 6791 */  { MAD_F(0x07d964be) /* 0.490574593 */, 18 },
  /* 6792 */  { MAD_F(0x07d9c9be) /* 0.490670914 */, 18 },
  /* 6793 */  { MAD_F(0x07da2ebf) /* 0.490767239 */, 18 },
  /* 6794 */  { MAD_F(0x07da93c2) /* 0.490863570 */, 18 },
  /* 6795 */  { MAD_F(0x07daf8c6) /* 0.490959905 */, 18 },
  /* 6796 */  { MAD_F(0x07db5dcb) /* 0.491056245 */, 18 },
  /* 6797 */  { MAD_F(0x07dbc2d1) /* 0.491152589 */, 18 },
  /* 6798 */  { MAD_F(0x07dc27d9) /* 0.491248939 */, 18 },
  /* 6799 */  { MAD_F(0x07dc8ce1) /* 0.491345293 */, 18 },

  /* 6800 */  { MAD_F(0x07dcf1ec) /* 0.491441651 */, 18 },
  /* 6801 */  { MAD_F(0x07dd56f7) /* 0.491538015 */, 18 },
  /* 6802 */  { MAD_F(0x07ddbc04) /* 0.491634383 */, 18 },
  /* 6803 */  { MAD_F(0x07de2111) /* 0.491730756 */, 18 },
  /* 6804 */  { MAD_F(0x07de8621) /* 0.491827134 */, 18 },
  /* 6805 */  { MAD_F(0x07deeb31) /* 0.491923516 */, 18 },
  /* 6806 */  { MAD_F(0x07df5043) /* 0.492019903 */, 18 },
  /* 6807 */  { MAD_F(0x07dfb556) /* 0.492116295 */, 18 },
  /* 6808 */  { MAD_F(0x07e01a6a) /* 0.492212691 */, 18 },
  /* 6809 */  { MAD_F(0x07e07f80) /* 0.492309093 */, 18 },
  /* 6810 */  { MAD_F(0x07e0e496) /* 0.492405499 */, 18 },
  /* 6811 */  { MAD_F(0x07e149ae) /* 0.492501909 */, 18 },
  /* 6812 */  { MAD_F(0x07e1aec8) /* 0.492598325 */, 18 },
  /* 6813 */  { MAD_F(0x07e213e2) /* 0.492694745 */, 18 },
  /* 6814 */  { MAD_F(0x07e278fe) /* 0.492791170 */, 18 },
  /* 6815 */  { MAD_F(0x07e2de1b) /* 0.492887599 */, 18 },

  /* 6816 */  { MAD_F(0x07e3433a) /* 0.492984033 */, 18 },
  /* 6817 */  { MAD_F(0x07e3a859) /* 0.493080472 */, 18 },
  /* 6818 */  { MAD_F(0x07e40d7a) /* 0.493176916 */, 18 },
  /* 6819 */  { MAD_F(0x07e4729c) /* 0.493273365 */, 18 },
  /* 6820 */  { MAD_F(0x07e4d7c0) /* 0.493369818 */, 18 },
  /* 6821 */  { MAD_F(0x07e53ce4) /* 0.493466275 */, 18 },
  /* 6822 */  { MAD_F(0x07e5a20a) /* 0.493562738 */, 18 },
  /* 6823 */  { MAD_F(0x07e60732) /* 0.493659205 */, 18 },
  /* 6824 */  { MAD_F(0x07e66c5a) /* 0.493755677 */, 18 },
  /* 6825 */  { MAD_F(0x07e6d184) /* 0.493852154 */, 18 },
  /* 6826 */  { MAD_F(0x07e736af) /* 0.493948635 */, 18 },
  /* 6827 */  { MAD_F(0x07e79bdb) /* 0.494045122 */, 18 },
  /* 6828 */  { MAD_F(0x07e80109) /* 0.494141612 */, 18 },
  /* 6829 */  { MAD_F(0x07e86638) /* 0.494238108 */, 18 },
  /* 6830 */  { MAD_F(0x07e8cb68) /* 0.494334608 */, 18 },
  /* 6831 */  { MAD_F(0x07e93099) /* 0.494431113 */, 18 },

  /* 6832 */  { MAD_F(0x07e995cc) /* 0.494527623 */, 18 },
  /* 6833 */  { MAD_F(0x07e9fb00) /* 0.494624137 */, 18 },
  /* 6834 */  { MAD_F(0x07ea6035) /* 0.494720656 */, 18 },
  /* 6835 */  { MAD_F(0x07eac56b) /* 0.494817180 */, 18 },
  /* 6836 */  { MAD_F(0x07eb2aa3) /* 0.494913709 */, 18 },
  /* 6837 */  { MAD_F(0x07eb8fdc) /* 0.495010242 */, 18 },
  /* 6838 */  { MAD_F(0x07ebf516) /* 0.495106780 */, 18 },
  /* 6839 */  { MAD_F(0x07ec5a51) /* 0.495203322 */, 18 },
  /* 6840 */  { MAD_F(0x07ecbf8e) /* 0.495299870 */, 18 },
  /* 6841 */  { MAD_F(0x07ed24cc) /* 0.495396422 */, 18 },
  /* 6842 */  { MAD_F(0x07ed8a0b) /* 0.495492978 */, 18 },
  /* 6843 */  { MAD_F(0x07edef4c) /* 0.495589540 */, 18 },
  /* 6844 */  { MAD_F(0x07ee548e) /* 0.495686106 */, 18 },
  /* 6845 */  { MAD_F(0x07eeb9d1) /* 0.495782677 */, 18 },
  /* 6846 */  { MAD_F(0x07ef1f15) /* 0.495879252 */, 18 },
  /* 6847 */  { MAD_F(0x07ef845b) /* 0.495975833 */, 18 },

  /* 6848 */  { MAD_F(0x07efe9a1) /* 0.496072418 */, 18 },
  /* 6849 */  { MAD_F(0x07f04ee9) /* 0.496169007 */, 18 },
  /* 6850 */  { MAD_F(0x07f0b433) /* 0.496265602 */, 18 },
  /* 6851 */  { MAD_F(0x07f1197d) /* 0.496362201 */, 18 },
  /* 6852 */  { MAD_F(0x07f17ec9) /* 0.496458804 */, 18 },
  /* 6853 */  { MAD_F(0x07f1e416) /* 0.496555413 */, 18 },
  /* 6854 */  { MAD_F(0x07f24965) /* 0.496652026 */, 18 },
  /* 6855 */  { MAD_F(0x07f2aeb5) /* 0.496748644 */, 18 },
  /* 6856 */  { MAD_F(0x07f31405) /* 0.496845266 */, 18 },
  /* 6857 */  { MAD_F(0x07f37958) /* 0.496941894 */, 18 },
  /* 6858 */  { MAD_F(0x07f3deab) /* 0.497038526 */, 18 },
  /* 6859 */  { MAD_F(0x07f44400) /* 0.497135162 */, 18 },
  /* 6860 */  { MAD_F(0x07f4a956) /* 0.497231804 */, 18 },
  /* 6861 */  { MAD_F(0x07f50ead) /* 0.497328450 */, 18 },
  /* 6862 */  { MAD_F(0x07f57405) /* 0.497425100 */, 18 },
  /* 6863 */  { MAD_F(0x07f5d95f) /* 0.497521756 */, 18 },

  /* 6864 */  { MAD_F(0x07f63eba) /* 0.497618416 */, 18 },
  /* 6865 */  { MAD_F(0x07f6a416) /* 0.497715081 */, 18 },
  /* 6866 */  { MAD_F(0x07f70974) /* 0.497811750 */, 18 },
  /* 6867 */  { MAD_F(0x07f76ed3) /* 0.497908425 */, 18 },
  /* 6868 */  { MAD_F(0x07f7d433) /* 0.498005103 */, 18 },
  /* 6869 */  { MAD_F(0x07f83994) /* 0.498101787 */, 18 },
  /* 6870 */  { MAD_F(0x07f89ef7) /* 0.498198475 */, 18 },
  /* 6871 */  { MAD_F(0x07f9045a) /* 0.498295168 */, 18 },
  /* 6872 */  { MAD_F(0x07f969c0) /* 0.498391866 */, 18 },
  /* 6873 */  { MAD_F(0x07f9cf26) /* 0.498488568 */, 18 },
  /* 6874 */  { MAD_F(0x07fa348e) /* 0.498585275 */, 18 },
  /* 6875 */  { MAD_F(0x07fa99f6) /* 0.498681987 */, 18 },
  /* 6876 */  { MAD_F(0x07faff60) /* 0.498778704 */, 18 },
  /* 6877 */  { MAD_F(0x07fb64cc) /* 0.498875425 */, 18 },
  /* 6878 */  { MAD_F(0x07fbca38) /* 0.498972150 */, 18 },
  /* 6879 */  { MAD_F(0x07fc2fa6) /* 0.499068881 */, 18 },

  /* 6880 */  { MAD_F(0x07fc9516) /* 0.499165616 */, 18 },
  /* 6881 */  { MAD_F(0x07fcfa86) /* 0.499262356 */, 18 },
  /* 6882 */  { MAD_F(0x07fd5ff8) /* 0.499359101 */, 18 },
  /* 6883 */  { MAD_F(0x07fdc56b) /* 0.499455850 */, 18 },
  /* 6884 */  { MAD_F(0x07fe2adf) /* 0.499552604 */, 18 },
  /* 6885 */  { MAD_F(0x07fe9054) /* 0.499649362 */, 18 },
  /* 6886 */  { MAD_F(0x07fef5cb) /* 0.499746126 */, 18 },
  /* 6887 */  { MAD_F(0x07ff5b43) /* 0.499842894 */, 18 },
  /* 6888 */  { MAD_F(0x07ffc0bc) /* 0.499939666 */, 18 },
  /* 6889 */  { MAD_F(0x0400131b) /* 0.250018222 */, 19 },
  /* 6890 */  { MAD_F(0x040045d9) /* 0.250066613 */, 19 },
  /* 6891 */  { MAD_F(0x04007897) /* 0.250115006 */, 19 },
  /* 6892 */  { MAD_F(0x0400ab57) /* 0.250163402 */, 19 },
  /* 6893 */  { MAD_F(0x0400de16) /* 0.250211800 */, 19 },
  /* 6894 */  { MAD_F(0x040110d7) /* 0.250260200 */, 19 },
  /* 6895 */  { MAD_F(0x04014398) /* 0.250308603 */, 19 },

  /* 6896 */  { MAD_F(0x04017659) /* 0.250357008 */, 19 },
  /* 6897 */  { MAD_F(0x0401a91c) /* 0.250405415 */, 19 },
  /* 6898 */  { MAD_F(0x0401dbdf) /* 0.250453825 */, 19 },
  /* 6899 */  { MAD_F(0x04020ea2) /* 0.250502237 */, 19 },
  /* 6900 */  { MAD_F(0x04024166) /* 0.250550652 */, 19 },
  /* 6901 */  { MAD_F(0x0402742b) /* 0.250599068 */, 19 },
  /* 6902 */  { MAD_F(0x0402a6f0) /* 0.250647488 */, 19 },
  /* 6903 */  { MAD_F(0x0402d9b6) /* 0.250695909 */, 19 },
  /* 6904 */  { MAD_F(0x04030c7d) /* 0.250744333 */, 19 },
  /* 6905 */  { MAD_F(0x04033f44) /* 0.250792759 */, 19 },
  /* 6906 */  { MAD_F(0x0403720c) /* 0.250841187 */, 19 },
  /* 6907 */  { MAD_F(0x0403a4d5) /* 0.250889618 */, 19 },
  /* 6908 */  { MAD_F(0x0403d79e) /* 0.250938051 */, 19 },
  /* 6909 */  { MAD_F(0x04040a68) /* 0.250986487 */, 19 },
  /* 6910 */  { MAD_F(0x04043d32) /* 0.251034924 */, 19 },
  /* 6911 */  { MAD_F(0x04046ffd) /* 0.251083365 */, 19 },

  /* 6912 */  { MAD_F(0x0404a2c9) /* 0.251131807 */, 19 },
  /* 6913 */  { MAD_F(0x0404d595) /* 0.251180252 */, 19 },
  /* 6914 */  { MAD_F(0x04050862) /* 0.251228699 */, 19 },
  /* 6915 */  { MAD_F(0x04053b30) /* 0.251277148 */, 19 },
  /* 6916 */  { MAD_F(0x04056dfe) /* 0.251325600 */, 19 },
  /* 6917 */  { MAD_F(0x0405a0cd) /* 0.251374054 */, 19 },
  /* 6918 */  { MAD_F(0x0405d39c) /* 0.251422511 */, 19 },
  /* 6919 */  { MAD_F(0x0406066c) /* 0.251470970 */, 19 },
  /* 6920 */  { MAD_F(0x0406393d) /* 0.251519431 */, 19 },
  /* 6921 */  { MAD_F(0x04066c0e) /* 0.251567894 */, 19 },
  /* 6922 */  { MAD_F(0x04069ee0) /* 0.251616360 */, 19 },
  /* 6923 */  { MAD_F(0x0406d1b3) /* 0.251664828 */, 19 },
  /* 6924 */  { MAD_F(0x04070486) /* 0.251713299 */, 19 },
  /* 6925 */  { MAD_F(0x0407375a) /* 0.251761772 */, 19 },
  /* 6926 */  { MAD_F(0x04076a2e) /* 0.251810247 */, 19 },
  /* 6927 */  { MAD_F(0x04079d03) /* 0.251858724 */, 19 },

  /* 6928 */  { MAD_F(0x0407cfd9) /* 0.251907204 */, 19 },
  /* 6929 */  { MAD_F(0x040802af) /* 0.251955686 */, 19 },
  /* 6930 */  { MAD_F(0x04083586) /* 0.252004171 */, 19 },
  /* 6931 */  { MAD_F(0x0408685e) /* 0.252052658 */, 19 },
  /* 6932 */  { MAD_F(0x04089b36) /* 0.252101147 */, 19 },
  /* 6933 */  { MAD_F(0x0408ce0f) /* 0.252149638 */, 19 },
  /* 6934 */  { MAD_F(0x040900e8) /* 0.252198132 */, 19 },
  /* 6935 */  { MAD_F(0x040933c2) /* 0.252246628 */, 19 },
  /* 6936 */  { MAD_F(0x0409669d) /* 0.252295127 */, 19 },
  /* 6937 */  { MAD_F(0x04099978) /* 0.252343627 */, 19 },
  /* 6938 */  { MAD_F(0x0409cc54) /* 0.252392131 */, 19 },
  /* 6939 */  { MAD_F(0x0409ff31) /* 0.252440636 */, 19 },
  /* 6940 */  { MAD_F(0x040a320e) /* 0.252489144 */, 19 },
  /* 6941 */  { MAD_F(0x040a64ec) /* 0.252537654 */, 19 },
  /* 6942 */  { MAD_F(0x040a97cb) /* 0.252586166 */, 19 },
  /* 6943 */  { MAD_F(0x040acaaa) /* 0.252634681 */, 19 },

  /* 6944 */  { MAD_F(0x040afd89) /* 0.252683198 */, 19 },
  /* 6945 */  { MAD_F(0x040b306a) /* 0.252731718 */, 19 },
  /* 6946 */  { MAD_F(0x040b634b) /* 0.252780240 */, 19 },
  /* 6947 */  { MAD_F(0x040b962c) /* 0.252828764 */, 19 },
  /* 6948 */  { MAD_F(0x040bc90e) /* 0.252877290 */, 19 },
  /* 6949 */  { MAD_F(0x040bfbf1) /* 0.252925819 */, 19 },
  /* 6950 */  { MAD_F(0x040c2ed5) /* 0.252974350 */, 19 },
  /* 6951 */  { MAD_F(0x040c61b9) /* 0.253022883 */, 19 },
  /* 6952 */  { MAD_F(0x040c949e) /* 0.253071419 */, 19 },
  /* 6953 */  { MAD_F(0x040cc783) /* 0.253119957 */, 19 },
  /* 6954 */  { MAD_F(0x040cfa69) /* 0.253168498 */, 19 },
  /* 6955 */  { MAD_F(0x040d2d4f) /* 0.253217040 */, 19 },
  /* 6956 */  { MAD_F(0x040d6037) /* 0.253265585 */, 19 },
  /* 6957 */  { MAD_F(0x040d931e) /* 0.253314133 */, 19 },
  /* 6958 */  { MAD_F(0x040dc607) /* 0.253362682 */, 19 },
  /* 6959 */  { MAD_F(0x040df8f0) /* 0.253411234 */, 19 },

  /* 6960 */  { MAD_F(0x040e2bda) /* 0.253459789 */, 19 },
  /* 6961 */  { MAD_F(0x040e5ec4) /* 0.253508345 */, 19 },
  /* 6962 */  { MAD_F(0x040e91af) /* 0.253556904 */, 19 },
  /* 6963 */  { MAD_F(0x040ec49b) /* 0.253605466 */, 19 },
  /* 6964 */  { MAD_F(0x040ef787) /* 0.253654029 */, 19 },
  /* 6965 */  { MAD_F(0x040f2a74) /* 0.253702595 */, 19 },
  /* 6966 */  { MAD_F(0x040f5d61) /* 0.253751164 */, 19 },
  /* 6967 */  { MAD_F(0x040f904f) /* 0.253799734 */, 19 },
  /* 6968 */  { MAD_F(0x040fc33e) /* 0.253848307 */, 19 },
  /* 6969 */  { MAD_F(0x040ff62d) /* 0.253896883 */, 19 },
  /* 6970 */  { MAD_F(0x0410291d) /* 0.253945460 */, 19 },
  /* 6971 */  { MAD_F(0x04105c0e) /* 0.253994040 */, 19 },
  /* 6972 */  { MAD_F(0x04108eff) /* 0.254042622 */, 19 },
  /* 6973 */  { MAD_F(0x0410c1f1) /* 0.254091207 */, 19 },
  /* 6974 */  { MAD_F(0x0410f4e3) /* 0.254139794 */, 19 },
  /* 6975 */  { MAD_F(0x041127d6) /* 0.254188383 */, 19 },

  /* 6976 */  { MAD_F(0x04115aca) /* 0.254236974 */, 19 },
  /* 6977 */  { MAD_F(0x04118dbe) /* 0.254285568 */, 19 },
  /* 6978 */  { MAD_F(0x0411c0b3) /* 0.254334165 */, 19 },
  /* 6979 */  { MAD_F(0x0411f3a9) /* 0.254382763 */, 19 },
  /* 6980 */  { MAD_F(0x0412269f) /* 0.254431364 */, 19 },
  /* 6981 */  { MAD_F(0x04125996) /* 0.254479967 */, 19 },
  /* 6982 */  { MAD_F(0x04128c8d) /* 0.254528572 */, 19 },
  /* 6983 */  { MAD_F(0x0412bf85) /* 0.254577180 */, 19 },
  /* 6984 */  { MAD_F(0x0412f27e) /* 0.254625790 */, 19 },
  /* 6985 */  { MAD_F(0x04132577) /* 0.254674403 */, 19 },
  /* 6986 */  { MAD_F(0x04135871) /* 0.254723017 */, 19 },
  /* 6987 */  { MAD_F(0x04138b6c) /* 0.254771635 */, 19 },
  /* 6988 */  { MAD_F(0x0413be67) /* 0.254820254 */, 19 },
  /* 6989 */  { MAD_F(0x0413f163) /* 0.254868876 */, 19 },
  /* 6990 */  { MAD_F(0x0414245f) /* 0.254917500 */, 19 },
  /* 6991 */  { MAD_F(0x0414575c) /* 0.254966126 */, 19 },

  /* 6992 */  { MAD_F(0x04148a5a) /* 0.255014755 */, 19 },
  /* 6993 */  { MAD_F(0x0414bd58) /* 0.255063386 */, 19 },
  /* 6994 */  { MAD_F(0x0414f057) /* 0.255112019 */, 19 },
  /* 6995 */  { MAD_F(0x04152356) /* 0.255160655 */, 19 },
  /* 6996 */  { MAD_F(0x04155657) /* 0.255209292 */, 19 },
  /* 6997 */  { MAD_F(0x04158957) /* 0.255257933 */, 19 },
  /* 6998 */  { MAD_F(0x0415bc59) /* 0.255306575 */, 19 },
  /* 6999 */  { MAD_F(0x0415ef5b) /* 0.255355220 */, 19 },
  /* 7000 */  { MAD_F(0x0416225d) /* 0.255403867 */, 19 },
  /* 7001 */  { MAD_F(0x04165561) /* 0.255452517 */, 19 },
  /* 7002 */  { MAD_F(0x04168864) /* 0.255501169 */, 19 },
  /* 7003 */  { MAD_F(0x0416bb69) /* 0.255549823 */, 19 },
  /* 7004 */  { MAD_F(0x0416ee6e) /* 0.255598479 */, 19 },
  /* 7005 */  { MAD_F(0x04172174) /* 0.255647138 */, 19 },
  /* 7006 */  { MAD_F(0x0417547a) /* 0.255695799 */, 19 },
  /* 7007 */  { MAD_F(0x04178781) /* 0.255744463 */, 19 },

  /* 7008 */  { MAD_F(0x0417ba89) /* 0.255793128 */, 19 },
  /* 7009 */  { MAD_F(0x0417ed91) /* 0.255841796 */, 19 },
  /* 7010 */  { MAD_F(0x0418209a) /* 0.255890467 */, 19 },
  /* 7011 */  { MAD_F(0x041853a3) /* 0.255939139 */, 19 },
  /* 7012 */  { MAD_F(0x041886ad) /* 0.255987814 */, 19 },
  /* 7013 */  { MAD_F(0x0418b9b8) /* 0.256036492 */, 19 },
  /* 7014 */  { MAD_F(0x0418ecc3) /* 0.256085171 */, 19 },
  /* 7015 */  { MAD_F(0x04191fcf) /* 0.256133853 */, 19 },
  /* 7016 */  { MAD_F(0x041952dc) /* 0.256182537 */, 19 },
  /* 7017 */  { MAD_F(0x041985e9) /* 0.256231224 */, 19 },
  /* 7018 */  { MAD_F(0x0419b8f7) /* 0.256279913 */, 19 },
  /* 7019 */  { MAD_F(0x0419ec05) /* 0.256328604 */, 19 },
  /* 7020 */  { MAD_F(0x041a1f15) /* 0.256377297 */, 19 },
  /* 7021 */  { MAD_F(0x041a5224) /* 0.256425993 */, 19 },
  /* 7022 */  { MAD_F(0x041a8534) /* 0.256474691 */, 19 },
  /* 7023 */  { MAD_F(0x041ab845) /* 0.256523392 */, 19 },

  /* 7024 */  { MAD_F(0x041aeb57) /* 0.256572095 */, 19 },
  /* 7025 */  { MAD_F(0x041b1e69) /* 0.256620800 */, 19 },
  /* 7026 */  { MAD_F(0x041b517c) /* 0.256669507 */, 19 },
  /* 7027 */  { MAD_F(0x041b848f) /* 0.256718217 */, 19 },
  /* 7028 */  { MAD_F(0x041bb7a3) /* 0.256766929 */, 19 },
  /* 7029 */  { MAD_F(0x041beab8) /* 0.256815643 */, 19 },
  /* 7030 */  { MAD_F(0x041c1dcd) /* 0.256864359 */, 19 },
  /* 7031 */  { MAD_F(0x041c50e3) /* 0.256913078 */, 19 },
  /* 7032 */  { MAD_F(0x041c83fa) /* 0.256961800 */, 19 },
  /* 7033 */  { MAD_F(0x041cb711) /* 0.257010523 */, 19 },
  /* 7034 */  { MAD_F(0x041cea28) /* 0.257059249 */, 19 },
  /* 7035 */  { MAD_F(0x041d1d41) /* 0.257107977 */, 19 },
  /* 7036 */  { MAD_F(0x041d505a) /* 0.257156708 */, 19 },
  /* 7037 */  { MAD_F(0x041d8373) /* 0.257205440 */, 19 },
  /* 7038 */  { MAD_F(0x041db68e) /* 0.257254175 */, 19 },
  /* 7039 */  { MAD_F(0x041de9a8) /* 0.257302913 */, 19 },

  /* 7040 */  { MAD_F(0x041e1cc4) /* 0.257351652 */, 19 },
  /* 7041 */  { MAD_F(0x041e4fe0) /* 0.257400394 */, 19 },
  /* 7042 */  { MAD_F(0x041e82fd) /* 0.257449139 */, 19 },
  /* 7043 */  { MAD_F(0x041eb61a) /* 0.257497885 */, 19 },
  /* 7044 */  { MAD_F(0x041ee938) /* 0.257546634 */, 19 },
  /* 7045 */  { MAD_F(0x041f1c57) /* 0.257595386 */, 19 },
  /* 7046 */  { MAD_F(0x041f4f76) /* 0.257644139 */, 19 },
  /* 7047 */  { MAD_F(0x041f8296) /* 0.257692895 */, 19 },
  /* 7048 */  { MAD_F(0x041fb5b6) /* 0.257741653 */, 19 },
  /* 7049 */  { MAD_F(0x041fe8d7) /* 0.257790414 */, 19 },
  /* 7050 */  { MAD_F(0x04201bf9) /* 0.257839176 */, 19 },
  /* 7051 */  { MAD_F(0x04204f1b) /* 0.257887941 */, 19 },
  /* 7052 */  { MAD_F(0x0420823e) /* 0.257936709 */, 19 },
  /* 7053 */  { MAD_F(0x0420b561) /* 0.257985478 */, 19 },
  /* 7054 */  { MAD_F(0x0420e885) /* 0.258034250 */, 19 },
  /* 7055 */  { MAD_F(0x04211baa) /* 0.258083025 */, 19 },

  /* 7056 */  { MAD_F(0x04214ed0) /* 0.258131801 */, 19 },
  /* 7057 */  { MAD_F(0x042181f6) /* 0.258180580 */, 19 },
  /* 7058 */  { MAD_F(0x0421b51c) /* 0.258229361 */, 19 },
  /* 7059 */  { MAD_F(0x0421e843) /* 0.258278145 */, 19 },
  /* 7060 */  { MAD_F(0x04221b6b) /* 0.258326931 */, 19 },
  /* 7061 */  { MAD_F(0x04224e94) /* 0.258375719 */, 19 },
  /* 7062 */  { MAD_F(0x042281bd) /* 0.258424509 */, 19 },
  /* 7063 */  { MAD_F(0x0422b4e6) /* 0.258473302 */, 19 },
  /* 7064 */  { MAD_F(0x0422e811) /* 0.258522097 */, 19 },
  /* 7065 */  { MAD_F(0x04231b3c) /* 0.258570894 */, 19 },
  /* 7066 */  { MAD_F(0x04234e67) /* 0.258619694 */, 19 },
  /* 7067 */  { MAD_F(0x04238193) /* 0.258668496 */, 19 },
  /* 7068 */  { MAD_F(0x0423b4c0) /* 0.258717300 */, 19 },
  /* 7069 */  { MAD_F(0x0423e7ee) /* 0.258766106 */, 19 },
  /* 7070 */  { MAD_F(0x04241b1c) /* 0.258814915 */, 19 },
  /* 7071 */  { MAD_F(0x04244e4a) /* 0.258863726 */, 19 },

  /* 7072 */  { MAD_F(0x04248179) /* 0.258912540 */, 19 },
  /* 7073 */  { MAD_F(0x0424b4a9) /* 0.258961356 */, 19 },
  /* 7074 */  { MAD_F(0x0424e7da) /* 0.259010174 */, 19 },
  /* 7075 */  { MAD_F(0x04251b0b) /* 0.259058994 */, 19 },
  /* 7076 */  { MAD_F(0x04254e3d) /* 0.259107817 */, 19 },
  /* 7077 */  { MAD_F(0x0425816f) /* 0.259156642 */, 19 },
  /* 7078 */  { MAD_F(0x0425b4a2) /* 0.259205469 */, 19 },
  /* 7079 */  { MAD_F(0x0425e7d6) /* 0.259254298 */, 19 },
  /* 7080 */  { MAD_F(0x04261b0a) /* 0.259303130 */, 19 },
  /* 7081 */  { MAD_F(0x04264e3f) /* 0.259351964 */, 19 },
  /* 7082 */  { MAD_F(0x04268174) /* 0.259400801 */, 19 },
  /* 7083 */  { MAD_F(0x0426b4aa) /* 0.259449639 */, 19 },
  /* 7084 */  { MAD_F(0x0426e7e1) /* 0.259498480 */, 19 },
  /* 7085 */  { MAD_F(0x04271b18) /* 0.259547324 */, 19 },
  /* 7086 */  { MAD_F(0x04274e50) /* 0.259596169 */, 19 },
  /* 7087 */  { MAD_F(0x04278188) /* 0.259645017 */, 19 },

  /* 7088 */  { MAD_F(0x0427b4c2) /* 0.259693868 */, 19 },
  /* 7089 */  { MAD_F(0x0427e7fb) /* 0.259742720 */, 19 },
  /* 7090 */  { MAD_F(0x04281b36) /* 0.259791575 */, 19 },
  /* 7091 */  { MAD_F(0x04284e71) /* 0.259840432 */, 19 },
  /* 7092 */  { MAD_F(0x042881ac) /* 0.259889291 */, 19 },
  /* 7093 */  { MAD_F(0x0428b4e8) /* 0.259938153 */, 19 },
  /* 7094 */  { MAD_F(0x0428e825) /* 0.259987017 */, 19 },
  /* 7095 */  { MAD_F(0x04291b63) /* 0.260035883 */, 19 },
  /* 7096 */  { MAD_F(0x04294ea1) /* 0.260084752 */, 19 },
  /* 7097 */  { MAD_F(0x042981df) /* 0.260133623 */, 19 },
  /* 7098 */  { MAD_F(0x0429b51f) /* 0.260182496 */, 19 },
  /* 7099 */  { MAD_F(0x0429e85f) /* 0.260231372 */, 19 },
  /* 7100 */  { MAD_F(0x042a1b9f) /* 0.260280249 */, 19 },
  /* 7101 */  { MAD_F(0x042a4ee0) /* 0.260329129 */, 19 },
  /* 7102 */  { MAD_F(0x042a8222) /* 0.260378012 */, 19 },
  /* 7103 */  { MAD_F(0x042ab564) /* 0.260426896 */, 19 },

  /* 7104 */  { MAD_F(0x042ae8a7) /* 0.260475783 */, 19 },
  /* 7105 */  { MAD_F(0x042b1beb) /* 0.260524673 */, 19 },
  /* 7106 */  { MAD_F(0x042b4f2f) /* 0.260573564 */, 19 },
  /* 7107 */  { MAD_F(0x042b8274) /* 0.260622458 */, 19 },
  /* 7108 */  { MAD_F(0x042bb5ba) /* 0.260671354 */, 19 },
  /* 7109 */  { MAD_F(0x042be900) /* 0.260720252 */, 19 },
  /* 7110 */  { MAD_F(0x042c1c46) /* 0.260769153 */, 19 },
  /* 7111 */  { MAD_F(0x042c4f8e) /* 0.260818056 */, 19 },
  /* 7112 */  { MAD_F(0x042c82d6) /* 0.260866961 */, 19 },
  /* 7113 */  { MAD_F(0x042cb61e) /* 0.260915869 */, 19 },
  /* 7114 */  { MAD_F(0x042ce967) /* 0.260964779 */, 19 },
  /* 7115 */  { MAD_F(0x042d1cb1) /* 0.261013691 */, 19 },
  /* 7116 */  { MAD_F(0x042d4ffb) /* 0.261062606 */, 19 },
  /* 7117 */  { MAD_F(0x042d8346) /* 0.261111522 */, 19 },
  /* 7118 */  { MAD_F(0x042db692) /* 0.261160441 */, 19 },
  /* 7119 */  { MAD_F(0x042de9de) /* 0.261209363 */, 19 },

  /* 7120 */  { MAD_F(0x042e1d2b) /* 0.261258286 */, 19 },
  /* 7121 */  { MAD_F(0x042e5078) /* 0.261307212 */, 19 },
  /* 7122 */  { MAD_F(0x042e83c6) /* 0.261356140 */, 19 },
  /* 7123 */  { MAD_F(0x042eb715) /* 0.261405071 */, 19 },
  /* 7124 */  { MAD_F(0x042eea64) /* 0.261454004 */, 19 },
  /* 7125 */  { MAD_F(0x042f1db4) /* 0.261502939 */, 19 },
  /* 7126 */  { MAD_F(0x042f5105) /* 0.261551876 */, 19 },
  /* 7127 */  { MAD_F(0x042f8456) /* 0.261600816 */, 19 },
  /* 7128 */  { MAD_F(0x042fb7a8) /* 0.261649758 */, 19 },
  /* 7129 */  { MAD_F(0x042feafa) /* 0.261698702 */, 19 },
  /* 7130 */  { MAD_F(0x04301e4d) /* 0.261747649 */, 19 },
  /* 7131 */  { MAD_F(0x043051a1) /* 0.261796597 */, 19 },
  /* 7132 */  { MAD_F(0x043084f5) /* 0.261845548 */, 19 },
  /* 7133 */  { MAD_F(0x0430b84a) /* 0.261894502 */, 19 },
  /* 7134 */  { MAD_F(0x0430eb9f) /* 0.261943458 */, 19 },
  /* 7135 */  { MAD_F(0x04311ef5) /* 0.261992416 */, 19 },

  /* 7136 */  { MAD_F(0x0431524c) /* 0.262041376 */, 19 },
  /* 7137 */  { MAD_F(0x043185a3) /* 0.262090338 */, 19 },
  /* 7138 */  { MAD_F(0x0431b8fb) /* 0.262139303 */, 19 },
  /* 7139 */  { MAD_F(0x0431ec54) /* 0.262188270 */, 19 },
  /* 7140 */  { MAD_F(0x04321fad) /* 0.262237240 */, 19 },
  /* 7141 */  { MAD_F(0x04325306) /* 0.262286211 */, 19 },
  /* 7142 */  { MAD_F(0x04328661) /* 0.262335185 */, 19 },
  /* 7143 */  { MAD_F(0x0432b9bc) /* 0.262384162 */, 19 },
  /* 7144 */  { MAD_F(0x0432ed17) /* 0.262433140 */, 19 },
  /* 7145 */  { MAD_F(0x04332074) /* 0.262482121 */, 19 },
  /* 7146 */  { MAD_F(0x043353d0) /* 0.262531104 */, 19 },
  /* 7147 */  { MAD_F(0x0433872e) /* 0.262580089 */, 19 },
  /* 7148 */  { MAD_F(0x0433ba8c) /* 0.262629077 */, 19 },
  /* 7149 */  { MAD_F(0x0433edea) /* 0.262678067 */, 19 },
  /* 7150 */  { MAD_F(0x0434214a) /* 0.262727059 */, 19 },
  /* 7151 */  { MAD_F(0x043454aa) /* 0.262776054 */, 19 },

  /* 7152 */  { MAD_F(0x0434880a) /* 0.262825051 */, 19 },
  /* 7153 */  { MAD_F(0x0434bb6b) /* 0.262874050 */, 19 },
  /* 7154 */  { MAD_F(0x0434eecd) /* 0.262923051 */, 19 },
  /* 7155 */  { MAD_F(0x0435222f) /* 0.262972055 */, 19 },
  /* 7156 */  { MAD_F(0x04355592) /* 0.263021061 */, 19 },
  /* 7157 */  { MAD_F(0x043588f6) /* 0.263070069 */, 19 },
  /* 7158 */  { MAD_F(0x0435bc5a) /* 0.263119079 */, 19 },
  /* 7159 */  { MAD_F(0x0435efbf) /* 0.263168092 */, 19 },
  /* 7160 */  { MAD_F(0x04362324) /* 0.263217107 */, 19 },
  /* 7161 */  { MAD_F(0x0436568a) /* 0.263266125 */, 19 },
  /* 7162 */  { MAD_F(0x043689f1) /* 0.263315144 */, 19 },
  /* 7163 */  { MAD_F(0x0436bd58) /* 0.263364166 */, 19 },
  /* 7164 */  { MAD_F(0x0436f0c0) /* 0.263413191 */, 19 },
  /* 7165 */  { MAD_F(0x04372428) /* 0.263462217 */, 19 },
  /* 7166 */  { MAD_F(0x04375791) /* 0.263511246 */, 19 },
  /* 7167 */  { MAD_F(0x04378afb) /* 0.263560277 */, 19 },

  /* 7168 */  { MAD_F(0x0437be65) /* 0.263609310 */, 19 },
  /* 7169 */  { MAD_F(0x0437f1d0) /* 0.263658346 */, 19 },
  /* 7170 */  { MAD_F(0x0438253c) /* 0.263707384 */, 19 },
  /* 7171 */  { MAD_F(0x043858a8) /* 0.263756424 */, 19 },
  /* 7172 */  { MAD_F(0x04388c14) /* 0.263805466 */, 19 },
  /* 7173 */  { MAD_F(0x0438bf82) /* 0.263854511 */, 19 },
  /* 7174 */  { MAD_F(0x0438f2f0) /* 0.263903558 */, 19 },
  /* 7175 */  { MAD_F(0x0439265e) /* 0.263952607 */, 19 },
  /* 7176 */  { MAD_F(0x043959cd) /* 0.264001659 */, 19 },
  /* 7177 */  { MAD_F(0x04398d3d) /* 0.264050713 */, 19 },
  /* 7178 */  { MAD_F(0x0439c0ae) /* 0.264099769 */, 19 },
  /* 7179 */  { MAD_F(0x0439f41f) /* 0.264148827 */, 19 },
  /* 7180 */  { MAD_F(0x043a2790) /* 0.264197888 */, 19 },
  /* 7181 */  { MAD_F(0x043a5b02) /* 0.264246951 */, 19 },
  /* 7182 */  { MAD_F(0x043a8e75) /* 0.264296016 */, 19 },
  /* 7183 */  { MAD_F(0x043ac1e9) /* 0.264345084 */, 19 },

  /* 7184 */  { MAD_F(0x043af55d) /* 0.264394153 */, 19 },
  /* 7185 */  { MAD_F(0x043b28d2) /* 0.264443225 */, 19 },
  /* 7186 */  { MAD_F(0x043b5c47) /* 0.264492300 */, 19 },
  /* 7187 */  { MAD_F(0x043b8fbd) /* 0.264541376 */, 19 },
  /* 7188 */  { MAD_F(0x043bc333) /* 0.264590455 */, 19 },
  /* 7189 */  { MAD_F(0x043bf6aa) /* 0.264639536 */, 19 },
  /* 7190 */  { MAD_F(0x043c2a22) /* 0.264688620 */, 19 },
  /* 7191 */  { MAD_F(0x043c5d9a) /* 0.264737706 */, 19 },
  /* 7192 */  { MAD_F(0x043c9113) /* 0.264786794 */, 19 },
  /* 7193 */  { MAD_F(0x043cc48d) /* 0.264835884 */, 19 },
  /* 7194 */  { MAD_F(0x043cf807) /* 0.264884976 */, 19 },
  /* 7195 */  { MAD_F(0x043d2b82) /* 0.264934071 */, 19 },
  /* 7196 */  { MAD_F(0x043d5efd) /* 0.264983168 */, 19 },
  /* 7197 */  { MAD_F(0x043d9279) /* 0.265032268 */, 19 },
  /* 7198 */  { MAD_F(0x043dc5f6) /* 0.265081369 */, 19 },
  /* 7199 */  { MAD_F(0x043df973) /* 0.265130473 */, 19 },

  /* 7200 */  { MAD_F(0x043e2cf1) /* 0.265179580 */, 19 },
  /* 7201 */  { MAD_F(0x043e6070) /* 0.265228688 */, 19 },
  /* 7202 */  { MAD_F(0x043e93ef) /* 0.265277799 */, 19 },
  /* 7203 */  { MAD_F(0x043ec76e) /* 0.265326912 */, 19 },
  /* 7204 */  { MAD_F(0x043efaef) /* 0.265376027 */, 19 },
  /* 7205 */  { MAD_F(0x043f2e6f) /* 0.265425145 */, 19 },
  /* 7206 */  { MAD_F(0x043f61f1) /* 0.265474264 */, 19 },
  /* 7207 */  { MAD_F(0x043f9573) /* 0.265523387 */, 19 },
  /* 7208 */  { MAD_F(0x043fc8f6) /* 0.265572511 */, 19 },
  /* 7209 */  { MAD_F(0x043ffc79) /* 0.265621638 */, 19 },
  /* 7210 */  { MAD_F(0x04402ffd) /* 0.265670766 */, 19 },
  /* 7211 */  { MAD_F(0x04406382) /* 0.265719898 */, 19 },
  /* 7212 */  { MAD_F(0x04409707) /* 0.265769031 */, 19 },
  /* 7213 */  { MAD_F(0x0440ca8d) /* 0.265818167 */, 19 },
  /* 7214 */  { MAD_F(0x0440fe13) /* 0.265867305 */, 19 },
  /* 7215 */  { MAD_F(0x0441319a) /* 0.265916445 */, 19 },

  /* 7216 */  { MAD_F(0x04416522) /* 0.265965588 */, 19 },
  /* 7217 */  { MAD_F(0x044198aa) /* 0.266014732 */, 19 },
  /* 7218 */  { MAD_F(0x0441cc33) /* 0.266063880 */, 19 },
  /* 7219 */  { MAD_F(0x0441ffbc) /* 0.266113029 */, 19 },
  /* 7220 */  { MAD_F(0x04423346) /* 0.266162181 */, 19 },
  /* 7221 */  { MAD_F(0x044266d1) /* 0.266211334 */, 19 },
  /* 7222 */  { MAD_F(0x04429a5c) /* 0.266260491 */, 19 },
  /* 7223 */  { MAD_F(0x0442cde8) /* 0.266309649 */, 19 },
  /* 7224 */  { MAD_F(0x04430174) /* 0.266358810 */, 19 },
  /* 7225 */  { MAD_F(0x04433501) /* 0.266407973 */, 19 },
  /* 7226 */  { MAD_F(0x0443688f) /* 0.266457138 */, 19 },
  /* 7227 */  { MAD_F(0x04439c1d) /* 0.266506305 */, 19 },
  /* 7228 */  { MAD_F(0x0443cfac) /* 0.266555475 */, 19 },
  /* 7229 */  { MAD_F(0x0444033c) /* 0.266604647 */, 19 },
  /* 7230 */  { MAD_F(0x044436cc) /* 0.266653822 */, 19 },
  /* 7231 */  { MAD_F(0x04446a5d) /* 0.266702998 */, 19 },

  /* 7232 */  { MAD_F(0x04449dee) /* 0.266752177 */, 19 },
  /* 7233 */  { MAD_F(0x0444d180) /* 0.266801358 */, 19 },
  /* 7234 */  { MAD_F(0x04450513) /* 0.266850541 */, 19 },
  /* 7235 */  { MAD_F(0x044538a6) /* 0.266899727 */, 19 },
  /* 7236 */  { MAD_F(0x04456c39) /* 0.266948915 */, 19 },
  /* 7237 */  { MAD_F(0x04459fce) /* 0.266998105 */, 19 },
  /* 7238 */  { MAD_F(0x0445d363) /* 0.267047298 */, 19 },
  /* 7239 */  { MAD_F(0x044606f8) /* 0.267096492 */, 19 },
  /* 7240 */  { MAD_F(0x04463a8f) /* 0.267145689 */, 19 },
  /* 7241 */  { MAD_F(0x04466e25) /* 0.267194888 */, 19 },
  /* 7242 */  { MAD_F(0x0446a1bd) /* 0.267244090 */, 19 },
  /* 7243 */  { MAD_F(0x0446d555) /* 0.267293294 */, 19 },
  /* 7244 */  { MAD_F(0x044708ee) /* 0.267342500 */, 19 },
  /* 7245 */  { MAD_F(0x04473c87) /* 0.267391708 */, 19 },
  /* 7246 */  { MAD_F(0x04477021) /* 0.267440919 */, 19 },
  /* 7247 */  { MAD_F(0x0447a3bb) /* 0.267490131 */, 19 },

  /* 7248 */  { MAD_F(0x0447d756) /* 0.267539347 */, 19 },
  /* 7249 */  { MAD_F(0x04480af2) /* 0.267588564 */, 19 },
  /* 7250 */  { MAD_F(0x04483e8e) /* 0.267637783 */, 19 },
  /* 7251 */  { MAD_F(0x0448722b) /* 0.267687005 */, 19 },
  /* 7252 */  { MAD_F(0x0448a5c9) /* 0.267736229 */, 19 },
  /* 7253 */  { MAD_F(0x0448d967) /* 0.267785456 */, 19 },
  /* 7254 */  { MAD_F(0x04490d05) /* 0.267834685 */, 19 },
  /* 7255 */  { MAD_F(0x044940a5) /* 0.267883915 */, 19 },
  /* 7256 */  { MAD_F(0x04497445) /* 0.267933149 */, 19 },
  /* 7257 */  { MAD_F(0x0449a7e5) /* 0.267982384 */, 19 },
  /* 7258 */  { MAD_F(0x0449db86) /* 0.268031622 */, 19 },
  /* 7259 */  { MAD_F(0x044a0f28) /* 0.268080862 */, 19 },
  /* 7260 */  { MAD_F(0x044a42ca) /* 0.268130104 */, 19 },
  /* 7261 */  { MAD_F(0x044a766d) /* 0.268179349 */, 19 },
  /* 7262 */  { MAD_F(0x044aaa11) /* 0.268228595 */, 19 },
  /* 7263 */  { MAD_F(0x044addb5) /* 0.268277844 */, 19 },

  /* 7264 */  { MAD_F(0x044b115a) /* 0.268327096 */, 19 },
  /* 7265 */  { MAD_F(0x044b44ff) /* 0.268376349 */, 19 },
  /* 7266 */  { MAD_F(0x044b78a5) /* 0.268425605 */, 19 },
  /* 7267 */  { MAD_F(0x044bac4c) /* 0.268474863 */, 19 },
  /* 7268 */  { MAD_F(0x044bdff3) /* 0.268524123 */, 19 },
  /* 7269 */  { MAD_F(0x044c139b) /* 0.268573386 */, 19 },
  /* 7270 */  { MAD_F(0x044c4743) /* 0.268622651 */, 19 },
  /* 7271 */  { MAD_F(0x044c7aec) /* 0.268671918 */, 19 },
  /* 7272 */  { MAD_F(0x044cae96) /* 0.268721187 */, 19 },
  /* 7273 */  { MAD_F(0x044ce240) /* 0.268770459 */, 19 },
  /* 7274 */  { MAD_F(0x044d15eb) /* 0.268819733 */, 19 },
  /* 7275 */  { MAD_F(0x044d4997) /* 0.268869009 */, 19 },
  /* 7276 */  { MAD_F(0x044d7d43) /* 0.268918287 */, 19 },
  /* 7277 */  { MAD_F(0x044db0ef) /* 0.268967568 */, 19 },
  /* 7278 */  { MAD_F(0x044de49d) /* 0.269016851 */, 19 },
  /* 7279 */  { MAD_F(0x044e184b) /* 0.269066136 */, 19 },

  /* 7280 */  { MAD_F(0x044e4bf9) /* 0.269115423 */, 19 },
  /* 7281 */  { MAD_F(0x044e7fa8) /* 0.269164713 */, 19 },
  /* 7282 */  { MAD_F(0x044eb358) /* 0.269214005 */, 19 },
  /* 7283 */  { MAD_F(0x044ee708) /* 0.269263299 */, 19 },
  /* 7284 */  { MAD_F(0x044f1ab9) /* 0.269312595 */, 19 },
  /* 7285 */  { MAD_F(0x044f4e6b) /* 0.269361894 */, 19 },
  /* 7286 */  { MAD_F(0x044f821d) /* 0.269411195 */, 19 },
  /* 7287 */  { MAD_F(0x044fb5cf) /* 0.269460498 */, 19 },
  /* 7288 */  { MAD_F(0x044fe983) /* 0.269509804 */, 19 },
  /* 7289 */  { MAD_F(0x04501d37) /* 0.269559111 */, 19 },
  /* 7290 */  { MAD_F(0x045050eb) /* 0.269608421 */, 19 },
  /* 7291 */  { MAD_F(0x045084a0) /* 0.269657734 */, 19 },
  /* 7292 */  { MAD_F(0x0450b856) /* 0.269707048 */, 19 },
  /* 7293 */  { MAD_F(0x0450ec0d) /* 0.269756365 */, 19 },
  /* 7294 */  { MAD_F(0x04511fc4) /* 0.269805684 */, 19 },
  /* 7295 */  { MAD_F(0x0451537b) /* 0.269855005 */, 19 },

  /* 7296 */  { MAD_F(0x04518733) /* 0.269904329 */, 19 },
  /* 7297 */  { MAD_F(0x0451baec) /* 0.269953654 */, 19 },
  /* 7298 */  { MAD_F(0x0451eea5) /* 0.270002982 */, 19 },
  /* 7299 */  { MAD_F(0x0452225f) /* 0.270052313 */, 19 },
  /* 7300 */  { MAD_F(0x0452561a) /* 0.270101645 */, 19 },
  /* 7301 */  { MAD_F(0x045289d5) /* 0.270150980 */, 19 },
  /* 7302 */  { MAD_F(0x0452bd91) /* 0.270200317 */, 19 },
  /* 7303 */  { MAD_F(0x0452f14d) /* 0.270249656 */, 19 },
  /* 7304 */  { MAD_F(0x0453250a) /* 0.270298998 */, 19 },
  /* 7305 */  { MAD_F(0x045358c8) /* 0.270348341 */, 19 },
  /* 7306 */  { MAD_F(0x04538c86) /* 0.270397687 */, 19 },
  /* 7307 */  { MAD_F(0x0453c045) /* 0.270447036 */, 19 },
  /* 7308 */  { MAD_F(0x0453f405) /* 0.270496386 */, 19 },
  /* 7309 */  { MAD_F(0x045427c5) /* 0.270545739 */, 19 },
  /* 7310 */  { MAD_F(0x04545b85) /* 0.270595094 */, 19 },
  /* 7311 */  { MAD_F(0x04548f46) /* 0.270644451 */, 19 },

  /* 7312 */  { MAD_F(0x0454c308) /* 0.270693811 */, 19 },
  /* 7313 */  { MAD_F(0x0454f6cb) /* 0.270743173 */, 19 },
  /* 7314 */  { MAD_F(0x04552a8e) /* 0.270792537 */, 19 },
  /* 7315 */  { MAD_F(0x04555e51) /* 0.270841903 */, 19 },
  /* 7316 */  { MAD_F(0x04559216) /* 0.270891271 */, 19 },
  /* 7317 */  { MAD_F(0x0455c5db) /* 0.270940642 */, 19 },
  /* 7318 */  { MAD_F(0x0455f9a0) /* 0.270990015 */, 19 },
  /* 7319 */  { MAD_F(0x04562d66) /* 0.271039390 */, 19 },
  /* 7320 */  { MAD_F(0x0456612d) /* 0.271088768 */, 19 },
  /* 7321 */  { MAD_F(0x045694f4) /* 0.271138148 */, 19 },
  /* 7322 */  { MAD_F(0x0456c8bc) /* 0.271187530 */, 19 },
  /* 7323 */  { MAD_F(0x0456fc84) /* 0.271236914 */, 19 },
  /* 7324 */  { MAD_F(0x0457304e) /* 0.271286301 */, 19 },
  /* 7325 */  { MAD_F(0x04576417) /* 0.271335689 */, 19 },
  /* 7326 */  { MAD_F(0x045797e2) /* 0.271385080 */, 19 },
  /* 7327 */  { MAD_F(0x0457cbac) /* 0.271434474 */, 19 },

  /* 7328 */  { MAD_F(0x0457ff78) /* 0.271483869 */, 19 },
  /* 7329 */  { MAD_F(0x04583344) /* 0.271533267 */, 19 },
  /* 7330 */  { MAD_F(0x04586711) /* 0.271582667 */, 19 },
  /* 7331 */  { MAD_F(0x04589ade) /* 0.271632069 */, 19 },
  /* 7332 */  { MAD_F(0x0458ceac) /* 0.271681474 */, 19 },
  /* 7333 */  { MAD_F(0x0459027b) /* 0.271730880 */, 19 },
  /* 7334 */  { MAD_F(0x0459364a) /* 0.271780289 */, 19 },
  /* 7335 */  { MAD_F(0x04596a19) /* 0.271829701 */, 19 },
  /* 7336 */  { MAD_F(0x04599dea) /* 0.271879114 */, 19 },
  /* 7337 */  { MAD_F(0x0459d1bb) /* 0.271928530 */, 19 },
  /* 7338 */  { MAD_F(0x045a058c) /* 0.271977948 */, 19 },
  /* 7339 */  { MAD_F(0x045a395e) /* 0.272027368 */, 19 },
  /* 7340 */  { MAD_F(0x045a6d31) /* 0.272076790 */, 19 },
  /* 7341 */  { MAD_F(0x045aa104) /* 0.272126215 */, 19 },
  /* 7342 */  { MAD_F(0x045ad4d8) /* 0.272175642 */, 19 },
  /* 7343 */  { MAD_F(0x045b08ad) /* 0.272225071 */, 19 },

  /* 7344 */  { MAD_F(0x045b3c82) /* 0.272274503 */, 19 },
  /* 7345 */  { MAD_F(0x045b7058) /* 0.272323936 */, 19 },
  /* 7346 */  { MAD_F(0x045ba42e) /* 0.272373372 */, 19 },
  /* 7347 */  { MAD_F(0x045bd805) /* 0.272422810 */, 19 },
  /* 7348 */  { MAD_F(0x045c0bdd) /* 0.272472251 */, 19 },
  /* 7349 */  { MAD_F(0x045c3fb5) /* 0.272521693 */, 19 },
  /* 7350 */  { MAD_F(0x045c738e) /* 0.272571138 */, 19 },
  /* 7351 */  { MAD_F(0x045ca767) /* 0.272620585 */, 19 },
  /* 7352 */  { MAD_F(0x045cdb41) /* 0.272670035 */, 19 },
  /* 7353 */  { MAD_F(0x045d0f1b) /* 0.272719486 */, 19 },
  /* 7354 */  { MAD_F(0x045d42f7) /* 0.272768940 */, 19 },
  /* 7355 */  { MAD_F(0x045d76d2) /* 0.272818396 */, 19 },
  /* 7356 */  { MAD_F(0x045daaaf) /* 0.272867855 */, 19 },
  /* 7357 */  { MAD_F(0x045dde8c) /* 0.272917315 */, 19 },
  /* 7358 */  { MAD_F(0x045e1269) /* 0.272966778 */, 19 },
  /* 7359 */  { MAD_F(0x045e4647) /* 0.273016243 */, 19 },

  /* 7360 */  { MAD_F(0x045e7a26) /* 0.273065710 */, 19 },
  /* 7361 */  { MAD_F(0x045eae06) /* 0.273115180 */, 19 },
  /* 7362 */  { MAD_F(0x045ee1e6) /* 0.273164652 */, 19 },
  /* 7363 */  { MAD_F(0x045f15c6) /* 0.273214126 */, 19 },
  /* 7364 */  { MAD_F(0x045f49a7) /* 0.273263602 */, 19 },
  /* 7365 */  { MAD_F(0x045f7d89) /* 0.273313081 */, 19 },
  /* 7366 */  { MAD_F(0x045fb16c) /* 0.273362561 */, 19 },
  /* 7367 */  { MAD_F(0x045fe54f) /* 0.273412044 */, 19 },
  /* 7368 */  { MAD_F(0x04601932) /* 0.273461530 */, 19 },
  /* 7369 */  { MAD_F(0x04604d16) /* 0.273511017 */, 19 },
  /* 7370 */  { MAD_F(0x046080fb) /* 0.273560507 */, 19 },
  /* 7371 */  { MAD_F(0x0460b4e1) /* 0.273609999 */, 19 },
  /* 7372 */  { MAD_F(0x0460e8c7) /* 0.273659493 */, 19 },
  /* 7373 */  { MAD_F(0x04611cad) /* 0.273708989 */, 19 },
  /* 7374 */  { MAD_F(0x04615094) /* 0.273758488 */, 19 },
  /* 7375 */  { MAD_F(0x0461847c) /* 0.273807989 */, 19 },

  /* 7376 */  { MAD_F(0x0461b864) /* 0.273857492 */, 19 },
  /* 7377 */  { MAD_F(0x0461ec4d) /* 0.273906997 */, 19 },
  /* 7378 */  { MAD_F(0x04622037) /* 0.273956505 */, 19 },
  /* 7379 */  { MAD_F(0x04625421) /* 0.274006015 */, 19 },
  /* 7380 */  { MAD_F(0x0462880c) /* 0.274055527 */, 19 },
  /* 7381 */  { MAD_F(0x0462bbf7) /* 0.274105041 */, 19 },
  /* 7382 */  { MAD_F(0x0462efe3) /* 0.274154558 */, 19 },
  /* 7383 */  { MAD_F(0x046323d0) /* 0.274204076 */, 19 },
  /* 7384 */  { MAD_F(0x046357bd) /* 0.274253597 */, 19 },
  /* 7385 */  { MAD_F(0x04638bab) /* 0.274303121 */, 19 },
  /* 7386 */  { MAD_F(0x0463bf99) /* 0.274352646 */, 19 },
  /* 7387 */  { MAD_F(0x0463f388) /* 0.274402174 */, 19 },
  /* 7388 */  { MAD_F(0x04642778) /* 0.274451704 */, 19 },
  /* 7389 */  { MAD_F(0x04645b68) /* 0.274501236 */, 19 },
  /* 7390 */  { MAD_F(0x04648f59) /* 0.274550771 */, 19 },
  /* 7391 */  { MAD_F(0x0464c34a) /* 0.274600307 */, 19 },

  /* 7392 */  { MAD_F(0x0464f73c) /* 0.274649846 */, 19 },
  /* 7393 */  { MAD_F(0x04652b2f) /* 0.274699387 */, 19 },
  /* 7394 */  { MAD_F(0x04655f22) /* 0.274748931 */, 19 },
  /* 7395 */  { MAD_F(0x04659316) /* 0.274798476 */, 19 },
  /* 7396 */  { MAD_F(0x0465c70a) /* 0.274848024 */, 19 },
  /* 7397 */  { MAD_F(0x0465faff) /* 0.274897574 */, 19 },
  /* 7398 */  { MAD_F(0x04662ef5) /* 0.274947126 */, 19 },
  /* 7399 */  { MAD_F(0x046662eb) /* 0.274996681 */, 19 },
  /* 7400 */  { MAD_F(0x046696e2) /* 0.275046238 */, 19 },
  /* 7401 */  { MAD_F(0x0466cad9) /* 0.275095797 */, 19 },
  /* 7402 */  { MAD_F(0x0466fed1) /* 0.275145358 */, 19 },
  /* 7403 */  { MAD_F(0x046732ca) /* 0.275194921 */, 19 },
  /* 7404 */  { MAD_F(0x046766c3) /* 0.275244487 */, 19 },
  /* 7405 */  { MAD_F(0x04679abd) /* 0.275294055 */, 19 },
  /* 7406 */  { MAD_F(0x0467ceb7) /* 0.275343625 */, 19 },
  /* 7407 */  { MAD_F(0x046802b2) /* 0.275393198 */, 19 },

  /* 7408 */  { MAD_F(0x046836ae) /* 0.275442772 */, 19 },
  /* 7409 */  { MAD_F(0x04686aaa) /* 0.275492349 */, 19 },
  /* 7410 */  { MAD_F(0x04689ea7) /* 0.275541928 */, 19 },
  /* 7411 */  { MAD_F(0x0468d2a4) /* 0.275591509 */, 19 },
  /* 7412 */  { MAD_F(0x046906a2) /* 0.275641093 */, 19 },
  /* 7413 */  { MAD_F(0x04693aa1) /* 0.275690679 */, 19 },
  /* 7414 */  { MAD_F(0x04696ea0) /* 0.275740267 */, 19 },
  /* 7415 */  { MAD_F(0x0469a2a0) /* 0.275789857 */, 19 },
  /* 7416 */  { MAD_F(0x0469d6a0) /* 0.275839449 */, 19 },
  /* 7417 */  { MAD_F(0x046a0aa1) /* 0.275889044 */, 19 },
  /* 7418 */  { MAD_F(0x046a3ea3) /* 0.275938641 */, 19 },
  /* 7419 */  { MAD_F(0x046a72a5) /* 0.275988240 */, 19 },
  /* 7420 */  { MAD_F(0x046aa6a8) /* 0.276037842 */, 19 },
  /* 7421 */  { MAD_F(0x046adaab) /* 0.276087445 */, 19 },
  /* 7422 */  { MAD_F(0x046b0eaf) /* 0.276137051 */, 19 },
  /* 7423 */  { MAD_F(0x046b42b3) /* 0.276186659 */, 19 },

  /* 7424 */  { MAD_F(0x046b76b9) /* 0.276236269 */, 19 },
  /* 7425 */  { MAD_F(0x046baabe) /* 0.276285882 */, 19 },
  /* 7426 */  { MAD_F(0x046bdec5) /* 0.276335497 */, 19 },
  /* 7427 */  { MAD_F(0x046c12cc) /* 0.276385113 */, 19 },
  /* 7428 */  { MAD_F(0x046c46d3) /* 0.276434733 */, 19 },
  /* 7429 */  { MAD_F(0x046c7adb) /* 0.276484354 */, 19 },
  /* 7430 */  { MAD_F(0x046caee4) /* 0.276533978 */, 19 },
  /* 7431 */  { MAD_F(0x046ce2ee) /* 0.276583604 */, 19 },
  /* 7432 */  { MAD_F(0x046d16f7) /* 0.276633232 */, 19 },
  /* 7433 */  { MAD_F(0x046d4b02) /* 0.276682862 */, 19 },
  /* 7434 */  { MAD_F(0x046d7f0d) /* 0.276732495 */, 19 },
  /* 7435 */  { MAD_F(0x046db319) /* 0.276782129 */, 19 },
  /* 7436 */  { MAD_F(0x046de725) /* 0.276831766 */, 19 },
  /* 7437 */  { MAD_F(0x046e1b32) /* 0.276881406 */, 19 },
  /* 7438 */  { MAD_F(0x046e4f40) /* 0.276931047 */, 19 },
  /* 7439 */  { MAD_F(0x046e834e) /* 0.276980691 */, 19 },

  /* 7440 */  { MAD_F(0x046eb75c) /* 0.277030337 */, 19 },
  /* 7441 */  { MAD_F(0x046eeb6c) /* 0.277079985 */, 19 },
  /* 7442 */  { MAD_F(0x046f1f7c) /* 0.277129635 */, 19 },
  /* 7443 */  { MAD_F(0x046f538c) /* 0.277179288 */, 19 },
  /* 7444 */  { MAD_F(0x046f879d) /* 0.277228942 */, 19 },
  /* 7445 */  { MAD_F(0x046fbbaf) /* 0.277278600 */, 19 },
  /* 7446 */  { MAD_F(0x046fefc1) /* 0.277328259 */, 19 },
  /* 7447 */  { MAD_F(0x047023d4) /* 0.277377920 */, 19 },
  /* 7448 */  { MAD_F(0x047057e8) /* 0.277427584 */, 19 },
  /* 7449 */  { MAD_F(0x04708bfc) /* 0.277477250 */, 19 },
  /* 7450 */  { MAD_F(0x0470c011) /* 0.277526918 */, 19 },
  /* 7451 */  { MAD_F(0x0470f426) /* 0.277576588 */, 19 },
  /* 7452 */  { MAD_F(0x0471283c) /* 0.277626261 */, 19 },
  /* 7453 */  { MAD_F(0x04715c52) /* 0.277675936 */, 19 },
  /* 7454 */  { MAD_F(0x04719069) /* 0.277725613 */, 19 },
  /* 7455 */  { MAD_F(0x0471c481) /* 0.277775292 */, 19 },

  /* 7456 */  { MAD_F(0x0471f899) /* 0.277824973 */, 19 },
  /* 7457 */  { MAD_F(0x04722cb2) /* 0.277874657 */, 19 },
  /* 7458 */  { MAD_F(0x047260cc) /* 0.277924343 */, 19 },
  /* 7459 */  { MAD_F(0x047294e6) /* 0.277974031 */, 19 },
  /* 7460 */  { MAD_F(0x0472c900) /* 0.278023722 */, 19 },
  /* 7461 */  { MAD_F(0x0472fd1b) /* 0.278073414 */, 19 },
  /* 7462 */  { MAD_F(0x04733137) /* 0.278123109 */, 19 },
  /* 7463 */  { MAD_F(0x04736554) /* 0.278172806 */, 19 },
  /* 7464 */  { MAD_F(0x04739971) /* 0.278222505 */, 19 },
  /* 7465 */  { MAD_F(0x0473cd8e) /* 0.278272207 */, 19 },
  /* 7466 */  { MAD_F(0x047401ad) /* 0.278321910 */, 19 },
  /* 7467 */  { MAD_F(0x047435cb) /* 0.278371616 */, 19 },
  /* 7468 */  { MAD_F(0x047469eb) /* 0.278421324 */, 19 },
  /* 7469 */  { MAD_F(0x04749e0b) /* 0.278471035 */, 19 },
  /* 7470 */  { MAD_F(0x0474d22c) /* 0.278520747 */, 19 },
  /* 7471 */  { MAD_F(0x0475064d) /* 0.278570462 */, 19 },

  /* 7472 */  { MAD_F(0x04753a6f) /* 0.278620179 */, 19 },
  /* 7473 */  { MAD_F(0x04756e91) /* 0.278669898 */, 19 },
  /* 7474 */  { MAD_F(0x0475a2b4) /* 0.278719619 */, 19 },
  /* 7475 */  { MAD_F(0x0475d6d7) /* 0.278769343 */, 19 },
  /* 7476 */  { MAD_F(0x04760afc) /* 0.278819069 */, 19 },
  /* 7477 */  { MAD_F(0x04763f20) /* 0.278868797 */, 19 },
  /* 7478 */  { MAD_F(0x04767346) /* 0.278918527 */, 19 },
  /* 7479 */  { MAD_F(0x0476a76c) /* 0.278968260 */, 19 },
  /* 7480 */  { MAD_F(0x0476db92) /* 0.279017995 */, 19 },
  /* 7481 */  { MAD_F(0x04770fba) /* 0.279067731 */, 19 },
  /* 7482 */  { MAD_F(0x047743e1) /* 0.279117471 */, 19 },
  /* 7483 */  { MAD_F(0x0477780a) /* 0.279167212 */, 19 },
  /* 7484 */  { MAD_F(0x0477ac33) /* 0.279216956 */, 19 },
  /* 7485 */  { MAD_F(0x0477e05c) /* 0.279266701 */, 19 },
  /* 7486 */  { MAD_F(0x04781486) /* 0.279316449 */, 19 },
  /* 7487 */  { MAD_F(0x047848b1) /* 0.279366200 */, 19 },

  /* 7488 */  { MAD_F(0x04787cdc) /* 0.279415952 */, 19 },
  /* 7489 */  { MAD_F(0x0478b108) /* 0.279465707 */, 19 },
  /* 7490 */  { MAD_F(0x0478e535) /* 0.279515464 */, 19 },
  /* 7491 */  { MAD_F(0x04791962) /* 0.279565223 */, 19 },
  /* 7492 */  { MAD_F(0x04794d8f) /* 0.279614984 */, 19 },
  /* 7493 */  { MAD_F(0x047981be) /* 0.279664748 */, 19 },
  /* 7494 */  { MAD_F(0x0479b5ed) /* 0.279714513 */, 19 },
  /* 7495 */  { MAD_F(0x0479ea1c) /* 0.279764281 */, 19 },
  /* 7496 */  { MAD_F(0x047a1e4c) /* 0.279814051 */, 19 },
  /* 7497 */  { MAD_F(0x047a527d) /* 0.279863824 */, 19 },
  /* 7498 */  { MAD_F(0x047a86ae) /* 0.279913598 */, 19 },
  /* 7499 */  { MAD_F(0x047abae0) /* 0.279963375 */, 19 },
  /* 7500 */  { MAD_F(0x047aef12) /* 0.280013154 */, 19 },
  /* 7501 */  { MAD_F(0x047b2346) /* 0.280062935 */, 19 },
  /* 7502 */  { MAD_F(0x047b5779) /* 0.280112719 */, 19 },
  /* 7503 */  { MAD_F(0x047b8bad) /* 0.280162504 */, 19 },

  /* 7504 */  { MAD_F(0x047bbfe2) /* 0.280212292 */, 19 },
  /* 7505 */  { MAD_F(0x047bf418) /* 0.280262082 */, 19 },
  /* 7506 */  { MAD_F(0x047c284e) /* 0.280311875 */, 19 },
  /* 7507 */  { MAD_F(0x047c5c84) /* 0.280361669 */, 19 },
  /* 7508 */  { MAD_F(0x047c90bb) /* 0.280411466 */, 19 },
  /* 7509 */  { MAD_F(0x047cc4f3) /* 0.280461265 */, 19 },
  /* 7510 */  { MAD_F(0x047cf92c) /* 0.280511066 */, 19 },
  /* 7511 */  { MAD_F(0x047d2d65) /* 0.280560869 */, 19 },
  /* 7512 */  { MAD_F(0x047d619e) /* 0.280610675 */, 19 },
  /* 7513 */  { MAD_F(0x047d95d8) /* 0.280660483 */, 19 },
  /* 7514 */  { MAD_F(0x047dca13) /* 0.280710292 */, 19 },
  /* 7515 */  { MAD_F(0x047dfe4e) /* 0.280760105 */, 19 },
  /* 7516 */  { MAD_F(0x047e328a) /* 0.280809919 */, 19 },
  /* 7517 */  { MAD_F(0x047e66c7) /* 0.280859736 */, 19 },
  /* 7518 */  { MAD_F(0x047e9b04) /* 0.280909554 */, 19 },
  /* 7519 */  { MAD_F(0x047ecf42) /* 0.280959375 */, 19 },

  /* 7520 */  { MAD_F(0x047f0380) /* 0.281009199 */, 19 },
  /* 7521 */  { MAD_F(0x047f37bf) /* 0.281059024 */, 19 },
  /* 7522 */  { MAD_F(0x047f6bff) /* 0.281108852 */, 19 },
  /* 7523 */  { MAD_F(0x047fa03f) /* 0.281158682 */, 19 },
  /* 7524 */  { MAD_F(0x047fd47f) /* 0.281208514 */, 19 },
  /* 7525 */  { MAD_F(0x048008c1) /* 0.281258348 */, 19 },
  /* 7526 */  { MAD_F(0x04803d02) /* 0.281308184 */, 19 },
  /* 7527 */  { MAD_F(0x04807145) /* 0.281358023 */, 19 },
  /* 7528 */  { MAD_F(0x0480a588) /* 0.281407864 */, 19 },
  /* 7529 */  { MAD_F(0x0480d9cc) /* 0.281457707 */, 19 },
  /* 7530 */  { MAD_F(0x04810e10) /* 0.281507552 */, 19 },
  /* 7531 */  { MAD_F(0x04814255) /* 0.281557400 */, 19 },
  /* 7532 */  { MAD_F(0x0481769a) /* 0.281607250 */, 19 },
  /* 7533 */  { MAD_F(0x0481aae0) /* 0.281657101 */, 19 },
  /* 7534 */  { MAD_F(0x0481df27) /* 0.281706956 */, 19 },
  /* 7535 */  { MAD_F(0x0482136e) /* 0.281756812 */, 19 },

  /* 7536 */  { MAD_F(0x048247b6) /* 0.281806670 */, 19 },
  /* 7537 */  { MAD_F(0x04827bfe) /* 0.281856531 */, 19 },
  /* 7538 */  { MAD_F(0x0482b047) /* 0.281906394 */, 19 },
  /* 7539 */  { MAD_F(0x0482e491) /* 0.281956259 */, 19 },
  /* 7540 */  { MAD_F(0x048318db) /* 0.282006127 */, 19 },
  /* 7541 */  { MAD_F(0x04834d26) /* 0.282055996 */, 19 },
  /* 7542 */  { MAD_F(0x04838171) /* 0.282105868 */, 19 },
  /* 7543 */  { MAD_F(0x0483b5bd) /* 0.282155742 */, 19 },
  /* 7544 */  { MAD_F(0x0483ea0a) /* 0.282205618 */, 19 },
  /* 7545 */  { MAD_F(0x04841e57) /* 0.282255496 */, 19 },
  /* 7546 */  { MAD_F(0x048452a4) /* 0.282305377 */, 19 },
  /* 7547 */  { MAD_F(0x048486f3) /* 0.282355260 */, 19 },
  /* 7548 */  { MAD_F(0x0484bb42) /* 0.282405145 */, 19 },
  /* 7549 */  { MAD_F(0x0484ef91) /* 0.282455032 */, 19 },
  /* 7550 */  { MAD_F(0x048523e1) /* 0.282504921 */, 19 },
  /* 7551 */  { MAD_F(0x04855832) /* 0.282554813 */, 19 },

  /* 7552 */  { MAD_F(0x04858c83) /* 0.282604707 */, 19 },
  /* 7553 */  { MAD_F(0x0485c0d5) /* 0.282654603 */, 19 },
  /* 7554 */  { MAD_F(0x0485f527) /* 0.282704501 */, 19 },
  /* 7555 */  { MAD_F(0x0486297a) /* 0.282754401 */, 19 },
  /* 7556 */  { MAD_F(0x04865dce) /* 0.282804304 */, 19 },
  /* 7557 */  { MAD_F(0x04869222) /* 0.282854209 */, 19 },
  /* 7558 */  { MAD_F(0x0486c677) /* 0.282904116 */, 19 },
  /* 7559 */  { MAD_F(0x0486facc) /* 0.282954025 */, 19 },
  /* 7560 */  { MAD_F(0x04872f22) /* 0.283003936 */, 19 },
  /* 7561 */  { MAD_F(0x04876379) /* 0.283053850 */, 19 },
  /* 7562 */  { MAD_F(0x048797d0) /* 0.283103766 */, 19 },
  /* 7563 */  { MAD_F(0x0487cc28) /* 0.283153684 */, 19 },
  /* 7564 */  { MAD_F(0x04880080) /* 0.283203604 */, 19 },
  /* 7565 */  { MAD_F(0x048834d9) /* 0.283253527 */, 19 },
  /* 7566 */  { MAD_F(0x04886933) /* 0.283303451 */, 19 },
  /* 7567 */  { MAD_F(0x04889d8d) /* 0.283353378 */, 19 },

  /* 7568 */  { MAD_F(0x0488d1e8) /* 0.283403307 */, 19 },
  /* 7569 */  { MAD_F(0x04890643) /* 0.283453238 */, 19 },
  /* 7570 */  { MAD_F(0x04893a9f) /* 0.283503172 */, 19 },
  /* 7571 */  { MAD_F(0x04896efb) /* 0.283553107 */, 19 },
  /* 7572 */  { MAD_F(0x0489a358) /* 0.283603045 */, 19 },
  /* 7573 */  { MAD_F(0x0489d7b6) /* 0.283652985 */, 19 },
  /* 7574 */  { MAD_F(0x048a0c14) /* 0.283702927 */, 19 },
  /* 7575 */  { MAD_F(0x048a4073) /* 0.283752872 */, 19 },
  /* 7576 */  { MAD_F(0x048a74d3) /* 0.283802818 */, 19 },
  /* 7577 */  { MAD_F(0x048aa933) /* 0.283852767 */, 19 },
  /* 7578 */  { MAD_F(0x048add93) /* 0.283902718 */, 19 },
  /* 7579 */  { MAD_F(0x048b11f5) /* 0.283952671 */, 19 },
  /* 7580 */  { MAD_F(0x048b4656) /* 0.284002627 */, 19 },
  /* 7581 */  { MAD_F(0x048b7ab9) /* 0.284052584 */, 19 },
  /* 7582 */  { MAD_F(0x048baf1c) /* 0.284102544 */, 19 },
  /* 7583 */  { MAD_F(0x048be37f) /* 0.284152506 */, 19 },

  /* 7584 */  { MAD_F(0x048c17e3) /* 0.284202470 */, 19 },
  /* 7585 */  { MAD_F(0x048c4c48) /* 0.284252436 */, 19 },
  /* 7586 */  { MAD_F(0x048c80ad) /* 0.284302405 */, 19 },
  /* 7587 */  { MAD_F(0x048cb513) /* 0.284352376 */, 19 },
  /* 7588 */  { MAD_F(0x048ce97a) /* 0.284402349 */, 19 },
  /* 7589 */  { MAD_F(0x048d1de1) /* 0.284452324 */, 19 },
  /* 7590 */  { MAD_F(0x048d5249) /* 0.284502301 */, 19 },
  /* 7591 */  { MAD_F(0x048d86b1) /* 0.284552281 */, 19 },
  /* 7592 */  { MAD_F(0x048dbb1a) /* 0.284602263 */, 19 },
  /* 7593 */  { MAD_F(0x048def83) /* 0.284652246 */, 19 },
  /* 7594 */  { MAD_F(0x048e23ed) /* 0.284702233 */, 19 },
  /* 7595 */  { MAD_F(0x048e5858) /* 0.284752221 */, 19 },
  /* 7596 */  { MAD_F(0x048e8cc3) /* 0.284802211 */, 19 },
  /* 7597 */  { MAD_F(0x048ec12f) /* 0.284852204 */, 19 },
  /* 7598 */  { MAD_F(0x048ef59b) /* 0.284902199 */, 19 },
  /* 7599 */  { MAD_F(0x048f2a08) /* 0.284952196 */, 19 },

  /* 7600 */  { MAD_F(0x048f5e76) /* 0.285002195 */, 19 },
  /* 7601 */  { MAD_F(0x048f92e4) /* 0.285052197 */, 19 },
  /* 7602 */  { MAD_F(0x048fc753) /* 0.285102201 */, 19 },
  /* 7603 */  { MAD_F(0x048ffbc2) /* 0.285152206 */, 19 },
  /* 7604 */  { MAD_F(0x04903032) /* 0.285202214 */, 19 },
  /* 7605 */  { MAD_F(0x049064a3) /* 0.285252225 */, 19 },
  /* 7606 */  { MAD_F(0x04909914) /* 0.285302237 */, 19 },
  /* 7607 */  { MAD_F(0x0490cd86) /* 0.285352252 */, 19 },
  /* 7608 */  { MAD_F(0x049101f8) /* 0.285402269 */, 19 },
  /* 7609 */  { MAD_F(0x0491366b) /* 0.285452288 */, 19 },
  /* 7610 */  { MAD_F(0x04916ade) /* 0.285502309 */, 19 },
  /* 7611 */  { MAD_F(0x04919f52) /* 0.285552332 */, 19 },
  /* 7612 */  { MAD_F(0x0491d3c7) /* 0.285602358 */, 19 },
  /* 7613 */  { MAD_F(0x0492083c) /* 0.285652386 */, 19 },
  /* 7614 */  { MAD_F(0x04923cb2) /* 0.285702416 */, 19 },
  /* 7615 */  { MAD_F(0x04927128) /* 0.285752448 */, 19 },

  /* 7616 */  { MAD_F(0x0492a59f) /* 0.285802482 */, 19 },
  /* 7617 */  { MAD_F(0x0492da17) /* 0.285852519 */, 19 },
  /* 7618 */  { MAD_F(0x04930e8f) /* 0.285902557 */, 19 },
  /* 7619 */  { MAD_F(0x04934308) /* 0.285952598 */, 19 },
  /* 7620 */  { MAD_F(0x04937781) /* 0.286002641 */, 19 },
  /* 7621 */  { MAD_F(0x0493abfb) /* 0.286052687 */, 19 },
  /* 7622 */  { MAD_F(0x0493e076) /* 0.286102734 */, 19 },
  /* 7623 */  { MAD_F(0x049414f1) /* 0.286152784 */, 19 },
  /* 7624 */  { MAD_F(0x0494496c) /* 0.286202836 */, 19 },
  /* 7625 */  { MAD_F(0x04947de9) /* 0.286252890 */, 19 },
  /* 7626 */  { MAD_F(0x0494b266) /* 0.286302946 */, 19 },
  /* 7627 */  { MAD_F(0x0494e6e3) /* 0.286353005 */, 19 },
  /* 7628 */  { MAD_F(0x04951b61) /* 0.286403065 */, 19 },
  /* 7629 */  { MAD_F(0x04954fe0) /* 0.286453128 */, 19 },
  /* 7630 */  { MAD_F(0x0495845f) /* 0.286503193 */, 19 },
  /* 7631 */  { MAD_F(0x0495b8df) /* 0.286553260 */, 19 },

  /* 7632 */  { MAD_F(0x0495ed5f) /* 0.286603329 */, 19 },
  /* 7633 */  { MAD_F(0x049621e0) /* 0.286653401 */, 19 },
  /* 7634 */  { MAD_F(0x04965662) /* 0.286703475 */, 19 },
  /* 7635 */  { MAD_F(0x04968ae4) /* 0.286753551 */, 19 },
  /* 7636 */  { MAD_F(0x0496bf67) /* 0.286803629 */, 19 },
  /* 7637 */  { MAD_F(0x0496f3ea) /* 0.286853709 */, 19 },
  /* 7638 */  { MAD_F(0x0497286e) /* 0.286903792 */, 19 },
  /* 7639 */  { MAD_F(0x04975cf2) /* 0.286953876 */, 19 },
  /* 7640 */  { MAD_F(0x04979177) /* 0.287003963 */, 19 },
  /* 7641 */  { MAD_F(0x0497c5fd) /* 0.287054052 */, 19 },
  /* 7642 */  { MAD_F(0x0497fa83) /* 0.287104143 */, 19 },
  /* 7643 */  { MAD_F(0x04982f0a) /* 0.287154237 */, 19 },
  /* 7644 */  { MAD_F(0x04986392) /* 0.287204332 */, 19 },
  /* 7645 */  { MAD_F(0x0498981a) /* 0.287254430 */, 19 },
  /* 7646 */  { MAD_F(0x0498cca2) /* 0.287304530 */, 19 },
  /* 7647 */  { MAD_F(0x0499012c) /* 0.287354632 */, 19 },

  /* 7648 */  { MAD_F(0x049935b5) /* 0.287404737 */, 19 },
  /* 7649 */  { MAD_F(0x04996a40) /* 0.287454843 */, 19 },
  /* 7650 */  { MAD_F(0x04999ecb) /* 0.287504952 */, 19 },
  /* 7651 */  { MAD_F(0x0499d356) /* 0.287555063 */, 19 },
  /* 7652 */  { MAD_F(0x049a07e2) /* 0.287605176 */, 19 },
  /* 7653 */  { MAD_F(0x049a3c6f) /* 0.287655291 */, 19 },
  /* 7654 */  { MAD_F(0x049a70fc) /* 0.287705409 */, 19 },
  /* 7655 */  { MAD_F(0x049aa58a) /* 0.287755528 */, 19 },
  /* 7656 */  { MAD_F(0x049ada19) /* 0.287805650 */, 19 },
  /* 7657 */  { MAD_F(0x049b0ea8) /* 0.287855774 */, 19 },
  /* 7658 */  { MAD_F(0x049b4337) /* 0.287905900 */, 19 },
  /* 7659 */  { MAD_F(0x049b77c8) /* 0.287956028 */, 19 },
  /* 7660 */  { MAD_F(0x049bac58) /* 0.288006159 */, 19 },
  /* 7661 */  { MAD_F(0x049be0ea) /* 0.288056292 */, 19 },
  /* 7662 */  { MAD_F(0x049c157c) /* 0.288106427 */, 19 },
  /* 7663 */  { MAD_F(0x049c4a0e) /* 0.288156564 */, 19 },

  /* 7664 */  { MAD_F(0x049c7ea1) /* 0.288206703 */, 19 },
  /* 7665 */  { MAD_F(0x049cb335) /* 0.288256844 */, 19 },
  /* 7666 */  { MAD_F(0x049ce7ca) /* 0.288306988 */, 19 },
  /* 7667 */  { MAD_F(0x049d1c5e) /* 0.288357134 */, 19 },
  /* 7668 */  { MAD_F(0x049d50f4) /* 0.288407282 */, 19 },
  /* 7669 */  { MAD_F(0x049d858a) /* 0.288457432 */, 19 },
  /* 7670 */  { MAD_F(0x049dba21) /* 0.288507584 */, 19 },
  /* 7671 */  { MAD_F(0x049deeb8) /* 0.288557739 */, 19 },
  /* 7672 */  { MAD_F(0x049e2350) /* 0.288607895 */, 19 },
  /* 7673 */  { MAD_F(0x049e57e8) /* 0.288658054 */, 19 },
  /* 7674 */  { MAD_F(0x049e8c81) /* 0.288708215 */, 19 },
  /* 7675 */  { MAD_F(0x049ec11b) /* 0.288758379 */, 19 },
  /* 7676 */  { MAD_F(0x049ef5b5) /* 0.288808544 */, 19 },
  /* 7677 */  { MAD_F(0x049f2a50) /* 0.288858712 */, 19 },
  /* 7678 */  { MAD_F(0x049f5eeb) /* 0.288908881 */, 19 },
  /* 7679 */  { MAD_F(0x049f9387) /* 0.288959053 */, 19 },

  /* 7680 */  { MAD_F(0x049fc824) /* 0.289009227 */, 19 },
  /* 7681 */  { MAD_F(0x049ffcc1) /* 0.289059404 */, 19 },
  /* 7682 */  { MAD_F(0x04a0315e) /* 0.289109582 */, 19 },
  /* 7683 */  { MAD_F(0x04a065fd) /* 0.289159763 */, 19 },
  /* 7684 */  { MAD_F(0x04a09a9b) /* 0.289209946 */, 19 },
  /* 7685 */  { MAD_F(0x04a0cf3b) /* 0.289260131 */, 19 },
  /* 7686 */  { MAD_F(0x04a103db) /* 0.289310318 */, 19 },
  /* 7687 */  { MAD_F(0x04a1387b) /* 0.289360507 */, 19 },
  /* 7688 */  { MAD_F(0x04a16d1d) /* 0.289410699 */, 19 },
  /* 7689 */  { MAD_F(0x04a1a1be) /* 0.289460893 */, 19 },
  /* 7690 */  { MAD_F(0x04a1d661) /* 0.289511088 */, 19 },
  /* 7691 */  { MAD_F(0x04a20b04) /* 0.289561287 */, 19 },
  /* 7692 */  { MAD_F(0x04a23fa7) /* 0.289611487 */, 19 },
  /* 7693 */  { MAD_F(0x04a2744b) /* 0.289661689 */, 19 },
  /* 7694 */  { MAD_F(0x04a2a8f0) /* 0.289711894 */, 19 },
  /* 7695 */  { MAD_F(0x04a2dd95) /* 0.289762101 */, 19 },

  /* 7696 */  { MAD_F(0x04a3123b) /* 0.289812309 */, 19 },
  /* 7697 */  { MAD_F(0x04a346e2) /* 0.289862521 */, 19 },
  /* 7698 */  { MAD_F(0x04a37b89) /* 0.289912734 */, 19 },
  /* 7699 */  { MAD_F(0x04a3b030) /* 0.289962949 */, 19 },
  /* 7700 */  { MAD_F(0x04a3e4d8) /* 0.290013167 */, 19 },
  /* 7701 */  { MAD_F(0x04a41981) /* 0.290063387 */, 19 },
  /* 7702 */  { MAD_F(0x04a44e2b) /* 0.290113609 */, 19 },
  /* 7703 */  { MAD_F(0x04a482d5) /* 0.290163833 */, 19 },
  /* 7704 */  { MAD_F(0x04a4b77f) /* 0.290214059 */, 19 },
  /* 7705 */  { MAD_F(0x04a4ec2a) /* 0.290264288 */, 19 },
  /* 7706 */  { MAD_F(0x04a520d6) /* 0.290314519 */, 19 },
  /* 7707 */  { MAD_F(0x04a55582) /* 0.290364751 */, 19 },
  /* 7708 */  { MAD_F(0x04a58a2f) /* 0.290414986 */, 19 },
  /* 7709 */  { MAD_F(0x04a5bedd) /* 0.290465224 */, 19 },
  /* 7710 */  { MAD_F(0x04a5f38b) /* 0.290515463 */, 19 },
  /* 7711 */  { MAD_F(0x04a62839) /* 0.290565705 */, 19 },

  /* 7712 */  { MAD_F(0x04a65ce8) /* 0.290615948 */, 19 },
  /* 7713 */  { MAD_F(0x04a69198) /* 0.290666194 */, 19 },
  /* 7714 */  { MAD_F(0x04a6c648) /* 0.290716442 */, 19 },
  /* 7715 */  { MAD_F(0x04a6faf9) /* 0.290766692 */, 19 },
  /* 7716 */  { MAD_F(0x04a72fab) /* 0.290816945 */, 19 },
  /* 7717 */  { MAD_F(0x04a7645d) /* 0.290867199 */, 19 },
  /* 7718 */  { MAD_F(0x04a79910) /* 0.290917456 */, 19 },
  /* 7719 */  { MAD_F(0x04a7cdc3) /* 0.290967715 */, 19 },
  /* 7720 */  { MAD_F(0x04a80277) /* 0.291017976 */, 19 },
  /* 7721 */  { MAD_F(0x04a8372b) /* 0.291068239 */, 19 },
  /* 7722 */  { MAD_F(0x04a86be0) /* 0.291118505 */, 19 },
  /* 7723 */  { MAD_F(0x04a8a096) /* 0.291168772 */, 19 },
  /* 7724 */  { MAD_F(0x04a8d54c) /* 0.291219042 */, 19 },
  /* 7725 */  { MAD_F(0x04a90a03) /* 0.291269314 */, 19 },
  /* 7726 */  { MAD_F(0x04a93eba) /* 0.291319588 */, 19 },
  /* 7727 */  { MAD_F(0x04a97372) /* 0.291369865 */, 19 },

  /* 7728 */  { MAD_F(0x04a9a82b) /* 0.291420143 */, 19 },
  /* 7729 */  { MAD_F(0x04a9dce4) /* 0.291470424 */, 19 },
  /* 7730 */  { MAD_F(0x04aa119d) /* 0.291520706 */, 19 },
  /* 7731 */  { MAD_F(0x04aa4658) /* 0.291570991 */, 19 },
  /* 7732 */  { MAD_F(0x04aa7b13) /* 0.291621278 */, 19 },
  /* 7733 */  { MAD_F(0x04aaafce) /* 0.291671568 */, 19 },
  /* 7734 */  { MAD_F(0x04aae48a) /* 0.291721859 */, 19 },
  /* 7735 */  { MAD_F(0x04ab1947) /* 0.291772153 */, 19 },
  /* 7736 */  { MAD_F(0x04ab4e04) /* 0.291822449 */, 19 },
  /* 7737 */  { MAD_F(0x04ab82c2) /* 0.291872747 */, 19 },
  /* 7738 */  { MAD_F(0x04abb780) /* 0.291923047 */, 19 },
  /* 7739 */  { MAD_F(0x04abec3f) /* 0.291973349 */, 19 },
  /* 7740 */  { MAD_F(0x04ac20fe) /* 0.292023653 */, 19 },
  /* 7741 */  { MAD_F(0x04ac55be) /* 0.292073960 */, 19 },
  /* 7742 */  { MAD_F(0x04ac8a7f) /* 0.292124269 */, 19 },
  /* 7743 */  { MAD_F(0x04acbf40) /* 0.292174580 */, 19 },

  /* 7744 */  { MAD_F(0x04acf402) /* 0.292224893 */, 19 },
  /* 7745 */  { MAD_F(0x04ad28c5) /* 0.292275208 */, 19 },
  /* 7746 */  { MAD_F(0x04ad5d88) /* 0.292325526 */, 19 },
  /* 7747 */  { MAD_F(0x04ad924b) /* 0.292375845 */, 19 },
  /* 7748 */  { MAD_F(0x04adc70f) /* 0.292426167 */, 19 },
  /* 7749 */  { MAD_F(0x04adfbd4) /* 0.292476491 */, 19 },
  /* 7750 */  { MAD_F(0x04ae3099) /* 0.292526817 */, 19 },
  /* 7751 */  { MAD_F(0x04ae655f) /* 0.292577145 */, 19 },
  /* 7752 */  { MAD_F(0x04ae9a26) /* 0.292627476 */, 19 },
  /* 7753 */  { MAD_F(0x04aeceed) /* 0.292677808 */, 19 },
  /* 7754 */  { MAD_F(0x04af03b4) /* 0.292728143 */, 19 },
  /* 7755 */  { MAD_F(0x04af387d) /* 0.292778480 */, 19 },
  /* 7756 */  { MAD_F(0x04af6d45) /* 0.292828819 */, 19 },
  /* 7757 */  { MAD_F(0x04afa20f) /* 0.292879160 */, 19 },
  /* 7758 */  { MAD_F(0x04afd6d9) /* 0.292929504 */, 19 },
  /* 7759 */  { MAD_F(0x04b00ba3) /* 0.292979849 */, 19 },

  /* 7760 */  { MAD_F(0x04b0406e) /* 0.293030197 */, 19 },
  /* 7761 */  { MAD_F(0x04b0753a) /* 0.293080547 */, 19 },
  /* 7762 */  { MAD_F(0x04b0aa06) /* 0.293130899 */, 19 },
  /* 7763 */  { MAD_F(0x04b0ded3) /* 0.293181253 */, 19 },
  /* 7764 */  { MAD_F(0x04b113a1) /* 0.293231610 */, 19 },
  /* 7765 */  { MAD_F(0x04b1486f) /* 0.293281968 */, 19 },
  /* 7766 */  { MAD_F(0x04b17d3d) /* 0.293332329 */, 19 },
  /* 7767 */  { MAD_F(0x04b1b20c) /* 0.293382692 */, 19 },
  /* 7768 */  { MAD_F(0x04b1e6dc) /* 0.293433057 */, 19 },
  /* 7769 */  { MAD_F(0x04b21bad) /* 0.293483424 */, 19 },
  /* 7770 */  { MAD_F(0x04b2507d) /* 0.293533794 */, 19 },
  /* 7771 */  { MAD_F(0x04b2854f) /* 0.293584165 */, 19 },
  /* 7772 */  { MAD_F(0x04b2ba21) /* 0.293634539 */, 19 },
  /* 7773 */  { MAD_F(0x04b2eef4) /* 0.293684915 */, 19 },
  /* 7774 */  { MAD_F(0x04b323c7) /* 0.293735293 */, 19 },
  /* 7775 */  { MAD_F(0x04b3589b) /* 0.293785673 */, 19 },

  /* 7776 */  { MAD_F(0x04b38d6f) /* 0.293836055 */, 19 },
  /* 7777 */  { MAD_F(0x04b3c244) /* 0.293886440 */, 19 },
  /* 7778 */  { MAD_F(0x04b3f71a) /* 0.293936826 */, 19 },
  /* 7779 */  { MAD_F(0x04b42bf0) /* 0.293987215 */, 19 },
  /* 7780 */  { MAD_F(0x04b460c7) /* 0.294037606 */, 19 },
  /* 7781 */  { MAD_F(0x04b4959e) /* 0.294087999 */, 19 },
  /* 7782 */  { MAD_F(0x04b4ca76) /* 0.294138395 */, 19 },
  /* 7783 */  { MAD_F(0x04b4ff4e) /* 0.294188792 */, 19 },
  /* 7784 */  { MAD_F(0x04b53427) /* 0.294239192 */, 19 },
  /* 7785 */  { MAD_F(0x04b56901) /* 0.294289593 */, 19 },
  /* 7786 */  { MAD_F(0x04b59ddb) /* 0.294339997 */, 19 },
  /* 7787 */  { MAD_F(0x04b5d2b6) /* 0.294390403 */, 19 },
  /* 7788 */  { MAD_F(0x04b60791) /* 0.294440812 */, 19 },
  /* 7789 */  { MAD_F(0x04b63c6d) /* 0.294491222 */, 19 },
  /* 7790 */  { MAD_F(0x04b6714a) /* 0.294541635 */, 19 },
  /* 7791 */  { MAD_F(0x04b6a627) /* 0.294592049 */, 19 },

  /* 7792 */  { MAD_F(0x04b6db05) /* 0.294642466 */, 19 },
  /* 7793 */  { MAD_F(0x04b70fe3) /* 0.294692885 */, 19 },
  /* 7794 */  { MAD_F(0x04b744c2) /* 0.294743306 */, 19 },
  /* 7795 */  { MAD_F(0x04b779a1) /* 0.294793730 */, 19 },
  /* 7796 */  { MAD_F(0x04b7ae81) /* 0.294844155 */, 19 },
  /* 7797 */  { MAD_F(0x04b7e362) /* 0.294894583 */, 19 },
  /* 7798 */  { MAD_F(0x04b81843) /* 0.294945013 */, 19 },
  /* 7799 */  { MAD_F(0x04b84d24) /* 0.294995445 */, 19 },
  /* 7800 */  { MAD_F(0x04b88207) /* 0.295045879 */, 19 },
  /* 7801 */  { MAD_F(0x04b8b6ea) /* 0.295096315 */, 19 },
  /* 7802 */  { MAD_F(0x04b8ebcd) /* 0.295146753 */, 19 },
  /* 7803 */  { MAD_F(0x04b920b1) /* 0.295197194 */, 19 },
  /* 7804 */  { MAD_F(0x04b95596) /* 0.295247637 */, 19 },
  /* 7805 */  { MAD_F(0x04b98a7b) /* 0.295298082 */, 19 },
  /* 7806 */  { MAD_F(0x04b9bf61) /* 0.295348529 */, 19 },
  /* 7807 */  { MAD_F(0x04b9f447) /* 0.295398978 */, 19 },

  /* 7808 */  { MAD_F(0x04ba292e) /* 0.295449429 */, 19 },
  /* 7809 */  { MAD_F(0x04ba5e16) /* 0.295499883 */, 19 },
  /* 7810 */  { MAD_F(0x04ba92fe) /* 0.295550338 */, 19 },
  /* 7811 */  { MAD_F(0x04bac7e6) /* 0.295600796 */, 19 },
  /* 7812 */  { MAD_F(0x04bafcd0) /* 0.295651256 */, 19 },
  /* 7813 */  { MAD_F(0x04bb31b9) /* 0.295701718 */, 19 },
  /* 7814 */  { MAD_F(0x04bb66a4) /* 0.295752183 */, 19 },
  /* 7815 */  { MAD_F(0x04bb9b8f) /* 0.295802649 */, 19 },
  /* 7816 */  { MAD_F(0x04bbd07a) /* 0.295853118 */, 19 },
  /* 7817 */  { MAD_F(0x04bc0566) /* 0.295903588 */, 19 },
  /* 7818 */  { MAD_F(0x04bc3a53) /* 0.295954061 */, 19 },
  /* 7819 */  { MAD_F(0x04bc6f40) /* 0.296004536 */, 19 },
  /* 7820 */  { MAD_F(0x04bca42e) /* 0.296055013 */, 19 },
  /* 7821 */  { MAD_F(0x04bcd91d) /* 0.296105493 */, 19 },
  /* 7822 */  { MAD_F(0x04bd0e0c) /* 0.296155974 */, 19 },
  /* 7823 */  { MAD_F(0x04bd42fb) /* 0.296206458 */, 19 },

  /* 7824 */  { MAD_F(0x04bd77ec) /* 0.296256944 */, 19 },
  /* 7825 */  { MAD_F(0x04bdacdc) /* 0.296307432 */, 19 },
  /* 7826 */  { MAD_F(0x04bde1ce) /* 0.296357922 */, 19 },
  /* 7827 */  { MAD_F(0x04be16c0) /* 0.296408414 */, 19 },
  /* 7828 */  { MAD_F(0x04be4bb2) /* 0.296458908 */, 19 },
  /* 7829 */  { MAD_F(0x04be80a5) /* 0.296509405 */, 19 },
  /* 7830 */  { MAD_F(0x04beb599) /* 0.296559904 */, 19 },
  /* 7831 */  { MAD_F(0x04beea8d) /* 0.296610404 */, 19 },
  /* 7832 */  { MAD_F(0x04bf1f82) /* 0.296660907 */, 19 },
  /* 7833 */  { MAD_F(0x04bf5477) /* 0.296711413 */, 19 },
  /* 7834 */  { MAD_F(0x04bf896d) /* 0.296761920 */, 19 },
  /* 7835 */  { MAD_F(0x04bfbe64) /* 0.296812429 */, 19 },
  /* 7836 */  { MAD_F(0x04bff35b) /* 0.296862941 */, 19 },
  /* 7837 */  { MAD_F(0x04c02852) /* 0.296913455 */, 19 },
  /* 7838 */  { MAD_F(0x04c05d4b) /* 0.296963971 */, 19 },
  /* 7839 */  { MAD_F(0x04c09243) /* 0.297014489 */, 19 },

  /* 7840 */  { MAD_F(0x04c0c73d) /* 0.297065009 */, 19 },
  /* 7841 */  { MAD_F(0x04c0fc37) /* 0.297115531 */, 19 },
  /* 7842 */  { MAD_F(0x04c13131) /* 0.297166056 */, 19 },
  /* 7843 */  { MAD_F(0x04c1662d) /* 0.297216582 */, 19 },
  /* 7844 */  { MAD_F(0x04c19b28) /* 0.297267111 */, 19 },
  /* 7845 */  { MAD_F(0x04c1d025) /* 0.297317642 */, 19 },
  /* 7846 */  { MAD_F(0x04c20521) /* 0.297368175 */, 19 },
  /* 7847 */  { MAD_F(0x04c23a1f) /* 0.297418710 */, 19 },
  /* 7848 */  { MAD_F(0x04c26f1d) /* 0.297469248 */, 19 },
  /* 7849 */  { MAD_F(0x04c2a41b) /* 0.297519787 */, 19 },
  /* 7850 */  { MAD_F(0x04c2d91b) /* 0.297570329 */, 19 },
  /* 7851 */  { MAD_F(0x04c30e1a) /* 0.297620873 */, 19 },
  /* 7852 */  { MAD_F(0x04c3431b) /* 0.297671418 */, 19 },
  /* 7853 */  { MAD_F(0x04c3781c) /* 0.297721967 */, 19 },
  /* 7854 */  { MAD_F(0x04c3ad1d) /* 0.297772517 */, 19 },
  /* 7855 */  { MAD_F(0x04c3e21f) /* 0.297823069 */, 19 },

  /* 7856 */  { MAD_F(0x04c41722) /* 0.297873624 */, 19 },
  /* 7857 */  { MAD_F(0x04c44c25) /* 0.297924180 */, 19 },
  /* 7858 */  { MAD_F(0x04c48129) /* 0.297974739 */, 19 },
  /* 7859 */  { MAD_F(0x04c4b62d) /* 0.298025300 */, 19 },
  /* 7860 */  { MAD_F(0x04c4eb32) /* 0.298075863 */, 19 },
  /* 7861 */  { MAD_F(0x04c52038) /* 0.298126429 */, 19 },
  /* 7862 */  { MAD_F(0x04c5553e) /* 0.298176996 */, 19 },
  /* 7863 */  { MAD_F(0x04c58a44) /* 0.298227565 */, 19 },
  /* 7864 */  { MAD_F(0x04c5bf4c) /* 0.298278137 */, 19 },
  /* 7865 */  { MAD_F(0x04c5f453) /* 0.298328711 */, 19 },
  /* 7866 */  { MAD_F(0x04c6295c) /* 0.298379287 */, 19 },
  /* 7867 */  { MAD_F(0x04c65e65) /* 0.298429865 */, 19 },
  /* 7868 */  { MAD_F(0x04c6936e) /* 0.298480445 */, 19 },
  /* 7869 */  { MAD_F(0x04c6c878) /* 0.298531028 */, 19 },
  /* 7870 */  { MAD_F(0x04c6fd83) /* 0.298581612 */, 19 },
  /* 7871 */  { MAD_F(0x04c7328e) /* 0.298632199 */, 19 },

  /* 7872 */  { MAD_F(0x04c7679a) /* 0.298682788 */, 19 },
  /* 7873 */  { MAD_F(0x04c79ca7) /* 0.298733379 */, 19 },
  /* 7874 */  { MAD_F(0x04c7d1b4) /* 0.298783972 */, 19 },
  /* 7875 */  { MAD_F(0x04c806c1) /* 0.298834567 */, 19 },
  /* 7876 */  { MAD_F(0x04c83bcf) /* 0.298885165 */, 19 },
  /* 7877 */  { MAD_F(0x04c870de) /* 0.298935764 */, 19 },
  /* 7878 */  { MAD_F(0x04c8a5ed) /* 0.298986366 */, 19 },
  /* 7879 */  { MAD_F(0x04c8dafd) /* 0.299036970 */, 19 },
  /* 7880 */  { MAD_F(0x04c9100d) /* 0.299087576 */, 19 },
  /* 7881 */  { MAD_F(0x04c9451e) /* 0.299138184 */, 19 },
  /* 7882 */  { MAD_F(0x04c97a30) /* 0.299188794 */, 19 },
  /* 7883 */  { MAD_F(0x04c9af42) /* 0.299239406 */, 19 },
  /* 7884 */  { MAD_F(0x04c9e455) /* 0.299290021 */, 19 },
  /* 7885 */  { MAD_F(0x04ca1968) /* 0.299340638 */, 19 },
  /* 7886 */  { MAD_F(0x04ca4e7c) /* 0.299391256 */, 19 },
  /* 7887 */  { MAD_F(0x04ca8391) /* 0.299441877 */, 19 },

  /* 7888 */  { MAD_F(0x04cab8a6) /* 0.299492500 */, 19 },
  /* 7889 */  { MAD_F(0x04caedbb) /* 0.299543126 */, 19 },
  /* 7890 */  { MAD_F(0x04cb22d1) /* 0.299593753 */, 19 },
  /* 7891 */  { MAD_F(0x04cb57e8) /* 0.299644382 */, 19 },
  /* 7892 */  { MAD_F(0x04cb8d00) /* 0.299695014 */, 19 },
  /* 7893 */  { MAD_F(0x04cbc217) /* 0.299745648 */, 19 },
  /* 7894 */  { MAD_F(0x04cbf730) /* 0.299796284 */, 19 },
  /* 7895 */  { MAD_F(0x04cc2c49) /* 0.299846922 */, 19 },
  /* 7896 */  { MAD_F(0x04cc6163) /* 0.299897562 */, 19 },
  /* 7897 */  { MAD_F(0x04cc967d) /* 0.299948204 */, 19 },
  /* 7898 */  { MAD_F(0x04cccb98) /* 0.299998849 */, 19 },
  /* 7899 */  { MAD_F(0x04cd00b3) /* 0.300049495 */, 19 },
  /* 7900 */  { MAD_F(0x04cd35cf) /* 0.300100144 */, 19 },
  /* 7901 */  { MAD_F(0x04cd6aeb) /* 0.300150795 */, 19 },
  /* 7902 */  { MAD_F(0x04cda008) /* 0.300201448 */, 19 },
  /* 7903 */  { MAD_F(0x04cdd526) /* 0.300252103 */, 19 },

  /* 7904 */  { MAD_F(0x04ce0a44) /* 0.300302761 */, 19 },
  /* 7905 */  { MAD_F(0x04ce3f63) /* 0.300353420 */, 19 },
  /* 7906 */  { MAD_F(0x04ce7482) /* 0.300404082 */, 19 },
  /* 7907 */  { MAD_F(0x04cea9a2) /* 0.300454745 */, 19 },
  /* 7908 */  { MAD_F(0x04cedec3) /* 0.300505411 */, 19 },
  /* 7909 */  { MAD_F(0x04cf13e4) /* 0.300556079 */, 19 },
  /* 7910 */  { MAD_F(0x04cf4906) /* 0.300606749 */, 19 },
  /* 7911 */  { MAD_F(0x04cf7e28) /* 0.300657421 */, 19 },
  /* 7912 */  { MAD_F(0x04cfb34b) /* 0.300708096 */, 19 },
  /* 7913 */  { MAD_F(0x04cfe86e) /* 0.300758772 */, 19 },
  /* 7914 */  { MAD_F(0x04d01d92) /* 0.300809451 */, 19 },
  /* 7915 */  { MAD_F(0x04d052b6) /* 0.300860132 */, 19 },
  /* 7916 */  { MAD_F(0x04d087db) /* 0.300910815 */, 19 },
  /* 7917 */  { MAD_F(0x04d0bd01) /* 0.300961500 */, 19 },
  /* 7918 */  { MAD_F(0x04d0f227) /* 0.301012187 */, 19 },
  /* 7919 */  { MAD_F(0x04d1274e) /* 0.301062876 */, 19 },

  /* 7920 */  { MAD_F(0x04d15c76) /* 0.301113568 */, 19 },
  /* 7921 */  { MAD_F(0x04d1919e) /* 0.301164261 */, 19 },
  /* 7922 */  { MAD_F(0x04d1c6c6) /* 0.301214957 */, 19 },
  /* 7923 */  { MAD_F(0x04d1fbef) /* 0.301265655 */, 19 },
  /* 7924 */  { MAD_F(0x04d23119) /* 0.301316355 */, 19 },
  /* 7925 */  { MAD_F(0x04d26643) /* 0.301367057 */, 19 },
  /* 7926 */  { MAD_F(0x04d29b6e) /* 0.301417761 */, 19 },
  /* 7927 */  { MAD_F(0x04d2d099) /* 0.301468468 */, 19 },
  /* 7928 */  { MAD_F(0x04d305c5) /* 0.301519176 */, 19 },
  /* 7929 */  { MAD_F(0x04d33af2) /* 0.301569887 */, 19 },
  /* 7930 */  { MAD_F(0x04d3701f) /* 0.301620599 */, 19 },
  /* 7931 */  { MAD_F(0x04d3a54d) /* 0.301671314 */, 19 },
  /* 7932 */  { MAD_F(0x04d3da7b) /* 0.301722031 */, 19 },
  /* 7933 */  { MAD_F(0x04d40faa) /* 0.301772751 */, 19 },
  /* 7934 */  { MAD_F(0x04d444d9) /* 0.301823472 */, 19 },
  /* 7935 */  { MAD_F(0x04d47a09) /* 0.301874195 */, 19 },

  /* 7936 */  { MAD_F(0x04d4af3a) /* 0.301924921 */, 19 },
  /* 7937 */  { MAD_F(0x04d4e46b) /* 0.301975649 */, 19 },
  /* 7938 */  { MAD_F(0x04d5199c) /* 0.302026378 */, 19 },
  /* 7939 */  { MAD_F(0x04d54ecf) /* 0.302077110 */, 19 },
  /* 7940 */  { MAD_F(0x04d58401) /* 0.302127845 */, 19 },
  /* 7941 */  { MAD_F(0x04d5b935) /* 0.302178581 */, 19 },
  /* 7942 */  { MAD_F(0x04d5ee69) /* 0.302229319 */, 19 },
  /* 7943 */  { MAD_F(0x04d6239d) /* 0.302280060 */, 19 },
  /* 7944 */  { MAD_F(0x04d658d2) /* 0.302330802 */, 19 },
  /* 7945 */  { MAD_F(0x04d68e08) /* 0.302381547 */, 19 },
  /* 7946 */  { MAD_F(0x04d6c33e) /* 0.302432294 */, 19 },
  /* 7947 */  { MAD_F(0x04d6f875) /* 0.302483043 */, 19 },
  /* 7948 */  { MAD_F(0x04d72dad) /* 0.302533794 */, 19 },
  /* 7949 */  { MAD_F(0x04d762e5) /* 0.302584547 */, 19 },
  /* 7950 */  { MAD_F(0x04d7981d) /* 0.302635303 */, 19 },
  /* 7951 */  { MAD_F(0x04d7cd56) /* 0.302686060 */, 19 },

  /* 7952 */  { MAD_F(0x04d80290) /* 0.302736820 */, 19 },
  /* 7953 */  { MAD_F(0x04d837ca) /* 0.302787581 */, 19 },
  /* 7954 */  { MAD_F(0x04d86d05) /* 0.302838345 */, 19 },
  /* 7955 */  { MAD_F(0x04d8a240) /* 0.302889111 */, 19 },
  /* 7956 */  { MAD_F(0x04d8d77c) /* 0.302939879 */, 19 },
  /* 7957 */  { MAD_F(0x04d90cb9) /* 0.302990650 */, 19 },
  /* 7958 */  { MAD_F(0x04d941f6) /* 0.303041422 */, 19 },
  /* 7959 */  { MAD_F(0x04d97734) /* 0.303092197 */, 19 },
  /* 7960 */  { MAD_F(0x04d9ac72) /* 0.303142973 */, 19 },
  /* 7961 */  { MAD_F(0x04d9e1b1) /* 0.303193752 */, 19 },
  /* 7962 */  { MAD_F(0x04da16f0) /* 0.303244533 */, 19 },
  /* 7963 */  { MAD_F(0x04da4c30) /* 0.303295316 */, 19 },
  /* 7964 */  { MAD_F(0x04da8171) /* 0.303346101 */, 19 },
  /* 7965 */  { MAD_F(0x04dab6b2) /* 0.303396889 */, 19 },
  /* 7966 */  { MAD_F(0x04daebf4) /* 0.303447678 */, 19 },
  /* 7967 */  { MAD_F(0x04db2136) /* 0.303498469 */, 19 },

  /* 7968 */  { MAD_F(0x04db5679) /* 0.303549263 */, 19 },
  /* 7969 */  { MAD_F(0x04db8bbc) /* 0.303600059 */, 19 },
  /* 7970 */  { MAD_F(0x04dbc100) /* 0.303650857 */, 19 },
  /* 7971 */  { MAD_F(0x04dbf644) /* 0.303701657 */, 19 },
  /* 7972 */  { MAD_F(0x04dc2b8a) /* 0.303752459 */, 19 },
  /* 7973 */  { MAD_F(0x04dc60cf) /* 0.303803263 */, 19 },
  /* 7974 */  { MAD_F(0x04dc9616) /* 0.303854070 */, 19 },
  /* 7975 */  { MAD_F(0x04dccb5c) /* 0.303904878 */, 19 },
  /* 7976 */  { MAD_F(0x04dd00a4) /* 0.303955689 */, 19 },
  /* 7977 */  { MAD_F(0x04dd35ec) /* 0.304006502 */, 19 },
  /* 7978 */  { MAD_F(0x04dd6b34) /* 0.304057317 */, 19 },
  /* 7979 */  { MAD_F(0x04dda07d) /* 0.304108134 */, 19 },
  /* 7980 */  { MAD_F(0x04ddd5c7) /* 0.304158953 */, 19 },
  /* 7981 */  { MAD_F(0x04de0b11) /* 0.304209774 */, 19 },
  /* 7982 */  { MAD_F(0x04de405c) /* 0.304260597 */, 19 },
  /* 7983 */  { MAD_F(0x04de75a7) /* 0.304311423 */, 19 },

  /* 7984 */  { MAD_F(0x04deaaf3) /* 0.304362251 */, 19 },
  /* 7985 */  { MAD_F(0x04dee040) /* 0.304413080 */, 19 },
  /* 7986 */  { MAD_F(0x04df158d) /* 0.304463912 */, 19 },
  /* 7987 */  { MAD_F(0x04df4adb) /* 0.304514746 */, 19 },
  /* 7988 */  { MAD_F(0x04df8029) /* 0.304565582 */, 19 },
  /* 7989 */  { MAD_F(0x04dfb578) /* 0.304616421 */, 19 },
  /* 7990 */  { MAD_F(0x04dfeac7) /* 0.304667261 */, 19 },
  /* 7991 */  { MAD_F(0x04e02017) /* 0.304718103 */, 19 },
  /* 7992 */  { MAD_F(0x04e05567) /* 0.304768948 */, 19 },
  /* 7993 */  { MAD_F(0x04e08ab8) /* 0.304819795 */, 19 },
  /* 7994 */  { MAD_F(0x04e0c00a) /* 0.304870644 */, 19 },
  /* 7995 */  { MAD_F(0x04e0f55c) /* 0.304921495 */, 19 },
  /* 7996 */  { MAD_F(0x04e12aaf) /* 0.304972348 */, 19 },
  /* 7997 */  { MAD_F(0x04e16002) /* 0.305023203 */, 19 },
  /* 7998 */  { MAD_F(0x04e19556) /* 0.305074060 */, 19 },
  /* 7999 */  { MAD_F(0x04e1caab) /* 0.305124920 */, 19 },

  /* 8000 */  { MAD_F(0x04e20000) /* 0.305175781 */, 19 },
  /* 8001 */  { MAD_F(0x04e23555) /* 0.305226645 */, 19 },
  /* 8002 */  { MAD_F(0x04e26aac) /* 0.305277511 */, 19 },
  /* 8003 */  { MAD_F(0x04e2a002) /* 0.305328379 */, 19 },
  /* 8004 */  { MAD_F(0x04e2d55a) /* 0.305379249 */, 19 },
  /* 8005 */  { MAD_F(0x04e30ab2) /* 0.305430121 */, 19 },
  /* 8006 */  { MAD_F(0x04e3400a) /* 0.305480995 */, 19 },
  /* 8007 */  { MAD_F(0x04e37563) /* 0.305531872 */, 19 },
  /* 8008 */  { MAD_F(0x04e3aabd) /* 0.305582750 */, 19 },
  /* 8009 */  { MAD_F(0x04e3e017) /* 0.305633631 */, 19 },
  /* 8010 */  { MAD_F(0x04e41572) /* 0.305684513 */, 19 },
  /* 8011 */  { MAD_F(0x04e44acd) /* 0.305735398 */, 19 },
  /* 8012 */  { MAD_F(0x04e48029) /* 0.305786285 */, 19 },
  /* 8013 */  { MAD_F(0x04e4b585) /* 0.305837174 */, 19 },
  /* 8014 */  { MAD_F(0x04e4eae2) /* 0.305888066 */, 19 },
  /* 8015 */  { MAD_F(0x04e52040) /* 0.305938959 */, 19 },

  /* 8016 */  { MAD_F(0x04e5559e) /* 0.305989854 */, 19 },
  /* 8017 */  { MAD_F(0x04e58afd) /* 0.306040752 */, 19 },
  /* 8018 */  { MAD_F(0x04e5c05c) /* 0.306091652 */, 19 },
  /* 8019 */  { MAD_F(0x04e5f5bc) /* 0.306142554 */, 19 },
  /* 8020 */  { MAD_F(0x04e62b1c) /* 0.306193457 */, 19 },
  /* 8021 */  { MAD_F(0x04e6607d) /* 0.306244364 */, 19 },
  /* 8022 */  { MAD_F(0x04e695df) /* 0.306295272 */, 19 },
  /* 8023 */  { MAD_F(0x04e6cb41) /* 0.306346182 */, 19 },
  /* 8024 */  { MAD_F(0x04e700a3) /* 0.306397094 */, 19 },
  /* 8025 */  { MAD_F(0x04e73607) /* 0.306448009 */, 19 },
  /* 8026 */  { MAD_F(0x04e76b6b) /* 0.306498925 */, 19 },
  /* 8027 */  { MAD_F(0x04e7a0cf) /* 0.306549844 */, 19 },
  /* 8028 */  { MAD_F(0x04e7d634) /* 0.306600765 */, 19 },
  /* 8029 */  { MAD_F(0x04e80b99) /* 0.306651688 */, 19 },
  /* 8030 */  { MAD_F(0x04e84100) /* 0.306702613 */, 19 },
  /* 8031 */  { MAD_F(0x04e87666) /* 0.306753540 */, 19 },

  /* 8032 */  { MAD_F(0x04e8abcd) /* 0.306804470 */, 19 },
  /* 8033 */  { MAD_F(0x04e8e135) /* 0.306855401 */, 19 },
  /* 8034 */  { MAD_F(0x04e9169e) /* 0.306906334 */, 19 },
  /* 8035 */  { MAD_F(0x04e94c07) /* 0.306957270 */, 19 },
  /* 8036 */  { MAD_F(0x04e98170) /* 0.307008208 */, 19 },
  /* 8037 */  { MAD_F(0x04e9b6da) /* 0.307059148 */, 19 },
  /* 8038 */  { MAD_F(0x04e9ec45) /* 0.307110090 */, 19 },
  /* 8039 */  { MAD_F(0x04ea21b0) /* 0.307161034 */, 19 },
  /* 8040 */  { MAD_F(0x04ea571c) /* 0.307211980 */, 19 },
  /* 8041 */  { MAD_F(0x04ea8c88) /* 0.307262928 */, 19 },
  /* 8042 */  { MAD_F(0x04eac1f5) /* 0.307313879 */, 19 },
  /* 8043 */  { MAD_F(0x04eaf762) /* 0.307364831 */, 19 },
  /* 8044 */  { MAD_F(0x04eb2cd0) /* 0.307415786 */, 19 },
  /* 8045 */  { MAD_F(0x04eb623f) /* 0.307466743 */, 19 },
  /* 8046 */  { MAD_F(0x04eb97ae) /* 0.307517702 */, 19 },
  /* 8047 */  { MAD_F(0x04ebcd1e) /* 0.307568663 */, 19 },

  /* 8048 */  { MAD_F(0x04ec028e) /* 0.307619626 */, 19 },
  /* 8049 */  { MAD_F(0x04ec37ff) /* 0.307670591 */, 19 },
  /* 8050 */  { MAD_F(0x04ec6d71) /* 0.307721558 */, 19 },
  /* 8051 */  { MAD_F(0x04eca2e3) /* 0.307772528 */, 19 },
  /* 8052 */  { MAD_F(0x04ecd855) /* 0.307823499 */, 19 },
  /* 8053 */  { MAD_F(0x04ed0dc8) /* 0.307874473 */, 19 },
  /* 8054 */  { MAD_F(0x04ed433c) /* 0.307925449 */, 19 },
  /* 8055 */  { MAD_F(0x04ed78b0) /* 0.307976426 */, 19 },
  /* 8056 */  { MAD_F(0x04edae25) /* 0.308027406 */, 19 },
  /* 8057 */  { MAD_F(0x04ede39a) /* 0.308078389 */, 19 },
  /* 8058 */  { MAD_F(0x04ee1910) /* 0.308129373 */, 19 },
  /* 8059 */  { MAD_F(0x04ee4e87) /* 0.308180359 */, 19 },
  /* 8060 */  { MAD_F(0x04ee83fe) /* 0.308231347 */, 19 },
  /* 8061 */  { MAD_F(0x04eeb976) /* 0.308282338 */, 19 },
  /* 8062 */  { MAD_F(0x04eeeeee) /* 0.308333331 */, 19 },
  /* 8063 */  { MAD_F(0x04ef2467) /* 0.308384325 */, 19 },

  /* 8064 */  { MAD_F(0x04ef59e0) /* 0.308435322 */, 19 },
  /* 8065 */  { MAD_F(0x04ef8f5a) /* 0.308486321 */, 19 },
  /* 8066 */  { MAD_F(0x04efc4d5) /* 0.308537322 */, 19 },
  /* 8067 */  { MAD_F(0x04effa50) /* 0.308588325 */, 19 },
  /* 8068 */  { MAD_F(0x04f02fcb) /* 0.308639331 */, 19 },
  /* 8069 */  { MAD_F(0x04f06547) /* 0.308690338 */, 19 },
  /* 8070 */  { MAD_F(0x04f09ac4) /* 0.308741348 */, 19 },
  /* 8071 */  { MAD_F(0x04f0d041) /* 0.308792359 */, 19 },
  /* 8072 */  { MAD_F(0x04f105bf) /* 0.308843373 */, 19 },
  /* 8073 */  { MAD_F(0x04f13b3e) /* 0.308894389 */, 19 },
  /* 8074 */  { MAD_F(0x04f170bd) /* 0.308945407 */, 19 },
  /* 8075 */  { MAD_F(0x04f1a63c) /* 0.308996427 */, 19 },
  /* 8076 */  { MAD_F(0x04f1dbbd) /* 0.309047449 */, 19 },
  /* 8077 */  { MAD_F(0x04f2113d) /* 0.309098473 */, 19 },
  /* 8078 */  { MAD_F(0x04f246bf) /* 0.309149499 */, 19 },
  /* 8079 */  { MAD_F(0x04f27c40) /* 0.309200528 */, 19 },

  /* 8080 */  { MAD_F(0x04f2b1c3) /* 0.309251558 */, 19 },
  /* 8081 */  { MAD_F(0x04f2e746) /* 0.309302591 */, 19 },
  /* 8082 */  { MAD_F(0x04f31cc9) /* 0.309353626 */, 19 },
  /* 8083 */  { MAD_F(0x04f3524d) /* 0.309404663 */, 19 },
  /* 8084 */  { MAD_F(0x04f387d2) /* 0.309455702 */, 19 },
  /* 8085 */  { MAD_F(0x04f3bd57) /* 0.309506743 */, 19 },
  /* 8086 */  { MAD_F(0x04f3f2dd) /* 0.309557786 */, 19 },
  /* 8087 */  { MAD_F(0x04f42864) /* 0.309608831 */, 19 },
  /* 8088 */  { MAD_F(0x04f45dea) /* 0.309659879 */, 19 },
  /* 8089 */  { MAD_F(0x04f49372) /* 0.309710928 */, 19 },
  /* 8090 */  { MAD_F(0x04f4c8fa) /* 0.309761980 */, 19 },
  /* 8091 */  { MAD_F(0x04f4fe83) /* 0.309813033 */, 19 },
  /* 8092 */  { MAD_F(0x04f5340c) /* 0.309864089 */, 19 },
  /* 8093 */  { MAD_F(0x04f56996) /* 0.309915147 */, 19 },
  /* 8094 */  { MAD_F(0x04f59f20) /* 0.309966207 */, 19 },
  /* 8095 */  { MAD_F(0x04f5d4ab) /* 0.310017269 */, 19 },

  /* 8096 */  { MAD_F(0x04f60a36) /* 0.310068333 */, 19 },
  /* 8097 */  { MAD_F(0x04f63fc2) /* 0.310119400 */, 19 },
  /* 8098 */  { MAD_F(0x04f6754f) /* 0.310170468 */, 19 },
  /* 8099 */  { MAD_F(0x04f6aadc) /* 0.310221539 */, 19 },
  /* 8100 */  { MAD_F(0x04f6e06a) /* 0.310272611 */, 19 },
  /* 8101 */  { MAD_F(0x04f715f8) /* 0.310323686 */, 19 },
  /* 8102 */  { MAD_F(0x04f74b87) /* 0.310374763 */, 19 },
  /* 8103 */  { MAD_F(0x04f78116) /* 0.310425842 */, 19 },
  /* 8104 */  { MAD_F(0x04f7b6a6) /* 0.310476923 */, 19 },
  /* 8105 */  { MAD_F(0x04f7ec37) /* 0.310528006 */, 19 },
  /* 8106 */  { MAD_F(0x04f821c8) /* 0.310579091 */, 19 },
  /* 8107 */  { MAD_F(0x04f85759) /* 0.310630179 */, 19 },
  /* 8108 */  { MAD_F(0x04f88cec) /* 0.310681268 */, 19 },
  /* 8109 */  { MAD_F(0x04f8c27e) /* 0.310732360 */, 19 },
  /* 8110 */  { MAD_F(0x04f8f812) /* 0.310783453 */, 19 },
  /* 8111 */  { MAD_F(0x04f92da6) /* 0.310834549 */, 19 },

  /* 8112 */  { MAD_F(0x04f9633a) /* 0.310885647 */, 19 },
  /* 8113 */  { MAD_F(0x04f998cf) /* 0.310936747 */, 19 },
  /* 8114 */  { MAD_F(0x04f9ce65) /* 0.310987849 */, 19 },
  /* 8115 */  { MAD_F(0x04fa03fb) /* 0.311038953 */, 19 },
  /* 8116 */  { MAD_F(0x04fa3992) /* 0.311090059 */, 19 },
  /* 8117 */  { MAD_F(0x04fa6f29) /* 0.311141168 */, 19 },
  /* 8118 */  { MAD_F(0x04faa4c1) /* 0.311192278 */, 19 },
  /* 8119 */  { MAD_F(0x04fada59) /* 0.311243390 */, 19 },
  /* 8120 */  { MAD_F(0x04fb0ff2) /* 0.311294505 */, 19 },
  /* 8121 */  { MAD_F(0x04fb458c) /* 0.311345622 */, 19 },
  /* 8122 */  { MAD_F(0x04fb7b26) /* 0.311396741 */, 19 },
  /* 8123 */  { MAD_F(0x04fbb0c1) /* 0.311447862 */, 19 },
  /* 8124 */  { MAD_F(0x04fbe65c) /* 0.311498985 */, 19 },
  /* 8125 */  { MAD_F(0x04fc1bf8) /* 0.311550110 */, 19 },
  /* 8126 */  { MAD_F(0x04fc5194) /* 0.311601237 */, 19 },
  /* 8127 */  { MAD_F(0x04fc8731) /* 0.311652366 */, 19 },

  /* 8128 */  { MAD_F(0x04fcbcce) /* 0.311703498 */, 19 },
  /* 8129 */  { MAD_F(0x04fcf26c) /* 0.311754631 */, 19 },
  /* 8130 */  { MAD_F(0x04fd280b) /* 0.311805767 */, 19 },
  /* 8131 */  { MAD_F(0x04fd5daa) /* 0.311856905 */, 19 },
  /* 8132 */  { MAD_F(0x04fd934a) /* 0.311908044 */, 19 },
  /* 8133 */  { MAD_F(0x04fdc8ea) /* 0.311959186 */, 19 },
  /* 8134 */  { MAD_F(0x04fdfe8b) /* 0.312010330 */, 19 },
  /* 8135 */  { MAD_F(0x04fe342c) /* 0.312061476 */, 19 },
  /* 8136 */  { MAD_F(0x04fe69ce) /* 0.312112625 */, 19 },
  /* 8137 */  { MAD_F(0x04fe9f71) /* 0.312163775 */, 19 },
  /* 8138 */  { MAD_F(0x04fed514) /* 0.312214927 */, 19 },
  /* 8139 */  { MAD_F(0x04ff0ab8) /* 0.312266082 */, 19 },
  /* 8140 */  { MAD_F(0x04ff405c) /* 0.312317238 */, 19 },
  /* 8141 */  { MAD_F(0x04ff7601) /* 0.312368397 */, 19 },
  /* 8142 */  { MAD_F(0x04ffaba6) /* 0.312419558 */, 19 },
  /* 8143 */  { MAD_F(0x04ffe14c) /* 0.312470720 */, 19 },

  /* 8144 */  { MAD_F(0x050016f3) /* 0.312521885 */, 19 },
  /* 8145 */  { MAD_F(0x05004c9a) /* 0.312573052 */, 19 },
  /* 8146 */  { MAD_F(0x05008241) /* 0.312624222 */, 19 },
  /* 8147 */  { MAD_F(0x0500b7e9) /* 0.312675393 */, 19 },
  /* 8148 */  { MAD_F(0x0500ed92) /* 0.312726566 */, 19 },
  /* 8149 */  { MAD_F(0x0501233b) /* 0.312777742 */, 19 },
  /* 8150 */  { MAD_F(0x050158e5) /* 0.312828919 */, 19 },
  /* 8151 */  { MAD_F(0x05018e90) /* 0.312880099 */, 19 },
  /* 8152 */  { MAD_F(0x0501c43b) /* 0.312931280 */, 19 },
  /* 8153 */  { MAD_F(0x0501f9e6) /* 0.312982464 */, 19 },
  /* 8154 */  { MAD_F(0x05022f92) /* 0.313033650 */, 19 },
  /* 8155 */  { MAD_F(0x0502653f) /* 0.313084838 */, 19 },
  /* 8156 */  { MAD_F(0x05029aec) /* 0.313136028 */, 19 },
  /* 8157 */  { MAD_F(0x0502d09a) /* 0.313187220 */, 19 },
  /* 8158 */  { MAD_F(0x05030648) /* 0.313238414 */, 19 },
  /* 8159 */  { MAD_F(0x05033bf7) /* 0.313289611 */, 19 },

  /* 8160 */  { MAD_F(0x050371a7) /* 0.313340809 */, 19 },
  /* 8161 */  { MAD_F(0x0503a757) /* 0.313392010 */, 19 },
  /* 8162 */  { MAD_F(0x0503dd07) /* 0.313443212 */, 19 },
  /* 8163 */  { MAD_F(0x050412b9) /* 0.313494417 */, 19 },
  /* 8164 */  { MAD_F(0x0504486a) /* 0.313545624 */, 19 },
  /* 8165 */  { MAD_F(0x05047e1d) /* 0.313596833 */, 19 },
  /* 8166 */  { MAD_F(0x0504b3cf) /* 0.313648044 */, 19 },
  /* 8167 */  { MAD_F(0x0504e983) /* 0.313699257 */, 19 },
  /* 8168 */  { MAD_F(0x05051f37) /* 0.313750472 */, 19 },
  /* 8169 */  { MAD_F(0x050554eb) /* 0.313801689 */, 19 },
  /* 8170 */  { MAD_F(0x05058aa0) /* 0.313852909 */, 19 },
  /* 8171 */  { MAD_F(0x0505c056) /* 0.313904130 */, 19 },
  /* 8172 */  { MAD_F(0x0505f60c) /* 0.313955354 */, 19 },
  /* 8173 */  { MAD_F(0x05062bc3) /* 0.314006579 */, 19 },
  /* 8174 */  { MAD_F(0x0506617a) /* 0.314057807 */, 19 },
  /* 8175 */  { MAD_F(0x05069732) /* 0.314109037 */, 19 },

  /* 8176 */  { MAD_F(0x0506cceb) /* 0.314160269 */, 19 },
  /* 8177 */  { MAD_F(0x050702a4) /* 0.314211502 */, 19 },
  /* 8178 */  { MAD_F(0x0507385d) /* 0.314262739 */, 19 },
  /* 8179 */  { MAD_F(0x05076e17) /* 0.314313977 */, 19 },
  /* 8180 */  { MAD_F(0x0507a3d2) /* 0.314365217 */, 19 },
  /* 8181 */  { MAD_F(0x0507d98d) /* 0.314416459 */, 19 },
  /* 8182 */  { MAD_F(0x05080f49) /* 0.314467704 */, 19 },
  /* 8183 */  { MAD_F(0x05084506) /* 0.314518950 */, 19 },
  /* 8184 */  { MAD_F(0x05087ac2) /* 0.314570199 */, 19 },
  /* 8185 */  { MAD_F(0x0508b080) /* 0.314621449 */, 19 },
  /* 8186 */  { MAD_F(0x0508e63e) /* 0.314672702 */, 19 },
  /* 8187 */  { MAD_F(0x05091bfd) /* 0.314723957 */, 19 },
  /* 8188 */  { MAD_F(0x050951bc) /* 0.314775214 */, 19 },
  /* 8189 */  { MAD_F(0x0509877c) /* 0.314826473 */, 19 },
  /* 8190 */  { MAD_F(0x0509bd3c) /* 0.314877734 */, 19 },
  /* 8191 */  { MAD_F(0x0509f2fd) /* 0.314928997 */, 19 },

  /* 8192 */  { MAD_F(0x050a28be) /* 0.314980262 */, 19 },
  /* 8193 */  { MAD_F(0x050a5e80) /* 0.315031530 */, 19 },
  /* 8194 */  { MAD_F(0x050a9443) /* 0.315082799 */, 19 },
  /* 8195 */  { MAD_F(0x050aca06) /* 0.315134071 */, 19 },
  /* 8196 */  { MAD_F(0x050affc9) /* 0.315185344 */, 19 },
  /* 8197 */  { MAD_F(0x050b358e) /* 0.315236620 */, 19 },
  /* 8198 */  { MAD_F(0x050b6b52) /* 0.315287898 */, 19 },
  /* 8199 */  { MAD_F(0x050ba118) /* 0.315339178 */, 19 },
  /* 8200 */  { MAD_F(0x050bd6de) /* 0.315390460 */, 19 },
  /* 8201 */  { MAD_F(0x050c0ca4) /* 0.315441744 */, 19 },
  /* 8202 */  { MAD_F(0x050c426b) /* 0.315493030 */, 19 },
  /* 8203 */  { MAD_F(0x050c7833) /* 0.315544318 */, 19 },
  /* 8204 */  { MAD_F(0x050cadfb) /* 0.315595608 */, 19 },
  /* 8205 */  { MAD_F(0x050ce3c4) /* 0.315646901 */, 19 },
  /* 8206 */  { MAD_F(0x050d198d) /* 0.315698195 */, 19 }

};

/*
 * fractional powers of two
 * used for requantization and joint stereo decoding
 *
 * root_table[3 + x] = 2^(x/4)
 */
static
mad_fixed_t const root_table[7] = {
  MAD_F(0x09837f05) /* 2^(-3/4) == 0.59460355750136 */,
  MAD_F(0x0b504f33) /* 2^(-2/4) == 0.70710678118655 */,
  MAD_F(0x0d744fcd) /* 2^(-1/4) == 0.84089641525371 */,
  MAD_F(0x10000000) /* 2^( 0/4) == 1.00000000000000 */,
  MAD_F(0x1306fe0a) /* 2^(+1/4) == 1.18920711500272 */,
  MAD_F(0x16a09e66) /* 2^(+2/4) == 1.41421356237310 */,
  MAD_F(0x1ae89f99) /* 2^(+3/4) == 1.68179283050743 */
};

/*
 * coefficients for aliasing reduction
 * derived from Table B.9 of ISO/IEC 11172-3
 *
 *  c[]  = { -0.6, -0.535, -0.33, -0.185, -0.095, -0.041, -0.0142, -0.0037 }
 * cs[i] =    1 / sqrt(1 + c[i]^2)
 * ca[i] = c[i] / sqrt(1 + c[i]^2)
 */
static
mad_fixed_t const cs[8] = {
  +MAD_F(0x0db84a81) /* +0.857492926 */, +MAD_F(0x0e1b9d7f) /* +0.881741997 */,
  +MAD_F(0x0f31adcf) /* +0.949628649 */, +MAD_F(0x0fbba815) /* +0.983314592 */,
  +MAD_F(0x0feda417) /* +0.995517816 */, +MAD_F(0x0ffc8fc8) /* +0.999160558 */,
  +MAD_F(0x0fff964c) /* +0.999899195 */, +MAD_F(0x0ffff8d3) /* +0.999993155 */
};

static
mad_fixed_t const ca[8] = {
  -MAD_F(0x083b5fe7) /* -0.514495755 */, -MAD_F(0x078c36d2) /* -0.471731969 */,
  -MAD_F(0x05039814) /* -0.313377454 */, -MAD_F(0x02e91dd1) /* -0.181913200 */,
  -MAD_F(0x0183603a) /* -0.094574193 */, -MAD_F(0x00a7cb87) /* -0.040965583 */,
  -MAD_F(0x003a2847) /* -0.014198569 */, -MAD_F(0x000f27b4) /* -0.003699975 */
};

/*
 * IMDCT coefficients for short blocks
 * derived from section 2.4.3.4.10.2 of ISO/IEC 11172-3
 *
 * imdct_s[i/even][k] = cos((PI / 24) * (2 *       (i / 2) + 7) * (2 * k + 1))
 * imdct_s[i /odd][k] = cos((PI / 24) * (2 * (6 + (i-1)/2) + 7) * (2 * k + 1))
 */
static
mad_fixed_t const imdct_s[6][6] = {

  /*  0 */  {  MAD_F(0x09bd7ca0) /*  0.608761429 */,
	      -MAD_F(0x0ec835e8) /* -0.923879533 */,
	      -MAD_F(0x0216a2a2) /* -0.130526192 */,
	       MAD_F(0x0fdcf549) /*  0.991444861 */,
	      -MAD_F(0x061f78aa) /* -0.382683432 */,
	      -MAD_F(0x0cb19346) /* -0.793353340 */ },

  /*  6 */  { -MAD_F(0x0cb19346) /* -0.793353340 */,
	       MAD_F(0x061f78aa) /*  0.382683432 */,
	       MAD_F(0x0fdcf549) /*  0.991444861 */,
	       MAD_F(0x0216a2a2) /*  0.130526192 */,
	      -MAD_F(0x0ec835e8) /* -0.923879533 */,
	      -MAD_F(0x09bd7ca0) /* -0.608761429 */ },

  /*  1 */  {  MAD_F(0x061f78aa) /*  0.382683432 */,
	      -MAD_F(0x0ec835e8) /* -0.923879533 */,
	       MAD_F(0x0ec835e8) /*  0.923879533 */,
	      -MAD_F(0x061f78aa) /* -0.382683432 */,
	      -MAD_F(0x061f78aa) /* -0.382683432 */,
	       MAD_F(0x0ec835e8) /*  0.923879533 */ },

  /*  7 */  { -MAD_F(0x0ec835e8) /* -0.923879533 */,
	      -MAD_F(0x061f78aa) /* -0.382683432 */,
	       MAD_F(0x061f78aa) /*  0.382683432 */,
	       MAD_F(0x0ec835e8) /*  0.923879533 */,
	       MAD_F(0x0ec835e8) /*  0.923879533 */,
	       MAD_F(0x061f78aa) /*  0.382683432 */ },

  /*  2 */  {  MAD_F(0x0216a2a2) /*  0.130526192 */,
	      -MAD_F(0x061f78aa) /* -0.382683432 */,
	       MAD_F(0x09bd7ca0) /*  0.608761429 */,
	      -MAD_F(0x0cb19346) /* -0.793353340 */,
	       MAD_F(0x0ec835e8) /*  0.923879533 */,
	      -MAD_F(0x0fdcf549) /* -0.991444861 */ },

  /*  8 */  { -MAD_F(0x0fdcf549) /* -0.991444861 */,
	      -MAD_F(0x0ec835e8) /* -0.923879533 */,
	      -MAD_F(0x0cb19346) /* -0.793353340 */,
	      -MAD_F(0x09bd7ca0) /* -0.608761429 */,
	      -MAD_F(0x061f78aa) /* -0.382683432 */,
	      -MAD_F(0x0216a2a2) /* -0.130526192 */ }

};

# if !defined(ASO_IMDCT)
/*
 * windowing coefficients for long blocks
 * derived from section 2.4.3.4.10.3 of ISO/IEC 11172-3
 *
 * window_l[i] = sin((PI / 36) * (i + 1/2))
 */
static
mad_fixed_t const window_l[36] = {
  MAD_F(0x00b2aa3e) /* 0.043619387 */, MAD_F(0x0216a2a2) /* 0.130526192 */,
  MAD_F(0x03768962) /* 0.216439614 */, MAD_F(0x04cfb0e2) /* 0.300705800 */,
  MAD_F(0x061f78aa) /* 0.382683432 */, MAD_F(0x07635284) /* 0.461748613 */,
  MAD_F(0x0898c779) /* 0.537299608 */, MAD_F(0x09bd7ca0) /* 0.608761429 */,
  MAD_F(0x0acf37ad) /* 0.675590208 */, MAD_F(0x0bcbe352) /* 0.737277337 */,
  MAD_F(0x0cb19346) /* 0.793353340 */, MAD_F(0x0d7e8807) /* 0.843391446 */,

  MAD_F(0x0e313245) /* 0.887010833 */, MAD_F(0x0ec835e8) /* 0.923879533 */,
  MAD_F(0x0f426cb5) /* 0.953716951 */, MAD_F(0x0f9ee890) /* 0.976296007 */,
  MAD_F(0x0fdcf549) /* 0.991444861 */, MAD_F(0x0ffc19fd) /* 0.999048222 */,
  MAD_F(0x0ffc19fd) /* 0.999048222 */, MAD_F(0x0fdcf549) /* 0.991444861 */,
  MAD_F(0x0f9ee890) /* 0.976296007 */, MAD_F(0x0f426cb5) /* 0.953716951 */,
  MAD_F(0x0ec835e8) /* 0.923879533 */, MAD_F(0x0e313245) /* 0.887010833 */,

  MAD_F(0x0d7e8807) /* 0.843391446 */, MAD_F(0x0cb19346) /* 0.793353340 */,
  MAD_F(0x0bcbe352) /* 0.737277337 */, MAD_F(0x0acf37ad) /* 0.675590208 */,
  MAD_F(0x09bd7ca0) /* 0.608761429 */, MAD_F(0x0898c779) /* 0.537299608 */,
  MAD_F(0x07635284) /* 0.461748613 */, MAD_F(0x061f78aa) /* 0.382683432 */,
  MAD_F(0x04cfb0e2) /* 0.300705800 */, MAD_F(0x03768962) /* 0.216439614 */,
  MAD_F(0x0216a2a2) /* 0.130526192 */, MAD_F(0x00b2aa3e) /* 0.043619387 */,
};
# endif  /* ASO_IMDCT */

/*
 * windowing coefficients for short blocks
 * derived from section 2.4.3.4.10.3 of ISO/IEC 11172-3
 *
 * window_s[i] = sin((PI / 12) * (i + 1/2))
 */
static
mad_fixed_t const window_s[12] = {
  MAD_F(0x0216a2a2) /* 0.130526192 */, MAD_F(0x061f78aa) /* 0.382683432 */,
  MAD_F(0x09bd7ca0) /* 0.608761429 */, MAD_F(0x0cb19346) /* 0.793353340 */,
  MAD_F(0x0ec835e8) /* 0.923879533 */, MAD_F(0x0fdcf549) /* 0.991444861 */,
  MAD_F(0x0fdcf549) /* 0.991444861 */, MAD_F(0x0ec835e8) /* 0.923879533 */,
  MAD_F(0x0cb19346) /* 0.793353340 */, MAD_F(0x09bd7ca0) /* 0.608761429 */,
  MAD_F(0x061f78aa) /* 0.382683432 */, MAD_F(0x0216a2a2) /* 0.130526192 */,
};

/*
 * coefficients for intensity stereo processing
 * derived from section 2.4.3.4.9.3 of ISO/IEC 11172-3
 *
 * is_ratio[i] = tan(i * (PI / 12))
 * is_table[i] = is_ratio[i] / (1 + is_ratio[i])
 */
static
mad_fixed_t const is_table[7] = {
  MAD_F(0x00000000) /* 0.000000000 */,
  MAD_F(0x0361962f) /* 0.211324865 */,
  MAD_F(0x05db3d74) /* 0.366025404 */,
  MAD_F(0x08000000) /* 0.500000000 */,
  MAD_F(0x0a24c28c) /* 0.633974596 */,
  MAD_F(0x0c9e69d1) /* 0.788675135 */,
  MAD_F(0x10000000) /* 1.000000000 */
};

/*
 * coefficients for LSF intensity stereo processing
 * derived from section 2.4.3.2 of ISO/IEC 13818-3
 *
 * is_lsf_table[0][i] = (1 / sqrt(sqrt(2)))^(i + 1)
 * is_lsf_table[1][i] = (1 /      sqrt(2)) ^(i + 1)
 */
static
mad_fixed_t const is_lsf_table[2][15] = {
  {
    MAD_F(0x0d744fcd) /* 0.840896415 */,
    MAD_F(0x0b504f33) /* 0.707106781 */,
    MAD_F(0x09837f05) /* 0.594603558 */,
    MAD_F(0x08000000) /* 0.500000000 */,
    MAD_F(0x06ba27e6) /* 0.420448208 */,
    MAD_F(0x05a8279a) /* 0.353553391 */,
    MAD_F(0x04c1bf83) /* 0.297301779 */,
    MAD_F(0x04000000) /* 0.250000000 */,
    MAD_F(0x035d13f3) /* 0.210224104 */,
    MAD_F(0x02d413cd) /* 0.176776695 */,
    MAD_F(0x0260dfc1) /* 0.148650889 */,
    MAD_F(0x02000000) /* 0.125000000 */,
    MAD_F(0x01ae89fa) /* 0.105112052 */,
    MAD_F(0x016a09e6) /* 0.088388348 */,
    MAD_F(0x01306fe1) /* 0.074325445 */
  }, {
    MAD_F(0x0b504f33) /* 0.707106781 */,
    MAD_F(0x08000000) /* 0.500000000 */,
    MAD_F(0x05a8279a) /* 0.353553391 */,
    MAD_F(0x04000000) /* 0.250000000 */,
    MAD_F(0x02d413cd) /* 0.176776695 */,
    MAD_F(0x02000000) /* 0.125000000 */,
    MAD_F(0x016a09e6) /* 0.088388348 */,
    MAD_F(0x01000000) /* 0.062500000 */,
    MAD_F(0x00b504f3) /* 0.044194174 */,
    MAD_F(0x00800000) /* 0.031250000 */,
    MAD_F(0x005a827a) /* 0.022097087 */,
    MAD_F(0x00400000) /* 0.015625000 */,
    MAD_F(0x002d413d) /* 0.011048543 */,
    MAD_F(0x00200000) /* 0.007812500 */,
    MAD_F(0x0016a09e) /* 0.005524272 */
  }
};

/*
 * NAME:	III_sideinfo()
 * DESCRIPTION:	decode frame side information from a bitstream
 */
static
enum mad_error III_sideinfo(struct mad_bitptr *ptr, unsigned int nch,
			    int lsf, struct sideinfo *si,
			    unsigned int *data_bitlen,
			    unsigned int *priv_bitlen)
{
  unsigned int ngr, gr, ch, i;
  enum mad_error result = MAD_ERROR_NONE;

  *data_bitlen = 0;
  *priv_bitlen = lsf ? ((nch == 1) ? 1 : 2) : ((nch == 1) ? 5 : 3);

  si->main_data_begin = mad_bit_read(ptr, lsf ? 8 : 9);
  si->private_bits    = mad_bit_read(ptr, *priv_bitlen);

  ngr = 1;
  if (!lsf) {
    ngr = 2;

    for (ch = 0; ch < nch; ++ch)
      si->scfsi[ch] = mad_bit_read(ptr, 4);
  }

  for (gr = 0; gr < ngr; ++gr) {
    struct granule * granule = (struct granule *)  &si->gr[gr];

    for (ch = 0; ch < nch; ++ch) {
      struct channel *channel = &granule->ch[ch];

      channel->part2_3_length    = mad_bit_read(ptr, 12);
      channel->big_values        = mad_bit_read(ptr, 9);
      channel->global_gain       = mad_bit_read(ptr, 8);
      channel->scalefac_compress = mad_bit_read(ptr, lsf ? 9 : 4);

      *data_bitlen += channel->part2_3_length;

      if (channel->big_values > 288 && result == 0)
	result = MAD_ERROR_BADBIGVALUES;

      channel->flags = 0;

      /* window_switching_flag */
      if (mad_bit_read(ptr, 1)) {
	channel->block_type = mad_bit_read(ptr, 2);

	if (channel->block_type == 0 && result == 0)
	  result = MAD_ERROR_BADBLOCKTYPE;

	if (!lsf && channel->block_type == 2 && si->scfsi[ch] && result == 0)
	  result = MAD_ERROR_BADSCFSI;

	channel->region0_count = 7;
	channel->region1_count = 36;

	if (mad_bit_read(ptr, 1))
	  channel->flags |= mixed_block_flag;
	else if (channel->block_type == 2)
	  channel->region0_count = 8;

	for (i = 0; i < 2; ++i)
	  channel->table_select[i] = mad_bit_read(ptr, 5);

# if defined(DEBUG)
	channel->table_select[2] = 4;  /* not used */
# endif

	for (i = 0; i < 3; ++i)
	  channel->subblock_gain[i] = mad_bit_read(ptr, 3);
      }
      else {
	channel->block_type = 0;

	for (i = 0; i < 3; ++i)
	  channel->table_select[i] = mad_bit_read(ptr, 5);

	channel->region0_count = mad_bit_read(ptr, 4);
	channel->region1_count = mad_bit_read(ptr, 3);
      }

      /* [preflag,] scalefac_scale, count1table_select */
      channel->flags |= mad_bit_read(ptr, lsf ? 2 : 3);
    }
  }

  return result;
}

/*
 * NAME:	III_scalefactors_lsf()
 * DESCRIPTION:	decode channel scalefactors for LSF from a bitstream
 */
static
unsigned int III_scalefactors_lsf(struct mad_bitptr *ptr,
				  struct channel *channel,
				  struct channel *gr1ch, int mode_extension)
{
  struct mad_bitptr start;
  unsigned int scalefac_compress, index, slen[4], part, n, i;
  unsigned char const *nsfb;

  start = *ptr;

  scalefac_compress = channel->scalefac_compress;
  index = (channel->block_type == 2) ?
    ((channel->flags & mixed_block_flag) ? 2 : 1) : 0;

  if (!((mode_extension & I_STEREO) && gr1ch)) {
    if (scalefac_compress < 400) {
      slen[0] = (scalefac_compress >> 4) / 5;
      slen[1] = (scalefac_compress >> 4) % 5;
      slen[2] = (scalefac_compress % 16) >> 2;
      slen[3] =  scalefac_compress %  4;

      nsfb = nsfb_table[0][index];
    }
    else if (scalefac_compress < 500) {
      scalefac_compress -= 400;

      slen[0] = (scalefac_compress >> 2) / 5;
      slen[1] = (scalefac_compress >> 2) % 5;
      slen[2] =  scalefac_compress %  4;
      slen[3] = 0;

      nsfb = nsfb_table[1][index];
    }
    else {
      scalefac_compress -= 500;

      slen[0] = scalefac_compress / 3;
      slen[1] = scalefac_compress % 3;
      slen[2] = 0;
      slen[3] = 0;

      channel->flags |= preflag;

      nsfb = nsfb_table[2][index];
    }

    n = 0;
    for (part = 0; part < 4; ++part) {
      for (i = 0; i < nsfb[part]; ++i)
	channel->scalefac[n++] = mad_bit_read(ptr, slen[part]);
    }

    while (n < 39)
      channel->scalefac[n++] = 0;
  }
  else {  /* (mode_extension & I_STEREO) && gr1ch (i.e. ch == 1) */
    scalefac_compress >>= 1;

    if (scalefac_compress < 180) {
      slen[0] =  scalefac_compress / 36;
      slen[1] = (scalefac_compress % 36) / 6;
      slen[2] = (scalefac_compress % 36) % 6;
      slen[3] = 0;

      nsfb = nsfb_table[3][index];
    }
    else if (scalefac_compress < 244) {
      scalefac_compress -= 180;

      slen[0] = (scalefac_compress % 64) >> 4;
      slen[1] = (scalefac_compress % 16) >> 2;
      slen[2] =  scalefac_compress %  4;
      slen[3] = 0;

      nsfb = nsfb_table[4][index];
    }
    else {
      scalefac_compress -= 244;

      slen[0] = scalefac_compress / 3;
      slen[1] = scalefac_compress % 3;
      slen[2] = 0;
      slen[3] = 0;

      nsfb = nsfb_table[5][index];
    }

    n = 0;
    for (part = 0; part < 4; ++part) {
      unsigned int max, is_pos;

      max = (1 << slen[part]) - 1;

      for (i = 0; i < nsfb[part]; ++i) {
	is_pos = mad_bit_read(ptr, slen[part]);

	channel->scalefac[n] = is_pos;
	gr1ch->scalefac[n++] = (is_pos == max);
      }
    }

    while (n < 39) {
      channel->scalefac[n] = 0;
      gr1ch->scalefac[n++] = 0;  /* apparently not illegal */
    }
  }

  return mad_bit_length(&start, ptr);
}

/*
 * NAME:	III_scalefactors()
 * DESCRIPTION:	decode channel scalefactors of one granule from a bitstream
 */
static
unsigned int III_scalefactors(struct mad_bitptr *ptr, struct channel *channel,
			      struct channel const *gr0ch, unsigned int scfsi)
{
  struct mad_bitptr start;
  unsigned int slen1, slen2, sfbi;

  start = *ptr;

  slen1 = sflen_table[channel->scalefac_compress].slen1;
  slen2 = sflen_table[channel->scalefac_compress].slen2;

  if (channel->block_type == 2) {
    unsigned int nsfb;

    sfbi = 0;

    nsfb = (channel->flags & mixed_block_flag) ? 8 + 3 * 3 : 6 * 3;
    while (nsfb--)
      channel->scalefac[sfbi++] = mad_bit_read(ptr, slen1);

    nsfb = 6 * 3;
    while (nsfb--)
      channel->scalefac[sfbi++] = mad_bit_read(ptr, slen2);

    nsfb = 1 * 3;
    while (nsfb--)
      channel->scalefac[sfbi++] = 0;
  }
  else {  /* channel->block_type != 2 */
    if (scfsi & 0x8) {
      for (sfbi = 0; sfbi < 6; ++sfbi)
	channel->scalefac[sfbi] = gr0ch->scalefac[sfbi];
    }
    else {
      for (sfbi = 0; sfbi < 6; ++sfbi)
	channel->scalefac[sfbi] = mad_bit_read(ptr, slen1);
    }

    if (scfsi & 0x4) {
      for (sfbi = 6; sfbi < 11; ++sfbi)
	channel->scalefac[sfbi] = gr0ch->scalefac[sfbi];
    }
    else {
      for (sfbi = 6; sfbi < 11; ++sfbi)
	channel->scalefac[sfbi] = mad_bit_read(ptr, slen1);
    }

    if (scfsi & 0x2) {
      for (sfbi = 11; sfbi < 16; ++sfbi)
	channel->scalefac[sfbi] = gr0ch->scalefac[sfbi];
    }
    else {
      for (sfbi = 11; sfbi < 16; ++sfbi)
	channel->scalefac[sfbi] = mad_bit_read(ptr, slen2);
    }

    if (scfsi & 0x1) {
      for (sfbi = 16; sfbi < 21; ++sfbi)
	channel->scalefac[sfbi] = gr0ch->scalefac[sfbi];
    }
    else {
      for (sfbi = 16; sfbi < 21; ++sfbi)
	channel->scalefac[sfbi] = mad_bit_read(ptr, slen2);
    }

    channel->scalefac[21] = 0;
  }

  return mad_bit_length(&start, ptr);
}

/*
 * The Layer III formula for requantization and scaling is defined by
 * section 2.4.3.4.7.1 of ISO/IEC 11172-3, as follows:
 *
 *   long blocks:
 *   xr[i] = sign(is[i]) * abs(is[i])^(4/3) *
 *           2^((1/4) * (global_gain - 210)) *
 *           2^-(scalefac_multiplier *
 *               (scalefac_l[sfb] + preflag * pretab[sfb]))
 *
 *   short blocks:
 *   xr[i] = sign(is[i]) * abs(is[i])^(4/3) *
 *           2^((1/4) * (global_gain - 210 - 8 * subblock_gain[w])) *
 *           2^-(scalefac_multiplier * scalefac_s[sfb][w])
 *
 *   where:
 *   scalefac_multiplier = (scalefac_scale + 1) / 2
 *
 * The routines III_exponents() and III_requantize() facilitate this
 * calculation.
 */

/*
 * NAME:	III_exponents()
 * DESCRIPTION:	calculate scalefactor exponents
 */
static
void III_exponents(struct channel const *channel,
		   unsigned char const *sfbwidth, signed int exponents[39])
{
  signed int gain;
  unsigned int scalefac_multiplier, sfbi;

  gain = (signed int) channel->global_gain - 210;
  scalefac_multiplier = (channel->flags & scalefac_scale) ? 2 : 1;

  if (channel->block_type == 2) {
    unsigned int l;
    signed int gain0, gain1, gain2;

    sfbi = l = 0;

    if (channel->flags & mixed_block_flag) {
      unsigned int premask;

      premask = (channel->flags & preflag) ? ~0 : 0;

      /* long block subbands 0-1 */

      while (l < 36) {
	exponents[sfbi] = gain -
	  (signed int) ((channel->scalefac[sfbi] + (pretab[sfbi] & premask)) <<
			scalefac_multiplier);

	l += sfbwidth[sfbi++];
      }
    }

    /* this is probably wrong for 8000 Hz short/mixed blocks */

    gain0 = gain - 8 * (signed int) channel->subblock_gain[0];
    gain1 = gain - 8 * (signed int) channel->subblock_gain[1];
    gain2 = gain - 8 * (signed int) channel->subblock_gain[2];

    while (l < 576) {
      exponents[sfbi + 0] = gain0 -
	(signed int) (channel->scalefac[sfbi + 0] << scalefac_multiplier);
      exponents[sfbi + 1] = gain1 -
	(signed int) (channel->scalefac[sfbi + 1] << scalefac_multiplier);
      exponents[sfbi + 2] = gain2 -
	(signed int) (channel->scalefac[sfbi + 2] << scalefac_multiplier);

      l    += 3 * sfbwidth[sfbi];
      sfbi += 3;
    }
  }
  else {  /* channel->block_type != 2 */
    if (channel->flags & preflag) {
      for (sfbi = 0; sfbi < 22; ++sfbi) {
	exponents[sfbi] = gain -
	  (signed int) ((channel->scalefac[sfbi] + pretab[sfbi]) <<
			scalefac_multiplier);
      }
    }
    else {
      for (sfbi = 0; sfbi < 22; ++sfbi) {
	exponents[sfbi] = gain -
	  (signed int) (channel->scalefac[sfbi] << scalefac_multiplier);
      }
    }
  }
}

/*
 * NAME:	III_requantize()
 * DESCRIPTION:	requantize one (positive) value
 */
static
mad_fixed_t III_requantize(unsigned int value, signed int exp)
{
  mad_fixed_t requantized;
  signed int frac;
  struct fixedfloat const *power;

  frac = exp % 4;  /* assumes sign(frac) == sign(exp) */
  exp /= 4;

  power = &rq_table[value];
  requantized = power->mantissa;
  exp += power->exponent;

  if (exp < 0) {
    if (-exp >= sizeof(mad_fixed_t) * CHAR_BIT) {
      /* underflow */
      requantized = 0;
    }
    else {
      requantized += 1L << (-exp - 1);
      requantized >>= -exp;
    }
  }
  else {
    if (exp >= 5) {
      /* overflow */
# if defined(DEBUG)
      fprintf(stderr, "requantize overflow (%f * 2^%d)\n",
	      mad_f_todouble(requantized), exp);
# endif
      requantized = MAD_F_MAX;
    }
    else
      requantized <<= exp;
  }
  


  return frac ? mad_f_mul(requantized, root_table[3 + frac]) : requantized;
}

/* we must take care that sz >= bits and sz < sizeof(long) lest bits == 0 */
# define MASK(cache, sz, bits)	\
    (((cache) >> ((sz) - (bits))) & ((1 << (bits)) - 1))
# define MASK1BIT(cache, sz)  \
    ((cache) & (1 << ((sz) - 1)))

/*
 * NAME:	III_huffdecode()
 * DESCRIPTION:	decode Huffman code words of one channel of one granule
 */
static
enum mad_error III_huffdecode(struct mad_bitptr *ptr, mad_fixed_t xr[576],
			      struct channel *channel,
			      unsigned char const *sfbwidth,
			      unsigned int part2_length)
{
  signed int exponents[39], exp;
  signed int const *expptr;
  struct mad_bitptr peek;
  signed int bits_left, cachesz;
  register mad_fixed_t *xrptr;
  mad_fixed_t const *sfbound;
  register unsigned long bitcache;

  bits_left = (signed) channel->part2_3_length - (signed) part2_length;
  if (bits_left < 0)
    return MAD_ERROR_BADPART3LEN;

  III_exponents(channel, sfbwidth, exponents);

  peek = *ptr;
  mad_bit_skip(ptr, bits_left);

  /* align bit reads to byte boundaries */
  cachesz  = mad_bit_bitsleft(&peek);
  cachesz += ((32 - 1 - 24) + (24 - cachesz)) & ~7;

  bitcache   = mad_bit_read(&peek, cachesz);
  bits_left -= cachesz;

  xrptr = &xr[0];

  /* big_values */
  {
    unsigned int region, rcount;
    struct hufftable const *entry;
    union huffpair const *table;
    unsigned int linbits, startbits, big_values, reqhits;
    mad_fixed_t reqcache[16];

    sfbound = xrptr + *sfbwidth++;
    rcount  = channel->region0_count + 1;

    entry     = &mad_huff_pair_table[channel->table_select[region = 0]];
    table     = entry->table;
    linbits   = entry->linbits;
    startbits = entry->startbits;

    if (table == 0)
      return MAD_ERROR_BADHUFFTABLE;

    expptr  = &exponents[0];
    exp     = *expptr++;
    reqhits = 0;

    big_values = channel->big_values;

    while (big_values-- && cachesz + bits_left > 0) {
      union huffpair const *pair;
      unsigned int clumpsz, value;
      register mad_fixed_t requantized;

      if (xrptr == sfbound) {
	sfbound += *sfbwidth++;

	/* change table if region boundary */

	if (--rcount == 0) {
	  if (region == 0)
	    rcount = channel->region1_count + 1;
	  else
	    rcount = 0;  /* all remaining */

	  entry     = &mad_huff_pair_table[channel->table_select[++region]];
	  table     = entry->table;
	  linbits   = entry->linbits;
	  startbits = entry->startbits;

	  if (table == 0)
	    return MAD_ERROR_BADHUFFTABLE;
	}

	if (exp != *expptr) {
	  exp = *expptr;
	  reqhits = 0;
	}

	++expptr;
      }

      if (cachesz < 21) {
	unsigned int bits;

	bits       = ((32 - 1 - 21) + (21 - cachesz)) & ~7;
	bitcache   = (bitcache << bits) | mad_bit_read(&peek, bits);
	cachesz   += bits;
	bits_left -= bits;
      }

      /* hcod (0..19) */

      clumpsz = startbits;
      pair    = &table[MASK(bitcache, cachesz, clumpsz)];

      while (!pair->final) {
	cachesz -= clumpsz;

	clumpsz = pair->ptr.bits;
	pair    = &table[pair->ptr.offset + MASK(bitcache, cachesz, clumpsz)];
      }

      cachesz -= pair->value.hlen;

      if (linbits) {
	/* x (0..14) */

	value = pair->value.x;

	switch (value) {
	case 0:
	  xrptr[0] = 0;
	  break;

	case 15:
	  if (cachesz < linbits + 2) {
	    bitcache   = (bitcache << 16) | mad_bit_read(&peek, 16);
	    cachesz   += 16;
	    bits_left -= 16;
	  }

	  value += MASK(bitcache, cachesz, linbits);
	  cachesz -= linbits;

	  requantized = III_requantize(value, exp);
	  goto x_final;

	default:
	  if (reqhits & (1 << value))
	    requantized = reqcache[value];
	  else {
	    reqhits |= (1 << value);
	    requantized = reqcache[value] = III_requantize(value, exp);
	  }

	x_final:
	  xrptr[0] = MASK1BIT(bitcache, cachesz--) ?
	    -requantized : requantized;
	}

	/* y (0..14) */

	value = pair->value.y;

	switch (value) {
	case 0:
	  xrptr[1] = 0;
	  break;

	case 15:
	  if (cachesz < linbits + 1) {
	    bitcache   = (bitcache << 16) | mad_bit_read(&peek, 16);
	    cachesz   += 16;
	    bits_left -= 16;
	  }

	  value += MASK(bitcache, cachesz, linbits);
	  cachesz -= linbits;

	  requantized = III_requantize(value, exp);
	  goto y_final;

	default:
	  if (reqhits & (1 << value))
	    requantized = reqcache[value];
	  else {
	    reqhits |= (1 << value);
	    requantized = reqcache[value] = III_requantize(value, exp);
	  }

	y_final:
	  xrptr[1] = MASK1BIT(bitcache, cachesz--) ?
	    -requantized : requantized;
	}
      }
      else {
	/* x (0..1) */

	value = pair->value.x;

	if (value == 0)
	  xrptr[0] = 0;
	else {
	  if (reqhits & (1 << value))
	    requantized = reqcache[value];
	  else {
	    reqhits |= (1 << value);
	    requantized = reqcache[value] = III_requantize(value, exp);
	  }

	  xrptr[0] = MASK1BIT(bitcache, cachesz--) ?
	    -requantized : requantized;
	}

	/* y (0..1) */

	value = pair->value.y;

	if (value == 0)
	  xrptr[1] = 0;
	else {
	  if (reqhits & (1 << value))
	    requantized = reqcache[value];
	  else {
	    reqhits |= (1 << value);
	    requantized = reqcache[value] = III_requantize(value, exp);
	  }

	  xrptr[1] = MASK1BIT(bitcache, cachesz--) ?
	    -requantized : requantized;
	}
      }

      xrptr += 2;
    }
  }

  if (cachesz + bits_left < 0)
    return MAD_ERROR_BADHUFFDATA;  /* big_values overrun */

  /* count1 */
  {
    union huffquad const *table;
    register mad_fixed_t requantized;

    table = mad_huff_quad_table[channel->flags & count1table_select];

    requantized = III_requantize(1, exp);

    while (cachesz + bits_left > 0 && xrptr <= &xr[572]) {
      union huffquad const *quad;

      /* hcod (1..6) */

      if (cachesz < 10) {
	bitcache   = (bitcache << 16) | mad_bit_read(&peek, 16);
	cachesz   += 16;
	bits_left -= 16;
      }

      quad = &table[MASK(bitcache, cachesz, 4)];

      /* quad tables guaranteed to have at most one extra lookup */
      if (!quad->final) {
	cachesz -= 4;

	quad = &table[quad->ptr.offset +
		      MASK(bitcache, cachesz, quad->ptr.bits)];
      }

      cachesz -= quad->value.hlen;

      if (xrptr == sfbound) {
	sfbound += *sfbwidth++;

	if (exp != *expptr) {
	  exp = *expptr;
	  requantized = III_requantize(1, exp);
	}

	++expptr;
      }

      /* v (0..1) */

      xrptr[0] = quad->value.v ?
	(MASK1BIT(bitcache, cachesz--) ? -requantized : requantized) : 0;

      /* w (0..1) */

      xrptr[1] = quad->value.w ?
	(MASK1BIT(bitcache, cachesz--) ? -requantized : requantized) : 0;

      xrptr += 2;

      if (xrptr == sfbound) {
	sfbound += *sfbwidth++;

	if (exp != *expptr) {
	  exp = *expptr;
	  requantized = III_requantize(1, exp);
	}

	++expptr;
      }

      /* x (0..1) */

      xrptr[0] = quad->value.x ?
	(MASK1BIT(bitcache, cachesz--) ? -requantized : requantized) : 0;

      /* y (0..1) */

      xrptr[1] = quad->value.y ?
	(MASK1BIT(bitcache, cachesz--) ? -requantized : requantized) : 0;

      xrptr += 2;
    }

    if (cachesz + bits_left < 0) {
# if 0 && defined(DEBUG)
      fprintf(stderr, "huffman count1 overrun (%d bits)\n",
	      -(cachesz + bits_left));
# endif

      /* technically the bitstream is misformatted, but apparently
	 some encoders are just a bit sloppy with stuffing bits */

      xrptr -= 4;
    }
  }

/*  assert(-bits_left <= MAD_BUFFER_GUARD * CHAR_BIT);*/

# if 0 && defined(DEBUG)
  if (bits_left < 0)
    fprintf(stderr, "read %d bits too many\n", -bits_left);
  else if (cachesz + bits_left > 0)
    fprintf(stderr, "%d stuffing bits\n", cachesz + bits_left);
# endif

  /* rzero */
  while (xrptr < &xr[576]) {
    xrptr[0] = 0;
    xrptr[1] = 0;

    xrptr += 2;
  }

  return MAD_ERROR_NONE;
}

# undef MASK
# undef MASK1BIT

/*
 * NAME:	III_reorder()
 * DESCRIPTION:	reorder frequency lines of a short block into subband order
 */
static
void III_reorder(mad_fixed_t xr[576], struct channel const *channel,
		 unsigned char  sfbwidth[39])
{
  mad_fixed_t tmp[32][3][6];
  unsigned int sb, l, f, w, sbw[3], sw[3];

  /* this is probably wrong for 8000 Hz mixed blocks */

  sb = 0;
  if (channel->flags & mixed_block_flag) {
    sb = 2;

    l = 0;
    while (l < 36)
      l += *sfbwidth++;
  }

  for (w = 0; w < 3; ++w) {
    sbw[w] = sb;
    sw[w]  = 0;
  }

  f = *sfbwidth++;
  w = 0;

  for (l = 18 * sb; l < 576; ++l) {
    if (f-- == 0) {
      f = *sfbwidth++ - 1;
      w = (w + 1) % 3;
    }

    tmp[sbw[w]][w][sw[w]++] = xr[l];

    if (sw[w] == 6) {
      sw[w] = 0;
      ++sbw[w];
    }
  }

  CopyMemory(&xr[18 * sb], &tmp[sb], (576 - 18 * sb) * sizeof(mad_fixed_t));
}

/*
 * NAME:	III_stereo()
 * DESCRIPTION:	perform joint stereo processing on a granule
 */
static
enum mad_error III_stereo(mad_fixed_t xr[2][576],
			  struct granule const *granule,
			  struct mad_header *header,
			  unsigned char const *sfbwidth)
{
  short modes[39];
  unsigned int sfbi, l, n, i;

  if (granule->ch[0].block_type !=
      granule->ch[1].block_type ||
      (granule->ch[0].flags & mixed_block_flag) !=
      (granule->ch[1].flags & mixed_block_flag))
    return MAD_ERROR_BADSTEREO;

  for (i = 0; i < 39; ++i)
    modes[i] = header->mode_extension;

  /* intensity stereo */

  if (header->mode_extension & I_STEREO) {
    struct channel const *right_ch = &granule->ch[1];
    mad_fixed_t const *right_xr = xr[1];
    unsigned int is_pos;

    header->flags |= MAD_FLAG_I_STEREO;

    /* first determine which scalefactor bands are to be processed */

    if (right_ch->block_type == 2) {
      unsigned int lower, start, max, bound[3], w;

      lower = start = max = bound[0] = bound[1] = bound[2] = 0;

      sfbi = l = 0;

      if (right_ch->flags & mixed_block_flag) {
	while (l < 36) {
	  n = sfbwidth[sfbi++];

	  for (i = 0; i < n; ++i) {
	    if (right_xr[i]) {
	      lower = sfbi;
	      break;
	    }
	  }

	  right_xr += n;
	  l += n;
	}

	start = sfbi;
      }

      w = 0;
      while (l < 576) {
	n = sfbwidth[sfbi++];

	for (i = 0; i < n; ++i) {
	  if (right_xr[i]) {
	    max = bound[w] = sfbi;
	    break;
	  }
	}

	right_xr += n;
	l += n;
	w = (w + 1) % 3;
      }

      if (max)
	lower = start;

      /* long blocks */

      for (i = 0; i < lower; ++i)
	modes[i] = header->mode_extension & ~I_STEREO;

      /* short blocks */

      w = 0;
      for (i = start; i < max; ++i) {
	if (i < bound[w])
	  modes[i] = header->mode_extension & ~I_STEREO;

	w = (w + 1) % 3;
      }
    }
    else {  /* right_ch->block_type != 2 */
      unsigned int bound;

      bound = 0;
      for (sfbi = l = 0; l < 576; l += n) {
	n = sfbwidth[sfbi++];

	for (i = 0; i < n; ++i) {
	  if (right_xr[i]) {
	    bound = sfbi;
	    break;
	  }
	}

	right_xr += n;
      }

      for (i = 0; i < bound; ++i)
	modes[i] = header->mode_extension & ~I_STEREO;
    }

    /* now do the actual processing */

    if (header->flags & MAD_FLAG_LSF_EXT) {
      unsigned char const *illegal_pos = granule[1].ch[1].scalefac;
      mad_fixed_t const *lsf_scale;

      /* intensity_scale */
      lsf_scale = is_lsf_table[right_ch->scalefac_compress & 0x1];

      for (sfbi = l = 0; l < 576; ++sfbi, l += n) {
	n = sfbwidth[sfbi];

	if (!(modes[sfbi] & I_STEREO))
	  continue;

	if (illegal_pos[sfbi]) {
	  modes[sfbi] &= ~I_STEREO;
	  continue;
	}

	is_pos = right_ch->scalefac[sfbi];

	for (i = 0; i < n; ++i) {
	  register mad_fixed_t left;

	  left = xr[0][l + i];

	  if (is_pos == 0)
	    xr[1][l + i] = left;
	  else {
	    register mad_fixed_t opposite;

	    opposite = mad_f_mul(left, lsf_scale[(is_pos - 1) / 2]);

	    if (is_pos & 1) {
	      xr[0][l + i] = opposite;
	      xr[1][l + i] = left;
	    }
	    else
	      xr[1][l + i] = opposite;
	  }
	}
      }
    }
    else {  /* !(header->flags & MAD_FLAG_LSF_EXT) */
      for (sfbi = l = 0; l < 576; ++sfbi, l += n) {
	n = sfbwidth[sfbi];

	if (!(modes[sfbi] & I_STEREO))
	  continue;

	is_pos = right_ch->scalefac[sfbi];

	if (is_pos >= 7) {  /* illegal intensity position */
	  modes[sfbi] &= ~I_STEREO;
	  continue;
	}

	for (i = 0; i < n; ++i) {
	  register mad_fixed_t left;

	  left = xr[0][l + i];

	  xr[0][l + i] = mad_f_mul(left, is_table[    is_pos]);
	  xr[1][l + i] = mad_f_mul(left, is_table[6 - is_pos]);
	}
      }
    }
  }

  /* middle/side stereo */

  if (header->mode_extension & MS_STEREO) {
    register mad_fixed_t invsqrt2;

    header->flags |= MAD_FLAG_MS_STEREO;

    invsqrt2 = root_table[3 + -2];

    for (sfbi = l = 0; l < 576; ++sfbi, l += n) {
      n = sfbwidth[sfbi];

      if (modes[sfbi] != MS_STEREO)
	continue;

      for (i = 0; i < n; ++i) {
	register mad_fixed_t m, s;

	m = xr[0][l + i];
	s = xr[1][l + i];

	xr[0][l + i] = mad_f_mul(m + s, invsqrt2);  /* l = (m + s) / sqrt(2) */
	xr[1][l + i] = mad_f_mul(m - s, invsqrt2);  /* r = (m - s) / sqrt(2) */
      }
    }
  }

  return MAD_ERROR_NONE;
}

/*
 * NAME:	III_aliasreduce()
 * DESCRIPTION:	perform frequency line alias reduction
 */
static
void III_aliasreduce(mad_fixed_t xr[576], int lines)
{
  mad_fixed_t const *bound;
  int i;

  bound = &xr[lines];
  for (xr += 18; xr < bound; xr += 18) {
    for (i = 0; i < 8; ++i) {
      register mad_fixed_t a, b;
      register mad_fixed64hi_t hi;
      register mad_fixed64lo_t lo;

      a = xr[-1 - i];
      b = xr[     i];

# if defined(ASO_ZEROCHECK)
      if (a | b) {
# endif
	MAD_F_ML0(hi, lo,  a, cs[i]);
	MAD_F_MLA(hi, lo, -b, ca[i]);

	xr[-1 - i] = MAD_F_MLZ(hi, lo);

	MAD_F_ML0(hi, lo,  b, cs[i]);
	MAD_F_MLA(hi, lo,  a, ca[i]);

	xr[     i] = MAD_F_MLZ(hi, lo);
# if defined(ASO_ZEROCHECK)
      }
# endif
    }
  }
}

# if defined(ASO_IMDCT)
void III_imdct_l(mad_fixed_t const [18], mad_fixed_t [36], unsigned int);
# else
#  if 1
static
void fastsdct(mad_fixed_t const x[9], mad_fixed_t y[18])
{
  mad_fixed_t a0,  a1,  a2,  a3,  a4,  a5,  a6,  a7,  a8,  a9,  a10, a11, a12;
  mad_fixed_t a13, a14, a15, a16, a17, a18, a19, a20, a21, a22, a23, a24, a25;
  mad_fixed_t m0,  m1,  m2,  m3,  m4,  m5,  m6,  m7;

  enum {
    c0 =  MAD_F(0x1f838b8d),  /* 2 * cos( 1 * PI / 18) */
    c1 =  MAD_F(0x1bb67ae8),  /* 2 * cos( 3 * PI / 18) */
    c2 =  MAD_F(0x18836fa3),  /* 2 * cos( 4 * PI / 18) */
    c3 =  MAD_F(0x1491b752),  /* 2 * cos( 5 * PI / 18) */
    c4 =  MAD_F(0x0af1d43a),  /* 2 * cos( 7 * PI / 18) */
    c5 =  MAD_F(0x058e86a0),  /* 2 * cos( 8 * PI / 18) */
    c6 = -MAD_F(0x1e11f642)   /* 2 * cos(16 * PI / 18) */
  };

  a0 = x[3] + x[5];
  a1 = x[3] - x[5];
  a2 = x[6] + x[2];
  a3 = x[6] - x[2];
  a4 = x[1] + x[7];
  a5 = x[1] - x[7];
  a6 = x[8] + x[0];
  a7 = x[8] - x[0];

  a8  = a0  + a2;
  a9  = a0  - a2;
  a10 = a0  - a6;
  a11 = a2  - a6;
  a12 = a8  + a6;
  a13 = a1  - a3;
  a14 = a13 + a7;
  a15 = a3  + a7;
  a16 = a1  - a7;
  a17 = a1  + a3;

  m0 = mad_f_mul(a17, -c3);
  m1 = mad_f_mul(a16, -c0);
  m2 = mad_f_mul(a15, -c4);
  m3 = mad_f_mul(a14, -c1);
  m4 = mad_f_mul(a5,  -c1);
  m5 = mad_f_mul(a11, -c6);
  m6 = mad_f_mul(a10, -c5);
  m7 = mad_f_mul(a9,  -c2);

  a18 =     x[4] + a4;
  a19 = 2 * x[4] - a4;
  a20 = a19 + m5;
  a21 = a19 - m5;
  a22 = a19 + m6;
  a23 = m4  + m2;
  a24 = m4  - m2;
  a25 = m4  + m1;

  /* output to every other slot for convenience */

  y[ 0] = a18 + a12;
  y[ 2] = m0  - a25;
  y[ 4] = m7  - a20;
  y[ 6] = m3;
  y[ 8] = a21 - m6;
  y[10] = a24 - m1;
  y[12] = a12 - 2 * a18;
  y[14] = a23 + m0;
  y[16] = a22 + m7;
}


// promjena

//# if defined(_MSC_VER)
//extern  /* needed to satisfy bizarre MSVC++ interaction with inline */
//# endif

static inline void sdctII(mad_fixed_t const x[18], mad_fixed_t X[18])
{
  mad_fixed_t tmp[9];
  int i;

  /* scale[i] = 2 * cos(PI * (2 * i + 1) / (2 * 18)) */
  static mad_fixed_t const scale[9] = {
    MAD_F(0x1fe0d3b4), MAD_F(0x1ee8dd47), MAD_F(0x1d007930),
    MAD_F(0x1a367e59), MAD_F(0x16a09e66), MAD_F(0x125abcf8),
    MAD_F(0x0d8616bc), MAD_F(0x08483ee1), MAD_F(0x02c9fad7)
  };

  /* divide the 18-point SDCT-II into two 9-point SDCT-IIs */

  /* even input butterfly */

  for (i = 0; i < 9; i += 3) {
    tmp[i + 0] = x[i + 0] + x[18 - (i + 0) - 1];
    tmp[i + 1] = x[i + 1] + x[18 - (i + 1) - 1];
    tmp[i + 2] = x[i + 2] + x[18 - (i + 2) - 1];
  }

  fastsdct(tmp, &X[0]);

  /* odd input butterfly and scaling */

  for (i = 0; i < 9; i += 3) {
    tmp[i + 0] = mad_f_mul(x[i + 0] - x[18 - (i + 0) - 1], scale[i + 0]);
    tmp[i + 1] = mad_f_mul(x[i + 1] - x[18 - (i + 1) - 1], scale[i + 1]);
    tmp[i + 2] = mad_f_mul(x[i + 2] - x[18 - (i + 2) - 1], scale[i + 2]);
  }

  fastsdct(tmp, &X[1]);

  /* output accumulation */

  for (i = 3; i < 18; i += 8) {
    X[i + 0] -= X[(i + 0) - 2];
    X[i + 2] -= X[(i + 2) - 2];
    X[i + 4] -= X[(i + 4) - 2];
    X[i + 6] -= X[(i + 6) - 2];
  }
}

static inline
void dctIV(mad_fixed_t const y[18], mad_fixed_t X[18])
{
  mad_fixed_t tmp[18];
  int i;

  /* scale[i] = 2 * cos(PI * (2 * i + 1) / (4 * 18)) */
  static mad_fixed_t const scale[18] = {
    MAD_F(0x1ff833fa), MAD_F(0x1fb9ea93), MAD_F(0x1f3dd120),
    MAD_F(0x1e84d969), MAD_F(0x1d906bcf), MAD_F(0x1c62648b),
    MAD_F(0x1afd100f), MAD_F(0x1963268b), MAD_F(0x1797c6a4),
    MAD_F(0x159e6f5b), MAD_F(0x137af940), MAD_F(0x11318ef3),
    MAD_F(0x0ec6a507), MAD_F(0x0c3ef153), MAD_F(0x099f61c5),
    MAD_F(0x06ed12c5), MAD_F(0x042d4544), MAD_F(0x0165547c)
  };

  /* scaling */

  for (i = 0; i < 18; i += 3) {
    tmp[i + 0] = mad_f_mul(y[i + 0], scale[i + 0]);
    tmp[i + 1] = mad_f_mul(y[i + 1], scale[i + 1]);
    tmp[i + 2] = mad_f_mul(y[i + 2], scale[i + 2]);
  }

  /* SDCT-II */

  sdctII(tmp, X);

  /* scale reduction and output accumulation */

  X[0] /= 2;
  for (i = 1; i < 17; i += 4) {
    X[i + 0] = X[i + 0] / 2 - X[(i + 0) - 1];
    X[i + 1] = X[i + 1] / 2 - X[(i + 1) - 1];
    X[i + 2] = X[i + 2] / 2 - X[(i + 2) - 1];
    X[i + 3] = X[i + 3] / 2 - X[(i + 3) - 1];
  }
  X[17] = X[17] / 2 - X[16];
}

/*
 * NAME:	imdct36
 * DESCRIPTION:	perform X[18]->x[36] IMDCT using Szu-Wei Lee's fast algorithm
 */
static inline
void imdct36(mad_fixed_t const x[18], mad_fixed_t y[36])
{
  mad_fixed_t tmp[18];
  int i;

  /* DCT-IV */

  dctIV(x, tmp);

  /* convert 18-point DCT-IV to 36-point IMDCT */

  for (i =  0; i <  9; i += 3) {
    y[i + 0] =  tmp[9 + (i + 0)];
    y[i + 1] =  tmp[9 + (i + 1)];
    y[i + 2] =  tmp[9 + (i + 2)];
  }
  for (i =  9; i < 27; i += 3) {
    y[i + 0] = -tmp[36 - (9 + (i + 0)) - 1];
    y[i + 1] = -tmp[36 - (9 + (i + 1)) - 1];
    y[i + 2] = -tmp[36 - (9 + (i + 2)) - 1];
  }
  for (i = 27; i < 36; i += 3) {
    y[i + 0] = -tmp[(i + 0) - 27];
    y[i + 1] = -tmp[(i + 1) - 27];
    y[i + 2] = -tmp[(i + 2) - 27];
  }
}
#  else
/*
 * NAME:	imdct36
 * DESCRIPTION:	perform X[18]->x[36] IMDCT
 */
static inline
void imdct36(mad_fixed_t const X[18], mad_fixed_t x[36])
{
  mad_fixed_t t0, t1, t2,  t3,  t4,  t5,  t6,  t7;
  mad_fixed_t t8, t9, t10, t11, t12, t13, t14, t15;
  register mad_fixed64hi_t hi;
  register mad_fixed64lo_t lo;

  MAD_F_ML0(hi, lo, X[4],  MAD_F(0x0ec835e8));
  MAD_F_MLA(hi, lo, X[13], MAD_F(0x061f78aa));

  t6 = MAD_F_MLZ(hi, lo);

  MAD_F_MLA(hi, lo, (t14 = X[1] - X[10]), -MAD_F(0x061f78aa));
  MAD_F_MLA(hi, lo, (t15 = X[7] + X[16]), -MAD_F(0x0ec835e8));

  t0 = MAD_F_MLZ(hi, lo);

  MAD_F_MLA(hi, lo, (t8  = X[0] - X[11] - X[12]),  MAD_F(0x0216a2a2));
  MAD_F_MLA(hi, lo, (t9  = X[2] - X[9]  - X[14]),  MAD_F(0x09bd7ca0));
  MAD_F_MLA(hi, lo, (t10 = X[3] - X[8]  - X[15]), -MAD_F(0x0cb19346));
  MAD_F_MLA(hi, lo, (t11 = X[5] - X[6]  - X[17]), -MAD_F(0x0fdcf549));

  x[7]  = MAD_F_MLZ(hi, lo);
  x[10] = -x[7];

  MAD_F_ML0(hi, lo, t8,  -MAD_F(0x0cb19346));
  MAD_F_MLA(hi, lo, t9,   MAD_F(0x0fdcf549));
  MAD_F_MLA(hi, lo, t10,  MAD_F(0x0216a2a2));
  MAD_F_MLA(hi, lo, t11, -MAD_F(0x09bd7ca0));

  x[19] = x[34] = MAD_F_MLZ(hi, lo) - t0;

  t12 = X[0] - X[3] + X[8] - X[11] - X[12] + X[15];
  t13 = X[2] + X[5] - X[6] - X[9]  - X[14] - X[17];

  MAD_F_ML0(hi, lo, t12, -MAD_F(0x0ec835e8));
  MAD_F_MLA(hi, lo, t13,  MAD_F(0x061f78aa));

  x[22] = x[31] = MAD_F_MLZ(hi, lo) + t0;

  MAD_F_ML0(hi, lo, X[1],  -MAD_F(0x09bd7ca0));
  MAD_F_MLA(hi, lo, X[7],   MAD_F(0x0216a2a2));
  MAD_F_MLA(hi, lo, X[10], -MAD_F(0x0fdcf549));
  MAD_F_MLA(hi, lo, X[16],  MAD_F(0x0cb19346));

  t1 = MAD_F_MLZ(hi, lo) + t6;

  MAD_F_ML0(hi, lo, X[0],   MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[2],   MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[3],  -MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[5],  -MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[6],   MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[8],  -MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[9],   MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[11],  MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[14],  MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[15], -MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0f9ee890));

  x[6]  = MAD_F_MLZ(hi, lo) + t1;
  x[11] = -x[6];

  MAD_F_ML0(hi, lo, X[0],  -MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[2],  -MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[3],   MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[5],   MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[6],   MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[8],  -MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[9],  -MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[12], -MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[14],  MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[15],  MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[17],  MAD_F(0x04cfb0e2));

  x[23] = x[30] = MAD_F_MLZ(hi, lo) + t1;

  MAD_F_ML0(hi, lo, X[0],  -MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[2],   MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[3],  -MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[5],   MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[6],   MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[8],  -MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[9],  -MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[11],  MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[14],  MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[15],  MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0acf37ad));

  x[18] = x[35] = MAD_F_MLZ(hi, lo) - t1;

  MAD_F_ML0(hi, lo, X[4],   MAD_F(0x061f78aa));
  MAD_F_MLA(hi, lo, X[13], -MAD_F(0x0ec835e8));

  t7 = MAD_F_MLZ(hi, lo);

  MAD_F_MLA(hi, lo, X[1],  -MAD_F(0x0cb19346));
  MAD_F_MLA(hi, lo, X[7],   MAD_F(0x0fdcf549));
  MAD_F_MLA(hi, lo, X[10],  MAD_F(0x0216a2a2));
  MAD_F_MLA(hi, lo, X[16], -MAD_F(0x09bd7ca0));

  t2 = MAD_F_MLZ(hi, lo);

  MAD_F_MLA(hi, lo, X[0],   MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[2],   MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[3],  -MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[5],   MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[6],  -MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[8],  -MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[9],   MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[12],  MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[14],  MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[15],  MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[17],  MAD_F(0x0f426cb5));

  x[5]  = MAD_F_MLZ(hi, lo);
  x[12] = -x[5];

  MAD_F_ML0(hi, lo, X[0],   MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[2],  -MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[3],   MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[5],  -MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[6],  -MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[8],   MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[9],  -MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[11],  MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[12], -MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[14],  MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[15],  MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0bcbe352));

  x[0]  = MAD_F_MLZ(hi, lo) + t2;
  x[17] = -x[0];

  MAD_F_ML0(hi, lo, X[0],  -MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[2],  -MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[3],  -MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[5],   MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[6],   MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[8],   MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[9],   MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[11], -MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[14], -MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[15], -MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[17], -MAD_F(0x03768962));

  x[24] = x[29] = MAD_F_MLZ(hi, lo) + t2;

  MAD_F_ML0(hi, lo, X[1],  -MAD_F(0x0216a2a2));
  MAD_F_MLA(hi, lo, X[7],  -MAD_F(0x09bd7ca0));
  MAD_F_MLA(hi, lo, X[10],  MAD_F(0x0cb19346));
  MAD_F_MLA(hi, lo, X[16],  MAD_F(0x0fdcf549));

  t3 = MAD_F_MLZ(hi, lo) + t7;

  MAD_F_ML0(hi, lo, X[0],   MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[2],   MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[3],  -MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[5],  -MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[6],   MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[8],   MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[9],  -MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[12],  MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[14],  MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[15], -MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0ffc19fd));

  x[8] = MAD_F_MLZ(hi, lo) + t3;
  x[9] = -x[8];

  MAD_F_ML0(hi, lo, X[0],  -MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[2],   MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[3],   MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[5],  -MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[6],  -MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[8],   MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[9],   MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[11], -MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[14], -MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[15],  MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[17],  MAD_F(0x07635284));

  x[21] = x[32] = MAD_F_MLZ(hi, lo) + t3;

  MAD_F_ML0(hi, lo, X[0],  -MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[2],   MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[3],   MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[5],  -MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[6],  -MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[8],   MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[9],   MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[12],  MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[14],  MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[15], -MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0898c779));

  x[20] = x[33] = MAD_F_MLZ(hi, lo) - t3;

  MAD_F_ML0(hi, lo, t14, -MAD_F(0x0ec835e8));
  MAD_F_MLA(hi, lo, t15,  MAD_F(0x061f78aa));

  t4 = MAD_F_MLZ(hi, lo) - t7;

  MAD_F_ML0(hi, lo, t12, MAD_F(0x061f78aa));
  MAD_F_MLA(hi, lo, t13, MAD_F(0x0ec835e8));

  x[4]  = MAD_F_MLZ(hi, lo) + t4;
  x[13] = -x[4];

  MAD_F_ML0(hi, lo, t8,   MAD_F(0x09bd7ca0));
  MAD_F_MLA(hi, lo, t9,  -MAD_F(0x0216a2a2));
  MAD_F_MLA(hi, lo, t10,  MAD_F(0x0fdcf549));
  MAD_F_MLA(hi, lo, t11, -MAD_F(0x0cb19346));

  x[1]  = MAD_F_MLZ(hi, lo) + t4;
  x[16] = -x[1];

  MAD_F_ML0(hi, lo, t8,  -MAD_F(0x0fdcf549));
  MAD_F_MLA(hi, lo, t9,  -MAD_F(0x0cb19346));
  MAD_F_MLA(hi, lo, t10, -MAD_F(0x09bd7ca0));
  MAD_F_MLA(hi, lo, t11, -MAD_F(0x0216a2a2));

  x[25] = x[28] = MAD_F_MLZ(hi, lo) + t4;

  MAD_F_ML0(hi, lo, X[1],  -MAD_F(0x0fdcf549));
  MAD_F_MLA(hi, lo, X[7],  -MAD_F(0x0cb19346));
  MAD_F_MLA(hi, lo, X[10], -MAD_F(0x09bd7ca0));
  MAD_F_MLA(hi, lo, X[16], -MAD_F(0x0216a2a2));

  t5 = MAD_F_MLZ(hi, lo) - t6;

  MAD_F_ML0(hi, lo, X[0],   MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[2],   MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[3],   MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[5],   MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[6],   MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[8],  -MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[9],   MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[11], -MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[12],  MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[14], -MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[15],  MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[17], -MAD_F(0x0d7e8807));

  x[2]  = MAD_F_MLZ(hi, lo) + t5;
  x[15] = -x[2];

  MAD_F_ML0(hi, lo, X[0],   MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[2],   MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[3],   MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[5],   MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[6],  -MAD_F(0x00b2aa3e));
  MAD_F_MLA(hi, lo, X[8],   MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[9],  -MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[11],  MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[12], -MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[14],  MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[15], -MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[17],  MAD_F(0x0e313245));

  x[3]  = MAD_F_MLZ(hi, lo) + t5;
  x[14] = -x[3];

  MAD_F_ML0(hi, lo, X[0],  -MAD_F(0x0ffc19fd));
  MAD_F_MLA(hi, lo, X[2],  -MAD_F(0x0f9ee890));
  MAD_F_MLA(hi, lo, X[3],  -MAD_F(0x0f426cb5));
  MAD_F_MLA(hi, lo, X[5],  -MAD_F(0x0e313245));
  MAD_F_MLA(hi, lo, X[6],  -MAD_F(0x0d7e8807));
  MAD_F_MLA(hi, lo, X[8],  -MAD_F(0x0bcbe352));
  MAD_F_MLA(hi, lo, X[9],  -MAD_F(0x0acf37ad));
  MAD_F_MLA(hi, lo, X[11], -MAD_F(0x0898c779));
  MAD_F_MLA(hi, lo, X[12], -MAD_F(0x07635284));
  MAD_F_MLA(hi, lo, X[14], -MAD_F(0x04cfb0e2));
  MAD_F_MLA(hi, lo, X[15], -MAD_F(0x03768962));
  MAD_F_MLA(hi, lo, X[17], -MAD_F(0x00b2aa3e));

  x[26] = x[27] = MAD_F_MLZ(hi, lo) + t5;
}
#  endif

/*
 * NAME:	III_imdct_l()
 * DESCRIPTION:	perform IMDCT and windowing for long blocks
 */
static
void III_imdct_l(mad_fixed_t const X[18], mad_fixed_t z[36],
		 unsigned int block_type)
{
  unsigned int i;

  /* IMDCT */

  imdct36(X, z);

  /* windowing */

  switch (block_type) {
  case 0:  /* normal window */
# if defined(ASO_INTERLEAVE1)
    {
      register mad_fixed_t tmp1, tmp2;

      tmp1 = window_l[0];
      tmp2 = window_l[1];

      for (i = 0; i < 34; i += 2) {
	z[i + 0] = mad_f_mul(z[i + 0], tmp1);
	tmp1 = window_l[i + 2];
	z[i + 1] = mad_f_mul(z[i + 1], tmp2);
	tmp2 = window_l[i + 3];
      }

      z[34] = mad_f_mul(z[34], tmp1);
      z[35] = mad_f_mul(z[35], tmp2);
    }
# elif defined(ASO_INTERLEAVE2)
    {
      register mad_fixed_t tmp1, tmp2;

      tmp1 = z[0];
      tmp2 = window_l[0];

      for (i = 0; i < 35; ++i) {
	z[i] = mad_f_mul(tmp1, tmp2);
	tmp1 = z[i + 1];
	tmp2 = window_l[i + 1];
      }

      z[35] = mad_f_mul(tmp1, tmp2);
    }
# elif 1
    for (i = 0; i < 36; i += 4) {
      z[i + 0] = mad_f_mul(z[i + 0], window_l[i + 0]);
      z[i + 1] = mad_f_mul(z[i + 1], window_l[i + 1]);
      z[i + 2] = mad_f_mul(z[i + 2], window_l[i + 2]);
      z[i + 3] = mad_f_mul(z[i + 3], window_l[i + 3]);
    }
# else
    for (i =  0; i < 36; ++i) z[i] = mad_f_mul(z[i], window_l[i]);
# endif
    break;

  case 1:  /* start block */
    for (i =  0; i < 18; i += 3) {
      z[i + 0] = mad_f_mul(z[i + 0], window_l[i + 0]);
      z[i + 1] = mad_f_mul(z[i + 1], window_l[i + 1]);
      z[i + 2] = mad_f_mul(z[i + 2], window_l[i + 2]);
    }
    /*  (i = 18; i < 24; ++i) z[i] unchanged */
    for (i = 24; i < 30; ++i) z[i] = mad_f_mul(z[i], window_s[i - 18]);
    for (i = 30; i < 36; ++i) z[i] = 0;
    break;

  case 3:  /* stop block */
    for (i =  0; i <  6; ++i) z[i] = 0;
    for (i =  6; i < 12; ++i) z[i] = mad_f_mul(z[i], window_s[i - 6]);
    /*  (i = 12; i < 18; ++i) z[i] unchanged */
    for (i = 18; i < 36; i += 3) {
      z[i + 0] = mad_f_mul(z[i + 0], window_l[i + 0]);
      z[i + 1] = mad_f_mul(z[i + 1], window_l[i + 1]);
      z[i + 2] = mad_f_mul(z[i + 2], window_l[i + 2]);
    }
    break;
  }
}
# endif  /* ASO_IMDCT */

/*
 * NAME:	III_imdct_s()
 * DESCRIPTION:	perform IMDCT and windowing for short blocks
 */
static
void III_imdct_s(mad_fixed_t  X[18], mad_fixed_t z[36])
{
  mad_fixed_t y[36], *yptr;
  mad_fixed_t const *wptr;
  int w, i;
  register mad_fixed64hi_t hi;
  register mad_fixed64lo_t lo;

  /* IMDCT */

  yptr = &y[0];

  for (w = 0; w < 3; ++w) {
    register mad_fixed_t const (*s)[6];

    s = imdct_s;

    for (i = 0; i < 3; ++i) {
      MAD_F_ML0(hi, lo, X[0], (*s)[0]);
      MAD_F_MLA(hi, lo, X[1], (*s)[1]);
      MAD_F_MLA(hi, lo, X[2], (*s)[2]);
      MAD_F_MLA(hi, lo, X[3], (*s)[3]);
      MAD_F_MLA(hi, lo, X[4], (*s)[4]);
      MAD_F_MLA(hi, lo, X[5], (*s)[5]);

      yptr[i + 0] = MAD_F_MLZ(hi, lo);
      yptr[5 - i] = -yptr[i + 0];

      ++s;

      MAD_F_ML0(hi, lo, X[0], (*s)[0]);
      MAD_F_MLA(hi, lo, X[1], (*s)[1]);
      MAD_F_MLA(hi, lo, X[2], (*s)[2]);
      MAD_F_MLA(hi, lo, X[3], (*s)[3]);
      MAD_F_MLA(hi, lo, X[4], (*s)[4]);
      MAD_F_MLA(hi, lo, X[5], (*s)[5]);

      yptr[ i + 6] = MAD_F_MLZ(hi, lo);
      yptr[11 - i] = yptr[i + 6];

      ++s;
    }

    yptr += 12;
    X    += 6;
  }

  /* windowing, overlapping and concatenation */

  yptr = &y[0];
  wptr = &window_s[0];

  for (i = 0; i < 6; ++i) {
    z[i +  0] = 0;
    z[i +  6] = mad_f_mul(yptr[ 0 + 0], wptr[0]);

    MAD_F_ML0(hi, lo, yptr[ 0 + 6], wptr[6]);
    MAD_F_MLA(hi, lo, yptr[12 + 0], wptr[0]);

    z[i + 12] = MAD_F_MLZ(hi, lo);

    MAD_F_ML0(hi, lo, yptr[12 + 6], wptr[6]);
    MAD_F_MLA(hi, lo, yptr[24 + 0], wptr[0]);

    z[i + 18] = MAD_F_MLZ(hi, lo);

    z[i + 24] = mad_f_mul(yptr[24 + 6], wptr[6]);
    z[i + 30] = 0;

    ++yptr;
    ++wptr;
  }
}

/*
 * NAME:	III_overlap()
 * DESCRIPTION:	perform overlap-add of windowed IMDCT outputs
 */
static
void III_overlap(mad_fixed_t const output[36], mad_fixed_t overlap[18],
		 mad_fixed_t sample[18][32], unsigned int sb)
{
  unsigned int i;

# if defined(ASO_INTERLEAVE2)
  {
    register mad_fixed_t tmp1, tmp2;

    tmp1 = overlap[0];
    tmp2 = overlap[1];

    for (i = 0; i < 16; i += 2) {
      sample[i + 0][sb] = output[i + 0 +  0] + tmp1;
      overlap[i + 0]    = output[i + 0 + 18];
      tmp1 = overlap[i + 2];

      sample[i + 1][sb] = output[i + 1 +  0] + tmp2;
      overlap[i + 1]    = output[i + 1 + 18];
      tmp2 = overlap[i + 3];
    }

    sample[16][sb] = output[16 +  0] + tmp1;
    overlap[16]    = output[16 + 18];
    sample[17][sb] = output[17 +  0] + tmp2;
    overlap[17]    = output[17 + 18];
  }
# elif 0
  for (i = 0; i < 18; i += 2) {
    sample[i + 0][sb] = output[i + 0 +  0] + overlap[i + 0];
    overlap[i + 0]    = output[i + 0 + 18];

    sample[i + 1][sb] = output[i + 1 +  0] + overlap[i + 1];
    overlap[i + 1]    = output[i + 1 + 18];
  }
# else
  for (i = 0; i < 18; ++i) {
    sample[i][sb] = output[i +  0] + overlap[i];
    overlap[i]    = output[i + 18];
  }
# endif
}

/*
 * NAME:	III_overlap_z()
 * DESCRIPTION:	perform "overlap-add" of zero IMDCT outputs
 */
static inline
void III_overlap_z(mad_fixed_t overlap[18],
		   mad_fixed_t sample[18][32], unsigned int sb)
{
  unsigned int i;

# if defined(ASO_INTERLEAVE2)
  {
    register mad_fixed_t tmp1, tmp2;

    tmp1 = overlap[0];
    tmp2 = overlap[1];

    for (i = 0; i < 16; i += 2) {
      sample[i + 0][sb] = tmp1;
      overlap[i + 0]    = 0;
      tmp1 = overlap[i + 2];

      sample[i + 1][sb] = tmp2;
      overlap[i + 1]    = 0;
      tmp2 = overlap[i + 3];
    }

    sample[16][sb] = tmp1;
    overlap[16]    = 0;
    sample[17][sb] = tmp2;
    overlap[17]    = 0;
  }
# else
  for (i = 0; i < 18; ++i) {
    sample[i][sb] = overlap[i];
    overlap[i]    = 0;
  }
# endif
}

/*
 * NAME:	III_freqinver()
 * DESCRIPTION:	perform subband frequency inversion for odd sample lines
 */
static
void III_freqinver(mad_fixed_t sample[18][32], unsigned int sb)
{
  unsigned int i;

# if 1 || defined(ASO_INTERLEAVE1) || defined(ASO_INTERLEAVE2)
  {
    register mad_fixed_t tmp1, tmp2;

    tmp1 = sample[1][sb];
    tmp2 = sample[3][sb];

    for (i = 1; i < 13; i += 4) {
      sample[i + 0][sb] = -tmp1;
      tmp1 = sample[i + 4][sb];
      sample[i + 2][sb] = -tmp2;
      tmp2 = sample[i + 6][sb];
    }

    sample[13][sb] = -tmp1;
    tmp1 = sample[17][sb];
    sample[15][sb] = -tmp2;
    sample[17][sb] = -tmp1;
  }
# else
  for (i = 1; i < 18; i += 2)
    sample[i][sb] = -sample[i][sb];
# endif
}

/*
 * NAME:	III_decode()
 * DESCRIPTION:	decode frame main_data
 */
static
enum mad_error III_decode(struct mad_bitptr *ptr, struct mad_frame *frame,
			  struct sideinfo *si, unsigned int nch)
{
  struct mad_header *header = &frame->header;
  unsigned int sfreqi, ngr, gr;

  {
    unsigned int sfreq;

    sfreq = header->samplerate;
    if (header->flags & MAD_FLAG_MPEG_2_5_EXT)
      sfreq *= 2;

    /* 48000 => 0, 44100 => 1, 32000 => 2,
       24000 => 3, 22050 => 4, 16000 => 5 */
    sfreqi = ((sfreq >>  7) & 0x000f) +
             ((sfreq >> 15) & 0x0001) - 8;

    if (header->flags & MAD_FLAG_MPEG_2_5_EXT)
      sfreqi += 3;
  }

  /* scalefactors, Huffman decoding, requantization */

  ngr = (header->flags & MAD_FLAG_LSF_EXT) ? 1 : 2;

  for (gr = 0; gr < ngr; ++gr) {
    struct granule *granule = &si->gr[gr];
    unsigned char const *sfbwidth[2];
    mad_fixed_t xr[2][576];
    unsigned int ch;
    enum mad_error error;

    for (ch = 0; ch < nch; ++ch) {
      struct channel *channel = &granule->ch[ch];
      unsigned int part2_length;

      sfbwidth[ch] = sfbwidth_table[sfreqi].l;
      if (channel->block_type == 2) {
	sfbwidth[ch] = (channel->flags & mixed_block_flag) ?
	  sfbwidth_table[sfreqi].m : sfbwidth_table[sfreqi].s;
      }

      if (header->flags & MAD_FLAG_LSF_EXT) {
	part2_length = III_scalefactors_lsf(ptr, channel,
					    ch == 0 ? 0 : &si->gr[1].ch[1],
					    header->mode_extension);
      }
      else {
	part2_length = III_scalefactors(ptr, channel, &si->gr[0].ch[ch],
					gr == 0 ? 0 : si->scfsi[ch]);
      }

      error = III_huffdecode(ptr, xr[ch], channel, sfbwidth[ch], part2_length);
      if (error)
	return error;
    }

    /* joint stereo processing */

    if (header->mode == MAD_MODE_JOINT_STEREO && header->mode_extension) {
      error = III_stereo(xr, granule, header, sfbwidth[0]);
      if (error)
	return error;
    }

    /* reordering, alias reduction, IMDCT, overlap-add, frequency inversion */

    for (ch = 0; ch < nch; ++ch) {
      struct channel const *channel = &granule->ch[ch];
      mad_fixed_t (*sample)[32] = &frame->sbsample[ch][18 * gr];
      unsigned int sb, l, i, sblimit;
      mad_fixed_t output[36];

      if (channel->block_type == 2) {
	III_reorder(xr[ch], channel, (unsigned char*) sfbwidth[ch]);

# if !defined(OPT_STRICT)
	/*
	 * According to ISO/IEC 11172-3, "Alias reduction is not applied for
	 * granules with block_type == 2 (short block)." However, other
	 * sources suggest alias reduction should indeed be performed on the
	 * lower two subbands of mixed blocks. Most other implementations do
	 * this, so by default we will too.
	 */
	if (channel->flags & mixed_block_flag)
	  III_aliasreduce(xr[ch], 36);
# endif
      }
      else
	III_aliasreduce(xr[ch], 576);

      l = 0;

      /* subbands 0-1 */

      if (channel->block_type != 2 || (channel->flags & mixed_block_flag)) {
	unsigned int block_type;

	block_type = channel->block_type;
	if (channel->flags & mixed_block_flag)
	  block_type = 0;

	/* long blocks */
	for (sb = 0; sb < 2; ++sb, l += 18) {
	  III_imdct_l(&xr[ch][l], output, block_type);
	  III_overlap(output, (*frame->overlap)[ch][sb], sample, sb);
	}
      }
      else {
	/* short blocks */
	for (sb = 0; sb < 2; ++sb, l += 18) {
	  III_imdct_s(&xr[ch][l], output);
	  III_overlap(output, (*frame->overlap)[ch][sb], sample, sb);
	}
      }

      III_freqinver(sample, 1);

      /* (nonzero) subbands 2-31 */

      i = 576;
      while (i > 36 && xr[ch][i - 1] == 0)
	--i;

      sblimit = 32 - (576 - i) / 18;

      if (channel->block_type != 2) {
	/* long blocks */
	for (sb = 2; sb < sblimit; ++sb, l += 18) {
	  III_imdct_l(&xr[ch][l], output, channel->block_type);
	  III_overlap(output, (*frame->overlap)[ch][sb], sample, sb);

	  if (sb & 1)
	    III_freqinver(sample, sb);
	}
      }
      else {
	/* short blocks */
	for (sb = 2; sb < sblimit; ++sb, l += 18) {
	  III_imdct_s(&xr[ch][l], output);
	  III_overlap(output, (*frame->overlap)[ch][sb], sample, sb);

	  if (sb & 1)
	    III_freqinver(sample, sb);
	}
      }

      /* remaining (zero) subbands */

      for (sb = sblimit; sb < 32; ++sb) {
	III_overlap_z((*frame->overlap)[ch][sb], sample, sb);

	if (sb & 1)
	  III_freqinver(sample, sb);
      }
    }
  }

  return MAD_ERROR_NONE;
}

/*
 * NAME:	layer->III()
 * DESCRIPTION:	decode a single Layer III frame
 */
int mad_layer_III(struct mad_stream *stream, struct mad_frame *frame)
{
  struct mad_header *header = &frame->header;
  unsigned int nch, priv_bitlen, next_md_begin = 0;
  unsigned int si_len, data_bitlen, md_len;
  unsigned int frame_space, frame_used, frame_free;
  struct mad_bitptr ptr;
  struct sideinfo si;
  enum mad_error error;
  int result = 0;


  /* allocate Layer III dynamic structures */

  if (stream->main_data == 0)
  {
    stream->main_data = (unsigned char (*) [MAD_BUFFER_MDLEN]) LocalAlloc(0, MAD_BUFFER_MDLEN);
    if (stream->main_data == 0)
	{
      stream->error = MAD_ERROR_NOMEM;
      return -1;
    }
  }

  if (frame->overlap == 0)
  {
    frame->overlap = (mad_fixed_t (*)[2][32][18]) LocalAlloc(LPTR, 2 * 32 * 18 * sizeof(mad_fixed_t));
    if (frame->overlap == 0)
	{
      stream->error = MAD_ERROR_NOMEM;
      return -1;
    }
  }

  nch = MAD_NCHANNELS(header);
  si_len = (header->flags & MAD_FLAG_LSF_EXT) ? (nch == 1 ? 9 : 17) : (nch == 1 ? 17 : 32);

  /* check frame sanity */

  if (stream->next_frame - mad_bit_nextbyte(&stream->ptr) < (signed int) si_len)
  {
    stream->error = MAD_ERROR_BADFRAMELEN;
    stream->md_len = 0;
    return -1;
  }

  /* check CRC word */

  if (header->flags & MAD_FLAG_PROTECTION)
  {
    header->crc_check = mad_bit_crc(stream->ptr, si_len * CHAR_BIT, header->crc_check);

    if (header->crc_check != header->crc_target && !(frame->options & MAD_OPTION_IGNORECRC))
	{
      stream->error = MAD_ERROR_BADCRC;
      result = -1;
    }
  }
  

  /* decode frame side information */


  error = III_sideinfo(&stream->ptr, nch, header->flags & MAD_FLAG_LSF_EXT, &si, &data_bitlen, &priv_bitlen);
  if (error && result == 0)
  {
    stream->error = error;
    result = -1;
  }

  header->flags        |= priv_bitlen;
  header->private_bits |= si.private_bits;

  /* find main_data of next frame */

  {
    struct mad_bitptr peek;
    unsigned long header;

    mad_bit_init(&peek, stream->next_frame);

    header = mad_bit_read(&peek, 32);
    if ((header & 0xffe60000L) /* syncword | layer */ == 0xffe20000L)
	{
      if (!(header & 0x00010000L))  /* protection_bit */
		mad_bit_skip(&peek, 16);  /* crc_check */

      next_md_begin = mad_bit_read(&peek, (header & 0x00080000L) /* ID */ ? 9 : 8);
    }

    mad_bit_finish(&peek);
  }

  /* find main_data of this frame */

  /* size of data in this frame */
  frame_space = stream->next_frame - mad_bit_nextbyte(&stream->ptr);

 
  // if next_md_begin goes back too much, next_md_begin is wrong
  if (next_md_begin > si.main_data_begin + frame_space)
    next_md_begin = 0;


  // size of main data for this frame, data from previous frame + data in this frame - data for next frame
  // this number of bytes we need to send to decoder
  md_len = si.main_data_begin + frame_space - next_md_begin;

  // number of unsed bytes from this frame data

  frame_used = 0;

//	printf("si.main_data_begin %u\n", si.main_data_begin);
	if(si.main_data_begin == 0) /* don't need to go back */
	{
		ptr = stream->ptr;
		stream->md_len = 0;
		frame_used = md_len;
	}
	else
	{
		if (si.main_data_begin > stream->md_len) 
		{

	


			stream->md_len = 0;
			struct mad_stream stream_tmp;
			struct mad_header header_tmp;

			mad_stream_init(&stream_tmp);
			mad_header_init(&header_tmp);

			// need this amount of data in reservoir
			unsigned int need = si.main_data_begin;

			// set temporary stream
			stream_tmp.buffer = stream->buffer;
			stream_tmp.bufend = stream->bufend;
			stream_tmp.this_frame = stream->this_frame;
			stream_tmp.next_frame = stream->this_frame;
			stream_tmp.sync = stream->sync;
			stream_tmp.freerate = stream->freerate;
			mad_bit_init(&stream_tmp.ptr, stream_tmp.this_frame);

			// go back across the stream and load main_data into buffer
			int error_tmp = 0;
			unsigned int nchannel;
			unsigned int si_length;
			unsigned int data_length;
			unsigned int frame_length;
			unsigned int crc_len = 0;
			while(need)
			{
				error_tmp = 0;
				// search previous frame
				while(mad_header_decode_back(&header_tmp, &stream_tmp))
				{
					if(MAD_RECOVERABLE(stream_tmp.error))
						continue;

					//printf("MAD_RECOVERABLE\n");
					error_tmp = 1;
					
					break;
				}

				if(error_tmp == 0)
				{

					if(header_tmp.layer != header->layer)
						continue;

					if(header_tmp.samplerate != header->samplerate)
						continue;

					if(header_tmp.mode != header->mode)
						continue;

					

				}

				if(error_tmp)
				{
				//	printf("error_tmp\n");
					stream->md_len = 0;
					break;
				}

				// we have previous frame
				// calculate size of data in this frame
				crc_len = 0;
				if (header_tmp.flags & MAD_FLAG_PROTECTION)
					crc_len = 2;

				nchannel = MAD_NCHANNELS(&header_tmp);
				si_length = (header_tmp.flags & MAD_FLAG_LSF_EXT) ? (nchannel == 1 ? 9 : 17) : (nchannel == 1 ? 17 : 32);
				frame_length = ( stream_tmp.next_frame - stream_tmp.this_frame);
				if(frame_length < si_length + 4 + crc_len)
				{
					stream->md_len = 0;
					//printf("frame_length < si_length + 4 + crc_len}\n");
					break;
				}

	
				data_length = frame_length - 4 - si_length - crc_len;	
				if(need < data_length)
				{
					data_length = need;	
				}
						
				CopyMemory(*stream->main_data + si.main_data_begin - stream->md_len - data_length,
							stream_tmp.next_frame - data_length, 
							data_length);
			
			
				need -= data_length;
				stream->md_len += data_length;

			
				stream_tmp.next_frame = stream_tmp.this_frame;
		
			}

			mad_stream_finish(&stream_tmp);
			mad_header_finish(&header_tmp);

			if (si.main_data_begin > stream->md_len) 
			{
				//printf("si.main_data_begin > stream->md_len\n");
				if (result == 0)
				{
					stream->error = MAD_ERROR_BADDATAPTR;
					result = -1;
				}
				
			}
			else
			{
				mad_bit_init(&ptr, *stream->main_data + stream->md_len - si.main_data_begin);
				if (md_len > si.main_data_begin) // there in not enough data in main data buffer, we need some data from this frame
				{
					frame_used = md_len - si.main_data_begin;
					CopyMemory(*stream->main_data + stream->md_len, mad_bit_nextbyte(&stream->ptr), frame_used);
					stream->md_len += frame_used;

				}
			}
				
		}
		else

		{
			mad_bit_init(&ptr, *stream->main_data + stream->md_len - si.main_data_begin);

			if (md_len > si.main_data_begin) // there in not enough data in main data buffer, we need some data from this frame
			{
				
				frame_used = md_len - si.main_data_begin;
				CopyMemory(*stream->main_data + stream->md_len, mad_bit_nextbyte(&stream->ptr), frame_used);
				stream->md_len += frame_used;

			}
		
		}
	}





  /* number of unused bytes in this frame */
  
  frame_free = frame_space - frame_used;

/*
  printf("frame size: %u\n", stream->next_frame - stream->this_frame);
  printf("frame_used: %u\n", frame_used);
  printf("frame_free: %u\n", frame_free);
  printf("stream->md_len: %u\n", stream->md_len);
  printf("si.main_data_begin: %u\n\n", si.main_data_begin);

  */
  /* decode main_data */

  if (result == 0)
  {
    error = III_decode(&ptr, frame, &si, nch);
    if (error)
	{
      stream->error = error;
      result = -1;
    }

    /* designate ancillary bits */

    stream->anc_ptr    = ptr;
    stream->anc_bitlen = md_len * CHAR_BIT - data_bitlen;
  }

# if 0 && defined(DEBUG)
  fprintf(stderr,
	  "main_data_begin:%u, md_len:%u, frame_free:%u, "
	  "data_bitlen:%u, anc_bitlen: %u\n",
	  si.main_data_begin, md_len, frame_free,
	  data_bitlen, stream->anc_bitlen);
# endif

  /* preload main_data buffer with up to 511 bytes for next frame(s) */

	if (frame_free >= next_md_begin) // reservoir is empty, copy unused data from this frame into reservoir
	{
		// we can't have more than 511 bytes free_frame
	
		// copy unused data from this frame into buffer
		CopyMemory(*stream->main_data, stream->next_frame - next_md_begin, next_md_begin);
		stream->md_len = next_md_begin;
	}

	else  // reservoir isn't empty
	{
		if (md_len < si.main_data_begin) 
		{
		// there are unused data in main_data buffer, shift this data to the beginning of main_data buffer
			unsigned int extra;
			extra = si.main_data_begin - md_len;

			if (extra + frame_free > next_md_begin)
				extra = next_md_begin - frame_free;

			if (extra < stream->md_len)
			{
				MoveMemory(*stream->main_data, *stream->main_data + stream->md_len - extra, extra);
				stream->md_len = extra;
			}
		}
		else
		{	// main_data buffer is empty
			stream->md_len = 0;
		}


		// add unused data from this frame to main_data buffer
		CopyMemory(*stream->main_data + stream->md_len, stream->next_frame - frame_free, frame_free);
		stream->md_len += frame_free;
	}

//	printf("si.main_data_begin: %u\n", si.main_data_begin);
//	printf("stream->md_len:     %u\n", stream->md_len);

	return result;
}



// ************************* layer3.c END************************/



// ***************** fixed.c *************************************/


/*
 * NAME:	fixed->abs()
 * DESCRIPTION:	return absolute value of a fixed-point number
 */
mad_fixed_t mad_f_abs(mad_fixed_t x)
{
  return x < 0 ? -x : x;
}

/*
 * NAME:	fixed->div()
 * DESCRIPTION:	perform division using fixed-point math
 */
mad_fixed_t mad_f_div(mad_fixed_t x, mad_fixed_t y)
{
  mad_fixed_t q, r;
  unsigned int bits;

  q = mad_f_abs(x / y);

  if (x < 0) {
    x = -x;
    y = -y;
  }

  r = x % y;

  if (y < 0) {
    x = -x;
    y = -y;
  }

  if (q > mad_f_intpart(MAD_F_MAX) &&
      !(q == -mad_f_intpart(MAD_F_MIN) && r == 0 && (x < 0) != (y < 0)))
    return 0;

  for (bits = MAD_F_FRACBITS; bits && r; --bits) {
    q <<= 1, r <<= 1;
    if (r >= y)
      r -= y, ++q;
  }

  /* round */
  if (2 * r >= y)
    ++q;

  /* fix sign */
  if ((x < 0) != (y < 0))
    q = -q;

  return q << bits;
}


// ************************* fixed.c END *******************/



// ************************** frame.c ************************ /


static
unsigned long const bitrate_table[5][15] = {
  /* MPEG-1 */
  { 0,  32000,  64000,  96000, 128000, 160000, 192000, 224000,  /* Layer I   */
       256000, 288000, 320000, 352000, 384000, 416000, 448000 },
  { 0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,  /* Layer II  */
       128000, 160000, 192000, 224000, 256000, 320000, 384000 },
  { 0,  32000,  40000,  48000,  56000,  64000,  80000,  96000,  /* Layer III */
       112000, 128000, 160000, 192000, 224000, 256000, 320000 },

  /* MPEG-2 LSF */
  { 0,  32000,  48000,  56000,  64000,  80000,  96000, 112000,  /* Layer I   */
       128000, 144000, 160000, 176000, 192000, 224000, 256000 },
  { 0,   8000,  16000,  24000,  32000,  40000,  48000,  56000,  /* Layers    */
        64000,  80000,  96000, 112000, 128000, 144000, 160000 } /* II & III  */
};

static
unsigned int const samplerate_table[3] = { 44100, 48000, 32000 };

static
int (*const decoder_table[3])(struct mad_stream *, struct mad_frame *) = {
  mad_layer_I,
  mad_layer_II,
  mad_layer_III
};

/*
 * NAME:	header->init()
 * DESCRIPTION:	initialize header struct
 */
void mad_header_init(struct mad_header *header)
{

  header->layer          = 0;
  header->mode           = 0;
  header->mode_extension = 0;
  header->emphasis       = 0;

  header->bitrate        = 0;
  header->samplerate     = 0;

  header->crc_check      = 0;
  header->crc_target     = 0;

  header->flags          = 0;
  header->private_bits   = 0;

  

  
  header->duration       = mad_timer_zero;
}

/*
 * NAME:	frame->init()
 * DESCRIPTION:	initialize frame struct
 */
void mad_frame_init(struct mad_frame *frame)
{
  mad_header_init(&frame->header);

  frame->options = 0;

  frame->overlap = 0;
  mad_frame_mute(frame);
}

/*
 * NAME:	frame->finish()
 * DESCRIPTION:	deallocate any dynamic memory associated with frame
 */
void mad_frame_finish(struct mad_frame *frame)
{
  mad_header_finish(&frame->header);

  if (frame->overlap) {
    LocalFree(frame->overlap);
    frame->overlap = 0;
  }
}

/*
 * NAME:	decode_header()
 * DESCRIPTION:	read header data and following CRC word
 */
static
int decode_header(struct mad_header *header, struct mad_stream *stream)
{
  unsigned int index;

  header->flags        = 0;
  header->private_bits = 0;

  /* header() */

  /* syncword */
  mad_bit_skip(&stream->ptr, 11);

  /* MPEG 2.5 indicator (really part of syncword) */
  if (mad_bit_read(&stream->ptr, 1) == 0)
    header->flags |= MAD_FLAG_MPEG_2_5_EXT;

  /* ID */
  if (mad_bit_read(&stream->ptr, 1) == 0)
    header->flags |= MAD_FLAG_LSF_EXT;
  else if (header->flags & MAD_FLAG_MPEG_2_5_EXT)
  {
    stream->error = MAD_ERROR_LOSTSYNC;
    return -1;
  }

  /* layer */
  header->layer = 4 - mad_bit_read(&stream->ptr, 2);

  if (header->layer == 4)
  {
    stream->error = MAD_ERROR_BADLAYER;
    return -1;
  }

  /* protection_bit */
  if (mad_bit_read(&stream->ptr, 1) == 0)
  {
    header->flags    |= MAD_FLAG_PROTECTION;
    header->crc_check = mad_bit_crc(stream->ptr, 16, 0xffff);
  }

  /* bitrate_index */
  index = mad_bit_read(&stream->ptr, 4);

  if (index == 15)
  {
    stream->error = MAD_ERROR_BADBITRATE;
    return -1;
  }

  if (header->flags & MAD_FLAG_LSF_EXT)
    header->bitrate = bitrate_table[3 + (header->layer >> 1)][index];
  else
    header->bitrate = bitrate_table[header->layer - 1][index];

  /* sampling_frequency */
  index = mad_bit_read(&stream->ptr, 2);

  if (index == 3) {
    stream->error = MAD_ERROR_BADSAMPLERATE;
    return -1;
  }

  header->samplerate = samplerate_table[index];

  if (header->flags & MAD_FLAG_LSF_EXT)
  {
    header->samplerate /= 2;

    if (header->flags & MAD_FLAG_MPEG_2_5_EXT)
      header->samplerate /= 2;
  }

  /* padding_bit */
  if (mad_bit_read(&stream->ptr, 1))
    header->flags |= MAD_FLAG_PADDING;

  /* private_bit */
  if (mad_bit_read(&stream->ptr, 1))
    header->private_bits |= MAD_PRIVATE_HEADER;

  /* mode */
  header->mode = 3 - mad_bit_read(&stream->ptr, 2);

  /* mode_extension */
  header->mode_extension = mad_bit_read(&stream->ptr, 2);

  /* copyright */
  if (mad_bit_read(&stream->ptr, 1))
    header->flags |= MAD_FLAG_COPYRIGHT;

  /* original/copy */
  if (mad_bit_read(&stream->ptr, 1))
    header->flags |= MAD_FLAG_ORIGINAL;

  /* emphasis */
  header->emphasis = mad_bit_read(&stream->ptr, 2);

# if defined(OPT_STRICT)
  /*
   * ISO/IEC 11172-3 says this is a reserved emphasis value, but
   * streams exist which use it anyway. Since the value is not important
   * to the decoder proper, we allow it unless OPT_STRICT is defined.
   */
  if (header->emphasis == MAD_EMPHASIS_RESERVED) {
    stream->error = MAD_ERROR_BADEMPHASIS;
    return -1;
  }
# endif

  /* error_check() */

  /* crc_check */
  if (header->flags & MAD_FLAG_PROTECTION)
    header->crc_target = mad_bit_read(&stream->ptr, 16);



  return 0;
}

/*
 * NAME:	free_bitrate()
 * DESCRIPTION:	attempt to discover the bitstream's free bitrate
 */
static
int free_bitrate(struct mad_stream *stream, struct mad_header const *header)
{
  struct mad_bitptr keep_ptr;
  unsigned long rate = 0;
  unsigned int pad_slot, slots_per_frame;
  unsigned char const *ptr = 0;

  keep_ptr = stream->ptr;

  pad_slot = (header->flags & MAD_FLAG_PADDING) ? 1 : 0;
  slots_per_frame = (header->layer == MAD_LAYER_III &&
		     (header->flags & MAD_FLAG_LSF_EXT)) ? 72 : 144;

  while (mad_stream_sync(stream) == 0) {
    struct mad_stream peek_stream;
    struct mad_header peek_header;

    peek_stream = *stream;
    peek_header = *header;

    if (decode_header(&peek_header, &peek_stream) == 0 &&
	peek_header.layer == header->layer &&
	peek_header.samplerate == header->samplerate) {
      unsigned int N;

      ptr = mad_bit_nextbyte(&stream->ptr);

      N = ptr - stream->this_frame;

      if (header->layer == MAD_LAYER_I) {
	rate = (unsigned long) header->samplerate *
	  (N - 4 * pad_slot + 4) / 48 / 1000;
      }
      else {
	rate = (unsigned long) header->samplerate *
	  (N - pad_slot + 1) / slots_per_frame / 1000;
      }

      if (rate >= 8)
	break;
    }

    mad_bit_skip(&stream->ptr, 8);
  }

  stream->ptr = keep_ptr;

  if (rate < 8 || (header->layer == MAD_LAYER_III && rate > 640)) {
    stream->error = MAD_ERROR_LOSTSYNC;
    return -1;
  }

  stream->freerate = rate * 1000;

  return 0;
}



/*
 * NAME:	header->decode()
 * DESCRIPTION:	read the next frame header from the stream
 */
int mad_header_decode(struct mad_header *header, struct mad_stream *stream)
{
  register unsigned char const *ptr, *end;
  unsigned int pad_slot, N;
 

  ptr = stream->next_frame;
  end = stream->bufend;

  if (ptr == 0) {
    stream->error = MAD_ERROR_BUFPTR;
    goto fail;
  }

  /* stream skip */
  if (stream->skiplen)
  {
    if (!stream->sync)
      ptr = stream->this_frame;

    if (end - ptr < stream->skiplen)
	{
      stream->skiplen   -= end - ptr;
      stream->next_frame = end;

      stream->error = MAD_ERROR_BUFLEN;
      goto fail;
    }

    ptr += stream->skiplen;
    stream->skiplen = 0;

    stream->sync = 1;
  }

 sync:
  /* synchronize */
  if (stream->sync) {
    if (end - ptr < MAD_BUFFER_GUARD) {
      stream->next_frame = ptr;

      stream->error = MAD_ERROR_BUFLEN;
      goto fail;
    }
    else if (!(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0)) {
      /* mark point where frame sync word was expected */
      stream->this_frame = ptr;
      stream->next_frame = ptr + 1;

      stream->error = MAD_ERROR_LOSTSYNC;
      goto fail;
    }
  }
  else {
    mad_bit_init(&stream->ptr, ptr);

    if (mad_stream_sync(stream) == -1) {
      if (end - stream->next_frame >= MAD_BUFFER_GUARD)
	stream->next_frame = end - MAD_BUFFER_GUARD;

      stream->error = MAD_ERROR_BUFLEN;
      goto fail;
    }

    ptr = mad_bit_nextbyte(&stream->ptr);
  }

  /* begin processing */
  stream->this_frame = ptr;
  stream->next_frame = ptr + 1;  /* possibly bogus sync word */

  mad_bit_init(&stream->ptr, stream->this_frame);

  if (decode_header(header, stream) == -1)
    goto fail;

  /* calculate frame duration */
  mad_timer_set(&header->duration, 0,
		32 * MAD_NSBSAMPLES(header), header->samplerate);

  /* calculate free bit rate */
  if (header->bitrate == 0) {
    if ((stream->freerate == 0 || !stream->sync ||
	 (header->layer == MAD_LAYER_III && stream->freerate > 640000)) &&
	free_bitrate(stream, header) == -1)
      goto fail;

    header->bitrate = stream->freerate;
    header->flags  |= MAD_FLAG_FREEFORMAT;
  }

  /* calculate beginning of next frame */
  pad_slot = (header->flags & MAD_FLAG_PADDING) ? 1 : 0;

  if (header->layer == MAD_LAYER_I)
    N = ((12 * header->bitrate / header->samplerate) + pad_slot) * 4;
  else {
    unsigned int slots_per_frame;

    slots_per_frame = (header->layer == MAD_LAYER_III &&
		       (header->flags & MAD_FLAG_LSF_EXT)) ? 72 : 144;

	N = (slots_per_frame * header->bitrate / header->samplerate) + pad_slot;
	
  }

  header->size = N;

  /* verify there is enough data left in buffer to decode this frame */
  if (N + MAD_BUFFER_GUARD > end - stream->this_frame) {
    stream->next_frame = stream->this_frame;

    stream->error = MAD_ERROR_BUFLEN;
    goto fail;
  }

  stream->next_frame = stream->this_frame + N;

  if (!stream->sync) {
    /* check that a valid frame header follows this frame */

    ptr = stream->next_frame;
    if (!(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0))
	{
      ptr = stream->next_frame = stream->this_frame + 1;
      goto sync;
    }

    stream->sync = 1;
	
  }

  header->flags |= MAD_FLAG_INCOMPLETE;

  return 0;

 fail:
  stream->sync = 0;

  return -1;
}



/*
 * NAME:	header->decode()
 * DESCRIPTION:	read the next frame header from the stream
 */
int mad_header_decode_back(struct mad_header *header, struct mad_stream *stream)
{
/*
*	this_frame - points to current frame
*	next_frame - must point to ???



*/
  register unsigned char const *ptr, *end, *start;
  unsigned int pad_slot, N;

  start = stream->buffer;
  end = stream->bufend;
 
  if(stream->this_frame <= start) // we are at the start of stream, can't go back anymore
  {
	stream->error = MAD_ERROR_BUFPTR;
	//printf("stream->this_frame <= start\n");
    goto fail;
  }
   
  // go back one byte
  ptr = stream->this_frame - 1;
  


 sync:
  /* synchronize */
  if (stream->sync)
  {
    if(ptr < start)
	{
      stream->next_frame = start;
      stream->error = MAD_ERROR_BUFLEN;
	  //printf("ptr < start\n");
      goto fail;
    }
    else if (!(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0))
	{
      /* mark point where frame sync word was expected */
      stream->this_frame = ptr - 1;
      stream->error = MAD_ERROR_LOSTSYNC;
	  //printf("MAD_ERROR_LOSTSYNC\n");
      goto fail;
    }
  }
  else
  {
    mad_bit_init(&stream->ptr, ptr);

    if (mad_stream_sync_back(stream) == -1)
	{
      if (ptr < start)
		stream->this_frame = start;

      stream->error = MAD_ERROR_BUFLEN;
	  //printf("mad_stream_sync_back(stream) == -1\n");
      goto fail;
    }

    ptr = mad_bit_nextbyte(&stream->ptr);
  }

  /* begin processing */
  stream->this_frame = ptr;

  mad_bit_init(&stream->ptr, stream->this_frame);

  if (decode_header(header, stream) == -1)
  {
    stream->error = MAD_ERROR_LOSTSYNC;
//	printf("decode_header(header, stream) == -1\n");
    goto fail;
  }

  /* calculate frame duration */
  mad_timer_set(&header->duration, 0, 32 * MAD_NSBSAMPLES(header), header->samplerate);

  /* calculate free bit rate */

  
  if (header->bitrate == 0)
  {
    //stream->error = MAD_ERROR_LOSTSYNC;
    //printf("header->bitrate == 0\n");
	//goto fail;

    if ((stream->freerate == 0 || /*!stream->sync ||*/
	 (header->layer == MAD_LAYER_III && stream->freerate > 640000)))
	 {
	  stream->error = MAD_ERROR_LOSTSYNC;	
      goto fail;
	 }

    header->bitrate = stream->freerate;
    header->flags  |= MAD_FLAG_FREEFORMAT;
  }

  /* calculate beginning of next frame */
  pad_slot = (header->flags & MAD_FLAG_PADDING) ? 1 : 0;

  if (header->layer == MAD_LAYER_I)
    N = ((12 * header->bitrate / header->samplerate) + pad_slot) * 4;
  else
  {
    unsigned int slots_per_frame;

    slots_per_frame = (header->layer == MAD_LAYER_III && (header->flags & MAD_FLAG_LSF_EXT)) ? 72 : 144;
	N = (slots_per_frame * header->bitrate / header->samplerate) + pad_slot;
	
  }

  header->size = N;

  /* verify there is enough data left in buffer to decode this frame */

  if(N + stream->this_frame > stream->next_frame)
  {
	stream->error = MAD_ERROR_LOSTSYNC;
	goto fail;
  }

  if(stream->this_frame + MAD_BUFFER_MDLEN < stream->next_frame)
  {
      stream->error = MAD_ERROR_BUFLEN;
	  //printf("mad_stream_sync_back(stream) == -1\n");
      goto fail;

  }

  
 

  if (!stream->sync)
  {
	  const unsigned char *next = stream->this_frame + N;
	  if(next != stream->next_frame)
	  {
		if(stream->this_frame > start)
		{
			stream->this_frame--;
			ptr = stream->this_frame;
			goto sync;
		}
		
		stream->error = MAD_ERROR_LOSTSYNC;
		//printf("next != stream->next_frame\n");
		goto fail;	

	  }

	  stream->sync = 1;
  }


  header->flags |= MAD_FLAG_INCOMPLETE;

  
 
  return 0;

 fail:
  stream->sync = 0;

  return -1;
}






/*
 * NAME:	frame->decode()
 * DESCRIPTION:	decode a single frame from a bitstream
 */
int mad_frame_decode(struct mad_frame *frame, struct mad_stream *stream)
{
  frame->options = stream->options;

  /* header() */
  /* error_check() */

  if (!(frame->header.flags & MAD_FLAG_INCOMPLETE) &&
      mad_header_decode(&frame->header, stream) == -1)
    goto fail;

  /* audio_data() */

  frame->header.flags &= ~MAD_FLAG_INCOMPLETE;

  if (decoder_table[frame->header.layer - 1](stream, frame) == -1) {
    if (!MAD_RECOVERABLE(stream->error))
      stream->next_frame = stream->this_frame;

    goto fail;
  }

  /* ancillary_data() */

  if (frame->header.layer != MAD_LAYER_III) {
    struct mad_bitptr next_frame;

    mad_bit_init(&next_frame, stream->next_frame);

    stream->anc_ptr    = stream->ptr;
    stream->anc_bitlen = mad_bit_length(&stream->ptr, &next_frame);

    mad_bit_finish(&next_frame);
  }

  return 0;

 fail:
  stream->anc_bitlen = 0;
  return -1;
}

/*
 * NAME:	frame->mute()
 * DESCRIPTION:	zero all subband values so the frame becomes silent
 */
void mad_frame_mute(struct mad_frame *frame)
{
  unsigned int s, sb;

  for (s = 0; s < 36; ++s) {
    for (sb = 0; sb < 32; ++sb) {
      frame->sbsample[0][s][sb] =
      frame->sbsample[1][s][sb] = 0;
    }
  }

  if (frame->overlap) {
    for (s = 0; s < 18; ++s) {
      for (sb = 0; sb < 32; ++sb) {
	(*frame->overlap)[0][sb][s] =
	(*frame->overlap)[1][sb][s] = 0;
      }
    }
  }
}


// ************************* frame.c END *******************/

// **************************** decoder.c ********************/


# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif

# ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
# endif

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif

# ifdef HAVE_FCNTL_H
#  include <fcntl.h>
# endif

# include <stdlib.h>

# ifdef HAVE_ERRNO_H
#  include <errno.h>
# endif



/*
 * NAME:	decoder->init()
 * DESCRIPTION:	initialize a decoder object with callback routines
 */
void mad_decoder_init(struct mad_decoder *decoder, void *data,
		      enum mad_flow (*input_func)(void *,
						  struct mad_stream *),
		      enum mad_flow (*header_func)(void *,
						   struct mad_header const *),
		      enum mad_flow (*filter_func)(void *,
						   struct mad_stream const *,
						   struct mad_frame *),
		      enum mad_flow (*output_func)(void *,
						   struct mad_header const *,
						   struct mad_pcm *),
		      enum mad_flow (*error_func)(void *,
						  struct mad_stream *,
						  struct mad_frame *),
		      enum mad_flow (*message_func)(void *,
						    void *, unsigned int *))
{
  decoder->mode         = -1;

  decoder->options      = 0;

  decoder->async.pid    = 0;
  decoder->async.in     = -1;
  decoder->async.out    = -1;

  decoder->sync         = 0;

  decoder->cb_data      = data;

  decoder->input_func   = input_func;
  decoder->header_func  = header_func;
  decoder->filter_func  = filter_func;
  decoder->output_func  = output_func;
  decoder->error_func   = error_func;
  decoder->message_func = message_func;
}


int mad_decoder_finish(struct mad_decoder *decoder)
{
# if defined(USE_ASYNC)
  if (decoder->mode == MAD_DECODER_MODE_ASYNC && decoder->async.pid) {
    pid_t pid;
    int status;

    close(decoder->async.in);

    do
      pid = waitpid(decoder->async.pid, &status, 0);
    while (pid == -1 && errno == EINTR);

    decoder->mode = -1;

    close(decoder->async.out);

    decoder->async.pid = 0;
    decoder->async.in  = -1;
    decoder->async.out = -1;

    if (pid == -1)
      return -1;

    return (!WIFEXITED(status) || WEXITSTATUS(status)) ? -1 : 0;
  }
# endif

  return 0;
}

# if defined(USE_ASYNC)
static
enum mad_flow send_io(int fd, void const *data, size_t len)
{
  char const *ptr = data;
  ssize_t count;

  while (len) {
    do
      count = write(fd, ptr, len);
    while (count == -1 && errno == EINTR);

    if (count == -1)
      return MAD_FLOW_BREAK;

    len -= count;
    ptr += count;
  }

  return MAD_FLOW_CONTINUE;
}

static
enum mad_flow receive_io(int fd, void *buffer, size_t len)
{
  char *ptr = buffer;
  ssize_t count;

  while (len) {
    do
      count = read(fd, ptr, len);
    while (count == -1 && errno == EINTR);

    if (count == -1)
      return (errno == EAGAIN) ? MAD_FLOW_IGNORE : MAD_FLOW_BREAK;
    else if (count == 0)
      return MAD_FLOW_STOP;

    len -= count;
    ptr += count;
  }

  return MAD_FLOW_CONTINUE;
}

static
enum mad_flow receive_io_blocking(int fd, void *buffer, size_t len)
{
  int flags, blocking;
  enum mad_flow result;

  flags = fcntl(fd, F_GETFL);
  if (flags == -1)
    return MAD_FLOW_BREAK;

  blocking = flags & ~O_NONBLOCK;

  if (blocking != flags &&
      fcntl(fd, F_SETFL, blocking) == -1)
    return MAD_FLOW_BREAK;

  result = receive_io(fd, buffer, len);

  if (flags != blocking &&
      fcntl(fd, F_SETFL, flags) == -1)
    return MAD_FLOW_BREAK;

  return result;
}

static
enum mad_flow send(int fd, void const *message, unsigned int size)
{
  enum mad_flow result;

  /* send size */

  result = send_io(fd, &size, sizeof(size));

  /* send message */

  if (result == MAD_FLOW_CONTINUE)
    result = send_io(fd, message, size);

  return result;
}

static
enum mad_flow receive(int fd, void **message, unsigned int *size)
{
  enum mad_flow result;
  unsigned int actual;

  if (*message == 0)
    *size = 0;

  /* receive size */

  result = receive_io(fd, &actual, sizeof(actual));

  /* receive message */

  if (result == MAD_FLOW_CONTINUE) {
    if (actual > *size)
      actual -= *size;
    else {
      *size  = actual;
      actual = 0;
    }

    if (*size > 0) {
      if (*message == 0) {
	*message = LocalAlloc(0, *size);
	if (*message == 0)
	  return MAD_FLOW_BREAK;
      }

      result = receive_io_blocking(fd, *message, *size);
    }

    /* throw away remainder of message */

    while (actual && result == MAD_FLOW_CONTINUE) {
      char sink[256];
      unsigned int len;

      len = actual > sizeof(sink) ? sizeof(sink) : actual;

      result = receive_io_blocking(fd, sink, len);

      actual -= len;
    }
  }

  return result;
}

static
enum mad_flow check_message(struct mad_decoder *decoder)
{
  enum mad_flow result;
  void *message = 0;
  unsigned int size;

  result = receive(decoder->async.in, &message, &size);

  if (result == MAD_FLOW_CONTINUE) {
    if (decoder->message_func == 0)
      size = 0;
    else {
      result = decoder->message_func(decoder->cb_data, message, &size);

      if (result == MAD_FLOW_IGNORE ||
	  result == MAD_FLOW_BREAK)
	size = 0;
    }

    if (send(decoder->async.out, message, size) != MAD_FLOW_CONTINUE)
      result = MAD_FLOW_BREAK;
  }

  if (message)
    LocalFree(message);

  return result;
}
# endif

static
enum mad_flow error_default(void *data, struct mad_stream *stream,
			    struct mad_frame *frame)
{
  int *bad_last_frame = (int*) data;

  switch (stream->error) {
  case MAD_ERROR_BADCRC:
    if (*bad_last_frame)
      mad_frame_mute(frame);
    else
      *bad_last_frame = 1;

    return MAD_FLOW_IGNORE;

  default:
    return MAD_FLOW_CONTINUE;
  }
}

static
int run_sync(struct mad_decoder *decoder)
{
  enum mad_flow (*error_func)(void *, struct mad_stream *, struct mad_frame *);
  void *error_data;
  int bad_last_frame = 0;
  struct mad_stream *stream;
  struct mad_frame *frame;
  struct mad_synth *synth;
  int result = 0;

  if (decoder->input_func == 0)
    return 0;

  if (decoder->error_func) {
    error_func = decoder->error_func;
    error_data = decoder->cb_data;
  }
  else {
    error_func = error_default;
    error_data = &bad_last_frame;
  }

  stream = &decoder->sync->stream;
  frame  = &decoder->sync->frame;
  synth  = &decoder->sync->synth;

  mad_stream_init(stream);
  mad_frame_init(frame);
  mad_synth_init(synth);

  mad_stream_options(stream, decoder->options);

  do {
    switch (decoder->input_func(decoder->cb_data, stream)) {
    case MAD_FLOW_STOP:
      goto done;
    case MAD_FLOW_BREAK:
      goto fail;
    case MAD_FLOW_IGNORE:
      continue;
    case MAD_FLOW_CONTINUE:
      break;
    }

    while (1) {
# if defined(USE_ASYNC)
      if (decoder->mode == MAD_DECODER_MODE_ASYNC) {
	switch (check_message(decoder)) {
	case MAD_FLOW_IGNORE:
	case MAD_FLOW_CONTINUE:
	  break;
	case MAD_FLOW_BREAK:
	  goto fail;
	case MAD_FLOW_STOP:
	  goto done;
	}
      }
# endif

      if (decoder->header_func) {
	if (mad_header_decode(&frame->header, stream) == -1) {
	  if (!MAD_RECOVERABLE(stream->error))
	    break;

	  switch (error_func(error_data, stream, frame)) {
	  case MAD_FLOW_STOP:
	    goto done;
	  case MAD_FLOW_BREAK:
	    goto fail;
	  case MAD_FLOW_IGNORE:
	  case MAD_FLOW_CONTINUE:
	  default:
	    continue;
	  }
	}

	switch (decoder->header_func(decoder->cb_data, &frame->header)) {
	case MAD_FLOW_STOP:
	  goto done;
	case MAD_FLOW_BREAK:
	  goto fail;
	case MAD_FLOW_IGNORE:
	  continue;
	case MAD_FLOW_CONTINUE:
	  break;
	}
      }

      if (mad_frame_decode(frame, stream) == -1) {
	if (!MAD_RECOVERABLE(stream->error))
	  break;

	switch (error_func(error_data, stream, frame)) {
	case MAD_FLOW_STOP:
	  goto done;
	case MAD_FLOW_BREAK:
	  goto fail;
	case MAD_FLOW_IGNORE:
	  break;
	case MAD_FLOW_CONTINUE:
	default:
	  continue;
	}
      }
      else
	bad_last_frame = 0;

      if (decoder->filter_func) {
	switch (decoder->filter_func(decoder->cb_data, stream, frame)) {
	case MAD_FLOW_STOP:
	  goto done;
	case MAD_FLOW_BREAK:
	  goto fail;
	case MAD_FLOW_IGNORE:
	  continue;
	case MAD_FLOW_CONTINUE:
	  break;
	}
      }

      mad_synth_frame(synth, frame);

      if (decoder->output_func) {
	switch (decoder->output_func(decoder->cb_data,
				     &frame->header, &synth->pcm)) {
	case MAD_FLOW_STOP:
	  goto done;
	case MAD_FLOW_BREAK:
	  goto fail;
	case MAD_FLOW_IGNORE:
	case MAD_FLOW_CONTINUE:
	  break;
	}
      }
    }
  }
  while (stream->error == MAD_ERROR_BUFLEN);

 fail:
  result = -1;

 done:
  mad_synth_finish(synth);
  mad_frame_finish(frame);
  mad_stream_finish(stream);

  return result;
}

# if defined(USE_ASYNC)
static
int run_async(struct mad_decoder *decoder)
{
  pid_t pid;
  int ptoc[2], ctop[2], flags;

  if (pipe(ptoc) == -1)
    return -1;

  if (pipe(ctop) == -1) {
    close(ptoc[0]);
    close(ptoc[1]);
    return -1;
  }

  flags = fcntl(ptoc[0], F_GETFL);
  if (flags == -1 ||
      fcntl(ptoc[0], F_SETFL, flags | O_NONBLOCK) == -1) {
    close(ctop[0]);
    close(ctop[1]);
    close(ptoc[0]);
    close(ptoc[1]);
    return -1;
  }

  pid = fork();
  if (pid == -1) {
    close(ctop[0]);
    close(ctop[1]);
    close(ptoc[0]);
    close(ptoc[1]);
    return -1;
  }

  decoder->async.pid = pid;

  if (pid) {
    /* parent */

    close(ptoc[0]);
    close(ctop[1]);

    decoder->async.in  = ctop[0];
    decoder->async.out = ptoc[1];

    return 0;
  }

  /* child */

  close(ptoc[1]);
  close(ctop[0]);

  decoder->async.in  = ptoc[0];
  decoder->async.out = ctop[1];

  _exit(run_sync(decoder));

  /* not reached */
  return -1;
}
# endif

/*
 * NAME:	decoder->run()
 * DESCRIPTION:	run the decoder thread either synchronously or asynchronously
 */
int mad_decoder_run(struct mad_decoder *decoder, enum mad_decoder_mode mode)
{
  int result;
  int (*run)(struct mad_decoder *) = 0;

  switch (decoder->mode = mode) {
  case MAD_DECODER_MODE_SYNC:
    run = run_sync;
    break;

  case MAD_DECODER_MODE_ASYNC:
# if defined(USE_ASYNC)
    run = run_async;
# endif
    break;
  }

  if (run == 0)
    return -1;

  decoder->sync = (SYNC*) LocalAlloc(0, sizeof(*decoder->sync));
  if (decoder->sync == 0)
    return -1;

  result = run(decoder);

  LocalFree(decoder->sync);
  decoder->sync = 0;

  return result;
}

/*
 * NAME:	decoder->message()
 * DESCRIPTION:	send a message to and receive a reply from the decoder process
 */
int mad_decoder_message(struct mad_decoder *decoder,
			void *message, unsigned int *len)
{
# if defined(USE_ASYNC)
  if (decoder->mode != MAD_DECODER_MODE_ASYNC ||
      send(decoder->async.out, message, *len) != MAD_FLOW_CONTINUE ||
      receive(decoder->async.in, &message, len) != MAD_FLOW_CONTINUE)
    return -1;

  return 0;
# else
  return -1;
# endif
}


// ********************* decoder,c END **************************/


// ******************** sftable.dat *************************/

/*
 * libmad - MPEG audio decoder library
 * Copyright (C) 2000-2004 Underbit Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: sf_table.dat,v 1.7 2004/01/23 09:41:33 rob Exp $
 */

/*
 * These are the scalefactor values for Layer I and Layer II.
 * The values are from Table B.1 of ISO/IEC 11172-3.
 *
 * There is some error introduced by the 32-bit fixed-point representation;
 * the amount of error is shown. For 16-bit PCM output, this shouldn't be
 * too much of a problem.
 *
 * Strictly speaking, Table B.1 has only 63 entries (0-62), thus a strict
 * interpretation of ISO/IEC 11172-3 would suggest that a scalefactor index of
 * 63 is invalid. However, for better compatibility with current practices, we
 * add a 64th entry.
 */

 static
mad_fixed_t const sf_table[64] = {

  MAD_F(0x20000000),  /* 2.000000000000 => 2.000000000000, e  0.000000000000 */
  MAD_F(0x1965fea5),  /* 1.587401051968 => 1.587401051074, e  0.000000000894 */
  MAD_F(0x1428a2fa),  /* 1.259921049895 => 1.259921051562, e -0.000000001667 */
  MAD_F(0x10000000),  /* 1.000000000000 => 1.000000000000, e  0.000000000000 */
  MAD_F(0x0cb2ff53),  /* 0.793700525984 => 0.793700527400, e -0.000000001416 */
  MAD_F(0x0a14517d),  /* 0.629960524947 => 0.629960525781, e -0.000000000833 */
  MAD_F(0x08000000),  /* 0.500000000000 => 0.500000000000, e  0.000000000000 */
  MAD_F(0x06597fa9),  /* 0.396850262992 => 0.396850261837, e  0.000000001155 */

  MAD_F(0x050a28be),  /* 0.314980262474 => 0.314980261028, e  0.000000001446 */
  MAD_F(0x04000000),  /* 0.250000000000 => 0.250000000000, e  0.000000000000 */
  MAD_F(0x032cbfd5),  /* 0.198425131496 => 0.198425132781, e -0.000000001285 */
  MAD_F(0x0285145f),  /* 0.157490131237 => 0.157490130514, e  0.000000000723 */
  MAD_F(0x02000000),  /* 0.125000000000 => 0.125000000000, e  0.000000000000 */
  MAD_F(0x01965fea),  /* 0.099212565748 => 0.099212564528, e  0.000000001220 */
  MAD_F(0x01428a30),  /* 0.078745065618 => 0.078745067120, e -0.000000001501 */
  MAD_F(0x01000000),  /* 0.062500000000 => 0.062500000000, e  0.000000000000 */

  MAD_F(0x00cb2ff5),  /* 0.049606282874 => 0.049606282264, e  0.000000000610 */
  MAD_F(0x00a14518),  /* 0.039372532809 => 0.039372533560, e -0.000000000751 */
  MAD_F(0x00800000),  /* 0.031250000000 => 0.031250000000, e  0.000000000000 */
  MAD_F(0x006597fb),  /* 0.024803141437 => 0.024803142995, e -0.000000001558 */
  MAD_F(0x0050a28c),  /* 0.019686266405 => 0.019686266780, e -0.000000000375 */
  MAD_F(0x00400000),  /* 0.015625000000 => 0.015625000000, e  0.000000000000 */
  MAD_F(0x0032cbfd),  /* 0.012401570719 => 0.012401569635, e  0.000000001084 */
  MAD_F(0x00285146),  /* 0.009843133202 => 0.009843133390, e -0.000000000188 */

  MAD_F(0x00200000),  /* 0.007812500000 => 0.007812500000, e  0.000000000000 */
  MAD_F(0x001965ff),  /* 0.006200785359 => 0.006200786680, e -0.000000001321 */
  MAD_F(0x001428a3),  /* 0.004921566601 => 0.004921566695, e -0.000000000094 */
  MAD_F(0x00100000),  /* 0.003906250000 => 0.003906250000, e  0.000000000000 */
  MAD_F(0x000cb2ff),  /* 0.003100392680 => 0.003100391477, e  0.000000001202 */
  MAD_F(0x000a1451),  /* 0.002460783301 => 0.002460781485, e  0.000000001816 */
  MAD_F(0x00080000),  /* 0.001953125000 => 0.001953125000, e  0.000000000000 */
  MAD_F(0x00065980),  /* 0.001550196340 => 0.001550197601, e -0.000000001262 */

  MAD_F(0x00050a29),  /* 0.001230391650 => 0.001230392605, e -0.000000000955 */
  MAD_F(0x00040000),  /* 0.000976562500 => 0.000976562500, e  0.000000000000 */
  MAD_F(0x00032cc0),  /* 0.000775098170 => 0.000775098801, e -0.000000000631 */
  MAD_F(0x00028514),  /* 0.000615195825 => 0.000615194440, e  0.000000001385 */
  MAD_F(0x00020000),  /* 0.000488281250 => 0.000488281250, e  0.000000000000 */
  MAD_F(0x00019660),  /* 0.000387549085 => 0.000387549400, e -0.000000000315 */
  MAD_F(0x0001428a),  /* 0.000307597913 => 0.000307597220, e  0.000000000693 */
  MAD_F(0x00010000),  /* 0.000244140625 => 0.000244140625, e  0.000000000000 */

  MAD_F(0x0000cb30),  /* 0.000193774542 => 0.000193774700, e -0.000000000158 */
  MAD_F(0x0000a145),  /* 0.000153798956 => 0.000153798610, e  0.000000000346 */
  MAD_F(0x00008000),  /* 0.000122070313 => 0.000122070313, e  0.000000000000 */
  MAD_F(0x00006598),  /* 0.000096887271 => 0.000096887350, e -0.000000000079 */
  MAD_F(0x000050a3),  /* 0.000076899478 => 0.000076901168, e -0.000000001689 */
  MAD_F(0x00004000),  /* 0.000061035156 => 0.000061035156, e  0.000000000000 */
  MAD_F(0x000032cc),  /* 0.000048443636 => 0.000048443675, e -0.000000000039 */
  MAD_F(0x00002851),  /* 0.000038449739 => 0.000038448721, e  0.000000001018 */

  MAD_F(0x00002000),  /* 0.000030517578 => 0.000030517578, e  0.000000000000 */
  MAD_F(0x00001966),  /* 0.000024221818 => 0.000024221838, e -0.000000000020 */
  MAD_F(0x00001429),  /* 0.000019224870 => 0.000019226223, e -0.000000001354 */
  MAD_F(0x00001000),  /* 0.000015258789 => 0.000015258789, e -0.000000000000 */
  MAD_F(0x00000cb3),  /* 0.000012110909 => 0.000012110919, e -0.000000000010 */
  MAD_F(0x00000a14),  /* 0.000009612435 => 0.000009611249, e  0.000000001186 */
  MAD_F(0x00000800),  /* 0.000007629395 => 0.000007629395, e -0.000000000000 */
  MAD_F(0x00000659),  /* 0.000006055454 => 0.000006053597, e  0.000000001858 */

  MAD_F(0x0000050a),  /* 0.000004806217 => 0.000004805624, e  0.000000000593 */
  MAD_F(0x00000400),  /* 0.000003814697 => 0.000003814697, e  0.000000000000 */
  MAD_F(0x0000032d),  /* 0.000003027727 => 0.000003028661, e -0.000000000934 */
  MAD_F(0x00000285),  /* 0.000002403109 => 0.000002402812, e  0.000000000296 */
  MAD_F(0x00000200),  /* 0.000001907349 => 0.000001907349, e -0.000000000000 */
  MAD_F(0x00000196),  /* 0.000001513864 => 0.000001512468, e  0.000000001396 */
  MAD_F(0x00000143),  /* 0.000001201554 => 0.000001203269, e -0.000000001714 */
  MAD_F(0x00000000)   /* this compatibility entry is not part of Table B.1 */

};
// ******************* sftable.dat  **************************//


// ******************* layer12.c *******************************/

/* linear scaling table */
static
mad_fixed_t const linear_table[14] = {
  MAD_F(0x15555555),  /* 2^2  / (2^2  - 1) == 1.33333333333333 */
  MAD_F(0x12492492),  /* 2^3  / (2^3  - 1) == 1.14285714285714 */
  MAD_F(0x11111111),  /* 2^4  / (2^4  - 1) == 1.06666666666667 */
  MAD_F(0x10842108),  /* 2^5  / (2^5  - 1) == 1.03225806451613 */
  MAD_F(0x10410410),  /* 2^6  / (2^6  - 1) == 1.01587301587302 */
  MAD_F(0x10204081),  /* 2^7  / (2^7  - 1) == 1.00787401574803 */
  MAD_F(0x10101010),  /* 2^8  / (2^8  - 1) == 1.00392156862745 */
  MAD_F(0x10080402),  /* 2^9  / (2^9  - 1) == 1.00195694716243 */
  MAD_F(0x10040100),  /* 2^10 / (2^10 - 1) == 1.00097751710655 */
  MAD_F(0x10020040),  /* 2^11 / (2^11 - 1) == 1.00048851978505 */
  MAD_F(0x10010010),  /* 2^12 / (2^12 - 1) == 1.00024420024420 */
  MAD_F(0x10008004),  /* 2^13 / (2^13 - 1) == 1.00012208521548 */
  MAD_F(0x10004001),  /* 2^14 / (2^14 - 1) == 1.00006103888177 */
  MAD_F(0x10002000)   /* 2^15 / (2^15 - 1) == 1.00003051850948 */
};

/*
 * NAME:	I_sample()
 * DESCRIPTION:	decode one requantized Layer I sample from a bitstream
 */
static
mad_fixed_t I_sample(struct mad_bitptr *ptr, unsigned int nb)
{
  mad_fixed_t sample;

  sample = mad_bit_read(ptr, nb);

  /* invert most significant bit, extend sign, then scale to fixed format */

  sample ^= 1 << (nb - 1);
  sample |= -(sample & (1 << (nb - 1)));

  sample <<= MAD_F_FRACBITS - (nb - 1);

  /* requantize the sample */

  /* s'' = (2^nb / (2^nb - 1)) * (s''' + 2^(-nb + 1)) */

  sample += MAD_F_ONE >> (nb - 1);

  return mad_f_mul(sample, linear_table[nb - 2]);

  /* s' = factor * s'' */
  /* (to be performed by caller) */
}

/*
 * NAME:	layer->I()
 * DESCRIPTION:	decode a single Layer I frame
 */
int mad_layer_I(struct mad_stream *stream, struct mad_frame *frame)
{
  struct mad_header *header = &frame->header;
  unsigned int nch, bound, ch, s, sb, nb;
  unsigned char allocation[2][32], scalefactor[2][32];

  nch = MAD_NCHANNELS(header);

  bound = 32;
  if (header->mode == MAD_MODE_JOINT_STEREO) {
    header->flags |= MAD_FLAG_I_STEREO;
    bound = 4 + header->mode_extension * 4;
  }

  /* check CRC word */

  if (header->flags & MAD_FLAG_PROTECTION) {
    header->crc_check =
      mad_bit_crc(stream->ptr, 4 * (bound * nch + (32 - bound)),
		  header->crc_check);

    if (header->crc_check != header->crc_target &&
	!(frame->options & MAD_OPTION_IGNORECRC)) {
      stream->error = MAD_ERROR_BADCRC;
      return -1;
    }
  }

  /* decode bit allocations */

  for (sb = 0; sb < bound; ++sb) {
    for (ch = 0; ch < nch; ++ch) {
      nb = mad_bit_read(&stream->ptr, 4);

      if (nb == 15) {
	stream->error = MAD_ERROR_BADBITALLOC;
	return -1;
      }

      allocation[ch][sb] = nb ? nb + 1 : 0;
    }
  }

  for (sb = bound; sb < 32; ++sb) {
    nb = mad_bit_read(&stream->ptr, 4);

    if (nb == 15) {
      stream->error = MAD_ERROR_BADBITALLOC;
      return -1;
    }

    allocation[0][sb] =
    allocation[1][sb] = nb ? nb + 1 : 0;
  }

  /* decode scalefactors */

  for (sb = 0; sb < 32; ++sb) {
    for (ch = 0; ch < nch; ++ch) {
      if (allocation[ch][sb]) {
	scalefactor[ch][sb] = mad_bit_read(&stream->ptr, 6);

# if defined(OPT_STRICT)
	/*
	 * Scalefactor index 63 does not appear in Table B.1 of
	 * ISO/IEC 11172-3. Nonetheless, other implementations accept it,
	 * so we only reject it if OPT_STRICT is defined.
	 */
	if (scalefactor[ch][sb] == 63) {
	  stream->error = MAD_ERROR_BADSCALEFACTOR;
	  return -1;
	}
# endif
      }
    }
  }

  /* decode samples */

  for (s = 0; s < 12; ++s) {
    for (sb = 0; sb < bound; ++sb) {
      for (ch = 0; ch < nch; ++ch) {
	nb = allocation[ch][sb];
	frame->sbsample[ch][s][sb] = nb ?
	  mad_f_mul(I_sample(&stream->ptr, nb),
		    sf_table[scalefactor[ch][sb]]) : 0;
      }
    }

    for (sb = bound; sb < 32; ++sb) {
      if ((nb = allocation[0][sb])) {
	mad_fixed_t sample;

	sample = I_sample(&stream->ptr, nb);

	for (ch = 0; ch < nch; ++ch) {
	  frame->sbsample[ch][s][sb] =
	    mad_f_mul(sample, sf_table[scalefactor[ch][sb]]);
	}
      }
      else {
	for (ch = 0; ch < nch; ++ch)
	  frame->sbsample[ch][s][sb] = 0;
      }
    }
  }

  return 0;
}

/* --- Layer II ------------------------------------------------------------ */

/* possible quantization per subband table */
static
struct {
  unsigned int sblimit;
  unsigned char  offsets[30];
}  sbquant_table[5] = {
  /* ISO/IEC 11172-3 Table B.2a */
  { 27, { 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 3, 3, 3, 3, 3,	/* 0 */
	  3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0 } },
  /* ISO/IEC 11172-3 Table B.2b */
  { 30, { 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 3, 3, 3, 3, 3,	/* 1 */
	  3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0 } },
  /* ISO/IEC 11172-3 Table B.2c */
  {  8, { 5, 5, 2, 2, 2, 2, 2, 2 } },				/* 2 */
  /* ISO/IEC 11172-3 Table B.2d */
  { 12, { 5, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },		/* 3 */
  /* ISO/IEC 13818-3 Table B.1 */
  { 30, { 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1,	/* 4 */
	  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } }
};

/* bit allocation table */
static
struct {
  unsigned short nbal;
  unsigned short offset;
} const bitalloc_table[8] = {
  { 2, 0 },  /* 0 */
  { 2, 3 },  /* 1 */
  { 3, 3 },  /* 2 */
  { 3, 1 },  /* 3 */
  { 4, 2 },  /* 4 */
  { 4, 3 },  /* 5 */
  { 4, 4 },  /* 6 */
  { 4, 5 }   /* 7 */
};

/* offsets into quantization class table */
static
unsigned char const offset_table[6][15] = {
  { 0, 1, 16                                             },  /* 0 */
  { 0, 1,  2, 3, 4, 5, 16                                },  /* 1 */
  { 0, 1,  2, 3, 4, 5,  6, 7,  8,  9, 10, 11, 12, 13, 14 },  /* 2 */
  { 0, 1,  3, 4, 5, 6,  7, 8,  9, 10, 11, 12, 13, 14, 15 },  /* 3 */
  { 0, 1,  2, 3, 4, 5,  6, 7,  8,  9, 10, 11, 12, 13, 16 },  /* 4 */
  { 0, 2,  4, 5, 6, 7,  8, 9, 10, 11, 12, 13, 14, 15, 16 }   /* 5 */
};

/* quantization class table */
static
struct quantclass {
  unsigned short nlevels;
  unsigned char group;
  unsigned char bits;
  mad_fixed_t C;
  mad_fixed_t D;
} const qc_table[17] = {

/*
 * These are the Layer II classes of quantization.
 * The table is derived from Table B.4 of ISO/IEC 11172-3.
 */

  {     3, 2,  5,
    MAD_F(0x15555555) /* 1.33333333333 => 1.33333333209, e  0.00000000124 */,
    MAD_F(0x08000000) /* 0.50000000000 => 0.50000000000, e  0.00000000000 */ },
  {     5, 3,  7,
    MAD_F(0x1999999a) /* 1.60000000000 => 1.60000000149, e -0.00000000149 */,
    MAD_F(0x08000000) /* 0.50000000000 => 0.50000000000, e  0.00000000000 */ },
  {     7, 0,  3,
    MAD_F(0x12492492) /* 1.14285714286 => 1.14285714179, e  0.00000000107 */,
    MAD_F(0x04000000) /* 0.25000000000 => 0.25000000000, e  0.00000000000 */ },
  {     9, 4, 10,
    MAD_F(0x1c71c71c) /* 1.77777777777 => 1.77777777612, e  0.00000000165 */,
    MAD_F(0x08000000) /* 0.50000000000 => 0.50000000000, e  0.00000000000 */ },
  {    15, 0,  4,
    MAD_F(0x11111111) /* 1.06666666666 => 1.06666666642, e  0.00000000024 */,
    MAD_F(0x02000000) /* 0.12500000000 => 0.12500000000, e  0.00000000000 */ },
  {    31, 0,  5,
    MAD_F(0x10842108) /* 1.03225806452 => 1.03225806355, e  0.00000000097 */,
    MAD_F(0x01000000) /* 0.06250000000 => 0.06250000000, e  0.00000000000 */ },
  {    63, 0,  6,
    MAD_F(0x10410410) /* 1.01587301587 => 1.01587301493, e  0.00000000094 */,
    MAD_F(0x00800000) /* 0.03125000000 => 0.03125000000, e  0.00000000000 */ },
  {   127, 0,  7,
    MAD_F(0x10204081) /* 1.00787401575 => 1.00787401572, e  0.00000000003 */,
    MAD_F(0x00400000) /* 0.01562500000 => 0.01562500000, e  0.00000000000 */ },
  {   255, 0,  8,
    MAD_F(0x10101010) /* 1.00392156863 => 1.00392156839, e  0.00000000024 */,
    MAD_F(0x00200000) /* 0.00781250000 => 0.00781250000, e  0.00000000000 */ },
  {   511, 0,  9,
    MAD_F(0x10080402) /* 1.00195694716 => 1.00195694715, e  0.00000000001 */,
    MAD_F(0x00100000) /* 0.00390625000 => 0.00390625000, e  0.00000000000 */ },
  {  1023, 0, 10,
    MAD_F(0x10040100) /* 1.00097751711 => 1.00097751617, e  0.00000000094 */,
    MAD_F(0x00080000) /* 0.00195312500 => 0.00195312500, e  0.00000000000 */ },
  {  2047, 0, 11,
    MAD_F(0x10020040) /* 1.00048851979 => 1.00048851967, e  0.00000000012 */,
    MAD_F(0x00040000) /* 0.00097656250 => 0.00097656250, e  0.00000000000 */ },
  {  4095, 0, 12,
    MAD_F(0x10010010) /* 1.00024420024 => 1.00024420023, e  0.00000000001 */,
    MAD_F(0x00020000) /* 0.00048828125 => 0.00048828125, e  0.00000000000 */ },
  {  8191, 0, 13,
    MAD_F(0x10008004) /* 1.00012208522 => 1.00012208521, e  0.00000000001 */,
    MAD_F(0x00010000) /* 0.00024414063 => 0.00024414062, e  0.00000000000 */ },
  { 16383, 0, 14,
    MAD_F(0x10004001) /* 1.00006103888 => 1.00006103888, e -0.00000000000 */,
    MAD_F(0x00008000) /* 0.00012207031 => 0.00012207031, e -0.00000000000 */ },
  { 32767, 0, 15,
    MAD_F(0x10002000) /* 1.00003051851 => 1.00003051758, e  0.00000000093 */,
    MAD_F(0x00004000) /* 0.00006103516 => 0.00006103516, e  0.00000000000 */ },
  { 65535, 0, 16,
    MAD_F(0x10001000) /* 1.00001525902 => 1.00001525879, e  0.00000000023 */,
    MAD_F(0x00002000) /* 0.00003051758 => 0.00003051758, e  0.00000000000 */ }

};

/*
 * NAME:	II_samples()
 * DESCRIPTION:	decode three requantized Layer II samples from a bitstream
 */
static
void II_samples(struct mad_bitptr *ptr,
		struct quantclass const *quantclass,
		mad_fixed_t output[3])
{
  unsigned int nb, s, sample[3];

  if ((nb = quantclass->group)) {
    unsigned int c, nlevels;

    /* degrouping */
    c = mad_bit_read(ptr, quantclass->bits);
    nlevels = quantclass->nlevels;

    for (s = 0; s < 3; ++s) {
      sample[s] = c % nlevels;
      c /= nlevels;
    }
  }
  else {
    nb = quantclass->bits;

    for (s = 0; s < 3; ++s)
      sample[s] = mad_bit_read(ptr, nb);
  }

  for (s = 0; s < 3; ++s) {
    mad_fixed_t requantized;

    /* invert most significant bit, extend sign, then scale to fixed format */

    requantized  = sample[s] ^ (1 << (nb - 1));
    requantized |= -(requantized & (1 << (nb - 1)));

    requantized <<= MAD_F_FRACBITS - (nb - 1);

    /* requantize the sample */

    /* s'' = C * (s''' + D) */

    output[s] = mad_f_mul(requantized + quantclass->D, quantclass->C);

    /* s' = factor * s'' */
    /* (to be performed by caller) */
  }
}

/*
 * NAME:	layer->II()
 * DESCRIPTION:	decode a single Layer II frame
 */
int mad_layer_II(struct mad_stream *stream, struct mad_frame *frame)
{
  struct mad_header *header = &frame->header;
  struct mad_bitptr start;
  unsigned int index, sblimit, nbal, nch, bound, gr, ch, s, sb;
  unsigned char const *offsets;
  unsigned char allocation[2][32], scfsi[2][32], scalefactor[2][32][3];
  mad_fixed_t samples[3];

  nch = MAD_NCHANNELS(header);

  if (header->flags & MAD_FLAG_LSF_EXT)
    index = 4;
  else if (header->flags & MAD_FLAG_FREEFORMAT)
    goto freeformat;
  else {
    unsigned long bitrate_per_channel;

    bitrate_per_channel = header->bitrate;
    if (nch == 2) {
      bitrate_per_channel /= 2;

# if defined(OPT_STRICT)
      /*
       * ISO/IEC 11172-3 allows only single channel mode for 32, 48, 56, and
       * 80 kbps bitrates in Layer II, but some encoders ignore this
       * restriction. We enforce it if OPT_STRICT is defined.
       */
      if (bitrate_per_channel <= 28000 || bitrate_per_channel == 40000) {
	stream->error = MAD_ERROR_BADMODE;
	return -1;
      }
# endif
    }
    else {  /* nch == 1 */
      if (bitrate_per_channel > 192000) {
	/*
	 * ISO/IEC 11172-3 does not allow single channel mode for 224, 256,
	 * 320, or 384 kbps bitrates in Layer II.
	 */
	stream->error = MAD_ERROR_BADMODE;
	return -1;
      }
    }

    if (bitrate_per_channel <= 48000)
      index = (header->samplerate == 32000) ? 3 : 2;
    else if (bitrate_per_channel <= 80000)
      index = 0;
    else {
    freeformat:
      index = (header->samplerate == 48000) ? 0 : 1;
    }
  }

  sblimit = sbquant_table[index].sblimit;
  offsets = sbquant_table[index].offsets;

  bound = 32;
  if (header->mode == MAD_MODE_JOINT_STEREO) {
    header->flags |= MAD_FLAG_I_STEREO;
    bound = 4 + header->mode_extension * 4;
  }

  if (bound > sblimit)
    bound = sblimit;

  start = stream->ptr;

  /* decode bit allocations */

  for (sb = 0; sb < bound; ++sb) {
    nbal = bitalloc_table[offsets[sb]].nbal;

    for (ch = 0; ch < nch; ++ch)
      allocation[ch][sb] = mad_bit_read(&stream->ptr, nbal);
  }

  for (sb = bound; sb < sblimit; ++sb) {
    nbal = bitalloc_table[offsets[sb]].nbal;

    allocation[0][sb] =
    allocation[1][sb] = mad_bit_read(&stream->ptr, nbal);
  }

  /* decode scalefactor selection info */

  for (sb = 0; sb < sblimit; ++sb) {
    for (ch = 0; ch < nch; ++ch) {
      if (allocation[ch][sb])
	scfsi[ch][sb] = mad_bit_read(&stream->ptr, 2);
    }
  }

  /* check CRC word */

  if (header->flags & MAD_FLAG_PROTECTION) {
    header->crc_check =
      mad_bit_crc(start, mad_bit_length(&start, &stream->ptr),
		  header->crc_check);

    if (header->crc_check != header->crc_target &&
	!(frame->options & MAD_OPTION_IGNORECRC)) {
      stream->error = MAD_ERROR_BADCRC;
      return -1;
    }
  }

  /* decode scalefactors */

  for (sb = 0; sb < sblimit; ++sb) {
    for (ch = 0; ch < nch; ++ch) {
      if (allocation[ch][sb]) {
	scalefactor[ch][sb][0] = mad_bit_read(&stream->ptr, 6);

	switch (scfsi[ch][sb]) {
	case 2:
	  scalefactor[ch][sb][2] =
	  scalefactor[ch][sb][1] =
	  scalefactor[ch][sb][0];
	  break;

	case 0:
	  scalefactor[ch][sb][1] = mad_bit_read(&stream->ptr, 6);
	  /* fall through */

	case 1:
	case 3:
	  scalefactor[ch][sb][2] = mad_bit_read(&stream->ptr, 6);
	}

	if (scfsi[ch][sb] & 1)
	  scalefactor[ch][sb][1] = scalefactor[ch][sb][scfsi[ch][sb] - 1];

# if defined(OPT_STRICT)
	/*
	 * Scalefactor index 63 does not appear in Table B.1 of
	 * ISO/IEC 11172-3. Nonetheless, other implementations accept it,
	 * so we only reject it if OPT_STRICT is defined.
	 */
	if (scalefactor[ch][sb][0] == 63 ||
	    scalefactor[ch][sb][1] == 63 ||
	    scalefactor[ch][sb][2] == 63) {
	  stream->error = MAD_ERROR_BADSCALEFACTOR;
	  return -1;
	}
# endif
      }
    }
  }

  /* decode samples */

  for (gr = 0; gr < 12; ++gr) {
    for (sb = 0; sb < bound; ++sb) {
      for (ch = 0; ch < nch; ++ch) {
	if ((index = allocation[ch][sb])) {
	  index = offset_table[bitalloc_table[offsets[sb]].offset][index - 1];

	  II_samples(&stream->ptr, &qc_table[index], samples);

	  for (s = 0; s < 3; ++s) {
	    frame->sbsample[ch][3 * gr + s][sb] =
	      mad_f_mul(samples[s], sf_table[scalefactor[ch][sb][gr / 4]]);
	  }
	}
	else {
	  for (s = 0; s < 3; ++s)
	    frame->sbsample[ch][3 * gr + s][sb] = 0;
	}
      }
    }

    for (sb = bound; sb < sblimit; ++sb) {
      if ((index = allocation[0][sb])) {
	index = offset_table[bitalloc_table[offsets[sb]].offset][index - 1];

	II_samples(&stream->ptr, &qc_table[index], samples);

	for (ch = 0; ch < nch; ++ch) {
	  for (s = 0; s < 3; ++s) {
	    frame->sbsample[ch][3 * gr + s][sb] =
	      mad_f_mul(samples[s], sf_table[scalefactor[ch][sb][gr / 4]]);
	  }
	}
      }
      else {
	for (ch = 0; ch < nch; ++ch) {
	  for (s = 0; s < 3; ++s)
	    frame->sbsample[ch][3 * gr + s][sb] = 0;
	}
      }
    }

    for (ch = 0; ch < nch; ++ch) {
      for (s = 0; s < 3; ++s) {
	for (sb = sblimit; sb < 32; ++sb)
	  frame->sbsample[ch][3 * gr + s][sb] = 0;
      }
    }
  }

  return 0;
}


// ********************* layer12.c END *************************/


// ************************ timer.c *****************************/


mad_timer_t const mad_timer_zero = { 0, 0 };

/*
 * NAME:	timer->compare()
 * DESCRIPTION:	indicate relative order of two timers
 */
int mad_timer_compare(mad_timer_t timer1, mad_timer_t timer2)
{
  signed long diff;

  diff = timer1.seconds - timer2.seconds;
  if (diff < 0)
    return -1;
  else if (diff > 0)
    return +1;

  diff = timer1.fraction - timer2.fraction;
  if (diff < 0)
    return -1;
  else if (diff > 0)
    return +1;

  return 0;
}

/*
 * NAME:	timer->negate()
 * DESCRIPTION:	invert the sign of a timer
 */
void mad_timer_negate(mad_timer_t *timer)
{
  timer->seconds = -timer->seconds;

  if (timer->fraction) {
    timer->seconds -= 1;
    timer->fraction = MAD_TIMER_RESOLUTION - timer->fraction;
  }
}

/*
 * NAME:	timer->abs()
 * DESCRIPTION:	return the absolute value of a timer
 */
mad_timer_t mad_timer_abs(mad_timer_t timer)
{
  if (timer.seconds < 0)
    mad_timer_negate(&timer);

  return timer;
}

/*
 * NAME:	reduce_timer()
 * DESCRIPTION:	carry timer fraction into seconds
 */
static
void reduce_timer(mad_timer_t *timer)
{
  timer->seconds  += timer->fraction / MAD_TIMER_RESOLUTION;
  timer->fraction %= MAD_TIMER_RESOLUTION;
}

/*
 * NAME:	gcd()
 * DESCRIPTION:	compute greatest common denominator
 */
static
unsigned long gcd(unsigned long num1, unsigned long num2)
{
  unsigned long tmp;

  while (num2) {
    tmp  = num2;
    num2 = num1 % num2;
    num1 = tmp;
  }

  return num1;
}

/*
 * NAME:	reduce_rational()
 * DESCRIPTION:	convert rational expression to lowest terms
 */
static
void reduce_rational(unsigned long *numer, unsigned long *denom)
{
  unsigned long factor;

  factor = gcd(*numer, *denom);

/*  assert(factor != 0);*/

  *numer /= factor;
  *denom /= factor;
}

/*
 * NAME:	scale_rational()
 * DESCRIPTION:	solve numer/denom == ?/scale avoiding overflowing
 */
static
unsigned long scale_rational(unsigned long numer, unsigned long denom,
			     unsigned long scale)
{
  reduce_rational(&numer, &denom);
  reduce_rational(&scale, &denom);

 /* assert(denom != 0);*/

  if (denom < scale)
    return numer * (scale / denom) + numer * (scale % denom) / denom;
  if (denom < numer)
    return scale * (numer / denom) + scale * (numer % denom) / denom;

  return numer * scale / denom;
}

/*
 * NAME:	timer->set()
 * DESCRIPTION:	set timer to specific (positive) value
 */
void mad_timer_set(mad_timer_t *timer, unsigned long seconds,
		   unsigned long numer, unsigned long denom)
{
  timer->seconds = seconds;
  if (numer >= denom && denom > 0) {
    timer->seconds += numer / denom;
    numer %= denom;
  }

  switch (denom) {
  case 0:
  case 1:
    timer->fraction = 0;
    break;

  case MAD_TIMER_RESOLUTION:
    timer->fraction = numer;
    break;

  case 1000:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION /  1000);
    break;

  case 8000:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION /  8000);
    break;

  case 11025:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION / 11025);
    break;

  case 12000:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION / 12000);
    break;

  case 16000:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION / 16000);
    break;

  case 22050:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION / 22050);
    break;

  case 24000:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION / 24000);
    break;

  case 32000:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION / 32000);
    break;

  case 44100:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION / 44100);
    break;

  case 48000:
    timer->fraction = numer * (MAD_TIMER_RESOLUTION / 48000);
    break;

  default:
    timer->fraction = scale_rational(numer, denom, MAD_TIMER_RESOLUTION);
    break;
  }

  if (timer->fraction >= MAD_TIMER_RESOLUTION)
    reduce_timer(timer);
}

/*
 * NAME:	timer->add()
 * DESCRIPTION:	add one timer to another
 */
void mad_timer_add(mad_timer_t *timer, mad_timer_t incr)
{
  timer->seconds  += incr.seconds;
  timer->fraction += incr.fraction;

  if (timer->fraction >= MAD_TIMER_RESOLUTION)
    reduce_timer(timer);
}

/*
 * NAME:	timer->multiply()
 * DESCRIPTION:	multiply a timer by a scalar value
 */
void mad_timer_multiply(mad_timer_t *timer, signed long scalar)
{
  mad_timer_t addend;
  unsigned long factor;

  factor = scalar;
  if (scalar < 0) {
    factor = -scalar;
    mad_timer_negate(timer);
  }

  addend = *timer;
  *timer = mad_timer_zero;

  while (factor) {
    if (factor & 1)
      mad_timer_add(timer, addend);

    mad_timer_add(&addend, addend);
    factor >>= 1;
  }
}

/*
 * NAME:	timer->count()
 * DESCRIPTION:	return timer value in selected units
 */
signed long mad_timer_count(mad_timer_t timer, enum mad_units units)
{
  switch (units) {
  case MAD_UNITS_HOURS:
    return timer.seconds / 60 / 60;

  case MAD_UNITS_MINUTES:
    return timer.seconds / 60;

  case MAD_UNITS_SECONDS:
    return timer.seconds;

  case MAD_UNITS_DECISECONDS:
  case MAD_UNITS_CENTISECONDS:
  case MAD_UNITS_MILLISECONDS:

  case MAD_UNITS_8000_HZ:
  case MAD_UNITS_11025_HZ:
  case MAD_UNITS_12000_HZ:
  case MAD_UNITS_16000_HZ:
  case MAD_UNITS_22050_HZ:
  case MAD_UNITS_24000_HZ:
  case MAD_UNITS_32000_HZ:
  case MAD_UNITS_44100_HZ:
  case MAD_UNITS_48000_HZ:

  case MAD_UNITS_24_FPS:
  case MAD_UNITS_25_FPS:
  case MAD_UNITS_30_FPS:
  case MAD_UNITS_48_FPS:
  case MAD_UNITS_50_FPS:
  case MAD_UNITS_60_FPS:
  case MAD_UNITS_75_FPS:
    return timer.seconds * (signed long) units +
      (signed long) scale_rational(timer.fraction, MAD_TIMER_RESOLUTION,
				   units);

  case MAD_UNITS_23_976_FPS:
  case MAD_UNITS_24_975_FPS:
  case MAD_UNITS_29_97_FPS:
  case MAD_UNITS_47_952_FPS:
  case MAD_UNITS_49_95_FPS:
  case MAD_UNITS_59_94_FPS:
    return (mad_timer_count(timer, (enum mad_units) -units) + 1) * 1000 / 1001;
  }

  /* unsupported units */
  return 0;
}

/*
 * NAME:	timer->fraction()
 * DESCRIPTION:	return fractional part of timer in arbitrary terms
 */
unsigned long mad_timer_fraction(mad_timer_t timer, unsigned long denom)
{
  timer = mad_timer_abs(timer);

  switch (denom) {
  case 0:
    return timer.fraction ?
      MAD_TIMER_RESOLUTION / timer.fraction : MAD_TIMER_RESOLUTION + 1;

  case MAD_TIMER_RESOLUTION:
    return timer.fraction;

  default:
    return scale_rational(timer.fraction, MAD_TIMER_RESOLUTION, denom);
  }
}

/*
 * NAME:	timer->string()
 * DESCRIPTION:	write a string representation of a timer using a template
 */
void mad_timer_string(mad_timer_t timer,
		      char *dest, char const *format, enum mad_units units,
		      enum mad_units fracunits, unsigned long subparts)
{
  
  
  unsigned long hours, minutes, seconds, sub;
  unsigned int frac;

  timer = mad_timer_abs(timer);

  seconds = timer.seconds;
  frac = sub = 0;

  switch (fracunits) {
  case MAD_UNITS_HOURS:
  case MAD_UNITS_MINUTES:
  case MAD_UNITS_SECONDS:
    break;

  case MAD_UNITS_DECISECONDS:
  case MAD_UNITS_CENTISECONDS:
  case MAD_UNITS_MILLISECONDS:

  case MAD_UNITS_8000_HZ:
  case MAD_UNITS_11025_HZ:
  case MAD_UNITS_12000_HZ:
  case MAD_UNITS_16000_HZ:
  case MAD_UNITS_22050_HZ:
  case MAD_UNITS_24000_HZ:
  case MAD_UNITS_32000_HZ:
  case MAD_UNITS_44100_HZ:
  case MAD_UNITS_48000_HZ:

  case MAD_UNITS_24_FPS:
  case MAD_UNITS_25_FPS:
  case MAD_UNITS_30_FPS:
  case MAD_UNITS_48_FPS:
  case MAD_UNITS_50_FPS:
  case MAD_UNITS_60_FPS:
  case MAD_UNITS_75_FPS:
    {
      unsigned long denom;

      denom = MAD_TIMER_RESOLUTION / fracunits;

      frac = timer.fraction / denom;
      sub  = scale_rational(timer.fraction % denom, denom, subparts);
    }
    break;

  case MAD_UNITS_23_976_FPS:
  case MAD_UNITS_24_975_FPS:
  case MAD_UNITS_29_97_FPS:
  case MAD_UNITS_47_952_FPS:
  case MAD_UNITS_49_95_FPS:
  case MAD_UNITS_59_94_FPS:
    /* drop-frame encoding */
    /* N.B. this is only well-defined for MAD_UNITS_29_97_FPS */
    {
      unsigned long frame, cycle, d, m;

      frame = mad_timer_count(timer, fracunits);

      cycle = -fracunits * 60 * 10 - (10 - 1) * 2;

      d = frame / cycle;
      m = frame % cycle;
      frame += (10 - 1) * 2 * d;
      if (m > 2)
	frame += 2 * ((m - 2) / (cycle / 10));

      frac    = frame % -fracunits;
      seconds = frame / -fracunits;
    }
    break;
  }

  switch (units) {
  case MAD_UNITS_HOURS:
    minutes = seconds / 60;
    hours   = minutes / 60;

	sprintf(dest, format,
	    hours,
	    (unsigned int) (minutes % 60),
	    (unsigned int) (seconds % 60),
	    frac, sub);
    break;

  case MAD_UNITS_MINUTES:
    minutes = seconds / 60;

     sprintf(dest, format,
	    minutes,
	    (unsigned int) (seconds % 60),
	    frac, sub);
    break;

  case MAD_UNITS_SECONDS:
     sprintf(dest, format,
	    seconds,
	    frac, sub);
    break;

  case MAD_UNITS_23_976_FPS:
  case MAD_UNITS_24_975_FPS:
  case MAD_UNITS_29_97_FPS:
  case MAD_UNITS_47_952_FPS:
  case MAD_UNITS_49_95_FPS:
  case MAD_UNITS_59_94_FPS:
    if (fracunits < 0) {
      /* not yet implemented */
      sub = 0;
    }

    /* fall through */

  case MAD_UNITS_DECISECONDS:
  case MAD_UNITS_CENTISECONDS:
  case MAD_UNITS_MILLISECONDS:

  case MAD_UNITS_8000_HZ:
  case MAD_UNITS_11025_HZ:
  case MAD_UNITS_12000_HZ:
  case MAD_UNITS_16000_HZ:
  case MAD_UNITS_22050_HZ:
  case MAD_UNITS_24000_HZ:
  case MAD_UNITS_32000_HZ:
  case MAD_UNITS_44100_HZ:
  case MAD_UNITS_48000_HZ:

  case MAD_UNITS_24_FPS:
  case MAD_UNITS_25_FPS:
  case MAD_UNITS_30_FPS:
  case MAD_UNITS_48_FPS:
  case MAD_UNITS_50_FPS:
  case MAD_UNITS_60_FPS:
  case MAD_UNITS_75_FPS:
    sprintf(dest, format, mad_timer_count(timer, units), sub);
    break;
  }
}

// ************************ timer.c END ************************/

// ************************ stream.c ***************************/



/*
 * NAME:	stream->init()
 * DESCRIPTION:	initialize stream struct
 */
void mad_stream_init(struct mad_stream *stream)
{
  stream->buffer     = 0;
  stream->bufend     = 0;
  stream->skiplen    = 0;

  stream->sync       = 0;
  stream->freerate   = 0;

  stream->this_frame = 0;
  stream->next_frame = 0;
  mad_bit_init(&stream->ptr, 0);

  mad_bit_init(&stream->anc_ptr, 0);
  stream->anc_bitlen = 0;

  stream->main_data  = 0;
  stream->md_len     = 0;

  stream->options    = 0;
  stream->error      = MAD_ERROR_NONE;
}

/*
 * NAME:	stream->finish()
 * DESCRIPTION:	deallocate any dynamic memory associated with stream
 */
void mad_stream_finish(struct mad_stream *stream)
{
  if (stream->main_data) {
    LocalFree(stream->main_data);
    stream->main_data = 0;
  }

  mad_bit_finish(&stream->anc_ptr);
  mad_bit_finish(&stream->ptr);
}

/*
 * NAME:	stream->buffer()
 * DESCRIPTION:	set stream buffer pointers
 */
void mad_stream_buffer(struct mad_stream *stream,
		       unsigned char const *buffer, unsigned long length)
{
  stream->buffer = buffer;
  stream->bufend = buffer + length;

  stream->this_frame = buffer;
  stream->next_frame = buffer;

  stream->sync = 1;

  mad_bit_init(&stream->ptr, buffer);
}




/*
 * NAME:	stream->skip()
 * DESCRIPTION:	arrange to skip bytes before the next frame
 */
void mad_stream_skip(struct mad_stream *stream, unsigned long length)
{
  stream->skiplen += length;
  stream->md_len = 0;
}

/*
 * NAME:	stream->sync()
 * DESCRIPTION:	locate the next stream sync word
 */
int mad_stream_sync(struct mad_stream *stream)
{
  register unsigned char const *ptr, *end;

  ptr = mad_bit_nextbyte(&stream->ptr);
  end = stream->bufend;

  while (ptr < end - 1 &&
	 !(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0))
    ++ptr;

  if (end - ptr < MAD_BUFFER_GUARD)
    return -1;

  mad_bit_init(&stream->ptr, ptr);

  return 0;
}



/*
 * NAME:	stream->sync()
 * DESCRIPTION:	locate the next stream sync word
 */
int mad_stream_sync_back(struct mad_stream *stream)
{
  register unsigned char const *ptr, *start;

  ptr = mad_bit_nextbyte(&stream->ptr);
  start = stream->buffer;

  while (ptr >= start && !(ptr[0] == 0xff && (ptr[1] & 0xe0) == 0xe0))
    --ptr;

  if (ptr < start)
    return -1;

  mad_bit_init(&stream->ptr, ptr);

  return 0;
}





/*
 * NAME:	stream->errorstr()
 * DESCRIPTION:	return a string description of the current error condition
 */
char const *mad_stream_errorstr(struct mad_stream const *stream)
{
  switch (stream->error) {
  case MAD_ERROR_NONE:		 return "no error";

  case MAD_ERROR_BUFLEN:	 return "input buffer too small (or EOF)";
  case MAD_ERROR_BUFPTR:	 return "invalid (null) buffer pointer";

  case MAD_ERROR_NOMEM:		 return "not enough memory";

  case MAD_ERROR_LOSTSYNC:	 return "lost synchronization";
  case MAD_ERROR_BADLAYER:	 return "reserved header layer value";
  case MAD_ERROR_BADBITRATE:	 return "forbidden bitrate value";
  case MAD_ERROR_BADSAMPLERATE:	 return "reserved sample frequency value";
  case MAD_ERROR_BADEMPHASIS:	 return "reserved emphasis value";

  case MAD_ERROR_BADCRC:	 return "CRC check failed";
  case MAD_ERROR_BADBITALLOC:	 return "forbidden bit allocation value";
  case MAD_ERROR_BADSCALEFACTOR: return "bad scalefactor index";
  case MAD_ERROR_BADMODE:	 return "bad bitrate/mode combination";
  case MAD_ERROR_BADFRAMELEN:	 return "bad frame length";
  case MAD_ERROR_BADBIGVALUES:	 return "bad big_values count";
  case MAD_ERROR_BADBLOCKTYPE:	 return "reserved block_type";
  case MAD_ERROR_BADSCFSI:	 return "bad scalefactor selection info";
  case MAD_ERROR_BADDATAPTR:	 return "bad main_data_begin pointer";
  case MAD_ERROR_BADPART3LEN:	 return "bad audio data length";
  case MAD_ERROR_BADHUFFTABLE:	 return "bad Huffman table select";
  case MAD_ERROR_BADHUFFDATA:	 return "Huffman data overrun";
  case MAD_ERROR_BADSTEREO:	 return "incompatible block_type for JS";
  }

  return 0;
}


// *********************** stream.c END ************************/



// ********************** synth.c ******************************/


/*
 * NAME:	synth->init()
 * DESCRIPTION:	initialize synth struct
 */
void mad_synth_init(struct mad_synth *synth)
{
  mad_synth_mute(synth);

  synth->phase = 0;

  synth->pcm.samplerate = 0;
  synth->pcm.channels   = 0;
  synth->pcm.length     = 0;
}

/*
 * NAME:	synth->mute()
 * DESCRIPTION:	zero all polyphase filterbank values, resetting synthesis
 */
void mad_synth_mute(struct mad_synth *synth)
{
  unsigned int ch, s, v;

  for (ch = 0; ch < 2; ++ch) {
    for (s = 0; s < 16; ++s) {
      for (v = 0; v < 8; ++v) {
	synth->filter[ch][0][0][s][v] = synth->filter[ch][0][1][s][v] =
	synth->filter[ch][1][0][s][v] = synth->filter[ch][1][1][s][v] = 0;
      }
    }
  }
}

/*
 * An optional optimization called here the Subband Synthesis Optimization
 * (SSO) improves the performance of subband synthesis at the expense of
 * accuracy.
 *
 * The idea is to simplify 32x32->64-bit multiplication to 32x32->32 such
 * that extra scaling and rounding are not necessary. This often allows the
 * compiler to use faster 32-bit multiply-accumulate instructions instead of
 * explicit 64-bit multiply, shift, and add instructions.
 *
 * SSO works like this: a full 32x32->64-bit multiply of two mad_fixed_t
 * values requires the result to be right-shifted 28 bits to be properly
 * scaled to the same fixed-point format. Right shifts can be applied at any
 * time to either operand or to the result, so the optimization involves
 * careful placement of these shifts to minimize the loss of accuracy.
 *
 * First, a 14-bit shift is applied with rounding at compile-time to the D[]
 * table of coefficients for the subband synthesis window. This only loses 2
 * bits of accuracy because the lower 12 bits are always zero. A second
 * 12-bit shift occurs after the DCT calculation. This loses 12 bits of
 * accuracy. Finally, a third 2-bit shift occurs just before the sample is
 * saved in the PCM buffer. 14 + 12 + 2 == 28 bits.
 */

/* FPM_DEFAULT without OPT_SSO will actually lose accuracy and performance */

# if defined(FPM_DEFAULT) && !defined(OPT_SSO)
#  define OPT_SSO
# endif

/* second SSO shift, with rounding */

# if defined(OPT_SSO)
#  define SHIFT(x)  (((x) + (1L << 11)) >> 12)
# else
#  define SHIFT(x)  (x)
# endif

/* possible DCT speed optimization */

# if defined(OPT_SPEED) && defined(MAD_F_MLX)
#  define OPT_DCTO
#  define MUL(x, y)  \
    ({ mad_fixed64hi_t hi;  \
       mad_fixed64lo_t lo;  \
       MAD_F_MLX(hi, lo, (x), (y));  \
       hi << (32 - MAD_F_SCALEBITS - 3);  \
    })
# else
#  undef OPT_DCTO
#  define MUL(x, y)  mad_f_mul((x), (y))
# endif

/*
 * NAME:	dct32()
 * DESCRIPTION:	perform fast in[32]->out[32] DCT
 */
static
void dct32(mad_fixed_t const in[32], unsigned int slot,
	   mad_fixed_t lo[16][8], mad_fixed_t hi[16][8])
{
  mad_fixed_t t0,   t1,   t2,   t3,   t4,   t5,   t6,   t7;
  mad_fixed_t t8,   t9,   t10,  t11,  t12,  t13,  t14,  t15;
  mad_fixed_t t16,  t17,  t18,  t19,  t20,  t21,  t22,  t23;
  mad_fixed_t t24,  t25,  t26,  t27,  t28,  t29,  t30,  t31;
  mad_fixed_t t32,  t33,  t34,  t35,  t36,  t37,  t38,  t39;
  mad_fixed_t t40,  t41,  t42,  t43,  t44,  t45,  t46,  t47;
  mad_fixed_t t48,  t49,  t50,  t51,  t52,  t53,  t54,  t55;
  mad_fixed_t t56,  t57,  t58,  t59,  t60,  t61,  t62,  t63;
  mad_fixed_t t64,  t65,  t66,  t67,  t68,  t69,  t70,  t71;
  mad_fixed_t t72,  t73,  t74,  t75,  t76,  t77,  t78,  t79;
  mad_fixed_t t80,  t81,  t82,  t83,  t84,  t85,  t86,  t87;
  mad_fixed_t t88,  t89,  t90,  t91,  t92,  t93,  t94,  t95;
  mad_fixed_t t96,  t97,  t98,  t99,  t100, t101, t102, t103;
  mad_fixed_t t104, t105, t106, t107, t108, t109, t110, t111;
  mad_fixed_t t112, t113, t114, t115, t116, t117, t118, t119;
  mad_fixed_t t120, t121, t122, t123, t124, t125, t126, t127;
  mad_fixed_t t128, t129, t130, t131, t132, t133, t134, t135;
  mad_fixed_t t136, t137, t138, t139, t140, t141, t142, t143;
  mad_fixed_t t144, t145, t146, t147, t148, t149, t150, t151;
  mad_fixed_t t152, t153, t154, t155, t156, t157, t158, t159;
  mad_fixed_t t160, t161, t162, t163, t164, t165, t166, t167;
  mad_fixed_t t168, t169, t170, t171, t172, t173, t174, t175;
  mad_fixed_t t176;

  /* costab[i] = cos(PI / (2 * 32) * i) */

# if defined(OPT_DCTO)
#  define costab1	MAD_F(0x7fd8878e)
#  define costab2	MAD_F(0x7f62368f)
#  define costab3	MAD_F(0x7e9d55fc)
#  define costab4	MAD_F(0x7d8a5f40)
#  define costab5	MAD_F(0x7c29fbee)
#  define costab6	MAD_F(0x7a7d055b)
#  define costab7	MAD_F(0x78848414)
#  define costab8	MAD_F(0x7641af3d)
#  define costab9	MAD_F(0x73b5ebd1)
#  define costab10	MAD_F(0x70e2cbc6)
#  define costab11	MAD_F(0x6dca0d14)
#  define costab12	MAD_F(0x6a6d98a4)
#  define costab13	MAD_F(0x66cf8120)
#  define costab14	MAD_F(0x62f201ac)
#  define costab15	MAD_F(0x5ed77c8a)
#  define costab16	MAD_F(0x5a82799a)
#  define costab17	MAD_F(0x55f5a4d2)
#  define costab18	MAD_F(0x5133cc94)
#  define costab19	MAD_F(0x4c3fdff4)
#  define costab20	MAD_F(0x471cece7)
#  define costab21	MAD_F(0x41ce1e65)
#  define costab22	MAD_F(0x3c56ba70)
#  define costab23	MAD_F(0x36ba2014)
#  define costab24	MAD_F(0x30fbc54d)
#  define costab25	MAD_F(0x2b1f34eb)
#  define costab26	MAD_F(0x25280c5e)
#  define costab27	MAD_F(0x1f19f97b)
#  define costab28	MAD_F(0x18f8b83c)
#  define costab29	MAD_F(0x12c8106f)
#  define costab30	MAD_F(0x0c8bd35e)
#  define costab31	MAD_F(0x0647d97c)
# else
#  define costab1	MAD_F(0x0ffb10f2)  /* 0.998795456 */
#  define costab2	MAD_F(0x0fec46d2)  /* 0.995184727 */
#  define costab3	MAD_F(0x0fd3aac0)  /* 0.989176510 */
#  define costab4	MAD_F(0x0fb14be8)  /* 0.980785280 */
#  define costab5	MAD_F(0x0f853f7e)  /* 0.970031253 */
#  define costab6	MAD_F(0x0f4fa0ab)  /* 0.956940336 */
#  define costab7	MAD_F(0x0f109082)  /* 0.941544065 */
#  define costab8	MAD_F(0x0ec835e8)  /* 0.923879533 */
#  define costab9	MAD_F(0x0e76bd7a)  /* 0.903989293 */
#  define costab10	MAD_F(0x0e1c5979)  /* 0.881921264 */
#  define costab11	MAD_F(0x0db941a3)  /* 0.857728610 */
#  define costab12	MAD_F(0x0d4db315)  /* 0.831469612 */
#  define costab13	MAD_F(0x0cd9f024)  /* 0.803207531 */
#  define costab14	MAD_F(0x0c5e4036)  /* 0.773010453 */
#  define costab15	MAD_F(0x0bdaef91)  /* 0.740951125 */
#  define costab16	MAD_F(0x0b504f33)  /* 0.707106781 */
#  define costab17	MAD_F(0x0abeb49a)  /* 0.671558955 */
#  define costab18	MAD_F(0x0a267993)  /* 0.634393284 */
#  define costab19	MAD_F(0x0987fbfe)  /* 0.595699304 */
#  define costab20	MAD_F(0x08e39d9d)  /* 0.555570233 */
#  define costab21	MAD_F(0x0839c3cd)  /* 0.514102744 */
#  define costab22	MAD_F(0x078ad74e)  /* 0.471396737 */
#  define costab23	MAD_F(0x06d74402)  /* 0.427555093 */
#  define costab24	MAD_F(0x061f78aa)  /* 0.382683432 */
#  define costab25	MAD_F(0x0563e69d)  /* 0.336889853 */
#  define costab26	MAD_F(0x04a5018c)  /* 0.290284677 */
#  define costab27	MAD_F(0x03e33f2f)  /* 0.242980180 */
#  define costab28	MAD_F(0x031f1708)  /* 0.195090322 */
#  define costab29	MAD_F(0x0259020e)  /* 0.146730474 */
#  define costab30	MAD_F(0x01917a6c)  /* 0.098017140 */
#  define costab31	MAD_F(0x00c8fb30)  /* 0.049067674 */
# endif

  t0   = in[0]  + in[31];  t16  = MUL(in[0]  - in[31], costab1);
  t1   = in[15] + in[16];  t17  = MUL(in[15] - in[16], costab31);

  t41  = t16 + t17;
  t59  = MUL(t16 - t17, costab2);
  t33  = t0  + t1;
  t50  = MUL(t0  - t1,  costab2);

  t2   = in[7]  + in[24];  t18  = MUL(in[7]  - in[24], costab15);
  t3   = in[8]  + in[23];  t19  = MUL(in[8]  - in[23], costab17);

  t42  = t18 + t19;
  t60  = MUL(t18 - t19, costab30);
  t34  = t2  + t3;
  t51  = MUL(t2  - t3,  costab30);

  t4   = in[3]  + in[28];  t20  = MUL(in[3]  - in[28], costab7);
  t5   = in[12] + in[19];  t21  = MUL(in[12] - in[19], costab25);

  t43  = t20 + t21;
  t61  = MUL(t20 - t21, costab14);
  t35  = t4  + t5;
  t52  = MUL(t4  - t5,  costab14);

  t6   = in[4]  + in[27];  t22  = MUL(in[4]  - in[27], costab9);
  t7   = in[11] + in[20];  t23  = MUL(in[11] - in[20], costab23);

  t44  = t22 + t23;
  t62  = MUL(t22 - t23, costab18);
  t36  = t6  + t7;
  t53  = MUL(t6  - t7,  costab18);

  t8   = in[1]  + in[30];  t24  = MUL(in[1]  - in[30], costab3);
  t9   = in[14] + in[17];  t25  = MUL(in[14] - in[17], costab29);

  t45  = t24 + t25;
  t63  = MUL(t24 - t25, costab6);
  t37  = t8  + t9;
  t54  = MUL(t8  - t9,  costab6);

  t10  = in[6]  + in[25];  t26  = MUL(in[6]  - in[25], costab13);
  t11  = in[9]  + in[22];  t27  = MUL(in[9]  - in[22], costab19);

  t46  = t26 + t27;
  t64  = MUL(t26 - t27, costab26);
  t38  = t10 + t11;
  t55  = MUL(t10 - t11, costab26);

  t12  = in[2]  + in[29];  t28  = MUL(in[2]  - in[29], costab5);
  t13  = in[13] + in[18];  t29  = MUL(in[13] - in[18], costab27);

  t47  = t28 + t29;
  t65  = MUL(t28 - t29, costab10);
  t39  = t12 + t13;
  t56  = MUL(t12 - t13, costab10);

  t14  = in[5]  + in[26];  t30  = MUL(in[5]  - in[26], costab11);
  t15  = in[10] + in[21];  t31  = MUL(in[10] - in[21], costab21);

  t48  = t30 + t31;
  t66  = MUL(t30 - t31, costab22);
  t40  = t14 + t15;
  t57  = MUL(t14 - t15, costab22);

  t69  = t33 + t34;  t89  = MUL(t33 - t34, costab4);
  t70  = t35 + t36;  t90  = MUL(t35 - t36, costab28);
  t71  = t37 + t38;  t91  = MUL(t37 - t38, costab12);
  t72  = t39 + t40;  t92  = MUL(t39 - t40, costab20);
  t73  = t41 + t42;  t94  = MUL(t41 - t42, costab4);
  t74  = t43 + t44;  t95  = MUL(t43 - t44, costab28);
  t75  = t45 + t46;  t96  = MUL(t45 - t46, costab12);
  t76  = t47 + t48;  t97  = MUL(t47 - t48, costab20);

  t78  = t50 + t51;  t100 = MUL(t50 - t51, costab4);
  t79  = t52 + t53;  t101 = MUL(t52 - t53, costab28);
  t80  = t54 + t55;  t102 = MUL(t54 - t55, costab12);
  t81  = t56 + t57;  t103 = MUL(t56 - t57, costab20);

  t83  = t59 + t60;  t106 = MUL(t59 - t60, costab4);
  t84  = t61 + t62;  t107 = MUL(t61 - t62, costab28);
  t85  = t63 + t64;  t108 = MUL(t63 - t64, costab12);
  t86  = t65 + t66;  t109 = MUL(t65 - t66, costab20);

  t113 = t69  + t70;
  t114 = t71  + t72;

  /*  0 */ hi[15][slot] = SHIFT(t113 + t114);
  /* 16 */ lo[ 0][slot] = SHIFT(MUL(t113 - t114, costab16));

  t115 = t73  + t74;
  t116 = t75  + t76;

  t32  = t115 + t116;

  /*  1 */ hi[14][slot] = SHIFT(t32);

  t118 = t78  + t79;
  t119 = t80  + t81;

  t58  = t118 + t119;

  /*  2 */ hi[13][slot] = SHIFT(t58);

  t121 = t83  + t84;
  t122 = t85  + t86;

  t67  = t121 + t122;

  t49  = (t67 * 2) - t32;

  /*  3 */ hi[12][slot] = SHIFT(t49);

  t125 = t89  + t90;
  t126 = t91  + t92;

  t93  = t125 + t126;

  /*  4 */ hi[11][slot] = SHIFT(t93);

  t128 = t94  + t95;
  t129 = t96  + t97;

  t98  = t128 + t129;

  t68  = (t98 * 2) - t49;

  /*  5 */ hi[10][slot] = SHIFT(t68);

  t132 = t100 + t101;
  t133 = t102 + t103;

  t104 = t132 + t133;

  t82  = (t104 * 2) - t58;

  /*  6 */ hi[ 9][slot] = SHIFT(t82);

  t136 = t106 + t107;
  t137 = t108 + t109;

  t110 = t136 + t137;

  t87  = (t110 * 2) - t67;

  t77  = (t87 * 2) - t68;

  /*  7 */ hi[ 8][slot] = SHIFT(t77);

  t141 = MUL(t69 - t70, costab8);
  t142 = MUL(t71 - t72, costab24);
  t143 = t141 + t142;

  /*  8 */ hi[ 7][slot] = SHIFT(t143);
  /* 24 */ lo[ 8][slot] =
	     SHIFT((MUL(t141 - t142, costab16) * 2) - t143);

  t144 = MUL(t73 - t74, costab8);
  t145 = MUL(t75 - t76, costab24);
  t146 = t144 + t145;

  t88  = (t146 * 2) - t77;

  /*  9 */ hi[ 6][slot] = SHIFT(t88);

  t148 = MUL(t78 - t79, costab8);
  t149 = MUL(t80 - t81, costab24);
  t150 = t148 + t149;

  t105 = (t150 * 2) - t82;

  /* 10 */ hi[ 5][slot] = SHIFT(t105);

  t152 = MUL(t83 - t84, costab8);
  t153 = MUL(t85 - t86, costab24);
  t154 = t152 + t153;

  t111 = (t154 * 2) - t87;

  t99  = (t111 * 2) - t88;

  /* 11 */ hi[ 4][slot] = SHIFT(t99);

  t157 = MUL(t89 - t90, costab8);
  t158 = MUL(t91 - t92, costab24);
  t159 = t157 + t158;

  t127 = (t159 * 2) - t93;

  /* 12 */ hi[ 3][slot] = SHIFT(t127);

  t160 = (MUL(t125 - t126, costab16) * 2) - t127;

  /* 20 */ lo[ 4][slot] = SHIFT(t160);
  /* 28 */ lo[12][slot] =
	     SHIFT((((MUL(t157 - t158, costab16) * 2) - t159) * 2) - t160);

  t161 = MUL(t94 - t95, costab8);
  t162 = MUL(t96 - t97, costab24);
  t163 = t161 + t162;

  t130 = (t163 * 2) - t98;

  t112 = (t130 * 2) - t99;

  /* 13 */ hi[ 2][slot] = SHIFT(t112);

  t164 = (MUL(t128 - t129, costab16) * 2) - t130;

  t166 = MUL(t100 - t101, costab8);
  t167 = MUL(t102 - t103, costab24);
  t168 = t166 + t167;

  t134 = (t168 * 2) - t104;

  t120 = (t134 * 2) - t105;

  /* 14 */ hi[ 1][slot] = SHIFT(t120);

  t135 = (MUL(t118 - t119, costab16) * 2) - t120;

  /* 18 */ lo[ 2][slot] = SHIFT(t135);

  t169 = (MUL(t132 - t133, costab16) * 2) - t134;

  t151 = (t169 * 2) - t135;

  /* 22 */ lo[ 6][slot] = SHIFT(t151);

  t170 = (((MUL(t148 - t149, costab16) * 2) - t150) * 2) - t151;

  /* 26 */ lo[10][slot] = SHIFT(t170);
  /* 30 */ lo[14][slot] =
	     SHIFT((((((MUL(t166 - t167, costab16) * 2) -
		       t168) * 2) - t169) * 2) - t170);

  t171 = MUL(t106 - t107, costab8);
  t172 = MUL(t108 - t109, costab24);
  t173 = t171 + t172;

  t138 = (t173 * 2) - t110;

  t123 = (t138 * 2) - t111;

  t139 = (MUL(t121 - t122, costab16) * 2) - t123;

  t117 = (t123 * 2) - t112;

  /* 15 */ hi[ 0][slot] = SHIFT(t117);

  t124 = (MUL(t115 - t116, costab16) * 2) - t117;

  /* 17 */ lo[ 1][slot] = SHIFT(t124);

  t131 = (t139 * 2) - t124;

  /* 19 */ lo[ 3][slot] = SHIFT(t131);

  t140 = (t164 * 2) - t131;

  /* 21 */ lo[ 5][slot] = SHIFT(t140);

  t174 = (MUL(t136 - t137, costab16) * 2) - t138;

  t155 = (t174 * 2) - t139;

  t147 = (t155 * 2) - t140;

  /* 23 */ lo[ 7][slot] = SHIFT(t147);

  t156 = (((MUL(t144 - t145, costab16) * 2) - t146) * 2) - t147;

  /* 25 */ lo[ 9][slot] = SHIFT(t156);

  t175 = (((MUL(t152 - t153, costab16) * 2) - t154) * 2) - t155;

  t165 = (t175 * 2) - t156;

  /* 27 */ lo[11][slot] = SHIFT(t165);

  t176 = (((((MUL(t161 - t162, costab16) * 2) -
	     t163) * 2) - t164) * 2) - t165;

  /* 29 */ lo[13][slot] = SHIFT(t176);
  /* 31 */ lo[15][slot] =
	     SHIFT((((((((MUL(t171 - t172, costab16) * 2) -
			 t173) * 2) - t174) * 2) - t175) * 2) - t176);

  /*
   * Totals:
   *  80 multiplies
   *  80 additions
   * 119 subtractions
   *  49 shifts (not counting SSO)
   */
}

# undef MUL
# undef SHIFT

/* third SSO shift and/or D[] optimization preshift */

# if defined(OPT_SSO)
#  if MAD_F_FRACBITS != 28
#   error "MAD_F_FRACBITS must be 28 to use OPT_SSO"
#  endif
#  define ML0(hi, lo, x, y)	((lo)  = (x) * (y))
#  define MLA(hi, lo, x, y)	((lo) += (x) * (y))
#  define MLN(hi, lo)		((lo)  = -(lo))
#  define MLZ(hi, lo)		((void) (hi), (mad_fixed_t) (lo))
#  define SHIFT(x)		((x) >> 2)
#  define PRESHIFT(x)		((MAD_F(x) + (1L << 13)) >> 14)
# else
#  define ML0(hi, lo, x, y)	MAD_F_ML0((hi), (lo), (x), (y))
#  define MLA(hi, lo, x, y)	MAD_F_MLA((hi), (lo), (x), (y))
#  define MLN(hi, lo)		MAD_F_MLN((hi), (lo))
#  define MLZ(hi, lo)		MAD_F_MLZ((hi), (lo))
#  define SHIFT(x)		(x)
#  if defined(MAD_F_SCALEBITS)
#   undef  MAD_F_SCALEBITS
#   define MAD_F_SCALEBITS	(MAD_F_FRACBITS - 12)
#   define PRESHIFT(x)		(MAD_F(x) >> 12)
#  else
#   define PRESHIFT(x)		MAD_F(x)
#  endif
# endif

static
mad_fixed_t const D[17][32] = {

  {  PRESHIFT(0x00000000) /*  0.000000000 */,	/*  0 */
    -PRESHIFT(0x0001d000) /* -0.000442505 */,
     PRESHIFT(0x000d5000) /*  0.003250122 */,
    -PRESHIFT(0x001cb000) /* -0.007003784 */,
     PRESHIFT(0x007f5000) /*  0.031082153 */,
    -PRESHIFT(0x01421000) /* -0.078628540 */,
     PRESHIFT(0x019ae000) /*  0.100311279 */,
    -PRESHIFT(0x09271000) /* -0.572036743 */,
     PRESHIFT(0x1251e000) /*  1.144989014 */,
     PRESHIFT(0x09271000) /*  0.572036743 */,
     PRESHIFT(0x019ae000) /*  0.100311279 */,
     PRESHIFT(0x01421000) /*  0.078628540 */,
     PRESHIFT(0x007f5000) /*  0.031082153 */,
     PRESHIFT(0x001cb000) /*  0.007003784 */,
     PRESHIFT(0x000d5000) /*  0.003250122 */,
     PRESHIFT(0x0001d000) /*  0.000442505 */,

     PRESHIFT(0x00000000) /*  0.000000000 */,
    -PRESHIFT(0x0001d000) /* -0.000442505 */,
     PRESHIFT(0x000d5000) /*  0.003250122 */,
    -PRESHIFT(0x001cb000) /* -0.007003784 */,
     PRESHIFT(0x007f5000) /*  0.031082153 */,
    -PRESHIFT(0x01421000) /* -0.078628540 */,
     PRESHIFT(0x019ae000) /*  0.100311279 */,
    -PRESHIFT(0x09271000) /* -0.572036743 */,
     PRESHIFT(0x1251e000) /*  1.144989014 */,
     PRESHIFT(0x09271000) /*  0.572036743 */,
     PRESHIFT(0x019ae000) /*  0.100311279 */,
     PRESHIFT(0x01421000) /*  0.078628540 */,
     PRESHIFT(0x007f5000) /*  0.031082153 */,
     PRESHIFT(0x001cb000) /*  0.007003784 */,
     PRESHIFT(0x000d5000) /*  0.003250122 */,
     PRESHIFT(0x0001d000) /*  0.000442505 */ },

  { -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  1 */
    -PRESHIFT(0x0001f000) /* -0.000473022 */,
     PRESHIFT(0x000da000) /*  0.003326416 */,
    -PRESHIFT(0x00207000) /* -0.007919312 */,
     PRESHIFT(0x007d0000) /*  0.030517578 */,
    -PRESHIFT(0x0158d000) /* -0.084182739 */,
     PRESHIFT(0x01747000) /*  0.090927124 */,
    -PRESHIFT(0x099a8000) /* -0.600219727 */,
     PRESHIFT(0x124f0000) /*  1.144287109 */,
     PRESHIFT(0x08b38000) /*  0.543823242 */,
     PRESHIFT(0x01bde000) /*  0.108856201 */,
     PRESHIFT(0x012b4000) /*  0.073059082 */,
     PRESHIFT(0x0080f000) /*  0.031478882 */,
     PRESHIFT(0x00191000) /*  0.006118774 */,
     PRESHIFT(0x000d0000) /*  0.003173828 */,
     PRESHIFT(0x0001a000) /*  0.000396729 */,

    -PRESHIFT(0x00001000) /* -0.000015259 */,
    -PRESHIFT(0x0001f000) /* -0.000473022 */,
     PRESHIFT(0x000da000) /*  0.003326416 */,
    -PRESHIFT(0x00207000) /* -0.007919312 */,
     PRESHIFT(0x007d0000) /*  0.030517578 */,
    -PRESHIFT(0x0158d000) /* -0.084182739 */,
     PRESHIFT(0x01747000) /*  0.090927124 */,
    -PRESHIFT(0x099a8000) /* -0.600219727 */,
     PRESHIFT(0x124f0000) /*  1.144287109 */,
     PRESHIFT(0x08b38000) /*  0.543823242 */,
     PRESHIFT(0x01bde000) /*  0.108856201 */,
     PRESHIFT(0x012b4000) /*  0.073059082 */,
     PRESHIFT(0x0080f000) /*  0.031478882 */,
     PRESHIFT(0x00191000) /*  0.006118774 */,
     PRESHIFT(0x000d0000) /*  0.003173828 */,
     PRESHIFT(0x0001a000) /*  0.000396729 */ },

  { -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  2 */
    -PRESHIFT(0x00023000) /* -0.000534058 */,
     PRESHIFT(0x000de000) /*  0.003387451 */,
    -PRESHIFT(0x00245000) /* -0.008865356 */,
     PRESHIFT(0x007a0000) /*  0.029785156 */,
    -PRESHIFT(0x016f7000) /* -0.089706421 */,
     PRESHIFT(0x014a8000) /*  0.080688477 */,
    -PRESHIFT(0x0a0d8000) /* -0.628295898 */,
     PRESHIFT(0x12468000) /*  1.142211914 */,
     PRESHIFT(0x083ff000) /*  0.515609741 */,
     PRESHIFT(0x01dd8000) /*  0.116577148 */,
     PRESHIFT(0x01149000) /*  0.067520142 */,
     PRESHIFT(0x00820000) /*  0.031738281 */,
     PRESHIFT(0x0015b000) /*  0.005294800 */,
     PRESHIFT(0x000ca000) /*  0.003082275 */,
     PRESHIFT(0x00018000) /*  0.000366211 */,

    -PRESHIFT(0x00001000) /* -0.000015259 */,
    -PRESHIFT(0x00023000) /* -0.000534058 */,
     PRESHIFT(0x000de000) /*  0.003387451 */,
    -PRESHIFT(0x00245000) /* -0.008865356 */,
     PRESHIFT(0x007a0000) /*  0.029785156 */,
    -PRESHIFT(0x016f7000) /* -0.089706421 */,
     PRESHIFT(0x014a8000) /*  0.080688477 */,
    -PRESHIFT(0x0a0d8000) /* -0.628295898 */,
     PRESHIFT(0x12468000) /*  1.142211914 */,
     PRESHIFT(0x083ff000) /*  0.515609741 */,
     PRESHIFT(0x01dd8000) /*  0.116577148 */,
     PRESHIFT(0x01149000) /*  0.067520142 */,
     PRESHIFT(0x00820000) /*  0.031738281 */,
     PRESHIFT(0x0015b000) /*  0.005294800 */,
     PRESHIFT(0x000ca000) /*  0.003082275 */,
     PRESHIFT(0x00018000) /*  0.000366211 */ },

  { -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  3 */
    -PRESHIFT(0x00026000) /* -0.000579834 */,
     PRESHIFT(0x000e1000) /*  0.003433228 */,
    -PRESHIFT(0x00285000) /* -0.009841919 */,
     PRESHIFT(0x00765000) /*  0.028884888 */,
    -PRESHIFT(0x0185d000) /* -0.095169067 */,
     PRESHIFT(0x011d1000) /*  0.069595337 */,
    -PRESHIFT(0x0a7fe000) /* -0.656219482 */,
     PRESHIFT(0x12386000) /*  1.138763428 */,
     PRESHIFT(0x07ccb000) /*  0.487472534 */,
     PRESHIFT(0x01f9c000) /*  0.123474121 */,
     PRESHIFT(0x00fdf000) /*  0.061996460 */,
     PRESHIFT(0x00827000) /*  0.031845093 */,
     PRESHIFT(0x00126000) /*  0.004486084 */,
     PRESHIFT(0x000c4000) /*  0.002990723 */,
     PRESHIFT(0x00015000) /*  0.000320435 */,

    -PRESHIFT(0x00001000) /* -0.000015259 */,
    -PRESHIFT(0x00026000) /* -0.000579834 */,
     PRESHIFT(0x000e1000) /*  0.003433228 */,
    -PRESHIFT(0x00285000) /* -0.009841919 */,
     PRESHIFT(0x00765000) /*  0.028884888 */,
    -PRESHIFT(0x0185d000) /* -0.095169067 */,
     PRESHIFT(0x011d1000) /*  0.069595337 */,
    -PRESHIFT(0x0a7fe000) /* -0.656219482 */,
     PRESHIFT(0x12386000) /*  1.138763428 */,
     PRESHIFT(0x07ccb000) /*  0.487472534 */,
     PRESHIFT(0x01f9c000) /*  0.123474121 */,
     PRESHIFT(0x00fdf000) /*  0.061996460 */,
     PRESHIFT(0x00827000) /*  0.031845093 */,
     PRESHIFT(0x00126000) /*  0.004486084 */,
     PRESHIFT(0x000c4000) /*  0.002990723 */,
     PRESHIFT(0x00015000) /*  0.000320435 */ },

  { -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  4 */
    -PRESHIFT(0x00029000) /* -0.000625610 */,
     PRESHIFT(0x000e3000) /*  0.003463745 */,
    -PRESHIFT(0x002c7000) /* -0.010848999 */,
     PRESHIFT(0x0071e000) /*  0.027801514 */,
    -PRESHIFT(0x019bd000) /* -0.100540161 */,
     PRESHIFT(0x00ec0000) /*  0.057617187 */,
    -PRESHIFT(0x0af15000) /* -0.683914185 */,
     PRESHIFT(0x12249000) /*  1.133926392 */,
     PRESHIFT(0x075a0000) /*  0.459472656 */,
     PRESHIFT(0x0212c000) /*  0.129577637 */,
     PRESHIFT(0x00e79000) /*  0.056533813 */,
     PRESHIFT(0x00825000) /*  0.031814575 */,
     PRESHIFT(0x000f4000) /*  0.003723145 */,
     PRESHIFT(0x000be000) /*  0.002899170 */,
     PRESHIFT(0x00013000) /*  0.000289917 */,

    -PRESHIFT(0x00001000) /* -0.000015259 */,
    -PRESHIFT(0x00029000) /* -0.000625610 */,
     PRESHIFT(0x000e3000) /*  0.003463745 */,
    -PRESHIFT(0x002c7000) /* -0.010848999 */,
     PRESHIFT(0x0071e000) /*  0.027801514 */,
    -PRESHIFT(0x019bd000) /* -0.100540161 */,
     PRESHIFT(0x00ec0000) /*  0.057617187 */,
    -PRESHIFT(0x0af15000) /* -0.683914185 */,
     PRESHIFT(0x12249000) /*  1.133926392 */,
     PRESHIFT(0x075a0000) /*  0.459472656 */,
     PRESHIFT(0x0212c000) /*  0.129577637 */,
     PRESHIFT(0x00e79000) /*  0.056533813 */,
     PRESHIFT(0x00825000) /*  0.031814575 */,
     PRESHIFT(0x000f4000) /*  0.003723145 */,
     PRESHIFT(0x000be000) /*  0.002899170 */,
     PRESHIFT(0x00013000) /*  0.000289917 */ },

  { -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  5 */
    -PRESHIFT(0x0002d000) /* -0.000686646 */,
     PRESHIFT(0x000e4000) /*  0.003479004 */,
    -PRESHIFT(0x0030b000) /* -0.011886597 */,
     PRESHIFT(0x006cb000) /*  0.026535034 */,
    -PRESHIFT(0x01b17000) /* -0.105819702 */,
     PRESHIFT(0x00b77000) /*  0.044784546 */,
    -PRESHIFT(0x0b619000) /* -0.711318970 */,
     PRESHIFT(0x120b4000) /*  1.127746582 */,
     PRESHIFT(0x06e81000) /*  0.431655884 */,
     PRESHIFT(0x02288000) /*  0.134887695 */,
     PRESHIFT(0x00d17000) /*  0.051132202 */,
     PRESHIFT(0x0081b000) /*  0.031661987 */,
     PRESHIFT(0x000c5000) /*  0.003005981 */,
     PRESHIFT(0x000b7000) /*  0.002792358 */,
     PRESHIFT(0x00011000) /*  0.000259399 */,

    -PRESHIFT(0x00001000) /* -0.000015259 */,
    -PRESHIFT(0x0002d000) /* -0.000686646 */,
     PRESHIFT(0x000e4000) /*  0.003479004 */,
    -PRESHIFT(0x0030b000) /* -0.011886597 */,
     PRESHIFT(0x006cb000) /*  0.026535034 */,
    -PRESHIFT(0x01b17000) /* -0.105819702 */,
     PRESHIFT(0x00b77000) /*  0.044784546 */,
    -PRESHIFT(0x0b619000) /* -0.711318970 */,
     PRESHIFT(0x120b4000) /*  1.127746582 */,
     PRESHIFT(0x06e81000) /*  0.431655884 */,
     PRESHIFT(0x02288000) /*  0.134887695 */,
     PRESHIFT(0x00d17000) /*  0.051132202 */,
     PRESHIFT(0x0081b000) /*  0.031661987 */,
     PRESHIFT(0x000c5000) /*  0.003005981 */,
     PRESHIFT(0x000b7000) /*  0.002792358 */,
     PRESHIFT(0x00011000) /*  0.000259399 */ },

  { -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  6 */
    -PRESHIFT(0x00031000) /* -0.000747681 */,
     PRESHIFT(0x000e4000) /*  0.003479004 */,
    -PRESHIFT(0x00350000) /* -0.012939453 */,
     PRESHIFT(0x0066c000) /*  0.025085449 */,
    -PRESHIFT(0x01c67000) /* -0.110946655 */,
     PRESHIFT(0x007f5000) /*  0.031082153 */,
    -PRESHIFT(0x0bd06000) /* -0.738372803 */,
     PRESHIFT(0x11ec7000) /*  1.120223999 */,
     PRESHIFT(0x06772000) /*  0.404083252 */,
     PRESHIFT(0x023b3000) /*  0.139450073 */,
     PRESHIFT(0x00bbc000) /*  0.045837402 */,
     PRESHIFT(0x00809000) /*  0.031387329 */,
     PRESHIFT(0x00099000) /*  0.002334595 */,
     PRESHIFT(0x000b0000) /*  0.002685547 */,
     PRESHIFT(0x00010000) /*  0.000244141 */,

    -PRESHIFT(0x00001000) /* -0.000015259 */,
    -PRESHIFT(0x00031000) /* -0.000747681 */,
     PRESHIFT(0x000e4000) /*  0.003479004 */,
    -PRESHIFT(0x00350000) /* -0.012939453 */,
     PRESHIFT(0x0066c000) /*  0.025085449 */,
    -PRESHIFT(0x01c67000) /* -0.110946655 */,
     PRESHIFT(0x007f5000) /*  0.031082153 */,
    -PRESHIFT(0x0bd06000) /* -0.738372803 */,
     PRESHIFT(0x11ec7000) /*  1.120223999 */,
     PRESHIFT(0x06772000) /*  0.404083252 */,
     PRESHIFT(0x023b3000) /*  0.139450073 */,
     PRESHIFT(0x00bbc000) /*  0.045837402 */,
     PRESHIFT(0x00809000) /*  0.031387329 */,
     PRESHIFT(0x00099000) /*  0.002334595 */,
     PRESHIFT(0x000b0000) /*  0.002685547 */,
     PRESHIFT(0x00010000) /*  0.000244141 */ },

  { -PRESHIFT(0x00002000) /* -0.000030518 */,	/*  7 */
    -PRESHIFT(0x00035000) /* -0.000808716 */,
     PRESHIFT(0x000e3000) /*  0.003463745 */,
    -PRESHIFT(0x00397000) /* -0.014022827 */,
     PRESHIFT(0x005ff000) /*  0.023422241 */,
    -PRESHIFT(0x01dad000) /* -0.115921021 */,
     PRESHIFT(0x0043a000) /*  0.016510010 */,
    -PRESHIFT(0x0c3d9000) /* -0.765029907 */,
     PRESHIFT(0x11c83000) /*  1.111373901 */,
     PRESHIFT(0x06076000) /*  0.376800537 */,
     PRESHIFT(0x024ad000) /*  0.143264771 */,
     PRESHIFT(0x00a67000) /*  0.040634155 */,
     PRESHIFT(0x007f0000) /*  0.031005859 */,
     PRESHIFT(0x0006f000) /*  0.001693726 */,
     PRESHIFT(0x000a9000) /*  0.002578735 */,
     PRESHIFT(0x0000e000) /*  0.000213623 */,

    -PRESHIFT(0x00002000) /* -0.000030518 */,
    -PRESHIFT(0x00035000) /* -0.000808716 */,
     PRESHIFT(0x000e3000) /*  0.003463745 */,
    -PRESHIFT(0x00397000) /* -0.014022827 */,
     PRESHIFT(0x005ff000) /*  0.023422241 */,
    -PRESHIFT(0x01dad000) /* -0.115921021 */,
     PRESHIFT(0x0043a000) /*  0.016510010 */,
    -PRESHIFT(0x0c3d9000) /* -0.765029907 */,
     PRESHIFT(0x11c83000) /*  1.111373901 */,
     PRESHIFT(0x06076000) /*  0.376800537 */,
     PRESHIFT(0x024ad000) /*  0.143264771 */,
     PRESHIFT(0x00a67000) /*  0.040634155 */,
     PRESHIFT(0x007f0000) /*  0.031005859 */,
     PRESHIFT(0x0006f000) /*  0.001693726 */,
     PRESHIFT(0x000a9000) /*  0.002578735 */,
     PRESHIFT(0x0000e000) /*  0.000213623 */ },

  { -PRESHIFT(0x00002000) /* -0.000030518 */,	/*  8 */
    -PRESHIFT(0x0003a000) /* -0.000885010 */,
     PRESHIFT(0x000e0000) /*  0.003417969 */,
    -PRESHIFT(0x003df000) /* -0.015121460 */,
     PRESHIFT(0x00586000) /*  0.021575928 */,
    -PRESHIFT(0x01ee6000) /* -0.120697021 */,
     PRESHIFT(0x00046000) /*  0.001068115 */,
    -PRESHIFT(0x0ca8d000) /* -0.791213989 */,
     PRESHIFT(0x119e9000) /*  1.101211548 */,
     PRESHIFT(0x05991000) /*  0.349868774 */,
     PRESHIFT(0x02578000) /*  0.146362305 */,
     PRESHIFT(0x0091a000) /*  0.035552979 */,
     PRESHIFT(0x007d1000) /*  0.030532837 */,
     PRESHIFT(0x00048000) /*  0.001098633 */,
     PRESHIFT(0x000a1000) /*  0.002456665 */,
     PRESHIFT(0x0000d000) /*  0.000198364 */,

    -PRESHIFT(0x00002000) /* -0.000030518 */,
    -PRESHIFT(0x0003a000) /* -0.000885010 */,
     PRESHIFT(0x000e0000) /*  0.003417969 */,
    -PRESHIFT(0x003df000) /* -0.015121460 */,
     PRESHIFT(0x00586000) /*  0.021575928 */,
    -PRESHIFT(0x01ee6000) /* -0.120697021 */,
     PRESHIFT(0x00046000) /*  0.001068115 */,
    -PRESHIFT(0x0ca8d000) /* -0.791213989 */,
     PRESHIFT(0x119e9000) /*  1.101211548 */,
     PRESHIFT(0x05991000) /*  0.349868774 */,
     PRESHIFT(0x02578000) /*  0.146362305 */,
     PRESHIFT(0x0091a000) /*  0.035552979 */,
     PRESHIFT(0x007d1000) /*  0.030532837 */,
     PRESHIFT(0x00048000) /*  0.001098633 */,
     PRESHIFT(0x000a1000) /*  0.002456665 */,
     PRESHIFT(0x0000d000) /*  0.000198364 */ },

  { -PRESHIFT(0x00002000) /* -0.000030518 */,	/*  9 */
    -PRESHIFT(0x0003f000) /* -0.000961304 */,
     PRESHIFT(0x000dd000) /*  0.003372192 */,
    -PRESHIFT(0x00428000) /* -0.016235352 */,
     PRESHIFT(0x00500000) /*  0.019531250 */,
    -PRESHIFT(0x02011000) /* -0.125259399 */,
    -PRESHIFT(0x003e6000) /* -0.015228271 */,
    -PRESHIFT(0x0d11e000) /* -0.816864014 */,
     PRESHIFT(0x116fc000) /*  1.089782715 */,
     PRESHIFT(0x052c5000) /*  0.323318481 */,
     PRESHIFT(0x02616000) /*  0.148773193 */,
     PRESHIFT(0x007d6000) /*  0.030609131 */,
     PRESHIFT(0x007aa000) /*  0.029937744 */,
     PRESHIFT(0x00024000) /*  0.000549316 */,
     PRESHIFT(0x0009a000) /*  0.002349854 */,
     PRESHIFT(0x0000b000) /*  0.000167847 */,

    -PRESHIFT(0x00002000) /* -0.000030518 */,
    -PRESHIFT(0x0003f000) /* -0.000961304 */,
     PRESHIFT(0x000dd000) /*  0.003372192 */,
    -PRESHIFT(0x00428000) /* -0.016235352 */,
     PRESHIFT(0x00500000) /*  0.019531250 */,
    -PRESHIFT(0x02011000) /* -0.125259399 */,
    -PRESHIFT(0x003e6000) /* -0.015228271 */,
    -PRESHIFT(0x0d11e000) /* -0.816864014 */,
     PRESHIFT(0x116fc000) /*  1.089782715 */,
     PRESHIFT(0x052c5000) /*  0.323318481 */,
     PRESHIFT(0x02616000) /*  0.148773193 */,
     PRESHIFT(0x007d6000) /*  0.030609131 */,
     PRESHIFT(0x007aa000) /*  0.029937744 */,
     PRESHIFT(0x00024000) /*  0.000549316 */,
     PRESHIFT(0x0009a000) /*  0.002349854 */,
     PRESHIFT(0x0000b000) /*  0.000167847 */ },

  { -PRESHIFT(0x00002000) /* -0.000030518 */,	/* 10 */
    -PRESHIFT(0x00044000) /* -0.001037598 */,
     PRESHIFT(0x000d7000) /*  0.003280640 */,
    -PRESHIFT(0x00471000) /* -0.017349243 */,
     PRESHIFT(0x0046b000) /*  0.017257690 */,
    -PRESHIFT(0x0212b000) /* -0.129562378 */,
    -PRESHIFT(0x0084a000) /* -0.032379150 */,
    -PRESHIFT(0x0d78a000) /* -0.841949463 */,
     PRESHIFT(0x113be000) /*  1.077117920 */,
     PRESHIFT(0x04c16000) /*  0.297210693 */,
     PRESHIFT(0x02687000) /*  0.150497437 */,
     PRESHIFT(0x0069c000) /*  0.025817871 */,
     PRESHIFT(0x0077f000) /*  0.029281616 */,
     PRESHIFT(0x00002000) /*  0.000030518 */,
     PRESHIFT(0x00093000) /*  0.002243042 */,
     PRESHIFT(0x0000a000) /*  0.000152588 */,

    -PRESHIFT(0x00002000) /* -0.000030518 */,
    -PRESHIFT(0x00044000) /* -0.001037598 */,
     PRESHIFT(0x000d7000) /*  0.003280640 */,
    -PRESHIFT(0x00471000) /* -0.017349243 */,
     PRESHIFT(0x0046b000) /*  0.017257690 */,
    -PRESHIFT(0x0212b000) /* -0.129562378 */,
    -PRESHIFT(0x0084a000) /* -0.032379150 */,
    -PRESHIFT(0x0d78a000) /* -0.841949463 */,
     PRESHIFT(0x113be000) /*  1.077117920 */,
     PRESHIFT(0x04c16000) /*  0.297210693 */,
     PRESHIFT(0x02687000) /*  0.150497437 */,
     PRESHIFT(0x0069c000) /*  0.025817871 */,
     PRESHIFT(0x0077f000) /*  0.029281616 */,
     PRESHIFT(0x00002000) /*  0.000030518 */,
     PRESHIFT(0x00093000) /*  0.002243042 */,
     PRESHIFT(0x0000a000) /*  0.000152588 */ },

  { -PRESHIFT(0x00003000) /* -0.000045776 */,	/* 11 */
    -PRESHIFT(0x00049000) /* -0.001113892 */,
     PRESHIFT(0x000d0000) /*  0.003173828 */,
    -PRESHIFT(0x004ba000) /* -0.018463135 */,
     PRESHIFT(0x003ca000) /*  0.014801025 */,
    -PRESHIFT(0x02233000) /* -0.133590698 */,
    -PRESHIFT(0x00ce4000) /* -0.050354004 */,
    -PRESHIFT(0x0ddca000) /* -0.866363525 */,
     PRESHIFT(0x1102f000) /*  1.063217163 */,
     PRESHIFT(0x04587000) /*  0.271591187 */,
     PRESHIFT(0x026cf000) /*  0.151596069 */,
     PRESHIFT(0x0056c000) /*  0.021179199 */,
     PRESHIFT(0x0074e000) /*  0.028533936 */,
    -PRESHIFT(0x0001d000) /* -0.000442505 */,
     PRESHIFT(0x0008b000) /*  0.002120972 */,
     PRESHIFT(0x00009000) /*  0.000137329 */,

    -PRESHIFT(0x00003000) /* -0.000045776 */,
    -PRESHIFT(0x00049000) /* -0.001113892 */,
     PRESHIFT(0x000d0000) /*  0.003173828 */,
    -PRESHIFT(0x004ba000) /* -0.018463135 */,
     PRESHIFT(0x003ca000) /*  0.014801025 */,
    -PRESHIFT(0x02233000) /* -0.133590698 */,
    -PRESHIFT(0x00ce4000) /* -0.050354004 */,
    -PRESHIFT(0x0ddca000) /* -0.866363525 */,
     PRESHIFT(0x1102f000) /*  1.063217163 */,
     PRESHIFT(0x04587000) /*  0.271591187 */,
     PRESHIFT(0x026cf000) /*  0.151596069 */,
     PRESHIFT(0x0056c000) /*  0.021179199 */,
     PRESHIFT(0x0074e000) /*  0.028533936 */,
    -PRESHIFT(0x0001d000) /* -0.000442505 */,
     PRESHIFT(0x0008b000) /*  0.002120972 */,
     PRESHIFT(0x00009000) /*  0.000137329 */ },

  { -PRESHIFT(0x00003000) /* -0.000045776 */,	/* 12 */
    -PRESHIFT(0x0004f000) /* -0.001205444 */,
     PRESHIFT(0x000c8000) /*  0.003051758 */,
    -PRESHIFT(0x00503000) /* -0.019577026 */,
     PRESHIFT(0x0031a000) /*  0.012115479 */,
    -PRESHIFT(0x02326000) /* -0.137298584 */,
    -PRESHIFT(0x011b5000) /* -0.069168091 */,
    -PRESHIFT(0x0e3dd000) /* -0.890090942 */,
     PRESHIFT(0x10c54000) /*  1.048156738 */,
     PRESHIFT(0x03f1b000) /*  0.246505737 */,
     PRESHIFT(0x026ee000) /*  0.152069092 */,
     PRESHIFT(0x00447000) /*  0.016708374 */,
     PRESHIFT(0x00719000) /*  0.027725220 */,
    -PRESHIFT(0x00039000) /* -0.000869751 */,
     PRESHIFT(0x00084000) /*  0.002014160 */,
     PRESHIFT(0x00008000) /*  0.000122070 */,

    -PRESHIFT(0x00003000) /* -0.000045776 */,
    -PRESHIFT(0x0004f000) /* -0.001205444 */,
     PRESHIFT(0x000c8000) /*  0.003051758 */,
    -PRESHIFT(0x00503000) /* -0.019577026 */,
     PRESHIFT(0x0031a000) /*  0.012115479 */,
    -PRESHIFT(0x02326000) /* -0.137298584 */,
    -PRESHIFT(0x011b5000) /* -0.069168091 */,
    -PRESHIFT(0x0e3dd000) /* -0.890090942 */,
     PRESHIFT(0x10c54000) /*  1.048156738 */,
     PRESHIFT(0x03f1b000) /*  0.246505737 */,
     PRESHIFT(0x026ee000) /*  0.152069092 */,
     PRESHIFT(0x00447000) /*  0.016708374 */,
     PRESHIFT(0x00719000) /*  0.027725220 */,
    -PRESHIFT(0x00039000) /* -0.000869751 */,
     PRESHIFT(0x00084000) /*  0.002014160 */,
     PRESHIFT(0x00008000) /*  0.000122070 */ },

  { -PRESHIFT(0x00004000) /* -0.000061035 */,	/* 13 */
    -PRESHIFT(0x00055000) /* -0.001296997 */,
     PRESHIFT(0x000bd000) /*  0.002883911 */,
    -PRESHIFT(0x0054c000) /* -0.020690918 */,
     PRESHIFT(0x0025d000) /*  0.009231567 */,
    -PRESHIFT(0x02403000) /* -0.140670776 */,
    -PRESHIFT(0x016ba000) /* -0.088775635 */,
    -PRESHIFT(0x0e9be000) /* -0.913055420 */,
     PRESHIFT(0x1082d000) /*  1.031936646 */,
     PRESHIFT(0x038d4000) /*  0.221984863 */,
     PRESHIFT(0x026e7000) /*  0.151962280 */,
     PRESHIFT(0x0032e000) /*  0.012420654 */,
     PRESHIFT(0x006df000) /*  0.026840210 */,
    -PRESHIFT(0x00053000) /* -0.001266479 */,
     PRESHIFT(0x0007d000) /*  0.001907349 */,
     PRESHIFT(0x00007000) /*  0.000106812 */,

    -PRESHIFT(0x00004000) /* -0.000061035 */,
    -PRESHIFT(0x00055000) /* -0.001296997 */,
     PRESHIFT(0x000bd000) /*  0.002883911 */,
    -PRESHIFT(0x0054c000) /* -0.020690918 */,
     PRESHIFT(0x0025d000) /*  0.009231567 */,
    -PRESHIFT(0x02403000) /* -0.140670776 */,
    -PRESHIFT(0x016ba000) /* -0.088775635 */,
    -PRESHIFT(0x0e9be000) /* -0.913055420 */,
     PRESHIFT(0x1082d000) /*  1.031936646 */,
     PRESHIFT(0x038d4000) /*  0.221984863 */,
     PRESHIFT(0x026e7000) /*  0.151962280 */,
     PRESHIFT(0x0032e000) /*  0.012420654 */,
     PRESHIFT(0x006df000) /*  0.026840210 */,
    -PRESHIFT(0x00053000) /* -0.001266479 */,
     PRESHIFT(0x0007d000) /*  0.001907349 */,
     PRESHIFT(0x00007000) /*  0.000106812 */ },

  { -PRESHIFT(0x00004000) /* -0.000061035 */,	/* 14 */
    -PRESHIFT(0x0005b000) /* -0.001388550 */,
     PRESHIFT(0x000b1000) /*  0.002700806 */,
    -PRESHIFT(0x00594000) /* -0.021789551 */,
     PRESHIFT(0x00192000) /*  0.006134033 */,
    -PRESHIFT(0x024c8000) /* -0.143676758 */,
    -PRESHIFT(0x01bf2000) /* -0.109161377 */,
    -PRESHIFT(0x0ef69000) /* -0.935195923 */,
     PRESHIFT(0x103be000) /*  1.014617920 */,
     PRESHIFT(0x032b4000) /*  0.198059082 */,
     PRESHIFT(0x026bc000) /*  0.151306152 */,
     PRESHIFT(0x00221000) /*  0.008316040 */,
     PRESHIFT(0x006a2000) /*  0.025909424 */,
    -PRESHIFT(0x0006a000) /* -0.001617432 */,
     PRESHIFT(0x00075000) /*  0.001785278 */,
     PRESHIFT(0x00007000) /*  0.000106812 */,

    -PRESHIFT(0x00004000) /* -0.000061035 */,
    -PRESHIFT(0x0005b000) /* -0.001388550 */,
     PRESHIFT(0x000b1000) /*  0.002700806 */,
    -PRESHIFT(0x00594000) /* -0.021789551 */,
     PRESHIFT(0x00192000) /*  0.006134033 */,
    -PRESHIFT(0x024c8000) /* -0.143676758 */,
    -PRESHIFT(0x01bf2000) /* -0.109161377 */,
    -PRESHIFT(0x0ef69000) /* -0.935195923 */,
     PRESHIFT(0x103be000) /*  1.014617920 */,
     PRESHIFT(0x032b4000) /*  0.198059082 */,
     PRESHIFT(0x026bc000) /*  0.151306152 */,
     PRESHIFT(0x00221000) /*  0.008316040 */,
     PRESHIFT(0x006a2000) /*  0.025909424 */,
    -PRESHIFT(0x0006a000) /* -0.001617432 */,
     PRESHIFT(0x00075000) /*  0.001785278 */,
     PRESHIFT(0x00007000) /*  0.000106812 */ },

  { -PRESHIFT(0x00005000) /* -0.000076294 */,	/* 15 */
    -PRESHIFT(0x00061000) /* -0.001480103 */,
     PRESHIFT(0x000a3000) /*  0.002487183 */,
    -PRESHIFT(0x005da000) /* -0.022857666 */,
     PRESHIFT(0x000b9000) /*  0.002822876 */,
    -PRESHIFT(0x02571000) /* -0.146255493 */,
    -PRESHIFT(0x0215c000) /* -0.130310059 */,
    -PRESHIFT(0x0f4dc000) /* -0.956481934 */,
     PRESHIFT(0x0ff0a000) /*  0.996246338 */,
     PRESHIFT(0x02cbf000) /*  0.174789429 */,
     PRESHIFT(0x0266e000) /*  0.150115967 */,
     PRESHIFT(0x00120000) /*  0.004394531 */,
     PRESHIFT(0x00662000) /*  0.024932861 */,
    -PRESHIFT(0x0007f000) /* -0.001937866 */,
     PRESHIFT(0x0006f000) /*  0.001693726 */,
     PRESHIFT(0x00006000) /*  0.000091553 */,

    -PRESHIFT(0x00005000) /* -0.000076294 */,
    -PRESHIFT(0x00061000) /* -0.001480103 */,
     PRESHIFT(0x000a3000) /*  0.002487183 */,
    -PRESHIFT(0x005da000) /* -0.022857666 */,
     PRESHIFT(0x000b9000) /*  0.002822876 */,
    -PRESHIFT(0x02571000) /* -0.146255493 */,
    -PRESHIFT(0x0215c000) /* -0.130310059 */,
    -PRESHIFT(0x0f4dc000) /* -0.956481934 */,
     PRESHIFT(0x0ff0a000) /*  0.996246338 */,
     PRESHIFT(0x02cbf000) /*  0.174789429 */,
     PRESHIFT(0x0266e000) /*  0.150115967 */,
     PRESHIFT(0x00120000) /*  0.004394531 */,
     PRESHIFT(0x00662000) /*  0.024932861 */,
    -PRESHIFT(0x0007f000) /* -0.001937866 */,
     PRESHIFT(0x0006f000) /*  0.001693726 */,
     PRESHIFT(0x00006000) /*  0.000091553 */ },

  { -PRESHIFT(0x00005000) /* -0.000076294 */,	/* 16 */
    -PRESHIFT(0x00068000) /* -0.001586914 */,
     PRESHIFT(0x00092000) /*  0.002227783 */,
    -PRESHIFT(0x0061f000) /* -0.023910522 */,
    -PRESHIFT(0x0002d000) /* -0.000686646 */,
    -PRESHIFT(0x025ff000) /* -0.148422241 */,
    -PRESHIFT(0x026f7000) /* -0.152206421 */,
    -PRESHIFT(0x0fa13000) /* -0.976852417 */,
     PRESHIFT(0x0fa13000) /*  0.976852417 */,
     PRESHIFT(0x026f7000) /*  0.152206421 */,
     PRESHIFT(0x025ff000) /*  0.148422241 */,
     PRESHIFT(0x0002d000) /*  0.000686646 */,
     PRESHIFT(0x0061f000) /*  0.023910522 */,
    -PRESHIFT(0x00092000) /* -0.002227783 */,
     PRESHIFT(0x00068000) /*  0.001586914 */,
     PRESHIFT(0x00005000) /*  0.000076294 */,

    -PRESHIFT(0x00005000) /* -0.000076294 */,
    -PRESHIFT(0x00068000) /* -0.001586914 */,
     PRESHIFT(0x00092000) /*  0.002227783 */,
    -PRESHIFT(0x0061f000) /* -0.023910522 */,
    -PRESHIFT(0x0002d000) /* -0.000686646 */,
    -PRESHIFT(0x025ff000) /* -0.148422241 */,
    -PRESHIFT(0x026f7000) /* -0.152206421 */,
    -PRESHIFT(0x0fa13000) /* -0.976852417 */,
     PRESHIFT(0x0fa13000) /*  0.976852417 */,
     PRESHIFT(0x026f7000) /*  0.152206421 */,
     PRESHIFT(0x025ff000) /*  0.148422241 */,
     PRESHIFT(0x0002d000) /*  0.000686646 */,
     PRESHIFT(0x0061f000) /*  0.023910522 */,
    -PRESHIFT(0x00092000) /* -0.002227783 */,
     PRESHIFT(0x00068000) /*  0.001586914 */,
     PRESHIFT(0x00005000) /*  0.000076294 */ }

};

# if defined(ASO_SYNTH)
void synth_full(struct mad_synth *, struct mad_frame const *,
		unsigned int, unsigned int);
# else
/*
 * NAME:	synth->full()
 * DESCRIPTION:	perform full frequency PCM synthesis
 */
static
void synth_full(struct mad_synth *synth, struct mad_frame const *frame,
		unsigned int nch, unsigned int ns)
{
  unsigned int phase, ch, s, sb, pe, po;
  mad_fixed_t *pcm1, *pcm2, (*filter)[2][2][16][8];
  mad_fixed_t const (*sbsample)[36][32];
  register mad_fixed_t (*fe)[8], (*fx)[8], (*fo)[8];
  register mad_fixed_t const (*Dptr)[32], *ptr;
  register mad_fixed64hi_t hi;
  register mad_fixed64lo_t lo;

  for (ch = 0; ch < nch; ++ch) {
    sbsample = &frame->sbsample[ch];
    filter   = &synth->filter[ch];
    phase    = synth->phase;
    pcm1     = synth->pcm.samples[ch];

    for (s = 0; s < ns; ++s) {
      dct32((*sbsample)[s], phase >> 1,
	    (*filter)[0][phase & 1], (*filter)[1][phase & 1]);

      pe = phase & ~1;
      po = ((phase - 1) & 0xf) | 1;

      /* calculate 32 samples */

      fe = &(*filter)[0][ phase & 1][0];
      fx = &(*filter)[0][~phase & 1][0];
      fo = &(*filter)[1][~phase & 1][0];

      Dptr = &D[0];

      ptr = *Dptr + po;
      ML0(hi, lo, (*fx)[0], ptr[ 0]);
      MLA(hi, lo, (*fx)[1], ptr[14]);
      MLA(hi, lo, (*fx)[2], ptr[12]);
      MLA(hi, lo, (*fx)[3], ptr[10]);
      MLA(hi, lo, (*fx)[4], ptr[ 8]);
      MLA(hi, lo, (*fx)[5], ptr[ 6]);
      MLA(hi, lo, (*fx)[6], ptr[ 4]);
      MLA(hi, lo, (*fx)[7], ptr[ 2]);
      MLN(hi, lo);

      ptr = *Dptr + pe;
      MLA(hi, lo, (*fe)[0], ptr[ 0]);
      MLA(hi, lo, (*fe)[1], ptr[14]);
      MLA(hi, lo, (*fe)[2], ptr[12]);
      MLA(hi, lo, (*fe)[3], ptr[10]);
      MLA(hi, lo, (*fe)[4], ptr[ 8]);
      MLA(hi, lo, (*fe)[5], ptr[ 6]);
      MLA(hi, lo, (*fe)[6], ptr[ 4]);
      MLA(hi, lo, (*fe)[7], ptr[ 2]);

      *pcm1++ = SHIFT(MLZ(hi, lo));

      pcm2 = pcm1 + 30;

      for (sb = 1; sb < 16; ++sb) {
	++fe;
	++Dptr;

	/* D[32 - sb][i] == -D[sb][31 - i] */

	ptr = *Dptr + po;
	ML0(hi, lo, (*fo)[0], ptr[ 0]);
	MLA(hi, lo, (*fo)[1], ptr[14]);
	MLA(hi, lo, (*fo)[2], ptr[12]);
	MLA(hi, lo, (*fo)[3], ptr[10]);
	MLA(hi, lo, (*fo)[4], ptr[ 8]);
	MLA(hi, lo, (*fo)[5], ptr[ 6]);
	MLA(hi, lo, (*fo)[6], ptr[ 4]);
	MLA(hi, lo, (*fo)[7], ptr[ 2]);
	MLN(hi, lo);

	ptr = *Dptr + pe;
	MLA(hi, lo, (*fe)[7], ptr[ 2]);
	MLA(hi, lo, (*fe)[6], ptr[ 4]);
	MLA(hi, lo, (*fe)[5], ptr[ 6]);
	MLA(hi, lo, (*fe)[4], ptr[ 8]);
	MLA(hi, lo, (*fe)[3], ptr[10]);
	MLA(hi, lo, (*fe)[2], ptr[12]);
	MLA(hi, lo, (*fe)[1], ptr[14]);
	MLA(hi, lo, (*fe)[0], ptr[ 0]);

	*pcm1++ = SHIFT(MLZ(hi, lo));

	ptr = *Dptr - pe;
	ML0(hi, lo, (*fe)[0], ptr[31 - 16]);
	MLA(hi, lo, (*fe)[1], ptr[31 - 14]);
	MLA(hi, lo, (*fe)[2], ptr[31 - 12]);
	MLA(hi, lo, (*fe)[3], ptr[31 - 10]);
	MLA(hi, lo, (*fe)[4], ptr[31 -  8]);
	MLA(hi, lo, (*fe)[5], ptr[31 -  6]);
	MLA(hi, lo, (*fe)[6], ptr[31 -  4]);
	MLA(hi, lo, (*fe)[7], ptr[31 -  2]);

	ptr = *Dptr - po;
	MLA(hi, lo, (*fo)[7], ptr[31 -  2]);
	MLA(hi, lo, (*fo)[6], ptr[31 -  4]);
	MLA(hi, lo, (*fo)[5], ptr[31 -  6]);
	MLA(hi, lo, (*fo)[4], ptr[31 -  8]);
	MLA(hi, lo, (*fo)[3], ptr[31 - 10]);
	MLA(hi, lo, (*fo)[2], ptr[31 - 12]);
	MLA(hi, lo, (*fo)[1], ptr[31 - 14]);
	MLA(hi, lo, (*fo)[0], ptr[31 - 16]);

	*pcm2-- = SHIFT(MLZ(hi, lo));

	++fo;
      }

      ++Dptr;

      ptr = *Dptr + po;
      ML0(hi, lo, (*fo)[0], ptr[ 0]);
      MLA(hi, lo, (*fo)[1], ptr[14]);
      MLA(hi, lo, (*fo)[2], ptr[12]);
      MLA(hi, lo, (*fo)[3], ptr[10]);
      MLA(hi, lo, (*fo)[4], ptr[ 8]);
      MLA(hi, lo, (*fo)[5], ptr[ 6]);
      MLA(hi, lo, (*fo)[6], ptr[ 4]);
      MLA(hi, lo, (*fo)[7], ptr[ 2]);

      *pcm1 = SHIFT(-MLZ(hi, lo));
      pcm1 += 16;

      phase = (phase + 1) % 16;
    }
  }
}
# endif

/*
 * NAME:	synth->half()
 * DESCRIPTION:	perform half frequency PCM synthesis
 */
static
void synth_half(struct mad_synth *synth, struct mad_frame const *frame,
		unsigned int nch, unsigned int ns)
{
  unsigned int phase, ch, s, sb, pe, po;
  mad_fixed_t *pcm1, *pcm2, (*filter)[2][2][16][8];
  mad_fixed_t const (*sbsample)[36][32];
  register mad_fixed_t (*fe)[8], (*fx)[8], (*fo)[8];
  register mad_fixed_t const (*Dptr)[32], *ptr;
  register mad_fixed64hi_t hi;
  register mad_fixed64lo_t lo;

  for (ch = 0; ch < nch; ++ch) {
    sbsample = &frame->sbsample[ch];
    filter   = &synth->filter[ch];
    phase    = synth->phase;
    pcm1     = synth->pcm.samples[ch];

    for (s = 0; s < ns; ++s) {
      dct32((*sbsample)[s], phase >> 1,
	    (*filter)[0][phase & 1], (*filter)[1][phase & 1]);

      pe = phase & ~1;
      po = ((phase - 1) & 0xf) | 1;

      /* calculate 16 samples */

      fe = &(*filter)[0][ phase & 1][0];
      fx = &(*filter)[0][~phase & 1][0];
      fo = &(*filter)[1][~phase & 1][0];

      Dptr = &D[0];

      ptr = *Dptr + po;
      ML0(hi, lo, (*fx)[0], ptr[ 0]);
      MLA(hi, lo, (*fx)[1], ptr[14]);
      MLA(hi, lo, (*fx)[2], ptr[12]);
      MLA(hi, lo, (*fx)[3], ptr[10]);
      MLA(hi, lo, (*fx)[4], ptr[ 8]);
      MLA(hi, lo, (*fx)[5], ptr[ 6]);
      MLA(hi, lo, (*fx)[6], ptr[ 4]);
      MLA(hi, lo, (*fx)[7], ptr[ 2]);
      MLN(hi, lo);

      ptr = *Dptr + pe;
      MLA(hi, lo, (*fe)[0], ptr[ 0]);
      MLA(hi, lo, (*fe)[1], ptr[14]);
      MLA(hi, lo, (*fe)[2], ptr[12]);
      MLA(hi, lo, (*fe)[3], ptr[10]);
      MLA(hi, lo, (*fe)[4], ptr[ 8]);
      MLA(hi, lo, (*fe)[5], ptr[ 6]);
      MLA(hi, lo, (*fe)[6], ptr[ 4]);
      MLA(hi, lo, (*fe)[7], ptr[ 2]);

      *pcm1++ = SHIFT(MLZ(hi, lo));

      pcm2 = pcm1 + 14;

      for (sb = 1; sb < 16; ++sb) {
	++fe;
	++Dptr;

	/* D[32 - sb][i] == -D[sb][31 - i] */

	if (!(sb & 1)) {
	  ptr = *Dptr + po;
	  ML0(hi, lo, (*fo)[0], ptr[ 0]);
	  MLA(hi, lo, (*fo)[1], ptr[14]);
	  MLA(hi, lo, (*fo)[2], ptr[12]);
	  MLA(hi, lo, (*fo)[3], ptr[10]);
	  MLA(hi, lo, (*fo)[4], ptr[ 8]);
	  MLA(hi, lo, (*fo)[5], ptr[ 6]);
	  MLA(hi, lo, (*fo)[6], ptr[ 4]);
	  MLA(hi, lo, (*fo)[7], ptr[ 2]);
	  MLN(hi, lo);
 
	  ptr = *Dptr + pe;
	  MLA(hi, lo, (*fe)[7], ptr[ 2]);
	  MLA(hi, lo, (*fe)[6], ptr[ 4]);
	  MLA(hi, lo, (*fe)[5], ptr[ 6]);
	  MLA(hi, lo, (*fe)[4], ptr[ 8]);
	  MLA(hi, lo, (*fe)[3], ptr[10]);
	  MLA(hi, lo, (*fe)[2], ptr[12]);
	  MLA(hi, lo, (*fe)[1], ptr[14]);
	  MLA(hi, lo, (*fe)[0], ptr[ 0]);

	  *pcm1++ = SHIFT(MLZ(hi, lo));

	  ptr = *Dptr - po;
	  ML0(hi, lo, (*fo)[7], ptr[31 -  2]);
	  MLA(hi, lo, (*fo)[6], ptr[31 -  4]);
	  MLA(hi, lo, (*fo)[5], ptr[31 -  6]);
	  MLA(hi, lo, (*fo)[4], ptr[31 -  8]);
	  MLA(hi, lo, (*fo)[3], ptr[31 - 10]);
	  MLA(hi, lo, (*fo)[2], ptr[31 - 12]);
	  MLA(hi, lo, (*fo)[1], ptr[31 - 14]);
	  MLA(hi, lo, (*fo)[0], ptr[31 - 16]);

	  ptr = *Dptr - pe;
	  MLA(hi, lo, (*fe)[0], ptr[31 - 16]);
	  MLA(hi, lo, (*fe)[1], ptr[31 - 14]);
	  MLA(hi, lo, (*fe)[2], ptr[31 - 12]);
	  MLA(hi, lo, (*fe)[3], ptr[31 - 10]);
	  MLA(hi, lo, (*fe)[4], ptr[31 -  8]);
	  MLA(hi, lo, (*fe)[5], ptr[31 -  6]);
	  MLA(hi, lo, (*fe)[6], ptr[31 -  4]);
	  MLA(hi, lo, (*fe)[7], ptr[31 -  2]);

	  *pcm2-- = SHIFT(MLZ(hi, lo));
	}

	++fo;
      }

      ++Dptr;

      ptr = *Dptr + po;
      ML0(hi, lo, (*fo)[0], ptr[ 0]);
      MLA(hi, lo, (*fo)[1], ptr[14]);
      MLA(hi, lo, (*fo)[2], ptr[12]);
      MLA(hi, lo, (*fo)[3], ptr[10]);
      MLA(hi, lo, (*fo)[4], ptr[ 8]);
      MLA(hi, lo, (*fo)[5], ptr[ 6]);
      MLA(hi, lo, (*fo)[6], ptr[ 4]);
      MLA(hi, lo, (*fo)[7], ptr[ 2]);

      *pcm1 = SHIFT(-MLZ(hi, lo));
      pcm1 += 8;

      phase = (phase + 1) % 16;
    }
  }
}

/*
 * NAME:	synth->frame()
 * DESCRIPTION:	perform PCM synthesis of frame subband samples
 */
void mad_synth_frame(struct mad_synth *synth, struct mad_frame const *frame)
{
  unsigned int nch, ns;
  void (*synth_frame)(struct mad_synth *, struct mad_frame const *,
		      unsigned int, unsigned int);

  nch = MAD_NCHANNELS(&frame->header);
  ns  = MAD_NSBSAMPLES(&frame->header);

  synth->pcm.samplerate = frame->header.samplerate;
  synth->pcm.channels   = nch;
  synth->pcm.length     = 32 * ns;

  synth_frame = synth_full;

  if (frame->options & MAD_OPTION_HALFSAMPLERATE) {
    synth->pcm.samplerate /= 2;
    synth->pcm.length     /= 2;

    synth_frame = synth_half;
  }

  synth_frame(synth, frame, nch, ns);

  synth->phase = (synth->phase + ns) % 16;
}



