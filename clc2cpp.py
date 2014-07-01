#!/usr/bin/env python

"""
Takes OpenCL C code and turns it into constant strings in a C++ source file.
"""

from __future__ import print_function, division
import sys
import re
import hashlib
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
            #include <clogs/visibility_push.h>
            #include <map>
            #include <string>
            #include <clogs/visibility_pop.h>
            #include "../../src/utils.h"

            namespace clogs
            {
            namespace detail
            {

            static std::map<std::string, Source> g_sources;
            CLOGS_LOCAL const std::map<std::string, Source> &getSourceMap() { return g_sources; }

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
            label = re.sub(r'\\', '/', i) # Fix up Windows separators
            label = re.sub(r'\.\./kernels/', '', label)
            with open(i, 'r') as inf:
                lines = inf.readlines()
                hasher = hashlib.sha256()
                map(hasher.update, lines)
                lines = [escape(line.rstrip('\n')) for line in lines]
                print('    clogs::detail::g_sources["{0}"] = clogs::detail::Source('.format(escape(label)),
                        file = outf)
                for line in lines:
                    print('        "{0}\\n"'.format(line), file = outf)
                print('        , "{0}");'.format(hasher.hexdigest()), file = outf)
        print(dedent('''
            }

            static Init init;

            } // namespace'''), file = outf)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
