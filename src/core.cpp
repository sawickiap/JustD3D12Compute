// Copyright (c) 2025-2026 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#include <jd3d12/core.hpp>
#include <jd3d12/config.hpp>
#include "logger.hpp"
#include "internal_utils.hpp"

namespace jd3d12
{
using FileSystemPath = std::filesystem::path;

////////////////////////////////////////////////////////////////////////////////
// Type definitions

class DeviceObject
{
public:
    DeviceObject() = default;
    DeviceObject(DeviceImpl* device, const wchar_t* name);
    DeviceObject(DeviceImpl* device, const DeviceDesc& desc);
    virtual ~DeviceObject() = 0 { }

    DeviceImpl* GetDevice() const noexcept { return device_; }
    Logger* GetLogger() const;
    ID3D12Device* GetD3d12Device() const noexcept;
    const wchar_t* GetName() const { return !name_.empty() ? name_.c_str() : nullptr; }

    static void SetDeviceObjectName(uint32_t device_flags, ID3D12Object* obj, const wchar_t* name,
        const wchar_t* suffix = nullptr);
    void SetObjectName(ID3D12Object* obj, const wchar_t* name, const wchar_t* suffix = nullptr);

protected:
    DeviceImpl* const device_ = nullptr;
    std::wstring name_;
};

class StaticShader;
class StaticBuffer;

class Singleton
{
public:
    EnvironmentImpl* env_ = nullptr;
    Device* first_dev_ = nullptr;
    size_t dev_count_ = 0;
    std::vector<StaticShader*> static_shaders_;
    std::vector<StaticBuffer*> static_buffers_;

    static Singleton& GetInstance()
    {
        static Singleton instance;
        return instance;
    }

private:
};

enum class BufferStrategy
{
    kNone, kUpload, kGpuUpload, kDefault, kReadback
};

class BufferImpl : public DeviceObject
{
public:
    BufferImpl(Buffer* interface_obj, DeviceImpl* device, const BufferDesc& desc);
    ~BufferImpl() override;
    Result Init(ConstDataSpan initial_data);

    size_t GetSize() const noexcept { return desc_.size; }
    uint32_t GetFlags() const noexcept { return desc_.flags; }
    Format GetElementFormat() const noexcept { return desc_.element_format; }
    size_t GetStructureSize() const noexcept { return desc_.structure_size; }
    size_t GetElementSize() const noexcept;
    ID3D12Resource* GetD3D12Resource() const noexcept { return resource_; }

private:
    Buffer* const interface_obj_;
    BufferDesc desc_;
    BufferStrategy strategy_ = BufferStrategy::kNone;
    CComPtr<ID3D12Resource> resource_;
    void* persistently_mapped_ptr_ = nullptr;
    bool is_user_mapped_ = false;

    Result InitParameters(size_t initial_data_size);
    static D3D12_RESOURCE_STATES GetInitialState(D3D12_HEAP_TYPE heap_type);

    Result WriteInitialData(ConstDataSpan initial_data);

    friend class Buffer;
    friend class StaticBuffer;
    friend class DeviceImpl;
    JD3D12_NO_COPY_NO_MOVE_CLASS(BufferImpl)
};

class ShaderImpl : public DeviceObject
{
public:
    ShaderImpl(Shader* interface_obj, DeviceImpl* device, const ShaderDesc& desc);
    ~ShaderImpl();
    Result Init(ConstDataSpan bytecode, ID3D12ShaderReflection* reflection);

    UintVec3 GetThreadGroupSize() const noexcept { return thread_group_size_; }
    ID3D12PipelineState* GetD3D12PipelineState() const noexcept { return pipeline_state_; }

private:
    Shader* const interface_obj_;
    ShaderDesc desc_ = {};
    UintVec3 thread_group_size_ = {};
    CComPtr<ID3D12PipelineState> pipeline_state_;

    friend class Device;
    JD3D12_NO_COPY_NO_MOVE_CLASS(ShaderImpl)
};

class DescriptorHeap : public DeviceObject
{
public:
    static constexpr uint32_t kMaxDescriptorCount = 65536;
    static constexpr uint32_t kStaticDescriptorCount = 3;

    DescriptorHeap(DeviceImpl* device, const wchar_t* device_name, bool shader_visible)
        : DeviceObject{device, device_name}
        , shader_visible_{shader_visible}
    {
    }
    Result Init(const wchar_t* device_name);

    ID3D12DescriptorHeap* GetDescriptorHeap() const noexcept { return descriptor_heap_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandleBase() const noexcept { JD3D12_ASSERT(shader_visible_); return gpu_handle_; }
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
    std::unordered_map<BufferImpl*, ResourceUsage> map_;

    bool IsUsed(BufferImpl* buf, uint32_t usage_flags) const;
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

    MainRootSignature(DeviceImpl* device) : DeviceObject{device, nullptr} {}
    ID3D12RootSignature* GetRootSignature() const noexcept { return root_signature_; }
    Result Init();

private:
    CComPtr<ID3D12RootSignature> root_signature_;

    JD3D12_NO_COPY_NO_MOVE_CLASS(MainRootSignature)
};

struct Binding
{
    BufferImpl* buffer = nullptr;
    Range byte_range = kFullRange;
    uint32_t descriptor_index = UINT32_MAX;
};

struct BindingState
{
    Binding cbv_bindings_[MainRootSignature::kMaxCBVCount];
    Binding srv_bindings_[MainRootSignature::kMaxSRVCount];
    Binding uav_bindings_[MainRootSignature::kMaxUAVCount];

    void ResetDescriptors();
    bool IsBufferBound(BufferImpl* buf);
};

class ShaderCompilationResultImpl
{
public:
    ShaderCompilationResultImpl(ShaderCompilationResult* interface_obj, EnvironmentImpl* env);
    ~ShaderCompilationResultImpl() = default;
    Result Init(CComPtr<IDxcResult>&& dxc_result);
    EnvironmentImpl* GetEnvironment() const noexcept { return env_; }

    Result GetResult() { return status_; }
    const char* GetErrorsAndWarnings()
    {
        if (!errors_ || errors_->GetBufferSize() == 0)
            return "";
        return reinterpret_cast<const char*>(errors_->GetBufferPointer());
    }
    ConstDataSpan GetBytecode()
    {
        if(!object_ || object_->GetBufferSize() == 0)
            return kEmptyDataSpan;
        return { object_->GetBufferPointer(), object_->GetBufferSize() };
    }

private:
    ShaderCompilationResult* const interface_obj_ = nullptr;
    EnvironmentImpl* const env_ = nullptr;
    CComPtr<IDxcResult> dxc_result_;
    HRESULT status_ = E_FAIL;
    CComPtr<IDxcBlobUtf8> errors_;
    CComPtr<IDxcBlob> object_;

    friend class ShaderCompiler;
};

class DeviceImpl : public DeviceObject
{
public:
    DeviceImpl(Device* interface_obj, EnvironmentImpl* env, const DeviceDesc& desc);
    ~DeviceImpl();
    Result Init(bool enable_d3d12_debug_layer);

    Device* GetInterface() const noexcept { return interface_obj_; }
    EnvironmentImpl* GetEnvironment() const noexcept { return env_; }
    ID3D12Device* GetD3D12Device() const noexcept { return device_; }
    D3D12_FEATURE_DATA_D3D12_OPTIONS16 GetOptions16() const noexcept { return options16_; }

    Result CreateBuffer(const BufferDesc& desc, Buffer*& out_buffer);
    Result CreateBufferFromMemory(const BufferDesc& desc, ConstDataSpan initial_data,
        Buffer*& out_buffer);
    Result CreateBufferFromFile(const BufferDesc& desc, const wchar_t* initial_data_file_path,
        Buffer*& out_buffer);

    Result CreateShaderFromMemory(const ShaderDesc& desc, ConstDataSpan bytecode,
        Shader*& out_shader);
    Result CreateShaderFromFile(const ShaderDesc& desc, const wchar_t* bytecode_file_path,
        Shader*& out_shader);
    Result CompileAndCreateShaderFromMemory(const ShaderCompilationParams& compilation_params,
        const ShaderDesc& desc, ConstDataSpan hlsl_source, Shader*& out_shader);
    Result CompileAndCreateShaderFromFile(const ShaderCompilationParams& compilation_params,
        const ShaderDesc& desc, const wchar_t* hlsl_source_file_path, Shader*& out_shader);

    Result MapBuffer(BufferImpl& buf, Range byte_range, BufferFlags cpu_usage_flag, void*& out_data_ptr,
        uint32_t command_flags = 0);
    void UnmapBuffer(BufferImpl& buf);

    Result ReadBufferToMemory(BufferImpl& src_buf, Range src_byte_range, void* dst_memory,
        uint32_t command_flags = 0);
    Result WriteMemoryToBuffer(ConstDataSpan src_data, BufferImpl& dst_buf, size_t dst_byte_offset,
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

    Result CopyBuffer(BufferImpl& src_buf, BufferImpl& dst_buf);
    Result CopyBufferRegion(BufferImpl& src_buf, Range src_byte_range, BufferImpl& dst_buf, size_t dst_byte_offset);

    Result ClearBufferToUintValues(BufferImpl& buf, const UintVec4& values, Range element_range = kFullRange);
    Result ClearBufferToFloatValues(BufferImpl& buf, const FloatVec4& values, Range element_range = kFullRange);

    void ResetAllBindings();
    Result BindConstantBuffer(uint32_t b_slot, BufferImpl* buf, Range byte_range = kFullRange);
    Result BindBuffer(uint32_t t_slot, BufferImpl* buf, Range byte_range = kFullRange);
    Result BindRWBuffer(uint32_t u_slot, BufferImpl* buf, Range byte_range = kFullRange);
    Result DispatchComputeShader(ShaderImpl& shader, const UintVec3& group_count);

private:
    enum class CommandListState { kNone, kRecording, kExecuting };

    static void StaticDebugLayerMessageCallback(
        D3D12_MESSAGE_CATEGORY Category,
        D3D12_MESSAGE_SEVERITY Severity,
        D3D12_MESSAGE_ID ID,
        LPCSTR pDescription,
        void* pContext);

    Device* const interface_obj_;
    EnvironmentImpl* const env_;
    DeviceDesc desc_{};
    CComPtr<ID3D12Device> device_;

    // Optional, can be null if Debug Layer was not enabled.
    CComPtr<ID3D12InfoQueue1> info_queue_;
    DWORD debug_layer_callback_cookie_ = UINT32_MAX;

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
    std::unordered_set<ShaderImpl*> shader_usage_set_;
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

    Result EnableDebugLayer();
    void DebugLayerMessageCallback(
        D3D12_MESSAGE_CATEGORY Category,
        D3D12_MESSAGE_SEVERITY Severity,
        D3D12_MESSAGE_ID ID,
        LPCSTR pDescription);

    // Starts executing recorded commands on the GPU. (kRecording -> kExecuting)
    Result ExecuteRecordedCommands();
    // Waits on the CPU until the GPU completes execution. (kExecuting -> kNone)
    Result WaitForCommandExecution(uint32_t timeout_milliseconds);
    // Resets command list to the recording state. (kNone -> kRecording)
    Result ResetCommandListForRecording();

    Result EnsureCommandListState(CommandListState desired_state, uint32_t timeout_milliseconds = kTimeoutInfinite);

    Result WaitForBufferUnused(BufferImpl* buf);
    Result WaitForShaderUnused(ShaderImpl* shader);
    Result UseBuffer(BufferImpl& buf, D3D12_RESOURCE_STATES state);
    Result UpdateRootArguments();
    void FreeDescriptor(uint32_t desc_index);
    Result CreateNullDescriptors();
    Result CreateStaticShaders();
    Result CreateStaticBuffers();
    void DestroyStaticShaders();
    void DestroyStaticBuffers();
    Result BeginClearBufferToValues(BufferImpl& buf, Range element_range,
        D3D12_GPU_DESCRIPTOR_HANDLE& out_shader_visible_gpu_desc_handle,
        D3D12_CPU_DESCRIPTOR_HANDLE& out_shader_invisible_cpu_desc_handle);

    friend class Environment;
    friend class EnvironmentImpl;
    friend class DeviceObject;
    friend class BufferImpl;
    friend class ShaderImpl;
    JD3D12_NO_COPY_NO_MOVE_CLASS(DeviceImpl)
};

class ShaderCompiler;

class IncludeHandlerBase : public IDxcIncludeHandler
{
public:
    IncludeHandlerBase(ShaderCompiler* shader_compiler, CharacterEncoding character_encoding);
    virtual ~IncludeHandlerBase() = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID riid, _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef(void) override { return 1; }
    ULONG STDMETHODCALLTYPE Release(void) override { return 1; }

protected:
    ShaderCompiler* const shader_compiler_ = nullptr;
    const CharacterEncoding character_encoding_ = kCharacterEncodingAnsi;
};

class DefaultIncludeHandler : public IncludeHandlerBase
{
public:
    DefaultIncludeHandler(ShaderCompiler* shader_compiler, CharacterEncoding character_encoding);

    HRESULT STDMETHODCALLTYPE LoadSource(_In_z_ LPCWSTR pFilename,
        _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource) override;

private:
    static bool FileExists(const FileSystemPath& path);
};

class CallbackIncludeHandler : public IncludeHandlerBase
{
public:
    CallbackIncludeHandler(ShaderCompiler* shader_compiler, CharacterEncoding character_encoding,
        IncludeCallback callback, void* callback_context);

    HRESULT STDMETHODCALLTYPE LoadSource(_In_z_ LPCWSTR pFilename,
        _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource) override;

private:
    IncludeCallback callback_ = nullptr;
    void* callback_context_ = nullptr;
};

class ShaderCompiler
{
public:
    ShaderCompiler(EnvironmentImpl& env) : env_{env} { }
    ~ShaderCompiler() = default;
    Result Init(const EnvironmentDesc& env_desc);

    Logger* GetLogger() const noexcept;
    IDxcUtils* GetDxcUtils() const noexcept { return utils_.p; }

    Result CompileShaderFromMemory(const ShaderCompilationParams& params, const wchar_t* main_source_file_path,
        ConstDataSpan hlsl_source, ShaderCompilationResult*& out_result);

private:
    EnvironmentImpl& env_;
    HMODULE module_ = nullptr;
    DxcCreateInstanceProc create_instance_proc_ = nullptr;
    CComPtr<IDxcUtils> utils_;
    CComPtr<IDxcCompiler3> compiler3_;

    Result BuildArguments(const ShaderCompilationParams& params,
        const wchar_t* source_name, std::vector<std::wstring>& out_arguments);
    void LogCompilationResult(ShaderCompilationResult& result);

    JD3D12_NO_COPY_NO_MOVE_CLASS(ShaderCompiler)
};

class EnvironmentImpl
{
public:
    EnvironmentImpl(Environment* interface_obj, const EnvironmentDesc& desc);
    ~EnvironmentImpl();
    Result Init();

    void Log(LogSeverity severity, const wchar_t* message);
    void VLogF(LogSeverity severity, const wchar_t* format, va_list arg_list);

    Environment* GetInterface() const noexcept { return interface_obj_; }
    Logger* GetLogger() const noexcept { return logger_.get(); }
    IDXGIFactory6* GetDXGIFactory6() const noexcept { return dxgi_factory6_; }
    IDXGIAdapter1* GetDXGIAdapter1() const noexcept { return adapter_; }
    ID3D12SDKConfiguration1* GetD3D12SDKConfiguration1() const noexcept { return sdk_config1_; }
    ID3D12DeviceFactory* GetD3D12DeviceFactory() const noexcept { return device_factory_; }
    ShaderCompiler& GetShaderCompiler() { return shader_compiler_; }

    Result CreateDevice(const DeviceDesc& desc, Device*& out_device);

    Result CompileShaderFromMemory(const ShaderCompilationParams& params, const wchar_t* name,
        ConstDataSpan hlsl_source, ShaderCompilationResult*& out_result);
    Result CompileShaderFromFile(const ShaderCompilationParams& params,
        const wchar_t* hlsl_source_file_path, ShaderCompilationResult*& out_result);

private:
    Environment* const interface_obj_;
    EnvironmentDesc desc_ = {};
    std::unique_ptr<Logger> logger_;
    CComPtr<IDXGIFactory6> dxgi_factory6_;
    UINT selected_adapter_index_ = UINT32_MAX;
    CComPtr<IDXGIAdapter1> adapter_;
    CComPtr<ID3D12SDKConfiguration1> sdk_config1_;
    CComPtr<ID3D12DeviceFactory> device_factory_;
    std::atomic<size_t> device_count_{ 0 };
    ShaderCompiler shader_compiler_;

    Result EnableDebugLayer();

