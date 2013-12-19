# Copyright (c) 2012 University of Cape Town
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

# This is a gnuplot script that will generate the benchmark graph.
# It is run as
#
# gnuplot -e "in=<basename>; out=<name>; device=<string>" clogs-benchmark.plot
# where "out" is the output filename (including .svg extension), "in" is the
# common prefix of the input filenames, and "device" is used in the title.
#
# clogs version. It is currently run manually rather than from the build
# system, so that people don't need gnuplot just to build.

set terminal svg dashed size 800,400
set output out
set log x
set key outside
set title "CLOGS sorting rate"
set xlabel "Elements"
set ylabel "MKeys/s"
set ytics mirror
set grid ytics
set mytics
plot in.'480gtx/uint-void.txt' title "480 GTX / uint keys / no values" with linespoints lt 1 lc 1, \
     in.'480gtx/uint-uint.txt' title "480 GTX / uint keys / uint values" with linespoints lt 1 lc 2, \
     in.'480gtx/ulong-float4.txt' title "480 GTX / ulong keys / float4 values" with linespoints lt 1 lc 3, \
     in.'r9-270/uint-void.txt' title "R9 270 / uint keys / no values" with linespoints lt 3 lc 1, \
     in.'r9-270/uint-uint.txt' title "R9 270 / uint keys / uint values" with linespoints lt 3 lc 2, \
     in.'r9-270/ulong-float4.txt' title "R9 270 / ulong keys / float4 values" with linespoints lt 3 lc 3
