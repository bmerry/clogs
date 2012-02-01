/**
 * @file
 *
 * OpenCL primitives.
 */

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif
#include <clogs/clogs.h>
#include <string>
#include <algorithm>
#include <map>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cassert>
#include <climits>
#include <string>

namespace clogs
{

InternalError::InternalError(const std::string &msg) : std::runtime_error(msg)
{
}

namespace detail
{
// Implementation in generated code
CLOGS_LOCAL const std::map<std::string, std::string> &getSourceMap();
}

bool deviceHasExtension(const cl::Device &device, const std::string &extension)
{
    std::string extensions = device.getInfo<CL_DEVICE_EXTENSIONS>();
    std::string::size_type pos = extensions.find(extension);
    bool found = false;
    while (pos != std::string::npos)
    {
        if ((pos == 0 || extensions[pos - 1] == ' ')
            && (pos + extension.size() == extensions.size() || extensions[pos + extension.size()] == ' '))
        {
            found = true;
            break;
        }
        pos = extensions.find(extension, pos + 1);
    }
    return found;
}

Type::Type() : baseType(TYPE_VOID), length(0) {}

Type::Type(BaseType baseType, unsigned int length) : baseType(baseType), length(length)
{
    if (baseType == TYPE_VOID)
        throw std::invalid_argument("clogs::Type cannot be explicitly constructed with void type");
    switch (length)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 8:
    case 16:
        break;
    default:
        throw std::invalid_argument("length is not a valid value");
    }
}

bool Type::isIntegral() const
{
    switch (baseType)
    {
    case TYPE_UCHAR:
    case TYPE_CHAR:
    case TYPE_USHORT:
    case TYPE_SHORT:
    case TYPE_UINT:
    case TYPE_INT:
    case TYPE_ULONG:
    case TYPE_LONG:
        return true;
    case TYPE_VOID:
    case TYPE_HALF:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
        return false;
    }
    // Should never be reached
    return false;
}

bool Type::isSigned() const
{
    switch (baseType)
    {
    case TYPE_CHAR:
    case TYPE_SHORT:
    case TYPE_INT:
    case TYPE_LONG:
    case TYPE_HALF:
    case TYPE_FLOAT:
    case TYPE_DOUBLE:
        return true;
    case TYPE_UCHAR:
    case TYPE_USHORT:
    case TYPE_UINT:
    case TYPE_ULONG:
    case TYPE_VOID:
        return false;
    }
    // Should never be reached
    return false;
}

bool Type::isStorable(const cl::Device &device) const
{
    switch (baseType)
    {
    case TYPE_VOID:
        return false;
    case TYPE_UCHAR:
    case TYPE_CHAR:
        return (length >= 3 || deviceHasExtension(device, "cl_khr_byte_addressable_store"));
    case TYPE_USHORT:
    case TYPE_SHORT:
        return (length >= 2 || deviceHasExtension(device, "cl_khr_byte_addressable_store"));
    case TYPE_HALF:
        // half is always a valid storage type, but since it cannot be loaded or stored
        // without using built-in functions that is fairly meaningless.
        return deviceHasExtension(device, "cl_khr_fp16");
    case TYPE_DOUBLE:
        return deviceHasExtension(device, "cl_khr_fp64");
    case TYPE_UINT:
    case TYPE_INT:
    case TYPE_ULONG:
    case TYPE_LONG:
    case TYPE_FLOAT:
        return true;
    }
    // Should never be reached
    return false;
}

bool Type::isComputable(const cl::Device &device) const
{
    switch (baseType)
    {
    case TYPE_VOID:
        return false;
    case TYPE_HALF:
        return deviceHasExtension(device, "cl_khr_fp16");
    case TYPE_DOUBLE:
        return deviceHasExtension(device, "cl_khr_fp64");
    case TYPE_UCHAR:
    case TYPE_CHAR:
    case TYPE_USHORT:
    case TYPE_SHORT:
    case TYPE_UINT:
    case TYPE_INT:
    case TYPE_ULONG:
    case TYPE_LONG:
    case TYPE_FLOAT:
        return true;
    }
    // Should never be reached
    return false;
}

