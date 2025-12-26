// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#include <jd3d12/jd3d12.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

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

using namespace jd3d12;

namespace
{

/*
StaticShaderFromFile typed_shader{
    ShaderDesc{ L"Typed shader" }, L"Test_Typed.dxil" };
StaticShaderFromFile structured_shader{
    ShaderDesc{ L"Structured shader" }, L"Test_Structured.dxil" };
StaticShaderFromFile byte_address_shader{
    ShaderDesc{ L"ByteAddress shader" }, L"Test_ByteAddress.dxil" };
StaticShaderFromFile copy_squared_typed_shader{
    ShaderDesc{ L"copy_squared_typed" }, L"CopySquaredTyped.dxil" };
*/

constexpr size_t kMainBufSize = 10 * kMegabyte;

std::array<float, 8> main_src_data = { 1, 2, 3, 4, 5, 6, 7, 8 };

Buffer* g_main_upload_buffer;
Buffer* g_main_readback_buffer;

Environment* g_env;
Device* g_dev;

#define CHECK_OUTSIDE_TESTS(expr) \
    do { \
        if(!(expr)) { \
            std::fprintf(stderr, "CHECK failed: %s\n", #expr); \
            std::abort(); \
        } \
    } while(false)

bool HasListTests(int argc, char** argv)
{
    for(int i = 1; i < argc; ++i)
    {
        if(std::strcmp(argv[i], "--list-tests") == 0)
            return true;
    }
    return false;
}

} // anonymous namespace

int main(int argc, char** argv)
{
    std::unique_ptr<Environment> env;
    std::unique_ptr<Device> dev;
    std::unique_ptr<Buffer> main_upload_buf;
    std::unique_ptr<Buffer> main_readback_buf;

    if(!HasListTests(argc, argv))
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        EnvironmentDesc env_desc{};
        CHECK_OUTSIDE_TESTS(Succeeded(CreateEnvironment(env_desc, g_env)));
        env.reset(g_env);

        DeviceDesc device_desc;
        device_desc.name = L"My device";
        CHECK_OUTSIDE_TESTS(Succeeded(env->CreateDevice(device_desc, g_dev)));
        dev.reset(g_dev);
        
        {
            BufferDesc upload_buf_desc{};
            upload_buf_desc.name = L"My buffer UPLOAD";
            upload_buf_desc.flags = kBufferUsageFlagCpuSequentialWrite | kBufferUsageFlagCopySrc;
            upload_buf_desc.size = kMainBufSize;
            CHECK_OUTSIDE_TESTS(Succeeded(g_dev->CreateBufferFromMemory(
                upload_buf_desc,
                ConstDataSpan{ main_src_data.data(), main_src_data.size() * sizeof(float) },
                g_main_upload_buffer)));
            main_upload_buf.reset(g_main_upload_buffer);
        }

        {
            BufferDesc readback_buf_desc{};
            readback_buf_desc.name = L"My buffer READBACK";
            readback_buf_desc.flags = kBufferUsageFlagCopyDst | kBufferUsageFlagCpuRead;
            readback_buf_desc.size = kMainBufSize;
            CHECK_OUTSIDE_TESTS(Succeeded(g_dev->CreateBuffer(readback_buf_desc, g_main_readback_buffer)));
            main_readback_buf.reset(g_main_readback_buffer);
        }
    }

    return Catch::Session().run(argc, argv);
}

////////////////////////////////////////////////////////////////////////////////
// Tests

TEST_CASE("DivideRoundingUp scalar", "[types]")
{
    const uint32_t count = 100;
    const uint32_t group_count = 8;
    CHECK(DivideRoundingUp(count, group_count) == 13);
}

TEST_CASE("DivideRoundingUp vector", "[types]")
{
    UintVec3 count_v = { 100, 50, 25 };
    UintVec3 group_count_v = { 8, 16, 4 };
    UintVec3 expected_result = { 13, 4, 7 };
    CHECK(DivideRoundingUp(count_v, group_count_v) == expected_result);
}

TEST_CASE("Clamp scalar", "[types]")
{
    float a = 1.5f, b = 10.5f, c = 1e7f;
    CHECK(clamp(b, a, c) == b);
    CHECK(clamp(a, b, c) == b);
    CHECK(clamp(c, a, b) == b);
}

