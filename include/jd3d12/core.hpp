// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#pragma once

#include <jd3d12/utils.hpp>
#include <jd3d12/types.hpp>

namespace jd3d12
{

class BufferImpl;
class ShaderImpl;
class ShaderCompilationResultImpl;
class DeviceImpl;
class EnvironmentImpl;

class Device;
class Environment;

enum BufferFlags : uint32_t
{
    kBufferUsageMaskCpu                = 0x00000007u,
    kBufferUsageFlagCpuRead            = 0x00000001u,
    kBufferUsageFlagCpuSequentialWrite = 0x00000002u,
    //kBufferUsageFlagCpuRandomAccess    = 0x00000004u,

    kBufferUsageMaskCopy     = 0x00000018u,
    kBufferUsageFlagCopySrc  = 0x00000008u,
    kBufferUsageFlagCopyDst  = 0x00000010u,

    kBufferUsageMaskShader           = 0x000000E0u,
    kBufferUsageFlagShaderConstant   = 0x00000020u,
    kBufferUsageFlagShaderResource   = 0x00000040u,
    kBufferUsageFlagShaderRWResource = 0x00000080u,

    kBufferFlagTyped       = 0x00000100u,
    kBufferFlagStructured  = 0x00000200u,
    kBufferFlagByteAddress = 0x00000400u,
};

struct BufferDesc
{
    const wchar_t* name = nullptr;
    uint32_t flags = 0; // Use BufferFlags.
    size_t size = 0;
    Format element_format = Format::kUnknown;
    size_t structure_size = 0;
};

class Buffer
{
public:
    ~Buffer();
    BufferImpl* GetImpl() const noexcept { return impl_; }
    Device* GetDevice() const noexcept;
    const wchar_t* GetName() const noexcept;
    size_t GetSize() const noexcept;
    uint32_t GetFlags() const noexcept;
    Format GetElementFormat() const noexcept;
    size_t GetStructureSize() const noexcept;
    /** \brief Returns the size of a single buffer element in bytes.

    - For typed buffer, returns FormatDesc::bits_per_element / 8 of its BufferDesc::element_format.
    - For structured buffer, returns BufferDesc::structure_size.
    - For byte address buffer, returns `sizeof(uint32_t)` = 4.
    - If the element size is unknown, returns 0.
    */
    size_t GetElementSize() const noexcept;
    /// Returns `ID3D12Resource*`.
    void* GetResource() const noexcept;

private:
    BufferImpl* impl_ = nullptr;

    Buffer();

    friend class DeviceImpl;
    JD3D12_NO_COPY_NO_MOVE_CLASS(Buffer)
};

struct ShaderDesc
{
    const wchar_t* name = nullptr;
};

class Shader
{
public:
    ~Shader();
    ShaderImpl* GetImpl() const noexcept { return impl_; }
    Device* GetDevice() const noexcept;
    const wchar_t* GetName() const noexcept;
    /// Returns ID3D12PipelineState* object.
    void* GetNativePipelineState() const noexcept;

private:
    ShaderImpl* impl_ = nullptr;

    Shader();

