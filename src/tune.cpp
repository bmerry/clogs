/* Copyright (c) 2012-2014 University of Cape Town
 * Copyright (c) 2014, Bruce Merry
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
#include "clhpp11.h"
#include <string>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <cmath>
#include <utility>
#include <set>
#include <vector>
#include <locale>
#if CLOGS_FS_UNIX
# include <sys/stat.h>
#endif
#if CLOGS_FS_WINDOWS
# include <shlobj.h>
# include <winnls.h>
# include <shlwapi.h>
#endif
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "tune.h"
#include "parameters.h"
#include "scan.h"
#include "radixsort.h"
#include "utils.h"
#include "sqlite3.h"

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

CLOGS_LOCAL ParameterSet deviceKey(const cl::Device &device)
{
    ParameterSet key;
    cl::Platform platform(device.getInfo<CL_DEVICE_PLATFORM>());
    key["CL_PLATFORM_NAME"] = new TypedParameter<std::string>(platform.getInfo<CL_PLATFORM_NAME>());
    key["CL_DEVICE_NAME"] = new TypedParameter<std::string>(device.getInfo<CL_DEVICE_NAME>());
    key["CL_DEVICE_VENDOR_ID"] = new TypedParameter<cl_uint>(device.getInfo<CL_DEVICE_VENDOR_ID>());
    key["CL_DRIVER_VERSION"] = new TypedParameter<std::string>(device.getInfo<CL_DRIVER_VERSION>());
    return key;
}

namespace
{

#if CLOGS_FS_UNIX
typedef char path_char;
typedef std::string path_string;
static const char dirSep = '/';
#endif
#if CLOGS_FS_WINDOWS
typedef wchar_t path_char;
typedef std::wstring path_string;
static const wchar_t dirSep = L'\\';
#endif

/**
 * Determines the cache directory, without caching the result. The
 * directory is created if it does not exist, but failure to create it is not
 * reported.
 *
 * @todo Need to document the algorithm in the user manual
 *
 * @todo Store cache directory in UTF-8
 */
static path_string getCacheDirStatic();

#if CLOGS_FS_UNIX
static path_string getCacheDirStatic()
{
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

static std::string fromPathString(const path_string &s)
{
    return s;
}
#endif

#if CLOGS_FS_WINDOWS
static path_string getCacheDirStatic()
{
    const wchar_t *envCacheDir = _wgetenv(L"CLOGS_CACHE_DIR");
    if (envCacheDir == NULL)
    {
        bool success = false;
        wchar_t path[MAX_PATH + 20];
        HRESULT status = SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
                                         NULL, SHGFP_TYPE_CURRENT, path);
        if (PathAppend(path, L"clogs"))
        {
            CreateDirectory(path, NULL);
            if (PathAppend(path, L"cache"))
            {
                CreateDirectory(path, NULL);
                success = true;
            }
        }
        if (!success)
            path[0] = L'\0';
        return path;
    }
    else
        return envCacheDir;
}

static std::string fromPathString(const path_string &s)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, NULL, 0, NULL, NULL);
    char *out = new char[len];
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out, len, NULL, NULL);
    std::string ret(out);
    delete[] out;
    return ret;
}
#endif

/**
 * Returns the cache directory, caching the result after the first time.
 *
 * @see getCacheDirStatic.
 */
static path_string getCacheDir()
{
    static const path_string ans = getCacheDirStatic();
    return ans;
}

/**
 * Class for connection to the database. There is only ever one instance of
 * this class, which handles initialization and shutdown.
 */
class DB
{
private:
    // prevent copying
    DB(const DB &);
    DB &operator=(const DB &);
public:
    sqlite3 *con;

    explicit DB(const std::string &basename);
    ~DB();
};

DB::DB(const std::string &basename)
{
    con = NULL;
    const path_string path = fromPathString(getCacheDir() + dirSep) + basename;
    int status = sqlite3_open_v2(path.c_str(), &con,
                                 SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                                 NULL);
    if (status != SQLITE_OK)
    {
        CacheError error(sqlite3_errstr(status));
        if (con != NULL)
            sqlite3_close(con);
        throw error;
    }
}