    JD3D12_NO_COPY_NO_MOVE_CLASS(EnvironmentImpl)
};

////////////////////////////////////////////////////////////////////////////////
// class DeviceObject

DeviceObject::DeviceObject(DeviceImpl* device, const wchar_t* name)
    : device_{device}
{
    JD3D12_ASSERT(device != nullptr);
    if ((device->desc_.flags & kDeviceFlagDisableNameStoring) == 0 && !IsStringEmpty(name))
        name_ = name;
}

DeviceObject::DeviceObject(DeviceImpl* device, const DeviceDesc& desc)
    : device_{device}
{
    JD3D12_ASSERT(device != nullptr);

    // This constructor is inteded for Device class constructor, where device->desc_ is not yet initialized.
    if ((desc.flags & kDeviceFlagDisableNameStoring) == 0 && !IsStringEmpty(desc.name))
        name_ = desc.name;
}

ID3D12Device* DeviceObject::GetD3d12Device() const noexcept
{
    JD3D12_ASSERT(device_ != nullptr);
    return device_->GetD3D12Device();
}

Logger* DeviceObject::GetLogger() const
{
    JD3D12_ASSERT(device_ && device_->GetEnvironment());
    return device_->GetEnvironment()->GetLogger();
}

void DeviceObject::SetDeviceObjectName(uint32_t device_flags, ID3D12Object *obj, const wchar_t* name,
    const wchar_t* suffix)
{
    if (!IsStringEmpty(name) &&
        (device_flags & DeviceFlags::kDeviceFlagDisableNameSetting) == 0)
    {
        if (IsStringEmpty(suffix))
            obj->SetName(name);
        else
        {
            std::wstring full_name = name;
            full_name += L" [";
            full_name += suffix;
            full_name += L"]";
            obj->SetName(full_name.c_str());
        }
    }
}

void DeviceObject::SetObjectName(ID3D12Object* obj, const wchar_t* name, const wchar_t* suffix)
{
    if(device_ != nullptr)
        SetDeviceObjectName(device_->desc_.flags, obj, name, suffix);
}

////////////////////////////////////////////////////////////////////////////////
// class BufferImpl

Result BufferImpl::InitParameters(size_t initial_data_size)
{
    JD3D12_ASSERT_OR_RETURN((desc_.flags &
        (kBufferUsageMaskCpu | kBufferUsageMaskCopy | kBufferUsageMaskShader)) != 0,
        L"At least one usage flag must be specified - a buffer with no usage flags makes no sense.");
    JD3D12_ASSERT_OR_RETURN(CountBitsSet(desc_.flags & kBufferUsageMaskCpu) <= 1,
        L"kBufferUsageFlagCpu* are mutually exclusive - you can specify at most 1.");

    const bool is_typed = (desc_.flags & kBufferFlagTyped) != 0;
    const bool is_structured = (desc_.flags & kBufferFlagStructured) != 0;

    const uint32_t type_bit_count = CountBitsSet(desc_.flags
        & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress));
    JD3D12_ASSERT_OR_RETURN(type_bit_count <= 1,
        L"kBufferFlagTyped, kBufferFlagStructured, kBufferFlagByteAddress are mutually exclusive - you can specify at most 1.");

    JD3D12_ASSERT_OR_RETURN(desc_.size > 0 && desc_.size % 4 == 0,
        L"Buffer size must be greater than 0 and a multiple of 4.");
    JD3D12_ASSERT_OR_RETURN(initial_data_size <= desc_.size,
        L"initial_data_size exceeds buffer size.");

    if(initial_data_size > 0)
    {
        // TODO Remove this limitation in the future.
        JD3D12_ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0,
            L"Buffer initial data can only be used with kBufferUsageCpuSequentialWrite.");
    }

    JD3D12_ASSERT_OR_RETURN(is_typed == (desc_.element_format != Format::kUnknown),
        L"element_format should be set if and only if the buffer is used as typed buffer.");
    if(is_typed)
    {
        const FormatDesc* format_desc = GetFormatDesc(desc_.element_format);
        JD3D12_ASSERT_OR_RETURN(format_desc != nullptr && format_desc->bits_per_element > 0
            && format_desc->bits_per_element % 8 == 0,
            L"element_format must be a valid format with size multiple of 8 bits.");
    }

    JD3D12_ASSERT_OR_RETURN(is_structured == (desc_.structure_size > 0),
        L"structure_size should be set if and only if the buffer is used as structured buffer.");
    if(is_structured)
    {
        JD3D12_ASSERT_OR_RETURN(desc_.structure_size % 4 == 0, L"structure_size must be a multiple of 4.");
    }

    const size_t element_size = GetElementSize();
    if(element_size > 0)
    {
        JD3D12_ASSERT_OR_RETURN(desc_.size % element_size == 0, L"Buffer size must be a multiple of element size.");
    }

    // Choose strategy.
    if((desc_.flags & kBufferUsageFlagShaderRWResource) != 0)
    {
        strategy_ = BufferStrategy::kDefault;
        // kBufferUsageFlagCpuSequentialWrite is allowed.
        JD3D12_ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCpuRead) == 0,
            L"kBufferUsageFlagCpuRead cannot be used with kBufferUsageFlagShaderRWResource.");
    }
    else if((desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0)
    {
        strategy_ = BufferStrategy::kUpload;

        JD3D12_ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCopyDst) == 0,
            L"BufferUsageFlagCopyDst cannot be used with kBufferUsageFlagCpuSequentialWrite.");
    }
    else if((desc_.flags & kBufferUsageFlagCpuRead) != 0)
    {
        strategy_ = BufferStrategy::kReadback;

        JD3D12_ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCopySrc) == 0,
            L"kBufferUsageFlagCopySrc cannot be used with kBufferUsageFlagCpuRead.");
        JD3D12_ASSERT_OR_RETURN((desc_.flags & kBufferUsageMaskShader) == 0,
            L"kBufferUsageFlagShader* cannot be used with kBufferUsageFlagCpuRead.");
    }
    else
    {
        strategy_ = BufferStrategy::kDefault;
    }

    return kSuccess;
}

D3D12_RESOURCE_STATES BufferImpl::GetInitialState(D3D12_HEAP_TYPE heap_type)
{
    switch(heap_type)
    {
    case D3D12_HEAP_TYPE_DEFAULT:
        return D3D12_RESOURCE_STATE_COMMON;
    case D3D12_HEAP_TYPE_UPLOAD:
    case D3D12_HEAP_TYPE_GPU_UPLOAD:
        return D3D12_RESOURCE_STATE_GENERIC_READ;
    case D3D12_HEAP_TYPE_READBACK:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    default:
        JD3D12_ASSERT(0);
        return D3D12_RESOURCE_STATE_COMMON;
    }
}

BufferImpl::BufferImpl(Buffer* interface_obj, DeviceImpl* device, const BufferDesc& desc)
    : DeviceObject{device, desc.name}
    , interface_obj_{interface_obj}
    , desc_{desc}
{
    ++GetDevice()->buffer_count_;
}

BufferImpl::~BufferImpl()
{
    JD3D12_LOG(kLogSeverityInfo, L"Destroying Buffer 0x%016" PRIXPTR, uintptr_t(interface_obj_));

    DeviceImpl* const dev = GetDevice();

    HRESULT hr = dev->WaitForBufferUnused(this);
    JD3D12_ASSERT(SUCCEEDED(hr) && "Failed to wait for buffer unused in Buffer destructor.");

    JD3D12_ASSERT(!is_user_mapped_ && "Destroying buffer that is still mapped - missing call to Device::UnmapBuffer.");

    --dev->buffer_count_;
}

size_t BufferImpl::GetElementSize() const noexcept
{
    if ((desc_.flags & kBufferFlagTyped) != 0)
    {
        JD3D12_ASSERT(desc_.element_format != Format::kUnknown);
        const FormatDesc* format_desc = GetFormatDesc(desc_.element_format);
        if(format_desc != nullptr && format_desc->bits_per_element % 8 == 0)
            return format_desc->bits_per_element / 8;
    }
    else if ((desc_.flags & kBufferFlagStructured) != 0)
    {
        return desc_.structure_size;
    }
    else if ((desc_.flags & kBufferFlagByteAddress) != 0)
    {
        return sizeof(uint32_t);
    }
    return 0;
}

Result BufferImpl::Init(ConstDataSpan initial_data)
{
    if(initial_data.size > 0)
    {
        JD3D12_ASSERT_OR_RETURN(initial_data.data != nullptr,
            L"When initial_data.size > 0, initial_data pointer cannot be null.");
    }

    JD3D12_RETURN_IF_FAILED(InitParameters(initial_data.size));
    JD3D12_ASSERT(strategy_ != BufferStrategy::kNone);

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if((desc_.flags & kBufferUsageFlagShaderRWResource) != 0)
    {
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(desc_.size, flags);

    D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
    switch(strategy_)
    {
    case BufferStrategy::kDefault:
        heap_type = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case BufferStrategy::kUpload:
        heap_type = D3D12_HEAP_TYPE_UPLOAD;
        break;
    case BufferStrategy::kGpuUpload:
        heap_type = D3D12_HEAP_TYPE_GPU_UPLOAD;
        break;
    case BufferStrategy::kReadback:
        heap_type = D3D12_HEAP_TYPE_READBACK;
        break;
    default:
        JD3D12_ASSERT(0);
    }
    CD3DX12_HEAP_PROPERTIES heap_props = CD3DX12_HEAP_PROPERTIES{heap_type};
    const D3D12_RESOURCE_STATES initial_state = GetInitialState(heap_type);
    JD3D12_LOG_AND_RETURN_IF_FAILED(GetD3d12Device()->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE,
        &resource_desc, initial_state, nullptr, IID_PPV_ARGS(&resource_)));

    SetObjectName(resource_, desc_.name);
    desc_.name = nullptr;

    if (strategy_ == BufferStrategy::kUpload
        || strategy_ == BufferStrategy::kGpuUpload
        || strategy_ == BufferStrategy::kReadback)
    {
        JD3D12_LOG_AND_RETURN_IF_FAILED(resource_->Map(0, nullptr, &persistently_mapped_ptr_));
    }

    JD3D12_RETURN_IF_FAILED(WriteInitialData(initial_data));

    return kSuccess;
}

Result BufferImpl::WriteInitialData(ConstDataSpan initial_data)
{
    if(initial_data.size == 0)
        return kFalse;

    JD3D12_ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0,
        L"Buffer doesn't have kBufferUsageFlagCpuSequentialWrite but initial data was specified.");

    JD3D12_ASSERT(persistently_mapped_ptr_ != nullptr);
    memcpy(persistently_mapped_ptr_, initial_data.data, initial_data.size);

    return kSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// class ShaderImpl

ShaderImpl::ShaderImpl(Shader* interface_obj, DeviceImpl* device, const ShaderDesc& desc)
    : DeviceObject{device, desc.name}
    , interface_obj_{interface_obj}
    , desc_{desc}
{
    ++device->shader_count_;
}

ShaderImpl::~ShaderImpl()
{
    JD3D12_LOG(kLogSeverityInfo, L"Destroying Shader 0x%016" PRIXPTR, uintptr_t(interface_obj_));

    DeviceImpl* const dev = GetDevice();
    HRESULT hr = dev->WaitForShaderUnused(this);
    JD3D12_ASSERT(SUCCEEDED(hr) && "Failed to wait for shader unused in Shader destructor.");
    --dev->shader_count_;
}

