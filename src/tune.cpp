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
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <set>
#include <sys/stat.h>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "tune.h"
#include "parameters.h"
#include "scan.h"

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

CLOGS_LOCAL void deviceKey(const cl::Device &device, ParameterSet &key)
{
    key["CL_DEVICE_NAME"] = new TypedParameter<std::string>(device.getInfo<CL_DEVICE_NAME>());
    key["CL_DEVICE_VENDOR_ID"] = new TypedParameter<cl_uint>(device.getInfo<CL_DEVICE_VENDOR_ID>());
    key["CL_DRIVER_VERSION"] = new TypedParameter<std::string>(device.getInfo<CL_DRIVER_VERSION>());
}

static std::string getCacheDirStatic()
{
    // TODO: handle non-UNIX systems
    const char *envCacheDir = getenv("CLOGS_CACHE_DIR");
    if (envCacheDir == NULL)
    {
        const char *home = getenv("HOME");
        if (home == NULL)
            home = "";
        std::string clogsDir = std::string(home) + "/.clogs";
        mkdir(clogsDir.c_str(), 0777);
        std::string cacheDir = clogsDir + "/cache";
        mkdir(cacheDir.c_str(), 0777);
        return cacheDir;
    }
    else
        return envCacheDir;
}

static std::string getCacheDir()
{
    static const std::string ans = getCacheDirStatic();
    return ans;
}

static std::string getCacheFile(const std::string &algorithm, const ParameterSet &key)
{
    const std::string hash = key.hash();
    const std::string path = getCacheDir() + "/" + algorithm + "-" + hash;
    return path;
}

static void saveParameters(const std::string &algorithm, const ParameterSet &key, const ParameterSet &values)
{
    const std::string hash = key.hash();
    const std::string path = getCacheFile(algorithm, key);
    std::ofstream out(path.c_str());
    if (!out)
        throw SaveParametersError(path, errno);
    out << values;
    out.close();
    if (!out)
    {
        // TODO: erase the file?
        throw SaveParametersError(path, errno);
    }
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
        saveParameters("scan", key, params);
    }
}

CLOGS_API void tuneDevice(const cl::Context &context, const cl::Device &device)
{
    tuneDeviceScan(context, device);
}

static void parseParameters(std::istream &in, ParameterSet &params)
{
    std::string line;
    std::set<std::string> seen;
    while (getline(in, line))
    {
        std::string::size_type p = line.find('=');
        if (p == std::string::npos)
        {
            throw CacheError("line does not contain equals sign");
        }
        const std::string key = line.substr(0, p);
        if (!params.count(key))
            throw CacheError("unknown key `" + key + "'");
        if (seen.count(key))
            throw CacheError("duplicate key `" + key + "'");
        seen.insert(key);
        std::istringstream sub(line.substr(p + 1));
        sub >> *params[key];
        if (!sub)
            throw CacheError("could not parse value for " + key);
    }
    if (seen.size() < params.size())
        throw CacheError("missing key");
    if (in.bad())
        throw CacheError(strerror(errno));
}

CLOGS_LOCAL void getParameters(const std::string &algorithm, const ParameterSet &key, ParameterSet &params)
{
    std::string filename = getCacheFile(algorithm, key);
    try
    {
        std::ifstream in(filename.c_str());
        if (!in)
        {
            throw CacheError(strerror(errno));
        }
        parseParameters(in, params);
    }
    catch (std::runtime_error &e)
    {
        throw CacheError("Failed to read cache file " + filename + ": " + e.what());
    }
}

} // namespace detail
} // namespace clogs
