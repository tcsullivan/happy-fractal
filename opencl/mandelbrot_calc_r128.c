inline ulong2 r128Add(const ulong2 a, const ulong2 b)
{
    ulong2 dst;
    dst.lo = a.lo + b.lo;
    dst.hi = a.hi + b.hi + (dst.lo < a.lo);
    return dst;
}

inline ulong2 r128Sub(const ulong2 a, const ulong2 b)
{
    ulong2 dst;
    dst.lo = a.lo - b.lo;
    dst.hi = a.hi - b.hi - (dst.lo > a.lo);
    return dst;
}

inline ulong2 r128__umul128(ulong a, ulong b)
{
    ulong alo = a & 0xFFFFFFFF;
    ulong ahi = a >> 32;
    ulong blo = b & 0xFFFFFFFF;
    ulong bhi = b >> 32;
    ulong p0, p1, p2, p3;

    p0 = alo * blo;
    p1 = alo * bhi;
    p2 = ahi * blo;
    p3 = ahi * bhi;

    ulong2 dst;
    ulong carry;
    carry = ((p1 & 0xFFFFFFFF) + (p2 & 0xFFFFFFFF) + (p0 >> 32)) >> 32;

    dst.lo = p0 + ((p1 + p2) << 32);
    dst.hi = p3 + ((uint)(p1 >> 32) + (uint)(p2 >> 32)) + carry;
    return dst;
}

inline ulong2 r128__umul128S(ulong a)
{
    ulong alo = a & 0xFFFFFFFF;
    ulong ahi = a >> 32;
    ulong p0, p1, p3;

    p0 = alo * alo;
    p1 = alo * ahi;
    p3 = ahi * ahi;

    ulong2 dst;
    ulong carry;
    carry = ((p1 & 0xFFFFFFFF) * 2 + (p0 >> 32)) >> 32;

    dst.lo = p0 + (p1 << 33);
    dst.hi = p3 + (uint)(p1 >> 31) + carry;
    return dst;
}

inline ulong2 r128Shr(const ulong2 src, int amount)
{
    ulong2 r;
    r.lo = (src.lo >> amount) | (src.hi << (64 - amount));
    r.hi = src.hi >> amount;
    return r;
}

inline ulong2 r128__umul(const ulong2 a, const ulong2 b)
{
   ulong2 ahbl, ahbh, sum;

   sum.lo = r128__umul128(a.lo, b.lo).hi;
   ahbl = r128__umul128(a.hi, b.lo);
   ahbh = r128__umul128(a.hi, b.hi);

   ahbh = r128Shr(ahbh, 60);
   sum.hi = ahbh.lo;
   sum = r128Add(sum, ahbl);
   sum = r128Add(sum, ahbl);

   return sum;
}

inline ulong2 r128Mul(const ulong2 a, const ulong2 b)
{
   int sign = 0;
   ulong2 ta, tb, tc;

   ta = a;
   tb = b;

   if ((long)ta.hi < 0) {
      ta.lo = ~ta.lo + 1;
      ta.hi = ~ta.hi;
      sign = !sign;
   }
   if ((long)tb.hi < 0) {
      tb.lo = ~tb.lo + 1;
      tb.hi = ~tb.hi;
      sign = !sign;
   }

   tc = r128__umul(ta, tb);

   if (sign) {
      tc.lo = ~tc.lo + 1;
      tc.hi = ~tc.hi;
   }

   return tc;
}

inline ulong2 r128Square(const ulong2 a)
{
   ulong2 ta = a;

   if ((long)ta.hi < 0) {
      ta.lo = ~ta.lo + 1;
      ta.hi = ~ta.hi;
   }

   ulong2 ahbl, ahbh, sum;

   sum.lo = r128__umul128S(ta.lo).hi;
   ahbl = r128__umul128(ta.hi, ta.lo);
   ahbh = r128__umul128S(ta.hi);

   ahbh = r128Shr(ahbh, 60);
   sum.hi = ahbh.lo;
   sum = r128Add(sum, ahbl);
   sum = r128Add(sum, ahbl);
   return sum;
}

__kernel void mandelbrot_calc(const __global ulong4 *c_pt,
                              __global unsigned int *out_it,
                              const unsigned int max_iterations)
{
    const int id = get_global_id(0);
    const ulong4 opt = c_pt[id];

    ulong4 pt = opt;
    ulong2 tmp;
    unsigned int iterations;

    for (iterations = 0; iterations < max_iterations; ++iterations) {
        tmp = r128Mul(pt.lo, pt.hi);
        pt.lo = r128Square(pt.lo);
        pt.hi = r128Square(pt.hi);

        const ulong2 sum = r128Add(pt.lo, pt.hi);
        if ((long)sum.hi >= 0x4000000000000000)
            break;

        pt.lo = r128Sub(pt.lo, pt.hi);
        pt.hi = r128Add(tmp, tmp);
        pt.lo = r128Add(pt.lo, opt.lo);
        pt.hi = r128Add(pt.hi, opt.hi);
    }

    if (iterations == max_iterations)
        out_it[id] = 0;
    else
        out_it[id] = ((iterations & 0xFF) << 16) | ((iterations & 0x07) << 6);
}