DB::~DB()
{
    if (con != NULL)
    {
        int status = sqlite3_close(con);
        con = NULL;
        if (status != SQLITE_OK)
        {
            std::cerr << sqlite3_errstr(status) << '\n';
        }
    }
}

static DB &getDB()
{
    static DB db("cache.sqlite");
    return db;
}

static std::string tableName(const ParameterSet &key)
{
    std::string algorithm = key.getTyped<std::string>("algorithm")->get();
    int version = key.getTyped<int>("version")->get();
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << algorithm << "_v" << version;
    return out.str();
}

static void createTable(const ParameterSet &key, const ParameterSet &values)
{
    std::ostringstream statement;
    statement.imbue(std::locale::classic());
    statement << "CREATE TABLE IF NOT EXISTS " << tableName(key) << " (";
    for (ParameterSet::const_iterator i = key.begin(); i != key.end(); ++i)
    {
        if (i->first == "algorithm" || i->first == "version")
            continue; // these are encoded into the table name
        statement << i->first << ' ' << i->second->sql_type() << ", ";
    }
    for (ParameterSet::const_iterator i = values.begin(); i != values.end(); ++i)
    {
        statement << i->first << ' ' << i->second->sql_type() << ", ";
    }

    statement << "PRIMARY KEY(";
    bool first = true;
    for (ParameterSet::const_iterator i = key.begin(); i != key.end(); ++i)
    {
        if (i->first == "algorithm" || i->first == "version")
            continue; // these are encoded into the table name
        if (!first)
            statement << ", ";
        first = false;
        statement << i->first;
    }

    statement << "))";

    std::string s = statement.str();
    DB &db = getDB();
    char *err = NULL;
    int status = sqlite3_exec(db.con, s.c_str(), NULL, NULL, &err);
    if (status != SQLITE_OK || err != NULL)
    {
        CacheError error(s + ": " + err);
        sqlite3_free(err);
        throw error;
    }
}

/**
 * Write computed parameters to file.
 *
 * @param key            Algorithm key
 * @param params         Autotuned parameters.
 *
 * @throw SaveParametersError if the file could not be written
 */
static void saveParameters(const ParameterSet &key, const ParameterSet &values)
{
    createTable(key, values);

    std::ostringstream statement;
    statement.imbue(std::locale::classic());
    statement << "INSERT OR REPLACE INTO " << tableName(key) << "(";
    bool first = true;
    for (ParameterSet::const_iterator i = key.begin(); i != key.end(); ++i)
    {
        if (i->first == "algorithm" || i->first == "version")
            continue; // these are encoded into the table name
        if (!first)
            statement << ", ";
        first = false;
        statement << i->first;
    }
    for (ParameterSet::const_iterator i = values.begin(); i != values.end(); ++i)
    {
        if (!first)
            statement << ", ";
        first = false;
        statement << i->first;
    }

    statement << ") VALUES (";
    first = true;
    for (ParameterSet::const_iterator i = key.begin(); i != key.end(); ++i)
    {
        if (i->first == "algorithm" || i->first == "version")
            continue; // these are encoded into the table name
        if (!first)
            statement << ", ";
        first = false;
        statement << '?';
    }
    for (ParameterSet::const_iterator i = values.begin(); i != values.end(); ++i)
    {
        if (!first)
            statement << ", ";
        first = false;
        statement << '?';
    }
    statement << ')';

    DB &db = getDB();
    sqlite3_stmt *stmt = NULL;
    int status = sqlite3_prepare_v2(db.con, statement.str().c_str(), -1, &stmt, NULL);
    if (status != SQLITE_OK)
        throw CacheError(sqlite3_errstr(status));

    int pos = 1;
    for (ParameterSet::const_iterator i = key.begin(); i != key.end(); ++i)
    {
        if (i->first == "algorithm" || i->first == "version")
            continue; // these are encoded into the table name
        i->second->sql_bind(stmt, pos);
        pos++;
    }
    for (ParameterSet::const_iterator i = values.begin(); i != values.end(); ++i)
    {
        i->second->sql_bind(stmt, pos);
        pos++;
    }
    status = sqlite3_step(stmt);
    if (status != SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        throw CacheError(sqlite3_errstr(status));
    }

    status = sqlite3_finalize(stmt);
    if (status != SQLITE_OK)
        throw CacheError(sqlite3_errstr(status));
}

} // anonymous namespace

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
            ParameterSet key = Scan::makeKey(device, problem);
            bool doit = true;
            if (!seen.insert(key).second)
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
                    ParameterSet params = Scan::tune(*this, device, problem);
                    saveParameters(key, params);
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
                    ParameterSet key = Radixsort::makeKey(device, problem);
                    bool doit = true;
                    if (!seen.insert(key).second)
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
                            ParameterSet params = Radixsort::tune(*this, device, problem);
                            saveParameters(key, params);
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

