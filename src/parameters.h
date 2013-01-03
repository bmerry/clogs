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
 * Utilities for passing around generic sets of key/value parameters.
 */

#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <clogs/visibility_push.h>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <cstddef>
#include <locale>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>

namespace clogs
{
namespace detail
{

class CLOGS_LOCAL Parameter
{
public:
    virtual ~Parameter();
    virtual Parameter *clone() const = 0;

    virtual std::string serialize() const = 0;
    virtual void deserialize(const std::string &serial) = 0;
};

template<typename T>
class CLOGS_LOCAL TypedSerializer
{
public:
    typedef std::string result_type;

    std::string operator()(const T &x) const
    {
        std::ostringstream o;
        o.imbue(std::locale::classic());
        o << x;
        return o.str();
    }
};

template<typename T>
class CLOGS_LOCAL TypedDeserializer
{
public:
    typedef T result_type;

    T operator()(const std::string &s) const
    {
        T ans;
        std::istringstream in(s);
        in.imbue(std::locale::classic());
        in >> ans;
        if (!in || !in.eof())
            throw CacheError("invalid formatting");
        return ans;
    }
};

template<> class CLOGS_LOCAL TypedSerializer<std::string>
{
public:
    typedef std::string result_type;

    std::string operator()(const std::string &x) const;
};

template<> class CLOGS_LOCAL TypedDeserializer<std::string>
{
public:
    typedef std::string result_type;

    std::string operator()(const std::string &s) const;
};

template<typename T>
class CLOGS_LOCAL TypedParameter : public Parameter
{
private:
    T value;

public:
    explicit TypedParameter(const T &value = T()) : value(value) {}
    T get() const { return value; }
    void set(const T &value) { this->value = value; }
    virtual Parameter *clone() const { return new TypedParameter<T>(*this); }

    virtual std::string serialize() const
    {
        return TypedSerializer<T>()(value);
    }
    virtual void deserialize(const std::string &s)
    {
        value = TypedDeserializer<T>()(s);
    }
};

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

    static std::string hash(const std::string &plain);
    std::string hash() const;

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

CLOGS_LOCAL std::ostream &operator<<(std::ostream &o, const ParameterSet &params);

} // namespace detail
} // namespace clogs

#endif /* PARAMETERS_H */