Result ShaderImpl::Init(ConstDataSpan bytecode, ID3D12ShaderReflection* reflection)
{
    DeviceImpl* const dev = GetDevice();

    D3D12_SHADER_DESC shader_desc = {};
    JD3D12_LOG_AND_RETURN_IF_FAILED(reflection->GetDesc(&shader_desc));

    if(D3D12_SHVER_GET_TYPE(shader_desc.Version) != D3D12_SHVER_COMPUTE_SHADER)
    {
        JD3D12_LOG(kLogSeverityError, L"Only compute shaders are supported.");
        return kErrorInvalidArgument;
    }

    /* TODO:
    const uint64_t requires_flags = reflection->GetRequiresFlags();
    // shader_desc.Flags or GetRequiresFlags ??
    // D3D_SHADER_REQUIRES_RESOURCE_DESCRIPTOR_HEAP_INDEXING or D3D_SHADER_FEATURE_RESOURCE_DESCRIPTOR_HEAP_INDEXING ??
    JD3D12_ASSERT((desc.Flags & D3D_SHADER_REQUIRES_RESOURCE_DESCRIPTOR_HEAP_INDEXING) == 0);
    JD3D12_ASSERT((desc.Flags & D3D_SHADER_REQUIRES_SAMPLER_DESCRIPTOR_HEAP_INDEXING) == 0);
    */

    JD3D12_LOG_AND_RETURN_IF_FAILED(reflection->GetThreadGroupSize(
        &thread_group_size_.x, &thread_group_size_.y, &thread_group_size_.z));
    JD3D12_ASSERT(thread_group_size_.x > 0 && thread_group_size_.y > 0 && thread_group_size_.z > 0);

    /*
    for(uint32_t i = 0; i < shader_desc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC bind_desc = {};
        JD3D12_ASSERT(SUCCEEDED(reflection->GetResourceBindingDesc(0, &bind_desc)));
        Validate, save parameters...
    }
    */

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = dev->main_root_signature_->GetRootSignature();
    pso_desc.CS.pShaderBytecode = bytecode.data;
    pso_desc.CS.BytecodeLength = bytecode.size;
    JD3D12_LOG_AND_RETURN_IF_FAILED(dev->GetD3D12Device()->CreateComputePipelineState(
        &pso_desc, IID_PPV_ARGS(&pipeline_state_)));

    SetObjectName(pipeline_state_, desc_.name);
    desc_.name = nullptr;

    return kSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// class DescriptorHeap

Result DescriptorHeap::Init(const wchar_t* device_name)
{
    ID3D12Device* const d3d12_dev = GetD3d12Device();

    constexpr D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    handle_increment_size_ = d3d12_dev->GetDescriptorHandleIncrementSize(heap_type);

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = heap_type;
    desc.NumDescriptors = kMaxDescriptorCount;
    if(shader_visible_)
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    JD3D12_LOG_AND_RETURN_IF_FAILED(d3d12_dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap_)));

    SetObjectName(descriptor_heap_, device_name, shader_visible_
        ? L"Descriptor heap (shader-visible)"
        : L"Descriptor heap (shader-invisible)");

    if(shader_visible_)
        gpu_handle_ = descriptor_heap_->GetGPUDescriptorHandleForHeapStart();
    cpu_handle_ = descriptor_heap_->GetCPUDescriptorHandleForHeapStart();

    return kSuccess;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpuHandleForDescriptor(uint32_t index) const noexcept
{
    JD3D12_ASSERT(shader_visible_);
    JD3D12_ASSERT(index < kMaxDescriptorCount);

    D3D12_GPU_DESCRIPTOR_HANDLE h = gpu_handle_;
    h.ptr += index * handle_increment_size_;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpuHandleForDescriptor(uint32_t index) const noexcept
{
    JD3D12_ASSERT(index < kMaxDescriptorCount);

    D3D12_CPU_DESCRIPTOR_HANDLE h = cpu_handle_;
    h.ptr += index * handle_increment_size_;
    return h;
}

HRESULT DescriptorHeap::AllocateDynamic(uint32_t& out_index)
{
    if(next_dynamic_descriptor_index_ == kMaxDescriptorCount)
        return kErrorTooManyObjects;
    out_index = next_dynamic_descriptor_index_++;
    return kSuccess;
}

void DescriptorHeap::ClearDynamic()
{
    next_dynamic_descriptor_index_ = kStaticDescriptorCount;
}

////////////////////////////////////////////////////////////////////////////////
// class ResourceUsageMap

bool ResourceUsageMap::IsUsed(BufferImpl* buf, uint32_t usage_flags) const
{
    const auto it = map_.find(buf);
    if(it == map_.end())
        return false;
    return (it->second.flags & usage_flags) != 0;
}

////////////////////////////////////////////////////////////////////////////////
// class MainRootSignature

Result MainRootSignature::Init()
{
    D3D12_DESCRIPTOR_RANGE desc_ranges[kTotalParamCount] = {};
    D3D12_ROOT_PARAMETER params[kTotalParamCount] = {};
    uint32_t param_index = 0;
    for(uint32_t i = 0; i < kMaxCBVCount; ++i, ++param_index)
    {
        desc_ranges[param_index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        desc_ranges[param_index].NumDescriptors = 1;
        desc_ranges[param_index].BaseShaderRegister = i;
        desc_ranges[param_index].RegisterSpace = 0;
        desc_ranges[param_index].OffsetInDescriptorsFromTableStart = 0;

        params[param_index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[param_index].DescriptorTable.NumDescriptorRanges = 1;
        params[param_index].DescriptorTable.pDescriptorRanges = &desc_ranges[param_index];
        params[param_index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    for(uint32_t i = 0; i < kMaxSRVCount; ++i, ++param_index)
    {
        desc_ranges[param_index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        desc_ranges[param_index].NumDescriptors = 1;
        desc_ranges[param_index].BaseShaderRegister = i;
        desc_ranges[param_index].RegisterSpace = 0;
        desc_ranges[param_index].OffsetInDescriptorsFromTableStart = 0;

        params[param_index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[param_index].DescriptorTable.NumDescriptorRanges = 1;
        params[param_index].DescriptorTable.pDescriptorRanges = &desc_ranges[param_index];
        params[param_index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    for(uint32_t i = 0; i < kMaxUAVCount; ++i, ++param_index)
    {
        desc_ranges[param_index].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        desc_ranges[param_index].NumDescriptors = 1;
        desc_ranges[param_index].BaseShaderRegister = i;
        desc_ranges[param_index].RegisterSpace = 0;
        desc_ranges[param_index].OffsetInDescriptorsFromTableStart = 0;

        params[param_index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[param_index].DescriptorTable.NumDescriptorRanges = 1;
        params[param_index].DescriptorTable.pDescriptorRanges = &desc_ranges[param_index];
        params[param_index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    JD3D12_ASSERT(param_index == kTotalParamCount);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC root_sig_desc = {};
    root_sig_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
    root_sig_desc.Desc_1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
    root_sig_desc.Desc_1_0.pParameters = params;
    root_sig_desc.Desc_1_0.NumParameters = kTotalParamCount;

    ID3DBlob *root_sig_blob_ptr = nullptr, *error_blob_ptr = nullptr;
    JD3D12_LOG_AND_RETURN_IF_FAILED(D3D12SerializeVersionedRootSignature(&root_sig_desc, &root_sig_blob_ptr, &error_blob_ptr));
    CComPtr<ID3DBlob> root_sig_blob{root_sig_blob_ptr};
    CComPtr<ID3DBlob> error_blob{error_blob_ptr};

    JD3D12_LOG_AND_RETURN_IF_FAILED(GetD3d12Device()->CreateRootSignature(0, root_sig_blob->GetBufferPointer(),
        root_sig_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));
    SetObjectName(root_signature_, L"Main root signature");

    return kSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// class BindingState

void BindingState::ResetDescriptors()
{
    for(uint32_t slot = 0; slot < MainRootSignature::kMaxCBVCount; ++slot)
    {
        cbv_bindings_[slot].descriptor_index = UINT32_MAX;
    }
    for(uint32_t slot = 0; slot < MainRootSignature::kMaxSRVCount; ++slot)
    {
        srv_bindings_[slot].descriptor_index = UINT32_MAX;
    }
    for(uint32_t slot = 0; slot < MainRootSignature::kMaxUAVCount; ++slot)
    {
        uav_bindings_[slot].descriptor_index = UINT32_MAX;
    }
}

bool BindingState::IsBufferBound(BufferImpl* buf)
{
    for(uint32_t slot = 0; slot < MainRootSignature::kMaxCBVCount; ++slot)
    {
        if(cbv_bindings_[slot].buffer == buf)
            return true;
    }
    for(uint32_t slot = 0; slot < MainRootSignature::kMaxSRVCount; ++slot)
    {
        if(srv_bindings_[slot].buffer == buf)
            return true;
    }
    for(uint32_t slot = 0; slot < MainRootSignature::kMaxUAVCount; ++slot)
    {
        if(uav_bindings_[slot].buffer == buf)
            return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// class ShaderCompilationResultImpl

ShaderCompilationResultImpl::ShaderCompilationResultImpl(ShaderCompilationResult* interface_obj, EnvironmentImpl* env)
    : interface_obj_{ interface_obj }
    , env_{ env }
{
}

Result ShaderCompilationResultImpl::Init(CComPtr<IDxcResult>&& dxc_result)
{
    dxc_result_ = std::move(dxc_result);

    dxc_result_->GetStatus(&status_);

    if(FAILED(dxc_result_->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors_), nullptr)))
        errors_.Release();

    if(FAILED(dxc_result_->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object_), nullptr)))
        object_.Release();

    return kSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// class DeviceImpl

DeviceImpl::DeviceImpl(Device* interface_obj, EnvironmentImpl* env, const DeviceDesc& desc)
    : DeviceObject{ this, desc }
    , interface_obj_{ interface_obj }
    , env_{ env }
    , desc_{ desc }
    , main_root_signature_{ std::make_unique<MainRootSignature>(this) }
    , shader_visible_descriptor_heap_{ this, desc.name, true }
    , shader_invisible_descriptor_heap_{ this, desc.name, false }
{
    JD3D12_ASSERT(env);

    Singleton& singleton = Singleton::GetInstance();
    if(singleton.dev_count_ == 0)
    {
        singleton.first_dev_ = interface_obj;
        singleton.dev_count_ = 1;
    }
    else
    {
        ++singleton.dev_count_;
    }
}

DeviceImpl::~DeviceImpl()
{
    if(command_list_)
    {
        HRESULT hr = EnsureCommandListState(CommandListState::kNone);
        JD3D12_ASSERT(SUCCEEDED(hr) && "Failed to process pending command list in Device destructor.");
    }

    DestroyStaticShaders();
    DestroyStaticBuffers();

    // Log device destroy only after static resources have been destroyed.
    JD3D12_LOG(kLogSeverityInfo, L"Destroying Device 0x%016" PRIXPTR, uintptr_t(GetInterface()));

    JD3D12_ASSERT(buffer_count_ == 0 && "Destroying Device object while there are still Buffer objects not destroyed.");
    JD3D12_ASSERT(shader_count_ == 0 && "Destroying Device object while there are still Shader objects not destroyed.");

    if(info_queue_ && debug_layer_callback_cookie_ != UINT32_MAX)
    {
        info_queue_->UnregisterMessageCallback(debug_layer_callback_cookie_);
    }

    Singleton& singleton = Singleton::GetInstance();
    if(singleton.dev_count_ == 1)
    {
        singleton.first_dev_ = nullptr;
        singleton.dev_count_ = 0;
    }
    else
    {
        --singleton.dev_count_;
    }
}

Result DeviceImpl::CreateBuffer(const BufferDesc& desc, Buffer*& out_buffer)
{
    return CreateBufferFromMemory(desc, ConstDataSpan{nullptr, 0}, out_buffer);
}

Result DeviceImpl::CreateBufferFromMemory(const BufferDesc& desc, ConstDataSpan initial_data,
    Buffer*& out_buffer)
{
    out_buffer = nullptr;

    auto buf = std::unique_ptr<Buffer>{new Buffer{}};
    buf->impl_ = new BufferImpl{ buf.get(), this, desc };

    if(initial_data.size > 0)
    {
        JD3D12_LOG(kLogSeverityInfo, L"Creating Buffer 0x%016" PRIXPTR " \"%s\": flags=0x%X, size=%zu, initial_data.size=%zu",
            uintptr_t(buf.get()), EnsureNonNullString(desc.name), desc.flags, desc.size, initial_data.size);
    }
    else
    {
        JD3D12_LOG(kLogSeverityInfo, L"Creating Buffer 0x%016" PRIXPTR " \"%s\": flags=0x%X, size=%zu",
            uintptr_t(buf.get()), EnsureNonNullString(desc.name), desc.flags, desc.size);
    }

    JD3D12_RETURN_IF_FAILED(buf->GetImpl()->Init(initial_data));

    out_buffer = buf.release();
    return kSuccess;
}

Result DeviceImpl::CreateBufferFromFile(const BufferDesc& desc, const wchar_t* initial_data_file_path,
    Buffer*& out_buffer)
{
    JD3D12_ASSERT_OR_RETURN(!IsStringEmpty(initial_data_file_path),
        L"initial_data_file_path cannot be null or empty.");

    JD3D12_LOG(kLogSeverityInfo, L"Loading buffer initial data from file \"%s\"",
        initial_data_file_path);

    char* data_ptr = nullptr;
    size_t data_size = 0;
    JD3D12_LOG_AND_RETURN_IF_FAILED(LoadFile(initial_data_file_path, data_ptr, data_size, desc.size));
    std::unique_ptr<char[]> data{ data_ptr };

    return CreateBufferFromMemory(desc, ConstDataSpan{data_ptr, data_size}, out_buffer);
}

Result DeviceImpl::CreateShaderFromMemory(const ShaderDesc& desc, ConstDataSpan bytecode,
    Shader*& out_shader)
{
    out_shader = nullptr;

    JD3D12_ASSERT_OR_RETURN(bytecode.data != nullptr && bytecode.size > 0,
        L"Shader bytecode cannot be null or empty.");

    CComPtr<ID3D12ShaderReflection> reflection;
    DxcBuffer buf = {bytecode.data, bytecode.size};
    JD3D12_LOG_AND_RETURN_IF_FAILED(env_->GetShaderCompiler().GetDxcUtils()->CreateReflection(
        &buf, IID_PPV_ARGS(&reflection)));

    auto shader = std::unique_ptr<Shader>{new Shader{}};
    shader->impl_ = new ShaderImpl{ shader.get(), this, desc };

    JD3D12_LOG(kLogSeverityInfo, L"Creating Shader 0x%016" PRIXPTR " \"%s\"",
        uintptr_t(shader.get()), EnsureNonNullString(desc.name));

    JD3D12_RETURN_IF_FAILED(shader->GetImpl()->Init(bytecode, reflection));

    out_shader = shader.release();
    return kSuccess;
}

Result DeviceImpl::CreateShaderFromFile(const ShaderDesc& desc, const wchar_t* bytecode_file_path,
    Shader*& out_shader)
{
    JD3D12_ASSERT_OR_RETURN(!IsStringEmpty(bytecode_file_path),
        L"bytecode_file_path cannot be null or empty.");

    JD3D12_LOG(kLogSeverityInfo, L"Loading shader bytecode from file \"%s\"", bytecode_file_path);

    char* data_ptr = nullptr;
    size_t data_size = 0;
    JD3D12_LOG_AND_RETURN_IF_FAILED(LoadFile(bytecode_file_path, data_ptr, data_size));

    if(data_ptr == nullptr || data_size == 0)
        return kErrorUnexpected;

    std::unique_ptr<char[]> data{ data_ptr };

    return CreateShaderFromMemory(desc, ConstDataSpan{data_ptr, data_size}, out_shader);
}

Result DeviceImpl::CompileAndCreateShaderFromMemory(const ShaderCompilationParams& compilation_params,
    const ShaderDesc& desc, ConstDataSpan hlsl_source, Shader*& out_shader)
{
    ShaderCompilationResult* result_ptr = nullptr;
    JD3D12_RETURN_IF_FAILED(env_->CompileShaderFromMemory(compilation_params, L"shader_from_memory.hlsl",
        hlsl_source, result_ptr));
    std::unique_ptr<ShaderCompilationResult> result{ result_ptr };

    JD3D12_LOG_AND_RETURN_IF_FAILED(result->GetResult());

    const ConstDataSpan bytecode = result->GetBytecode();
    if(bytecode.size == 0)
        return kErrorFail;

    return CreateShaderFromMemory(desc, bytecode, out_shader);
}

Result DeviceImpl::CompileAndCreateShaderFromFile(const ShaderCompilationParams& compilation_params,
    const ShaderDesc& desc, const wchar_t* hlsl_source_file_path, Shader*& out_shader)
{
    ShaderCompilationResult* result_ptr = nullptr;
    JD3D12_RETURN_IF_FAILED(env_->CompileShaderFromFile(compilation_params, hlsl_source_file_path,
        result_ptr));
    std::unique_ptr<ShaderCompilationResult> shader_compilation_result{ result_ptr };

    JD3D12_LOG_AND_RETURN_IF_FAILED(shader_compilation_result->GetResult());

    const ConstDataSpan bytecode = shader_compilation_result->GetBytecode();
    if(bytecode.size == 0)
        return kErrorFail;

    return CreateShaderFromMemory(desc, bytecode, out_shader);
}

Result DeviceImpl::MapBuffer(BufferImpl& buf, Range byte_range, BufferFlags cpu_usage_flag, void*& out_data_ptr,
    uint32_t command_flags)
{
    out_data_ptr = nullptr;
    JD3D12_ASSERT_OR_RETURN(buf.GetDevice() == this,
        L"Buffer does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN(!buf.is_user_mapped_,
        L"Device::MapBuffer called twice. Nested mapping is not supported.");
    JD3D12_ASSERT_OR_RETURN(buf.persistently_mapped_ptr_ != nullptr,
        L"Cannot map this buffer.");
    JD3D12_ASSERT_OR_RETURN(CountBitsSet(cpu_usage_flag &
        (kBufferUsageFlagCpuSequentialWrite | kBufferUsageFlagCpuRead)) == 1,
        L"cpu_usage_flag must be kBufferUsageFlagCpuSequentialWrite or kBufferUsageFlagCpuRead.");
    JD3D12_ASSERT_OR_RETURN((buf.desc_.flags & cpu_usage_flag) == cpu_usage_flag,
        L"Buffer was not created with the kBufferUsageFlagCpu* used for mapping.");
    const bool is_reading = (buf.desc_.flags & kBufferUsageFlagCpuRead) != 0;
    const bool is_writing = (buf.desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0;

    byte_range = LimitRange(byte_range, buf.desc_.size);
    JD3D12_ASSERT_OR_RETURN(byte_range.count > 0, L"byte_range is empty.");
    JD3D12_ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf.GetSize(), L"byte_range out of bounds.");

    // If the buffer is being written or read to in the current command list, execute it and wait for it to finish.
    const uint32_t conflicting_usage_flags = is_writing
        ? (kResourceUsageFlagWrite | kResourceUsageFlagRead)
        : kResourceUsageFlagWrite;
    if(resource_usage_map_.IsUsed(&buf, conflicting_usage_flags))
    {
        const uint32_t timeout = (command_flags & kCommandFlagDontWait) ? 0 : kTimeoutInfinite;
        const Result res = EnsureCommandListState(CommandListState::kNone, timeout);
        if(res != kSuccess)
            return res;
    }

    buf.is_user_mapped_ = true;

    out_data_ptr = (char*)buf.persistently_mapped_ptr_ + byte_range.first;
    return kSuccess;
}

void DeviceImpl::UnmapBuffer(BufferImpl& buf)
{
    JD3D12_ASSERT(buf.GetDevice() == this && "Buffer does not belong to this Device.");
    JD3D12_ASSERT(buf.is_user_mapped_ && "Device::UnmapBuffer called but the buffer wasn't mapped.");

    buf.is_user_mapped_ = false;
}

Result DeviceImpl::ReadBufferToMemory(BufferImpl& src_buf, Range src_byte_range, void* dst_memory,
    uint32_t command_flags)
{
    JD3D12_ASSERT_OR_RETURN(src_buf.GetDevice() == this, L"Buffer does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN(!src_buf.is_user_mapped_, L"Cannot call this command while the buffer is mapped.");

    src_byte_range = LimitRange(src_byte_range, src_buf.GetSize());
    if(src_byte_range.count == 0)
        return kFalse;

    JD3D12_ASSERT_OR_RETURN(dst_memory != nullptr, L"dst_memory cannot be null.");
    JD3D12_ASSERT_OR_RETURN(src_byte_range.first < src_buf.GetSize()
        && src_byte_range.first + src_byte_range.count <= src_buf.GetSize(),
        L"Source buffer region out of bounds.");

    // If the buffer is being written to in the current command list, execute it and wait for it to finish.
    // If it is only read, there is no hazard.
    if(resource_usage_map_.IsUsed(&src_buf, kResourceUsageFlagWrite))
    {
        const uint32_t timeout = (command_flags & kCommandFlagDontWait) ? 0 : kTimeoutInfinite;
        const Result res = EnsureCommandListState(CommandListState::kNone, timeout);
        if(res == kNotReady)
            return res;
        JD3D12_RETURN_IF_FAILED(res);
    }

    void* mapped_ptr = nullptr;
    HRESULT hr = MapBuffer(src_buf, src_byte_range, kBufferUsageFlagCpuRead, mapped_ptr);
    if(FAILED(hr))
        return hr;
    memcpy(dst_memory, (char*)mapped_ptr, src_byte_range.count);
    UnmapBuffer(src_buf);
    return kSuccess;
}

Result DeviceImpl::WriteMemoryToBuffer(ConstDataSpan src_data, BufferImpl& dst_buf, size_t dst_byte_offset,
    uint32_t command_flags)
{
    JD3D12_ASSERT_OR_RETURN(dst_buf.GetDevice() == this, L"Buffer does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN(!dst_buf.is_user_mapped_, L"Cannot call this command while the buffer is mapped.");

    if(src_data.size == 0)
        return kFalse;

    JD3D12_ASSERT_OR_RETURN(src_data.data != nullptr, L"src_memory cannot be null.");
    JD3D12_ASSERT_OR_RETURN(src_data.size % 4 == 0, L"src_data.size must be a multiple of 4 B.");
    JD3D12_ASSERT_OR_RETURN(dst_byte_offset < dst_buf.GetSize()
        && dst_byte_offset + src_data.size <= dst_buf.GetSize(),
        L"Destination buffer region out of bounds.");

    if(dst_buf.strategy_ == BufferStrategy::kUpload)
    {
        // Use MapBuffer.

        // If the buffer is being written or read to in the current command list, execute it and wait for it to finish.
        if(resource_usage_map_.IsUsed(&dst_buf, kResourceUsageFlagWrite | kResourceUsageFlagRead))
        {
            const uint32_t timeout = (command_flags & kCommandFlagDontWait) ? 0 : kTimeoutInfinite;
            const Result res = EnsureCommandListState(CommandListState::kNone, timeout);
            if(res != kSuccess)
                return res;
        }

        void* mapped_ptr = nullptr;
        HRESULT hr = MapBuffer(dst_buf, Range{dst_byte_offset, src_data.size},
            kBufferUsageFlagCpuSequentialWrite, mapped_ptr);
        if(FAILED(hr))
            return hr;
        memcpy((char*)mapped_ptr, src_data.data, src_data.size);
        UnmapBuffer(dst_buf);
        return kSuccess;
    }
    else if(dst_buf.strategy_ == BufferStrategy::kDefault)
    {
        // Use WriteBufferImmediate.

        JD3D12_ASSERT_OR_RETURN(src_data.size <= 0x10000,
            L"Writing to buffers in GPU memory is currently limited to 64 KB per call. It will be improved in the future.");

        const uint32_t timeout = (command_flags & kCommandFlagDontWait) ? 0 : kTimeoutInfinite;
        const Result res = EnsureCommandListState(CommandListState::kRecording, timeout);
        if(res != kSuccess)
            return res;

        JD3D12_RETURN_IF_FAILED(UseBuffer(dst_buf, D3D12_RESOURCE_STATE_COPY_DEST));

        const uint32_t param_count = uint32_t(src_data.size / 4);
        StackOrHeapVector<D3D12_WRITEBUFFERIMMEDIATE_PARAMETER, 8> params{param_count};
        {
            const uint32_t* src_data_u32 = reinterpret_cast<const uint32_t*>(src_data.data);
            D3D12_GPU_VIRTUAL_ADDRESS dst_gpu_address = dst_buf.GetD3D12Resource()->GetGPUVirtualAddress();
            for(uint32_t param_index = 0; param_index < param_count
                ; ++param_index, ++src_data_u32, dst_gpu_address += sizeof(uint32_t))
            {
                params[param_index] = D3D12_WRITEBUFFERIMMEDIATE_PARAMETER{ dst_gpu_address, *src_data_u32 };
            }
            command_list_->WriteBufferImmediate(param_count, params.GetData(), nullptr);
        }
        return kSuccess;
    }
    else
    {
        JD3D12_ASSERT(0);
        return kErrorUnexpected;
    }
}

Result DeviceImpl::SubmitPendingCommands()
{
    if(command_list_state_ == CommandListState::kRecording)
        JD3D12_RETURN_IF_FAILED(ExecuteRecordedCommands());
    return kSuccess;
}

Result DeviceImpl::WaitForGPU(uint32_t timeout_milliseconds)
{
    JD3D12_RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kNone, timeout_milliseconds));
    return kSuccess;
}

Result DeviceImpl::CopyBuffer(BufferImpl& src_buf, BufferImpl& dst_buf)
{
    JD3D12_ASSERT_OR_RETURN(src_buf.GetDevice() == this, L"src_buf does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN(dst_buf.GetDevice() == this, L"dst_buf does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN((src_buf.desc_.flags & kBufferUsageFlagCopySrc) != 0,
        L"src_buf was not created with kBufferUsageFlagCopySource.");
    JD3D12_ASSERT_OR_RETURN((dst_buf.desc_.flags & kBufferUsageFlagCopyDst) != 0,
        L"dst_buf was not created with kBufferUsageFlagCopyDest.");
    JD3D12_ASSERT_OR_RETURN(src_buf.GetSize() == dst_buf.GetSize(),
        L"Source and destination buffers must have the same size.");

    JD3D12_RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kRecording));

    JD3D12_RETURN_IF_FAILED(UseBuffer(src_buf, D3D12_RESOURCE_STATE_COPY_SOURCE));
    JD3D12_RETURN_IF_FAILED(UseBuffer(dst_buf, D3D12_RESOURCE_STATE_COPY_DEST));

    command_list_->CopyResource(dst_buf.GetD3D12Resource(), src_buf.GetD3D12Resource());

    return kSuccess;
}

Result DeviceImpl::CopyBufferRegion(BufferImpl& src_buf, Range src_byte_range, BufferImpl& dst_buf, size_t dst_byte_offset)
{
    JD3D12_ASSERT_OR_RETURN(src_buf.GetDevice() == this, L"src_buf does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN(dst_buf.GetDevice() == this, L"dst_buf does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN((src_buf.desc_.flags & kBufferUsageFlagCopySrc) != 0,
        L"src_buf was not created with kBufferUsageFlagCopySource.");
    JD3D12_ASSERT_OR_RETURN((dst_buf.desc_.flags & kBufferUsageFlagCopyDst) != 0,
        L"dst_buf was not created with kBufferUsageFlagCopyDest.");
    src_byte_range = LimitRange(src_byte_range, src_buf.GetSize());
    JD3D12_ASSERT_OR_RETURN(src_byte_range.count >= 0 && src_byte_range.count % 4 == 0, L"Size must be non-zero and a multiple of 4.");
    JD3D12_ASSERT_OR_RETURN(src_byte_range.first + src_byte_range.count <= src_buf.GetSize(), L"Source buffer overflow.");
    JD3D12_ASSERT_OR_RETURN(dst_byte_offset + src_byte_range.count <= dst_buf.GetSize(), L"Destination buffer overflow.");

    JD3D12_RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kRecording));

    JD3D12_RETURN_IF_FAILED(UseBuffer(src_buf, D3D12_RESOURCE_STATE_COPY_SOURCE));
    JD3D12_RETURN_IF_FAILED(UseBuffer(dst_buf, D3D12_RESOURCE_STATE_COPY_DEST));

    command_list_->CopyBufferRegion(dst_buf.GetD3D12Resource(), dst_byte_offset,
        src_buf.GetD3D12Resource(), src_byte_range.first, src_byte_range.count);

    return kSuccess;
}

Result DeviceImpl::ClearBufferToUintValues(BufferImpl& buf, const UintVec4& values, Range element_range)
{
    JD3D12_ASSERT_OR_RETURN(buf.GetDevice() == this,
        L"ClearBufferToUintValues: Buffer does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN((buf.desc_.flags & kBufferUsageFlagShaderRWResource) != 0,
        L"ClearBufferToUintValues: Buffer was not created with kBufferUsageFlagShaderRWResource.");
    JD3D12_ASSERT_OR_RETURN((buf.desc_.flags & (kBufferFlagTyped | kBufferFlagByteAddress)) != 0,
        L"ClearBufferToUintValues: Buffer was not created with kBufferFlagTyped or kBufferFlagByteAddress.");

    if(element_range.count == 0)
        return kFalse;

    D3D12_GPU_DESCRIPTOR_HANDLE shader_visible_gpu_desc_handle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE shader_invisible_cpu_desc_handle = {};
    JD3D12_RETURN_IF_FAILED(BeginClearBufferToValues(buf, element_range,
        shader_visible_gpu_desc_handle, shader_invisible_cpu_desc_handle));

    command_list_->ClearUnorderedAccessViewUint(
        shader_visible_gpu_desc_handle, // ViewGPUHandleInCurrentHeap
        shader_invisible_cpu_desc_handle, // ViewCPUHandle
        buf.GetD3D12Resource(), // pResource
        &values.x, // Values
        0, // NumRects
        nullptr); // pRects

    return kSuccess;
}

Result DeviceImpl::ClearBufferToFloatValues(BufferImpl& buf, const FloatVec4& values, Range element_range)
{
    JD3D12_ASSERT_OR_RETURN(buf.GetDevice() == this,
        L"ClearBufferToFloatValues: Buffer does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN((buf.desc_.flags & kBufferUsageFlagShaderRWResource) != 0,
        L"ClearBufferToFloatValues: Buffer was not created with kBufferUsageFlagShaderRWResource.");
    JD3D12_ASSERT_OR_RETURN((buf.desc_.flags & kBufferFlagTyped) != 0,
        L"ClearBufferToFloatValues: Buffer was not created with kBufferFlagTyped.");

    if(element_range.count == 0)
        return kFalse;

    D3D12_GPU_DESCRIPTOR_HANDLE shader_visible_gpu_desc_handle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE shader_invisible_cpu_desc_handle = {};
    JD3D12_RETURN_IF_FAILED(BeginClearBufferToValues(buf, element_range,
        shader_visible_gpu_desc_handle, shader_invisible_cpu_desc_handle));

    command_list_->ClearUnorderedAccessViewFloat(
        shader_visible_gpu_desc_handle, // ViewGPUHandleInCurrentHeap
        shader_invisible_cpu_desc_handle, // ViewCPUHandle
        buf.GetD3D12Resource(), // pResource
        &values.x, // Values
        0, // NumRects
        nullptr); // pRects

    return kSuccess;
}

void DeviceImpl::ResetAllBindings()
{
    for(uint32_t slot = 0; slot < MainRootSignature::kMaxCBVCount; ++slot)
    {
        binding_state_.cbv_bindings_[slot] = Binding{};
    }
    for(uint32_t slot = 0; slot < MainRootSignature::kMaxSRVCount; ++slot)
    {
        binding_state_.srv_bindings_[slot] = Binding{};
    }
    for(uint32_t slot = 0; slot < MainRootSignature::kMaxUAVCount; ++slot)
    {
        binding_state_.uav_bindings_[slot] = Binding{};
    }
}

Result DeviceImpl::BindConstantBuffer(uint32_t b_slot, BufferImpl* buf, Range byte_range)
{
    JD3D12_ASSERT_OR_RETURN(b_slot < MainRootSignature::kMaxCBVCount, L"CBV slot out of bounds.");

    if(buf != nullptr)
        byte_range = LimitRange(byte_range, buf->GetSize());
    else
        byte_range = kEmptyRange;

    JD3D12_ASSERT_OR_RETURN(byte_range.first % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0
        && byte_range.count % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
        L"Constant buffer offset and size must be a multiple of 256 B.");

    Binding* binding = &binding_state_.cbv_bindings_[b_slot];
    if(binding->buffer == buf && binding->byte_range == byte_range)
        return kFalse;

    if(buf == nullptr)
    {
        *binding = Binding{};
        return kSuccess;
    }

    size_t alignment = buf->GetElementSize();
    if(alignment == 0)
        alignment = 4;

    JD3D12_ASSERT_OR_RETURN(byte_range.first % alignment == 0, L"Buffer offset must be a multiple of element size.");
    JD3D12_ASSERT_OR_RETURN(byte_range.count > 0 && byte_range.count % alignment == 0,
        L"Size must be greater than zero and a multiple of element size.");
    JD3D12_ASSERT_OR_RETURN(buf->GetDevice() == this, L"Buffer does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN((buf->desc_.flags & kBufferUsageFlagShaderConstant) != 0,
        L"BindConstantBuffer: Buffer was not created with kBufferUsageFlagShaderConstant.");
    JD3D12_ASSERT_OR_RETURN(byte_range.first < buf->GetSize(), L"Buffer offset out of bounds.");
    JD3D12_ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf->GetSize(), L"Buffer region out of bounds.");

    binding->buffer = buf;
    binding->byte_range = byte_range;
    binding->descriptor_index = UINT32_MAX;

    return kSuccess;
}

Result DeviceImpl::BindBuffer(uint32_t t_slot, BufferImpl* buf, Range byte_range)
{
    JD3D12_ASSERT_OR_RETURN(t_slot < MainRootSignature::kMaxSRVCount, L"SRV slot out of bounds.");

    if(buf != nullptr)
        byte_range = LimitRange(byte_range, buf->GetSize());
    else
        byte_range = kEmptyRange;

    Binding* binding = &binding_state_.srv_bindings_[t_slot];
    if(binding->buffer == buf && binding->byte_range == byte_range)
        return kFalse;

    if(buf == nullptr)
    {
        *binding = Binding{};
        return kSuccess;
    }

    const uint32_t type_bit_count = CountBitsSet(buf->desc_.flags
        & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress));
    JD3D12_ASSERT_OR_RETURN(type_bit_count == 1,
        L"Buffer must be one of: kBufferFlagTyped, kBufferFlagStructured, kBufferFlagByteAddress.");

    size_t alignment = buf->GetElementSize();
    if(alignment == 0)
        alignment = 4;

    JD3D12_ASSERT_OR_RETURN(byte_range.first % alignment == 0, L"Buffer offset must be a multiple of element size.");
    JD3D12_ASSERT_OR_RETURN(byte_range.count > 0 && byte_range.count % alignment == 0,
        L"Size must be greater than zero and a multiple of element size.");
    JD3D12_ASSERT_OR_RETURN(buf->GetDevice() == this, L"Buffer does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN((buf->desc_.flags & kBufferUsageFlagShaderResource) != 0,
        L"BindBuffer: Buffer was not created with kBufferUsageFlagGpuShaderResource.");
    JD3D12_ASSERT_OR_RETURN(byte_range.first < buf->GetSize(), L"Buffer offset out of bounds.");
    JD3D12_ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf->GetSize(), L"Buffer region out of bounds.");

    binding->buffer = buf;
    binding->byte_range = byte_range;
    binding->descriptor_index = UINT32_MAX;

    return kSuccess;
}

Result DeviceImpl::BindRWBuffer(uint32_t u_slot, BufferImpl* buf, Range byte_range)
{
    JD3D12_ASSERT_OR_RETURN(u_slot < MainRootSignature::kMaxUAVCount, L"UAV slot out of bounds.");

    if(buf != nullptr)
        byte_range = LimitRange(byte_range, buf->GetSize());
    else
        byte_range = kEmptyRange;

    Binding* binding = &binding_state_.uav_bindings_[u_slot];
    if(binding->buffer == buf && binding->byte_range == byte_range)
        return kFalse;

    if(buf == nullptr)
    {
        *binding = Binding{};
        return kSuccess;
    }

    const uint32_t type_bit_count = CountBitsSet(buf->desc_.flags
        & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress));
    JD3D12_ASSERT_OR_RETURN(type_bit_count == 1,
        L"Buffer must be one of: kBufferFlagTyped, kBufferFlagStructured, kBufferFlagByteAddress.");

    size_t alignment = buf->GetElementSize();
    if(alignment == 0)
        alignment = 4;
    JD3D12_ASSERT_OR_RETURN(byte_range.first % alignment == 0, L"Buffer offset must be a multiple of element size.");
    JD3D12_ASSERT_OR_RETURN(byte_range.count > 0 && byte_range.count % alignment == 0,
        L"Size must be greater than zero and a multiple of element size.");
    JD3D12_ASSERT_OR_RETURN(buf->GetDevice() == this, L"Buffer does not belong to this Device.");
    JD3D12_ASSERT_OR_RETURN((buf->desc_.flags & kBufferUsageFlagShaderRWResource) != 0,
        L"BindRWBuffer: Buffer was not created with kBufferUsageFlagShaderRWResource.");
    JD3D12_ASSERT_OR_RETURN(byte_range.first < buf->GetSize(), L"Buffer offset out of bounds.");
    JD3D12_ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf->GetSize(), L"Buffer region out of bounds.");

    binding->buffer = buf;
    binding->byte_range = byte_range;
    binding->descriptor_index = UINT32_MAX;

    return kSuccess;
}

Result DeviceImpl::Init(bool enable_d3d12_debug_layer)
{
    DXGI_ADAPTER_DESC adapter_desc = {};
    {
        env_->GetDXGIAdapter1()->GetDesc(&adapter_desc);
        // Ignoring the result.
    }

    JD3D12_LOG(kLogSeverityInfo, L"Creating Device 0x%016" PRIXPTR " \"%s\": flags=0x%X for GPU \"%s\"",
        uintptr_t(GetInterface()),
        EnsureNonNullString(desc_.name), desc_.flags, adapter_desc.Description);

    JD3D12_LOG_AND_RETURN_IF_FAILED(env_->GetD3D12DeviceFactory()->CreateDevice(env_->GetDXGIAdapter1(),
        D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device_)));

    if(enable_d3d12_debug_layer)
    {
        JD3D12_RETURN_IF_FAILED(EnableDebugLayer());
    }

    HRESULT hr = device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &options16_, sizeof(options16_));
    if(FAILED(hr))
        options16_ = {};

    if (!IsStringEmpty(desc_.name))
        device_->SetName(desc_.name);

    D3D12_COMMAND_QUEUE_DESC cmd_queue_desc{};
    cmd_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    cmd_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    if((desc_.flags & kDeviceFlagDisableGpuTimeout) != 0)
        cmd_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
    JD3D12_LOG_AND_RETURN_IF_FAILED(device_->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&command_queue_)));
    SetObjectName(command_queue_, desc_.name, L"CommandQueue");

    JD3D12_LOG_AND_RETURN_IF_FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
        IID_PPV_ARGS(&command_allocator_)));
    SetObjectName(command_allocator_, desc_.name, L"CommandAllocator");

    JD3D12_LOG_AND_RETURN_IF_FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
        command_allocator_, nullptr, IID_PPV_ARGS(&command_list_)));
    SetObjectName(command_list_, desc_.name, L"CommandList");

    JD3D12_LOG_AND_RETURN_IF_FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
    SetObjectName(fence_, desc_.name, L"Fence");

    HANDLE fence_event_handle = NULL;
    fence_event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    fence_event_.reset(fence_event_handle);

    JD3D12_RETURN_IF_FAILED(main_root_signature_->Init());

    JD3D12_RETURN_IF_FAILED(shader_visible_descriptor_heap_.Init(desc_.name));
    JD3D12_RETURN_IF_FAILED(shader_invisible_descriptor_heap_.Init(desc_.name));
    JD3D12_RETURN_IF_FAILED(CreateNullDescriptors());

    JD3D12_RETURN_IF_FAILED(CreateStaticBuffers());
    JD3D12_RETURN_IF_FAILED(CreateStaticShaders());

    desc_.name = nullptr;
    return kSuccess;
}

