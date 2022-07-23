/**
 * happy-fractal - A study of efficient and precise fractal rendering.
 * Copyright (C) 2022  Clyne Sullivan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// If defined, program auto-zooms and measures runtime.
//#define BENCHMARK

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#define CL_HPP_TARGET_OPENCL_VERSION (300)
#define CL_HPP_ENABLE_EXCEPTIONS (1)
#include <CL/opencl.hpp>
#include <SDL2/SDL.h>

// The "Float" type determines what data type will store numbers for calculations.
// Can use native float or double; or, a custom Q4.124 fixed-point data type.
// If fixed point, use "*_r128.c" OpenCL kernel. Otherwise, use regular kernel.

#define R128_IMPLEMENTATION
#include "r128.h"
using Float = R128;

//using Float = double;

// Sets the window's dimensions. The window is square.
constexpr static int WIN_DIM = 800;

// Not allowed to calculate less iterations than this.
constexpr uint32_t MIN_MAX_ITERATIONS = 70;
// Not allowed to zoom out farther than this.
static const Float MIN_ZOOM (4.0);

/**
 * A packed Float-pair for storing complex numbers.
 * Must match a vector type for OpenCL.
 */
struct Complex {
    Float real = Float(0);
    Float imag = Float(0);
} __attribute__ ((packed));

class MandelbrotState
{
public:
    // Initializes data and spawns the calculation thread.
    MandelbrotState();
    // Joins threads.
    ~MandelbrotState();

    // Prepares to use the given OpenCL kernel for calculations.
    void initKernel(cl::Context& clcontext, cl::Program& clprogram, const char *kernelname);

    Float zoom() const;

    // Offsets the view's origin by the given Complex, and changes zoom by the given factor.
    // Returns true if a new calculation has been scheduled (false if one is in progress).
    bool moveOriginAndZoomBy(Complex c, Float z);

    // Outputs the results of the latest calculation into the given SDL texture.
    // Returns true if successful.
    // Returns false if a calculation is in progress. The texture is not updated.
    bool intoTexture(SDL_Texture *texture);
    // Requests the initiation of a new calculation.
    void scheduleRecalculation();

private:
    std::thread m_calc_thread;
    std::atomic_bool m_calcing; // If false, we're ready for a new calculation.
    std::atomic_flag m_recalc;  // Tell calcThread to recalc, or tell main thread recalcing is done.
    uint32_t m_max_iterations;
    Float m_zoom;
    Complex m_origin;

    std::unique_ptr<cl::Kernel> m_cl_kernel;
    std::unique_ptr<cl::CommandQueue> m_cl_queue;
    std::unique_ptr<cl::Buffer> m_cl_input;
    std::unique_ptr<cl::Buffer> m_cl_output;

    // Enters main loop of calcThread.
    void calcThread();
    // Calls the OpenCL kernel to compute new results.
    void calculateBitmap();

    // Determine the max iteration count based on zoom factor.
    static uint32_t calculateMaxIterations(Float zoom);
};

static bool done = false;
static std::atomic_int fps = 0;
static std::chrono::time_point<std::chrono::high_resolution_clock> clTime;

static cl::Context initCLContext();
static cl::Program initCLProgram(cl::Context&, const char * const);
static void initSDL(SDL_Window **, SDL_Renderer **, SDL_Texture **);
static void threadFpsMonitor(MandelbrotState&);
static void threadEventMonitor(MandelbrotState&);

