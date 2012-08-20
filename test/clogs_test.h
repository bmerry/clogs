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
 * Common functions to support testing CLOGS.
 */

#ifndef CLOGS_TEST_CLOGS_H
#define CLOGS_TEST_CLOGS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <cppunit/extensions/HelperMacros.h>
#include <boost/tr1/functional.hpp>
#include <boost/tr1/random.hpp>
#include <string>
#include <vector>
#include <cstddef>
#include <CL/cl.hpp>
#include <clogs/clogs.h>
#ifdef min
 #undef min
#endif
#ifdef max
# undef max
#endif

namespace clogs
{

namespace Test
{

/**
 * A compile-time encapsulation of an CL C that can be stored in buffers.
 *
 * Note that the C API types are not unique (@c cl_half is the same C++ type as
 * @c cl_ushort, and @c cl_uint3 is the same as @c cl_uint4), so the actual
 * type cannot be used to specify the semantic type at compile time. Instead, we
 * use the enumeration and a vector length count.
 *
 * This is the generic template, which is specialized for each support type.
 */
template<BaseType Base, unsigned int Length = 1>
class TypeTag
{
public:
    /// The C API type
    typedef void type;
    /// The C API type of vector elements (same as @ref type for scalars).
    typedef void scalarType;
    /// The runtime element type.
    static const BaseType baseType = TYPE_VOID;
    /// The vector length (1 for scalars, 0 for void).
    static const unsigned int length = 0;
    /// Whether this class has been specialized from the generic template.
    static const bool is_specialized = false;
    /// The runtime form of the type.
    static Type makeType() { return Type(); }
};

template<unsigned int Length>
class TypeTag<TYPE_VOID, Length>
{
public:
    typedef void type;
    typedef void scalarType;
    static const bool is_specialized = true;
    static const BaseType baseType = TYPE_VOID;
    static const unsigned int length = 0;
    static Type makeType() { return Type(); }
};

namespace detail
{

/// Implementation of @ref clogs::TypeTag for scalar types.
template<BaseType Base, typename T>
class TypeTagScalar
{
public:
    typedef T type;
    typedef T scalarType;
    static const bool is_specialized = true;
    static const BaseType baseType = Base;
    static const unsigned int length = 1;
    static Type makeType() { return Type(Base); }
    static T &access(T &x, unsigned int) { return x; }
    static const T &access(const T &x, unsigned int idx) { return x; }
    static bool equal(const T &x, const T &y) { return x == y; }
    static std::ostream &format(std::ostream &o, const T &x) { return (o << x); }
};

/// Implementation of @ref clogs::TypeTag for vector types.
template<BaseType Base, unsigned int Length, typename T, typename S>
class TypeTagVector
{
public:
    typedef T type;
    typedef S scalarType;
    static const BaseType baseType = Base;
    static const unsigned int length = Length;
    static Type makeType() { return Type(Base, Length); }
    static S &access(T &x, unsigned int idx) { return x.s[idx]; }
    static const S &access(const T &x, unsigned int idx) { return x.s[idx]; }
    static bool equal(const T &x, const T &y)
    {
        for (unsigned int i = 0; i < length; i++)
            if (x.s[i] != y.s[i])
                return false;
        return true;
    }
    static std::ostream &format(std::ostream &o, const T &x)
    {
        o << '(';
        for (unsigned int i = 0; i < length; i++)
        {
            if (i > 0)
                o << ", ";
            clogs::Test::TypeTag<Base, 1>::format(o, access(x, i));
        }
        o << ')';
        return o;
    }
};

} // namespace detail

#define DECLARE_TYPE_TAG(base_, stype_) \
    template<> class TypeTag<base_, 1> : public detail::TypeTagScalar<base_, stype_> {}; \
    template<> class TypeTag<base_, 2> : public detail::TypeTagVector<base_, 2, stype_ ## 2, stype_> {}; \
    template<> class TypeTag<base_, 3> : public detail::TypeTagVector<base_, 3, stype_ ## 3, stype_> {}; \
    template<> class TypeTag<base_, 4> : public detail::TypeTagVector<base_, 4, stype_ ## 4, stype_> {}; \
    template<> class TypeTag<base_, 8> : public detail::TypeTagVector<base_, 8, stype_ ## 8, stype_> {}; \
    template<> class TypeTag<base_, 16> : public detail::TypeTagVector<base_, 16, stype_ ## 16, stype_> {}
DECLARE_TYPE_TAG(TYPE_UCHAR, cl_uchar);
DECLARE_TYPE_TAG(TYPE_CHAR, cl_char);
DECLARE_TYPE_TAG(TYPE_USHORT, cl_ushort);
DECLARE_TYPE_TAG(TYPE_SHORT, cl_short);
DECLARE_TYPE_TAG(TYPE_UINT, cl_uint);
DECLARE_TYPE_TAG(TYPE_INT, cl_int);
DECLARE_TYPE_TAG(TYPE_ULONG, cl_ulong);
DECLARE_TYPE_TAG(TYPE_LONG, cl_long);
DECLARE_TYPE_TAG(TYPE_FLOAT, cl_float);
DECLARE_TYPE_TAG(TYPE_DOUBLE, cl_double);
// cl_ext.h is missing the typedefs for vector types, so we only support half with scalar for now.
// TODO: override equal and format
template<> class TypeTag<TYPE_HALF, 1> : public detail::TypeTagScalar<TYPE_HALF, cl_half> {};

#undef DECLARE_TYPE_TAG

/**
 * Utility class for moving data of unknown type to and from buffers.
 * It also provides helpers for checking equality, accessing vector elements
 * and gracefully deals with a void type for algorithms that have optional
 * associated metadata (e.g. sorting).
 *
 * The @a Tag must be a specialization of @ref clogs::TypeTag.
 */
template<typename Tag>
class Array : public std::vector<typename Tag::type>
{
public:
    typedef typename Tag::type value_type;
    typedef typename std::vector<value_type>::size_type size_type;

