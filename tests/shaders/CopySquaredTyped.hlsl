// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

Buffer<float> src_buf : register(t0);
RWBuffer<float> dst_buf : register(u0);

[numthreads(1, 1, 1)]
void Main(uint3 dtid : SV_DispatchThreadID)
{
    float f = src_buf[dtid.x];
    f *= f;
    dst_buf[dtid.x] = f;
}
