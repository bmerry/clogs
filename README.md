# Introduction

**Note**: CLOGS is no longer being actively developed. If you need new
features, your best option is to develop and submit a pull request, or even
better, offer to take over.

CLOGS is a library for higher-level operations on top of the OpenCL C++ API. It
is designed to integrate with other OpenCL code, including synchronization
using OpenCL events. Currently only three operations are supported: radix
sorting, reduction, and exclusive scan. Radix sort supports all the unsigned
integral types as keys, and all the built-in scalar and vector types suitable
for storage in buffers as values. Scan supports all the integral types. It also
supports vector types, which allows for limited multi-scan capabilities.
Reduction supports all the built-in types, but the floating-point types are not
tested.

For more information, refer to the [user
manual](http://bmerry.github.com/clogs). There is also a
[paper](http://brucemerry.org.za/publications.html#a-performance-comparison-of-sort-and-scan-libraries-for-gpus)
comparing performance to other libraries.

## Quick install instructions

For the impatient, the process is

```sh
$ ./waf configure [ --prefix=install-path ]
$ ./waf build
$ sudo ./waf install
```

which will build and install to the install path (defaults to `/usr/local`),
and which will install the documentation in `/usr/local/share/doc/clogs`
if you have xsltproc and doxygen installed.