    /// Default constructor.
    Array() : std::vector<value_type>() {}

    /// Constructor with a specified size and optional initial value.
    explicit Array(size_type size, const value_type &init = value_type())
        : std::vector<value_type>(size, init) {}

    /**
     * Constructor that populates with random values.
     * @todo Currently this will only work with integral types.
     */
    Array(std::tr1::mt19937 &engine, size_t size,
          typename Tag::scalarType minValue = std::numeric_limits<typename Tag::scalarType>::min(),
          typename Tag::scalarType maxValue = std::numeric_limits<typename Tag::scalarType>::max())
        : std::vector<value_type>(size)
    {
        typedef typename Tag::scalarType T;
        std::tr1::uniform_int<T> dist(minValue, maxValue);
        std::tr1::variate_generator<std::tr1::mt19937 &, std::tr1::uniform_int<T> > gen(engine, dist);
        for (size_t i = 0; i < size; i++)
            for (size_t j = 0; j < Tag::length; j++)
            {
                Tag::access((*this)[i], j) = gen();
            }
    }

    /// Load from an OpenCL buffer
    Array(cl::CommandQueue &queue, const cl::Buffer &buffer, size_t size)
        : std::vector<value_type>(size)
    {
        download(queue, buffer);
    }

    /// Create an OpenCL buffer with the same content
    cl::Buffer upload(cl::Context &context, cl_mem_flags flags) const
    {
        return cl::Buffer(context, flags | CL_MEM_COPY_HOST_PTR, this->size() * sizeof(value_type), (void *) &(*this)[0]);
    }

    /// Copy contents from an OpenCL buffer into this array
    void download(cl::CommandQueue &queue, const cl::Buffer &buffer)
    {
        queue.enqueueReadBuffer(buffer, CL_TRUE, 0, this->size() * sizeof(value_type), &(*this)[0]);
    }

    /// Asserts (in the CppUnit sense) that two arrays are equal
    void checkEqual(const std::vector<value_type> &other, CppUnit::SourceLine sourceLine) const
    {
        std::ostringstream amsg;
        std::ostringstream bmsg;
        if (this->size() != other.size())
        {
            amsg << this->size();
            bmsg << other.size();
            CppUnit::Asserter::failNotEqual(amsg.str(), bmsg.str(), sourceLine, "sizes differ");
        }
        for (typename std::vector<value_type>::size_type i = 0; i < this->size(); i++)
        {
            if (!Tag::equal((*this)[i], other[i]))
            {
                std::ostringstream extra;
                Tag::format(amsg, (*this)[i]);
                Tag::format(bmsg, other[i]);
                extra << "differ at position " << i;
                CppUnit::Asserter::failNotEqual(amsg.str(), bmsg.str(), sourceLine, extra.str());
            }
        }
    }
};

template<unsigned int Length>
class Array<clogs::Test::TypeTag<clogs::TYPE_VOID, Length> >
{
public:
    struct value_type {};

public:
    typedef std::size_t size_type;

