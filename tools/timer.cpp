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

#include "timer.h"

#if TIMER_TYPE_POSIX

Timer::Timer()
{
    clock_gettime(CLOCK_MONOTONIC, &start);
}

double Timer::getElapsed() const
{
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = end.tv_sec - start.tv_sec + 1e-9 * (end.tv_nsec - start.tv_nsec);
    return elapsed;
}

#endif // TIMER_TYPE_POSIX

#if TIMER_TYPE_WINDOWS

Timer::Timer()
{
    BOOL ret = QueryPerformanceCounter(&start);
    if (!ret)
        throw std::runtime_error("QueryPerformanceCounter failed");
}

double Timer::getElapsed() const
{
    LARGE_INTEGER end;
    LARGE_INTEGER freq;
    BOOL ret;
    ret = QueryPerformanceCounter(&end);
    if (!ret)
        throw std::runtime_error("QueryPerformanceCounter failed");
    ret = QueryPerformanceFrequency(&freq);
    if (!ret)
        throw std::runtime_error("QueryPerformanceFrequency failed");
    return (double) (end.QuadPart - start.QuadPart) / freq.QuadPart;
}

#endif // TIMER_TYPE_WINDOWS