Result DeviceImpl::EnableDebugLayer()
{
    HRESULT hr = device_->QueryInterface(IID_PPV_ARGS(&info_queue_));
    // QueryInterface for ID3D12InfoQueue1 or ID3D12InfoQueue fails unless the Debug Layer
    // is force-enabled externally using "dxcpl" app. I don't know why.
    if(SUCCEEDED(hr))
    {
        info_queue_->RegisterMessageCallback(StaticDebugLayerMessageCallback,
            D3D12_MESSAGE_CALLBACK_FLAG_NONE, this, &debug_layer_callback_cookie_);
    }

    return kSuccess;
}

void DeviceImpl::DebugLayerMessageCallback(
    D3D12_MESSAGE_CATEGORY Category,
    D3D12_MESSAGE_SEVERITY Severity,
    D3D12_MESSAGE_ID ID,
    LPCSTR pDescription)
{
    const LogSeverity log_severity = D3d12MessageSeverityToLogSeverity(Severity);
    GetLogger()->LogF(log_severity, L"%S [%s #%u]", pDescription,
        GetD3d12MessageCategoryString(Category), uint32_t(ID));
}

Result DeviceImpl::ExecuteRecordedCommands()
{
    JD3D12_ASSERT(command_list_state_ == CommandListState::kRecording);

    JD3D12_LOG_AND_RETURN_IF_FAILED(command_list_->Close());

    ID3D12CommandList* command_lists[] = { command_list_ };
    command_queue_->ExecuteCommandLists(1, command_lists);

    ++submitted_fence_value_;
    JD3D12_LOG_AND_RETURN_IF_FAILED(command_queue_->Signal(fence_, submitted_fence_value_));

    command_list_state_ = CommandListState::kExecuting;

    return kSuccess;
}