    friend class DeviceImpl;
    JD3D12_NO_COPY_NO_MOVE_CLASS(Shader)
};

enum ShaderCompilationFlags
{
    /// Passed to DXC as `-denorm preserve`.
    kShaderCompilationFlagDenormPreserve = 0x00000001u,
    /// Passed to DXC as `-denorm ftz`.
    kShaderCompilationFlagDenormFlushToZero = 0x00000002u,
    /// Enables 16-bit types support. Passed to DXC as `-enable-16bit-types`.
    kShaderCompilationFlagEnable16BitTypes = 0x00000004u,
    /// Passed to DXC as `-Gfa`.
    kShaderCompilationFlagAvoidFlowControl = 0x00000008u,
    /// Passed to DXC as `-Gfp`.
    kShaderCompilationFlagPreferFlowControl = 0x00000010u,
    /// Force IEEE strictness. Passed to DXC as `-Gis`.
    kShaderCompilationFlagEnableIeeeStrictness = 0x00000020u,
    /// Passed to DXC as `-no-warnings`.
    kShaderCompilationFlagSuppressWarnings = 0x00000040u,
    /// Passed to DXC as `-WX`.
    kShaderCompilationFlagTreatWarningsAsErrors = 0x00000080u,
    /// Pack matrices in column-major order. Passed to DXC as `-Zpc`.
    /// TODO Which one is the default?
    kShaderCompilationFlagPackMatricesColumnMajor = 0x00000100u,
    /// Pack matrices in row-major order. Passed to DXC as `-Zpr`.
    kShaderCompilationFlagPackMatricesRowMajor = 0x00000200u,
    /// Allow optimizations for floating-point arithmetic that assume that arguments and results are not NaNs or +-Infs.
    /// Passed to DXC as `-ffinite-math-only`.
    /// TODO Which one is the default?
    kShaderCompilationFlagFiniteMathOnly = 0x00000400u,
    /// Disallow optimizations for floating-point arithmetic that assume that arguments and results are not NaNs or +-Infs.
    /// Passed to DXC as `-fno-finite-math-only`.
    kShaderCompilationFlagNoFiniteMathOnly = 0x00000800u,
};

enum HlslVersion
{
    kHlslVersion2016 = 2016,
    kHlslVersion2017 = 2017,
    kHlslVersion2018 = 2018,
    kHlslVersion2021 = 2021,
};

enum ShaderOptimizationLevel
{
    kShaderOptimizationDisabled = -1,
    kShaderOptimizationLevel0 = 0,
    kShaderOptimizationLevel1 = 1,
    kShaderOptimizationLevel2 = 2,
    kShaderOptimizationLevel3 = 3,
};

enum ShaderModel
{
    kShaderModel6_0 = 0x0600,
    kShaderModel6_1 = 0x0601,
    kShaderModel6_2 = 0x0602,
    kShaderModel6_3 = 0x0603,
    kShaderModel6_4 = 0x0604,
    kShaderModel6_5 = 0x0605,
    kShaderModel6_6 = 0x0606,
    kShaderModel6_7 = 0x0607,
    kShaderModel6_8 = 0x0608,
    kShaderModel6_9 = 0x0609,
};

struct ShaderCompilationParams
{
    /// Use #ShaderCompilationFlags.
    uint32_t flags = 0;
    /** Name of the main function within the HLSL code that should be the entry point of the shader.

    Passed to DXC as `-E` parameter.
    */
    const wchar_t* entry_point = nullptr;
    /** HLSL language version. Use #HlslVersion enum. */
    uint32_t hlsl_version = kHlslVersion2021;
    /** Shader model version. Use #ShaderModel enum. */
    uint32_t shader_model = kShaderModel6_0;
    /** Optimization level. Use #ShaderOptimizationLevel enum. */
    int32_t optimization_level = kShaderOptimizationLevel3;
    /** Additional arguments passed directly to DXC. */
    ArraySpan<const wchar_t*> additional_dxc_args = { nullptr, 0 };
};

/** \brief Stores the result of a shader compilation from HLSL source code to bytecode.

Successful creation of this object doesn't necessarily mean the compilation succeeded.
In case of failed compilation, it stores error messages.
Call GetResult() to check whether the compilation was successful.
*/
class ShaderCompilationResult
{
public:
    ~ShaderCompilationResult();
    ShaderCompilationResultImpl* GetImpl() const noexcept { return impl_; }
    Environment* GetEnvironment() const noexcept;

    /** \brief Returns the result of the shader compilation (#kOK in case of success).
    */
    Result GetResult();
    /** \brief Returns compilation errors and warnings as a null-terminated string, possibly multi-line,
    in UTF-8 encoding.

    If there are no errors or warnings, an empty string is returned. It never returns null.

    Returned memory is owned by this object. You shoudln't free it.
    */
    const char* GetErrorsAndWarnings();
    /** \brief Returns the compiled shader bytecode.

    If the compilation failed, an empty data span is returned with `data` = null.

    Returned memory is owned by this object. You shoudln't free it.
    */
    ConstDataSpan GetBytecode();

private:
    ShaderCompilationResultImpl* impl_ = nullptr;