TEST_CASE("Clamp vector with scalar min/max", "[types]")
{
    FloatVec2 v1 = { 1.0f, 5.0f };
    float min_val = 2.0f;
    float max_val = 4.0f;
    FloatVec2 expected_result = { 2.0f, 4.0f };
    CHECK(clamp(v1, min_val, max_val) == expected_result);
}

TEST_CASE("Clamp vector with vector min/max", "[types]")
{
    FloatVec2 v1 = { 1.0f, 5.0f };
    FloatVec2 min_val = { 2.0f, 2.0f };
    FloatVec2 max_val = { 4.0f, 4.0f };
    FloatVec2 expected_result = { 2.0f, 4.0f };
    CHECK(clamp(v1, min_val, max_val) == expected_result);
}

TEST_CASE("Saturate float and double", "[types]")
{
    {
        float a = -0.5f;
        float b = 0.5f;
        float c = 2.0f;
        CHECK(saturate(a) == 0.0f);
        CHECK(saturate(b) == 0.5f);
        CHECK(saturate(c) == 1.0f);
    }

    {
        double a = -0.5;
        double b = 0.5;
        double c = 2.0;
        CHECK(saturate(a) == 0.0);
        CHECK(saturate(b) == 0.5);
        CHECK(saturate(c) == 1.0);
    }
}

TEST_CASE("Saturate vector", "[types]")
{
    FloatVec3 v = { -1.0f, 0.5f, 2.0f };
    FloatVec3 expected = { 0.0f, 0.5f, 1.0f };
    CHECK(saturate(v) == expected);
}

TEST_CASE("Lerp scalar", "[types]")
{
    float a = 10.f, b = 20.f;
    CHECK(lerp(a, b, 0.f) == a);
    CHECK(lerp(a, b, 1.f) == b);
    CHECK(lerp(a, b, 0.5f) == 15.f);
    CHECK(lerp(a, b, 2.f) == 30.f);
}

TEST_CASE("Lerp vector DoubleVec2 with scalar and vector t", "[types]")
{
    {
        DoubleVec2 a = { 0., 10. }, b = { 10., 20. };
        DoubleVec2 expected = { 0., 10. };
        CHECK(lerp(a, b, 0.f) == expected);
        expected = { 10., 20. };
        CHECK(lerp(a, b, 1.f) == expected);
        expected = { 5., 15. };
        CHECK(lerp(a, b, 0.5f) == expected);
        expected = { 20., 30. };
        CHECK(lerp(a, b, 2.f) == expected);
    }

    {
        DoubleVec2 a = { 0., 10. }, b = { 10., 20. }, t = { 0.5, 1.0 };
        DoubleVec2 expected = { 5., 20. };
        CHECK(lerp(a, b, t) == expected);
    }
}

TEST_CASE("Bitcast conversions asuint/asfloat", "[types]")
{
    {
        float a = 1.5f;
        CHECK(asuint(a) == 0x3fc00000u);
    }

    {
        uint32_t u = 0x44268000u;
        CHECK(asfloat(u) == 666.f);
    }

    {
        FloatVec3 fv = { 0.f, 1.f, -10.f };
        UintVec3 expected = { 0x00000000u, 0x3f800000u, 0xc1200000u };
        CHECK(asuint(fv) == expected);
    }
}

TEST_CASE("Format description", "[utils][format]")
{
    const FormatDesc* desc = GetFormatDesc(Format::kR16G16_Snorm);
    REQUIRE(desc != nullptr);
    CHECK(desc->name == std::wstring(L"R16G16_Snorm"));
    CHECK(desc->component_format == Format::kR16_Snorm);
    CHECK(desc->bits_per_element == 32);
    CHECK(desc->component_count == 2);
    CHECK(desc->active_component_count == 2);
    CHECK(desc->is_simple == 1);
}

