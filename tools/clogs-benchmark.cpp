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

#ifndef __CL_ENABLE_EXCEPTIONS
# define __CL_ENABLE_EXCEPTIONS
#endif

#include <clogs/clogs.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <algorithm>
#include <limits>
#include <cassert>
#include <string>
#include <stdexcept>
#include <locale>
#include "timer.h"
#include "options.h"
#include "../src/utils.h"
#include "../src/tr1_random.h"

namespace po = boost::program_options;

static cl::Device g_device;
static cl::Context g_context;

/* Quick-and-dirty approach to match up a name to a type. It
 * iterates through all valid types and check if they have
 * the desired name.
 */
static clogs::Type matchType(const std::string &typeName)
{
    std::vector<clogs::Type> types = clogs::Type::allTypes();
    for (std::size_t i = 0; i < types.size(); i++)
        if (types[i].getName() == typeName)
            return types[i];
    std::cerr << "Type '" << typeName << "' is not recognized.\n";
    std::exit(1);
}

static po::variables_map processOptions(int argc, const char **argv)
{
    po::options_description desc("Options");
    desc.add_options()
        ("help",                                      "Show help")
        ("items",         po::value<std::size_t>()->default_value(10000000),
                                                      "Number of elements to process")
        ("key-type",      po::value<std::string>()->default_value("uint"),
                                                      "Type for keys in sort")
        ("key-bits",      po::value<unsigned int>(),  "Number of bits on which to sort")
        ("key-min",       po::value<cl_ulong>()->default_value(0),
                                                      "Minimum random key")
        ("key-max",       po::value<cl_ulong>(),      "Maximum random key")
        ("value-type",    po::value<std::string>()->default_value("uint"),
                                                      "Type of values to sort or scan")
        ("iterations",    po::value<unsigned int>()->default_value(10),
                                                      "Number of repetitions to run")
        ("algorithm",     po::value<std::string>()->default_value("sort"),
                                                      "Algorithm to benchmark (sort | scan)");

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

/* Figures out which uniform distribution class we want to use.
 * We default to uniform_int, then override to uniform_real if
 * T is real.
 */
template<typename T>
class uniform
{
public:
    typedef RANDOM_NAMESPACE::uniform_int<T> type;
};

template<>
class uniform<float>
{
public:
    typedef RANDOM_NAMESPACE::uniform_real<float> type;
};

template<>
class uniform<double>
{
public:
    typedef RANDOM_NAMESPACE::uniform_real<double> type;
};

template<typename T>
static cl::Buffer randomBuffer(
    const cl::CommandQueue &queue,
    RANDOM_NAMESPACE::mt19937 &engine,
    std::size_t elements, int length,
    T minValue, T maxValue)
{
    const cl::Context &context = queue.getInfo<CL_QUEUE_CONTEXT>();

    if (elements > std::numeric_limits<size_t>::max() / (length * sizeof(T)))
    {
        std::cerr << "Number of elements is too large for size_t.\n";
        std::exit(1);
    }

    typename uniform<T>::type dist(minValue, maxValue);
    RANDOM_NAMESPACE::variate_generator<RANDOM_NAMESPACE::mt19937 &, typename uniform<T>::type> gen(engine, dist);

    elements *= length;
    std::size_t size = elements * sizeof(T);
    cl::Buffer buffer(context, CL_MEM_READ_WRITE, size);
    T *data = static_cast<T *>(queue.enqueueMapBuffer(buffer, CL_TRUE, CL_MAP_WRITE, 0, size));
    std::generate(data, data + elements, gen);
    queue.enqueueUnmapMemObject(buffer, data);
    return buffer;
}

/* Overload of randomBuffer that has no explicitly specified min and max elements.
 */
template<typename T>
static cl::Buffer randomBuffer(
    const cl::CommandQueue &queue,
    RANDOM_NAMESPACE::mt19937 &engine,
    std::size_t elements, int length)
{
    if (std::numeric_limits<T>::is_integer)
    {
        return randomBuffer(queue, engine, elements, length,
                            std::numeric_limits<T>::min(),
                            std::numeric_limits<T>::max());
    }
    else
    {
        return randomBuffer(queue, engine, elements, length, (T) -10.0, (T) 10.0);
    }
}

/* Overload of randomBuffer that determines the actual type at runtime.
 */
static cl::Buffer randomBuffer(
    const cl::CommandQueue &queue,
    RANDOM_NAMESPACE::mt19937 &engine,
    std::size_t elements, const clogs::Type &type,
    cl_ulong minValue, cl_ulong maxValue)
{
    if (type.getBaseType() == clogs::TYPE_VOID)
        return cl::Buffer();

    int length = type.getSize() / clogs::Type(type.getBaseType()).getSize();
    switch (type.getBaseType())
    {
    case clogs::TYPE_UCHAR:
        {
            typedef cl_uchar T;
            return randomBuffer<T>(queue, engine, elements, length, (T) minValue, (T) maxValue);
        }
    case clogs::TYPE_CHAR:
        {
            typedef cl_char T;
            return randomBuffer<T>(queue, engine, elements, length, (T) minValue, (T) maxValue);
        }
    case clogs::TYPE_USHORT:
        {
            typedef cl_ushort T;
            return randomBuffer<T>(queue, engine, elements, length, (T) minValue, (T) maxValue);
        }
    case clogs::TYPE_SHORT:
        {
            typedef cl_short T;
            return randomBuffer<T>(queue, engine, elements, length, (T) minValue, (T) maxValue);
        }
    case clogs::TYPE_UINT:
        {
            typedef cl_uint T;
            return randomBuffer<T>(queue, engine, elements, length, (T) minValue, (T) maxValue);
        }
    case clogs::TYPE_INT:
        {
            typedef cl_int T;
            return randomBuffer<T>(queue, engine, elements, length, (T) minValue, (T) maxValue);
        }
    case clogs::TYPE_ULONG:
        {
            typedef cl_ulong T;
            return randomBuffer<T>(queue, engine, elements, length, (T) minValue, (T) maxValue);
        }
    case clogs::TYPE_LONG:
        {
            typedef cl_long T;
            return randomBuffer<T>(queue, engine, elements, length, (T) minValue, (T) maxValue);
        }
    default:
        return cl::Buffer(); // should never be reached
    }
}

/* Overload of randomBuffer that determines the actual type at runtime.
 */
static cl::Buffer randomBuffer(
    const cl::CommandQueue &queue,
    RANDOM_NAMESPACE::mt19937 &engine,
    std::size_t elements, const clogs::Type &type)
{
    if (type.getBaseType() == clogs::TYPE_VOID)
        return cl::Buffer();

    int length = type.getSize() / clogs::Type(type.getBaseType()).getSize();
    switch (type.getBaseType())
    {
    case clogs::TYPE_UCHAR:
        {
            typedef cl_uchar T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    case clogs::TYPE_CHAR:
        {
            typedef cl_char T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    case clogs::TYPE_USHORT:
        {
            typedef cl_ushort T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    case clogs::TYPE_SHORT:
        {
            typedef cl_short T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    case clogs::TYPE_UINT:
        {
            typedef cl_uint T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    case clogs::TYPE_INT:
        {
            typedef cl_int T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    case clogs::TYPE_ULONG:
        {
            typedef cl_ulong T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    case clogs::TYPE_LONG:
        {
            typedef cl_long T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    case clogs::TYPE_HALF:
        {
            /* Special case: half is a 16-bit integral C type. Pick
             * the random to avoid non-finite values and denorms.
             */
            return randomBuffer<cl_half>(queue, engine, elements, length, 0x0400, 0x7BFF);
        }
    case clogs::TYPE_FLOAT:
        {
            typedef cl_float T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    case clogs::TYPE_DOUBLE:
        {
            typedef cl_double T;
            return randomBuffer<T>(queue, engine, elements, length);
        }
    default:
        return cl::Buffer(); // should never be reached
    }
}

/* Computes 2^x - 1, in a way that is well-defined if x is the number of bits
 * in cl_ulong.
 */
static cl_ulong upper(unsigned int bits)
{
    assert(bits > 0);
    cl_ulong mid = cl_ulong(1) << (bits - 1);
    return mid + (mid - 1);
}

static void runSort(const cl::CommandQueue &queue, const po::variables_map &vm)
{
    const cl::Context &context = queue.getInfo<CL_QUEUE_CONTEXT>();
    const cl::Device &device = queue.getInfo<CL_QUEUE_DEVICE>();

    const std::string keyTypeName = vm["key-type"].as<std::string>();
    clogs::Type keyType = matchType(keyTypeName);
    if (keyType.getLength() != 1
        || keyType.isSigned()
        || !keyType.isIntegral()
        || !keyType.isStorable(device)
        || !keyType.isComputable(device))
    {
        std::cerr << keyTypeName << " cannot be used as a sort key (must be a scalar unsigned integer).\n";
        std::exit(1);
    }

    const std::string valueTypeName = vm["value-type"].as<std::string>();
    clogs::Type valueType = matchType(valueTypeName);
    if (valueType.getBaseType() != clogs::TYPE_VOID
        && !valueType.isStorable(device))
    {
        std::cerr << valueTypeName << " is not usable on this device.\n";
        std::exit(1);
    }

    std::size_t elements = vm["items"].as<std::size_t>();
    if (elements <= 0)
    {
        std::cerr << "Number of items must be positive.\n";
        std::exit(1);
    };

    unsigned int bits;
    if (vm.count("key-bits"))
    {
        bits = vm["key-bits"].as<unsigned int>();
        if (bits <= 0)
        {
            std::cerr << "Number of bits must be positive.\n";
            std::exit(1);
        }
        else if (bits > keyType.getBaseSize() * 8)
        {
            std::cerr << "Number of bits is too large.\n";
            std::exit(1);
        }
    }
    else
        bits = keyType.getBaseSize() * 8;

    cl_ulong minValue = vm["key-min"].as<cl_ulong>();
    cl_ulong maxValue;
    if (vm.count("key-max"))
    {
        maxValue = vm["key-max"].as<cl_ulong>();
        if (maxValue > upper(bits))
        {
            std::cerr << "Maximum key value is too large.\n";
            std::exit(1);
        }
    }
    else
    {
        maxValue = upper(bits);
    }

    unsigned int iterations = vm["iterations"].as<unsigned int>();
    if (iterations <= 0)
    {
        std::cerr << "Number of iterations must be positive.\n";
        std::exit(1);
    }


    RANDOM_NAMESPACE::mt19937 engine;
    cl::Buffer keyBuffer = randomBuffer(queue, engine, elements, keyType, minValue, maxValue);
    std::size_t keyBufferSize = keyBuffer.getInfo<CL_MEM_SIZE>();
    cl::Buffer tmpKeyBuffer1(context, CL_MEM_READ_WRITE, keyBufferSize);
    cl::Buffer tmpKeyBuffer2(context, CL_MEM_READ_WRITE, keyBufferSize);

    cl::Buffer valueBuffer = randomBuffer(queue, engine, elements, valueType);
    cl::Buffer tmpValueBuffer1, tmpValueBuffer2;
    std::size_t valueBufferSize = 0;
    if (valueBuffer())
    {
        valueBufferSize = valueBuffer.getInfo<CL_MEM_SIZE>();
        tmpValueBuffer1 = cl::Buffer(context, CL_MEM_READ_WRITE, valueBufferSize);
        tmpValueBuffer2 = cl::Buffer(context, CL_MEM_READ_WRITE, valueBufferSize);
    }

    clogs::Radixsort sort(context, device, keyType, valueType);
    sort.setTemporaryBuffers(tmpKeyBuffer2, tmpValueBuffer2);

    double elapsed = 0.0;
    // pass 0 is a warm-up pass
    for (unsigned int i = 0; i <= iterations; i++)
    {
        /* Copy the keys to the buffer to be sorted */
        queue.enqueueCopyBuffer(keyBuffer, tmpKeyBuffer1, 0, 0, keyBufferSize);
        if (valueBuffer())
            queue.enqueueCopyBuffer(valueBuffer, tmpValueBuffer1, 0, 0, valueBufferSize);
        queue.finish();

        Timer timer;
        sort.enqueue(queue, tmpKeyBuffer1, tmpValueBuffer1, elements, bits, NULL, NULL);
        queue.finish();
        if (i != 0)
            elapsed += timer.getElapsed();
    }
    std::cout << "Sorted " << elements << " items " << iterations << " times in " << elapsed << " seconds.\n";
    std::cout << "Rate: " << double(elements) * iterations / elapsed / 1e6 << "M/s\n";
}

static void runScan(const cl::CommandQueue &queue, const po::variables_map &vm)
{
    const cl::Context &context = queue.getInfo<CL_QUEUE_CONTEXT>();
    const cl::Device &device = queue.getInfo<CL_QUEUE_DEVICE>();

    const std::string valueTypeName = vm["value-type"].as<std::string>();
    clogs::Type valueType = matchType(valueTypeName);
    if (!valueType.isIntegral()
        || !valueType.isStorable(device)
        || !valueType.isComputable(device))
    {
        std::cerr << valueTypeName << " cannot be used as a sort key (must be an integral type).\n";
        std::exit(1);
    }

    std::size_t elements = vm["items"].as<std::size_t>();
    if (elements <= 0)
    {
        std::cerr << "Number of items must be positive.\n";
        std::exit(1);
    };

    unsigned int iterations = vm["iterations"].as<unsigned int>();
    if (iterations <= 0)
    {
        std::cerr << "Number of iterations must be positive.\n";
        std::exit(1);
    }

    RANDOM_NAMESPACE::mt19937 engine;
    cl::Buffer buffer = randomBuffer<cl_uchar>(queue, engine, elements * valueType.getSize(), 1);
    clogs::Scan scan(context, device, valueType);

    double elapsed = 0.0;
    // pass 0 is a warm-up pass
    for (unsigned int i = 0; i <= iterations; i++)
    {
        Timer timer;
        scan.enqueue(queue, buffer, elements);
        queue.finish();
        if (i != 0)
            elapsed += timer.getElapsed();
    }
    std::cout << "Scanned " << elements << " items " << iterations << " times in " << elapsed << " seconds.\n";
    std::cout << "Rate: " << double(elements) * iterations / elapsed / 1e6 << "M/s\n";
}

int main(int argc, const char **argv)
{
    std::locale::global(std::locale(""));
    std::cout.imbue(std::locale());
    std::cerr.imbue(std::locale());
    try
    {
        po::variables_map vm = processOptions(argc, argv);

        cl::Device device;
        if (!findDevice(vm, device))
        {
            std::cerr << "Could not find a suitable OpenCL device\n";
            return 1;
        }
        else
            std::cout << "Using device " << device.getInfo<CL_DEVICE_NAME>() << "\n\n";
        cl::Context context = makeContext(device);
        cl::CommandQueue queue(context, device);

        const std::string algorithm = vm["algorithm"].as<std::string>();
        if (algorithm == "sort")
            runSort(queue, vm);
        else if (algorithm == "scan")
            runScan(queue, vm);
        else
        {
            std::cerr << "No such algorithm `" << algorithm << "'\n";
            return 1;
        }
    }
    catch (std::invalid_argument &e)
    {
        std::cerr << "\nERROR: " << e.what() << "\n";
        return 2;
    }
}
