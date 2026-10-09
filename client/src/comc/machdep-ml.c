#ifndef MACHDEP_MACHDEP_CONFIG_H
#define MACHDEP_MACHDEP_CONFIG_H
#define HAVE_BUILTIN_VA_LIST_DEF
#define HAVE_INTTYPES_H
#define HAVE_STDBOOL_H
#define HAVE_STDINT_H
#define HAVE_STDLIB_H
#define HAVE_WCHAR_H
#define THREAD_IS_KEYWORD_DEF
#define TYPE_CHAR16_T "unsigned short"
#define TYPE_CHAR32_T "unsigned int"
#define TYPE_SIZE_T "unsigned long long"
#define TYPE_WCHAR_T "int"
#undef  UNDERSCORE_NAME_DEF
#endif
/*

   Copyright (c) 2001-2002,
    George C. Necula    <necula@cs.berkeley.edu>
    Scott McPeak        <smcpeak@cs.berkeley.edu>
    Wes Weimer          <weimer@cs.berkeley.edu>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   3. The names of the contributors may not be used to endorse or promote
   products derived from this software without specific prior written
   permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */


#include <stdio.h>
#include <string.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#else
typedef int bool;
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#define __STRICT_ANSI__

#ifdef THREAD_IS_KEYWORD_DEF
#define THREAD_IS_KEYWORD "true"
#else
#define THREAD_IS_KEYWORD "false"
#endif

#ifdef HAVE_BUILTIN_VA_LIST_DEF
#define HAVE_BUILTIN_VA_LIST "true"
#else
#define HAVE_BUILTIN_VA_LIST "false"
#endif

#if (defined _GNUC || defined __clang__)
#define VERSION __VERSION__
#define VERSION_MAJOR __GNUC__
#define VERSION_MINOR __GNUC_MINOR__
#else
#ifdef _MSC_VER
#define VERSION "Microsoft C"
#define VERSION_MAJOR (_MSC_VER / 100)
#define VERSION_MINOR (_MSC_VER % 100)
#else
#define VERSION "Unknown"
#define VERSION_MAJOR -1
#define VERSION_MINOR -1
#endif
#endif

#ifdef _MSC_VER
#define LONGLONG __int64
#define CONST_STRING_LITERALS "false"
#define __alignof(x) 0
#define UNDERSCORE_NAME "true"
#include <complex.h>
#else
#define LONGLONG long long
#ifdef UNDERSCORE_NAME_DEF
#define UNDERSCORE_NAME "true"
#else
#define UNDERSCORE_NAME "false"
#endif
#endif

/* BEGIN ADDED BY Collberg */
#define __STDC_WANT_IEC_60559_TYPES_EXT__ 1
#include<float.h>

/* Also, all the code related to Float16 below. */
/* END ADDED BY Collberg */

//#ifdef __is_identifier
//#if __is_identifier(_Float16) 
//#define HAS_FLOAT16 1
//#endif
//#endif

//#ifndef __is_identifier
//#ifdef FLT16_MAX
//#define HAS_FLOAT16 1
//#endif
//#endif

#ifndef HAS_FLOAT16
#define HAS_FLOAT16 0
#endif

char *underscore(char *s)
{
  char *copy = strdup(s);
  char *space = copy;

  while ((space = strchr(space, ' ')))
    *space++ = '_';

  return copy;
}

/* The type for the machine dependency structure is generated from the
   Makefile */
