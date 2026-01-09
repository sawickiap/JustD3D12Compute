// Copyright (c) 2025-2026 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

// This file is deliberately saved in UTF-16 LE, unlike other .hlsl files, to test
// character encoding.

#include "include_test_header.hlsl"

[numthreads(1, 1, 1)]
void Main(uint3 dtid : SV_DispatchThreadID)
{
    float f = typed_buf[dtid.x];
    f = f * f + 1.0;
    typed_buf[dtid.x] = f;
}