::size_t Type::getBaseSize() const
{
    switch (baseType)
    {
    case TYPE_VOID:    return 0;
    case TYPE_UCHAR:   return sizeof(cl_uchar);
    case TYPE_CHAR:    return sizeof(cl_char);
    case TYPE_USHORT:  return sizeof(cl_ushort);
    case TYPE_SHORT:   return sizeof(cl_short);
    case TYPE_UINT:    return sizeof(cl_uint);
    case TYPE_INT:     return sizeof(cl_int);
    case TYPE_ULONG:   return sizeof(cl_ulong);
    case TYPE_LONG:    return sizeof(cl_long);
    case TYPE_HALF:    return sizeof(cl_half);
    case TYPE_FLOAT:   return sizeof(cl_float);
    case TYPE_DOUBLE:  return sizeof(cl_double);
    }
    // Should never be reached
    return 0;
}

::size_t Type::getSize() const
{
    return getBaseSize() * (length == 3 ? 4 : length);
}

std::string Type::getName() const
{
    const char *baseName = NULL;
    switch (baseType)
    {
    case TYPE_VOID:   baseName = "void";   break;
    case TYPE_UCHAR:  baseName = "uchar";  break;
    case TYPE_CHAR:   baseName = "char";   break;
    case TYPE_USHORT: baseName = "ushort"; break;
    case TYPE_SHORT:  baseName = "short";  break;
    case TYPE_UINT:   baseName = "uint";   break;
    case TYPE_INT:    baseName = "int";    break;
    case TYPE_ULONG:  baseName = "ulong";  break;
    case TYPE_LONG:   baseName = "long";   break;
    case TYPE_HALF:   baseName = "half";   break;
    case TYPE_FLOAT:  baseName = "float";  break;
    case TYPE_DOUBLE: baseName = "double"; break;
    }
    if (length <= 1)
        return baseName;
    else
    {
        std::ostringstream s;
        s << baseName << length;
        return s.str();
    }
}

BaseType Type::getBaseType() const
{
    return baseType;
}

unsigned int Type::getLength() const
{
    return length;
}

namespace
{

static unsigned int getWarpSize(const cl::Device &device)
{
    if (deviceHasExtension(device, "cl_nv_device_attribute_query"))
        return device.getInfo<CL_DEVICE_WARP_SIZE_NV>();
    else
        return 1U;
}

template<typename T>
static T roundDownPower2(T x)
{
    T y = 1;
    while (y * 2 <= x)
        y <<= 1;
    return y;
}

template<typename T>
static inline T roundDown(T x, T y)
{
    return x / y * y;
}

template<typename T>
static inline T roundUp(T x, T y)
{
    return (x + y - 1) / y * y;
}

static cl::Program build(const cl::Context &context, const std::vector<cl::Device> &devices, const std::string &filename, const std::map<std::string, int> &defines, const std::string &options = "")
{
    const std::map<std::string, std::string> &sourceMap = detail::getSourceMap();
    if (!sourceMap.count(filename))
        throw std::invalid_argument("No such program " + filename);
    const std::string &source = sourceMap.find(filename)->second;

    std::ostringstream s;
    for (std::map<std::string, int>::const_iterator i = defines.begin(); i != defines.end(); i++)
    {
        s << "#define " << i->first << " " << i->second << "\n";
    }
    s << "#line 1 \"" << filename << "\"\n";
    const std::string header = s.str();
    cl::Program::Sources sources(2);
    sources[0] = std::make_pair(header.data(), header.length());
    sources[1] = std::make_pair(source.data(), source.length());
    cl::Program program(context, sources);

    bool failed = false;
    try
    {
        std::string allOptions = options;
        for (std::map<std::string, int>::const_iterator i = defines.begin(); i != defines.end(); i++)
        {
            std::ostringstream d;
            d << " -D" << i->first << "=" << i->second;
            allOptions += d.str();
        }
        /* TODO
        if (state.forceDebug)
            allOptions += " -g -cl-opt-disable";
        if (state.forceVerbose)
            allOptions += " -cl-nv-verbose";
        */
        int status = program.build(devices, allOptions.c_str());
        if (status != 0)
            failed = true;
    }
    catch (cl::Error &e)
    {
        failed = true;
    }

    if (failed)
    {
        std::ostringstream msg;
        msg << "Internal error compiling " << filename << '\n';
        for (std::vector<cl::Device>::const_iterator device = devices.begin(); device != devices.end(); ++device)
        {
            const std::string log = program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(*device);
            if (log != "" && log != "\n")
            {
                msg << "Log for device " << device->getInfo<CL_DEVICE_NAME>() << '\n';
                msg << log << '\n';
            }
        }
        throw InternalError(msg.str());
    }

    return program;
}

} // namespace

