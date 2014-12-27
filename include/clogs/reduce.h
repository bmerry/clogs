/* Copyright (c) 2014, Bruce Merry
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
 * Reduce primitive.
 */

#ifndef CLOGS_REDUCE_H
#define CLOGS_REDUCE_H

#include <clogs/visibility_push.h>
#include <CL/cl.hpp>
#include <cstddef>
#include <clogs/visibility_pop.h>

#include <clogs/core.h>

namespace clogs
{

namespace detail
{
    class Reduce;
    class ReduceProblem;
} // namespace detail

class Reduce;

/**
 * Encapsulates the specifics of a reduction problem. After construction, use
 * methods (particularly @ref setType) to configure the reduction.
 */
class CLOGS_API ReduceProblem
{
private:
    friend class Reduce;
    detail::ReduceProblem *detail_;

public:
    ReduceProblem();
    ~ReduceProblem();
    ReduceProblem(const ReduceProblem &);
    ReduceProblem &operator=(const ReduceProblem &);

    /**
     * Set the element type for the reduction.
     *
     * @param type      The element type
     */
    void setType(const Type &type);
};

/**
 * Reduction primitive.
 *
 *
 * One instance of this class can be reused for multiple scans, provided that
 *  - calls to @ref enqueue do not overlap; and
 *  - their execution does not overlap.
 *
 * An instance of the class is specialized to a specific context, device, and
 * type of value to scan. Any CL integral scalar or vector type can
 * be used.
 *
 * The implementation divides the data into a number of blocks, each of which
 * is reduced by a work-group. The last work-group handles the final reduction.
 */
class CLOGS_API Reduce
{
private:
    detail::Reduce *detail_;

    /* Prevent copying */
    Reduce(const Reduce &);
    Reduce &operator=(const Reduce &);

private:
    void construct(
        cl_context context, cl_device_id device, const ReduceProblem &problem,
        cl_int &err, const char *&errStr);

protected:
    void enqueue(cl_command_queue commandQueue,
                 cl_mem inBuffer,
                 cl_mem outBuffer,
                 ::size_t first,
                 ::size_t elements,
                 ::size_t outPosition,
                 cl_uint numEvents,
                 const cl_event *events,
                 cl_event *event,
                 cl_int &err,
                 const char *&errStr);

    void enqueue(cl_command_queue commandQueue,
                 bool blocking,
                 cl_mem inBuffer,
                 void *out,
                 ::size_t first,
                 ::size_t elements,
                 cl_uint numEvents,
                 const cl_event *events,
                 cl_event *event,
                 cl_int &err,
                 const char *&errStr);

public:
    /**
     * Constructor.
     *
     * @param context              OpenCL context to use
     * @param device               OpenCL device to use.
     * @param problem              Description of the specific reduction problem.
     *
     * @throw std::invalid_argument if @a problem is not supported on the device or is not initialized.
     * @throw clogs::InternalError if there was a problem with initialization.
     */
    Reduce(const cl::Context &context, const cl::Device &device, const ReduceProblem &problem)
    {
        int err;
        const char *errStr;
        construct(context(), device(), problem, err, errStr);
        detail::handleError(err, errStr);
    }

    /**
     * Constructor. This class will add new references to the @a context and @a device.
     *
     * @param context              OpenCL context to use
     * @param device               OpenCL device to use.
     * @param problem              Description of the specific reduction problem.
     *
     * @throw std::invalid_argument if @a problem is not supported on the device or is not initialized.
     * @throw clogs::InternalError if there was a problem with initialization.
     */
    Reduce(cl_context context, cl_device_id device, const ReduceProblem &problem)
    {
        int err;
        const char *errStr;
        construct(context, device, problem, err, errStr);
        detail::handleError(err, errStr);
    }

    ~Reduce(); ///< Destructor

    /**
     * Set a callback function that will receive a list of all underlying events.
     * The callback may be called multiple times during each enqueue, if
     * the implementation uses multiple commands. This allows profiling information
     * to be extracted from the events once they complete.
     *
     * The callback may also be set to @c NULL to disable it.
     *
     * @note This is not an event completion callback: it is called during
     * @c enqueue, generally before the events complete.
     *
     * @param callback The callback function.
     * @param userData Arbitrary data to be passed to the callback.
     */
    void setEventCallback(void (CL_CALLBACK *callback)(const cl::Event &, void *), void *userData);

