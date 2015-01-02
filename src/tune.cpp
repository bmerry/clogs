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
#include "utils.h"
#include "cache.h"

namespace clogs
{
namespace detail
{

TunerBase::TunerBase()
{
}

void TunerBase::logStartGroup()
{
}

void TunerBase::logEndGroup()
{
    std::cout << std::endl;
}

void TunerBase::logStartTest()
{
}

void TunerBase::logEndTest(bool success, double rate)
{
    (void) rate;
    std::cout << "!."[success] << std::flush;
}

void TunerBase::logResult()
{
}

boost::any TunerBase::tuneOne(
    const cl::Device &device,
    const std::vector<boost::any> &parameterSets,
    const std::vector<std::size_t> &problemSizes,
    FUNCTIONAL_NAMESPACE::function<
    std::pair<double, double>(
        const cl::Context &,
        const cl::Device &,
        std::size_t,
        const boost::any &)> callback,
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
    return boost::any();
}

} // namespace detail
} // namespace clogs