Scan::Scan(const cl::Context &context, const cl::Device &device, const Type &type)
{
    if (!type.isIntegral() || !type.isComputable(device) || !type.isStorable(device))
        throw std::invalid_argument("type is not a supported integral format on this device");

    const ::size_t elementSize = type.getSize();
    const ::size_t maxWorkGroupSize = device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    const ::size_t localMemElements = device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / elementSize;

    this->elementSize = elementSize;
    ::size_t workGroupSize = 256U;
    scanWorkScale = 8U;
    maxBlocks = 1024U;
    ::size_t warpSize = getWarpSize(device);
    if (device.getInfo<CL_DEVICE_TYPE>() & CL_DEVICE_TYPE_CPU)
    {
        scanWorkScale = 1U;
        workGroupSize = 1U;
        maxBlocks = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
        if (maxBlocks < 2U)
            maxBlocks = 2U;
    }

    workGroupSize = std::min(workGroupSize, maxWorkGroupSize);
    workGroupSize = std::min(workGroupSize, localMemElements / 2 - 1);
    workGroupSize = roundDownPower2(workGroupSize);
    reduceWorkGroupSize = workGroupSize;
    scanWorkGroupSize = workGroupSize;

    scanWorkScale = std::min(scanWorkScale, localMemElements / workGroupSize);
    scanWorkScale = roundDownPower2(scanWorkScale);

    maxBlocks = std::min(maxBlocks, 2 * maxWorkGroupSize);
    maxBlocks = std::min(maxBlocks, localMemElements);
    maxBlocks = roundDownPower2(maxBlocks);

    std::map<std::string, int> defines;
    defines["WARP_SIZE"] = warpSize;
    defines["REDUCE_WORK_GROUP_SIZE"] = reduceWorkGroupSize;
    defines["SCAN_WORK_GROUP_SIZE"] = scanWorkGroupSize;
    defines["SCAN_WORK_SCALE"] = scanWorkScale;
    defines["SCAN_BLOCKS"] = maxBlocks;

    try
    {
        sums = cl::Buffer(context, CL_MEM_READ_WRITE, maxBlocks * elementSize);
        std::vector<cl::Device> devices(1, device);
        program = build(context, devices, "scan.cl", defines, std::string(" -DSCAN_T=") + type.getName());

        reduceKernel = cl::Kernel(program, "reduce");
        reduceKernel.setArg(0, sums);

        scanSmallKernel = cl::Kernel(program, "scanExclusiveSmall");
        scanSmallKernel.setArg(0, sums);

        scanSmallKernelOffset = cl::Kernel(program, "scanExclusiveSmallOffset");
        scanSmallKernelOffset.setArg(0, sums);

        scanKernel = cl::Kernel(program, "scanExclusive");
        scanKernel.setArg(1, sums);
    }
    catch (cl::Error &e)
    {
        throw InternalError(std::string("Error preparing kernels for scan: ") + e.what());
    }
}

