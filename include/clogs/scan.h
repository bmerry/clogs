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
 * Scan primitive.
 */

#ifndef CLOGS_SCAN_H
#define CLOGS_SCAN_H

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <cstddef>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>

namespace clogs
{

namespace detail
{
    class Scan;
} // namespace detail

/**
 * Exclusive scan (prefix sum) primitive.
 *
 * One instance of this class can be reused for multiple scans, provided that
 *  - calls to @ref enqueue do not overlap; and
 *  - their execution does not overlap.
 *
 * An instance of the class is specialized to a specific context, device, and
 * type of value to scan. Any CL integral scalar or vector type can
 * be used.
 *
 * The implementation is based on the reduce-then-scan strategy described at
 * https://sites.google.com/site/duanemerrill/ScanTR2.pdf?attredirects=0
 */
class CLOGS_API Scan
{
private:
    detail::Scan *detail_;

    /* Prevent copying */
    Scan(const Scan &);
    Scan &operator=(const Scan &);

public:
    /**
     * Constructor.
     *
     * @param context              OpenCL context to use
     * @param device               OpenCL device to use.
     * @param type                 %Type of the values to scan.
     *
     * @throw std::invalid_argument if @a type is not an integral type supported on the device.
     * @throw clogs::InternalError if there was a problem with initialization.
     */
    Scan(const cl::Context &context, const cl::Device &device, const Type &type);

    ~Scan(); ///< Destructor

    /**
     * Enqueue a scan operation on a command queue.
     *
     * @param commandQueue         The command queue to use.
     * @param buffer               The buffer to scan.
     * @param elements             The number of elements to scan.
     * @param events               Events to wait for before starting.
     * @param event                Event that will be signaled on completion.
     *
     * @throw cl::Error            If @a buffer is not read-write.
     * @throw cl::Error            If the element range overruns the buffer.
     * @throw cl::Error            If @a elements is zero.
     * @pre
     * - @a commandQueue was created with the context and device given to the constructor.
     * @post
     * - After execution, element @c i will be replaced by the sum of all elements strictly
     *   before @c i.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &buffer,
                 ::size_t elements,
                 const VECTOR_CLASS<cl::Event> *events,
                 cl::Event *event);

    /**
     * Enqueue a scan operation on a command queue, with a CPU offset.
     *
     * The offset is passed in a void pointer, which must point to an element of the
     * type passed to the constructor.
     *
     * @param commandQueue         The command queue to use.
     * @param buffer               The buffer to scan.
     * @param elements             The number of elements to scan.
     * @param offset               The offset to add to all elements.
     * @param events               Events to wait for before starting.
     * @param event                Event that will be signaled on completion.
     *
     * @throw cl::Error            If @a buffer is not read-write.
     * @throw cl::Error            If the element range overruns the buffer.
     * @throw cl::Error            If @a elements is zero.
     * @pre
     * - @a commandQueue was created with the context and device given to the constructor.
     * @post
     * - After execution, element @c i will be replaced by the sum of all elements strictly
     *   before @c i, plus the @a offset.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &buffer,
                 ::size_t elements,
                 const void *offset,
                 const VECTOR_CLASS<cl::Event> *events,
                 cl::Event *event);

    /**
     * Enqueue a scan operation on a command queue, with an offset in a buffer.
     *
     * The offset is of the same type as the elements to be scanned, and is
     * stored in a buffer. It is added to all elements of the result. It is legal
     * for the offset to be in the same buffer as the values to scan, and it may
     * even be safely overwritten by the scan (it will be read before being
     * overwritten). This makes it possible to use do multi-pass algorithms with
     * variable output. The counting pass fills in the desired allocations, a
     * scan is used with one extra element at the end to hold the grand total,
     * and the subsequent passes use this extra element as the offset.
     *
     * @param commandQueue         The command queue to use.
     * @param buffer               The buffer to scan.
     * @param elements             The number of elements to scan.
     * @param offsetBuffer         Buffer containing a value to add to all elements.
     * @param offsetIndex          Index (in units of the scan type) into @a offsetBuffer.
     * @param events               Events to wait for before starting.
     * @param event                Event that will be signaled on completion.
     *
     * @throw cl::Error            If @a buffer is not read-write.
     * @throw cl::Error            If the element range overruns the buffer.
     * @throw cl::Error            If @a elements is zero.
     * @throw cl::Error            If @a offsetBuffer is not readable.
     * @throw cl::Error            If @a offsetIndex overruns @a offsetBuffer.
     * @pre
     * - @a commandQueue was created with the context and device given to the constructor.
     * @post
     * - After execution, element @c i will be replaced by the sum of all elements strictly
     *   before @c i, plus the offset.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &buffer,
                 ::size_t elements,
                 const cl::Buffer &offsetBuffer,
                 cl_uint offsetIndex,
                 const VECTOR_CLASS<cl::Event> *events,
                 cl::Event *event);
};

} // namespace clogs

#endif /* !CLOGS_SCAN_H */