    Array() {}
    explicit Array(size_type) {}
    Array(std::tr1::mt19937 &, size_t) {}
    Array(cl::CommandQueue &, const cl::Buffer &, size_t) {}
    cl::Buffer upload(cl::Context &, cl_mem_flags) const { return cl::Buffer(); }
    void download(cl::CommandQueue &, const cl::Buffer &) {}
    void checkEqual(const Array<clogs::Test::TypeTag<clogs::TYPE_VOID, Length> > &, CppUnit::SourceLine) const {}

    /// Dummy operator[] to allow for syntax like copying one array into another
    value_type operator[](size_type) { return value_type(); }
    value_type operator[](size_type) const { return value_type(); }
};

/**
 * A generalization of CppUnit's @c TestCaller class that takes a generic
 * function object instead of just a void member function. This allows test
 * cases to be generated by binding parameters to a method call.
 */
template<class Fixture>
class TestCaller : public CppUnit::TestCaller<Fixture>
{
    typedef std::tr1::function<void(Fixture *)> TestFunction;
public:
    TestCaller(std::string name, TestFunction test, Fixture &fixture) :
        CppUnit::TestCaller<Fixture>(name, NULL, fixture), fixture(&fixture), test(test)
    {
    }

    TestCaller(std::string name, TestFunction test, Fixture *fixture) :
        CppUnit::TestCaller<Fixture>(name, NULL, fixture), fixture(fixture), test(test)
    {
    }

    void runTest()
    {
        test(fixture);
    }

private:
    Fixture *fixture;
    TestFunction test;
};

#define CLOGS_TEST_BIND(member, ...) \
    CPPUNIT_TEST_SUITE_ADD_TEST( (new clogs::Test::TestCaller<TestFixtureType>( \
                context.getTestNameFor(#member) + "::" #__VA_ARGS__, boost::bind(&TestFixtureType::member, _1, __VA_ARGS__), context.makeFixture()) ) )

#define CLOGS_TEST_BIND_NAME(member, name, ...) \
    CPPUNIT_TEST_SUITE_ADD_TEST( (new clogs::Test::TestCaller<TestFixtureType>( \
                context.getTestNameFor(#member) + "::" + (name), boost::bind(&TestFixtureType::member, _1, __VA_ARGS__), context.makeFixture()) ) )

#define CLOGS_TEST_BIND_NAME_FULL(member, name, ...) \
    CPPUNIT_TEST_SUITE_ADD_TEST( (new clogs::Test::TestCaller<TestFixtureType>( \
                context.getTestNameFor((name)), boost::bind(&TestFixtureType::member, _1, __VA_ARGS__), context.makeFixture()) ) )


/**
 * Custom assertion to compare the contents of two vectors. This function is not expected
 * to be called directly. Use @ref CLOGS_ASSERT_VECTORS_EQUAL instead.
 *
 * @param a,b          Vectors to compare.
 * @param sourceLine   Line number information for caller.
 */
template<typename T>
void checkVectorsEqual(const std::vector<T> &a, const std::vector<T> &b, CppUnit::SourceLine sourceLine)
{
    std::ostringstream amsg;
    std::ostringstream bmsg;
    if (a.size() != b.size())
    {
        amsg << a.size();
        bmsg << b.size();
        CppUnit::Asserter::failNotEqual(amsg.str(), bmsg.str(), sourceLine, "sizes differ");
    }
    for (typename std::vector<T>::size_type i = 0; i < a.size(); i++)
    {
        if (a[i] != b[i])
        {
            std::ostringstream extra;
            amsg << a[i];
            bmsg << b[i];
            extra << "differ at position " << i;
            CppUnit::Asserter::failNotEqual(amsg.str(), bmsg.str(), sourceLine, extra.str());
        }
    }
}

/**
 * @hideinitializer
 * Custom assertion that two vectors are equal. Use this in place of @c CPPUNIT_ASSERT_EQUAL
 * when comparing vectors.
 */
#define CLOGS_ASSERT_VECTORS_EQUAL(expected, actual) \
    ::clogs::Test::checkVectorsEqual((expected), (actual), CPPUNIT_SOURCELINE())

/**
 * Test fixture class that handles OpenCL setup.
 *
 * The provided command queue is guaranteed to be in-order. Create a
 * separate command queue if out-of-order execution is needed.
 */
class TestFixture : public CppUnit::TestFixture
{
protected:
    cl::Context context;           ///< OpenCL context
    cl::Device device;             ///< OpenCL device
    cl::CommandQueue queue;        ///< OpenCL command queue
public:
    void setUp();                  ///< Create context, etc.
    void tearDown();               ///< Release context, etc.
};

} // namespace Test
} // namespace clogs

#endif /* CLOGS_TEST_CLOGS_H */
