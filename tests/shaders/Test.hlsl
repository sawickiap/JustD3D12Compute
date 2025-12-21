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