    ShaderCompilationResult();

    friend class EnvironmentImpl;
    JD3D12_NO_COPY_NO_MOVE_CLASS(ShaderCompilationResult)
};

enum DeviceFlags
{
    kDeviceFlagDisableGpuTimeout  = 0x1,
    kDeviceFlagDisableNameSetting = 0x2,
    kDeviceFlagDisableNameStoring = 0x3,
};

struct DeviceDesc
{
    const wchar_t* name = nullptr;
    uint32_t flags = 0; // Use DeviceFlags.
};

enum CommandFlags : uint32_t
{
    kCommandFlagDontWait = 0x1,
};

class Device
{
public:
    ~Device();
    DeviceImpl* GetImpl() const noexcept { return impl_; }
    Environment* GetEnvironment() const noexcept;
    void* GetNativeDevice() const noexcept;

    Result CreateBuffer(const BufferDesc& desc, Buffer*& out_buffer);
    /** \brief Creates a buffer and initializes it with data from memory.

    \param initial_data Optional, can be null.
    \param initial_data_size Optional, can be 0. If not 0, initial_data must not be null and this size
    must be not greater than the buffer size.
    */
    Result CreateBufferFromMemory(const BufferDesc& desc, ConstDataSpan initial_data,
        Buffer*& out_buffer);
    Result CreateBufferFromFile(const BufferDesc& desc, const wchar_t* initial_data_file_path,
        Buffer*& out_buffer);

    Result CreateShaderFromMemory(const ShaderDesc& desc, ConstDataSpan bytecode,
        Shader*& out_shader);
    Result CreateShaderFromFile(const ShaderDesc& desc, const wchar_t* bytecode_file_path,
        Shader*& out_shader);

    /** \brief Maps a buffer, returning a CPU pointer for reading or writing its data.

    `cpu_usage_flag` must be exactly one of `kBufferUsageFlagCpu*`.

    Returned `out_data_ptr` points to the beginning of the requested range, NOT the beginning of the entire buffer.
    */
    Result MapBuffer(Buffer& buf, Range byte_range, BufferFlags cpu_usage_flag, void*& out_data_ptr,
        uint32_t command_flags = 0);
    void UnmapBuffer(Buffer& buf);

    Result ReadBufferToMemory(Buffer& src_buf, Range src_byte_range, void* dst_memory,
        uint32_t command_flags = 0);
    /** \brief Writes data to a buffer.

    The buffer must be created with kBufferUsageFlagCpuSequentialWrite.

    Memory pointed by `src_buf` can be modified or freed immediately after this call, as the data is
    written immediately, an internal copy is made, or the function blocks until the write is finished.
    */
    Result WriteMemoryToBuffer(ConstDataSpan src_data, Buffer& dst_buf, size_t dst_byte_offset,
        uint32_t command_flags = 0);

    template<typename T>
    Result ReadBufferToValue(Buffer& src_buf, size_t src_byte_offset, T& out_val,
        uint32_t command_flags = 0)
    {
        return ReadBufferToMemory(src_buf, Range{ src_byte_offset, sizeof(out_val) }, &out_val, command_flags);
    }
    template<typename T>
    Result WriteValueToBuffer(const T& src_val, Buffer& dst_buf, size_t dst_byte_offset,
        uint32_t command_flags = 0)
    {
        return WriteMemoryToBuffer(ConstDataSpan{ &src_val, sizeof(src_val) }, dst_buf, dst_byte_offset, command_flags);
    }

    Result SubmitPendingCommands();
    Result WaitForGPU(uint32_t timeout_milliseconds = kTimeoutInfinite);

    Result CopyBuffer(Buffer& src_buf, Buffer& dst_buf);
    Result CopyBufferRegion(Buffer& src_buf, Range src_byte_range, Buffer& dst_buf, size_t dst_byte_offset);

