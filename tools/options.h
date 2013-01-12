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
 * Miscellaneous utilities for command-line option processing, shared
 * between the test frontend and clogs-benchmark.
 */

#ifndef CLOGS_OPTIONS_H
#define CLOGS_OPTIONS_H

#include <CL/cl.hpp>
#include <vector>
#include <boost/program_options.hpp>

/**
 * Add the options accepted by @ref findDevice.
 */
void addOptions(boost::program_options::options_description &opts);

/**
 * Find all OpenCL devices matching command-line criteria.
 * @param     vm      Command-line options
 * @see @ref findDevice.
 */
std::vector<cl::Device> findDevices(const boost::program_options::variables_map &vm);

/**
 * Find an OpenCL device based on given command-line options.
 * The recognized command-line options are:
 * --cl-cpu: Match CPU devices only
 * --cl-gpu: Match GPU devices only
 * --cl-device: Specify device name
 *
 * @param      vm      Command-line options
 * @param[out] device  Matched device
 * @return True if a matching device was found.
 */
bool findDevice(const boost::program_options::variables_map &vm, cl::Device &device);

/**
 * Create a context suitable for use with a given device.
 *
 * @param device       The device to use (not null).
 * @return A context that can be used with @a device.
 */
cl::Context makeContext(const cl::Device &device);

#endif /* !CLOGS_OPTIONS_H */