int main(int argc, char **argv)
{
    MandelbrotState Mandelbrot;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *MandelbrotTexture;

    initSDL(&window, &renderer, &MandelbrotTexture);

    std::ifstream clSource ("opencl/mandelbrot_calc_r128.c");
    if (!clSource.good())
        throw std::runtime_error("Failed to open OpenCL kernel!");

    // Dump OpenCL kernel into a std::string.
    std::ostringstream oss;
    oss << clSource.rdbuf();
    std::string clSourceStr (oss.str());

    auto clContext = initCLContext();
    auto clProgram = initCLProgram(clContext, clSourceStr.data());
    Mandelbrot.initKernel(clContext, clProgram, "mandelbrot_calc");

    // Initiate first calculation so something appears on the screen.
    Mandelbrot.scheduleRecalculation();

    std::thread fpsMonitor ([&Mandelbrot] { threadFpsMonitor(Mandelbrot); });
    std::thread eventMonitor ([&Mandelbrot] { threadEventMonitor(Mandelbrot); });

#ifdef BENCHMARK
    auto start = std::chrono::high_resolution_clock::now();
#endif

    while (!done) {
        if (Mandelbrot.intoTexture(MandelbrotTexture)) {
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, MandelbrotTexture, nullptr, nullptr);
            SDL_RenderPresent(renderer);

            ++fps;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }

#ifdef BENCHMARK
        if (Mandelbrot.zoom() < Float(1e-5))
            done = true;
#endif
    }

#ifdef BENCHMARK
    std::chrono::duration<double> seconds = std::chrono::high_resolution_clock::now() - start;
    std::cout << "Calculations took: " << seconds.count() << "s" << std::endl;
#endif

    eventMonitor.join();
    fpsMonitor.join();
    SDL_DestroyRenderer(renderer);
    return 0;
}


static cl::Platform clplatform;
static std::vector<cl::Device> cldevices;

cl::Context initCLContext()
{
    clplatform = cl::Platform::getDefault();
    clplatform.getDevices(CL_DEVICE_TYPE_GPU, &cldevices);

    return cl::Context(cldevices.front());
}

cl::Program initCLProgram(cl::Context& clcontext, const char * const source)
{
    cl::Program *prog;

    try {
        prog = new cl::Program(clcontext, source);
        prog->build();
        return *prog;
    } catch (const cl::Error& err) {
        const auto& dev = cldevices.front();
        std::cout << "Build Log: " << prog->getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev) << std::endl;
        throw err;
    }
}

void initSDL(SDL_Window **window, SDL_Renderer **renderer, SDL_Texture **texture)
{
    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        throw std::runtime_error("initSDL failed!");
    }

    *window = SDL_CreateWindow("Happy Mandelbrot",
                               SDL_WINDOWPOS_UNDEFINED,
                               SDL_WINDOWPOS_UNDEFINED,
                               WIN_DIM, WIN_DIM,
                               SDL_WINDOW_RESIZABLE);
    if (!*window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set create window: %s\n", SDL_GetError());
        throw std::runtime_error("initSDL failed!");
    }

    *renderer = SDL_CreateRenderer(*window, -1, 0);
    if (!*renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set create renderer: %s\n", SDL_GetError());
        throw std::runtime_error("initSDL failed!");
    }

    SDL_GL_SetSwapInterval(0);

    *texture = SDL_CreateTexture(*renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WIN_DIM, WIN_DIM);
    if (!*texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set create texture: %s\n", SDL_GetError());
        throw std::runtime_error("initSDL failed!");
    }

    atexit(SDL_Quit);
}

