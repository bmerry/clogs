Please refer to the [user manual](http://bmerry.github.com/clogs)
for instructions on compiling and installing clogs.

For the impatient, the process is

```sh
$ ./waf configure [ --prefix=install-path ]
$ ./waf build
$ sudo ./waf install
```

which will build and install to the install path (defaults to `/usr/local`),
and which will install the documentation in `/usr/local/share/doc/clogs`
if you have xsltproc and doxygen installed.
