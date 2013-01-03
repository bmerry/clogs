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
 * OpenCL primitives.
 */

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <clogs/visibility_push.h>
#include <string>
#include <sstream>
#include <stdexcept>
#include <string>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "utils.h"

namespace clogs
{

InternalError::InternalError(const std::string &msg) : std::runtime_error(msg)
{
}

CacheError::CacheError(const std::string &msg) : InternalError(msg)
{
}

Type::Type() : baseType(TYPE_VOID), length(0) {}

Type::Type(BaseType baseType, unsigned int length) : baseType(baseType), length(length)
{
    if (baseType == TYPE_VOID)
        throw std::invalid_argument("clogs::Type cannot be explicitly constructed with void type");
    switch (length)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 8:
    case 16:
        break;
    default:
        throw std::invalid_argument("length is not a valid value");
    }
}

bool Type::isIntegral() const
{
    switch (baseType)
    {
    case TYPE_UCHAR:
    case TYPE_CHAR:
    case TYPE_USHORT:
    case TYPE_SHORT:
    case TYPE_UINT:
    case TYPE_INT:
    case TYPE_ULONG:
    case TYPE_LONG:
        return true;
    case TYPE_VOID:
    case TYPE_HALF:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
        return false;
    }
    // Should never be reached
    return false;
}

bool Type::isSigned() const
{
    switch (baseType)
    {
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_HALF:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
        return true;
    case TYPE_UCHAR:
    case TYPE_USHORT:
    case TYPE_UINT:
    case TYPE_ULONG:
    case TYPE_VOID:
        return false;
    }
    // Should never be reached
    return false;
}

bool Type::isStorable(const cl::Device &device) const
{
    using namespace clogs::detail;

    switch (baseType)
    {
    case TYPE_VOID:
        return false;
    case TYPE_UCHAR:
    case TYPE_CHAR:
        return (length >= 3 || deviceHasExtension(device, "cl_khr_byte_addressable_store"));
    case TYPE_USHORT:
    case TYPE_SHORT:
        return (length >= 2 || deviceHasExtension(device, "cl_khr_byte_addressable_store"));
    case TYPE_HALF:
        // half is always a valid storage type, but since it cannot be loaded or stored
        // without using built-in functions that is fairly meaningless.
        return deviceHasExtension(device, "cl_khr_fp16");
    case TYPE_DOUBLE:
        return deviceHasExtension(device, "cl_khr_fp64");
    case TYPE_UINT:
    case TYPE_INT:
    case TYPE_ULONG:
    case TYPE_LONG:
    case TYPE_FLOAT:
        return true;
    }
    // Should never be reached
    return false;
}

bool Type::isComputable(const cl::Device &device) const
{
    using namespace clogs::detail;

    switch (baseType)
    {
    case TYPE_VOID:
        return false;
    case TYPE_HALF:
        return deviceHasExtension(device, "cl_khr_fp16");
    case TYPE_DOUBLE:
        return deviceHasExtension(device, "cl_khr_fp64");
    case TYPE_UCHAR:
    case TYPE_CHAR:
    case TYPE_USHORT:
    case TYPE_SHORT:
    case TYPE_UINT:
    case TYPE_INT:
    case TYPE_ULONG:
    case TYPE_LONG:
    case TYPE_FLOAT:
        return true;
    }
    // Should never be reached
    return false;
}

::size_t Type::getBaseSize() const
{
    switch (baseType)
    {
    case TYPE_VOID:    return 0;
    case TYPE_UCHAR:   return sizeof(cl_uchar);
    case TYPE_CHAR:    return sizeof(cl_char);
    case TYPE_USHORT:  return sizeof(cl_ushort);
    case TYPE_SHORT:   return sizeof(cl_short);
    case TYPE_UINT:    return sizeof(cl_uint);
    case TYPE_INT:     return sizeof(cl_int);
    case TYPE_ULONG:   return sizeof(cl_ulong);
    case TYPE_LONG:    return sizeof(cl_long);
    case TYPE_HALF:    return sizeof(cl_half);
    case TYPE_FLOAT:   return sizeof(cl_float);
    case TYPE_DOUBLE:  return sizeof(cl_double);
    }
    // Should never be reached
    return 0;
}

::size_t Type::getSize() const
{
    return getBaseSize() * (length == 3 ? 4 : length);
}

std::string Type::getName() const
{
    const char *baseName = NULL;
    switch (baseType)
    {
    case TYPE_VOID:   baseName = "void";   break;
    case TYPE_UCHAR:  baseName = "uchar";  break;
    case TYPE_CHAR:   baseName = "char";   break;
    case TYPE_USHORT: baseName = "ushort"; break;
    case TYPE_SHORT:  baseName = "short";  break;
    case TYPE_UINT:   baseName = "uint";   break;
    case TYPE_INT:    baseName = "int";    break;
    case TYPE_ULONG:  baseName = "ulong";  break;
    case TYPE_LONG:   baseName = "long";   break;
    case TYPE_HALF:   baseName = "half";   break;
    case TYPE_FLOAT:  baseName = "float";  break;
    case TYPE_DOUBLE: baseName = "double"; break;
    }
    if (length <= 1)
        return baseName;
    else
    {
        std::ostringstream s;
        s << baseName << length;
        return s.str();
    }
}

BaseType Type::getBaseType() const
{
    return baseType;
}

unsigned int Type::getLength() const
{
    return length;
}

std::vector<Type> Type::allTypes()
{
    static const int sizes[] = {1, 2, 3, 4, 8, 16};
    std::vector<Type> ans;
    for (int i = int(TYPE_UCHAR); i <= int(TYPE_DOUBLE); i++)
        for (int j = 0; sizeof(sizes) / sizeof(sizes[0]); j++)
        {
            ans.push_back(Type(BaseType(i), sizes[j]));
        }
    return ans;
}

} // namespace clogs
