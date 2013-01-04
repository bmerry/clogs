/* Copyright (c) 2012-2013 University of Cape Town
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
#include <stdexcept>
#include <clogs/visibility_pop.h>

#include "parameters.h"

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
 * Exception thrown when autotuning parameters could not be saved.
 */
class CLOGS_API SaveParametersError : public std::runtime_error
{
private:
    const std::string &filename;
    int err;
public:
    SaveParametersError(const std::string &filename, int err);

    const std::string &getFilename() const;
    int getError() const;
};

/**
 * Create a key with fields uniquely describing @a device.
 */
CLOGS_LOCAL ParameterSet deviceKey(const cl::Device &device);

/**
 * Look up tuning parameters for a specific algorithm.
 *
 * @param key             Lookup key, including algorithm and device-specific fields
 * @param[in,out] params  On input, uninitialized parameters. Initialized on output
 *
 * @throw CacheError if the cache did not exist or could not be read
 */
CLOGS_LOCAL void getParameters(const ParameterSet &key, ParameterSet &params);

/**
 * Generate the tuning parameters for all algorithms on all devices.
 * This is not thread-safe (or even multi-process safe).
 */
CLOGS_API void tuneAll();

} // namespace detail
} // namespace clogs

#endif /* TUNE_H */