TEST_CASE("Typed buffer", "[gpu][buffer][hlsl]")
{
    std::unique_ptr<Shader> typed_shader;
    {
        ShaderCompilationParams compilation_params{};
        compilation_params.entry_point = L"Main_Typed";

        ShaderDesc shader_desc{};
        shader_desc.name = L"Typed shader";

        Shader* shader_ptr = nullptr;
        REQUIRE(Succeeded(g_dev->CompileAndCreateShaderFromFile(compilation_params,
            shader_desc, L"shaders/Test.hlsl", shader_ptr)));
        typed_shader.reset(shader_ptr);
    }

    BufferDesc default_buf_desc{};
    default_buf_desc.name = L"My buffer DEFAULT";
    default_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagCopyDst | kBufferUsageFlagShaderRWResource
        | kBufferFlagTyped;
    default_buf_desc.size = kMainBufSize;
    default_buf_desc.element_format = Format::kR32_Float;
    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> default_buffer{ buffer_ptr };

    REQUIRE(g_main_upload_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*g_main_upload_buffer,
        Range{0, kMainBufSize}, *default_buffer, 0)));
    REQUIRE(Succeeded(g_dev->BindRWBuffer(0, default_buffer.get())));
    REQUIRE(Succeeded(g_dev->DispatchComputeShader(*typed_shader, { 8, 1, 1 })));
    g_dev->ResetAllBindings();

    REQUIRE(g_main_readback_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBuffer(*default_buffer, *g_main_readback_buffer)));

    std::array<float, 8> dst_data;
    REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer,
        Range{0, dst_data.size() * sizeof(float)},
        dst_data.data())));

    for(size_t i = 0; i < dst_data.size(); ++i)
    {
        CHECK(dst_data[i] == main_src_data[i] * main_src_data[i] + 1.f);
    }
}

TEST_CASE("Structured buffer", "[gpu][buffer][hlsl]")
{
    std::unique_ptr<Shader> structured_shader;
    {
        ShaderCompilationParams compilation_params{};
        compilation_params.entry_point = L"Main_Structured";

        ShaderDesc shader_desc{};
        shader_desc.name = L"Structured shader";

        Shader* shader_ptr = nullptr;
        REQUIRE(Succeeded(g_dev->CompileAndCreateShaderFromFile(compilation_params,
            shader_desc, L"shaders/Test.hlsl", shader_ptr)));
        structured_shader.reset(shader_ptr);
    }

    BufferDesc default_buf_desc{};
    default_buf_desc.name = L"My buffer DEFAULT";
    default_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagCopyDst | kBufferFlagStructured
        | kBufferUsageFlagShaderRWResource;
    default_buf_desc.size = kMainBufSize;
    default_buf_desc.structure_size = sizeof(float);
    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> default_buffer{ buffer_ptr };

    REQUIRE(g_main_upload_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*g_main_upload_buffer,
        Range{0, kMainBufSize}, *default_buffer, 0)));
    REQUIRE(Succeeded(g_dev->BindRWBuffer(1, default_buffer.get())));
    REQUIRE(Succeeded(g_dev->DispatchComputeShader(*structured_shader, { 8, 1, 1 })));
    g_dev->ResetAllBindings();
    REQUIRE(g_main_readback_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBuffer(*default_buffer, *g_main_readback_buffer)));

    std::array<float, 8> dst_data;
    REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer,
        Range{0, dst_data.size() * sizeof(float)}, dst_data.data())));

    for(size_t i = 0; i < dst_data.size(); ++i)
    {
        CHECK(dst_data[i] == main_src_data[i] * main_src_data[i] + 1.f);
    }
}

TEST_CASE("ByteAddress buffer", "[gpu][buffer][hlsl]")
{
    std::unique_ptr<Shader> byte_address_shader;
    {
        ShaderCompilationParams compilation_params{};
        compilation_params.entry_point = L"Main_ByteAddress";

        ShaderDesc shader_desc{};
        shader_desc.name = L"ByteAddress shader";

        Shader* shader_ptr = nullptr;
        REQUIRE(Succeeded(g_dev->CompileAndCreateShaderFromFile(compilation_params,
            shader_desc, L"shaders/Test.hlsl", shader_ptr)));
        byte_address_shader.reset(shader_ptr);
    }

    BufferDesc default_buf_desc{};
    default_buf_desc.name = L"My buffer DEFAULT";
    default_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagCopyDst | kBufferFlagByteAddress
        | kBufferUsageFlagShaderRWResource;
    default_buf_desc.size = kMainBufSize;

    // Additionally test adding this flag next to ShaderRWResource.
    default_buf_desc.flags |= kBufferUsageFlagShaderResource;

    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> default_buffer{ buffer_ptr };

    REQUIRE(g_main_upload_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*g_main_upload_buffer,
        Range{0, kMainBufSize}, *default_buffer, 0)));
    REQUIRE(Succeeded(g_dev->BindRWBuffer(2, default_buffer.get())));
    REQUIRE(Succeeded(g_dev->DispatchComputeShader(*byte_address_shader, { 8, 1, 1 })));
    g_dev->ResetAllBindings();
    REQUIRE(g_main_readback_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBuffer(*default_buffer, *g_main_readback_buffer)));

    std::array<float, 8> dst_data;
    REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer,
        Range{0, dst_data.size() * sizeof(float)}, dst_data.data())));

    for(size_t i = 0; i < dst_data.size(); ++i)
    {
        CHECK(dst_data[i] == main_src_data[i] * main_src_data[i] + 1.f);
    }
}

