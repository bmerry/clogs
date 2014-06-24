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
#include <boost/noncopyable.hpp>
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

CLOGS_LOCAL DeviceKey deviceKey(const cl::Device &device)
{
    DeviceKey key;
    cl::Platform platform(device.getInfo<CL_DEVICE_PLATFORM>());
    key.platformName = platform.getInfo<CL_PLATFORM_NAME>();
    key.deviceName = device.getInfo<CL_DEVICE_NAME>();
    key.deviceVendorId = device.getInfo<CL_DEVICE_VENDOR_ID>();
    key.driverVersion = device.getInfo<CL_DRIVER_VERSION>();
    return key;
}

namespace
{

/**
 * Determines the cache file, without caching the result. The directory is
 * created if it does not exist, but failure to create it is not reported.
 *
 * @todo Need to document the algorithm in the user manual
 *
 * @todo Not XDG-compliant
 */
static std::string getCacheFileStatic();

#if CLOGS_FS_UNIX
/**
 * Retrieve an environment variable as a C++ string. If the environment
 * variable is missing, returns an empty string.
 */
static std::string getenvString(const char *name)
{
    const char *env = getenv(name);
    if (env == NULL)
        env = "";
    return env;
}

static std::string getCacheFileStatic()
{
    std::string cacheDir = getenvString("CLOGS_CACHE_DIR");
    if (cacheDir.empty())
    {
        std::string cacheHome = getenvString("XDG_CACHE_HOME");
        if (cacheHome.empty())
            cacheHome = getenvString("HOME") + "/.cache";
        mkdir(cacheHome.c_str(), 0700);
        cacheDir = cacheHome + "/clogs";
    }
    mkdir(cacheDir.c_str(), 0700);
    return cacheDir + "/cache.sqlite";
}
#endif

#if CLOGS_FS_WINDOWS
static std::string getCacheFileStatic()
{
    const wchar_t *envCacheDir = _wgetenv(L"CLOGS_CACHE_DIR");
    wchar_t path[MAX_PATH + 30] = L"";
    bool success = false;
    if (envCacheDir == NULL)
    {
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
    }
    else
    {
        success = true;
        if (wcslen(envCacheDir) < sizeof(path) / sizeof(path[0]))
        {
            wcscpy(path, envCacheDir);
            success = true;
        }
    }
    if (success && !PathAppend(path, L"cache.sqlite"))
        success = false;
    if (!success)
        path[0] = L'\0';

    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
    if (len == 0) // indicates failure
        return "";
    std::vector<char> out(len);
    WideCharToMultiByte(CP_UTF8, 0, path, -1, &out[0], len, NULL, NULL);
    return std::string(&out[0]);
}
#endif

/**
 * Returns the cache file, caching the result after the first time.
 *
 * @see getCacheFileStatic.
 */
static std::string getCacheFile()
{
    static const std::string ans = getCacheFileStatic();
    return ans;
}

/// RAII wrapper around sqlite3*.
class sqlite3_ptr : public boost::noncopyable
{
private:
    sqlite3 *ptr;

public:
    explicit sqlite3_ptr(sqlite3 *p = NULL) : ptr(p) {}
    ~sqlite3_ptr() { reset(); }

    void reset(sqlite3 *p = NULL)
    {
        if (ptr != NULL && ptr != p)
        {
            int status = sqlite3_close(ptr);
            if (status != SQLITE_OK)
            {
                std::cerr << sqlite3_errstr(status) << '\n';
            }
        }
        ptr = p;
    }

    sqlite3 *get() const { return ptr; }
};

/// RAII wrapper around sqlite3_stmt*.
class sqlite3_stmt_ptr : public boost::noncopyable
{
private:
    sqlite3_stmt *ptr;

public:
    explicit sqlite3_stmt_ptr(sqlite3_stmt *p = NULL) : ptr(p) {}
    ~sqlite3_stmt_ptr() { reset(); }

    void reset(sqlite3_stmt *p = NULL)
    {
        if (ptr != NULL && ptr != p)
        {
            int status = sqlite3_finalize(ptr);
            if (status != SQLITE_OK)
            {
                std::cerr << sqlite3_errstr(status) << '\n';
            }
        }
        ptr = p;
    }

    sqlite3_stmt *get() const { return ptr; }
};

/**
 * Abstraction of a table supporting insertion of one row at a time, and
 * lookup using the primary key. The key type @a K and value type @a V
 * should be declared with #CLOGS_STRUCT.
 *
 * Column names are used without any quoting, so they must not use any SQL
 * keywords.
 */
template<typename K, typename V>
class Table
{
private:
    sqlite3 *con;
    sqlite3_stmt_ptr addStmt, queryStmt;

    /// Create the table if it does not exist
    void createTable(const char *name);
    /// Create the prepared statement for insertions
    void prepareAdd(const char *name);
    /// Create the prepared statement for queries
    void prepareQuery(const char *name);

public:
    /**
     * Constructor. The provided @a con must not be closed before
     * this object is destroyed.
     *
     * @param con      Connection to the database
     * @param table    The name for the table
     */
    Table(sqlite3 *con, const char *table);

