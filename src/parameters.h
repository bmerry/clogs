/* Copyright (c) 2012-2014 University of Cape Town
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
 * Utilities for passing around generic sets of key/value parameters.
 */

#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <clogs/visibility_push.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstddef>
#include <locale>
#include <cassert>
#include <boost/preprocessor.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/utility/enable_if.hpp>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "sqlite3.h"

namespace clogs
{
namespace detail
{

CLOGS_LOCAL int bindFields(sqlite3_stmt *stmt, int pos, sqlite3_int64 value);
CLOGS_LOCAL int bindFields(sqlite3_stmt *stmt, int pos, const std::string &value);
CLOGS_LOCAL int bindFields(sqlite3_stmt *stmt, int pos, const std::vector<unsigned char> &value);

CLOGS_LOCAL int readFields(sqlite3_stmt *stmt, int pos, std::string &value);
CLOGS_LOCAL int readFields(sqlite3_stmt *stmt, int pos, std::vector<unsigned char> &value);

template<typename T>
typename boost::enable_if<boost::is_integral<T>, int>::type
static inline readFields(sqlite3_stmt *stmt, int pos, T &value)
{
    assert(pos >= 0 && pos < sqlite3_column_count(stmt));
    assert(sqlite3_column_type(stmt, pos) == SQLITE_INTEGER);
    value = sqlite3_column_int64(stmt, pos);
    return pos + 1;
}

template<typename T>
static inline void fieldNames(const T *, const char *root, std::vector<const char *> &out)
{
    out.push_back(root);
}

template<typename T>
static inline typename boost::enable_if<boost::is_integral<T> >::type
fieldTypes(const T *, std::vector<const char *> &out)
{
    out.push_back("INT");
}

static inline void fieldTypes(const std::string *, std::vector<const char *> &out)
{
    out.push_back("TEXT");
}

static inline void fieldTypes(const std::vector<unsigned char> *, std::vector<const char *> &out)
{
    out.push_back("BLOB");
}

#define CLOGS_BIND_FIELD(r, s, field)                                   \
    pos = bindFields(stmt, pos, (s).field);

#define CLOGS_READ_FIELD(r, s, field)                                   \
    pos = readFields(stmt, pos, (s).field);

#define CLOGS_FIELD_NAME(r, s, field)                                   \
    fieldNames(&dummy->field, BOOST_PP_STRINGIZE(field), out);

#define CLOGS_FIELD_TYPE(r, name, field)                                \
    fieldTypes(&dummy->field, out);

#define CLOGS_FIELD_COMPARE(r, s, field)                                \
    if (a.field < b.field) return true;                                 \
    if (b.field < a.field) return false;

#define CLOGS_STRUCT_FORWARD(name)                                      \
    int bindFields(sqlite3_stmt *stmt, int pos, const name &s);         \
    int readFields(sqlite3_stmt *stmt, int pos, name &s);               \
    void fieldNames(const name *, const char *root, std::vector<const char *> &out); \
    void fieldTypes(const name *, std::vector<const char *> &out);      \
    bool operator<(const name &a, const name &b);

#define CLOGS_STRUCT(name, fields)                                      \
    int bindFields(sqlite3_stmt *stmt, int pos, const name &s)          \
    {                                                                   \
        BOOST_PP_SEQ_FOR_EACH(CLOGS_BIND_FIELD, s, fields);             \
        return pos;                                                     \
    }                                                                   \
    int readFields(sqlite3_stmt *stmt, int pos, name &s)                \
    {                                                                   \
        BOOST_PP_SEQ_FOR_EACH(CLOGS_READ_FIELD, s, fields)              \
        return pos;                                                     \
    }                                                                   \
    void fieldNames(const name *dummy, const char *, std::vector<const char *> &out) \
    {                                                                   \
        BOOST_PP_SEQ_FOR_EACH(CLOGS_FIELD_NAME, name, fields)           \
    }                                                                   \
    void fieldTypes(const name *dummy, std::vector<const char *> &out)  \
    {                                                                   \
        BOOST_PP_SEQ_FOR_EACH(CLOGS_FIELD_TYPE, name, fields)           \
    }                                                                   \
    bool operator<(const name &a, const name &b)                        \
    {                                                                   \
        BOOST_PP_SEQ_FOR_EACH(CLOGS_FIELD_COMPARE, name, fields)        \
        return false;                                                   \
    }

struct CLOGS_LOCAL DeviceKey
{
    std::string platformName;
    std::string deviceName;
    cl_uint deviceVendorId;
    std::string driverVersion;
};

CLOGS_STRUCT_FORWARD(DeviceKey)

} // namespace detail
} // namespace clogs

#endif /* PARAMETERS_H */