TEST_CASE("ClearBufferToUintValues", "[gpu][buffer][clear]")
{
    BufferDesc typed_buf_desc{};
    typed_buf_desc.name = L"My buffer Typed";
    typed_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagShaderRWResource | kBufferFlagTyped;
    typed_buf_desc.size = 256;
    typed_buf_desc.element_format = Format::kR16G16B16A16_Sint;
    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(typed_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> typed_buffer{ buffer_ptr };

    REQUIRE(Succeeded(g_dev->ClearBufferToUintValues(*typed_buffer, UintVec4{0, 666, 0xFF, 0x7FFF})));
    REQUIRE(g_main_readback_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*typed_buffer, Range{0, typed_buf_desc.size},
        *g_main_readback_buffer, 0)));

    {
        std::array<int16_t, 4> dst_data;
        REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer,
            Range{16, dst_data.size() * sizeof(dst_data[0])}, dst_data.data())));
        std::array<int16_t, 4> expected = {0, 666, 0xFF, 0x7FFF};
        REQUIRE(memcmp(dst_data.data(), expected.data(), dst_data.size() * sizeof(dst_data[0])) == 0);
    }

    BufferDesc byte_address_buf_desc{};
    byte_address_buf_desc.name = L"My buffer Byte address";
    byte_address_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagShaderRWResource | kBufferFlagByteAddress;
    byte_address_buf_desc.size = 256;
    REQUIRE(Succeeded(g_dev->CreateBuffer(byte_address_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> byte_address_buffer{ buffer_ptr };

    REQUIRE(Succeeded(g_dev->ClearBufferToUintValues(*byte_address_buffer,
        UintVec4{0xAA112233u, 0xBB112233u, 0xCC112233u, 0xDD112233u})));
    REQUIRE(g_main_readback_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*byte_address_buffer, Range{0, byte_address_buf_desc.size},
        *g_main_readback_buffer, 0)));

    {
        std::array<uint32_t, 4> dst_data;
        REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer,
            Range{16, dst_data.size() * sizeof(dst_data[0])}, dst_data.data())));
        std::array<uint32_t, 4> expected =
        {0xAA112233u, 0xAA112233u, 0xAA112233u, 0xAA112233u};
        CHECK(memcmp(dst_data.data(), expected.data(), dst_data.size() * sizeof(dst_data[0])) == 0);
    }
}

TEST_CASE("ClearBufferToFloatValues", "[gpu][buffer][clear]")
{
    BufferDesc default_buf_desc{};
    default_buf_desc.name = L"My buffer DEFAULT";
    default_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagShaderRWResource | kBufferFlagTyped;
    default_buf_desc.size = 256;
    default_buf_desc.element_format = Format::kR16G16B16A16_Float;
    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> default_buffer{ buffer_ptr };

    REQUIRE(Succeeded(g_dev->ClearBufferToFloatValues(*default_buffer,
        FloatVec4{1.f, -1.f, 0.5f, 2.f})));
    REQUIRE(g_main_readback_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*default_buffer, Range{0, default_buf_desc.size},
        *g_main_readback_buffer, 0)));

    std::array<uint16_t, 4> dst_data;
    REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer,
        Range{16, dst_data.size() * sizeof(dst_data[0])},
        dst_data.data())));
    std::array<uint16_t, 4> expected = {0x3C00, 0xBC00,0x3800, 0x4000};
    CHECK(memcmp(dst_data.data(), expected.data(), dst_data.size() * sizeof(dst_data[0])) == 0);
}