void Scan::enqueueInternal(const cl::CommandQueue &commandQueue,
                           const cl::Buffer &buffer,
                           ::size_t elements,
                           const void *offsetHost,
                           const cl::Buffer *offsetBuffer,
                           cl_uint offsetIndex,
                           const VECTOR_CLASS<cl::Event> *events,
                           cl::Event *event)
{
    /* Validate parameters */
    if (buffer.getInfo<CL_MEM_SIZE>() < elements * elementSize)
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Scan::enqueue: range out of buffer bounds");
    }
    if (!(buffer.getInfo<CL_MEM_FLAGS>() & CL_MEM_READ_WRITE))
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Scan::enqueue: buffer is not read-write");
    }
    if (offsetBuffer != NULL)
    {
        if (offsetBuffer->getInfo<CL_MEM_SIZE>() < (offsetIndex + 1) * elementSize)
        {
            throw cl::Error(CL_INVALID_VALUE, "clogs::Scan::enqueue: offsetIndex out of buffer bounds");
        }
        if (!(offsetBuffer->getInfo<CL_MEM_FLAGS>() & (CL_MEM_READ_ONLY | CL_MEM_READ_WRITE)))
        {
            throw cl::Error(CL_INVALID_VALUE, "clogs::Scan::enqueue: offsetBuffer is not readable");
        }
    }

    if (elements == 0)
        throw cl::Error(CL_INVALID_GLOBAL_WORK_SIZE, "clogs::Scan::enqueue: elements is zero");

    // block size must be a multiple of this
    const ::size_t tileSize = std::max(reduceWorkGroupSize, scanWorkScale * scanWorkGroupSize);

    /* Ensure that blockSize * blocks >= elements while blockSize is a multiply of tileSize */
    const ::size_t blockSize = roundUp(elements, tileSize * maxBlocks) / maxBlocks;
    const ::size_t allBlocks = (elements + blockSize - 1) / blockSize;
    assert(allBlocks > 0 && allBlocks <= maxBlocks);
    assert((allBlocks - 1) * blockSize <= elements);
    assert(allBlocks * blockSize >= elements);

    reduceKernel.setArg(1, buffer);
    reduceKernel.setArg(2, (cl_uint) blockSize);

    scanKernel.setArg(0, buffer);
    scanKernel.setArg(2, (cl_uint) blockSize);
    scanKernel.setArg(3, (cl_uint) elements);

    const cl::Kernel &smallKernel = offsetBuffer ? scanSmallKernelOffset : scanSmallKernel;
    if (offsetBuffer != NULL)
    {
        scanSmallKernelOffset.setArg(1, *offsetBuffer);
        scanSmallKernelOffset.setArg(2, offsetIndex);
    }
    else if (offsetHost != NULL)
    {
        // setArg is missing a const qualifier, hence the cast
        scanSmallKernel.setArg(1, elementSize, const_cast<void *>(offsetHost));
    }
    else
    {
        std::vector<cl_uchar> zero(elementSize);
        scanSmallKernel.setArg(1, elementSize, &zero[0]);
    }

    std::vector<cl::Event> reduceEvents(1);
    std::vector<cl::Event> scanSmallEvents(1);
    const std::vector<cl::Event> *waitFor = events;
    if (allBlocks > 1)
    {
        commandQueue.enqueueNDRangeKernel(reduceKernel,
                                          cl::NullRange,
                                          cl::NDRange(reduceWorkGroupSize * (allBlocks - 1)),
                                          cl::NDRange(reduceWorkGroupSize),
                                          events, &reduceEvents[0]);
        waitFor = &reduceEvents;
    }
    commandQueue.enqueueNDRangeKernel(smallKernel,
                                      cl::NullRange,
                                      cl::NDRange(maxBlocks / 2),
                                      cl::NDRange(maxBlocks / 2),
                                      waitFor, &scanSmallEvents[0]);
    commandQueue.enqueueNDRangeKernel(scanKernel,
                                      cl::NullRange,
                                      cl::NDRange(scanWorkGroupSize * allBlocks),
                                      cl::NDRange(scanWorkGroupSize),
                                      &scanSmallEvents, event);
}

void Scan::enqueue(const cl::CommandQueue &commandQueue,
                   const cl::Buffer &buffer,
                   ::size_t elements,
                   const VECTOR_CLASS<cl::Event> *events,
                   cl::Event *event)
{
    enqueueInternal(commandQueue, buffer, elements, NULL, NULL, 0, events, event);
}

void Scan::enqueue(const cl::CommandQueue &commandQueue,
                   const cl::Buffer &buffer,
                   ::size_t elements,
                   const void *offset,
                   const VECTOR_CLASS<cl::Event> *events,
                   cl::Event *event)
{
    enqueueInternal(commandQueue, buffer, elements, offset, NULL, 0, events, event);
}

void Scan::enqueue(const cl::CommandQueue &commandQueue,
                   const cl::Buffer &buffer,
                   ::size_t elements,
                   const cl::Buffer &offsetBuffer,
                   cl_uint offsetIndex,
                   const VECTOR_CLASS<cl::Event> *events,
                   cl::Event *event)
{
    enqueueInternal(commandQueue, buffer, elements, NULL, &offsetBuffer, offsetIndex, events, event);
}

