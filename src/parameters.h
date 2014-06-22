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
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "sqlite3.h"

namespace clogs
{
namespace detail
{

class CLOGS_LOCAL Parameter
{
public:
    virtual ~Parameter();
    virtual Parameter *clone() const = 0;

    virtual std::string sql_type() const = 0;
    virtual int sql_bind(sqlite3_stmt *stmt, int pos) const = 0;
    virtual void sql_get(sqlite3_stmt *stmt, int column) = 0;
};

template<typename T>
class CLOGS_LOCAL SqlTraits;

// TODO: document these, move to .cpp file, and remove sql_ prefix

template<>
class CLOGS_LOCAL SqlTraits<int>
{
public:
    static std::string sql_type() { return "INT"; }
    static int sql_bind(sqlite3_stmt *stmt, int pos, int value)
    {
        return sqlite3_bind_int(stmt, pos, value);
    }
    static int sql_get(sqlite3_stmt *stmt, int column)
    {
        assert(column >= 0 && column < sqlite3_column_count(stmt));
        assert(sqlite3_column_type(stmt, column) == SQLITE_INTEGER);
        return sqlite3_column_int(stmt, column);
    }
};

template<>
class CLOGS_LOCAL SqlTraits<unsigned int> : public SqlTraits<int>
{
};

template<>
class CLOGS_LOCAL SqlTraits< ::size_t>
{
public:
    static std::string sql_type() { return "INT"; }
    static int sql_bind(sqlite3_stmt *stmt, int pos, ::size_t value)
    {
        return sqlite3_bind_int64(stmt, pos, value);
    }
    static ::size_t sql_get(sqlite3_stmt *stmt, int column)
    {
        assert(column >= 0 && column < sqlite3_column_count(stmt));
        assert(sqlite3_column_type(stmt, column) == SQLITE_INTEGER);
        return sqlite3_column_int64(stmt, column);
    }
};

template<>
class CLOGS_LOCAL SqlTraits<std::string>
{
public:
    static std::string sql_type() { return "TEXT"; }
    static int sql_bind(sqlite3_stmt *stmt, int pos, const std::string &value)
    {
        return sqlite3_bind_text(stmt, pos, value.data(), value.size(), SQLITE_TRANSIENT);
    }
    static std::string sql_get(sqlite3_stmt *stmt, int column)
    {
        assert(column >= 0 && column < sqlite3_column_count(stmt));
        assert(sqlite3_column_type(stmt, column) == SQLITE_TEXT);
        const char *data = (const char *) sqlite3_column_text(stmt, column);
        ::size_t size = sqlite3_column_bytes(stmt, column);
        return std::string(data, size);
    }
};

template<>
class CLOGS_LOCAL SqlTraits<std::vector<unsigned char> >
{
public:
    static std::string sql_type() { return "BLOB"; }
    static int sql_bind(sqlite3_stmt *stmt, int pos, const std::vector<unsigned char> &value)
    {
        const unsigned char dummy = 0;
        return sqlite3_bind_blob(
            stmt, pos,
            static_cast<const void *>(value.empty() ? &dummy : &value[0]),
            value.size(), SQLITE_TRANSIENT);
    }
    static std::vector<unsigned char> sql_get(sqlite3_stmt *stmt, int column)
    {
        assert(column >= 0 && column < sqlite3_column_count(stmt));
        assert(sqlite3_column_type(stmt, column) == SQLITE_BLOB);
        const unsigned char *data = (const unsigned char *) sqlite3_column_blob(stmt, column);
        ::size_t size = sqlite3_column_bytes(stmt, column);
        return std::vector<unsigned char>(data, data + size);
    }
};

template<typename T>
class CLOGS_LOCAL TypedParameter : public Parameter
{
private:
    T value;

public:
    explicit TypedParameter(const T &value = T()) : value(value) {}
    T get() const { return value; }
    T &get() { return value; }
    void set(const T &value) { this->value = value; }
    virtual Parameter *clone() const { return new TypedParameter<T>(*this); }

    virtual std::string sql_type() const
    {
        return SqlTraits<T>::sql_type();
    }

    virtual int sql_bind(sqlite3_stmt *stmt, int pos) const
    {
        return SqlTraits<T>::sql_bind(stmt, pos, value);
    }

    virtual void sql_get(sqlite3_stmt *stmt, int column)
    {
        value = SqlTraits<T>::sql_get(stmt, column);
    }
};

/**
 * A key/value collection of parameters that can be serialized. The keys are
 * strings while the values are of arbitrary type.
 */
class CLOGS_LOCAL ParameterSet : public std::map<std::string, Parameter *>
{
public:
    /// Default constructor
    ParameterSet();
    /// Copy constructor
    ParameterSet(const ParameterSet &params);
    /// Assignment operator
    ParameterSet &operator=(const ParameterSet &params);
    ~ParameterSet();

    template<typename T> const TypedParameter<T> *getTyped(const std::string &name) const
    {
        const_iterator pos = find(name);
        if (pos != end())
            return dynamic_cast<const TypedParameter<T> *>(pos->second);
        else
            return NULL;
    }

    template<typename T> TypedParameter<T> *getTyped(const std::string &name)
    {
        iterator pos = find(name);
        if (pos != end())
            return dynamic_cast<TypedParameter<T> *>(pos->second);
        else
            return NULL;
    }
};

} // namespace detail
} // namespace clogs

#endif /* PARAMETERS_H */
