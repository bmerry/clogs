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
 * Miscellaneous utilities for command-line option processing, shared
 * between the test frontend and clogs-benchmark.
 */

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include "../src/clhpp11.h"
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <vector>
#include "options.h"

using namespace std;
namespace po = boost::program_options;

void addOptions(boost::program_options::options_description &opts)
{
    opts.add_options()
        ("cl-device",     po::value<string>(),        "OpenCL device name")
        ("cl-gpu",                                    "Only search GPU devices")
        ("cl-cpu",                                    "Only search CPU devices");
}

std::vector<cl::Device> findDevices(const boost::program_options::variables_map &vm)
{
    std::vector<cl::Device> ans;

    vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    BOOST_FOREACH(const cl::Platform &platform, platforms)
    {
        vector<cl::Device> devices;
        cl_device_type type = CL_DEVICE_TYPE_ALL;

        platform.getDevices(type, &devices);
        BOOST_FOREACH(const cl::Device &d, devices)
        {
            bool good = true;
            /* Match name if given */
            if (vm.count("cl-device"))
            {
                if (d.getInfo<CL_DEVICE_NAME>() != vm["cl-device"].as<string>())
                    good = false;
            }
            /* Match type if given */
            if (vm.count("cl-gpu"))
            {
                if (!(d.getInfo<CL_DEVICE_TYPE>() & CL_DEVICE_TYPE_GPU))
                    good = false;
            }
            if (vm.count("cl-cpu"))
            {
                if (!(d.getInfo<CL_DEVICE_TYPE>() & CL_DEVICE_TYPE_CPU))
                    good = false;
            }
            /* Require device to be online */
            if (!d.getInfo<CL_DEVICE_AVAILABLE>())
                good = false;
            if (!d.getInfo<CL_DEVICE_COMPILER_AVAILABLE>())
                good = false;
            if (good)
                ans.push_back(d);
        }
    }
    return ans;
}

bool findDevice(const boost::program_options::variables_map &vm, cl::Device &device)
{
    std::vector<cl::Device> devices = findDevices(vm);
    if (!devices.empty())
    {
        device = devices[0];
        return true;
    }
    else
        return false;
}

cl::Context makeContext(const cl::Device &device)
{
    const cl::Platform &platform = device.getInfo<CL_DEVICE_PLATFORM>();
    cl_context_properties props[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) platform(), 0};
    return cl::Context(device.getInfo<CL_DEVICE_TYPE>(), props, NULL);
}
