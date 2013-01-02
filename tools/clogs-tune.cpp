/* Copyright (c) 2013 University of Cape Town
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

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <CL/cl.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <vector>
#include "../src/tune.h"
#include "../src/parameters.h"

static void tuneDevice(const cl::Platform &platform, const cl::Device &device)
{
    cl_context_properties props[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) platform(), 0};
    std::vector<cl::Device> devices(1, device);
    cl::Context context(devices, props, NULL);

    clogs::detail::tuneDevice(context, device);
}

static void tunePlatform(const cl::Platform &platform)
{
    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
    BOOST_FOREACH(const cl::Device &device, devices)
    {
        tuneDevice(platform, device);
    }
}

int main(int argc, char **argv)
{
    try
    {
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        BOOST_FOREACH(const cl::Platform &platform, platforms)
        {
            tunePlatform(platform);
        }
    }
    catch (clogs::detail::SaveParametersError &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