Result DeviceImpl::WaitForCommandExecution(uint32_t timeout_milliseconds)
{
    JD3D12_ASSERT(command_list_state_ == CommandListState::kExecuting);

    if(fence_->GetCompletedValue() < submitted_fence_value_)
    {
        JD3D12_LOG_AND_RETURN_IF_FAILED(fence_->SetEventOnCompletion(submitted_fence_value_, fence_event_.get()));
        const DWORD wait_result = WaitForSingleObject(fence_event_.get(), timeout_milliseconds);
        switch(wait_result)
        {
        case WAIT_OBJECT_0:
            // Waiting succeeded, event was is signaled (and got automatically reset to unsignaled).
            break;
        case WAIT_TIMEOUT:
            return kNotReady;
        default: // Most likely WAIT_FAILED.
            return MakeResultFromLastError();
        }
    }

    command_list_state_ = CommandListState::kNone;
    resource_usage_map_.map_.clear();
    shader_usage_set_.clear();

    return kSuccess;
}

Result DeviceImpl::ResetCommandListForRecording()
{
    JD3D12_ASSERT(command_list_state_ == CommandListState::kNone);

    binding_state_.ResetDescriptors();
    shader_invisible_descriptor_heap_.ClearDynamic();
    shader_visible_descriptor_heap_.ClearDynamic();

    JD3D12_LOG_AND_RETURN_IF_FAILED(command_allocator_->Reset());
    JD3D12_LOG_AND_RETURN_IF_FAILED(command_list_->Reset(command_allocator_, nullptr));

    command_list_state_ = CommandListState::kRecording;

    return kSuccess;
}

Result DeviceImpl::EnsureCommandListState(CommandListState desired_state, uint32_t timeout_milliseconds)
{
    if(desired_state == command_list_state_)
        return kSuccess;
    if(command_list_state_ == CommandListState::kRecording)
        JD3D12_RETURN_IF_FAILED(ExecuteRecordedCommands());
    if(desired_state == command_list_state_)
        return kSuccess;
    if(command_list_state_ == CommandListState::kExecuting)
        JD3D12_RETURN_IF_FAILED(WaitForCommandExecution(timeout_milliseconds));
    if(desired_state == command_list_state_)
        return kSuccess;
    if(command_list_state_ == CommandListState::kNone)
        JD3D12_RETURN_IF_FAILED(ResetCommandListForRecording());
    return kSuccess;
}

Result DeviceImpl::WaitForBufferUnused(BufferImpl* buf)
{
    JD3D12_ASSERT_OR_RETURN(!binding_state_.IsBufferBound(buf), L"Buffer is still bound.");

    if(resource_usage_map_.map_.find(buf) != resource_usage_map_.map_.end())
        return EnsureCommandListState(CommandListState::kNone);
    return kSuccess;
}

Result DeviceImpl::WaitForShaderUnused(ShaderImpl* shader)
{
    if(shader_usage_set_.find(shader) != shader_usage_set_.end())
        return EnsureCommandListState(CommandListState::kNone);
    return kSuccess;
}

Result DeviceImpl::UseBuffer(BufferImpl& buf, D3D12_RESOURCE_STATES state)
{
    JD3D12_ASSERT(command_list_state_ == CommandListState::kRecording);

    JD3D12_ASSERT_OR_RETURN(!buf.is_user_mapped_, L"Cannot use a buffer on the GPU while it is mapped.");

    uint32_t usage_flags = kResourceUsageFlagRead;
    switch(state)
    {
    case D3D12_RESOURCE_STATE_COPY_SOURCE:
    case D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:
    case D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER:
        break;
    case D3D12_RESOURCE_STATE_COPY_DEST:
    case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
        usage_flags |= kResourceUsageFlagWrite;
        break;
    default:
        JD3D12_ASSERT(0);
    }

    const auto it = resource_usage_map_.map_.find(&buf);
    // Buffer wasn't used in this command list before.
    // No barrier issued - rely on automatic state promotion.
    if(it == resource_usage_map_.map_.end())
    {
        resource_usage_map_.map_.emplace(&buf, ResourceUsage{usage_flags, state});
        return kSuccess;
    }

    // Use barriers only in DEAFULT heap type.
    if(buf.strategy_ == BufferStrategy::kDefault)
    {
        // Transition the state if necessary.
        if(state != it->second.last_state)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = buf.GetD3D12Resource();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = it->second.last_state;
            barrier.Transition.StateAfter = state;
            command_list_->ResourceBarrier(1, &barrier);
        }
        // UAV barrier if necessary.
        else if(state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS &&
            it->second.last_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.UAV.pResource = buf.GetD3D12Resource();
            command_list_->ResourceBarrier(1, &barrier);
        }
    }

    it->second.flags |= usage_flags;
    it->second.last_state = state;

    return kSuccess;
}