void Tuner::logStartTest(const ParameterSet &params)
{
    (void) params;
}

void Tuner::logEndTest(const ParameterSet &params, bool success, double rate)
{
    (void) params;
    (void) rate;
    std::cout << "!."[success] << std::flush;
}

void Tuner::logResult(const ParameterSet &params)
{
    // std::cout << params << std::endl;
    (void) params;
}

ParameterSet Tuner::tuneOne(
    const cl::Device &device,
    const std::vector<ParameterSet> &parameterSets,
    const std::vector<std::size_t> &problemSizes,
    FUNCTIONAL_NAMESPACE::function<
    std::pair<double, double>(
        const cl::Context &,
        const cl::Device &,
        std::size_t,
        ParameterSet &)> callback,
    double ratio)
{
    std::vector<ParameterSet> retained = parameterSets;
    for (std::size_t pass = 0; pass < problemSizes.size(); pass++)
    {
        logStartGroup();
        const std::size_t problemSize = problemSizes[pass];
        std::vector<std::pair<double, double> > results;
        results.reserve(retained.size());
        std::vector<ParameterSet> retained2;
        double maxA = -HUGE_VAL;
        for (std::size_t i = 0; i < retained.size(); i++)
        {
            ParameterSet &params = retained[i];
            logStartTest(params);
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
                    logEndTest(params, true, r.first);
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
                logEndTest(params, false, 0.0);
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

CLOGS_LOCAL void getParameters(const ParameterSet &key, ParameterSet &params)
{
    DB &db = getDB();
    createTable(key, params);

    std::ostringstream query;
    query.imbue(std::locale::classic());
    query << "SELECT ";
    bool first = true;
    for (ParameterSet::const_iterator i = params.begin(); i != params.end(); ++i)
    {
        if (!first)
            query << ", ";
        first = false;
        query << i->first;
    }

    query << " FROM " << tableName(key) << " WHERE ";
    first = true;
    for (ParameterSet::const_iterator i = key.begin(); i != key.end(); ++i)
    {
        if (i->first == "algorithm" || i->first == "version")
            continue; // these are encoded into the table name
        if (!first)
            query << " AND ";
        first = false;
        query << i->first << "=?";
    }

    sqlite3_stmt *stmt = NULL;
    int status = sqlite3_prepare_v2(db.con, query.str().c_str(), -1, &stmt, NULL);
    if (status != SQLITE_OK)
        throw CacheError(sqlite3_errstr(status));
    int pos = 1;
    for (ParameterSet::const_iterator i = key.begin(); i != key.end(); ++i)
    {
        if (i->first == "algorithm" || i->first == "version")
            continue; // these are encoded into the table name
        i->second->sql_bind(stmt, pos);
        pos++;
    }

    status = sqlite3_step(stmt);
    if (status == SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        throw CacheError("No cache entry found");
    }
    else if (status != SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        throw CacheError(sqlite3_errstr(status));
    }
    else
    {
        int column = 0;
        for (ParameterSet::iterator i = params.begin(); i != params.end(); ++i)
        {
            i->second->sql_get(stmt, column);
            column++;
        }
        sqlite3_finalize(stmt);
    }
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