    /** \brief Fills a buffer with given integer numeric values.

    Buffer must be created with kBufferUsageFlagShaderRWResource, as well as either kBufferFlagTyped or
    kBufferFlagByteAddress.

    Only a subset of formats are supported, following these rules:

    - When using Format::kR32G32B32A32_Uint, values are written as-is, as the type of `values` matches exactly.
    - If you specify `{1, 2, 3, 4}`, but the format has only 2 components, like Format::kR32G32_Uint,
      only `{1, 2}` is written repeatedly.
    - Values `{0, 0, 0, 0}` are allowed with any format.
    - When using non-zero values, the format must be `Uint`, and the values cannot exceed the maximum for
      that format, like `0xFF` for `8_Uint` or `0xFFFF` for `16_Uint`.
    - `Sint` formats are also supported, but the values must be non-negative. For example, for `16_Sint`,
      they must be between 0 and `0x7FFF`.
    - `32_Float` formats are also supported and the values are directly reinterpreted as 32-bit floats.
      For example, `0x3F800000u` becomes 1.0.
    - Byte address buffers are treated as typed buffers with Format::kR32_Uint, using only the first component.
    */
    Result ClearBufferToUintValues(Buffer& buf, const UintVec4& values, Range element_range = kFullRange);
    /** \brief Fills a buffer with given floating-point numeric values.

    Buffer must be created with kBufferUsageFlagShaderRWResource and kBufferFlagTyped.

    Only a subset of formats are supported, following these rules:

    - When using Format::kR32G32B32A32_Float, values are written as-is, as the type of `values` matches exactly.
    - If you specify `{1.0, 2.0, 3.0, 4.0}`, but the format has only 2 components, like Format::kR32G32_Float,
      only `{1.0, 2.0}` is written repeatedly.
    - For half-float formats `16_Float`, values are correctly converted to half-floats.
    - For normalized formats `Unorm`, values are correctly mapped from range `0.0...1.0` to the full range of the
      integer type. Values beyond `0.0...1.0` are clamped to the minimum/maximum.
    - For normalized formats `Snorm`, values are correctly mapped from range `-1.0...1.0` to the correct range of the
      integer type. Values beyond `-1.0...1.0` are clamped to the minimum/maximum.
    */
    Result ClearBufferToFloatValues(Buffer& buf, const FloatVec4& values, Range element_range = kFullRange);

    void ResetAllBindings();
    /** \brief Binds a buffer as a constant buffer to b# slot.

    There is no requirement for the buffer to be typed, structured, or byte address buffer.
    However, `byte_range.first` and `byte_range.count` must be aligned to 256 B.
    */
    Result BindConstantBuffer(uint32_t b_slot, Buffer* buf, Range byte_range = kFullRange);
    Result BindBuffer(uint32_t t_slot, Buffer* buf, Range byte_range = kFullRange);
    Result BindRWBuffer(uint32_t u_slot, Buffer* buf, Range byte_range = kFullRange);
    Result DispatchComputeShader(Shader& shader, const UintVec3& group_count);

private:
    DeviceImpl* impl_ = nullptr;

    Device();

    friend class EnvironmentImpl;
    JD3D12_NO_COPY_NO_MOVE_CLASS(Device)
};

/// Abstract base class for #StaticShaderFromMemory, #StaticShaderFromFile.
class StaticShader
{
public:
    StaticShader();
    StaticShader(const ShaderDesc& desc);
    virtual ~StaticShader() = 0;

    Shader* GetShader() const noexcept;

protected:
    ShaderDesc desc_ = {};
    // Created in Device::Init, destroyed in Device destructor.
    Shader* shader_ = nullptr;

    virtual Result Init() = 0;

