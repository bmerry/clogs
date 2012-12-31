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

#ifndef TUNE_H
#define TUNE_H

#include <clogs/visibility_push.h>
#include <iostream>
#include <string>
#include <map>
#include <cstddef>
#include <clogs/visibility_pop.h>

namespace clogs
{
namespace detail
{

class CLOGS_LOCAL Parameter
{
    friend std::ostream &operator<<(std::ostream &o, const Parameter &param);
    friend std::istream &operator>>(std::istream &i, Parameter &param);
private:
    virtual std::ostream &write(std::ostream &o) const = 0;
    virtual std::istream &read(std::istream &i) = 0;

public:
    virtual ~Parameter();
    virtual Parameter *clone() const = 0;
};

CLOGS_LOCAL std::ostream &operator<<(std::ostream &o, const Parameter &param);
CLOGS_LOCAL std::istream &operator>>(std::istream &i, Parameter &param);

template<typename T>
class CLOGS_LOCAL TypedParameter : public Parameter
{
private:
    T value;

    virtual std::ostream &write(std::ostream &o) const { return o << value; }
    virtual std::istream &read(std::istream &i) { return i >> value; }
public:
    explicit TypedParameter(const T &value = T()) : value(value) {}
    T get() const { return value; }
    void set(const T &value) { this->value = value; }
    virtual Parameter *clone() const { return new TypedParameter<T>(*this); }
};

class CLOGS_LOCAL StringParameter : public Parameter
{
private:
    std::string value;

    virtual std::ostream &write(std::ostream &o) const;
    virtual std::istream &read(std::istream &i) const;
    virtual Parameter *clone() const;

public:
    explicit StringParameter(const std::string &value = std::string());
    const std::string &get() const;
    void set(const std::string &value);
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

/**
 * Look up tuning parameters for a specific algorithm.
 */
CLOGS_LOCAL void getParameters(const std::string &algorithm, const ParameterSet &key, ParameterSet &out);

} // namespace detail
} // namespace clogs

#endif /* TUNE_H */
