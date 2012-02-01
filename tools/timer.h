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

#include <string>
#include <time.h>

/**
 * Simple timer that measures elapsed time. Under POSIX, it uses the realtime
 * monotonic timer, and so it may be necessary to pass @c -lrt when linking.
 */
class Timer
{
private:
    struct timespec start;

public:
    /// Constructor. Starts the timer.
    Timer();

    /// Get elapsed time since the timer was constructor
    double getElapsed() const;
};

#endif /* CLOGS_TIMER_H */
