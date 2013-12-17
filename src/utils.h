/* Copyright (c) 2012 University of Cape Town
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file
 *
 * Utility functions that are private to the library.
 */

#ifndef UTILS_H
#define UTILS_H

#include <clogs/visibility_push.h>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <locale>
#include <CL/cl.hpp>
#include <clogs/visibility_pop.h>

namespace clogs
{
namespace detail
{

/**
 * Returns true if @a device supports @a extension.
 * At present, no caching is done, so this is a potentially slow operation.
 */
CLOGS_LOCAL bool deviceHasExtension(const cl::Device &device, const std::string &extension);

/**
 * Retrieves the kernel sources baked into the library.
 *
 * The implementation of this function is in generated code.
 */
CLOGS_LOCAL const std::map<std::string, std::string> &getSourceMap();

/**
 * Subgroups of this size are guaranteed to have a synchronized view of
 * local memory at sequence points, provided that memory is declared volatile.
 */
CLOGS_LOCAL unsigned int getWarpSizeMem(const cl::Device &device);

/**
 * Subgroups of this size are expected to be scheduled as SIMD, making it
 * worth avoiding branch divergence below this level. Unlike getWarpSizeMem,
 * this is purely a hint and does not affect correctness.
 */
CLOGS_LOCAL unsigned int getWarpSizeSchedule(const cl::Device &device);

template<typename T>
static inline std::string toString(const T &x)
{
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << x;
    return o.str();
}

CLOGS_LOCAL cl::Program build(
    const cl::Context &context,
    const std::vector<cl::Device> &devices,
    const std::string &filename,
    const std::map<std::string, int> &defines,
    const std::map<std::string, std::string> &stringDefines,
    const std::string &options = "");

template<typename T>
static inline T roundDownPower2(T x)
{
    T y = 1;
    while (y * 2 <= x)
        y <<= 1;
    return y;
}

template<typename T>
static inline T roundDown(T x, T y)
{
    return x / y * y;
}

template<typename T>
static inline T roundUp(T x, T y)
{
    return (x + y - 1) / y * y;
}

} // namespace detail
} // namespace clogs

#endif /* !UTILS_H */

// windows.h defines these as macros, which break when used as std::min
#ifdef min
# undef min
#endif
#ifdef max
# undef max
#endif
