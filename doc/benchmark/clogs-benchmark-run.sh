#!/bin/sh

# Copyright (c) 2012-2013 University of Cape Town
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

export LC_ALL=C
if [ "$#" -ne 1 ]; then
    echo "Usage: clogs-benchmark <directory>" 1>&2
    exit 2
fi
if ! [ -d "$1" ]; then
    echo "$1 does not exist or is not a directory" 1>&2
    exit 1
fi
small_sizes="1000 2000 5000 10000 20000 50000 100000 200000 500000 1000000 2000000 5000000 10000000"
big_sizes="20000000 50000000"
(for i in $small_sizes $big_sizes; do
    echo -n "$i "; clogs-benchmark --key-type=uint --value-type=uint --iterations 50 --items $i --cl-gpu | tail -n 1 | sed 's/.* \(.*\)M.s/\1/'
done) > "$1/uint-uint.txt"
(for i in $small_sizes $big_sizes; do
    echo -n "$i "; clogs-benchmark --key-type=uint --value-type=void --iterations 50 --items $i --cl-gpu | tail -n 1 | sed 's/.* \(.*\)M.s/\1/'
done) > "$1/uint-void.txt"
(for i in $small_sizes; do
    echo -n "$i "; clogs-benchmark --key-type=ulong --value-type=float4 --iterations 50 --items $i --cl-gpu | tail -n 1 | sed 's/.* \(.*\)M.s/\1/'
done) > "$1/ulong-float4.txt"
