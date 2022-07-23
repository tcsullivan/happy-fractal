__kernel void mandelbrot_calc(const __global double2 *c_pt,
                            __global unsigned int *out_it,
                            const unsigned int max_iterations)
{
    const int id = get_global_id(0);
    const double2 opt = c_pt[id];

    double2 pt = opt;
    double tmp = pt.x * pt.y;
    unsigned int iterations = 0;

    double q = (pt.x - 0.25) * (pt.x - 0.25) + pt.y * pt.y;
    if (q * (q + (pt.x - 0.25)) <= 0.25 * pt.y * pt.y)
        iterations = max_iterations;

    while (iterations < max_iterations) {
        pt *= pt;

        if (pt.x + pt.y > 4.0)
            break;

        pt.x = pt.x - pt.y;
        pt.y = 2 * tmp;
        pt += opt;
        tmp = pt.x * pt.y;

        ++iterations;
    }

    if (iterations == max_iterations)
        out_it[id] = 0;
    else
        out_it[id] = ((iterations & 0xFF) << 16) | ((iterations & 0x07) << 6);
}