// Using UPLOAD buffer as both copy source and GPU read, to check if barriers are done correctly.
TEST_CASE("UPLOAD as copy source and GPU read", "[gpu][buffer][hlsl]")
{
    std::unique_ptr<Shader> copy_squared_typed_shader;
    {
        ShaderCompilationParams compilation_params{};
        compilation_params.entry_point = L"Main";

        ShaderDesc shader_desc{};
        shader_desc.name = L"ByteAddress shader";

        Shader* shader_ptr = nullptr;
        REQUIRE(Succeeded(g_dev->CompileAndCreateShaderFromFile(compilation_params,
            shader_desc, L"shaders/CopySquaredTyped.hlsl", shader_ptr)));
        copy_squared_typed_shader.reset(shader_ptr);
    }

    constexpr size_t kSectionCount = 8;
    constexpr size_t kNumbersPerSection = 1024;
    constexpr size_t kSectionSize = kNumbersPerSection * sizeof(float);
    constexpr size_t kBufSize = kSectionSize * kSectionCount;

    BufferDesc upload_buf_desc{};
    upload_buf_desc.name = L"My buffer UPLOAD";
    upload_buf_desc.flags = kBufferUsageFlagCpuSequentialWrite
        | kBufferUsageFlagCopySrc
        | kBufferUsageFlagShaderResource
        | kBufferFlagTyped;
    upload_buf_desc.size = kBufSize;
    upload_buf_desc.element_format = Format::kR32_Float;
    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(upload_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> my_upload_buffer{ buffer_ptr };
    REQUIRE(my_upload_buffer->GetResource() != nullptr);

    BufferDesc default_buf_desc{};
    default_buf_desc.name = L"My buffer DEFAULT";
    default_buf_desc.flags = kBufferUsageFlagCopyDst
        | kBufferUsageFlagCopySrc
        | kBufferUsageFlagShaderRWResource
        | kBufferFlagTyped;
    default_buf_desc.size = kBufSize;
    default_buf_desc.element_format = Format::kR32_Float;
    REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> my_default_buffer{ buffer_ptr };
    REQUIRE(my_default_buffer->GetResource() != nullptr);

    float* mapped_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->MapBuffer(*my_upload_buffer, kFullRange, kBufferUsageFlagCpuSequentialWrite,
        (void*&)mapped_ptr)));
    for(size_t i = 0; i < kSectionCount * kNumbersPerSection; ++i)
    {
        mapped_ptr[i] = static_cast<float>(i + 2);
    }
    g_dev->UnmapBuffer(*my_upload_buffer);

    // The buffer is divided into 8 sections.
    // 1. Copy region [0...5)
    // 2. Process region [1...6) using a shader.
    // 3. Copy region [2...7)
    // 4. Process region [3...8) using a shader.

    constexpr UintVec3 group_count = {5 * kNumbersPerSection, 1, 1};

    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*my_upload_buffer, Range{0 * kSectionSize, 5 * kSectionSize},
        *my_default_buffer, 0 * kSectionSize)));

    REQUIRE(Succeeded(g_dev->BindBuffer(0, my_upload_buffer.get(), Range{1 * kSectionSize, 5 * kSectionSize})));
    REQUIRE(Succeeded(g_dev->BindRWBuffer(0, my_default_buffer.get(), Range{1 * kSectionSize, 5 * kSectionSize})));
    REQUIRE(Succeeded(g_dev->DispatchComputeShader(*copy_squared_typed_shader, group_count)));

    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*my_upload_buffer, Range{2 * kSectionSize, 5 * kSectionSize},
        *my_default_buffer, 2 * kSectionSize)));

    REQUIRE(Succeeded(g_dev->BindBuffer(0, my_upload_buffer.get(), Range{3 * kSectionSize, 5 * kSectionSize})));
    REQUIRE(Succeeded(g_dev->BindRWBuffer(0, my_default_buffer.get(), Range{3 * kSectionSize, 5 * kSectionSize})));
    REQUIRE(Succeeded(g_dev->DispatchComputeShader(*copy_squared_typed_shader, group_count)));

    g_dev->ResetAllBindings();

    REQUIRE(g_main_readback_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*my_default_buffer, Range{0, kBufSize},
        *g_main_readback_buffer, 0)));

    std::array<float, kSectionCount * kNumbersPerSection> dst_data;
    REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer, Range{0, kBufSize},
        dst_data.data())));

    enum class SectionResult { kUnknown = 0, kCopy, kSquared, kZero, kError };
    std::array<SectionResult, kSectionCount> section_results = {};
    for(size_t section_index = 0; section_index < kSectionCount; ++section_index)
    {
        for(size_t index_in_section = 0
            ; index_in_section < kNumbersPerSection && section_results[section_index] != SectionResult::kError
            ; ++index_in_section)
        {
            const size_t index_in_buffer = section_index * kNumbersPerSection + index_in_section;
            const float index_f = float(index_in_buffer + 2);
            const bool is_zero = asuint(dst_data[index_in_buffer]) == 0;
            const bool is_copy = dst_data[index_in_buffer] == index_f;
            const bool is_squared = dst_data[index_in_buffer] == index_f * index_f;
            switch(section_results[section_index])
            {
            case SectionResult::kUnknown:
                if(is_zero)
                    section_results[section_index] = SectionResult::kZero;
                else if(is_copy)
                    section_results[section_index] = SectionResult::kCopy;
                else if(is_squared)
                    section_results[section_index] = SectionResult::kSquared;
                else
                    section_results[section_index] = SectionResult::kError;
                break;
            case SectionResult::kZero:
                if(!is_zero)
                    section_results[section_index] = SectionResult::kError;
                break;
            case SectionResult::kCopy:
                if(!is_copy)
                    section_results[section_index] = SectionResult::kError;
                break;
            case SectionResult::kSquared:
                if(!is_squared)
                    section_results[section_index] = SectionResult::kError;
                break;
            }
        }
    }
    CHECK(section_results[0] == SectionResult::kCopy);
    CHECK(section_results[1] == SectionResult::kSquared);
    CHECK(section_results[2] == SectionResult::kCopy);
    CHECK(section_results[3] == SectionResult::kSquared);
    CHECK(section_results[4] == SectionResult::kSquared);
    CHECK(section_results[5] == SectionResult::kSquared);
    CHECK(section_results[6] == SectionResult::kSquared);
    CHECK(section_results[7] == SectionResult::kSquared);
}

