dxc -T cs_6_6 -E Main_Typed -Fo Test_Typed.dxil Test.hlsl
dxc -T cs_6_6 -E Main_Structured -Fo Test_Structured.dxil Test.hlsl
dxc -T cs_6_6 -E Main_ByteAddress -Fo Test_ByteAddress.dxil Test.hlsl

dxc -T cs_6_6 -E Main -Fo CopySquaredTyped.dxil CopySquaredTyped.hlsl

dxc -T cs_6_6 -E Main -Fo constant_buffer.dxil constant_buffer.hlsl