Result DeviceImpl::UpdateRootArguments()
{
    JD3D12_ASSERT(command_list_state_ == CommandListState::kRecording);

    for(uint32_t slot = 0; slot < MainRootSignature::kMaxCBVCount; ++slot)
    {
        Binding& binding = binding_state_.cbv_bindings_[slot];
        const uint32_t root_param_index = main_root_signature_->GetRootParamIndexForCBV(slot);
        if(binding.buffer == nullptr)
        {
            command_list_->SetComputeRootDescriptorTable(root_param_index,
                shader_visible_descriptor_heap_.GetGpuHandleForDescriptor(null_cbv_index_));
        }
        else
        {
            JD3D12_RETURN_IF_FAILED(UseBuffer(*binding.buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

            if(binding.descriptor_index == UINT32_MAX)
            {
                JD3D12_LOG_AND_RETURN_IF_FAILED(shader_visible_descriptor_heap_.AllocateDynamic(binding.descriptor_index));

                D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
                cbv_desc.BufferLocation = binding.buffer->GetD3D12Resource()->GetGPUVirtualAddress()
                    + binding.byte_range.first;
                const size_t final_size = binding.byte_range.count;
                JD3D12_ASSERT(final_size <= UINT32_MAX);
                cbv_desc.SizeInBytes = uint32_t(final_size);
                device_->CreateConstantBufferView(&cbv_desc,
                    shader_visible_descriptor_heap_.GetCpuHandleForDescriptor(binding.descriptor_index));
            }

            command_list_->SetComputeRootDescriptorTable(root_param_index,
                shader_visible_descriptor_heap_.GetGpuHandleForDescriptor(binding.descriptor_index));
        }
    }

    for(uint32_t slot = 0; slot < MainRootSignature::kMaxSRVCount; ++slot)
    {
        Binding& binding = binding_state_.srv_bindings_[slot];
        const uint32_t root_param_index = main_root_signature_->GetRootParamIndexForSRV(slot);
        if(binding.buffer == nullptr)
        {
            command_list_->SetComputeRootDescriptorTable(root_param_index,
                shader_visible_descriptor_heap_.GetGpuHandleForDescriptor(null_srv_index_));
        }
        else
        {
            JD3D12_RETURN_IF_FAILED(UseBuffer(*binding.buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

            if(binding.descriptor_index == UINT32_MAX)
            {
                JD3D12_LOG_AND_RETURN_IF_FAILED(shader_visible_descriptor_heap_.AllocateDynamic(binding.descriptor_index));

                const uint32_t buffer_type = binding.buffer->desc_.flags
                    & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress);
                JD3D12_ASSERT(CountBitsSet(buffer_type) == 1);

                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                switch(buffer_type)
                {
                case kBufferFlagTyped:
                {
                    const Format element_format = binding.buffer->desc_.element_format;
                    const DXGI_FORMAT dxgi_format = DXGI_FORMAT(element_format);
                    const FormatDesc* format_desc = GetFormatDesc(element_format);
                    JD3D12_ASSERT(format_desc != nullptr && format_desc->bits_per_element > 0
                        && format_desc->bits_per_element % 8 == 0);

                    const size_t element_size = format_desc->bits_per_element / 8;
                    const size_t final_size = binding.byte_range.count;
                    JD3D12_ASSERT(binding.byte_range.first % element_size == 0 && final_size % element_size == 0);
                    JD3D12_ASSERT(final_size / element_size <= UINT32_MAX);

                    srv_desc.Format = dxgi_format;
                    srv_desc.Buffer.StructureByteStride = 0;
                    srv_desc.Buffer.FirstElement = binding.byte_range.first / element_size;
                    srv_desc.Buffer.NumElements = uint32_t(final_size / element_size);
                    break;
                }
                case kBufferFlagStructured:
                {
                    const size_t structure_size = binding.buffer->desc_.structure_size;
                    const size_t final_size = binding.byte_range.count;
                    JD3D12_ASSERT(binding.byte_range.first % structure_size == 0 && final_size % structure_size == 0);
                    JD3D12_ASSERT(structure_size <= UINT32_MAX && final_size / structure_size <= UINT32_MAX);

                    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
                    srv_desc.Buffer.StructureByteStride = uint32_t(structure_size);
                    srv_desc.Buffer.FirstElement = binding.byte_range.first / structure_size;
                    srv_desc.Buffer.NumElements = uint32_t(final_size / structure_size);
                    break;
                }
                case kBufferFlagByteAddress:
                {
                    const size_t final_size = binding.byte_range.count;
                    JD3D12_ASSERT(binding.byte_range.first % 4 == 0 && final_size % 4 == 0);
                    JD3D12_ASSERT(final_size / 4 <= UINT32_MAX);

                    srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                    srv_desc.Buffer.StructureByteStride = 0;
                    srv_desc.Buffer.FirstElement = binding.byte_range.first / 4;
                    srv_desc.Buffer.NumElements = uint32_t(final_size / 4);
                    srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                    break;
                }
                default:
                    JD3D12_ASSERT(0);
                }
                device_->CreateShaderResourceView(binding.buffer->GetD3D12Resource(), &srv_desc,
                    shader_visible_descriptor_heap_.GetCpuHandleForDescriptor(binding.descriptor_index));
            }

            command_list_->SetComputeRootDescriptorTable(root_param_index,
                shader_visible_descriptor_heap_.GetGpuHandleForDescriptor(binding.descriptor_index));
        }
    }

    for(uint32_t slot = 0; slot < MainRootSignature::kMaxUAVCount; ++slot)
    {
        Binding& binding = binding_state_.uav_bindings_[slot];
        const uint32_t root_param_index = main_root_signature_->GetRootParamIndexForUAV(slot);
        if(binding.buffer == nullptr)
        {
            command_list_->SetComputeRootDescriptorTable(root_param_index,
                shader_visible_descriptor_heap_.GetGpuHandleForDescriptor(null_uav_index_));
        }
        else
        {
            JD3D12_RETURN_IF_FAILED(UseBuffer(*binding.buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

            if(binding.descriptor_index == UINT32_MAX)
            {
                JD3D12_LOG_AND_RETURN_IF_FAILED(shader_visible_descriptor_heap_.AllocateDynamic(binding.descriptor_index));

                const uint32_t buffer_type = binding.buffer->desc_.flags
                    & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress);
                JD3D12_ASSERT(CountBitsSet(buffer_type) == 1);

                D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                switch(buffer_type)
                {
                case kBufferFlagTyped:
                {
                    const Format element_format = binding.buffer->desc_.element_format;
                    const DXGI_FORMAT dxgi_format = DXGI_FORMAT(element_format);
                    const FormatDesc* format_desc = GetFormatDesc(element_format);
                    JD3D12_ASSERT(format_desc != nullptr && format_desc->bits_per_element > 0
                        && format_desc->bits_per_element % 8 == 0);

                    const size_t element_size = format_desc->bits_per_element / 8;
                    const size_t final_size = binding.byte_range.count;
                    JD3D12_ASSERT(binding.byte_range.first % element_size == 0 && final_size % element_size == 0);
                    JD3D12_ASSERT(final_size / element_size <= UINT32_MAX);

                    uav_desc.Format = dxgi_format;
                    uav_desc.Buffer.FirstElement = binding.byte_range.first / element_size;
                    uav_desc.Buffer.NumElements = uint32_t(final_size / element_size);
                    break;
                }
                case kBufferFlagStructured:
                {
                    const size_t structure_size = binding.buffer->desc_.structure_size;
                    const size_t final_size = binding.byte_range.count;
                    JD3D12_ASSERT(binding.byte_range.first % structure_size == 0 && final_size % structure_size == 0);
                    JD3D12_ASSERT(structure_size <= UINT32_MAX && final_size / structure_size <= UINT32_MAX);

                    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
                    uav_desc.Buffer.StructureByteStride = uint32_t(structure_size);
                    uav_desc.Buffer.FirstElement = binding.byte_range.first / structure_size;
                    uav_desc.Buffer.NumElements = uint32_t(final_size / structure_size);
                    break;
                }
                case kBufferFlagByteAddress:
                {
                    const size_t final_size = binding.byte_range.count;
                    JD3D12_ASSERT(binding.byte_range.first % 4 == 0 && final_size % 4 == 0);
                    JD3D12_ASSERT(final_size / 4 <= UINT32_MAX);

                    uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                    uav_desc.Buffer.StructureByteStride = 0;
                    uav_desc.Buffer.FirstElement = binding.byte_range.first / 4;
                    uav_desc.Buffer.NumElements = uint32_t(final_size / 4);
                    uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
                    break;
                }
                default:
                    JD3D12_ASSERT(0);
                }
                device_->CreateUnorderedAccessView(binding.buffer->GetD3D12Resource(), nullptr, &uav_desc,
                    shader_visible_descriptor_heap_.GetCpuHandleForDescriptor(binding.descriptor_index));
            }

            command_list_->SetComputeRootDescriptorTable(root_param_index,
                shader_visible_descriptor_heap_.GetGpuHandleForDescriptor(binding.descriptor_index));
        }
    }

    return kSuccess;
}

Result DeviceImpl::CreateNullDescriptors()
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;

    cpu_handle = shader_visible_descriptor_heap_.GetCpuHandleForDescriptor(null_cbv_index_);
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
    device_->CreateConstantBufferView(&cbv_desc, cpu_handle);

    cpu_handle = shader_visible_descriptor_heap_.GetCpuHandleForDescriptor(null_srv_index_);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = DXGI_FORMAT_R32_UINT;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    device_->CreateShaderResourceView(nullptr, &srv_desc, cpu_handle);

    cpu_handle = shader_visible_descriptor_heap_.GetCpuHandleForDescriptor(null_uav_index_);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.Format = DXGI_FORMAT_R32_UINT;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    device_->CreateUnorderedAccessView(nullptr, nullptr, &uav_desc, cpu_handle);

    return kSuccess;
}

Result DeviceImpl::CreateStaticShaders()
{
    Singleton& singleton = Singleton::GetInstance();

    JD3D12_ASSERT_OR_RETURN(singleton.static_shaders_.empty() || singleton.dev_count_ == 1,
        L"Static shaders can only be used when at most 1 Device is created at a time.");

    for(StaticShader* static_shader : singleton.static_shaders_)
    {
        JD3D12_RETURN_IF_FAILED(static_shader->Init());
    }
    return kSuccess;
}

Result DeviceImpl::CreateStaticBuffers()
{
    Singleton& singleton = Singleton::GetInstance();

    JD3D12_ASSERT_OR_RETURN(singleton.static_buffers_.empty() || singleton.dev_count_ == 1,
        L"Static buffers can only be used when at most 1 Device is created at a time.");

    for(StaticBuffer* static_buffer: singleton.static_buffers_)
    {
        JD3D12_RETURN_IF_FAILED(static_buffer->Init());
    }
    return kSuccess;
}

void DeviceImpl::DestroyStaticShaders()
{
    Singleton& singleton = Singleton::GetInstance();
    for(size_t i = singleton.static_shaders_.size(); i--; )
    {
        StaticShader* const static_shader = singleton.static_shaders_[i];
        delete static_shader->shader_;
        static_shader->shader_ = nullptr;
    }
}

void DeviceImpl::DestroyStaticBuffers()
{
    Singleton& singleton = Singleton::GetInstance();
    for(size_t i = singleton.static_buffers_.size(); i--; )
    {
        StaticBuffer* const static_buffer = singleton.static_buffers_[i];
        delete static_buffer->buffer_;
        static_buffer->buffer_ = nullptr;
    }
}

Result DeviceImpl::BeginClearBufferToValues(BufferImpl& buf, Range element_range,
    D3D12_GPU_DESCRIPTOR_HANDLE& out_shader_visible_gpu_desc_handle,
    D3D12_CPU_DESCRIPTOR_HANDLE& out_shader_invisible_cpu_desc_handle)
{
    out_shader_visible_gpu_desc_handle = {};
    out_shader_invisible_cpu_desc_handle = {};

    JD3D12_ASSERT_OR_RETURN(buf.GetDevice() == this, L"buf does not belong to this Device.");

    JD3D12_RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kRecording));

    ID3D12DescriptorHeap* const desc_heap = shader_visible_descriptor_heap_.GetDescriptorHeap();
    command_list_->SetDescriptorHeaps(1, &desc_heap);

    JD3D12_RETURN_IF_FAILED(UseBuffer(buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    uint32_t shader_visible_desc_index = UINT32_MAX;
    uint32_t shader_invisible_desc_index = UINT32_MAX;
    JD3D12_RETURN_IF_FAILED(shader_visible_descriptor_heap_.AllocateDynamic(shader_visible_desc_index));
    JD3D12_RETURN_IF_FAILED(shader_invisible_descriptor_heap_.AllocateDynamic(shader_invisible_desc_index));

    const size_t buf_size = buf.GetSize();

    DXGI_FORMAT dxgi_format;
    size_t element_size;
    if((buf.desc_.flags & kBufferFlagTyped) != 0)
    {
        const Format element_format = buf.desc_.element_format;
        dxgi_format = DXGI_FORMAT(element_format);
        const FormatDesc* format_desc = GetFormatDesc(element_format);
        JD3D12_ASSERT_OR_RETURN(format_desc != nullptr && format_desc->bits_per_element > 0
            && format_desc->bits_per_element % 8 == 0,
            L"Invalid element format of a typed buffer.");
        element_size = format_desc->bits_per_element / 8;
    }
    else
    {
        JD3D12_ASSERT_OR_RETURN((buf.desc_.flags & kBufferFlagByteAddress) != 0,
            L"Only typed and byte address buffers are supported.");
        dxgi_format = DXGI_FORMAT_R32_UINT;
        element_size = sizeof(uint32_t);
    }

    element_range = LimitRange(element_range, buf_size / element_size);
    JD3D12_ASSERT_OR_RETURN(element_range.first * element_size < buf_size
        && (element_range.first + element_range.count) * element_size <= buf_size,
        L"Element range out of bounds.");
    JD3D12_ASSERT_OR_RETURN(element_range.count <= UINT32_MAX, L"Element count exceeds UINT32_MAX.");

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Format = dxgi_format;
    uav_desc.Buffer.FirstElement = element_range.first;
    uav_desc.Buffer.NumElements = uint32_t(element_range.count);

    out_shader_visible_gpu_desc_handle =
        shader_visible_descriptor_heap_.GetGpuHandleForDescriptor(shader_visible_desc_index);
    const D3D12_CPU_DESCRIPTOR_HANDLE shader_visible_cpu_desc_handle =
        shader_visible_descriptor_heap_.GetCpuHandleForDescriptor(shader_visible_desc_index);
    out_shader_invisible_cpu_desc_handle =
        shader_invisible_descriptor_heap_.GetCpuHandleForDescriptor(shader_invisible_desc_index);

    ID3D12Resource* const d3d12_res = buf.GetD3D12Resource();
    JD3D12_ASSERT(d3d12_res != nullptr);
    device_->CreateUnorderedAccessView(d3d12_res, nullptr, &uav_desc, shader_visible_cpu_desc_handle);
    device_->CreateUnorderedAccessView(d3d12_res, nullptr, &uav_desc, out_shader_invisible_cpu_desc_handle);

    return kSuccess;
}

Result DeviceImpl::DispatchComputeShader(ShaderImpl& shader, const UintVec3& group_count)
{
    JD3D12_ASSERT_OR_RETURN(shader.GetDevice() == this, L"Shader does not belong to this Device.");

    if(group_count.x == 0 || group_count.y == 0 || group_count.z == 0)
        return kFalse;

    /*
    This is the most common bug when calling compute shader dispatch!!!
    While GPUs are able to quickly run millions of threads, the number of groups is
    limited to maximum 65535 per dimension. If you need more, use any or all of
    these methods:
    - Process multiple elements per thread, using a loop in the shader.
    - Use more threads per group, using numthreads attribute in HLSL, like:
      [numthreads(256, 1, 1)].
      A power of two is a good number, like 32 or 64, but it can be up to 1024.
    - Use 2D or 3D group_count, e.g. UintVec3(1000, 1000, 1) and flatten the index
      in the shader code, like:
      uint group_index = GroupID.y * 1000 + GroupID.x;
    */
    JD3D12_ASSERT_OR_RETURN(group_count.x <= UINT16_MAX
        && group_count.y <= UINT16_MAX
        && group_count.z <= UINT16_MAX, L"Dispatch group count cannot exceed 65535 in any dimension.");

    JD3D12_RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kRecording));

    ID3D12DescriptorHeap* const desc_heap = shader_visible_descriptor_heap_.GetDescriptorHeap();
    command_list_->SetDescriptorHeaps(1, &desc_heap);

    command_list_->SetPipelineState(shader.GetD3D12PipelineState());

    command_list_->SetComputeRootSignature(main_root_signature_->GetRootSignature());

    JD3D12_RETURN_IF_FAILED(UpdateRootArguments());

    command_list_->Dispatch(group_count.x, group_count.y, group_count.z);

    return kSuccess;
}

void DeviceImpl::StaticDebugLayerMessageCallback(
    D3D12_MESSAGE_CATEGORY Category,
    D3D12_MESSAGE_SEVERITY Severity,
    D3D12_MESSAGE_ID ID,
    LPCSTR pDescription,
    void* pContext)
{
    DeviceImpl* const dev = (DeviceImpl*)pContext;
    JD3D12_ASSERT(dev != nullptr);
    dev->DebugLayerMessageCallback(Category, Severity, ID, pDescription);
}

////////////////////////////////////////////////////////////////////////////////
// class IncludeHandlerBase

IncludeHandlerBase::IncludeHandlerBase(ShaderCompiler* shader_compiler, CharacterEncoding character_encoding)
    : shader_compiler_{shader_compiler}
    , character_encoding_{character_encoding}
{
}

////////////////////////////////////////////////////////////////////////////////
// class DefaultIncludeHandler

DefaultIncludeHandler::DefaultIncludeHandler(ShaderCompiler* shader_compiler, CharacterEncoding character_encoding)
    : IncludeHandlerBase{shader_compiler, character_encoding}
{
}

HRESULT STDMETHODCALLTYPE DefaultIncludeHandler::LoadSource(_In_z_ LPCWSTR pFilename,
    _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource)
{
    *ppIncludeSource = nullptr;

    FileSystemPath file_path{pFilename};
    if(FileExists(file_path))
    {
        IDxcBlobEncoding* blob_encoding = nullptr;
        UINT32 codepage = UINT32(character_encoding_);
        HRESULT hr = shader_compiler_->GetDxcUtils()->LoadFile(file_path.c_str(), &codepage, &blob_encoding);
        if(SUCCEEDED(hr))
        {
            JD3D12_ASSERT(blob_encoding != nullptr);
            *ppIncludeSource = blob_encoding;
        }
        return hr;
    }

    return kErrorNotFound;
}

bool DefaultIncludeHandler::FileExists(const FileSystemPath& path)
{
    std::error_code error_code;
    const bool exists = std::filesystem::is_regular_file(path, error_code);
    return !error_code && exists;
}

////////////////////////////////////////////////////////////////////////////////
// class CallbackIncludeHandler

CallbackIncludeHandler::CallbackIncludeHandler(ShaderCompiler* shader_compiler, CharacterEncoding character_encoding,
    IncludeCallback callback, void* callback_context)
    : IncludeHandlerBase{shader_compiler, character_encoding}
    , callback_{callback}
    , callback_context_{callback_context}
{
    JD3D12_ASSERT(callback != nullptr);
}

HRESULT STDMETHODCALLTYPE CallbackIncludeHandler::LoadSource(_In_z_ LPCWSTR pFilename,
    _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource)
{
    *ppIncludeSource = nullptr;

    char* data_ptr = nullptr;
    size_t data_size = 0;
    Result res = callback_(pFilename, callback_context_, data_ptr, data_size);
    std::unique_ptr<char[]> data{data_ptr};

    if(Failed(res))
        return res;
    if(data_size > UINT32_MAX)
        return kErrorOutOfBounds;

    IDxcBlobEncoding* blob_encoding = nullptr;
    JD3D12_RETURN_IF_FAILED(shader_compiler_->GetDxcUtils()->CreateBlob(data_ptr, static_cast<UINT32>(data_size),
        static_cast<UINT32>(character_encoding_), &blob_encoding));

    *ppIncludeSource = blob_encoding;
    return res;
}

////////////////////////////////////////////////////////////////////////////////
// class ShaderCompiler

Result ShaderCompiler::Init(const EnvironmentDesc& env_desc)
{
    FileSystemPath compiler_path =
        FileSystemPath(env_desc.dxc_dll_path) / L"dxcompiler.dll";
    module_ = LoadLibrary(compiler_path.c_str());
    if (module_ == NULL)
        return MakeResultFromLastError();

    create_instance_proc_ = DxcCreateInstanceProc(GetProcAddress(module_, "DxcCreateInstance"));
    if(create_instance_proc_ == nullptr)
        return kErrorFail;

    JD3D12_LOG_AND_RETURN_IF_FAILED(create_instance_proc_(CLSID_DxcUtils, IID_PPV_ARGS(&utils_)));
    JD3D12_LOG_AND_RETURN_IF_FAILED(create_instance_proc_(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler3_)));

    return kSuccess;
}

Logger* ShaderCompiler::GetLogger() const noexcept
{
    return env_.GetLogger();
}

Result ShaderCompiler::CompileShaderFromMemory(const ShaderCompilationParams& params,
    const wchar_t* main_source_file_path, ConstDataSpan hlsl_source, ShaderCompilationResult*& out_result)
{
    out_result = nullptr;

    JD3D12_ASSERT_OR_RETURN(!IsStringEmpty(params.entry_point),
        L"ShaderCompilationParams::entry_point cannot be null or empty.");
    JD3D12_ASSERT_OR_RETURN(hlsl_source.data != nullptr && hlsl_source.size > 0,
        L"HLSL source data cannot be null or empty.");

    JD3D12_LOG(kLogSeverityInfo, L"Compiling shader \"%s\": flags=0x%X, entry_point=%s",
        main_source_file_path, params.flags, params.entry_point);

    // Build the list of arguments.
    std::vector<std::wstring> args;
    JD3D12_RETURN_IF_FAILED(BuildArguments(params, main_source_file_path, args));

    // Build the list of arguments as const wchar_t*[].
    StackOrHeapVector<const wchar_t*, 16> arg_pointers;
    for (const std::wstring& arg : args)
    {
        arg_pointers.Emplace(arg.c_str());
    }
    for (size_t i = 0; i < params.additional_dxc_args.count; ++i)
    {
        if(!IsStringEmpty(params.additional_dxc_args.data[i]))
            arg_pointers.PushBack(params.additional_dxc_args.data[i]);
    }

    std::unique_ptr<IncludeHandlerBase> include_handler;
    if((params.flags & kShaderCompilationFlagDisableIncludes) == 0)
    {
        if(params.include_callback != nullptr)
        {
            include_handler = std::make_unique<CallbackIncludeHandler>(this, params.character_encoding,
                params.include_callback, params.include_callback_context);
        }
        else
        {
            include_handler = std::make_unique<DefaultIncludeHandler>(this, params.character_encoding);
        }
    }

    // Invoke DXC.
    DxcBuffer source_buf{};
    source_buf.Ptr = hlsl_source.data;
    source_buf.Size = hlsl_source.size;
    source_buf.Encoding = DXC_CP_ACP; // TODO support other encodings.

    CComPtr<IDxcResult> dxc_result;
    JD3D12_LOG_AND_RETURN_IF_FAILED(compiler3_->Compile(&source_buf,
        arg_pointers.GetData(), UINT32(arg_pointers.GetCount()),
        include_handler.get(), IID_PPV_ARGS(&dxc_result)));

    // Create ShaderCompilationResult.
    auto result = std::unique_ptr<ShaderCompilationResult>{ new ShaderCompilationResult{} };
    result->impl_ = new ShaderCompilationResultImpl(result.get(), &env_);
    JD3D12_RETURN_IF_FAILED(result->impl_->Init(std::move(dxc_result)));
    LogCompilationResult(*result);
    out_result = result.release();
    return kSuccess;
}

