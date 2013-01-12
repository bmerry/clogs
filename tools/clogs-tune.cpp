/* Copyright (c) 2013 University of Cape Town
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

#include <CL/cl.hpp>
#include <boost/program_options.hpp>
#include <vector>
#include "options.h"
#include "../src/tune.h"
#include "../src/parameters.h"

namespace po = boost::program_options;

po::variables_map processOptions(int argc, char **argv)
{
    po::options_description desc("Options");
    desc.add_options()
        ("help",                    "Show help")
        ("force",                   "Re-tune already-tuned configurations");

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
            std::cout << desc << '\n';
            std::exit(0);
        }
        return vm;
    }
    catch (po::error &e)
    {
        std::cerr << e.what() << "\n\n" << desc << '\n';
        std::exit(1);
    }
}

int main(int argc, char **argv)
{
    po::variables_map vm = processOptions(argc, argv);
    bool force = vm.count("force");
    try
    {
        std::vector<cl::Device> devices = findDevices(vm);
        clogs::detail::tuneAll(devices, force);
    }
    catch (clogs::detail::SaveParametersError &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
