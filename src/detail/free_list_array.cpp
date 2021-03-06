// Copyright (C) 2015 Jonathan Müller <jonathanmueller.dev@gmail.com>
// This file is subject to the license terms in the LICENSE file
// found in the top-level directory of this distribution.

#include "detail/free_list_array.hpp"

#include <cfloat>
#include <climits>
#include <cstdint>

#include "error.hpp"

using namespace foonathan::memory;
using namespace detail;

namespace
{
    template <typename Integral>
    bool is_power_of_two(Integral no) FOONATHAN_NOEXCEPT
    {
        return (no & (no - 1)) == 0;
    }

    // ilog2 is a ceiling implementation of log2 for integers
    // that is: ilog2(4) == 2, ilog2(5) == 3
#if defined __GNUC__
    // we have a builtin to count leading zeros, use it
    // subtract one if power of two, otherwise 0
    // multiple overloads to support each size of std::size_t
    std::size_t ilog2(unsigned int no) FOONATHAN_NOEXCEPT
    {
        return sizeof(no) * CHAR_BIT
                - unsigned(__builtin_clz(no)) - unsigned(is_power_of_two(no));
    }

    std::size_t ilog2(unsigned long no) FOONATHAN_NOEXCEPT
    {
        return sizeof(no) * CHAR_BIT
                - unsigned(__builtin_clzl(no)) - unsigned(is_power_of_two(no));
    }

    std::size_t ilog2(unsigned long long no) FOONATHAN_NOEXCEPT
    {
        return sizeof(no) * CHAR_BIT
                - unsigned(__builtin_clzll(no)) - unsigned(is_power_of_two(no));
    }
#else
    // naive loop, starting with 1 for non power of two to be ceiling
    std::size_t ilog2(std::size_t no) FOONATHAN_NOEXCEPT
    {
        std::size_t result = is_power_of_two(no) ? 0u : 1u;
        while (no >>= 1)
            ++result;
        return result;
    }
#endif
}

std::size_t log2_access_policy::index_from_size(std::size_t size) FOONATHAN_NOEXCEPT
{
    FOONATHAN_MEMORY_ASSERT_MSG(size, "size must not be zero");
    return ilog2(size);
}

std::size_t log2_access_policy::size_from_index(std::size_t index) FOONATHAN_NOEXCEPT
{
    return 1 << index;
}
