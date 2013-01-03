/* Copyright (c) 2013 University of Cape Town
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
 * Base 64 encoding, as per RFC 4648.
 */

#include <cstddef>
#include <string>
#include "base64_encode.h"

static const char base64table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64encode(const std::string &plain)
{
    std::string out;
    std::size_t i;
    for (i = 0; i < plain.size(); i += 3)
    {
        unsigned int q = 0;
        for (int j = 0; j < 3; j++)
        {
            q <<= 8;
            if (i + j < plain.size())
                q |= (unsigned char) plain[i + j];
        }
        int oc = 4;
        if (plain.size() - i + 1 < std::size_t(oc))
            oc = plain.size() - i + 1;
        int shift = 18;
        for (int j = 0; j < oc; j++)
        {
            unsigned int idx = (q >> shift) & 0x3f;
            out += base64table[idx];
            shift -= 6;
        }
        for (int j = oc; j < 4; j++)
            out += '=';
    }
    return out;
}
