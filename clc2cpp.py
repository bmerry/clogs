#!/usr/bin/env python

"""
Takes OpenCL C code and turns it into constant strings in a C++ source file.
"""

from __future__ import print_function, division
import sys
import re
from textwrap import dedent

__author__ = "Bruce Merry"
__copyright__ = "Copyright 2012, University of Cape Town"
__license__ = "MIT"
__maintainer__ = "Bruce Merry"
__email__ = "bmerry@users.sourceforge.net"

ESCAPE_RE = re.compile(r'[\\"]')

def escape(string):
    """
    Escapes a string so that it can be safely included inside a C string.
    The string must not contain newlines, nor characters not valid in OpenCL C.
    """
    return ESCAPE_RE.sub(r'\\\g<0>', string)

def main(argv):
    """
    Main program
    """
    if len(argv) < 2:
        print("Usage: {0} <input.cl>... <output.cpp>".format(sys.argv[0]),
                file = sys.stderr)
        return 2

    with open(sys.argv[-1], 'w') as outf:
        print(dedent('''
            #include <clogs/visibility.h>

            #ifdef CLOGS_DLL_DO_PUSH_POP
            # pragma GCC visibility push(default)
            #endif
            #include <map>
            #include <string>
            #ifdef CLOGS_DLL_DO_PUSH_POP
            # pragma GCC visibility pop
            #endif

            static std::map<std::string, std::string> g_sources;

            namespace clogs
            {
            namespace detail
            {

            CLOGS_LOCAL const std::map<std::string, std::string> &getSourceMap() { return g_sources; }

            }} // namespace clogs::detail

            namespace
            {

            struct Init
            {
                Init();
            };

            Init::Init()
            {'''), file = outf)
        for i in sys.argv[1:-1]:
            label = re.sub(r'\.\./kernels/', '', i)
            with open(i, 'r') as inf:
                lines = inf.readlines()
                lines = [escape(line.rstrip('\n')) for line in lines]
                print('    g_sources["{0}"] ='.format(escape(label)),
                        file = outf)
                for line in lines:
                    print('        "{0}\\n"'.format(line), file = outf)
                print('        ;', file = outf)
        print(dedent('''
            }

            static Init init;

            } // namespace'''), file = outf)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
