#include "include_test_header.hlsl"

[numthreads(1, 1, 1)]
void Main(uint3 dtid : SV_DispatchThreadID)
{
    float f = typed_buf[dtid.x];
    f = f * f + 1.0;
    typed_buf[dtid.x] = f;
}
