/**
 * @file
 *
 * Miscellaneous utilities for command-line option processing, shared
 * between the test frontend and clogs-benchmark.
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <CL/cl.hpp>
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

bool findDevice(const boost::program_options::variables_map &vm, cl::Device &device)
{
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
            {
                device = d;
                return true;
            }
        }
    }

    return false;
}

cl::Context makeContext(const cl::Device &device)
{
    const cl::Platform &platform = device.getInfo<CL_DEVICE_PLATFORM>();
    cl_context_properties props[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) platform(), 0};
    return cl::Context(device.getInfo<CL_DEVICE_TYPE>(), props, NULL);
}