TEST_CASE("ClearBufferToUintValues with a sub-range of elements", "[gpu][buffer][clear]")
{
    constexpr size_t kElementCount = 256;

    BufferDesc default_buf_desc{};
    default_buf_desc.name = L"My buffer DEFAULT";
    default_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagShaderRWResource | kBufferFlagTyped;
    default_buf_desc.size = kElementCount * sizeof(uint32_t);
    default_buf_desc.element_format = Format::kR8G8B8A8_Uint;
    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> default_buffer{ buffer_ptr };

    // Clear entire buffer.
    REQUIRE(Succeeded(g_dev->ClearBufferToUintValues(*default_buffer, UintVec4{1, 2, 3, 4})));
    // Clear sub-range.
    REQUIRE(Succeeded(g_dev->ClearBufferToUintValues(*default_buffer, UintVec4{5, 6, 7, 8}, Range{64, 64})));

    REQUIRE(g_main_readback_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*default_buffer, Range{0, default_buf_desc.size},
        *g_main_readback_buffer, 0)));

    std::array<uint32_t, kElementCount> dst_data;
    REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer, Range{0, kElementCount * sizeof(uint32_t)},
        dst_data.data())));

    std::array<uint32_t, kElementCount> expected;
    for(size_t i = 0; i < kElementCount; ++i)
    {
        expected[i] = (i >= 64 && i < 128) ? 0x08070605u : 0x04030201u;
    }

    CHECK(memcmp(dst_data.data(), expected.data(), kElementCount * sizeof(uint32_t)) == 0);
}

