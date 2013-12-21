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
 * Utility functions that are private to the library.
 */

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <locale>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "utils.h"

namespace clogs
{
namespace detail
{

bool deviceHasExtension(const cl::Device &device, const std::string &extension)
{
    std::string extensions = device.getInfo<CL_DEVICE_EXTENSIONS>();
    std::string::size_type pos = extensions.find(extension);
    bool found = false;
    while (pos != std::string::npos)
    {
        if ((pos == 0 || extensions[pos - 1] == ' ')
            && (pos + extension.size() == extensions.size() || extensions[pos + extension.size()] == ' '))
        {
            found = true;
            break;
        }
        pos = extensions.find(extension, pos + 1);
    }
    return found;
}

unsigned int getWarpSizeMem(const cl::Device &device)
{
    /* According to an AMD engineer, AMD GPU wavefronts do not guarantee the
     * synchronization semantics implied by this function, so we do not
     * try to detect them.
     */
    if (deviceHasExtension(device, "cl_nv_device_attribute_query"))
        return device.getInfo<CL_DEVICE_WARP_SIZE_NV>();
    else
        return 1U;
}

unsigned int getWarpSizeSchedule(const cl::Device &device)
{
    if (deviceHasExtension(device, "cl_nv_device_attribute_query"))
        return device.getInfo<CL_DEVICE_WARP_SIZE_NV>();
    else
    {
        cl::Platform platform(device.getInfo<CL_DEVICE_PLATFORM>());
        if (platform.getInfo<CL_PLATFORM_NAME>() == "AMD Accelerated Parallel Processing")
        {
            if (device.getInfo<CL_DEVICE_TYPE>() & CL_DEVICE_TYPE_GPU)
                return 64U; // true for many AMD GPUs, not all
            else
                return 1U;  // might eventually need to change if autovectorization is done
        }
    }
    return 1U;
}

cl::Program build(
    const cl::Context &context,
    const std::vector<cl::Device> &devices,
    const std::string &filename,
    const std::map<std::string, int> &defines,
    const std::map<std::string, std::string> &stringDefines,
    const std::string &options)
{
    const std::map<std::string, std::string> &sourceMap = detail::getSourceMap();
    if (!sourceMap.count(filename))
        throw std::invalid_argument("No such program " + filename);
    const std::string &source = sourceMap.find(filename)->second;

    std::ostringstream s;
    s.imbue(std::locale::classic());
    for (std::map<std::string, int>::const_iterator i = defines.begin(); i != defines.end(); ++i)
    {
        s << "#define " << i->first << " " << i->second << "\n";
    }
    for (std::map<std::string, std::string>::const_iterator i = stringDefines.begin();
         i != stringDefines.end(); ++i)
    {
        s << "#define " << i->first << " " << i->second << "\n";
    }
    s << "#line 1 \"" << filename << "\"\n";
    const std::string header = s.str();
    cl::Program::Sources sources(2);
    sources[0] = std::make_pair(header.data(), header.length());
    sources[1] = std::make_pair(source.data(), source.length());
    cl::Program program(context, sources);

    bool failed = false;
    try
    {
        int status = program.build(devices, options.c_str());
        if (status != 0)
            failed = true;
    }
    catch (cl::Error &e)
    {
        failed = true;
    }

    if (failed)
    {
        std::ostringstream msg;
        msg << "Internal error compiling " << filename << '\n';
        for (std::vector<cl::Device>::const_iterator device = devices.begin(); device != devices.end(); ++device)
        {
            const std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(*device);
            if (log != "" && log != "\n")
            {
                msg << "Log for device " << device->getInfo<CL_DEVICE_NAME>() << '\n';
                msg << log << '\n';
            }
        }
        throw InternalError(msg.str());
    }

    return program;
}

} // namespace detail
} // namespace clogs
