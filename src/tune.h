/* Copyright (c) 2012-2014 University of Cape Town
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
#include <vector>
#include <set>
#include <boost/any.hpp>
#include "tr1_functional.h"
#include <clogs/visibility_pop.h>

#include "parameters.h"
#include "scan.h"
#include "radixsort.h"

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
 * Exception thrown when a configuration could not be tuned at all.
 */
class CLOGS_API TuneError : public std::runtime_error
{
public:
    TuneError(const std::string &msg) : std::runtime_error(msg) {}
};

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
CLOGS_LOCAL DeviceKey deviceKey(const cl::Device &device);

/**
 * Look up tuning parameters for a scan
 *
 * @param key             Lookup key
 * @param[out] params     Found parameters
 *
 * @throw CacheError if the cache entry did not exist or could not be read
 */
CLOGS_LOCAL void getScanParameters(const ScanParameters::Key &key, ScanParameters::Value &values);

/**
 * Look up tuning parameters for a radixsort
 *
 * @param key             Lookup key
 * @param[out] params     Found parameters
 *
 * @throw CacheError if the cache entry did not exist or could not be read
 */
CLOGS_LOCAL void getRadixsortParameters(const RadixsortParameters::Key &key, RadixsortParameters::Value &values);

/**
 * Generate the tuning parameters for all algorithms.
 * This is not thread-safe (or even multi-process safe).
 *
 * @param devices   Devices to tune on.
 * @param force     Whether to re-tune configurations that have already been tuned.
 * @param keepGoing Whether to carry on after encounting an otherwise fatal error.
 */
CLOGS_API void tuneAll(const std::vector<cl::Device> &devices, bool force, bool keepGoing);

class CLOGS_LOCAL Tuner
{
private:
    std::set<ScanParameters::Key> seenScan;
    std::set<RadixsortParameters::Key> seenRadixsort;
    bool force;
    bool keepGoing;

    void tuneScan(const cl::Context &context, const cl::Device &device);
    void tuneRadixsort(const cl::Context &context, const cl::Device &device);

    void tuneDevice(const cl::Device &context);

public:
    Tuner();

    void setForce(bool force);
    void setKeepGoing(bool keepGoing);
    void tuneAll(const std::vector<cl::Device> &devices);

    /**
     * Perform low-level tuning. The callback function is called for each set of parameters,
     * and returns two values, A and B. The selected parameter set is computed as follows:
     *
     * -# The largest value of A, Amax is computed.
     * -# The first parameter set with B >= Amax is returned.
     *
     * To simply pick the
     * best, simply return B = A. However, if earlier parameter sets are in some
     * way intrinsicly better, then setting e.g. B = 1.05 * A will yield a
     * parameter set that has A ~= Amax but possibly much earlier. It is required
     * that A <= B.
     *
     * @a problemSizes contains values to pass to the callback. A separate phase is
     * run for each value in sequence. In the first phase, all parameter sets are
     * used. In each subsequent phase, only those whose A value was at least
     * ratio*Amax are retained. This allows for very slow parameter sets to be
     * quickly eliminated on small problem sizes (which can also avoid hardware
     * timeouts), before refining the selection on more representative problem sizes.
     *
     * It is legal for the callback function to throw @c cl::Error or @ref
     * InternalError. In either case, the parameter set will be dropped from consideration.
     *
     * Each call will be made with a fresh context. It is advisable for the
     * callback function to execute a warmup pass to obtain reliable results.
     */
    boost::any tuneOne(
        const cl::Device &device,
        const std::vector<boost::any> &parameterSets,
        const std::vector<std::size_t> &problemSizes,
        FUNCTIONAL_NAMESPACE::function<
        std::pair<double, double>(
            const cl::Context &,
            const cl::Device &,
            std::size_t,
            boost::any &)> callback,
        double ratio = 0.5);

    /**
     * @name Methods called by the tuning algorithms to report progress
     * @{
     */

    /// Called at the beginning of a related set of tuning tests
    void logStartGroup();
    /// Called at the end of a related set of tuning tests
    void logEndGroup();
    /// Called at the start of a single tuning test
    void logStartTest();
    /**
     * Called at the end of a single tuning test.
     * @param success    Whether the test succeeded i.e. did not throw an exception
     * @param rate       Rate at which operations occurred (arbitrary scale)
     */
    void logEndTest(bool success, double rate);
    /**
     * Logs final result of autotuning.
     */
    void logResult();

    /**
     * @}
     */
};

} // namespace detail
} // namespace clogs

#endif /* TUNE_H */
