// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#include <jd3d12/types.hpp>
#if defined(_MSC_VER)
    #include <intrin.h>
#elif JD3D12_CPP20
    #include <bit>
#endif

namespace jd3d12
{

uint32_t CountBitsSet(uint32_t a)
{
#if defined(_MSC_VER)
    return __popcnt(a);
#elif defined(__GNUC__) || defined(__clang__)
    return uint32_t(__builtin_popcount(x));
#elif JD3D12_CPP20
    return uint32_t(std::popcount(a));
#else
    uint32_t count = 0;
    while (a > 0)
    {
        a &= (a - 1);
        ++count;
    }
    return count;
#endif
}

} // namespace jd3d12