TEST_CASE("Mapping of a sub-range", "[gpu][buffer]")
{
    BufferDesc upload_buf_desc{};
    upload_buf_desc.name = L"My buffer UPLOAD";
    upload_buf_desc.flags = kBufferUsageFlagCpuSequentialWrite | kBufferUsageFlagCopySrc;
    upload_buf_desc.size = 64 * kKilobyte;
    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(upload_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> my_upload_buffer{ buffer_ptr };

    // Map entire 64 KB of my_upload_buffer, fill it with zeros.
    float* mapped_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->MapBuffer(*my_upload_buffer, Range{0, 64 * kKilobyte},
        kBufferUsageFlagCpuSequentialWrite, (void*&)mapped_ptr)));
    ZeroMemory(mapped_ptr, 64 * kKilobyte);
    g_dev->UnmapBuffer(*my_upload_buffer);

    // Additionally test CopyValueToBuffer.
    REQUIRE(Succeeded(g_dev->WriteValueToBuffer(0u, *my_upload_buffer, 256)));

    // Map 2nd kilobyte, fill it with ones.
    REQUIRE(Succeeded(g_dev->MapBuffer(*my_upload_buffer, Range{1 * kKilobyte, kKilobyte},
        kBufferUsageFlagCpuSequentialWrite, (void*&)mapped_ptr)));
    memset(mapped_ptr, 0xFF, kKilobyte);
    g_dev->UnmapBuffer(*my_upload_buffer);

    // Copy 64 KB from upload buffer to readback buffer.
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*my_upload_buffer, Range{0, 64 * kKilobyte},
        *g_main_readback_buffer, 0)));

    // Map readback buffer at 512 B and validate.
    REQUIRE(Succeeded(g_dev->MapBuffer(*g_main_readback_buffer, Range{512, kKilobyte},
        kBufferUsageFlagCpuRead, (void*&)mapped_ptr)));
    std::array<uint8_t, kKilobyte> dst_data;
    memcpy(dst_data.data(), mapped_ptr, kKilobyte);
    g_dev->UnmapBuffer(*g_main_readback_buffer);

    // Additionally test CopyBufferToValue.
    uint32_t dst_val = 0;
    REQUIRE(Succeeded(g_dev->ReadBufferToValue(*g_main_readback_buffer, 1024, dst_val)));
    CHECK(dst_val == 0xFFFFFFFF);

    std::array<uint8_t, kKilobyte> expected_data;
    size_t i;
    for(i = 0; i < 512; ++i)
        expected_data[i] = 0;
    for(; i < 1024; ++i)
        expected_data[i] = 0xFF;
    CHECK(memcmp(dst_data.data(), expected_data.data(), kKilobyte) == 0);
}

TEST_CASE("Constant buffer", "[gpu][buffer]")
{
    std::unique_ptr<Shader> shader;
    {
        ShaderCompilationParams compilation_params{};
        compilation_params.character_encoding = kCharacterEncodingUtf16;
        compilation_params.entry_point = L"Main";

        ShaderDesc shader_desc{};
        shader_desc.name = L"Constant buffer shader";

        Shader* shader_ptr = nullptr;
        REQUIRE(Succeeded(g_dev->CompileAndCreateShaderFromFile(compilation_params,
            shader_desc, L"shaders/constant_buffer.hlsl", shader_ptr)));
        shader.reset(shader_ptr);
    }

    struct MyConstants
    {
        FloatVec4 floats;
        UintVec4 uints;
    };
    MyConstants const_values = {
        {1.f, 2.f, 3.f, 4.f},
        {0xFFFFFFFFu, 0xAAAAAAAAu, 0x55555555u, 0x00000000u}};
    constexpr size_t kConstBufSize = std::max<size_t>(sizeof(MyConstants), 256);

    // Intentionally creating is as none of Typed, Structured or ByteAddress buffer.
    BufferDesc const_buf_desc = {
        L"My constant buffer", // name
        kBufferUsageFlagCpuSequentialWrite | kBufferUsageFlagShaderConstant | kBufferUsageFlagCopySrc, // flags
        kConstBufSize // size
    };
    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBufferFromMemory(const_buf_desc,
        ConstDataSpan{&const_values, sizeof(MyConstants)}, buffer_ptr)));
    std::unique_ptr<Buffer> const_buf{buffer_ptr};

    BufferDesc struct_buf_desc = {
        L"My structured buffer", // name
        kBufferUsageFlagShaderRWResource | kBufferFlagStructured | kBufferUsageFlagCopySrc, // flags
        kConstBufSize, // size
        Format::kUnknown, // element_format
        sizeof(MyConstants) // structure_size
    };
    REQUIRE(Succeeded(g_dev->CreateBuffer(struct_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> struct_buf{buffer_ptr};

    // Bind the buffers and dispatch the shader.
    REQUIRE(Succeeded(g_dev->BindConstantBuffer(0, const_buf.get())));
    REQUIRE(Succeeded(g_dev->BindRWBuffer(0, struct_buf.get())));
    REQUIRE(Succeeded(g_dev->DispatchComputeShader(*shader, { 1, 1, 1 })));
    g_dev->ResetAllBindings();
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*struct_buf, Range{0, struct_buf_desc.size},
        *g_main_readback_buffer, 0)));
    MyConstants dst_data;
    REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer,
        Range{0, sizeof(MyConstants)}, &dst_data)));
    CHECK(memcmp(&dst_data, &const_values, sizeof(MyConstants)) == 0); // HEY COPILOT, THIS FAILS!!! dst_data is all zeros.
}