    /// Insert a record into the table, replacing any previous one with the same key
    void add(const K &key, const V &value);
    /**
     * Find a record in the table.
     *
     * @param key         Lookup key
     * @param[out] value  Found value
     * @throw clogs::CacheError if the key could not be found
     */
    void lookup(const K &key, V &value);
};

template<typename T>
static void writeFieldDefinitions(std::ostream &statement)
{
    std::vector<const char *> names, types;
    fieldNames((T *) NULL, NULL, names);
    fieldTypes((T *) NULL, types);
    assert(names.size() == types.size());
    for (std::size_t i = 0; i < names.size(); i++)
        statement << names[i] << ' ' << types[i] << ", ";
}

template<typename T>
static void writeFieldNames(std::ostream &statement, const char *sep = ", ", const char *suffix = "")
{
    std::vector<const char *> names;
    fieldNames((T *) NULL, NULL, names);
    for (std::size_t i = 0; i < names.size(); i++)
    {
        if (i > 0)
            statement << sep;
        statement << names[i] << suffix;
    }
}

template<typename K, typename V>
void Table<K, V>::createTable(const char *name)
{
    std::ostringstream statement;
    statement.imbue(std::locale::classic());
    statement << "CREATE TABLE IF NOT EXISTS " << name << " (";
    writeFieldDefinitions<K>(statement);
    writeFieldDefinitions<V>(statement);

    statement << "PRIMARY KEY(";
    writeFieldNames<K>(statement);
    statement << "))";

    const std::string s = statement.str();
    char *err = NULL;
    int status = sqlite3_exec(con, s.c_str(), NULL, NULL, &err);
    if (status != SQLITE_OK || err != NULL)
    {
        CacheError error(s + ": " + err);
        sqlite3_free(err);
        throw error;
    }
}

template<typename K, typename V>
void Table<K, V>::prepareAdd(const char *name)
{
    std::ostringstream statement;
    statement.imbue(std::locale::classic());
    statement << "INSERT OR REPLACE INTO " << name << "(";
    writeFieldNames<K>(statement);
    statement << ", ";
    writeFieldNames<V>(statement);

    statement << ") VALUES (";
    std::vector<const char *> names;
    fieldNames((K *) NULL, NULL, names);
    fieldNames((V *) NULL, NULL, names);
    for (std::size_t i = 0; i < names.size(); i++)
    {
        if (i > 0)
            statement << ", ";
        statement << '?';
    }
    statement << ')';

    sqlite3_stmt *stmt = NULL;
    int status = sqlite3_prepare_v2(con, statement.str().c_str(), -1, &stmt, NULL);
    addStmt.reset(stmt);
    if (status != SQLITE_OK)
        throw CacheError(sqlite3_errstr(status));
}

template<typename K, typename V>
void Table<K, V>::prepareQuery(const char *name)
{
    std::ostringstream query;
    query.imbue(std::locale::classic());
    query << "SELECT ";
    writeFieldNames<V>(query);

    query << " FROM " << name << " WHERE ";
    writeFieldNames<K>(query, " AND ", "=?");

    sqlite3_stmt *stmt = NULL;
    int status = sqlite3_prepare_v2(con, query.str().c_str(), -1, &stmt, NULL);
    queryStmt.reset(stmt);
    if (status != SQLITE_OK)
        throw CacheError(sqlite3_errstr(status));
}

template<typename K, typename V>
void Table<K, V>::add(const K &key, const V &value)
{
    sqlite3_reset(addStmt.get());

    int pos = 1;
    pos = bindFields(addStmt.get(), pos, key);
    pos = bindFields(addStmt.get(), pos, value);

    int status = sqlite3_step(addStmt.get());
    if (status != SQLITE_DONE)
        throw CacheError(sqlite3_errstr(status));
}

template<typename K, typename V>
void Table<K, V>::lookup(const K &key, V &value)
{
    sqlite3_reset(queryStmt.get());

    bindFields(queryStmt.get(), 1, key);
    int status = sqlite3_step(queryStmt.get());
    if (status == SQLITE_DONE)
        throw CacheError("No cache entry found");
    else if (status != SQLITE_ROW)
        throw CacheError(sqlite3_errstr(status));
    else
        readFields(queryStmt.get(), 0, value);
}

template<typename K, typename V>
Table<K, V>::Table(sqlite3 *con, const char *name)
    : con(con)
{
    createTable(name);
    prepareAdd(name);
    prepareQuery(name);
}

/**
 * Class for connection to the database. There is only ever one instance of
 * this class, which handles initialization and shutdown.
 */
class DB
{
private:
    sqlite3_ptr con;
    static sqlite3 *open(); ///< Open the database connection (used by constructor).

public:
    Table<ScanParameters::Key, ScanParameters::Value> scan;
    Table<RadixsortParameters::Key, RadixsortParameters::Value> radixsort;

    DB();
};

sqlite3 *DB::open()
{
    sqlite3 *c = NULL;
    const std::string filename = getCacheFile();
    int status = sqlite3_open_v2(
        filename.c_str(), &c,
        SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
        NULL);
    if (status != SQLITE_OK)
    {
        if (c != NULL)
            sqlite3_close(c);
        throw CacheError(sqlite3_errstr(status));
    }
    return c;
}

DB::DB() :
    con(open()),
    scan(con.get(), ScanParameters::tableName()),
    radixsort(con.get(), RadixsortParameters::tableName())
{
}

/// Retrieve a singleton database instance
static DB &getDB()
{
    static DB db;
    return db;
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

CLOGS_LOCAL void getScanParameters(const ScanParameters::Key &key, ScanParameters::Value &values)
{
    getDB().scan.lookup(key, values);
}

CLOGS_LOCAL void getRadixsortParameters(const RadixsortParameters::Key &key, RadixsortParameters::Value &values)
{
    getDB().radixsort.lookup(key, values);
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
