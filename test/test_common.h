/* Copyright (c) 2014 Bruce Merry
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
 * Tests that are common to all the algorithms.
 */

#ifndef CLOGS_TEST_COMMON_H
#define CLOGS_TEST_COMMON_H

#include <cstddef>
#include <stdexcept>
#include <utility>
#include <boost/smart_ptr/scoped_ptr.hpp>
#include <clogs/core.h>
#include <clogs/platform.h>
#include "clogs_test.h"

namespace clogs
{
namespace Test
{

template<typename T>
class TestCommon : public TestFixture
{
    CPPUNIT_TEST_SUITE(TestCommon<T>);
#ifdef CLOGS_HAVE_RVALUE_REFERENCES
    CPPUNIT_TEST(testMoveConstruct);
    CPPUNIT_TEST(testMoveAssign);
#endif
    CPPUNIT_TEST(testSwap);
    CPPUNIT_TEST_EXCEPTION(testUninitialized, std::logic_error);
    CPPUNIT_TEST_SUITE_END_ABSTRACT();

protected:
    /**
     * Determines whether @a obj has been initialized.
     */
    static bool initialized(const T &obj);

    virtual T *factory() = 0;

#ifdef CLOGS_HAVE_RVALUE_REFERENCES
    void testMoveConstruct();      ///< Test move constructor
    void testMoveAssign();         ///< Test move assignment operator
#endif
    void testSwap();               ///< Test swapping of two objects
    void testUninitialized();      ///< Test error handling when an uninitialized object is used
};

template<typename T>
bool TestCommon<T>::initialized(const T &obj)
{
    // A bit of a nasty hack, because there is no API to access this: we alias
    // it to a layout-compatible structure.
    struct Alias
    {
        void *detail;
    };

    return reinterpret_cast<const Alias *>(&obj)->detail != NULL;
}

#ifdef CLOGS_HAVE_RVALUE_REFERENCES
template<typename T>
void TestCommon<T>::testMoveConstruct()
{
    boost::scoped_ptr<T> s1((factory()));
    T s2(std::move(*s1));
    CPPUNIT_ASSERT(initialized(s2));
    CPPUNIT_ASSERT(!initialized(*s1));
}

template<typename T>
void TestCommon<T>::testMoveAssign()
{
    boost::scoped_ptr<T> s1(factory());
    T s2;
    s2 = std::move(*s1);
    CPPUNIT_ASSERT(initialized(s2));
    CPPUNIT_ASSERT(!initialized(*s1));
}
#endif // CLOGS_HAVE_RVALUE_REFERENCES

template<typename T>
void TestCommon<T>::testSwap()
{
    boost::scoped_ptr<T> s1(factory());
    T s2;
    clogs::swap(*s1, s2);
    CPPUNIT_ASSERT(initialized(s2));
    CPPUNIT_ASSERT(!initialized(*s1));
}

template<typename T>
void TestCommon<T>::testUninitialized()
{
    T obj;
    obj.setEventCallback(eventCallback, NULL);
}

} // namespace Test
} // namespace clogs

#endif /* !CLOGS_TEST_COMMON_H */
