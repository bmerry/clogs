/**
 * @file
 *
 * OpenCL primitives.
 */

#ifndef CLOGS_H
#define CLOGS_H

#include <clogs/visibility.h>

/* We need to using CL/cl.hpp with default visibility, as otherwise
 * cl::Error cannot safely be thrown across the DLL boundary.
 */
#ifdef CLOGS_DLL_DO_PUSH_POP
# pragma GCC visibility push(default)
#endif

/// API major version number.
#define CLOGS_VERSION_MAJOR 1
/// API minor version number.
#define CLOGS_VERSION_MINOR 0

#include <CL/cl.hpp>
#include <string>
#include <ostream>
#include <stdexcept>

#ifdef CLOGS_DLL_DO_PUSH_POP
# pragma GCC visibility pop
#endif

class TestRadixsort;

/**
 * OpenCL primitives.
 *
 * The primary classes of interest are @ref Scan and @ref Radixsort, which
 * provide the algorithms. The other classes are utilities and helpers.
 */
namespace clogs
{

/**
 * Exception thrown on internal errors that are not the user's fault.
 */
class CLOGS_API InternalError : public std::runtime_error
{
public:
    InternalError(const std::string &msg);
};

/**
 * Enumeration of scalar types supported by OpenCL C which can be stored in a buffer.
 */
enum CLOGS_API BaseType
{
    TYPE_VOID,
    TYPE_UCHAR,
    TYPE_CHAR,
    TYPE_USHORT,
    TYPE_SHORT,
    TYPE_UINT,
    TYPE_INT,
    TYPE_ULONG,
    TYPE_LONG,
    TYPE_HALF,
    TYPE_FLOAT,
    TYPE_DOUBLE
};

/**
 * Encapsulation of an OpenCL built-in type that can be stored in a buffer.
 *
 * An instance of this class can represent either a scalar, a vector, or the
 * @c void type.
 */
class CLOGS_API Type
{
private:
    BaseType baseType;    ///< Element type
    unsigned int length;  ///< Vector length (1 for scalars)

public:
    /// Default constructor, creating the void type.
    Type();
    /**
     * Constructor. It is deliberately not declared @c explicit, so that
     * an instance of @ref BaseType can be used where @ref Type is expected.
     *
     * @pre @a baseType is not @c TYPE_VOID.
     */
    Type(BaseType baseType, unsigned int length = 1);

    /**
     * Whether the type can be stored in a buffer and read/written in a CL C
     * program using the assignment operator.
     */
    bool isStorable(const cl::Device &device) const;

    /**
     * Whether the type can be used in expressions.
     */
    bool isComputable(const cl::Device &device) const;
    bool isIntegral() const;       ///< True if the type stores integer values.
    bool isSigned() const;         ///< True if the type is signed.
    std::string getName() const;   ///< Name of the CL C type
    ::size_t getSize() const;      ///< Size in bytes of the C API form of the type (0 for void)
    ::size_t getBaseSize() const;  ///< Size in bytes of the scalar elements (0 for void)

    /// The scalar element type.
    BaseType getBaseType() const;
    /// The vector length (1 for scalars, 0 for void).
    unsigned int getLength() const;
};

/**
 * Returns true if @a device supports @a extension.
 * At present, no caching is done, so this is a potentially slow operation.
 */
CLOGS_API bool deviceHasExtension(const cl::Device &device, const std::string &extension);

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

    /**
     * Implementation of @ref enqueueInternal, supporting both offsetting and
     * non-offseting. If @a offsetBuffer is not @c NULL, we are doing offseting.
     */
    CLOGS_LOCAL void enqueueInternal(
        const cl::CommandQueue &commandQueue,
        const cl::Buffer &buffer,
        ::size_t elements,
        const void *offsetCPU,
        const cl::Buffer *offsetBuffer,
        cl_uint offsetIndex,
        const VECTOR_CLASS<cl::Event> *events,
        cl::Event *event);

    /* Prevent copying */
    CLOGS_LOCAL Scan(const Scan &);
    CLOGS_LOCAL Scan &operator=(const Scan &);

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

/**
 * Radix-sort implementation.
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

} // namespace CLOGS

/**
 * @mainpage
 *
 * Please refer to the user manual for an introduction to CLOGS, or the
 * @ref clogs namespace page for reference documentation of the classes.
 */

#endif /* !CLOGS_H */