// Test WriteMemoryToBuffer with a GPU read-write buffer, so that WriteBufferImmediate is used.
TEST_CASE("WriteMemoryToBuffer with a GPU read-write buffer", "[gpu][buffer]")
{
    constexpr size_t kElementCount = 64;
    using ElementType = uint32_t;
    BufferDesc buf_desc = {
        L"My GPU buffer", // name
        kBufferUsageFlagShaderRWResource | kBufferUsageFlagCpuSequentialWrite | kBufferUsageFlagCopySrc, // flags
        kElementCount * sizeof(ElementType), // size
    };
    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> buf{buffer_ptr};

    std::array<ElementType, kElementCount> src_data;
    for(uint32_t i = 0; i < kElementCount; ++i)
        src_data[i] = ElementType(i * 3 + 1);
    REQUIRE(Succeeded(g_dev->WriteMemoryToBuffer(
        ConstDataSpan{src_data.data(), buf_desc.size}, *buf, 0)));

    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*buf, Range{0, buf_desc.size},
        *g_main_readback_buffer, 0)));
    std::array<ElementType, kElementCount> dst_data;
    REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer,
        Range{0, buf_desc.size}, dst_data.data())));
    CHECK(memcmp(dst_data.data(), src_data.data(), buf_desc.size) == 0);
}

TEST_CASE("Shader compilation params", "[gpu][buffer][hlsl]")
{
    std::unique_ptr<Shader> shader;
    {
        const wchar_t* macros[] = { L"ENABLING_MACRO", L"4" };

        ShaderCompilationParams compilation_params{};
        compilation_params.flags = kShaderCompilationFlagEnableIeeeStrictness
            //| kShaderCompilationFlagAvoidFlowControl // Doesn't work, I don't know why.
            //| kShaderCompilationFlagDenormPreserve // Doesn't work, I don't know why.
            | kShaderCompilationFlagNoFiniteMathOnly
            | kShaderCompilationFlagTreatWarningsAsErrors;
        compilation_params.entry_point = L"Main_Conditional";
        compilation_params.optimization_level = kShaderOptimizationDisabled;
        compilation_params.shader_model = kShaderModel6_1;
        compilation_params.macro_defines = { macros, _countof(macros) };

        ShaderDesc shader_desc{};
        shader_desc.name = L"Main_Conditional shader";

        Shader* shader_ptr = nullptr;
        REQUIRE(Succeeded(g_dev->CompileAndCreateShaderFromFile(compilation_params,
            shader_desc, L"shaders/Test.hlsl", shader_ptr)));
        shader.reset(shader_ptr);
    }

    BufferDesc default_buf_desc{};
    default_buf_desc.name = L"My buffer DEFAULT";
    default_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferFlagByteAddress
        | kBufferUsageFlagShaderRWResource;
    default_buf_desc.size = 32 * sizeof(uint32_t);

    Buffer* buffer_ptr = nullptr;
    REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
    std::unique_ptr<Buffer> default_buffer{ buffer_ptr };

    REQUIRE(Succeeded(g_dev->BindRWBuffer(2, default_buffer.get())));
    // (1, 1, 1) group of (32, 1, 1) threads = 32 uint32_t values to fill.
    REQUIRE(Succeeded(g_dev->DispatchComputeShader(*shader, { 1, 1, 1 })));
    g_dev->ResetAllBindings();
    REQUIRE(g_main_readback_buffer != nullptr);
    REQUIRE(Succeeded(g_dev->CopyBufferRegion(*default_buffer, Range{ 0, default_buf_desc.size },
        *g_main_readback_buffer, 0)));

    std::array<uint32_t, 32> dst_data, expected_data;
    REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*g_main_readback_buffer,
        Range{ 0, default_buf_desc.size }, dst_data.data())));
    for(uint32_t i = 0; i < 32; ++i)
        expected_data[i] = i * i + 1;
    CHECK(memcmp(dst_data.data(), expected_data.data(), default_buf_desc.size) == 0);
}
