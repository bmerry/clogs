/* Copyright (c) 2012-2014 University of Cape Town
 * Copyright (c) 2014 Bruce Merry
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

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <clogs/visibility_push.h>
#include "clhpp11.h"
#include <string>
#include <iostream>
#include <cassert>
#include <cmath>
#include <utility>
#include <set>
#include <vector>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "tune.h"
#include "parameters.h"
#include "scan.h"
#include "reduce.h"
#include "radixsort.h"
#include "utils.h"
#include "cache.h"

namespace clogs
{
namespace detail
{

SaveParametersError::SaveParametersError(const std::string &filename, int err)
    : std::runtime_error(filename + ": " + strerror(err)),
    filename(filename), err(err)
{
}

const std::string &SaveParametersError::getFilename() const
{
    return filename;
}

int SaveParametersError::getError() const
{
    return err;
}

Tuner::Tuner() : force(true), keepGoing(false)
{
}

void Tuner::setForce(bool force)
{
    this->force = force;
}

void Tuner::setKeepGoing(bool keepGoing)
{
    this->keepGoing = keepGoing;
}

/**
 * Tune the scan algorithm for a device.
 */
void Tuner::tuneScan(const cl::Context &context, const cl::Device &device)
{
    const std::vector<Type> types = Type::allTypes();
    for (std::size_t i = 0; i < types.size(); i++)
    {
        const Type &type = types[i];
        if (Scan::typeSupported(device, type))
        {
            ScanProblem problem;
            problem.setType(type);
            ScanParameters::Key key = Scan::makeKey(device, problem);
            bool doit = true;
            if (!seenScan.insert(key).second)
                doit = false; // already done in this round of tuning
            else if (!force)
            {
                /* Catches are all no-ops: doit = false will not be reached.
                 * Note that we catch InternalError and not just CacheError, in
                 * case some driver change now makes the generated kernel
                 * invalid. We also catch cl::Error in case a kernel causes
                 * CL_OUT_OF_RESOURCES for some reason (which does happen).
                 */
                try
                {
                    Scan scan(context, device, problem);
                    doit = false;
                }
                catch (cl::Error &e)
                {
                }
                catch (InternalError &e)
                {
                }
            }
            if (doit)
            {
                std::cout << "Tuning scan for " << type.getName() << " elements on " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

                try
                {
                    ScanParameters::Value values = Scan::tune(*this, device, problem);
                    getDB().scan.add(key, values);
                }
                catch (TuneError &e)
                {
                    if (keepGoing)
                        std::cerr << "WARNING: " << e.what() << std::endl;
                    else
                        throw;
                }
            }
        }
    }
}

/**
 * Tune the reduction algorithm for a device.
 */
void Tuner::tuneReduce(const cl::Context &context, const cl::Device &device)
{
    const std::vector<Type> types = Type::allTypes();
    for (std::size_t i = 0; i < types.size(); i++)
    {
        const Type &type = types[i];
        if (Reduce::typeSupported(device, type))
        {
            ReduceProblem problem;
            problem.setType(type);
            ReduceParameters::Key key = Reduce::makeKey(device, problem);
            bool doit = true;
            if (!seenReduce.insert(key).second)
                doit = false; // already done in this round of tuning
            else if (!force)
            {
                /* Catches are all no-ops: doit = false will not be reached.
                 * Note that we catch InternalError and not just CacheError, in
                 * case some driver change now makes the generated kernel
                 * invalid. We also catch cl::Error in case a kernel causes
                 * CL_OUT_OF_RESOURCES for some reason (which does happen).
                 */
                try
                {
                    Reduce reduce(context, device, problem);
                    doit = false;
                }
                catch (cl::Error &e)
                {
                }
                catch (InternalError &e)
                {
                }
            }

            if (doit)
            {
                std::cout << "Tuning reduce for " << type.getName() << " elements on " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

                try
                {
                    ReduceParameters::Value values = Reduce::tune(*this, device, problem);
                    getDB().reduce.add(key, values);
                }
                catch (TuneError &e)
                {
                    if (keepGoing)
                        std::cerr << "WARNING: " << e.what() << std::endl;
                    else
                        throw;
                }
            }
        }
    }
}

/**
 * Tune the radix sort algorithm for a device.
 */
