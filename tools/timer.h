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
 * Simple timer functions.
 */

#ifndef CLOGS_TIMER_H
#define CLOGS_TIMER_H

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_CLOCK_GETTIME
# ifndef _POSIX_C_SOURCE
#  define _POSIX_C_SOURCE 200112L
# endif
# include <time.h>
# define TIMER_TYPE_POSIX 1
#elif HAVE_QUERY_PERFORMANCE_COUNTER
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <windows.h>
# define TIMER_TYPE_WINDOWS 1
#else
# error "No timer implementation found"
#endif


#include <time.h>

/**
 * Simple timer that measures elapsed time. Under POSIX, it uses the realtime
 * monotonic timer, and so it may be necessary to pass @c -lrt when linking.
 * Under Windows it uses QueryPerformanceCounter.
 */
class Timer
{
private:
#if TIMER_TYPE_POSIX
    struct timespec start;
#endif
#if TIMER_TYPE_WINDOWS
    LARGE_INTEGER start;
#endif

public:
    /**
     * Constructor. Starts the timer.
     */
    Timer();

    /// Get elapsed time since the timer was constructed
    double getElapsed() const;
};

#endif /* CLOGS_TIMER_H */
