Buffer<float> src_buf : register(t0);
RWBuffer<float> dst_buf : register(u0);

[numthreads(1, 1, 1)]
void Main(uint3 dtid : SV_DispatchThreadID)
{
    float f = src_buf[dtid.x];
    f *= f;
    dst_buf[dtid.x] = f;
}
