struct MyConstants
{
	float4 floats;
	uint4 uints;
};

cbuffer MyConstantBuffer : register(b0)
{
	MyConstants constants;
}
RWStructuredBuffer<MyConstants> output_buffer : register(u0);

[numthreads(1, 1, 1)]
void Main()
{
	output_buffer[0] = constants;
}