    /**
     * Enqueue a reduction operation on a command queue.
     *
     * @param commandQueue         The command queue to use.
     * @param inBuffer             The buffer to reduce.
     * @param outBuffer            The buffer to which the result is written.
     * @param first                The index (in elements, not bytes) to begin the reduction.
     * @param elements             The number of elements to reduce.
     * @param outPosition          The index (in elements, not bytes) at which to write the result.
     * @param events               Events to wait for before starting.
     * @param event                Event that will be signaled on completion.
     *
     * @throw cl::Error            If @a inBuffer is not readable on the device
     * @throw cl::Error            If @a outBuffer is not writable on the devic
     * @throw cl::Error            If the element range overruns the buffer.
     * @throw cl::Error            If @a elements is zero.
     *
     * @pre
     * - @a commandQueue was created with the context and device given to the constructor.
     * - The output does not overlap with the input.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 const cl::Buffer &inBuffer,
                 const cl::Buffer &outBuffer,
                 ::size_t first,
                 ::size_t elements,
                 ::size_t outPosition,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL)
    {
        cl_event outEvent;
        int err;
        const char *errStr;
        detail::UnwrapArray<cl::Event> rawEvents(events);
        enqueue(commandQueue(), inBuffer(), outBuffer(), first, elements, outPosition,
                rawEvents.size(), rawEvents.data(),
                event != NULL ? &outEvent : NULL,
                err, errStr);
        detail::handleError(err, errStr);
        if (event != NULL)
            *event = outEvent; // steals the reference
    }

    /**
     * Enqueue a reduction operation on a command queue.
     *
     * @param commandQueue         The command queue to use.
     * @param inBuffer             The buffer to reduce.
     * @param outBuffer            The buffer to which the result is written.
     * @param first                The index (in elements, not bytes) to begin the reduction.
     * @param elements             The number of elements to reduce.
     * @param outPosition          The index (in elements, not bytes) at which to write the result.
     * @param numEvents            Number of events in @a events
     * @param events               Events to wait for before starting.
     * @param event                Event that will be signaled on completion.
     *
     * @throw cl::Error            If @a inBuffer is not readable on the device
     * @throw cl::Error            If @a outBuffer is not writable on the devic
     * @throw cl::Error            If the element range overruns the buffer.
     * @throw cl::Error            If @a elements is zero.
     *
     * @pre
     * - @a commandQueue was created with the context and device given to the constructor.
     * - The output does not overlap with the input.
     */
    void enqueue(cl_command_queue commandQueue,
                 cl_mem inBuffer,
                 cl_mem outBuffer,
                 ::size_t first,
                 ::size_t elements,
                 ::size_t outPosition,
                 cl_uint numEvents = 0,
                 const cl_event *events = NULL,
                 cl_event *event = NULL)
    {
        cl_int err;
        const char *errStr;
        enqueue(commandQueue, inBuffer, outBuffer, first, elements, outPosition,
                numEvents, events, event, err, errStr);
        detail::handleError(err, errStr);
    }

    /**
     * Enqueue a reduction operation and read the result back to the host.
     * This is a convenience wrapper that avoids the need to separately
     * call @c clEnqueueReadBuffer.
     */
    void enqueue(const cl::CommandQueue &commandQueue,
                 bool blocking,
                 const cl::Buffer &inBuffer,
                 void *out,
                 ::size_t first,
                 ::size_t elements,
                 const VECTOR_CLASS<cl::Event> *events = NULL,
                 cl::Event *event = NULL)
    {
        cl_event outEvent;
        cl_int err;
        const char *errStr;
        detail::UnwrapArray<cl::Event> rawEvents(events);
        enqueue(commandQueue(), blocking, inBuffer(), out, first, elements,
                rawEvents.size(), rawEvents.data(),
                event != NULL ? &outEvent : NULL,
                err, errStr);
        detail::handleError(err, errStr);
        if (event != NULL)
            *event = outEvent; // steals the reference
    }

    /**
     * Enqueue a reduction operation and read the result back to the host.
     * This is a convenience wrapper that avoids the need to separately
     * call @c clEnqueueReadBuffer.
     */
    void enqueue(cl_command_queue commandQueue,
                 bool blocking,
                 cl_mem inBuffer,
                 void *out,
                 ::size_t first,
                 ::size_t elements,
                 cl_uint numEvents = 0,
                 const cl_event *events = NULL,
                 cl_event *event = NULL)
    {
        cl_int err;
        const char *errStr;
        enqueue(commandQueue, blocking, inBuffer, out, first, elements,
                numEvents, events, event, err, errStr);
        detail::handleError(err, errStr);
    }
};

} // namespace clogs

#endif /* !CLOGS_REDUCE_H */
