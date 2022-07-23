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

inline ulong r128__umul128Hi(ulong a, ulong b)
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

    ulong carry = ((p1 & 0xFFFFFFFF) + (p2 & 0xFFFFFFFF) + (p0 >> 32)) >> 32;
    return p3 + ((uint)(p1 >> 32) + (uint)(p2 >> 32)) + carry;
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
   ulong2 ahbl, albh, ahbh, sum;

   sum.lo = r128__umul128Hi(a.lo, b.lo);
   ahbl = r128__umul128(a.hi, b.lo);
   albh = r128__umul128(a.lo, b.hi);
   ahbh = r128__umul128(a.hi, b.hi);

   ahbh = r128Shr(ahbh, 60);
   sum.hi = ahbh.lo;
   sum = r128Add(sum, ahbl);
   sum = r128Add(sum, albh);

   return sum;
}

inline ulong2 r128Mul(const ulong2 a, const ulong2 b)
{
   int sign = 0;
   ulong2 ta, tb, tc;

   ta = a;
   tb = b;

   if ((long)ta.hi < 0) {
      if (ta.lo) {
         ta.lo = ~ta.lo + 1;
         ta.hi = ~ta.hi;
      } else {
         ta.lo = 0;
         ta.hi = ~ta.hi + 1;
      }
      sign = !sign;
   }
   if ((long)tb.hi < 0) {
      if (tb.lo) {
         tb.lo = ~tb.lo + 1;
         tb.hi = ~tb.hi;
      } else {
         tb.lo = 0;
         tb.hi = ~tb.hi + 1;
      }
      sign = !sign;
   }

   tc = r128__umul(ta, tb);

   if (sign) {
      if (tc.lo) {
         tc.lo = ~tc.lo + 1;
         tc.hi = ~tc.hi;
      } else {
         tc.lo = 0;
         tc.hi = ~tc.hi + 1;
      }
   }

   return tc;
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
        pt.lo = r128Mul(pt.lo, pt.lo);
        pt.hi = r128Mul(pt.hi, pt.hi);

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