    friend class DeviceImpl;
};

/** \brief Helper class that represents a shader created automatically when #Device is created
and destroyed when the device is deleted, based on bytecode provided throught a pointer in memory.

The lifetime of this object should be extend beyond the lifetime of the main #Environment object.
It can be defined as a global variable or a static class member. For example:

\code
const uint32_t gMainShaderData[] = { 0x43425844, 0x3FAFC874, ... };
StaticShaderFromMemory gMainShader{
ShaderDesc{ L"Main shader" }, gMainShaderData, sizeof(gMainShaderData) };
\endcode

WARNING! Memory pointed by `bytecode` and `desc.name` must be alive and unchanged for the whole
lifetime of this object. No internal copy is made. They can point to static constants.

This is just a helper class provided for convenience.
Same effect can be achieved by using `Device::CreateShaderFromMemory`.
A disadvantage of using this class is no explicit control over the moment when the shader is created
and destroyed and related error reporting. When the shader creation fails, Environment::CreateDevice
returns an error. To observe the details, check the log.
*/
class StaticShaderFromMemory : public StaticShader
{
public:
    StaticShaderFromMemory();
    StaticShaderFromMemory(const ShaderDesc& desc, ConstDataSpan bytecode);
    ~StaticShaderFromMemory() override;
    bool IsSet() const noexcept { return bytecode_.data != nullptr && bytecode_.size > 0; }
    void Set(const ShaderDesc& desc, ConstDataSpan bytecode);

protected:
    Result Init() override;

private:
    ConstDataSpan bytecode_ = {};
};

/** \brief Helper class that represents a shader created automatically when #Device is created
and destroyed when the device is deleted, based on bytecode loaded from a file.

The lifetime of this object should be extend beyond the lifetime of the main #Environment object.
It can be defined as a global variable or a static class member. For example:

\code
StaticShaderFromFile gMainShader{ ShaderDesc{ L"Main shader" }, L"MainShader.dxil" };
\endcode

WARNING! Memory pointed by `bytecode_file_path` and `desc.name` must be alive and unchanged for the whole
lifetime of this object. No internal copy is made. They can point to static constants.

This is just a helper class provided for convenience.
Same effect can be achieved by using `Device::CreateShaderFromFile`.
A disadvantage of using this class is no explicit control over the moment when the shader is created
and destroyed and related error reporting. When the shader creation fails, Environment::CreateDevice
returns an error. To observe the details, check the log.
*/
class StaticShaderFromFile : public StaticShader
{
public:
    StaticShaderFromFile();
    StaticShaderFromFile(const ShaderDesc& desc, const wchar_t* bytecode_file_path);
    ~StaticShaderFromFile() override;
    bool IsSet() const noexcept;
    void Set(const ShaderDesc& desc, const wchar_t* bytecode_file_path);

protected:
    Result Init() override;

private:
    const wchar_t* bytecode_file_path_ = nullptr;
};

class StaticBuffer
{
public:
    StaticBuffer();
    StaticBuffer(const BufferDesc& desc);
    virtual ~StaticBuffer();
    bool IsSet() const noexcept { return desc_.size > 0; }
    void Set(const BufferDesc& desc);

    Buffer* GetBuffer() const noexcept;

protected:
    BufferDesc desc_ = {};
    // Created in Device::Init, destroyed in Device destructor.
    Buffer* buffer_ = nullptr;

    virtual Result Init();

