/* Copyright (c) 2015 Bruce Merry
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
 * Test code for tuning infrastructure.
 */

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include "../src/clhpp11.h"
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <clogs/tune.h>
#include "../src/tune.h"
#include "test_common.h"

/// Test that @ref clogs::TunePolicy works
class TestTunePolicy : public clogs::Test::TestFixture
{
    CPPUNIT_TEST_SUITE(TestTunePolicy);
    CPPUNIT_TEST_EXCEPTION(testDisable, clogs::CacheError);
    CPPUNIT_TEST(testVerbositySilent);
    CPPUNIT_TEST(testVerbosityTerse);
    CPPUNIT_TEST(testVerbosityNormal);
    CPPUNIT_TEST_SUITE_END();

private:
    static std::pair<double, double> callback(
        const cl::Context &context,
        const cl::Device &device,
        std::size_t size,
        const boost::any &param);
    std::string getOutput(const clogs::TunePolicy &tunePolicy);

    void testDisable();
    void testVerbositySilent();
    void testVerbosityTerse();
    void testVerbosityNormal();
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestTunePolicy);

std::pair<double, double> TestTunePolicy::callback(
    const cl::Context &context,
    const cl::Device &device,
    std::size_t size,
    const boost::any &param)
{
    (void) context;
    (void) device;
    (void) size;
    double p = boost::any_cast<double>(param);
    if (p < 0)
        throw clogs::InternalError("negative parameter");
    else
        return std::pair<double, double>(p, p);
}

std::string TestTunePolicy::getOutput(const clogs::TunePolicy &tunePolicy)
{
    std::vector<boost::any> parameters;
    parameters.push_back(1.0);
    parameters.push_back(-1.0);
    parameters.push_back(3.0);
    std::vector<size_t> sizes;
    sizes.push_back(1);
    sizes.push_back(2);

    std::ostringstream out;
    clogs::TunePolicy policy = tunePolicy;
    policy.setOutput(out);

    clogs::detail::getDetail(policy).logStartAlgorithm("test", device);
    clogs::detail::tuneOne(clogs::detail::getDetail(policy), device, parameters, sizes, callback);
    clogs::detail::getDetail(policy).logEndAlgorithm();
    return out.str();
}

void TestTunePolicy::testDisable()
{
    clogs::TunePolicy policy;
    policy.setEnabled(false);
    getOutput(policy);
}

void TestTunePolicy::testVerbositySilent()
{
    clogs::TunePolicy policy;
    policy.setVerbosity(clogs::TUNE_VERBOSITY_SILENT);
    std::string out = getOutput(policy);
    CPPUNIT_ASSERT_EQUAL(std::string(""), out);
}

void TestTunePolicy::testVerbosityTerse()
{
    clogs::TunePolicy policy;
    policy.setVerbosity(clogs::TUNE_VERBOSITY_TERSE);
    std::string out = getOutput(policy);
    std::string deviceName = device.getInfo<CL_DEVICE_NAME>();
    CPPUNIT_ASSERT_EQUAL(std::string("Tuning test on ") + deviceName + '\n', out);
}

void TestTunePolicy::testVerbosityNormal()
{
    clogs::TunePolicy policy;
    policy.setVerbosity(clogs::TUNE_VERBOSITY_NORMAL);
    std::string out = getOutput(policy);
    std::string deviceName = device.getInfo<CL_DEVICE_NAME>();
    CPPUNIT_ASSERT_EQUAL(
        std::string("Tuning test on ") + deviceName + '\n'
        + ".!.\n.\n", out);
}
