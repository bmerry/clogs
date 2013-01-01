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
 * Utilities for autotuning
 */

#ifndef TUNE_H
#define TUNE_H

#include <clogs/visibility_push.h>
#include <string>
#include "parameters.h"
#include <clogs/visibility_pop.h>

namespace cl
{

class Context;
class Device;

} // namespace cl

namespace clogs
{
namespace detail
{

/**
 * Look up tuning parameters for a specific algorithm.
 */
CLOGS_LOCAL void getParameters(const std::string &algorithm, const ParameterSet &key, ParameterSet &out);

/**
 * Generate the tuning parameters for all algorithms on a specific device.
 * This is not thread-safe (or even multi-process safe).
 */
CLOGS_API void tuneDevice(const cl::Context &context, const cl::Device &device);

} // namespace detail
} // namespace clogs

#endif /* TUNE_H */