void Tuner::tuneRadixsort(const cl::Context &context, const cl::Device &device)
{
    const std::vector<Type> types = Type::allTypes();
    for (std::size_t i = 0; i < types.size(); i++)
    {
        const Type &keyType = types[i];
        if (Radixsort::keyTypeSupported(device, keyType))
        {
            for (std::size_t j = 0; j < types.size(); j++)
            {
                const Type &valueType = types[j];
                if (Radixsort::valueTypeSupported(device, valueType))
                {
                    RadixsortProblem problem;
                    problem.setKeyType(keyType);
                    problem.setValueType(valueType);
                    RadixsortParameters::Key key = Radixsort::makeKey(device, problem);
                    bool doit = true;
                    if (!seenRadixsort.insert(key).second)
                        doit = false; // already done in this round of tuning
                    else if (!force)
                    {
                        // See comments in tuneScan
                        try
                        {
                            Radixsort sort(context, device, problem);
                            doit = false;
                        }
                        catch (cl::Error &e)
                        {
                        }
                        catch (InternalError &e)
                        {
                        }
                    }
                    if (doit)
                    {
                        std::cout
                            << "Tuning radixsort for " << keyType.getName() << " keys and "
                            << valueType.getSize() << " byte values on " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

                        try
                        {
                            RadixsortParameters::Value values = Radixsort::tune(*this, device, problem);
                            getDB().radixsort.add(key, values);
                        }
                        catch (TuneError &e)
                        {
                            if (keepGoing)
                                std::cerr << "WARNING: " << e.what() << std::endl;
                            else
                                throw;
                        }
                    }
                }
            }
        }
    }
}

void Tuner::tuneDevice(const cl::Device &device)
{
    cl_context_properties props[3] =
    {
        CL_CONTEXT_PLATFORM,
        (cl_context_properties) device.getInfo<CL_DEVICE_PLATFORM>(),
        0
    };
    std::vector<cl::Device> devices(1, device);
    cl::Context context(devices, props, NULL);

    tuneScan(context, device);
    tuneReduce(context, device);
    tuneRadixsort(context, device);
}

void Tuner::tuneAll(const std::vector<cl::Device> &devices)
{
    for (std::size_t i = 0; i < devices.size(); i++)
    {
        tuneDevice(devices[i]);
    }
}

void Tuner::logStartGroup()
{
}

void Tuner::logEndGroup()
{
    std::cout << std::endl;
}

void Tuner::logStartTest()
{
}

void Tuner::logEndTest(bool success, double rate)
{
    (void) rate;
    std::cout << "!."[success] << std::flush;
}

void Tuner::logResult()
{
}

boost::any Tuner::tuneOne(
    const cl::Device &device,
    const std::vector<boost::any> &parameterSets,
    const std::vector<std::size_t> &problemSizes,
    FUNCTIONAL_NAMESPACE::function<
    std::pair<double, double>(
        const cl::Context &,
        const cl::Device &,
        std::size_t,
        boost::any &)> callback,
    double ratio)
{
    std::vector<boost::any> retained = parameterSets;
    for (std::size_t pass = 0; pass < problemSizes.size(); pass++)
    {
        logStartGroup();
        const std::size_t problemSize = problemSizes[pass];
        std::vector<std::pair<double, double> > results;
        results.reserve(retained.size());
        std::vector<boost::any> retained2;
        double maxA = -HUGE_VAL;
        for (std::size_t i = 0; i < retained.size(); i++)
        {
            boost::any &params = retained[i];
            logStartTest();
            bool valid = false;
            try
            {
                cl::Context context = contextForDevice(device);
                std::pair<double, double> r = callback(context, device, problemSize, params);
                if (r.first == r.first) // filter out NaN
                {
                    assert(r.first <= r.second);
                    retained2.push_back(params);
                    results.push_back(r);
                    if (r.first > maxA)
                        maxA = r.first;
                    logEndTest(true, r.first);
                    valid = true;
                }
            }
            catch (InternalError &e)
            {
            }
            catch (cl::Error &e)
            {
            }
            if (!valid)
                logEndTest(false, 0.0);
        }
        retained.swap(retained2);
        retained2.clear();
        if (retained.empty())
        {
            throw TuneError("no suitable kernel found");
        }
        logEndGroup();

        if (pass < problemSizes.size() - 1)
        {
            for (std::size_t i = 0; i < results.size(); i++)
                if (results[i].first >= ratio * maxA)
                    retained2.push_back(retained[i]);
            retained.swap(retained2);
            retained2.clear();
        }
        else
        {
            for (std::size_t i = 0; i < results.size(); i++)
                if (results[i].second >= maxA)
                    return retained[i];
        }
    }
    abort(); // should never be reached due to A <= B
}

CLOGS_API void tuneAll(const std::vector<cl::Device> &devices, bool force, bool keepGoing)
{
    Tuner tuner;
    tuner.setForce(force);
    tuner.setKeepGoing(keepGoing);
    tuner.tuneAll(devices);
}

} // namespace detail
} // namespace clogs