    friend class DeviceImpl;
};

class StaticBufferFromMemory : public StaticBuffer
{
public:
    StaticBufferFromMemory() = default;
    StaticBufferFromMemory(const BufferDesc& desc, ConstDataSpan initial_data);
    ~StaticBufferFromMemory() override = default;
    void Set(const BufferDesc& desc, ConstDataSpan initial_data);

protected:
    Result Init() override;

private:
    ConstDataSpan initial_data_ = {};
};

class StaticBufferFromFile : public StaticBuffer
{
public:
    StaticBufferFromFile() = default;
    StaticBufferFromFile(const BufferDesc& desc, const wchar_t* initial_data_file_path);
    ~StaticBufferFromFile() override = default;
    void Set(const BufferDesc& desc, const wchar_t* initial_data_file_path);

protected:
    Result Init() override;

private:
    const wchar_t* initial_data_file_path_ = nullptr;
};

struct EnvironmentDesc
{
    /** \brief Path to a directory, relative to the program executable (NOT the current working directory),
    where .dll files from DirectX 12 Agility SDK will be placed.

    You must ensure at least `D3D12Core.dll` is copied from the Agility SDK to this location as part
    of your building process.

    TODO `d3d12SDKLayers.dll` as well if you enable the Debug Layer?
    TODO what about `D3D12StateObjectCompiler.dll`?

    This string is `const char*` not `const wchar_t*` because that's how Microsoft defined function
    `ID3D12SDKConfiguration1::CreateDeviceFactory` that accepts this path.
    */
    const char* d3d12_dll_path = ".\\D3D12\\";
    /** \brief Path to a directory where .dll files from DirectX Shader Compiler (DXC) will be placed.

    You must ensure at least `dxcompiler.dll` is copied from DXC to this location as part of your building process.

    TODO `dxil.dll` as well?

    The path is passed to `LoadLibrary` function, so if not absolute, it may search in the current
    working directory first.
    */
    const wchar_t* dxc_dll_path = L".\\D3D12\\";
    /** \brief Set to true if you are using a preview version of DirectX 12 Agility SDK.

    This controls whether the library uses `D3D12_SDK_VERSION` or `D3D12_PREVIEW_SDK_VERSION` when
    initializing D3D12.
    */
    bool is_d3d12_agility_sdk_preview = false;
};

class Environment
{
public:
    ~Environment();
    EnvironmentImpl* GetImpl() const noexcept { return impl_; }
    /// Returns `IDXGIFactory6*`.
    void* GetNativeDXGIFactory6() const noexcept;
    /// Returns `IDXGIAdapter1*`.
    void* GetNativeAdapter1() const noexcept;
    /// Returns `ID3D12SDKConfiguration1*`.
    void* GetNativeSDKConfiguration1() const noexcept;
    /// Returns `ID3D12DeviceFactory*`.
    void* GetNativeDeviceFactory() const noexcept;

    /** \brief Creates the main #Device object, initializing selected GPU to prepare it for work.
    */
    Result CreateDevice(const DeviceDesc& desc, Device*& out_device);

    /** \brief Compiles a compute shader from HLSL source code to bytecode.

    Note that this function returning #kOK doesn't necessarily mean the compilation succeeded.
    It only means the shader compiler has been invoked and the `out_result` object has been created.
    Inspect that object to check the compilation status (ShaderCompilationResult::GetResult),
    obtain the compiled bytecode (ShaderCompilationResult::GetBytecode), if present, and/or
    error/warning messages (ShaderCompilationResult::GetErrorsAndWarnings).

    This operation is not connected to any particular #Device. Call Device::CreateShaderFromMemory
    to create an actual shader from the bytecode.
    */
    Result CompileShaderFromMemory(const ShaderCompilationParams& params,
        ConstDataSpan hlsl_source, ShaderCompilationResult*& out_result);
    /** \brief Compiles a compute shader from HLSL source code loaded from a file.

    Note that this function returning #kOK doesn't necessarily mean the compilation succeeded.
    It only means the shader compiler has been invoked and the `out_result` object has been created.
    Inspect that object to check the compilation status (ShaderCompilationResult::GetResult),
    obtain the compiled bytecode (ShaderCompilationResult::GetBytecode), if present, and/or
    error/warning messages (ShaderCompilationResult::GetErrorsAndWarnings).

    This operation is not connected to any particular #Device. Call Device::CreateShaderFromMemory
    to create an actual shader from the bytecode.
    */
    Result CompileShaderFromFile(const ShaderCompilationParams& params,
        const wchar_t* hlsl_source_file_path, ShaderCompilationResult*& out_result);
private:
    EnvironmentImpl* impl_ = nullptr;

    Environment();

    friend Result CreateEnvironment(const EnvironmentDesc& desc, Environment*&);
    JD3D12_NO_COPY_NO_MOVE_CLASS(Environment)
};

Result CreateEnvironment(const EnvironmentDesc& desc, Environment*& out_env);

} // namespace jd3d12