Scan::~Scan()
{
}

::size_t Radixsort::getBlocks(::size_t elements, ::size_t len)
{
    const ::size_t slicesPerWorkGroup = scatterWorkGroupSize / scatterSlice;
    ::size_t blocks = (elements + len - 1) / len;
    blocks = roundUp(blocks, slicesPerWorkGroup);
    assert(blocks <= maxBlocks);
    return blocks;
}

void Radixsort::enqueueReduce(
    const cl::CommandQueue &queue, const cl::Buffer &out, const cl::Buffer &in,
    ::size_t len, ::size_t elements, unsigned int firstBit,
    const VECTOR_CLASS<cl::Event> *events, cl::Event *event)
{
    reduceKernel.setArg(0, out);
    reduceKernel.setArg(1, in);
    reduceKernel.setArg(2, (cl_uint) len);
    reduceKernel.setArg(3, (cl_uint) elements);
    reduceKernel.setArg(4, (cl_uint) firstBit);
    cl_uint blocks = getBlocks(elements, len);
    queue.enqueueNDRangeKernel(reduceKernel,
                               cl::NullRange,
                               cl::NDRange(reduceWorkGroupSize * blocks),
                               cl::NDRange(reduceWorkGroupSize),
                               events, event);
}

void Radixsort::enqueueScan(
    const cl::CommandQueue &queue, const cl::Buffer &histogram, ::size_t blocks,
    const VECTOR_CLASS<cl::Event> *events, cl::Event *event)
{
    scanKernel.setArg(0, histogram);
    scanKernel.setArg(1, (cl_uint) blocks);
    queue.enqueueNDRangeKernel(scanKernel,
                               cl::NullRange,
                               cl::NDRange(scanWorkGroupSize),
                               cl::NDRange(scanWorkGroupSize),
                               events, event);
}

void Radixsort::enqueueScatter(
    const cl::CommandQueue &queue, const cl::Buffer &outKeys, const cl::Buffer &outValues,
    const cl::Buffer &inKeys, const cl::Buffer &inValues, const cl::Buffer &histogram,
    ::size_t len, ::size_t elements, unsigned int firstBit,
    const VECTOR_CLASS<cl::Event> *events, cl::Event *event)
{
    scatterKernel.setArg(0, outKeys);
    scatterKernel.setArg(1, inKeys);
    scatterKernel.setArg(2, histogram);
    scatterKernel.setArg(3, (cl_uint) len);
    scatterKernel.setArg(4, (cl_uint) elements);
    scatterKernel.setArg(5, (cl_uint) firstBit);
    if (valueSize != 0)
    {
        scatterKernel.setArg(6, outValues);
        scatterKernel.setArg(7, inValues);
    }
    const ::size_t blocks = getBlocks(elements, len);
    const ::size_t slicesPerWorkGroup = scatterWorkGroupSize / scatterSlice;
    assert(blocks % slicesPerWorkGroup == 0);
    const ::size_t workGroups = blocks / slicesPerWorkGroup;
    queue.enqueueNDRangeKernel(scatterKernel,
                               cl::NullRange,
                               cl::NDRange(scatterWorkGroupSize * workGroups),
                               cl::NDRange(scatterWorkGroupSize),
                               events, event);
}

