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

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <string>
#include <fstream>
#include <iostream>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "tune.h"
#include "parameters.h"
#include "scan.h"

namespace clogs
{
namespace detail
{

CLOGS_LOCAL void getParameters(const std::string &algorithm, const ParameterSet &key, ParameterSet &out)
{
}

CLOGS_LOCAL void deviceKey(const cl::Device &device, ParameterSet &key)
{
    key["CL_DEVICE_NAME"] = new TypedParameter<std::string>(device.getInfo<CL_DEVICE_NAME>());
    key["CL_DEVICE_VENDOR_ID"] = new TypedParameter<cl_uint>(device.getInfo<CL_DEVICE_VENDOR_ID>());
    key["CL_DRIVER_VERSION"] = new TypedParameter<std::string>(device.getInfo<CL_DRIVER_VERSION>());
}

static void tuneDeviceScan(const cl::Context &context, const cl::Device &device)
{
    std::vector<Type> types = Scan::types(device);
    for (std::size_t i = 0; i < types.size(); i++)
    {
        const Type &type = types[i];
        std::cout << "Tuning scan for " << type.getName() << " on " << device.getInfo<CL_DEVICE_NAME>() << std::endl;

        ParameterSet key;
        Scan::makeKey(key, device, type);
        const std::string hash = key.hash();
        ParameterSet params;
        Scan::tune(params, context, device, type);
    }
}

CLOGS_API void tuneDevice(const cl::Context &context, const cl::Device &device)
{
    tuneDeviceScan(context, device);
}

} // namespace detail
} // namespace clogs
