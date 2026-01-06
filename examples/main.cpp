// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#include <jd3d12/jd3d12.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <atlbase.h>

#include <array>
#include <string>
#include <memory>

#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cassert>

using namespace jd3d12;

constexpr size_t kMainBufSize = 10 * kMegabyte;

std::array<float, 8> main_src_data = { 1, 2, 3, 4, 5, 6, 7, 8 };
StaticBufferFromMemory main_upload_buffer{
    BufferDesc{
        L"My buffer UPLOAD",
        kBufferUsageFlagCpuSequentialWrite | kBufferUsageFlagCopySrc,
        kMainBufSize },
        ConstDataSpan{main_src_data.data(), main_src_data.size() * sizeof(float)} };
StaticBuffer main_readback_buffer{
    BufferDesc{
        L"My buffer READBACK",
        kBufferUsageFlagCopyDst | kBufferUsageFlagCpuRead,
        kMainBufSize } };

StaticShaderCompiledFromFile byte_address_shader{
    ShaderCompilationParams{
        0,//kShaderCompilationFlagDisableIncludes,
        kCharacterEncodingAnsi,
        L"Main_ByteAddress" },
    ShaderDesc{ },
    L"c:/Code/JustD3D12Compute/REPO/tests/shaders/Test.hlsl"
};

Environment* g_env;
Device* g_dev;

#define REQUIRE assert
#define CHECK assert

void MyLogCallback(LogSeverity severity, const wchar_t* message, void* context)
{
    wprintf(L"LogCallback [%s] %s\n", GetLogSeverityString(severity), message);
}

int main(int argc, char** argv)
{

    std::unique_ptr<Environment> env;
    std::unique_ptr<Device> dev;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    EnvironmentDesc env_desc{};
    env_desc.flags = kEnvironmentFlagLogStandardOutput
        | kEnvironmentFlagEnableD3d12DebugLayer
        | kEnvironmentFlagEnableD3d12GpuBasedValidation;
    env_desc.log_severity = kLogSeverityAll;
    //env_desc.log_callback = MyLogCallback;
    //env_desc.log_file_path = L"log.log";
    assert(Succeeded(CreateEnvironment(env_desc, g_env)));
    env.reset(g_env);

    //g_env->Log(kLogSeverityDebug, L"Test log message");

    /*
    std::unique_ptr<ShaderCompilationResult> shader_compilation_result;
    {
        const wchar_t* predefined_macros[] = { L"ENABLE_FOO", L"2" };

        ShaderCompilationParams shader_compilation_params{};
        shader_compilation_params.entry_point = L"Main_ByteAddress";
        shader_compilation_params.macro_defines = { predefined_macros, _countof(predefined_macros) };

        ShaderCompilationResult* result_ptr = nullptr;
        Result r = env->CompileShaderFromFile(shader_compilation_params,
            L"c:/Code/JustD3D12Compute/REPO/tests/shaders/Test.hlsl", result_ptr);
        shader_compilation_result.reset(result_ptr);

        const char* errors_and_warnings = shader_compilation_result->GetErrorsAndWarnings();
        if(!IsStringEmpty(errors_and_warnings))
            wprintf(L"ERRORS AND WARNINGS:\n%S\n", errors_and_warnings);

        REQUIRE(Succeeded(r));
        REQUIRE(Succeeded(shader_compilation_result->GetResult()));
    }
    */

    DeviceDesc device_desc;
    device_desc.name = L"My device za\u017C\u00F3\u0142\u0107 g\u0119\u015Bl\u0105 ja\u017A\u0144";
    assert(Succeeded(env->CreateDevice(device_desc, g_dev)));
    dev.reset(g_dev);

    {
        BufferDesc default_buf_desc{};
        default_buf_desc.name = L"My buffer DEFAULT";
        default_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagCopyDst | kBufferFlagByteAddress
            | kBufferUsageFlagShaderRWResource;
        default_buf_desc.size = kMainBufSize;
        Buffer* buffer_ptr = nullptr;
        REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
        std::unique_ptr<Buffer> default_buffer{ buffer_ptr };

        REQUIRE(main_upload_buffer.GetBuffer() != nullptr);
        REQUIRE(Succeeded(g_dev->CopyBufferRegion(*main_upload_buffer.GetBuffer(),
            Range{0, kMainBufSize}, *default_buffer, 0)));
        REQUIRE(Succeeded(g_dev->BindRWBuffer(2, default_buffer.get())));
        REQUIRE(byte_address_shader.GetShader() != nullptr);
        REQUIRE(Succeeded(g_dev->DispatchComputeShader(*byte_address_shader.GetShader(), { 8, 1, 1 })));
        g_dev->ResetAllBindings();
        REQUIRE(main_readback_buffer.GetBuffer() != nullptr);
        REQUIRE(Succeeded(g_dev->CopyBuffer(*default_buffer, *main_readback_buffer.GetBuffer())));

        REQUIRE(Succeeded(g_dev->SubmitPendingCommands()));
        REQUIRE(Succeeded(g_dev->WaitForGPU()));

        std::array<float, 8> dst_data;
        REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*main_readback_buffer.GetBuffer(),
            Range{0, dst_data.size() * sizeof(float)}, dst_data.data())));

        for(size_t i = 0; i < dst_data.size(); ++i)
        {
            CHECK(dst_data[i] == main_src_data[i] * main_src_data[i] + 1.f);
        }
    }
}