void Radixsort::enqueue(
    const cl::CommandQueue &queue,
    const cl::Buffer &keys, const cl::Buffer &values,
    ::size_t elements, unsigned int maxBits,
    const VECTOR_CLASS<cl::Event> *events,
    cl::Event *event)
{
    /* Validate parameters */
    if (keys.getInfo<CL_MEM_SIZE>() < elements * keySize)
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: range of out of buffer bounds for key");
    }
    if (valueSize != 0 && values.getInfo<CL_MEM_SIZE>() < elements * valueSize)
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: range of out of buffer bounds for value");
    }
    if (!(keys.getInfo<CL_MEM_FLAGS>() & CL_MEM_READ_WRITE))
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: keys is not read-write");
    }
    if (valueSize != 0 && !(values.getInfo<CL_MEM_FLAGS>() & CL_MEM_READ_WRITE))
    {
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: values is not read-write");
    }

    if (elements == 0)
        throw cl::Error(CL_INVALID_GLOBAL_WORK_SIZE, "clogs::Radixsort::enqueue: elements is zero");
    if (maxBits == 0)
        throw cl::Error(CL_INVALID_GLOBAL_WORK_SIZE, "clogs::Radixsort::enqueue: maxBits is zero");
    if (maxBits > CHAR_BIT * keySize)
        throw cl::Error(CL_INVALID_VALUE, "clogs::Radixsort::enqueue: maxBits is too large");

    const cl::Context &context = queue.getInfo<CL_QUEUE_CONTEXT>();

    // If necessary, allocate temporary buffers for ping-pong
    cl::Buffer tmpKeys, tmpValues;
    if (this->tmpKeys() && this->tmpKeys.getInfo<CL_MEM_SIZE>() >= elements * keySize)
        tmpKeys = this->tmpKeys;
    else
        tmpKeys = cl::Buffer(context, CL_MEM_READ_WRITE, elements * keySize);
    if (valueSize != 0)
    {
        if (this->tmpValues() && this->tmpValues.getInfo<CL_MEM_SIZE>() >= elements * valueSize)
            tmpValues = this->tmpValues;
        else
            tmpValues = cl::Buffer(context, CL_MEM_READ_WRITE, elements * valueSize);
    }

    cl::Event next;
    std::vector<cl::Event> prev(1);
    const std::vector<cl::Event> *waitFor = events;
    const cl::Buffer *curKeys = &keys;
    const cl::Buffer *curValues = &values;
    const cl::Buffer *nextKeys = &tmpKeys;
    const cl::Buffer *nextValues = &tmpValues;

    // block size must be a multiple of this
    const ::size_t tileSize = std::max(reduceWorkGroupSize, scatterWorkScale * scatterWorkGroupSize);
    const ::size_t blockSize = (elements + tileSize * maxBlocks - 1) / (tileSize * maxBlocks) * tileSize;
    const ::size_t blocks = getBlocks(elements, blockSize);
    assert(blocks <= maxBlocks);

    for (unsigned int firstBit = 0; firstBit < maxBits; firstBit += radixBits)
    {
        enqueueReduce(queue, histogram, *curKeys, blockSize, elements, firstBit, waitFor, &next);
        prev[0] = next; waitFor = &prev;
        enqueueScan(queue, histogram, blocks, waitFor, &next);
        prev[0] = next; waitFor = &prev;
        enqueueScatter(queue, *nextKeys, *nextValues, *curKeys, *curValues, histogram, blockSize,
                       elements, firstBit, waitFor, &next);
        prev[0] = next; waitFor = &prev;
        std::swap(curKeys, nextKeys);
        std::swap(curValues, nextValues);
    }
    if (curKeys != &keys)
    {
        /* Odd number of ping-pongs, so we have to copy back again.
         * We don't actually need to serialize the copies, but it simplifies the event
         * management.
         */
        queue.enqueueCopyBuffer(*curKeys, *nextKeys, 0, 0, elements * keySize, waitFor, &next);
        prev[0] = next; waitFor = &prev;
        if (valueSize != 0)
        {
            queue.enqueueCopyBuffer(*curValues, *nextValues, 0, 0, elements * valueSize, waitFor, &next);
            prev[0] = next; waitFor = &prev;
        }
    }
    if (event != NULL)
        *event = next;
}

void Radixsort::enqueue(
    const cl::CommandQueue &queue,
    const cl::Buffer &keys, const cl::Buffer &values,
    ::size_t elements,
    const VECTOR_CLASS<cl::Event> *events,
    cl::Event *event)
{
    enqueue(queue, keys, values, elements, keySize * 8, events, event);
}

void Radixsort::setTemporaryBuffers(const cl::Buffer &keys, const cl::Buffer &values)
{
    tmpKeys = keys;
    tmpValues = values;
}

