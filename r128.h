/*
r128.h: 128-bit (64.64) signed fixed-point arithmetic. Version 1.6.0

Modified by tcsullivan to be used as (4.124) fixed-point.
Unused operators also removed.

COMPILATION
-----------
Drop this header file somewhere in your project and include it wherever it is
needed. There is no separate .c file for this library. To get the code, in ONE
file in your project, put:

#define R128_IMPLEMENTATION

before you include this file. You may also provide a definition for R128_ASSERT
to force the library to use a custom assert macro.

COMPILER/LIBRARY SUPPORT
------------------------
This library requires a C89 compiler with support for 64-bit integers. If your
compiler does not support the long long data type, the R128_U64, etc. macros
must be set appropriately. On x86 and x64 targets, Intel intrinsics are used
for speed. If your compiler does not support these intrinsics, you can add
#define R128_STDC_ONLY
in your implementation file before including r128.h.

The only C runtime library functionality used by this library is <assert.h>.
This can be avoided by defining an R128_ASSERT macro in your implementation
file. Since this library uses 64-bit arithmetic, this may implicitly add a
runtime library dependency on 32-bit platforms.

C++ SUPPORT
-----------
Operator overloads are supplied for C++ files that include this file. Since all
C++ functions are declared inline (or static inline), the R128_IMPLEMENTATION
file can be either C++ or C.

LICENSE
-------
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef H_R128_H
#define H_R128_H

#include <stddef.h>
#include <stdint.h>

#define R128_S32 int32_t
#define R128_U32 uint32_t
#define R128_S64 int64_t
#define R128_U64 uint64_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct R128 {
   R128_U64 lo;
   R128_U64 hi;

#ifdef __cplusplus
   R128();
   R128(double);
   R128(int);
   R128(R128_S64);
   R128(R128_U64 low, R128_U64 high);

   operator double() const;

   R128 operator-() const;
   R128 &operator+=(const R128 &rhs);
   R128 &operator-=(const R128 &rhs);
   R128 &operator*=(const R128 &rhs);
#endif   //__cplusplus
} __attribute__ ((packed)) R128;

// Type conversion
extern void r128FromInt(R128 *dst, R128_S64 v);
extern void r128FromFloat(R128 *dst, double v);
extern double r128ToFloat(const R128 *v);

// Copy
extern void r128Copy(R128 *dst, const R128 *src);

// Sign manipulation
extern void r128Neg(R128 *dst, const R128 *v);   // -v

// Bitwise operations
extern void r128Shl(R128 *dst, const R128 *src, int amount);   // shift left by amount mod 128
extern void r128Shr(R128 *dst, const R128 *src, int amount);   // shift right logical by amount mod 128

// Arithmetic
extern void r128Add(R128 *dst, const R128 *a, const R128 *b);  // a + b
extern void r128Sub(R128 *dst, const R128 *a, const R128 *b);  // a - b
extern void r128Mul(R128 *dst, const R128 *a, const R128 *b);  // a * b

// Comparison
extern int  r128Cmp(const R128 *a, const R128 *b);  // sign of a-b
extern int  r128IsNeg(const R128 *v); // quick check for < 0

// Constants
extern const R128 R128_min;      // minimum (most negative) value
extern const R128 R128_max;      // maximum (most positive) value
extern const R128 R128_smallest; // smallest positive value
extern const R128 R128_zero;     // zero
extern const R128 R128_one;      // 1.0

extern char R128_decimal;        // decimal point character used by r128From/ToString. defaults to '.'

#ifdef __cplusplus
}

inline R128::R128() {}

inline R128::R128(double v)
{
   r128FromFloat(this, v);
}

inline R128::R128(int v)
{
   r128FromInt(this, v);
}

inline R128::R128(R128_S64 v)
{
   r128FromInt(this, v);
}

inline R128::R128(R128_U64 low, R128_U64 high)
{
   lo = low;
   hi = high;
}

inline R128::operator double() const
{
   return r128ToFloat(this);
}

inline R128 R128::operator-() const
{
   R128 r;
   r128Neg(&r, this);
   return r;
}

inline R128 &R128::operator+=(const R128 &rhs)
{
   r128Add(this, this, &rhs);
   return *this;
}

inline R128 &R128::operator-=(const R128 &rhs)
{
   r128Sub(this, this, &rhs);
   return *this;
}

inline R128 &R128::operator*=(const R128 &rhs)
{
   r128Mul(this, this, &rhs);
   return *this;
}

static inline R128 operator+(const R128 &lhs, const R128 &rhs)
{
   R128 r(lhs);
   return r += rhs;
}

static inline R128 operator-(const R128 &lhs, const R128 &rhs)
{
   R128 r(lhs);
   return r -= rhs;
}

static inline R128 operator*(const R128 &lhs, const R128 &rhs)
{
   R128 r(lhs);
   return r *= rhs;
}

static inline bool operator<(const R128 &lhs, const R128 &rhs)
{
   return r128Cmp(&lhs, &rhs) < 0;
}

static inline bool operator>(const R128 &lhs, const R128 &rhs)
{
   return r128Cmp(&lhs, &rhs) > 0;
}

static inline bool operator<=(const R128 &lhs, const R128 &rhs)
{
   return r128Cmp(&lhs, &rhs) <= 0;
}

static inline bool operator>=(const R128 &lhs, const R128 &rhs)
{
   return r128Cmp(&lhs, &rhs) >= 0;
}

static inline bool operator==(const R128 &lhs, const R128 &rhs)
{
   return lhs.lo == rhs.lo && lhs.hi == rhs.hi;
}

static inline bool operator!=(const R128 &lhs, const R128 &rhs)
{
   return lhs.lo != rhs.lo || lhs.hi != rhs.hi;
}

#endif   //__cplusplus
#endif   //H_R128_H

#ifdef R128_IMPLEMENTATION

#define R128_SET2(x, l, h) { (x)->lo = (R128_U64)(l); (x)->hi = (R128_U64)(h); }

#include <stdlib.h>  // for NULL

const R128 R128_min = { 0, 0x8000000000000000 };
const R128 R128_max = { 0xffffffffffffffff, 0x7fffffffffffffff };
const R128 R128_smallest = { 1, 0 };
const R128 R128_zero = { 0, 0 };
const R128 R128_one = { 0, 1 };
char R128_decimal = '.';

static void r128__neg(R128 *dst, const R128 *src)
{
   if (src->lo) {
      dst->lo = ~src->lo + 1;
      dst->hi = ~src->hi;
   } else {
      dst->lo = 0;
      dst->hi = ~src->hi + 1;
   }
}

// 64*64->128
static void r128__umul128(R128 *dst, R128_U64 a, R128_U64 b)
{
   R128_U32 alo = (R128_U32)a;
   R128_U32 ahi = (R128_U32)(a >> 32);
   R128_U32 blo = (R128_U32)b;
   R128_U32 bhi = (R128_U32)(b >> 32);
   R128_U64 p0, p1, p2, p3;

   p0 = alo * (uint64_t)blo;
   p1 = alo * (uint64_t)bhi;
   p2 = ahi * (uint64_t)blo;
   p3 = ahi * (uint64_t)bhi;

   {
      R128_U64 carry, lo, hi;
      carry = ((R128_U64)(R128_U32)p1 + (R128_U64)(R128_U32)p2 + (p0 >> 32)) >> 32;

      lo = p0 + ((p1 + p2) << 32);
      hi = p3 + ((R128_U32)(p1 >> 32) + (R128_U32)(p2 >> 32)) + carry;

      R128_SET2(dst, lo, hi);
   }
}

static int r128__ucmp(const R128 *a, const R128 *b)
{
   if (a->hi != b->hi) {
      if (a->hi > b->hi) {
         return 1;
      } else {
         return -1;
      }
   } else {
      if (a->lo == b->lo) {
         return 0;
      } else if (a->lo > b->lo) {
         return 1;
      } else {
         return -1;
      }
   }
}

static void r128__umul(R128 *dst, const R128 *a, const R128 *b)
{
   R128 albl, ahbl, albh, ahbh;

   r128__umul128(&albl, a->lo, b->lo);
   r128__umul128(&ahbl, a->hi, b->lo);
   r128__umul128(&albh, a->lo, b->hi);
   r128__umul128(&ahbh, a->hi, b->hi);

   R128 sum;

   r128Shr(&ahbh, &ahbh, 60);
   sum.lo = 0;
   sum.hi = ahbh.lo;

   albl.lo = albl.hi;
   albl.hi = 0;
   r128Add(&sum, &sum, &albl);
   r128Add(&sum, &sum, &ahbl);
   r128Add(&sum, &sum, &albh);

   R128_SET2(dst, sum.lo, sum.hi);
}

void r128FromInt(R128 *dst, R128_S64 v)
{
   dst->lo = 0;
   dst->hi = (R128_U64)v << 60;
}

void r128FromFloat(R128 *dst, double v)
{
   if (v < -9223372036854775808.0) {
      r128Copy(dst, &R128_min);
   } else if (v >= 9223372036854775808.0) {
      r128Copy(dst, &R128_max);
   } else {
      R128 r;
      int sign = 0;

      if (v < 0) {
         v = -v;
         sign = 1;
      }

      r.hi = ((R128_U64)(R128_S32)v) << 60;
      v -= (R128_S32)v;
      v *= (double)((R128_U64)1 << 60);
      r.hi = (r.hi & 0xF000000000000000) + (R128_U64)v;
      r.lo = (R128_U64)(v * 18446744073709551616.0);

      if (sign) {
         r128__neg(&r, &r);
      }

      r128Copy(dst, &r);
   }
}

R128_S64 r128ToInt(const R128 *v)
{
   if ((R128_S64)v->hi < 0) {
      return (R128_S64)v->hi + (v->lo != 0);
   } else {
      return (R128_S64)v->hi;
   }
}

double r128ToFloat(const R128 *v)
{
   R128 tmp;
   int sign = 0;
   double d;

   R128_SET2(&tmp, v->lo, v->hi);
   if (r128IsNeg(&tmp)) {
      r128__neg(&tmp, &tmp);
      sign = 1;
   }

   d = (tmp.hi >> 60);
   d += (tmp.hi & 0xFFFFFFFFFFFFFF) / (double)((uint64_t)1 << 60);
   d += tmp.lo / (double)((uint64_t)1 << 60) / 18446744073709551616.0;
   
   if (sign) {
      d = -d;
   }

   return d;
}

void r128Copy(R128 *dst, const R128 *src)
{
   dst->lo = src->lo;
   dst->hi = src->hi;
}

void r128Neg(R128 *dst, const R128 *v)
{
   r128__neg(dst, v);
}

void r128Shl(R128 *dst, const R128 *src, int amount)
{
   R128_U64 r[4];

   r[0] = src->lo;
   r[1] = src->hi;

   amount &= 127;
   if (amount >= 64) {
      r[1] = r[0] << (amount - 64);
      r[0] = 0;
   } else if (amount) {
      r[1] = (r[1] << amount) | (r[0] >> (64 - amount));
      r[0] = r[0] << amount;
   }

   dst->lo = r[0];
   dst->hi = r[1];
}

void r128Shr(R128 *dst, const R128 *src, int amount)
{
   R128_U64 r[4];

   r[2] = src->lo;
   r[3] = src->hi;

   amount &= 127;
   if (amount >= 64) {
      r[2] = r[3] >> (amount - 64);
      r[3] = 0;
   } else if (amount) {
      r[2] = (r[2] >> amount) | (r[3] << (64 - amount));
      r[3] = r[3] >> amount;
   }

   dst->lo = r[2];
   dst->hi = r[3];
}

void r128Add(R128 *dst, const R128 *a, const R128 *b)
{
   unsigned char carry = 0;

   {
      R128_U64 r = a->lo + b->lo;
      carry = r < a->lo;
      dst->lo = r;
      dst->hi = a->hi + b->hi + carry;
   }
}

void r128Sub(R128 *dst, const R128 *a, const R128 *b)
{
   unsigned char borrow = 0;

   {
      R128_U64 r = a->lo - b->lo;
      borrow = r > a->lo;
      dst->lo = r;
      dst->hi = a->hi - b->hi - borrow;
   }
}

void r128Mul(R128 *dst, const R128 *a, const R128 *b)
{
   int sign = 0;
   R128 ta, tb, tc;

   R128_SET2(&ta, a->lo, a->hi);
   R128_SET2(&tb, b->lo, b->hi);

   if (r128IsNeg(&ta)) {
      r128__neg(&ta, &ta);
      sign = !sign;
   }
   if (r128IsNeg(&tb)) {
      r128__neg(&tb, &tb);
      sign = !sign;
   }

   r128__umul(&tc, &ta, &tb);
   if (sign) {
      r128__neg(&tc, &tc);
   }

   r128Copy(dst, &tc);
}

int r128Cmp(const R128 *a, const R128 *b)
{
   if (a->hi == b->hi) {
      if (a->lo == b->lo) {
         return 0;
      } else if (a->lo > b->lo) {
         return 1;
      } else {
         return -1;
      }
   } else if ((R128_S64)a->hi > (R128_S64)b->hi) {
      return 1;
   } else {
      return -1;
   }
}

int r128IsNeg(const R128 *v)
{
   return (R128_S64)v->hi < 0;
}

#endif   //R128_IMPLEMENTATION

