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
 * Radix-sort interface.
 */

#ifndef CLOGS_RADIXSORT_H
#define CLOGS_RADIXSORT_H

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <cstddef>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>

// Only for use by internal test code.
class TestRadixsort;

namespace clogs
{

/**
 * Radix-sort interface.
 *
 * One instance of this class can be re-used for multiple sorts, provided that
 *  - calls to @ref enqueue do not overlap; and
 *  - their execution does not overlap.
 *
 * An instance of the class is specialized to a specific context, device, and
 * types for the keys and values. The keys can be any unsigned integral scalar
 * type, and the values can be any built-in OpenCL type (including @c void to
 * indicate that there are no values).
 *
 * The implementation is loosely based on the reduce-then-scan strategy
 * described at http://code.google.com/p/back40computing/wiki/RadixSorting,
 * but does not appear to be as efficient.
 */
class CLOGS_API Radixsort
{
    friend class ::TestRadixsort;
private:
    ::size_t reduceWorkGroupSize;    ///< Work group size for the initial reduce phase
    ::size_t scanWorkGroupSize;      ///< Work group size for the middle scan phase
    ::size_t scatterWorkGroupSize;   ///< Work group size for the final scatter phase
    ::size_t scatterWorkScale;       ///< Elements for work item for the final scan/scatter phase
    ::size_t scatterSlice;           ///< Number of work items that cooperate
    ::size_t maxBlocks;              ///< Maximum number of items in the middle phase
    ::size_t keySize;                ///< Size of the key type
    ::size_t valueSize;              ///< Size of the value type
    unsigned int radix;              ///< Sort radix
    unsigned int radixBits;          ///< Number of bits forming radix
    cl::Program program;             ///< Program containing the kernels
    cl::Kernel reduceKernel;         ///< Initial reduction kernel
    cl::Kernel scanKernel;           ///< Middle-phase scan kernel
    cl::Kernel scatterKernel;        ///< Final scan/scatter kernel
    cl::Buffer histogram;            ///< Histogram of the blocks by radix
    cl::Buffer tmpKeys;              ///< User-provided buffer to hold temporary keys
    cl::Buffer tmpValues;            ///< User-provided buffer to hold temporary values

    CLOGS_LOCAL ::size_t getBlocks(::size_t elements, ::size_t len);