void threadFpsMonitor(MandelbrotState& Mandelbrot)
{
    while (!done) {
        std::cout << "Rendered FPS: " << fps.load() << ", Z: " << (double)Mandelbrot.zoom() << std::endl;
        fps.store(0);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void threadEventMonitor(MandelbrotState& Mandelbrot)
{
    Float zfactor (1.03);
    Float zooming (1);
    Complex newoffset;

    while (!done) {
        auto next = std::chrono::steady_clock::now() + std::chrono::milliseconds(17);

        for (SDL_Event event; SDL_PollEvent(&event);) {
            switch (event.type) {
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    done = true;
                break;
            case SDL_MOUSEBUTTONDOWN:
                // Calculate desired "normal" change from origin. -0.5 to 0.5.
                // Zoom scales this result later.
                newoffset = Complex {
                    Float(event.button.x / (double)WIN_DIM) - Float(0.5),
                    Float(event.button.y / (double)WIN_DIM) - Float(0.5)
                };

                // Zoom in with left button, zoom out with right.
                if (event.button.button == SDL_BUTTON_LEFT)
                    zooming *= Float(1) - (zfactor - Float(1));
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    zooming = zfactor;
                break;
            case SDL_MOUSEBUTTONUP:
                // Stop moving.
                zooming = 1;
                newoffset.real = 0;
                newoffset.imag = 0;
                break;
            case SDL_MOUSEMOTION:
                // Update offset on mouse movement, so zoom continues towards where the user expects.
                if (zooming != Float(1)) {
                    newoffset.real += Float(event.motion.xrel / (double)WIN_DIM);
                    newoffset.imag += Float(event.motion.yrel / (double)WIN_DIM);
                }
                break;
            case SDL_MOUSEWHEEL:
                // Increase zoom factor with scroll up, or decrease with scroll down.
                zfactor = std::max(Float(1), zfactor + Float(0.005) * Float(event.wheel.y));

                // Update zoom speed if zfactor is changed while zooming.
                if (zooming != Float(1)) {
                    if (zooming < Float(1))
                        zooming *= Float(1) - (zfactor - Float(1));
                    else
                        zooming = zfactor;
                }
                break;
            case SDL_QUIT:
                done = true;
                break;
            }
        }

#ifdef BENCHMARK
        // Constant zoom on a constant point.
        bool b = Mandelbrot.moveOriginAndZoomBy({}, Float(1) - (zfactor - Float(1)));
        std::this_thread::sleep_for(std::chrono::milliseconds(b ? 8 : 1)); // max 125fps
#else
        if (zooming != Float(1) || newoffset.real != Float(0) || newoffset.imag != Float(0)) {
            // Scale new offset according to zoom.
            const auto zoom = Mandelbrot.zoom();
            Complex c = newoffset;
            c.real *= zoom * (Float(1) - zooming);
            c.imag *= zoom * (Float(1) - zooming);

            // Don't sleep as long if a recalculation is already running, so
            // we can get our new request in sooner once recalculation completes.
            if (Mandelbrot.moveOriginAndZoomBy(c, zooming))
                std::this_thread::sleep_until(next);
            else
                std::this_thread::sleep_for(std::chrono::microseconds(50));
        } else {
            std::this_thread::sleep_until(next);
        }
#endif
    }
}

MandelbrotState::MandelbrotState():
    m_calcing(false),
    m_max_iterations(MIN_MAX_ITERATIONS),
    m_zoom(MIN_ZOOM)
{
#ifndef BENCHMARK
    // This is a good starting point.
    m_origin.real = -1;
    m_origin.imag = 0;
#else
    m_origin.real = -1.5;
    m_origin.imag = 0;
#endif

    // Spawn calcThread to begin receiving recalculation requests.
    m_recalc.clear();
    m_calc_thread = std::thread([this] { calcThread(); });
}

MandelbrotState::~MandelbrotState() {
    // calcThread is likely waiting for m_recalc to become true.
    m_recalc.test_and_set();
    m_recalc.notify_all();

    // Bring calcThread in if it's still running.
    if (m_calc_thread.joinable())
        m_calc_thread.join();
}

void MandelbrotState::initKernel(cl::Context& clcontext, cl::Program& clprogram, const char *kernelname)
{
    m_cl_kernel.reset(new cl::Kernel(clprogram, "mandelbrot_calc"));
    m_cl_queue.reset(new cl::CommandQueue(clcontext));
    m_cl_input.reset(new cl::Buffer(clcontext, CL_MEM_READ_ONLY, WIN_DIM * WIN_DIM * sizeof(Complex)));
    m_cl_output.reset(new cl::Buffer(clcontext, CL_MEM_WRITE_ONLY, WIN_DIM * WIN_DIM * sizeof(uint32_t)));

    // These kernel parameters do not change throughout execution.
    // Max iteration count does, and is set with each kernel execution.
    m_cl_kernel->setArg(0, *m_cl_input);
    m_cl_kernel->setArg(1, *m_cl_output);
}

Float MandelbrotState::zoom() const {
    return m_zoom;
}

bool MandelbrotState::moveOriginAndZoomBy(Complex c, Float z) {
    if (!m_calcing) {
        m_origin.real += c.real;
        m_origin.imag += c.imag;
        m_zoom = std::min(MIN_ZOOM, m_zoom * z);
        m_max_iterations = std::max(MIN_MAX_ITERATIONS, calculateMaxIterations(m_zoom));

        scheduleRecalculation();
    }

    return !m_calcing;
}

bool MandelbrotState::intoTexture(SDL_Texture *texture) {
    if (m_calcing) {
        // Wait for the calculations to complete.
        m_recalc.wait(true);

        // Lock the SDL texture, then stream the OpenCL output into it.
        void *dst;
        int pitch;
        SDL_LockTexture(texture, nullptr, &dst, &pitch);
        m_cl_queue->enqueueReadBuffer(*m_cl_output, CL_TRUE, 0, WIN_DIM * WIN_DIM * sizeof(uint32_t), dst);
        SDL_UnlockTexture(texture);

        std::chrono::duration<double> diff = 
            std::chrono::high_resolution_clock::now() - clTime;
        std::cout << "Time: " << diff.count() << "s" << std::endl;

        // Allow user input to modify origin and zoom,
        // also allowing the next calculation to be scheduled.
        m_calcing = false;
        return true;
    } else {
        return false;
    }
}

void MandelbrotState::scheduleRecalculation() {
    if (!m_calcing) {
        // Tell calcThread that it's time to recalculate.
        m_recalc.test_and_set();
        m_recalc.notify_one();
    }
}

void MandelbrotState::calcThread() {
    while (!done) {
        // Wait for a recalculation to be requested, indicated by m_recalc becoming true.
        m_recalc.wait(false);
        calculateBitmap();

        // Finished. Clear m_recalc, and notify the render thread (checked at MandelbrotState::intoTexture).
        m_recalc.clear();
        m_recalc.notify_one();
    }
}

uint32_t MandelbrotState::calculateMaxIterations(Float zoom)
{
    // The max iteration count will increase linearly with zoom factor.
    // TODO Does the result increase too quickly?

    return MIN_MAX_ITERATIONS * (1.5 - std::log(static_cast<double>(zoom)) / std::log((double)MIN_ZOOM));
}

void MandelbrotState::calculateBitmap()
{
    static std::array<Complex, WIN_DIM * WIN_DIM> points;
    static std::array<Float, WIN_DIM> row;
    static std::array<Float, WIN_DIM> col;

    //
    // Generate a list of every Complex coordinate that needs to be calculated.

    const Float dz (m_zoom * Float(1.0 / WIN_DIM));
    Complex pt;
    pt.real = m_origin.real - m_zoom * Float(0.5);
    pt.imag = m_origin.imag - m_zoom * Float(0.5);

    {
        auto p = row.begin();
        Float r = pt.real;
        for (int i = 0; i < WIN_DIM; ++i) {
            *p++ = r;
            r += dz;
        }
    }

    {
        auto p = col.begin();
        Float r = pt.imag;
        for (int i = 0; i < WIN_DIM; ++i) {
            *p++ = r;
            r += dz;
        }
    }

    auto ptr = points.begin();
    for (int j = 0; j < WIN_DIM; ++j) {
        Complex c;
        c.imag = col[j];

        for (int i = 0; i < WIN_DIM; ++i) {
            c.real = row[i];
            *ptr++ = c;
        }
    }

    //
    // Pass the list into the OpenCL kernel, and begin execution.

    while (m_calcing)
        std::this_thread::yield();

    m_calcing = true;

    clTime = std::chrono::high_resolution_clock::now();
    m_cl_kernel->setArg(2, m_max_iterations);
    m_cl_queue->enqueueWriteBuffer(*m_cl_input, CL_TRUE, 0, points.size() * sizeof(Complex), points.data());
    m_cl_queue->enqueueNDRangeKernel(*m_cl_kernel, cl::NullRange, cl::NDRange(points.size()), cl::NullRange);
}

