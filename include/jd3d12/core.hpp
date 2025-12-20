#pragma once

#include <jd3d12/utils.hpp>

#if 0
enum BufferFlags : uint32_t
{
    kBufferUsageMaskCpu                = 0x00000007u,
    kBufferUsageFlagCpuRead            = 0x00000001u,
    kBufferUsageFlagCpuSequentialWrite = 0x00000002u,
    //kBufferUsageFlagCpuRandomAccess    = 0x00000004u,

    kBufferUsageMaskCopy     = 0x00000018u,
    kBufferUsageFlagCopySrc  = 0x00000008u,
    kBufferUsageFlagCopyDst  = 0x00000010u,

    kBufferUsageMaskGpu          = 0x000000E0u,
    kBufferUsageFlagGpuConstant  = 0x00000020u,
    kBufferUsageFlagGpuReadOnly  = 0x00000040u,
    kBufferUsageFlagGpuReadWrite = 0x00000080u,

    kBufferFlagTyped       = 0x00000100u,
    kBufferFlagStructured  = 0x00000200u,
    kBufferFlagByteAddress = 0x00000400u,
};

class Device;
class Environment;
struct DeviceDesc;

class DeviceObject
{
public:
    DeviceObject() = default;
    DeviceObject(Device* device, const wchar_t* name);
    DeviceObject(Device* device, const DeviceDesc& desc);
    virtual ~DeviceObject() = 0 { }

    Device* GetDevice() const noexcept { return device_; }
    const wchar_t* GetName() const { return !name_.empty() ? name_.c_str() : nullptr; }

    static void SetDeviceObjectName(uint32_t device_flags, ID3D12Object* obj, const wchar_t* name,
        const wchar_t* suffix = nullptr);
    void SetObjectName(ID3D12Object* obj, const wchar_t* name, const wchar_t* suffix = nullptr);

protected:
    Device* const device_ = nullptr;
    std::wstring name_;
};

struct BufferDesc
{
    const wchar_t* name = nullptr;
    uint32_t flags = 0; // Use BufferFlags.
    size_t size = 0;
    Format element_format = Format::kUnknown;
    size_t structure_size = 0;
};

enum class BufferStrategy
{
    kNone, kUpload, kGpuUpload, kDefault, kReadback
};

class Buffer : public DeviceObject
{
public:
    Buffer() = default;
    ~Buffer() override;

    bool IsValid() const noexcept { return GetDevice() != nullptr; }
    size_t GetSize() const noexcept { return desc_.size; }
    uint32_t GetFlags() const noexcept { return desc_.flags; }
    Format GetElementFormat() const noexcept { return desc_.element_format; }
    size_t GetStructureSize() const noexcept { return desc_.structure_size; }
    /** \brief Returns the size of a single buffer element in bytes.

    - For typed buffer, returns FormatDesc::bits_per_element / 8 of its BufferDesc::element_format.
    - For structured buffer, returns BufferDesc::structure_size.
    - For byte address buffer, returns `sizeof(uint32_t)` = 4.
    - If the element size is unknown, returns 0.
    */
    size_t GetElementSize() const noexcept;
    ID3D12Resource* GetResource() const noexcept { return resource_; }

private:
    BufferDesc desc_;
    BufferStrategy strategy_ = BufferStrategy::kNone;
    CComPtr<ID3D12Resource> resource_;
    void* persistently_mapped_ptr_ = nullptr;
    bool is_user_mapped_ = false;

    Result InitParameters(size_t initial_data_size);
    static D3D12_RESOURCE_STATES GetInitialState(D3D12_HEAP_TYPE heap_type);

    Buffer(Device* device, const BufferDesc& desc);
    Result Init(ConstDataSpan initial_data);
    Result WriteInitialData(ConstDataSpan initial_data);

    friend class StaticBuffer;
    friend class Device;
    JD3D12_NO_COPY_CLASS(Buffer)
};

struct ShaderDesc
{
    const wchar_t* name = nullptr;
};

class Shader : public DeviceObject
{
public:
    Shader() = default;
    ~Shader();
    bool IsValid() const noexcept { return pipeline_state_ != nullptr; }
    ID3D12PipelineState* GetPipelineState() const noexcept { return pipeline_state_; }

private:
    ShaderDesc desc_ = {};
    CComPtr<ID3D12PipelineState> pipeline_state_;

    Shader(Device* device, const ShaderDesc& desc);
    Result Init(ConstDataSpan bytecode);

