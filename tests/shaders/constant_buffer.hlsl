// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

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
