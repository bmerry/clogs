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

#include <clogs/visibility_push.h>
#include <string>
#include <istream>
#include <ostream>
#include <sstream>
#include <map>
#include <locale>
#include <memory>
#include <clogs/visibility_pop.h>

#include "parameters.h"
#include "md5.h"
#include "base64_encode.h"
#include "base64_decode.h"

namespace clogs
{
namespace detail
{

Parameter::~Parameter() {}

std::string TypedSerializer<std::string>::operator()(const std::string &x) const
{
    return base64encode(x);
}

std::string TypedDeserializer<std::string>::operator()(const std::string &s) const
{
    try
    {
        return base64decode(s);
    }
    catch (Base64DecodeError &e)
    {
        throw CacheError(e.what());
    }
}

ParameterSet::ParameterSet()
{
}

ParameterSet::ParameterSet(const ParameterSet &params) : std::map<std::string, Parameter *>()
{
    try
    {
        *this = params;
    }
    catch (...)
    {
        for (iterator i = begin(); i != end(); ++i)
            delete i->second;
        throw;
    }
}

ParameterSet &ParameterSet::operator=(const ParameterSet &params)
{
    for (iterator i = begin(); i != end(); ++i)
        delete i->second;
    clear();
    for (const_iterator i = params.begin(); i != params.end(); ++i)
    {
        std::auto_ptr<Parameter> clone(i->second->clone());
        insert(std::make_pair(i->first, clone.get()));
        clone.release();
    }
    return *this;
}

ParameterSet::~ParameterSet()
{
    for (iterator i = begin(); i != end(); ++i)
        delete i->second;
}

std::string ParameterSet::hash(const std::string &plain)
{
    static const char hex[] = "0123456789abcdef";
    md5_byte_t digest[16];
    md5_state_t pms;
    md5_init(&pms);
    md5_append(&pms, reinterpret_cast<const md5_byte_t *>(plain.data()), plain.size());
    md5_finish(&pms, digest);
    std::string ans(32, ' ');
    for (int i = 0; i < 16; i++)
    {
        ans[2 * i] = hex[digest[i] >> 4];
        ans[2 * i + 1] = hex[digest[i] & 0xf];
    }
    return ans;
}

std::string ParameterSet::hash() const
{
    std::ostringstream plain;
    plain.imbue(std::locale::classic());
    plain << *this;
    return hash(plain.str());
}

bool ParameterSet::operator==(const ParameterSet &other) const
{
    const_iterator a = begin(), b = other.begin();
    while (a != end() && b != other.end())
    {
        if (a->first != b->first)
            return false;
        if (a->second->serialize() != b->second->serialize())
            return false;
        ++a;
        ++b;
    }
    return a == end() && b == other.end();
}

bool ParameterSet::operator<(const ParameterSet &other) const
{
    const_iterator a = begin(), b = other.begin();
    while (a != end() && b != other.end())
    {
        if (a->first != b->first)
            return a->first < b->first;
        const std::string av = a->second->serialize();
        const std::string bv = b->second->serialize();
        if (av != bv)
            return av < bv;
        ++a;
        ++b;
    }
    return b != other.end();
}

bool ParameterSet::operator!=(const ParameterSet &other) const
{
    return !(*this == other);
}

bool ParameterSet::operator>(const ParameterSet &other) const
{
    return other < *this;
}

bool ParameterSet::operator<=(const ParameterSet &other) const
{
    return !(other < *this);
}

bool ParameterSet::operator>=(const ParameterSet &other) const
{
    return !(*this < other);
}

std::ostream &operator<<(std::ostream &o, const ParameterSet &params)
{
    for (ParameterSet::const_iterator i = params.begin(); i != params.end(); ++i)
    {
        o << i->first << '=' << i->second->serialize() << '\n';
    }
    return o;
}

CLOGS_LOCAL ParameterSet getParameters(const std::string &algorithm, const ParameterSet &key);

} // namespace detail
} // namespace clogs
