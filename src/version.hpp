// Copyright (c) 2025 David Lucius Severus
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#pragma once

#include <micron/version.hpp>

namespace abc
{

constexpr static const int ABCMALLOC_VERSION_MAJOR = 0x000;
constexpr static const int ABCMALLOC_VERSION_MINOR = 0x090;
constexpr static const int ABCMALLOC_VERSION_PATCH = 0x000;

template <int __major, int __minor, int __patch>
constexpr bool
is_version()
{
  if constexpr ( __major == ABCMALLOC_VERSION_MAJOR and __minor == ABCMALLOC_VERSION_MINOR and __patch == ABCMALLOC_VERSION_PATCH )
    return true;
  return false;
}

constexpr int
get_version(void)
{
  return ABCMALLOC_VERSION_MAJOR | ABCMALLOC_VERSION_MINOR | ABCMALLOC_VERSION_PATCH;
}

static_assert(MICRON_VERSION_MAJOR == 0x000, "! invalid micron version, could result in UB");
static_assert(MICRON_VERSION_MINOR == 0x050, "! invalid micron version, could result in UB");

};