int main(int argc, char **argv)
{
  int env = argc == 2 && !strcmp(argv[1], "--env");
  int json = argc == 2 && !strcmp(argv[1], "--json");
  int little_endian, char_is_unsigned;
static struct {
  int sizeof_short;
  int sizeof_int;
  int sizeof_bool;
  int sizeof_long;
  int sizeof_longlong;
  int sizeof_ptr;
  int sizeof_float;
  int sizeof_float32x;
  int sizeof_float64x;
  int sizeof_float32;
  int sizeof_float64;
  int sizeof_double;
  int sizeof_longdouble;
  int sizeof_float16;
  int sizeof_float128;
  int sizeof_floatcomplex;
  int sizeof_doublecomplex;
  int sizeof_longdoublecomplex;
  int sizeof_float128complex;
  int sizeof_int128;
  int sizeof_void;
  int sizeof_fun;
  int alignof_short; 
  int alignof_int; 
  int alignof_long;
  int alignof_ptr;
  int alignof_enum;
  int alignof_float;
  int alignof_float32x;
  int alignof_float64x;
  int alignof_float32;
  int alignof_float64;
  int alignof_double;
  int alignof_longdouble;
  int alignof_float16;
  int alignof_float128;
  int alignof_floatcomplex;
  int alignof_doublecomplex;
  int alignof_longdoublecomplex;
  int alignof_float128complex;
  int alignof_fun;
  int alignof_str;
  int alignof_aligned;
  int alignof_int128;
  int alignof_longlong;
  int alignof_bool;
} dep = {0};
 
  // The alignment of a short
  {
    struct shortstruct {
      char c;
      short s;
    };
    dep.alignof_short = (intptr_t)(&((struct shortstruct*)0)->s);
  }

  // The alignment of an int
  {
    struct intstruct {
      char c;
      int i;
    };
    dep.alignof_int = (intptr_t)(&((struct intstruct*)0)->i);
  }

  // The alignment of a bool
  {
    struct boolstruct {
      char c;
      bool b;
    };
    dep.alignof_bool = (intptr_t)(&((struct boolstruct*)0)->b);
  }

  // The alignment of a long
  {
    struct longstruct {
      char c;
      long l;
    };
    dep.alignof_long = (intptr_t)(&((struct longstruct*)0)->l);
  }

  // The alignment of long long
  {
    struct longlong {
      char c;
      LONGLONG ll;
    };
    dep.alignof_longlong = (intptr_t)(&((struct longlong*)0)->ll);
  }

  // The alignment of a ptr
  {
    struct ptrstruct {
      char c;
      int * p;
    };
    dep.alignof_ptr = (intptr_t)(&((struct ptrstruct*)0)->p);
  }

  // The alignment of an enum
  {
    struct enumstruct {
      char c;
      enum e2 { THREE, FOUR, FIVE } e;
    };
    dep.alignof_enum = (intptr_t)(&((struct enumstruct*)0)->e);
  }

  // The alignment of a float
  {
    struct floatstruct {
      char c;
      float f;
    };
    dep.alignof_float = (intptr_t)(&((struct floatstruct*)0)->f);
  }

#if (defined(__HAS_FLOAT32X) && __HAS_FLOAT32X) || (!defined(__HAS_FLOAT32X) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__)
  // The alignment of a _Float32x
  {
    struct floatstruct {
      char c;
      _Float32x f;
    };
    dep.alignof_float32x = (intptr_t)(&((struct floatstruct*)0)->f);
  }
#endif

#if (defined(__HAS_FLOAT64X) && __HAS_FLOAT64X) || (!defined(__HAS_FLOAT64X) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_DFP__)
  // The alignment of a _Float64x
  {
    struct floatstruct {
      char c;
      _Float64x f;
    };
    dep.alignof_float64x = (intptr_t)(&((struct floatstruct*)0)->f);
  }
#endif

#if (defined(__HAS_FLOAT32) && __HAS_FLOAT32) || (!defined(__HAS_FLOAT32) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__)
  // The alignment of a _Float32x
  {
    struct floatstruct {
      char c;
      _Float32 f;
    };
    dep.alignof_float32 = (intptr_t)(&((struct floatstruct*)0)->f);
  }
#endif

#if (defined(__HAS_FLOAT64) && __HAS_FLOAT64) || (!defined(__HAS_FLOAT64) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__)
  // The alignment of a _Float64x
  {
    struct floatstruct {
      char c;
      _Float64 f;
    };
    dep.alignof_float64 = (intptr_t)(&((struct floatstruct*)0)->f);
  }
#endif

  // The alignment of double
  {
    struct s1 {
      char c;
      double d;
    };
    dep.alignof_double = (intptr_t)(&((struct s1*)0)->d);
  }

  // The alignment of long  double
  {
    struct s1 {
      char c;
      long double ld;
    };
    dep.alignof_longdouble = (intptr_t)(&((struct s1*)0)->ld);
  }

#if HAS_FLOAT16
  // The alignment of float16
  {
    struct s1 {
      char c;
      _Float16 ld;
    };
    dep.alignof_float16 = (intptr_t)(&((struct s1*)0)->ld);
  }
#endif

#if (defined(__HAS_FLOAT128) && __HAS_FLOAT128) || (!defined(__HAS_FLOAT128) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__)
  // The alignment of float128
  {
    struct s1 {
      char c;
      _Float128 ld;
    };
    dep.alignof_float128 = (intptr_t)(&((struct s1*)0)->ld);
  }
#endif

  // The alignment of __int128. gcc and clang define __SIZEOF_INT128__ exactly
  // when the type exists (64-bit targets and wasm32; not i386, armv7, or MSVC),
  // so the fields stay 0 -- "unsupported" -- everywhere else.
#ifdef __SIZEOF_INT128__
  {
    struct s1 {
      char c;
      __int128 ld;
    };
    dep.alignof_int128 = (intptr_t)(&((struct s1*)0)->ld);
  }
#endif

  // The alignment of a float complex
  {
    struct floatstruct {
      char c;
#if _MSC_VER
      _Fcomplex f;
#else
      float _Complex f;
#endif
    };
    dep.alignof_floatcomplex = (intptr_t)(&((struct floatstruct*)0)->f);
  }

  // The alignment of double complex
  {
    struct s1 {
      char c;
#if _MSC_VER
      _Dcomplex d;
#else
      double _Complex d;
#endif
    };
    dep.alignof_doublecomplex = (intptr_t)(&((struct s1*)0)->d);
  }

  // The alignment of long  double complex
  {
    struct s1 {
      char c;
#if _MSC_VER
      _Lcomplex ld;
#else
      long double _Complex ld;
#endif
    };
    dep.alignof_longdoublecomplex = (intptr_t)(&((struct s1*)0)->ld);
  }

#if !_MSC_VER && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__
  // The alignment of float128 complex
  {
    struct s1 {
      char c;
      _Float128 _Complex ld;
    };
    dep.alignof_float128complex = (intptr_t)(&((struct s1*)0)->ld);
  }
#endif


  dep.alignof_str = __alignof("a string");
  dep.alignof_fun = __alignof(main);

  dep.sizeof_fun = 0;
#ifdef __GNUC__
  dep.sizeof_fun = sizeof(main);
#endif

  // The alignment of anything with __attribute__((aligned))
  dep.alignof_aligned = 0;
#ifdef __GNUC__
  {
    char __attribute__((aligned)) c;
    long double  __attribute__((aligned)) ld;
    if (__alignof(c) != __alignof(ld)) {
      fprintf(stderr, "__attribute__((aligned)) has a different effect on different types.  alignments may be computed incorrectly.\n");
    }
    dep.alignof_aligned = __alignof(c);
  }
#endif


  // Whether char is unsigned
  char_is_unsigned = ((char)0xff) > 0;

  // endianness
  {
    int e = 0x11223344;
    little_endian = (0x44 == *(char*)&e) ? 1 :
      ((0x11 == *(char*)&e) ? 0 : (exit(1), 0));
  }

dep.sizeof_short = (int)sizeof(short);
dep.sizeof_int = (int)sizeof(int);
dep.sizeof_bool = (int)sizeof(bool);
dep.sizeof_long = (int)sizeof(long);
dep.sizeof_longlong = (int)sizeof(long long);
dep.sizeof_ptr = (int)sizeof(int *);
dep.sizeof_float = (int)sizeof(float);
dep.sizeof_double = (int)sizeof(double);
dep.sizeof_longdouble = (int)sizeof(long double);
dep.sizeof_void = (int)sizeof(void);
dep.sizeof_fun = 
#if (defined(__HAS_FLOAT32X) && __HAS_FLOAT32X) || (!defined(__HAS_FLOAT32X) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__)
dep.sizeof_float32x = (int)sizeof(_Float32x);
#endif
#if (defined(__HAS_FLOAT64X) && __HAS_FLOAT64X) || (!defined(__HAS_FLOAT64X) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_DFP__)
dep.sizeof_float64x = (int)sizeof(_Float64x);
#endif
#if HAS_FLOAT16
dep.sizeof_float16 = (int)sizeof(_Float16);
#endif
#ifdef __SIZEOF_INT128__
dep.sizeof_int128 = (int)sizeof(__int128);
#endif
#if (defined(__HAS_FLOAT32) && __HAS_FLOAT32) || (!defined(__HAS_FLOAT32) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__)
dep.sizeof_float32 = (int)sizeof(_Float32);
#endif
#if (defined(__HAS_FLOAT64) && __HAS_FLOAT64) || (!defined(__HAS_FLOAT64) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__)
dep.sizeof_float64 = (int)sizeof(_Float64);
#endif
#if (defined(__HAS_FLOAT128) && __HAS_FLOAT128) || (!defined(__HAS_FLOAT128) && __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__)
dep.sizeof_float128 = (int)sizeof(_Float128);
#endif
#if _MSC_VER
dep.sizeof_floatcomplex = (int)sizeof(_Fcomplex);
dep.sizeof_doublecomplex = (int)sizeof(_Dcomplex);
dep.sizeof_longdoublecomplex = (int)sizeof(_Lcomplex);
#else
dep.sizeof_floatcomplex = (int)sizeof(float _Complex);
dep.sizeof_doublecomplex = (int)sizeof(double _Complex);
dep.sizeof_longdoublecomplex = (int)sizeof(long double _Complex);
#if __STDC_IEC_60559_TYPES__ && __STDC_IEC_60559_BFP__
dep.sizeof_float128complex = (int)sizeof(_Float128 _Complex);
#endif
#endif
 

  if (env)
    {
      fprintf(stderr, "Generating CIL_MACHINE machine dependency information string (for CIL)\n");
      printf("short=%d,%d int=%d,%d long=%d,%d long_long=%d,%d pointer=%d,%d "
	     "alignof_enum=%d float=%d,%d float32x=%d,%d float64x=%d,%d double=%d,%d long_double=%d,%d float16=%d,%d float128=%d,%d float_complex=%d,%d double_complex=%d,%d long_double_complex=%d,%d float128_complex=%d,%d void=%d "
	     "bool=%d,%d fun=%d,%d alignof_string=%d max_alignment=%d size_t=%s "
	     "wchar_t=%s char16_t=%s char32_t=%s char_signed=%s "
	     "big_endian=%s __thread_is_keyword=%s __builtin_va_list=%s "
	     "underscore_name=%s\n",
	     dep.sizeof_short, dep.alignof_short, dep.sizeof_int, dep.alignof_int,
	     dep.sizeof_long, dep.alignof_long, dep.sizeof_longlong, dep.alignof_longlong,
	     dep.sizeof_ptr, dep.alignof_ptr,
	     dep.alignof_enum,
	     dep.sizeof_float32x, dep.alignof_float32x,
	     dep.sizeof_float64x, dep.alignof_float64x,
	     dep.sizeof_float, dep.alignof_float, 
             dep.sizeof_double, dep.alignof_double,
	     dep.sizeof_longdouble, dep.alignof_longdouble,
             dep.sizeof_float16, dep.alignof_float16, 
             dep.sizeof_float128, dep.alignof_float128,
             dep.sizeof_floatcomplex, dep.alignof_floatcomplex, 
             dep.sizeof_doublecomplex, dep.alignof_doublecomplex,
	     dep.sizeof_longdoublecomplex, dep.alignof_longdoublecomplex,
             dep.sizeof_float128complex, dep.alignof_float128complex,
             dep.sizeof_void,
	     dep.sizeof_bool, dep.alignof_bool,
	     dep.sizeof_fun, dep.alignof_fun, 
             dep.alignof_str, dep.alignof_aligned,
	     underscore(TYPE_SIZE_T), underscore(TYPE_WCHAR_T), underscore(TYPE_CHAR16_T), underscore(TYPE_CHAR32_T),
	     char_is_unsigned ? "false" : "true",
	     little_endian ? "false" : "true",
	     THREAD_IS_KEYWORD, HAVE_BUILTIN_VA_LIST, UNDERSCORE_NAME);
    }
  else if (json) {
      fprintf(stderr, "Generating machine dependency information for CIL\n");
      
      printf("{\n");
/*      printf("\t _comment: \"Generated by code in %s\",\n", __FILE__); */
      printf("\t \"version_major\"    : %d,\n", VERSION_MAJOR);
      printf("\t \"version_minor\"    : %d,\n", VERSION_MINOR);
      printf("\t \"version\"          : \"%s\",\n", VERSION);

      // Size of certain types
      printf("\t \"sizeof_short\"               : %d,\n", dep.sizeof_short);
      printf("\t \"sizeof_int\"                 : %d,\n", dep.sizeof_int);
      printf("\t \"sizeof_bool\"                : %d,\n", dep.sizeof_bool);
      printf("\t \"sizeof_long\"                : %d,\n", dep.sizeof_long);
      printf("\t \"sizeof_longlong\"            : %d,\n", dep.sizeof_longlong);
      printf("\t \"sizeof_int128\"              : %d,\n", dep.sizeof_int128);
      printf("\t \"sizeof_ptr\"                 : %d,\n", dep.sizeof_ptr);
      printf("\t \"sizeof_float\"               : %d,\n", dep.sizeof_float);
      printf("\t \"sizeof_float32x\"            : %d,\n", dep.sizeof_float32x);
      printf("\t \"sizeof_float128\"            : %d,\n", dep.sizeof_float128);
      printf("\t \"sizeof_float64x\"            : %d,\n", dep.sizeof_float64x);
      printf("\t \"sizeof_float32\"            : %d,\n", dep.sizeof_float32);
      printf("\t \"sizeof_float64\"            : %d,\n", dep.sizeof_float64);
      printf("\t \"sizeof_double\"              : %d,\n", dep.sizeof_double);
      printf("\t \"sizeof_longdouble\"          : %d,\n", dep.sizeof_longdouble);
      printf("\t \"sizeof_float16\"             : %d,\n", dep.sizeof_float16);
      printf("\t \"sizeof_floatcomplex\"        : %d,\n", dep.sizeof_floatcomplex);
      printf("\t \"sizeof_doublecomplex\"       : %d,\n", dep.sizeof_doublecomplex);
      printf("\t \"sizeof_longdoublecomplex\"   : %d,\n", dep.sizeof_longdoublecomplex);
      printf("\t \"sizeof_float128complex\"     : %d,\n", dep.sizeof_float128complex);
      printf("\t \"sizeof_void\"                : %d,\n", dep.sizeof_void);
      printf("\t \"sizeof_fun\"                 : %d,\n", dep.sizeof_fun);
      printf("\t \"size_t\"                     : \"%s\",\n", TYPE_SIZE_T);
      printf("\t \"wchar_t\"                    : \"%s\",\n", TYPE_WCHAR_T);
      printf("\t \"char16_t\"                   : \"%s\",\n", TYPE_CHAR16_T);
      printf("\t \"char32_t\"                   : \"%s\",\n", TYPE_CHAR32_T);
      printf("\t \"alignof_short\"              : %d,\n", dep.alignof_short);
      printf("\t \"alignof_int\"                : %d,\n", dep.alignof_int);
      printf("\t \"alignof_bool\"               : %d,\n", dep.alignof_bool);
      printf("\t \"alignof_long\"               : %d,\n", dep.alignof_long);
      printf("\t \"alignof_longlong\"           : %d,\n", dep.alignof_longlong);
      printf("\t \"alignof_int128\"             : %d,\n", dep.alignof_int128);
      printf("\t \"alignof_ptr\"                : %d,\n", dep.alignof_ptr);
      printf("\t \"alignof_enum\"               : %d,\n", dep.alignof_enum);
      printf("\t \"alignof_float\"              : %d,\n", dep.alignof_float);
      printf("\t \"alignof_float32x\"           : %d,\n", dep.alignof_float32x);
      printf("\t \"alignof_float128\"           : %d,\n", dep.alignof_float128);
      printf("\t \"alignof_float64x\"           : %d,\n", dep.alignof_float64x);
      printf("\t \"alignof_float32\"           : %d,\n", dep.alignof_float32);
      printf("\t \"alignof_float64\"           : %d,\n", dep.alignof_float64);
      printf("\t \"alignof_double\"             : %d,\n", dep.alignof_double);
      printf("\t \"alignof_longdouble\"         : %d,\n", dep.alignof_longdouble);
      printf("\t \"alignof_float16\"            : %d,\n", dep.alignof_float16);
      printf("\t \"alignof_floatcomplex\"       : %d,\n", dep.alignof_floatcomplex);
      printf("\t \"alignof_doublecomplex\"      : %d,\n", dep.alignof_doublecomplex);
      printf("\t \"alignof_longdoublecomplex\"  : %d,\n", dep.alignof_longdoublecomplex);
      printf("\t \"alignof_float128complex\"    : %d,\n", dep.alignof_float128complex);
      printf("\t \"alignof_str\"                : %d,\n", dep.alignof_str);
      printf("\t \"alignof_fun\"                : %d,\n", dep.alignof_fun);
      printf("\t \"alignof_aligned\"            : %d,\n", dep.alignof_aligned);
      printf("\t \"char_is_unsigned\"           : %s,\n", char_is_unsigned ? "true" : "false");
      printf("\t \"underscore_name\"            : %s,\n", UNDERSCORE_NAME);
      printf("\t \"__builtin_va_list\"          : %s,\n", HAVE_BUILTIN_VA_LIST);
      printf("\t \"__thread_is_keyword\"        : %s,\n", THREAD_IS_KEYWORD);
      printf("\t \"little_endian\"              : %s\n", little_endian ? "true" : "false");
      printf("}\n");
 }
  else
    {
      fprintf(stderr, "Generating machine dependency information for CIL\n");

      printf("(* Generated by code in %s *)\n", __FILE__);
      printf("\t version_major    = %d;\n", VERSION_MAJOR);
      printf("\t version_minor    = %d;\n", VERSION_MINOR);
      printf("\t version          = \"%s\";\n", VERSION);

      // Size of certain types
      printf("\t sizeof_short               = %d;\n", dep.sizeof_short);
      printf("\t sizeof_int                 = %d;\n", dep.sizeof_int);
      printf("\t sizeof_bool                = %d;\n", dep.sizeof_bool);
      printf("\t sizeof_long                = %d;\n", dep.sizeof_long);
      printf("\t sizeof_longlong            = %d;\n", dep.sizeof_longlong);
      printf("\t sizeof_int128             = Some %d;\n", dep.sizeof_int128);
      printf("\t sizeof_ptr                 = %d;\n", dep.sizeof_ptr);
      printf("\t sizeof_float               = %d;\n", dep.sizeof_float);
      printf("\t sizeof_float32x            = %d;\n", dep.sizeof_float32x);
      printf("\t sizeof_float128            = %d;\n", dep.sizeof_float128);
      printf("\t sizeof_float64x            = %d;\n", dep.sizeof_float64x);
      printf("\t sizeof_float32            = Some %d;\n", dep.sizeof_float32);
      printf("\t sizeof_float64            = Some %d;\n", dep.sizeof_float64);
      printf("\t sizeof_double              = %d;\n", dep.sizeof_double);
      printf("\t sizeof_longdouble          = %d;\n", dep.sizeof_longdouble);
      printf("\t sizeof_float16             = %d;\n", dep.sizeof_float16);
      printf("\t sizeof_floatcomplex        = %d;\n", dep.sizeof_floatcomplex);
      printf("\t sizeof_doublecomplex       = %d;\n", dep.sizeof_doublecomplex);
      printf("\t sizeof_longdoublecomplex   = %d;\n", dep.sizeof_longdoublecomplex);
      printf("\t sizeof_float128complex     = %d;\n", dep.sizeof_float128complex);
      printf("\t sizeof_void                = %d;\n", dep.sizeof_void);
      printf("\t sizeof_fun                 = %d;\n", dep.sizeof_fun);
      printf("\t size_t                     = \"%s\";\n", TYPE_SIZE_T);
      printf("\t wchar_t                    = \"%s\";\n", TYPE_WCHAR_T);
      printf("\t char16_t                   = \"%s\";\n", TYPE_CHAR16_T);
      printf("\t char32_t                   = \"%s\";\n", TYPE_CHAR32_T);
      printf("\t alignof_short              = %d;\n", dep.alignof_short);
      printf("\t alignof_int                = %d;\n", dep.alignof_int);
      printf("\t alignof_bool               = %d;\n", dep.alignof_bool);
      printf("\t alignof_long               = %d;\n", dep.alignof_long);
      printf("\t alignof_longlong           = %d;\n", dep.alignof_longlong);
      printf("\t alignof_int128            = Some %d;\n", dep.alignof_int128);
      printf("\t alignof_ptr                = %d;\n", dep.alignof_ptr);
      printf("\t alignof_enum               = %d;\n", dep.alignof_enum);
      printf("\t alignof_float              = %d;\n", dep.alignof_float);
      printf("\t alignof_float32x           = %d;\n", dep.alignof_float32x);
      printf("\t alignof_float128           = %d;\n", dep.alignof_float128);
      printf("\t alignof_float64x           = %d;\n", dep.alignof_float64x);
      printf("\t alignof_float32           = Some %d;\n", dep.alignof_float32);
      printf("\t alignof_float64           = Some %d;\n", dep.alignof_float64);
      printf("\t alignof_double             = %d;\n", dep.alignof_double);
      printf("\t alignof_longdouble         = %d;\n", dep.alignof_longdouble);
      printf("\t alignof_float16            = %d;\n", dep.alignof_float16);
      printf("\t alignof_floatcomplex       = %d;\n", dep.alignof_floatcomplex);
      printf("\t alignof_doublecomplex      = %d;\n", dep.alignof_doublecomplex);
      printf("\t alignof_longdoublecomplex  = %d;\n", dep.alignof_longdoublecomplex);
      printf("\t alignof_float128complex    = %d;\n", dep.alignof_float128complex);
      printf("\t alignof_str                = %d;\n", dep.alignof_str);
      printf("\t alignof_fun                = %d;\n", dep.alignof_fun);
      printf("\t alignof_aligned            = %d;\n", dep.alignof_aligned);
      printf("\t char_is_unsigned           = %s;\n", char_is_unsigned ? "true" : "false");
      printf("\t underscore_name            = %s;\n", UNDERSCORE_NAME);
      printf("\t __builtin_va_list          = %s;\n", HAVE_BUILTIN_VA_LIST);
      printf("\t __thread_is_keyword        = %s;\n", THREAD_IS_KEYWORD);
      printf("\t little_endian              = %s;\n", little_endian ? "true" : "false");
    }
  return 0;
}
