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
 * Base 64 decoding, as per RFC 4648.
 */

#ifndef BASE64_DECODE_H
#define BASE64_DECODE_H

#include <clogs/visibility_push.h>
#include <string>
#include <stdexcept>
#include <clogs/visibility_pop.h>

/**
 * Exception thrown to indicate a malformed base64 string.
 */
class CLOGS_LOCAL Base64DecodeError : public std::runtime_error
{
public:
    Base64DecodeError(const std::string &msg) : std::runtime_error(msg) {}
};

/**
 * Decode a base64 string. The input string must conform to RFC 4648, and
 * specifically it must not contain any characters other than the 65 permitted
 * ones (no whitespace), and it must be correctly padded.
 *
 * @return The binary string encoded by @a encoded, which may include ASCII NULs
 * @throw Base64DecodeError if the string does not conform to RFC 4648.
 */
std::string base64decode(const std::string &encoded);

#endif /* !BASE64_DECODE_H */
