/**
 * @file
 *
 * Miscellaneous utilities for command-line option processing, shared
 * between the test frontend and clogs-benchmark.
 */

#ifndef CLOGS_OPTIONS_H
#define CLOGS_OPTIONS_H

#include <CL/cl.hpp>
#include <boost/program_options.hpp>

/**
 * Add the options accepted by @ref findDevice.
 */
void addOptions(boost::program_options::options_description &opts);

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