Radixsort::Radixsort(
    const cl::Context &context, const cl::Device &device,
    const Type &keyType, const Type &valueType)
{
    if (!keyType.isIntegral() || keyType.isSigned() || keyType.getLength() != 1
        || !keyType.isComputable(device) || !keyType.isStorable(device))
        throw std::invalid_argument("keyType is not valid");
    if (valueType.getBaseType() != TYPE_VOID
        && !valueType.isStorable(device))
        throw std::invalid_argument("valueType is not valid");

    const ::size_t keySize = keyType.getSize();
    const ::size_t maxWorkGroupSize = device.getInfo<CL_DEVICE_MAX_WORK_GROUP_SIZE>();
    const ::size_t units = device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>();
    const ::size_t warpSize = getWarpSize(device);

    this->keySize = keySize;
    this->valueSize = valueType.getSize();
    radixBits = 4;
    radix = 1 << radixBits;
    if (maxWorkGroupSize < radix)
    {
        throw InternalError("Device capabilities are too limited for radixsort");
    }

    scatterWorkScale = 7;
    if (device.getInfo<CL_DEVICE_TYPE>() & CL_DEVICE_TYPE_CPU)
    {
        maxBlocks = units * 4;
        reduceWorkGroupSize = 1;
        scanWorkGroupSize = 1;
        scatterWorkGroupSize = 1;
    }
    else
    {
        maxBlocks = units * 128;
        reduceWorkGroupSize = 128;
        scanWorkGroupSize = 128;
        scatterWorkGroupSize = 64;
    }

    reduceWorkGroupSize = std::min(reduceWorkGroupSize, maxWorkGroupSize);
    reduceWorkGroupSize = std::max(reduceWorkGroupSize, ::size_t(radix));
    reduceWorkGroupSize = roundDownPower2(reduceWorkGroupSize);

    scanWorkGroupSize = std::min(scanWorkGroupSize, maxWorkGroupSize);
    scanWorkGroupSize = std::max(scanWorkGroupSize, ::size_t(radix));
    scanWorkGroupSize = roundDownPower2(scanWorkGroupSize);

    scatterSlice = std::max(warpSize, ::size_t(radix));
    scatterWorkGroupSize = std::max(scatterWorkGroupSize, scatterSlice);
    scatterWorkGroupSize = roundDown(scatterWorkGroupSize, scatterSlice);
    // TODO: adjust based on local memory availability. That might need
    // autotuning though.
    if (scatterWorkGroupSize > maxWorkGroupSize)
        throw InternalError("Device capabilities are too limited for radixsort");

    if (radix < scanWorkGroupSize)
        maxBlocks = roundUp(maxBlocks, scanWorkGroupSize / radix);
    // maximum that will fit in local memory
    maxBlocks = std::min(maxBlocks, ::size_t(device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() / radix - 1) / 4);
    // must have an exact multiple of the workitem count in scan phase
    if (radix < scanWorkGroupSize)
        maxBlocks = roundDown(maxBlocks, scanWorkGroupSize / radix);
    if (maxBlocks == 0)
        throw InternalError("Device capabilities are too limited for radixsort");

    std::string options;
    options += "-DKEY_T=" + keyType.getName() + " ";
    if (valueType.getBaseType() != TYPE_VOID)
        options += "-DVALUE_T=" + valueType.getName() + " ";

    std::map<std::string, int> defines;
    defines["WARP_SIZE"] = warpSize;
    defines["REDUCE_WORK_GROUP_SIZE"] = reduceWorkGroupSize;
    defines["SCAN_WORK_GROUP_SIZE"] = scanWorkGroupSize;
    defines["SCATTER_WORK_GROUP_SIZE"] = scatterWorkGroupSize;
    defines["SCATTER_WORK_SCALE"] = scatterWorkScale;
    defines["SCATTER_SLICE"] = scatterSlice;
    defines["SCAN_BLOCKS"] = maxBlocks;
    defines["RADIX_BITS"] = radixBits;

    try
    {
        histogram = cl::Buffer(context, CL_MEM_READ_WRITE, maxBlocks * radix * sizeof(cl_uint));
        std::vector<cl::Device> devices(1, device);
        program = build(context, devices, "radixsort.cl", defines, options);

        reduceKernel = cl::Kernel(program, "radixsortReduce");

        scanKernel = cl::Kernel(program, "radixsortScan");
        scanKernel.setArg(0, histogram);

        scatterKernel = cl::Kernel(program, "radixsortScatter");
        scatterKernel.setArg(1, histogram);
    }
    catch (cl::Error &e)
    {
        throw InternalError(std::string("Error preparing kernels for radixsort: ") + e.what());
    }

}

Radixsort::~Radixsort()
{
}

} // namespace clogs