Result ShaderCompiler::BuildArguments(const ShaderCompilationParams& params,
    const wchar_t* source_name, std::vector<std::wstring>& out_arguments)
{
    out_arguments.clear();

    // Source file name (optional).
    if(!IsStringEmpty(source_name))
        out_arguments.push_back(source_name);

    // Entry point.
    JD3D12_ASSERT_OR_RETURN(IsHlslIdentifier(params.entry_point),
        L"ShaderCompilationParams::entry_point must be a valid HLSL identifier.");
    out_arguments.push_back(L"-E");
    out_arguments.push_back(params.entry_point);

    // HLSL version.
    JD3D12_ASSERT_OR_RETURN(params.hlsl_version == kHlslVersion2016
        || params.hlsl_version == kHlslVersion2017
        || params.hlsl_version == kHlslVersion2018
        || params.hlsl_version == kHlslVersion2021,
        L"Unsupported HLSL version specified in ShaderCompilationParams::hlsl_version.");
    out_arguments.push_back(L"-HV");
    out_arguments.push_back(std::to_wstring(params.hlsl_version));

    // Shader model.
    const uint32_t shader_model_major = params.shader_model >> 8;
    const uint32_t shader_model_minor = params.shader_model & 0xFF;
    JD3D12_ASSERT_OR_RETURN(shader_model_major == 6 && shader_model_major <= 9,
        L"Unsupported shader model specified in ShaderCompilationParams::shader_model.");
    out_arguments.push_back(L"-T");
    out_arguments.emplace_back(SPrintF(L"cs_%u_%u", shader_model_major, shader_model_minor));

    // Optimization level.
    JD3D12_ASSERT_OR_RETURN(params.optimization_level == kShaderOptimizationDisabled
        || params.optimization_level == kShaderOptimizationLevel0
        || params.optimization_level == kShaderOptimizationLevel1
        || params.optimization_level == kShaderOptimizationLevel2
        || params.optimization_level == kShaderOptimizationLevel3,
        L"Invalid optimization level specified in ShaderCompilationParams::optimization_level.");
    if (params.optimization_level == kShaderOptimizationDisabled)
        out_arguments.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
    else if (params.optimization_level != kShaderOptimizationLevel3)
        out_arguments.push_back(SPrintF(L"-O%d", params.optimization_level));

    JD3D12_ASSERT_OR_RETURN(CountBitsSet(params.flags & (
        kShaderCompilationFlagDenormPreserve | kShaderCompilationFlagDenormFlushToZero)) <= 1,
        L"kShaderCompilationFlagDenormPreserve and kShaderCompilationFlagDenormFlushToZero and mutually exclusive.");
    JD3D12_ASSERT_OR_RETURN(CountBitsSet(params.flags & (
        kShaderCompilationFlagAvoidFlowControl | kShaderCompilationFlagPreferFlowControl)) <= 1,
        L"kShaderCompilationFlagAvoidFlowControl and kShaderCompilationFlagPreferFlowControl and mutually exclusive.");
    JD3D12_ASSERT_OR_RETURN(CountBitsSet(params.flags & (
        kShaderCompilationFlagPackMatricesColumnMajor | kShaderCompilationFlagPackMatricesRowMajor)) <= 1,
        L"kShaderCompilationFlagPackMatricesColumnMajor and kShaderCompilationFlagPackMatricesRowMajor and mutually exclusive.");
    JD3D12_ASSERT_OR_RETURN(CountBitsSet(params.flags & (
        kShaderCompilationFlagFiniteMathOnly | kShaderCompilationFlagNoFiniteMathOnly)) <= 1,
        L"kShaderCompilationFlagFiniteMathOnly and kShaderCompilationFlagNoFiniteMathOnly and mutually exclusive.");

    // Macro definitions.
    JD3D12_ASSERT_OR_RETURN(params.macro_defines.count % 2 == 0,
        L"ShaderCompilationParams::macro_defines must contains an even number of elements for macro names and their values.");
    for(size_t i = 0; i < params.macro_defines.count; i += 2)
    {
        const wchar_t* const macro_name = params.macro_defines.data[i];
        const wchar_t* const macro_value = params.macro_defines.data[i + 1];
        JD3D12_ASSERT_OR_RETURN(IsHlslIdentifier(macro_name), L"Macro name must be a valid HLSL identifier.");
        out_arguments.push_back(L"-D");
        if(IsStringEmpty(macro_value))
            out_arguments.push_back(macro_name);
        else
            out_arguments.push_back(macro_name + std::wstring{L"="} + macro_value);
    }

    // Individual flags.
    if((params.flags & kShaderCompilationFlagDenormPreserve) != 0)
    {
        out_arguments.push_back(L"-denorm");
        out_arguments.push_back(L"preserve");
    }
    else if((params.flags & kShaderCompilationFlagDenormFlushToZero) != 0)
    {
        out_arguments.push_back(L"-denorm");
        out_arguments.push_back(L"ftz");
    }
    if((params.flags & kShaderCompilationFlagEnable16BitTypes) != 0)
        out_arguments.push_back(L"-enable-16bit-types");
    if((params.flags & kShaderCompilationFlagAvoidFlowControl) != 0)
        out_arguments.push_back(DXC_ARG_AVOID_FLOW_CONTROL);
    else if ((params.flags & kShaderCompilationFlagPreferFlowControl) != 0)
        out_arguments.push_back(DXC_ARG_PREFER_FLOW_CONTROL);
    if((params.flags & kShaderCompilationFlagEnableIeeeStrictness) != 0)
        out_arguments.push_back(DXC_ARG_IEEE_STRICTNESS);
    if((params.flags & kShaderCompilationFlagSuppressWarnings) != 0)
        out_arguments.push_back(L"-no-warnings");
    if((params.flags & kShaderCompilationFlagTreatWarningsAsErrors) != 0)
        out_arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
    if((params.flags & kShaderCompilationFlagPackMatricesColumnMajor) != 0)
        out_arguments.push_back(DXC_ARG_PACK_MATRIX_COLUMN_MAJOR);
    else if((params.flags & kShaderCompilationFlagPackMatricesRowMajor) != 0)
        out_arguments.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR);
    if((params.flags & kShaderCompilationFlagFiniteMathOnly) != 0)
        out_arguments.push_back(L"-ffinite-math-only");
    else if((params.flags & kShaderCompilationFlagNoFiniteMathOnly) != 0)
        out_arguments.push_back(L"-fno-finite-math-only");

    return kSuccess;
}

void ShaderCompiler::LogCompilationResult(ShaderCompilationResult& result)
{
    const char* errors_and_warnings = result.GetErrorsAndWarnings();
    if(IsStringEmpty(errors_and_warnings))
        return;

    const LogSeverity log_severity = Succeeded(result.GetResult())
        ? kLogSeverityWarning : kLogSeverityError;
    JD3D12_LOG(log_severity, L"%S", errors_and_warnings);
}

////////////////////////////////////////////////////////////////////////////////
// class EnvironmentImpl

EnvironmentImpl::EnvironmentImpl(Environment* interface_obj, const EnvironmentDesc& desc)
    : interface_obj_{interface_obj}
    , desc_{desc}
    , shader_compiler_{*this}
{
    Singleton& singleton = Singleton::GetInstance();
    JD3D12_ASSERT(singleton.env_ == nullptr && "Only one Environment instance can be created.");
    singleton.env_ = this;
}

Result EnvironmentImpl::Init()
{
    const bool needs_logger = (desc_.flags & (
        kEnvironmentFlagLogStandardOutput | kEnvironmentFlagLogStandardError | kEnvironmentFlagLogDebug)) != 0
        || !IsStringEmpty(desc_.log_file_path)
        || desc_.log_callback != nullptr;
    if(needs_logger)
    {
        logger_ = std::make_unique<Logger>();
        JD3D12_RETURN_IF_FAILED(logger_->Init(desc_));
    }

    JD3D12_LOG(kLogSeverityInfo, L"Creating Environment 0x%016" PRIXPTR ": flags=0x%X",
        uintptr_t(GetInterface()), desc_.flags);

    JD3D12_ASSERT_OR_RETURN(!IsStringEmpty(desc_.d3d12_dll_path),
        L"EnvironmentDesc::d3d12_dll_path cannot be null or empty.");
    JD3D12_ASSERT_OR_RETURN(!IsStringEmpty(desc_.dxc_dll_path),
        L"EnvironmentDesc::dxc_dll_path cannot be null or empty.");

    const bool enable_debug_layer = (desc_.flags & kEnvironmentFlagEnableD3d12DebugLayer) != 0;
    UINT create_factory_flags = 0;
    if(enable_debug_layer)
    {
        JD3D12_RETURN_IF_FAILED(EnableDebugLayer());
        create_factory_flags = DXGI_CREATE_FACTORY_DEBUG;
    }

    JD3D12_LOG_AND_RETURN_IF_FAILED(CreateDXGIFactory2(create_factory_flags, IID_PPV_ARGS(&dxgi_factory6_)));

    for(UINT adapter_index = 0; ; ++adapter_index)
    {
        CComPtr<IDXGIAdapter1> adapter1;
        HRESULT hr = dxgi_factory6_->EnumAdapterByGpuPreference(adapter_index,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter1));
        if(FAILED(hr))
            break;

        DXGI_ADAPTER_DESC1 adapter_desc = {};
        JD3D12_LOG_AND_RETURN_IF_FAILED(adapter1->GetDesc1(&adapter_desc));

        selected_adapter_index_ = adapter_index;
        adapter_ = std::move(adapter1);
        break;
    }

    JD3D12_ASSERT_OR_RETURN(selected_adapter_index_ != UINT32_MAX, L"Adapter not found.");

    JD3D12_LOG_AND_RETURN_IF_FAILED(D3D12GetInterface(CLSID_D3D12SDKConfiguration, IID_PPV_ARGS(&sdk_config1_)));

    uint32_t sdk_version = desc_.is_d3d12_agility_sdk_preview
        ? D3D12_PREVIEW_SDK_VERSION : D3D12_SDK_VERSION;
    JD3D12_LOG_AND_RETURN_IF_FAILED(sdk_config1_->CreateDeviceFactory(sdk_version, desc_.d3d12_dll_path,
        IID_PPV_ARGS(&device_factory_)));

    JD3D12_RETURN_IF_FAILED(shader_compiler_.Init(desc_));

    // Clear strings as they can become invalid after this call.
    desc_.d3d12_dll_path = nullptr;
    desc_.dxc_dll_path = nullptr;

    return kSuccess;
}

void EnvironmentImpl::Log(LogSeverity severity, const wchar_t* message)
{
    if(logger_)
        logger_->Log(severity, message);
}

void EnvironmentImpl::VLogF(LogSeverity severity, const wchar_t* format, va_list arg_list)
{
    if(logger_)
        logger_->VLogF(severity, format, arg_list);
}

EnvironmentImpl::~EnvironmentImpl()
{
    JD3D12_LOG(kLogSeverityInfo, L"Destroying Environment 0x%016" PRIXPTR, uintptr_t(GetInterface()));

    JD3D12_ASSERT(device_count_ == 0 && "Destroying Environment object while there are still Device objects not destroyed.");
    Singleton::GetInstance().env_ = nullptr;
}

Result EnvironmentImpl::CreateDevice(const DeviceDesc& desc, Device*& out_device)
{
    out_device = nullptr;

    auto device = std::unique_ptr<Device>{new Device{}};
    device->impl_ = new DeviceImpl(device.get(), this, desc);

    const bool enable_d3d12_debug_layer = (desc_.flags & kEnvironmentFlagEnableD3d12DebugLayer) != 0;
    JD3D12_RETURN_IF_FAILED(device->impl_->Init(enable_d3d12_debug_layer));

    out_device = device.release();
    return kSuccess;
}

Result EnvironmentImpl::CompileShaderFromMemory(const ShaderCompilationParams& params, const wchar_t* name,
    ConstDataSpan hlsl_source, ShaderCompilationResult*& out_result)
{
    return shader_compiler_.CompileShaderFromMemory(params, name, hlsl_source, out_result);
}

Result EnvironmentImpl::CompileShaderFromFile(const ShaderCompilationParams& params,
    const wchar_t* hlsl_source_file_path, ShaderCompilationResult*& out_result)
{
    JD3D12_ASSERT_OR_RETURN(!IsStringEmpty(hlsl_source_file_path),
        L"hlsl_source_file_path cannot be null or empty.");

    JD3D12_LOG(kLogSeverityInfo, L"Loading shader source from file \"%s\"",
        hlsl_source_file_path);

    char* source_ptr = nullptr;
    size_t source_size = 0;
    JD3D12_LOG_AND_RETURN_IF_FAILED(LoadFile(hlsl_source_file_path, source_ptr, source_size));
    std::unique_ptr<char[]> source{ source_ptr };

    return CompileShaderFromMemory(params, hlsl_source_file_path,
        ConstDataSpan{source_ptr, source_size}, out_result);
}

Result EnvironmentImpl::EnableDebugLayer()
{
    CComPtr<ID3D12Debug> debug;
    JD3D12_LOG_AND_RETURN_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
    debug->EnableDebugLayer();

    const bool enable_gbv = (desc_.flags & kEnvironmentFlagEnableD3d12GpuBasedValidation) != 0;
    const bool disable_synchronized_command_queue_validation
        = (desc_.flags & kEnvironmentFlagDisableD3d12SynchronizedCommandQueueValidation) != 0;

    if(enable_gbv || disable_synchronized_command_queue_validation)
    {
        if(CComPtr<ID3D12Debug1> debug1
            ; SUCCEEDED(debug->QueryInterface(IID_PPV_ARGS(&debug1))))
        {
            if(enable_gbv)
            {
                debug1->SetEnableGPUBasedValidation(TRUE);

                if((desc_.flags & kEnvironmentFlagDisableD3d12StateTracking) != 0)
                {
                    if(CComPtr<ID3D12Debug2> debug2
                        ; SUCCEEDED(debug->QueryInterface(IID_PPV_ARGS(&debug2))))
                    {
                        debug2->SetGPUBasedValidationFlags(
                            D3D12_GPU_BASED_VALIDATION_FLAGS_DISABLE_STATE_TRACKING);
                    }
                }
            }

            if(disable_synchronized_command_queue_validation)
            {
                debug1->SetEnableSynchronizedCommandQueueValidation(FALSE);
            }
        }
    }

    return kSuccess;
}

////////////////////////////////////////////////////////////////////////////////
// public class Buffer

Buffer::Buffer()
{
    // Empty.
}

Buffer::~Buffer()
{
    delete impl_;
}

Device* Buffer::GetDevice() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetDevice()->GetInterface();
}

const wchar_t* Buffer::GetName() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetName();
}

size_t Buffer::GetSize() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->desc_.size;
}

uint32_t Buffer::GetFlags() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->desc_.flags;
}

Format Buffer::GetElementFormat() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->desc_.element_format;
}

size_t Buffer::GetStructureSize() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->desc_.structure_size;
}

size_t Buffer::GetElementSize() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetElementSize();
}

void* Buffer::GetD3D12Resource() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetD3D12Resource();
}

////////////////////////////////////////////////////////////////////////////////
// Public class Shader

Shader::Shader()
{
    // Empty.
}

Shader::~Shader()
{
    delete impl_;
}

Device* Shader::GetDevice() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetDevice()->GetInterface();
}

const wchar_t* Shader::GetName() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetName();
}

UintVec3 Shader::GetThreadGroupSize() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetThreadGroupSize();
}

void* Shader::GetD3D12PipelineState() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetD3D12PipelineState();
}

////////////////////////////////////////////////////////////////////////////////
// Public class ShaderCompilationResult

ShaderCompilationResult::ShaderCompilationResult()
{
    // Empty.
}

ShaderCompilationResult::~ShaderCompilationResult()
{
    delete impl_;
}

Environment* ShaderCompilationResult::GetEnvironment() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetEnvironment()->GetInterface();
}

Result jd3d12::ShaderCompilationResult::GetResult()
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetResult();
}

const char* jd3d12::ShaderCompilationResult::GetErrorsAndWarnings()
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetErrorsAndWarnings();
}

ConstDataSpan jd3d12::ShaderCompilationResult::GetBytecode()
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetBytecode();
}

////////////////////////////////////////////////////////////////////////////////
// Public class Device

Device::Device()
{
    // Empty.
}

Device::~Device()
{
    delete impl_;
}

Environment* Device::GetEnvironment() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetEnvironment()->GetInterface();
}

void* Device::GetD3D12Device() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetD3D12Device();
}

Result Device::CreateBuffer(const BufferDesc& desc, Buffer*& out_buffer)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CreateBuffer(desc, out_buffer);
}

Result Device::CreateBufferFromMemory(const BufferDesc& desc, ConstDataSpan initial_data,
    Buffer*& out_buffer)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CreateBufferFromMemory(desc, initial_data, out_buffer);
}

Result Device::CreateBufferFromFile(const BufferDesc& desc, const wchar_t* initial_data_file_path,
    Buffer*& out_buffer)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CreateBufferFromFile(desc, initial_data_file_path, out_buffer);
}

Result Device::CreateShaderFromMemory(const ShaderDesc& desc, ConstDataSpan bytecode,
    Shader*& out_shader)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CreateShaderFromMemory(desc, bytecode, out_shader);
}

