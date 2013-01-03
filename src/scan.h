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
 * Scan implementation.
 */

#ifndef SCAN_H
#define SCAN_H

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <clogs/visibility_push.h>
#include <cstddef>
#include <vector>
#include <CL/cl.hpp>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>
#include "parameters.h"

namespace clogs
{
namespace detail
{

/**
 * Internal implementation of @ref clogs::Scan.
 */
class CLOGS_LOCAL Scan
{
private:
    ::size_t reduceWorkGroupSize;    ///< Work group size for the initial reduce phase
    ::size_t scanWorkGroupSize;      ///< Work group size for the final scan phase
    ::size_t scanWorkScale;          ///< Elements for work item for the final scan phase
    ::size_t maxBlocks;              ///< Maximum number of items in the middle phase
    ::size_t elementSize;            ///< Size of the element type
    cl::Program program;             ///< Program containing the kernels
    cl::Kernel reduceKernel;         ///< Initial reduction kernel
    cl::Kernel scanSmallKernel;      ///< Middle-phase scan kernel
    cl::Kernel scanSmallKernelOffset; ///< Middle-phase scan kernel with offset support
    cl::Kernel scanKernel;           ///< Final scan kernel
    cl::Buffer sums;                 ///< Reductions of the blocks for middle phase

    void (CL_CALLBACK *eventCallback)(const cl::Event &event, void *);
    void *eventCallbackUserData;

    /**
     * Implementation of @ref enqueueInternal, supporting both offsetting and
     * non-offsetting. If @a offsetBuffer is not @c NULL, we are doing offseting.
     */
    void enqueueInternal(
        const cl::CommandQueue &commandQueue,
        const cl::Buffer &buffer,
        ::size_t elements,
        const void *offsetCPU,
        const cl::Buffer *offsetBuffer,
        cl_uint offsetIndex,
        const VECTOR_CLASS<cl::Event> *events,
        cl::Event *event);

    /**
     * Call the event callback, if there is one.
     */
    void doEventCallback(const cl::Event &event);

    /* Prevent copying */
    Scan(const Scan &);
    Scan &operator=(const Scan &);

    /**
     * Second construction phase. This is called either by the normal constructor
     * or during autotuning.
     *
     * @param context, device, type    Constructor arguments
     * @param params                   Autotuned parameters
     */
    void initialize(
        const cl::Context &context, const cl::Device &device, const Type &type,
        const ParameterSet &params);

public:
    /**
     * Constructor.
     * @see @ref clogs::Scan::Scan
     */
    Scan(const cl::Context &context, const cl::Device &device, const Type &type);

    /**
     * Set a callback to be notified of enqueued commands.
     * @see @ref clogs::Scan::setEventCallback
     */
    void setEventCallback(void (CL_CALLBACK *callback)(const cl::Event &, void *), void *userData);

    /**
     * Enqueue a scan operation on a command queue, with a CPU offset.
     * @see @ref clogs::Scan::enqueue.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &buffer,
                 ::size_t elements,
                 const void *offset = NULL,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);

    /**
     * Enqueue a scan operation on a command queue, with an offset in a buffer.
     * @see clogs::Scan::enqueue.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &buffer,
                 ::size_t elements,
                 const cl::Buffer &offsetBuffer,
                 cl_uint offsetIndex,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);

    /**
     * Create the keys for autotuning. The values are undefined.
     */
    static ParameterSet parameters();

    /**
     * Returns key for looking up autotuning parameters.
     *
     * @param device, type  Constructor parameters.
     */
    static ParameterSet makeKey(const cl::Device &device, const Type &type);

    /**
     * Perform autotuning.
     *
     * @param context     Context for executing autotuning tests
     * @param device      Device to tune for
     * @param type        Type to scan
     */
    static ParameterSet tune(const cl::Context &context, const cl::Device &device, const Type &type);

    /**
     * Return whether a type is supported for scanning on a device.
     */
    static bool typeSupported(const cl::Device &device, const Type &type);
};

} // namespace detail
} // namespace clogs

#endif /* SCAN_H */