    /**
     * Enqueue the reduction kernel.
     * @param queue                Command queue to enqueue to.
     * @param out                  Histogram table, with storage for @a len * @ref radix uints.
     * @param in                   Keys to sort.
     * @param len                  Length of each block to reduce.
     * @param elements             Number of elements to reduce.
     * @param firstBit             Index of first bit forming radix.
     * @param events               Events to wait for (if not @c NULL).
     * @param[out] event           Event for this work (if not @c NULL).
     */
    CLOGS_LOCAL void enqueueReduce(
        const cl::CommandQueue &queue, const cl::Buffer &out, const cl::Buffer &in,
        ::size_t len, ::size_t elements, unsigned int firstBit,
        const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

    /**
     * Enqueue the scan kernel.
     * @param queue                Command queue to enqueue to.
     * @param histogram            Histogram of @ref maxBlocks * @ref radix elements, block-major.
     * @param blocks               Actual number of blocks to scan
     * @param events               Events to wait for (if not @c NULL).
     * @param[out] event           Event for this work (if not @c NULL).
     */
    CLOGS_LOCAL void enqueueScan(
        const cl::CommandQueue &queue, const cl::Buffer &histogram, ::size_t blocks,
        const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

    /**
     * Enqueue the scatter kernel.
     * @param queue                Command queue to enqueue to.
     * @param outKeys              Output buffer for partitioned keys.
     * @param outValues            Output buffer for parititoned values.
     * @param inKeys               Input buffer with unsorted keys.
     * @param inValues             Input buffer with values corresponding to @a inKeys.
     * @param histogram            Scanned histogram of @ref maxBlocks * @ref radix elements, block-major.
     * @param len                  Length of each block to reduce.
     * @param elements             Total number of key/value pairs.
     * @param firstBit             Index of first bit to sort on.
     * @param events               Events to wait for (if not @c NULL).
     * @param[out] event           Event for this work (if not @c NULL).
     *
     * @pre The input and output buffers must all be distinct.
     */
    CLOGS_LOCAL void enqueueScatter(
        const cl::CommandQueue &queue, const cl::Buffer &outKeys, const cl::Buffer &outValues,
        const cl::Buffer &inKeys, const cl::Buffer &inValues, const cl::Buffer &histogram,
        ::size_t len, ::size_t elements, unsigned int firstBit,
        const VECTOR_CLASS<cl::Event> *events, cl::Event *event);

    /* Prevent copying */
    CLOGS_LOCAL Radixsort(const Radixsort &);
    CLOGS_LOCAL Radixsort &operator =(const Radixsort &);
public:
    /**
     * Constructor.
     *
     * @param context              OpenCL context to use
     * @param device               OpenCL device to use.
     * @param keyType              %Type for the keys. Must be an unsigned integral scalar type.
     * @param valueType            %Type for the values. Can be any storable type, including void.
     *
     * @throw std::invalid_argument if @a keyType is not an unsigned integral scalar type.
     * @throw std::invalid_argument if @a valueType is not a storable type for @a device.
     * @throw clogs::InternalError if there was a problem with initialization
     */
    Radixsort(const cl::Context &context, const cl::Device &device, const Type &keyType, const Type &valueType = Type());

    ~Radixsort(); ///< Destructor

    /**
     * Enqueue a scan operation on a command queue.
     *
     * @param commandQueue         The command queue to use.
     * @param keys                 The keys to sort.
     * @param values               The values associated with the keys.
     * @param elements             The number of elements to sort.
     * @param maxBits              Upper bound on the number of bits in any key.
     * @param events               Events to wait for before starting.
     * @param event                Event that will be signaled on completion.
     *
     * @throw cl::Error            If @a keys or @a values is not read-write.
     * @throw cl::Error            If the element range overruns either buffer.
     * @throw cl::Error            If @a elements or @a maxBits is zero.
     * @throw cl::Error            If @a maxBits is greater than the number of bits in the key type.
     * @pre
     * - @a commandQueue was created with the context and device given to the constructor.
     * - @a keys and @a values do not overlap in memory.
     * - All keys are strictly less than 2<sup>@a maxBits</sup>.
     * @post
     * - After execution, the keys will be sorted (with stability), and the values will be
     *   in the same order as the keys.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &keys, const cl::Buffer &values,
                 ::size_t elements, unsigned int maxBits,
                 const VECTOR_CLASS<cl::Event> *events,
                 cl::Event *event);

    /**
     * Enqueue a sort operation on a command queue, without specifying a bound on
     * the number of bits. This overload is provided for simplicity in case no
     * information is available on the range of the keys, but it may be
     * significantly less efficient than the generic version.
     *
     * @see @ref enqueue for details.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &keys, const cl::Buffer &values,
                 ::size_t elements,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL);

    /**
     * Set temporary buffers used during sorting. These buffers are
     * used if they are big enough (as big as the buffers that are
     * being sorted); otherwise temporary buffers are allocated on
     * the fly. Providing suitably large buffers guarantees that
     * no buffer storage is allocated by @ref enqueue.
     *
     * It is legal to set either or both values to <code>cl::Buffer()</code>
     * to clear the temporary buffer, in which case @ref enqueue will revert
     * to allocating its own temporary buffers as needed.
     *
     * This object will retain references to the buffers, so it is
     * safe for the caller to release its reference.
     */
    void setTemporaryBuffers(const cl::Buffer &keys, const cl::Buffer &values);
};

} // namespace clogs

#endif /* !CLOGS_RADIXSORT_H */