    friend class Device;
    JD3D12_NO_COPY_CLASS(Shader)
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

class DescriptorHeap : public DeviceObject
{
public:
    static constexpr uint32_t kMaxDescriptorCount = 65536;
    static constexpr uint32_t kStaticDescriptorCount = 3;

    DescriptorHeap(Device* device, const wchar_t* device_name, bool shader_visible)
        : DeviceObject{device, device_name}
        , shader_visible_{shader_visible}
    {
    }
    Result Init(const wchar_t* device_name);

    ID3D12DescriptorHeap* GetDescriptorHeap() const noexcept { return descriptor_heap_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandleBase() const noexcept { assert(shader_visible_); return gpu_handle_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandleBase() const noexcept { return cpu_handle_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandleForDescriptor(uint32_t index) const noexcept;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandleForDescriptor(uint32_t index) const noexcept;

    void ClearDynamic();
    HRESULT AllocateDynamic(uint32_t& out_index);

private:
    const bool shader_visible_ = false;
    uint32_t handle_increment_size_ = 0;
    CComPtr<ID3D12DescriptorHeap> descriptor_heap_;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_ = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle_ = {};
    uint32_t next_dynamic_descriptor_index_ = kStaticDescriptorCount;
};

enum ResourceUsageFlags
{
    kResourceUsageFlagRead = 0x1,
    kResourceUsageFlagWrite = 0x2,
};

struct ResourceUsage
{
    uint32_t flags = 0; // Use ResourceUsageFlags.
    D3D12_RESOURCE_STATES last_state = D3D12_RESOURCE_STATE_COMMON;
};

class ResourceUsageMap
{
public:
    std::map<Buffer*, ResourceUsage> map_;

    bool IsUsed(Buffer* buf, uint32_t usage_flags) const;
};

class MainRootSignature : public DeviceObject
{
public:
    static constexpr uint32_t kMaxCBVCount = 16;
    static constexpr uint32_t kMaxSRVCount = 16;
    static constexpr uint32_t kMaxUAVCount = 8;
    static constexpr uint32_t kTotalParamCount = kMaxCBVCount + kMaxSRVCount + kMaxUAVCount;

    static uint32_t GetRootParamIndexForCBV(uint32_t cbv_index)
    {
        return cbv_index;
    }
    static uint32_t GetRootParamIndexForSRV(uint32_t srv_index)
    {
        return kMaxCBVCount + srv_index;
    }
    static uint32_t GetRootParamIndexForUAV(uint32_t uav_index)
    {
        return kMaxCBVCount + kMaxSRVCount + uav_index;
    }

    MainRootSignature(Device* device) : DeviceObject{device, nullptr} {}
    ID3D12RootSignature* GetRootSignature() const noexcept { return root_signature_; }
    Result Init();

private:
    CComPtr<ID3D12RootSignature> root_signature_;
};

struct Binding
{
    Buffer* buffer = nullptr;
    Range byte_range = kFullRange;
    uint32_t descriptor_index = UINT32_MAX;
};

struct BindingState
{
    Binding cbv_bindings_[MainRootSignature::kMaxCBVCount];
    Binding srv_bindings_[MainRootSignature::kMaxSRVCount];
    Binding uav_bindings_[MainRootSignature::kMaxUAVCount];

    void ResetDescriptors();
    bool IsBufferBound(Buffer* buf);
};

enum CommandFlags : uint32_t
{
    kCommandFlagDontWait = 0x1,
};

class Device : public DeviceObject
{
public:
    Device();
    ~Device();

    bool IsValid() const noexcept { return device_ != nullptr; }
    Environment* GetEnvironment() const noexcept { return env_; }
    ID3D12Device* GetDevice() const noexcept { return device_; }
    D3D12_FEATURE_DATA_D3D12_OPTIONS16 GetOptions16() const noexcept { return options16_; }

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

    Buffer must be created with kBufferUsageFlagGpuReadWrite, as well as either kBufferFlagTyped or
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

    Buffer must be created with kBufferUsageFlagGpuReadWrite and kBufferFlagTyped.

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
    enum class CommandListState { kNone, kRecording, kExecuting };

    Environment* env_ = nullptr;
    DeviceDesc desc_{};
    CComPtr<ID3D12Device> device_;
    D3D12_FEATURE_DATA_D3D12_OPTIONS16 options16_{};

    CComPtr<ID3D12CommandQueue> command_queue_;
    CComPtr<ID3D12CommandAllocator> command_allocator_;
    CComPtr<ID3D12GraphicsCommandList2> command_list_;
    CommandListState command_list_state_ = CommandListState::kRecording;
    CComPtr<ID3D12Fence> fence_;
    std::unique_ptr<HANDLE, CloseHandleDeleter> fence_event_;
    uint64_t submitted_fence_value_ = 0;
    // Used in CommandListState::kRecording and kExecuting.
    ResourceUsageMap resource_usage_map_;
    std::set<Shader*> shader_usage_set_;
    DescriptorHeap shader_visible_descriptor_heap_;
    DescriptorHeap shader_invisible_descriptor_heap_;
    BindingState binding_state_;

    std::unique_ptr<MainRootSignature> main_root_signature_;

    std::atomic<size_t> buffer_count_{ 0 };
    std::atomic<size_t> shader_count_{ 0 };

    // Static descriptors in descriptor_heap_.
    uint32_t null_cbv_index_ = 0;
    uint32_t null_srv_index_ = 1;
    uint32_t null_uav_index_ = 2;

    Device(Environment* env, const DeviceDesc& desc);
    Result Init();

    // Starts executing recorded commands on the GPU. (kRecording -> kExecuting)
    Result ExecuteRecordedCommands();
    // Waits on the CPU until the GPU completes execution. (kExecuting -> kNone)
    Result WaitForCommandExecution(uint32_t timeout_milliseconds);
    // Resets command list to the recording state. (kNone -> kRecording)
    Result ResetCommandListForRecording();

    Result EnsureCommandListState(CommandListState desired_state, uint32_t timeout_milliseconds = kTimeoutInfinite);

    Result WaitForBufferUnused(Buffer* buf);
    Result WaitForShaderUnused(Shader* shader);
    Result UseBuffer(Buffer& buf, D3D12_RESOURCE_STATES state);
    Result UpdateRootArguments();
    void FreeDescriptor(uint32_t desc_index);
    Result CreateNullDescriptors();
    Result CreateStaticShaders();
    Result CreateStaticBuffers();
    void DestroyStaticShaders();
    void DestroyStaticBuffers();
    Result BeginClearBufferToValues(Buffer& buf, Range element_range,
        D3D12_GPU_DESCRIPTOR_HANDLE& out_shader_visible_gpu_desc_handle,
        D3D12_CPU_DESCRIPTOR_HANDLE& out_shader_invisible_cpu_desc_handle);

    friend class Environment;
    friend class DeviceObject;
    friend class Buffer;
    friend class Shader;
    JD3D12_NO_COPY_CLASS(Device)
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

    friend class Device;
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

    friend class Device;
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

class ShaderCompiler
{
public:
    ShaderCompiler() = default;
    ~ShaderCompiler() = default;
    Result Init();
    bool IsValid() const noexcept { return compiler_ != nullptr; }

private:
    HMODULE module_ = nullptr;
    DxcCreateInstanceProc create_instance_proc_ = nullptr;
    CComPtr<IDxcUtils> utils_;
    CComPtr<IDxcCompiler3> compiler_;
    CComPtr<IDxcIncludeHandler> include_handler_;

    JD3D12_NO_COPY_CLASS(ShaderCompiler)
};

class Environment
{
public:
    Environment();
    ~Environment();

    bool IsValid() const noexcept { return dxgi_factory6_ != nullptr; }
    IDXGIFactory6* GetDXGIFactory6() const noexcept { return dxgi_factory6_; }
    IDXGIAdapter1* GetAdapter() const noexcept { return adapter_; }
    ID3D12SDKConfiguration1* GetSDKConfiguration1() const noexcept { return sdk_config1_; }
    ID3D12DeviceFactory* GetDeviceFactory() const noexcept { return device_factory_; }

    Result CreateDevice(const DeviceDesc& desc, Device*& out_device);

private:
    CComPtr<IDXGIFactory6> dxgi_factory6_;
    UINT selected_adapter_index_ = UINT32_MAX;
    CComPtr<IDXGIAdapter1> adapter_;
    CComPtr<ID3D12SDKConfiguration1> sdk_config1_;
    CComPtr<ID3D12DeviceFactory> device_factory_;
    std::atomic<size_t> device_count_{ 0 };
    ShaderCompiler shader_compiler_;

    Result Init();

    friend Result CreateEnvironment(Environment*&);
    friend class Device;
    JD3D12_NO_COPY_CLASS(Environment)
};

Result CreateEnvironment(Environment*& out_env);
#endif
