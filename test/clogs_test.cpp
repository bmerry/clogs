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

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <iostream>
#include <cppunit/Test.h>
#include <cppunit/TestCase.h>
#include <cppunit/TextTestRunner.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <string>
#include <stdexcept>
#include <typeinfo>
#include "../src/clhpp11.h"
#include "clogs_test.h"
#include "../tools/options.h"
#include "../src/utils.h"

using namespace std;
namespace po = boost::program_options;

static cl::Device g_device;
static cl::Context g_context;

/*** Client callable parts ***/

namespace clogs
{

namespace Test
{

void TestFixture::setUp()
{
    CppUnit::TestFixture::setUp();

    device = g_device;
    context = g_context;
    queue = cl::CommandQueue(context, device, 0);
}

void TestFixture::tearDown()
{
    context = NULL;
    device = NULL;
    queue = NULL;

    CppUnit::TestFixture::tearDown();
}

void CL_CALLBACK eventCallback(const cl::Event &event, void *eventCount)
{
    CPPUNIT_ASSERT(event() != NULL);
    CPPUNIT_ASSERT(eventCount != NULL);
    (*static_cast<int *>(eventCount))++;
}

void CL_CALLBACK eventCallbackFree(void *eventCount)
{
    CPPUNIT_ASSERT(eventCount != NULL);
    *static_cast<int *>(eventCount) = -1;
}

} // namespace clogs::Test
} // namespace clogs

/*** End client callable parts ***/

static void listTests(CppUnit::Test *root, string path)
{
    if (!path.empty())
        path += '/';
    path += root->getName();

    cout << path << '\n';
    for (int i = 0; i < root->getChildTestCount(); i++)
    {
        CppUnit::Test *sub = root->getChildTestAt(i);
        listTests(sub, path);
    }
}
static po::variables_map processOptions(int argc, const char **argv)
{
    po::options_description desc("Options");
    desc.add_options()
        ("help",                                      "Show help");

    po::options_description test("Test options");
    test.add_options()
        ("test",          po::value<string>(),        "Choose test")
        ("list",                                      "List all tests")
        ("benchmark",                                 "Include benchmark tests");
    desc.add(test);

    po::options_description cl("OpenCL Options");
    addOptions(cl);
    desc.add(cl);

    try
    {
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv)
                  .style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
                  .options(desc)
                  .run(), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            cout << desc << '\n';
            exit(0);
        }
        return vm;
    }
    catch (po::error &e)
    {
        cerr << e.what() << "\n\n" << desc << '\n';
        exit(1);
    }
}

int main(int argc, const char ** argv)
{
    clogs::detail::enableUnitTests();
    try
    {
        po::variables_map vm = processOptions(argc, argv);

        CppUnit::TestSuite *rootSuite = new CppUnit::TestSuite("All tests");
        CppUnit::TestFactoryRegistry::getRegistry().addTestToSuite(rootSuite);
        if (vm.count("benchmark"))
            CppUnit::TestFactoryRegistry::getRegistry("benchmark").addTestToSuite(rootSuite);
        if (vm.count("list"))
        {
            listTests(rootSuite, "");
            return 0;
        }
        string path = "";
        if (vm.count("test"))
            path = vm["test"].as<string>();

        if (!findDevice(vm, g_device))
        {
            cerr << "No suitable OpenCL device found\n";
            return 1;
        }
        else
        {
            cout << "Using device " << g_device.getInfo<CL_DEVICE_NAME>() << "\n";
        }
        g_context = makeContext(g_device);

        CppUnit::BriefTestProgressListener listener;
        CppUnit::TextTestRunner runner;
        runner.addTest(rootSuite);
        runner.setOutputter(new CppUnit::CompilerOutputter(&runner.result(), std::cerr));
        runner.eventManager().addListener(&listener);
        bool success = runner.run(path, false, true, false);
        g_device = NULL;
        g_context = NULL;
        return success ? 0 : 1;
    }
    catch (invalid_argument &e)
    {
        cerr << "\nERROR: " << e.what() << "\n";
        return 2;
    }
}
