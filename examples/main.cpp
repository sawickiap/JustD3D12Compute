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

StaticShaderFromFile typed_shader{
    ShaderDesc{ L"Typed shader" }, L"Test_Typed.dxil" };
StaticShaderFromFile structured_shader{
    ShaderDesc{ L"Structured shader" }, L"Test_Structured.dxil" };
StaticShaderFromFile byte_address_shader{
    ShaderDesc{ L"ByteAddress shader" }, L"Test_ByteAddress.dxil" };
StaticShaderFromFile copy_squared_typed_shader{
    ShaderDesc{ L"copy_squared_typed" }, L"CopySquaredTyped.dxil" };

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

Environment* g_env;
Device* g_dev;

#define REQUIRE assert
#define CHECK assert

int main(int argc, char** argv)
{
    std::unique_ptr<Environment> env;
    std::unique_ptr<Device> dev;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    assert(Succeeded(CreateEnvironment(g_env)));
    env.reset(g_env);

    DeviceDesc device_desc;
    device_desc.name = L"My device";
    assert(Succeeded(env->CreateDevice(device_desc, g_dev)));
    dev.reset(g_dev);

    {
        BufferDesc default_buf_desc{};
        default_buf_desc.name = L"My buffer DEFAULT";
        default_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagCopyDst | kBufferUsageFlagShaderRWResource
            | kBufferFlagTyped;
        default_buf_desc.size = kMainBufSize;
        default_buf_desc.element_format = Format::kR32_Float;
        Buffer* buffer_ptr = nullptr;
        REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
        std::unique_ptr<Buffer> default_buffer{ buffer_ptr };

        REQUIRE(main_upload_buffer.GetBuffer() != nullptr);
        REQUIRE(Succeeded(g_dev->CopyBufferRegion(*main_upload_buffer.GetBuffer(), Range{0, kMainBufSize}, *default_buffer, 0)));
        REQUIRE(Succeeded(g_dev->BindRWBuffer(0, default_buffer.get())));
        REQUIRE(typed_shader.GetShader() != nullptr);
        REQUIRE(Succeeded(g_dev->DispatchComputeShader(*typed_shader.GetShader(), { 8, 1, 1 })));
        g_dev->ResetAllBindings();
        REQUIRE(main_readback_buffer.GetBuffer() != nullptr);
        REQUIRE(Succeeded(g_dev->CopyBuffer(*default_buffer, *main_readback_buffer.GetBuffer())));

        REQUIRE(Succeeded(g_dev->SubmitPendingCommands()));
        REQUIRE(Succeeded(g_dev->WaitForGPU()));

        std::array<float, 8> dst_data;
        REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*main_readback_buffer.GetBuffer(), Range{0, dst_data.size() * sizeof(float)},
            dst_data.data())));

        for(size_t i = 0; i < dst_data.size(); ++i)
        {
            CHECK(dst_data[i] == main_src_data[i] * main_src_data[i]);
        }
    }

    {
        BufferDesc default_buf_desc{};
        default_buf_desc.name = L"My buffer DEFAULT";
        default_buf_desc.flags = kBufferUsageFlagCopySrc | kBufferUsageFlagCopyDst | kBufferFlagStructured
            | kBufferUsageFlagShaderRWResource;
        default_buf_desc.size = kMainBufSize;
        default_buf_desc.structure_size = sizeof(float);
        Buffer* buffer_ptr = nullptr;
        REQUIRE(Succeeded(g_dev->CreateBuffer(default_buf_desc, buffer_ptr)));
        std::unique_ptr<Buffer> default_buffer{ buffer_ptr };

        REQUIRE(main_upload_buffer.GetBuffer() != nullptr);
        REQUIRE(Succeeded(g_dev->CopyBufferRegion(*main_upload_buffer.GetBuffer(), Range{0, kMainBufSize}, *default_buffer, 0)));
        REQUIRE(Succeeded(g_dev->BindRWBuffer(1, default_buffer.get())));
        REQUIRE(structured_shader.GetShader() != nullptr);
        REQUIRE(Succeeded(g_dev->DispatchComputeShader(*structured_shader.GetShader(), { 8, 1, 1 })));
        g_dev->ResetAllBindings();
        REQUIRE(main_readback_buffer.GetBuffer() != nullptr);
        REQUIRE(Succeeded(g_dev->CopyBuffer(*default_buffer, *main_readback_buffer.GetBuffer())));

        REQUIRE(Succeeded(g_dev->SubmitPendingCommands()));
        REQUIRE(Succeeded(g_dev->WaitForGPU()));

        std::array<float, 8> dst_data;
        REQUIRE(Succeeded(g_dev->ReadBufferToMemory(*main_readback_buffer.GetBuffer(), Range{0,
            dst_data.size() * sizeof(float)}, dst_data.data())));

        for(size_t i = 0; i < dst_data.size(); ++i)
        {
            CHECK(dst_data[i] == main_src_data[i] * main_src_data[i]);
        }
    }

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
            CHECK(dst_data[i] == main_src_data[i] * main_src_data[i]);
        }
    }
}
