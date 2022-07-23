# happy-fractal

This is a casual project investigating Mandelbrot rendering. It began with the task of rendering the fractal, then achieving a smooth framerate (60+ FPS); now, I am substituting custom data types to allow for deeper zooms.

This project is written in modern C++, using SDL2 for rendering. OpenCL has been adopted for GPU-accelerated rendering.

The [r128](https://github.com/fahickman/r128) library was modified to provide a Q4.124 fixed-point data type. Rendering is slow, but more precise than native `double`. With Q4.124, we can get very close to the precision of [XaoS](https://xaos-project.github.io/), which I believe uses [80-bit extended-precision floating-point](https://en.wikipedia.org/wiki/Extended_precision#x86_extended_precision_format).

In the future, this may either see optimization for faster and smoother Q4.124 rendering, or a further increase in precision.

## Controls, notes

Point your mouse to where you would like to zoom from. Left click to zoom in, right click to zoom out.

The scroll wheel adjusts the zoom speed.

The source code has a BENCHMARK flag, which times an automated zoom to a given point.

To switch the source code between `double` and Q4.124, change the `Float` type accordingly and set the appropriate OpenCL kernel (in `main()`, add or remove the `_r128` suffix).