Result Device::CreateShaderFromFile(const ShaderDesc& desc, const wchar_t* bytecode_file_path,
    Shader*& out_shader)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CreateShaderFromFile(desc, bytecode_file_path, out_shader);
}

Result Device::CompileAndCreateShaderFromMemory(const ShaderCompilationParams& compilation_params,
    const ShaderDesc& desc, ConstDataSpan hlsl_source, Shader*& out_shader)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CompileAndCreateShaderFromMemory(compilation_params, desc, hlsl_source, out_shader);
}

Result Device::CompileAndCreateShaderFromFile(const ShaderCompilationParams& compilation_params,
    const ShaderDesc& desc, const wchar_t* hlsl_source_file_path, Shader*& out_shader)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CompileAndCreateShaderFromFile(compilation_params, desc, hlsl_source_file_path, out_shader);
}

Result Device::MapBuffer(Buffer& buf, Range byte_range, BufferFlags cpu_usage_flag, void*& out_data_ptr,
    uint32_t command_flags)
{
    JD3D12_ASSERT(impl_ != nullptr && buf.GetImpl() != nullptr);
    return impl_->MapBuffer(*buf.GetImpl(), byte_range, cpu_usage_flag, out_data_ptr, command_flags);
}

void Device::UnmapBuffer(Buffer& buf)
{
    JD3D12_ASSERT(impl_ != nullptr && buf.GetImpl() != nullptr);
    impl_->UnmapBuffer(*buf.GetImpl());
}

Result Device::ReadBufferToMemory(Buffer& src_buf, Range src_byte_range, void* dst_memory,
    uint32_t command_flags)
{
    JD3D12_ASSERT(impl_ != nullptr && src_buf.GetImpl() != nullptr);
    return impl_->ReadBufferToMemory(*src_buf.GetImpl(), src_byte_range, dst_memory, command_flags);
}

Result Device::WriteMemoryToBuffer(ConstDataSpan src_data, Buffer& dst_buf, size_t dst_byte_offset,
    uint32_t command_flags)
{
    JD3D12_ASSERT(impl_ != nullptr && dst_buf.GetImpl() != nullptr);
    return impl_->WriteMemoryToBuffer(src_data, *dst_buf.GetImpl(), dst_byte_offset, command_flags);
}

Result Device::SubmitPendingCommands()
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->SubmitPendingCommands();
}

Result Device::WaitForGPU(uint32_t timeout_milliseconds)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->WaitForGPU(timeout_milliseconds);
}

Result Device::CopyBuffer(Buffer& src_buf, Buffer& dst_buf)
{
    JD3D12_ASSERT(impl_ != nullptr && src_buf.GetImpl() != nullptr && dst_buf.GetImpl() != nullptr);
    return impl_->CopyBuffer(*src_buf.GetImpl(), *dst_buf.GetImpl());
}

Result Device::CopyBufferRegion(Buffer& src_buf, Range src_byte_range, Buffer& dst_buf, size_t dst_byte_offset)
{
    JD3D12_ASSERT(impl_ != nullptr && src_buf.GetImpl() != nullptr && dst_buf.GetImpl() != nullptr);
    return impl_->CopyBufferRegion(*src_buf.GetImpl(), src_byte_range, *dst_buf.GetImpl(), dst_byte_offset);
}

Result Device::ClearBufferToUintValues(Buffer& buf, const UintVec4& values, Range element_range)
{
    JD3D12_ASSERT(impl_ != nullptr && buf.GetImpl() != nullptr);
    return impl_->ClearBufferToUintValues(*buf.GetImpl(), values, element_range);
}

Result Device::ClearBufferToFloatValues(Buffer& buf, const FloatVec4& values, Range element_range)
{
    JD3D12_ASSERT(impl_ != nullptr && buf.GetImpl() != nullptr);
    return impl_->ClearBufferToFloatValues(*buf.GetImpl(), values, element_range);
}

void Device::ResetAllBindings()
{
    JD3D12_ASSERT(impl_ != nullptr);
    impl_->ResetAllBindings();
}

Result Device::BindConstantBuffer(uint32_t b_slot, Buffer* buf, Range byte_range)
{
    JD3D12_ASSERT(impl_ != nullptr);
    JD3D12_ASSERT(buf == nullptr || buf->GetImpl() != nullptr);
    return impl_->BindConstantBuffer(b_slot, buf ? buf->GetImpl() : nullptr, byte_range);
}

Result Device::BindBuffer(uint32_t t_slot, Buffer* buf, Range byte_range)
{
    JD3D12_ASSERT(impl_ != nullptr);
    JD3D12_ASSERT(buf == nullptr || buf->GetImpl() != nullptr);
    return impl_->BindBuffer(t_slot, buf ? buf->GetImpl() : nullptr, byte_range);
}

Result Device::BindRWBuffer(uint32_t u_slot, Buffer* buf, Range byte_range)
{
    JD3D12_ASSERT(impl_ != nullptr);
    JD3D12_ASSERT(buf == nullptr || buf->GetImpl() != nullptr);
    return impl_->BindRWBuffer(u_slot, buf ? buf->GetImpl() : nullptr, byte_range);
}

Result Device::DispatchComputeShader(Shader& shader, const UintVec3& group_count)
{
    JD3D12_ASSERT(impl_ != nullptr && shader.GetImpl() != nullptr);
    return impl_->DispatchComputeShader(*shader.GetImpl(), group_count);
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticShader

StaticShader::StaticShader()
{
    Singleton& singleton = Singleton::GetInstance();
    JD3D12_ASSERT(singleton.env_ == nullptr &&
        "StaticShader objects can only be created before Environment object is created.");
    singleton.static_shaders_.push_back(this);
}

StaticShader::StaticShader(const ShaderDesc& desc)
    : desc_{ desc }
{
    Singleton& singleton = Singleton::GetInstance();
    JD3D12_ASSERT(singleton.env_ == nullptr &&
        "StaticShader objects can only be created before Environment object is created.");
    singleton.static_shaders_.push_back(this);
}

StaticShader::~StaticShader()
{
    JD3D12_ASSERT(shader_ == nullptr
        && "StaticBuffer objects lifetime must extend beyond the lifetime of the main Environment object.");

    // Intentionally not removing itself from Singleton::static_shaders_ because the order
    // of global object destruction may be undefined.
}

Shader* StaticShader::GetShader() const noexcept
{
    return shader_;
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticShaderFromMemory

StaticShaderFromMemory::StaticShaderFromMemory()
{
}

StaticShaderFromMemory::StaticShaderFromMemory(const ShaderDesc& desc, ConstDataSpan bytecode)
    : StaticShader{ desc }
    , bytecode_{ bytecode }
{
}

StaticShaderFromMemory::~StaticShaderFromMemory()
{
}

inline void StaticShaderFromMemory::Set(const ShaderDesc& desc, ConstDataSpan bytecode)
{
    JD3D12_ASSERT(!shader_ && "Cannot call StaticShaderFromMemory::Set when the shader is already created.");

    desc_ = desc;
    bytecode_ = bytecode;
}

Result StaticShaderFromMemory::Init()
{
    if(!IsSet())
        return kFalse;

    Singleton& singleton = Singleton::GetInstance();
    Device* dev = singleton.first_dev_;
    JD3D12_ASSERT(dev != nullptr && singleton.dev_count_ == 1);
    return dev->CreateShaderFromMemory(desc_, bytecode_, shader_);
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticShaderFromFile

StaticShaderFromFile::StaticShaderFromFile()
{
}

StaticShaderFromFile::StaticShaderFromFile(const ShaderDesc& desc, const wchar_t* bytecode_file_path)
    : StaticShader{ desc }
    , bytecode_file_path_{ bytecode_file_path }
{
}

StaticShaderFromFile::~StaticShaderFromFile()
{
}

bool StaticShaderFromFile::IsSet() const noexcept
{
    return !IsStringEmpty(bytecode_file_path_);
}

void StaticShaderFromFile::Set(const ShaderDesc& desc, const wchar_t* bytecode_file_path)
{
    JD3D12_ASSERT(!shader_ && "Cannot call StaticShaderFromFile::Set when the shader is already created.");

    desc_ = desc;
    bytecode_file_path_ = bytecode_file_path;
}

Result StaticShaderFromFile::Init()
{
    if(!IsSet())
        return kFalse;

    Singleton& singleton = Singleton::GetInstance();
    Device* dev = singleton.first_dev_;
    JD3D12_ASSERT(dev != nullptr && singleton.dev_count_ == 1);
    return dev->CreateShaderFromFile(desc_, bytecode_file_path_, shader_);
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticShaderCompiledFromMemory

StaticShaderCompiledFromMemory::StaticShaderCompiledFromMemory()
{
}

StaticShaderCompiledFromMemory::StaticShaderCompiledFromMemory(const ShaderCompilationParams& compilation_params,
    const ShaderDesc& desc, ConstDataSpan hlsl_source)
    : StaticShader{ desc }
    , compilation_params_{ compilation_params }
    , hlsl_source_{ hlsl_source }
{
}

Result StaticShaderCompiledFromMemory::Init()
{
    if(!IsSet())
        return kFalse;

    Singleton& singleton = Singleton::GetInstance();
    Device* dev = singleton.first_dev_;
    JD3D12_ASSERT(dev != nullptr && singleton.dev_count_ == 1);
    return dev->CompileAndCreateShaderFromMemory(compilation_params_, desc_, hlsl_source_, shader_);
}

StaticShaderCompiledFromMemory::~StaticShaderCompiledFromMemory()
{
}

void StaticShaderCompiledFromMemory::Set(const ShaderCompilationParams& compilation_params,
    const ShaderDesc& desc, ConstDataSpan hlsl_source)
{
    JD3D12_ASSERT(!shader_ && "Cannot call StaticShaderCompiledFromMemory::Set when the shader is already created.");

    desc_ = desc;
    compilation_params_ = compilation_params;
    hlsl_source_ = hlsl_source;
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticShaderCompiledFromMemory

StaticShaderCompiledFromFile::StaticShaderCompiledFromFile()
{
}

StaticShaderCompiledFromFile::StaticShaderCompiledFromFile(const ShaderCompilationParams& compilation_params,
    const ShaderDesc& desc, const wchar_t* hlsl_source_file_path)
    : StaticShader{ desc }
    , compilation_params_{ compilation_params }
    , hlsl_source_file_path_{ hlsl_source_file_path }
{
}

Result StaticShaderCompiledFromFile::Init()
{
    if(!IsSet())
        return kFalse;

    Singleton& singleton = Singleton::GetInstance();
    Device* dev = singleton.first_dev_;
    JD3D12_ASSERT(dev != nullptr && singleton.dev_count_ == 1);
    return dev->CompileAndCreateShaderFromFile(compilation_params_, desc_, hlsl_source_file_path_, shader_);
}

StaticShaderCompiledFromFile::~StaticShaderCompiledFromFile()
{
}

bool StaticShaderCompiledFromFile::IsSet() const noexcept
{
    return !IsStringEmpty(hlsl_source_file_path_);
}

void StaticShaderCompiledFromFile::Set(const ShaderCompilationParams& compilation_params,
    const ShaderDesc& desc, const wchar_t* hlsl_source_file_path)
{
    JD3D12_ASSERT(!shader_ && "Cannot call StaticShaderCompiledFromFile::Set when the shader is already created.");

    desc_ = desc;
    compilation_params_ = compilation_params;
    hlsl_source_file_path_ = hlsl_source_file_path;
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticBuffer

StaticBuffer::StaticBuffer()
{
    Singleton& singleton = Singleton::GetInstance();
    JD3D12_ASSERT(singleton.env_ == nullptr &&
        "StaticBuffer objects can only be created before Environment object is created.");
    singleton.static_buffers_.push_back(this);
}

StaticBuffer::StaticBuffer(const BufferDesc& desc)
    : desc_{ desc }
{
    Singleton& singleton = Singleton::GetInstance();
    JD3D12_ASSERT(singleton.env_ == nullptr &&
        "StaticBuffer objects can only be created before Environment object is created.");
    singleton.static_buffers_.push_back(this);
}

StaticBuffer::~StaticBuffer()
{
    JD3D12_ASSERT(buffer_ == nullptr
        && "StaticBuffer objects lifetime must extend beyond the lifetime of the main Environment object.");

    // Intentionally not removing itself from Singleton::static_buffers_ because the order
    // of global object destruction may be undefined.
}

void StaticBuffer::Set(const BufferDesc& desc)
{
    JD3D12_ASSERT(!buffer_ && "Cannot call StaticBuffer::Set when the buffer is already created.");
    desc_ = desc;
}

Buffer* StaticBuffer::GetBuffer() const noexcept
{
    return buffer_;
}

Result StaticBuffer::Init()
{
    if(!IsSet())
        return kFalse;

    Singleton& singleton = Singleton::GetInstance();
    Device* dev = singleton.first_dev_;
    JD3D12_ASSERT(dev != nullptr && singleton.dev_count_ == 1);
    return dev->CreateBuffer(desc_, buffer_);
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticBufferFromMemory

StaticBufferFromMemory::StaticBufferFromMemory(const BufferDesc& desc, ConstDataSpan initial_data)
    : StaticBuffer{ desc }
    , initial_data_{ initial_data }
{
}

void StaticBufferFromMemory::Set(const BufferDesc& desc, ConstDataSpan initial_data)
{
    __super::Set(desc);
    initial_data_ = initial_data;
}

Result StaticBufferFromMemory::Init()
{
    if(!IsSet())
        return kFalse;

    Singleton& singleton = Singleton::GetInstance();
    Device* dev = singleton.first_dev_;
    JD3D12_ASSERT(dev != nullptr && singleton.dev_count_ == 1);
    return dev->CreateBufferFromMemory(desc_, initial_data_, buffer_);
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticBufferFromFile

StaticBufferFromFile::StaticBufferFromFile(const BufferDesc& desc, const wchar_t* initial_data_file_path)
    : StaticBuffer{ desc }
    , initial_data_file_path_{ initial_data_file_path }
{
}

void StaticBufferFromFile::Set(const BufferDesc& desc, const wchar_t* initial_data_file_path)
{
    __super::Set(desc);
    initial_data_file_path_ = initial_data_file_path;
}

Result StaticBufferFromFile::Init()
{
    if(!IsSet())
        return kFalse;

    Singleton& singleton = Singleton::GetInstance();
    Device* dev = singleton.first_dev_;
    JD3D12_ASSERT(dev != nullptr && singleton.dev_count_ == 1);
    return dev->CreateBufferFromFile(desc_, initial_data_file_path_, buffer_);
}

////////////////////////////////////////////////////////////////////////////////
// Public class Environment

Environment::Environment()
{
    // Empty.
}

Environment::~Environment()
{
    delete impl_;
}

void* Environment::GetDXGIFactory6() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetDXGIFactory6();
}

void* Environment::GetDXGIAdapter1() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetDXGIAdapter1();
}

void* Environment::GetD3D12SDKConfiguration1() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetD3D12SDKConfiguration1();
}

void* Environment::GetD3D12DeviceFactory() const noexcept
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->GetD3D12DeviceFactory();
}

void Environment::Log(LogSeverity severity, const wchar_t* message)
{
    JD3D12_ASSERT(impl_ != nullptr);
    impl_->Log(severity, message);
}

void jd3d12::Environment::LogF(LogSeverity severity, const wchar_t* format, ...)
{
    JD3D12_ASSERT(impl_ != nullptr);

    va_list arg_list;
    va_start(arg_list, format);
    impl_->VLogF(severity, format, arg_list);
    va_end(arg_list);
}

Result Environment::CreateDevice(const DeviceDesc& desc, Device*& out_device)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CreateDevice(desc, out_device);
}

Result Environment::CompileShaderFromMemory(const ShaderCompilationParams& params,
    ConstDataSpan hlsl_source, ShaderCompilationResult*& out_result)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CompileShaderFromMemory(params, L"shader_from_memory.hlsl",
        hlsl_source, out_result);
}

Result Environment::CompileShaderFromFile(const ShaderCompilationParams& params,
    const wchar_t* hlsl_source_file_path, ShaderCompilationResult*& out_result)
{
    JD3D12_ASSERT(impl_ != nullptr);
    return impl_->CompileShaderFromFile(params, hlsl_source_file_path, out_result);
}

////////////////////////////////////////////////////////////////////////////////
// Public global functions

Result CreateEnvironment(const EnvironmentDesc& desc, Environment*& out_env)
{
    out_env = nullptr;

    auto env = std::unique_ptr<Environment>{new Environment{}};
    env->impl_ = new EnvironmentImpl{env.get(), desc};
    JD3D12_RETURN_IF_FAILED(env->impl_->Init());

    out_env = env.release();
    return kSuccess;
}

} // namespace jd3d12
