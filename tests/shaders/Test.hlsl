// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

struct MyStruct
{
    float value;
};

RWBuffer<float> typed_buf : register(u0);
RWStructuredBuffer<MyStruct> structured_buf : register(u1);
RWByteAddressBuffer byte_address_buf : register(u2);

[numthreads(1, 1, 1)]
void Main_Typed(uint3 dtid : SV_DispatchThreadID)
{
    float f = typed_buf[dtid.x];
    f = f * f + 1.0;
    typed_buf[dtid.x] = f;
}

[numthreads(1, 1, 1)]
void Main_Structured(uint3 dtid : SV_DispatchThreadID)
{
    MyStruct s = structured_buf[dtid.x];
    s.value = s.value * s.value + 1.0;
    structured_buf[dtid.x] = s;
}

[numthreads(1, 1, 1)]
void Main_ByteAddress(uint3 dtid : SV_DispatchThreadID)
{
    uint address = dtid.x * sizeof(float);
    float f = byte_address_buf.Load<float>(address);
    f = f * f + 1.0;
    byte_address_buf.Store<float>(address, f);
}

#if ENABLING_MACRO == 4
[numthreads(32, 1, 1)]
void Main_Conditional(uint3 dtid : SV_DispatchThreadID)
{
    uint address = dtid.x * sizeof(float);
    uint value = dtid.x * dtid.x + 1;
    byte_address_buf.Store<uint>(address, value);
}
#endif
