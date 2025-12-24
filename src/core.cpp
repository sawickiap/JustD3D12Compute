#include <jd3d12/core.hpp>
#include "internal_utils.hpp"

namespace jd3d12
{

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
    ID3D12Resource* GetResource() const noexcept { return resource_; }

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
    Result Init(ConstDataSpan bytecode);

    ID3D12PipelineState* GetPipelineState() const noexcept { return pipeline_state_; }

private:
    Shader* const interface_obj_;
    ShaderDesc desc_ = {};
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

class DeviceImpl : public DeviceObject
{
public:
    DeviceImpl(Device* interface_obj, EnvironmentImpl* env, const DeviceDesc& desc);
    ~DeviceImpl();
    Result Init();

    Device* GetInterface() const noexcept { return interface_obj_; }
    EnvironmentImpl* GetEnvironment() const noexcept { return env_; }
    ID3D12Device* GetDevice() const noexcept { return device_; }
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

    Device* const interface_obj_;
    EnvironmentImpl* const env_;
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

    JD3D12_NO_COPY_NO_MOVE_CLASS(ShaderCompiler)
};

class EnvironmentImpl
{
public:
    EnvironmentImpl(Environment* interface_obj, const EnvironmentDesc& desc);
    ~EnvironmentImpl();
    Result Init(const EnvironmentDesc& desc);

    Environment* GetInterface() const noexcept { return interface_obj_; }
    IDXGIFactory6* GetDXGIFactory6() const noexcept { return dxgi_factory6_; }
    IDXGIAdapter1* GetAdapter1() const noexcept { return adapter_; }
    ID3D12SDKConfiguration1* GetSDKConfiguration1() const noexcept { return sdk_config1_; }
    ID3D12DeviceFactory* GetDeviceFactory() const noexcept { return device_factory_; }

    Result CreateDevice(const DeviceDesc& desc, Device*& out_device);

private:
    Environment* const interface_obj_;
    CComPtr<IDXGIFactory6> dxgi_factory6_;
    UINT selected_adapter_index_ = UINT32_MAX;
    CComPtr<IDXGIAdapter1> adapter_;
    CComPtr<ID3D12SDKConfiguration1> sdk_config1_;
    CComPtr<ID3D12DeviceFactory> device_factory_;
    std::atomic<size_t> device_count_{ 0 };
    ShaderCompiler shader_compiler_;

    JD3D12_NO_COPY_NO_MOVE_CLASS(EnvironmentImpl)
};

////////////////////////////////////////////////////////////////////////////////
// class DeviceObject

DeviceObject::DeviceObject(DeviceImpl* device, const wchar_t* name)
    : device_{device}
{
    assert(device != nullptr);
    if ((device->desc_.flags & kDeviceFlagDisableNameStoring) == 0 && !IsStringEmpty(name))
        name_ = name;
}

DeviceObject::DeviceObject(DeviceImpl* device, const DeviceDesc& desc)
    : device_{device}
{
    assert(device != nullptr);

    // This constructor is inteded for Device class constructor, where device->desc_ is not yet initialized.
    if ((desc.flags & kDeviceFlagDisableNameStoring) == 0 && !IsStringEmpty(desc.name))
        name_ = desc.name;
}

ID3D12Device* DeviceObject::GetD3d12Device() const noexcept
{
    assert(device_ != nullptr);
    return device_->GetDevice();
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
    ASSERT_OR_RETURN((desc_.flags &
        (kBufferUsageMaskCpu | kBufferUsageMaskCopy | kBufferUsageMaskShader)) != 0,
        "At least one usage flag must be specified - a buffer with no usage flags makes no sense.");
    ASSERT_OR_RETURN(CountBitsSet(desc_.flags & kBufferUsageMaskCpu) <= 1,
        "kBufferUsageFlagCpu* are mutually exclusive - you can specify at most 1.");

    const bool is_typed = (desc_.flags & kBufferFlagTyped) != 0;
    const bool is_structured = (desc_.flags & kBufferFlagStructured) != 0;

    const uint32_t type_bit_count = CountBitsSet(desc_.flags
        & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress));
    ASSERT_OR_RETURN(type_bit_count <= 1,
        "kBufferFlagTyped, kBufferFlagStructured, kBufferFlagByteAddress are mutually exclusive - you can specify at most 1.");

    ASSERT_OR_RETURN(desc_.size > 0 && desc_.size % 4 == 0, "Buffer size must be greater than 0 and a multiple of 4.");
    ASSERT_OR_RETURN(initial_data_size <= desc_.size, "initial_data_size exceeds buffer size.");

    if(initial_data_size > 0)
    {
        // TODO Remove this limitation in the future.
        ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0,
            "Buffer initial data can only be used with kBufferUsageCpuSequentialWrite.");
    }

    ASSERT_OR_RETURN(is_typed == (desc_.element_format != Format::kUnknown),
        "element_format should be set if and only if the buffer is used as typed buffer.");
    if(is_typed)
    {
        const FormatDesc* format_desc = GetFormatDesc(desc_.element_format);
        ASSERT_OR_RETURN(format_desc != nullptr && format_desc->bits_per_element > 0
            && format_desc->bits_per_element % 8 == 0,
            "element_format must be a valid format with size multiple of 8 bits.");
    }

    ASSERT_OR_RETURN(is_structured == (desc_.structure_size > 0),
        "structure_size should be set if and only if the buffer is used as structured buffer.");
    if(is_structured)
    {
        ASSERT_OR_RETURN(desc_.structure_size % 4 == 0, "structure_size must be a multiple of 4.");
    }

    const size_t element_size = GetElementSize();
    if(element_size > 0)
    {
        ASSERT_OR_RETURN(desc_.size % element_size == 0, "Buffer size must be a multiple of element size.");
    }

    // Choose strategy.
    if((desc_.flags & kBufferUsageFlagShaderRWResource) != 0)
    {
        strategy_ = BufferStrategy::kDefault;
        // kBufferUsageFlagCpuSequentialWrite is allowed.
        ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCpuRead) == 0,
            "kBufferUsageFlagCpuRead cannot be used with kBufferUsageFlagShaderRWResource.");
    }
    else if((desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0)
    {
        strategy_ = BufferStrategy::kUpload;

        ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCopyDst) == 0,
            "BufferUsageFlagCopyDst cannot be used with kBufferUsageFlagCpuSequentialWrite.");
    }
    else if((desc_.flags & kBufferUsageFlagCpuRead) != 0)
    {
        strategy_ = BufferStrategy::kReadback;

        ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCopySrc) == 0,
            "kBufferUsageFlagCopySrc cannot be used with kBufferUsageFlagCpuRead.");
        ASSERT_OR_RETURN((desc_.flags & kBufferUsageMaskShader) == 0,
            "kBufferUsageFlagShader* cannot be used with kBufferUsageFlagCpuRead.");
    }
    else
    {
        strategy_ = BufferStrategy::kDefault;
    }

    return kOK;
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
        assert(0);
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
    DeviceImpl* const dev = GetDevice();

    HRESULT hr = dev->WaitForBufferUnused(this);
    assert(SUCCEEDED(hr) && "Failed to wait for buffer unused in Buffer destructor.");

    assert(!is_user_mapped_ && "Destroying buffer that is still mapped - missing call to Device::UnmapBuffer.");

    --dev->buffer_count_;
}

size_t BufferImpl::GetElementSize() const noexcept
{
    if ((desc_.flags & kBufferFlagTyped) != 0)
    {
        assert(desc_.element_format != Format::kUnknown);
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
        ASSERT_OR_RETURN(initial_data.data != nullptr,
            "When initial_data.size > 0, initial_data pointer cannot be null.");
    }

    RETURN_IF_FAILED(InitParameters(initial_data.size));
    assert(strategy_ != BufferStrategy::kNone);

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
        assert(0);
    }
    CD3DX12_HEAP_PROPERTIES heap_props = CD3DX12_HEAP_PROPERTIES{heap_type};
    const D3D12_RESOURCE_STATES initial_state = GetInitialState(heap_type);
    RETURN_IF_FAILED(GetD3d12Device()->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE,
        &resource_desc, initial_state, nullptr, IID_PPV_ARGS(&resource_)));

    SetObjectName(resource_, desc_.name);
    desc_.name = nullptr;

    if (strategy_ == BufferStrategy::kUpload
        || strategy_ == BufferStrategy::kGpuUpload
        || strategy_ == BufferStrategy::kReadback)
    {
        RETURN_IF_FAILED(resource_->Map(0, nullptr, &persistently_mapped_ptr_));
    }

    RETURN_IF_FAILED(WriteInitialData(initial_data));

    return kOK;
}

Result BufferImpl::WriteInitialData(ConstDataSpan initial_data)
{
    if(initial_data.size == 0)
        return kFalse;

    ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0,
        "Buffer doesn't have kBufferUsageFlagCpuSequentialWrite but initial data was specified.");

    assert(persistently_mapped_ptr_ != nullptr);
    memcpy(persistently_mapped_ptr_, initial_data.data, initial_data.size);

    return kOK;
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
    DeviceImpl* const dev = GetDevice();
    HRESULT hr = dev->WaitForShaderUnused(this);
    assert(SUCCEEDED(hr) && "Failed to wait for shader unused in Shader destructor.");
    --dev->shader_count_;
}

Result ShaderImpl::Init(ConstDataSpan bytecode)
{
    DeviceImpl* const dev = GetDevice();

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = dev->main_root_signature_->GetRootSignature();
    pso_desc.CS.pShaderBytecode = bytecode.data;
    pso_desc.CS.BytecodeLength = bytecode.size;
    RETURN_IF_FAILED(dev->GetDevice()->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state_)));

    SetObjectName(pipeline_state_, desc_.name);
    desc_.name = nullptr;

    return kOK;
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
    RETURN_IF_FAILED(d3d12_dev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptor_heap_)));

    SetObjectName(descriptor_heap_, device_name, shader_visible_
        ? L"Descriptor heap (shader-visible)"
        : L"Descriptor heap (shader-invisible)");

    if(shader_visible_)
        gpu_handle_ = descriptor_heap_->GetGPUDescriptorHandleForHeapStart();
    cpu_handle_ = descriptor_heap_->GetCPUDescriptorHandleForHeapStart();

    return kOK;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpuHandleForDescriptor(uint32_t index) const noexcept
{
    assert(shader_visible_);
    assert(index < kMaxDescriptorCount);

    D3D12_GPU_DESCRIPTOR_HANDLE h = gpu_handle_;
    h.ptr += index * handle_increment_size_;
    return h;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpuHandleForDescriptor(uint32_t index) const noexcept
{
    assert(index < kMaxDescriptorCount);

    D3D12_CPU_DESCRIPTOR_HANDLE h = cpu_handle_;
    h.ptr += index * handle_increment_size_;
    return h;
}

HRESULT DescriptorHeap::AllocateDynamic(uint32_t& out_index)
{
    if(next_dynamic_descriptor_index_ == kMaxDescriptorCount)
        return kErrorTooManyObjects;
    out_index = next_dynamic_descriptor_index_++;
    return kOK;
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
    assert(param_index == kTotalParamCount);

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
    RETURN_IF_FAILED(D3D12SerializeVersionedRootSignature(&root_sig_desc, &root_sig_blob_ptr, &error_blob_ptr));
    CComPtr<ID3DBlob> root_sig_blob{root_sig_blob_ptr};
    CComPtr<ID3DBlob> error_blob{error_blob_ptr};

    RETURN_IF_FAILED(GetD3d12Device()->CreateRootSignature(0, root_sig_blob->GetBufferPointer(),
        root_sig_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));
    SetObjectName(root_signature_, L"Main root signature");

    return kOK;
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
    assert(env);

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
    if(env_ == nullptr) // Empty object, created using default constructor.
        return;

    HRESULT hr = EnsureCommandListState(CommandListState::kNone);
    assert(SUCCEEDED(hr) && "Failed to process pending command list in Device destructor.");

    DestroyStaticShaders();
    DestroyStaticBuffers();

    assert(buffer_count_ == 0 && "Destroying Device object while there are still Buffer objects not destroyed.");
    assert(shader_count_ == 0 && "Destroying Device object while there are still Shader objects not destroyed.");

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
    RETURN_IF_FAILED(buf->GetImpl()->Init(initial_data));

    out_buffer = buf.release();
    return kOK;
}

Result DeviceImpl::CreateBufferFromFile(const BufferDesc& desc, const wchar_t* initial_data_file_path,
    Buffer*& out_buffer)
{
    ASSERT_OR_RETURN(!IsStringEmpty(initial_data_file_path), "initial_data_file_path cannot be null or empty.");

    char* data_ptr = nullptr;
    size_t data_size = 0;
    RETURN_IF_FAILED(LoadFile(initial_data_file_path, data_ptr, data_size, desc.size));
    std::unique_ptr<char[]> data{ data_ptr };

    return CreateBufferFromMemory(desc, ConstDataSpan{data_ptr, data_size}, out_buffer);
}

Result DeviceImpl::CreateShaderFromMemory(const ShaderDesc& desc, ConstDataSpan bytecode,
    Shader*& out_shader)
{
    out_shader = nullptr;

    ASSERT_OR_RETURN(bytecode.data != nullptr && bytecode.size > 0,
        "Shader bytecode cannot be null or empty.");

    auto shader = std::unique_ptr<Shader>{new Shader{}};
    shader->impl_ = new ShaderImpl{ shader.get(), this, desc };
    RETURN_IF_FAILED(shader->GetImpl()->Init(bytecode));

    out_shader = shader.release();
    return kOK;
}

Result DeviceImpl::CreateShaderFromFile(const ShaderDesc& desc, const wchar_t* bytecode_file_path,
    Shader*& out_shader)
{
    ASSERT_OR_RETURN(!IsStringEmpty(bytecode_file_path), "bytecode_file_path cannot be null or empty.");

    char* data_ptr = nullptr;
    size_t data_size = 0;
    RETURN_IF_FAILED(LoadFile(bytecode_file_path, data_ptr, data_size));

    if(data_ptr == nullptr || data_size == 0)
        return kErrorUnexpected;

    std::unique_ptr<char[]> data{ data_ptr };

    return CreateShaderFromMemory(desc, ConstDataSpan{data_ptr, data_size}, out_shader);
}

Result DeviceImpl::MapBuffer(BufferImpl& buf, Range byte_range, BufferFlags cpu_usage_flag, void*& out_data_ptr,
    uint32_t command_flags)
{
    out_data_ptr = nullptr;
    ASSERT_OR_RETURN(buf.GetDevice() == this, "Buffer does not belong to this Device.");
    ASSERT_OR_RETURN(!buf.is_user_mapped_, "Device::MapBuffer called twice. Nested mapping is not supported.");
    ASSERT_OR_RETURN(buf.persistently_mapped_ptr_ != nullptr, "Cannot map this buffer.");
    ASSERT_OR_RETURN(CountBitsSet(cpu_usage_flag &
        (kBufferUsageFlagCpuSequentialWrite | kBufferUsageFlagCpuRead)) == 1,
        "cpu_usage_flag must be kBufferUsageFlagCpuSequentialWrite or kBufferUsageFlagCpuRead.");
    ASSERT_OR_RETURN((buf.desc_.flags & cpu_usage_flag) == cpu_usage_flag,
        "Buffer was not created with the kBufferUsageFlagCpu* used for mapping.");
    const bool is_reading = (buf.desc_.flags & kBufferUsageFlagCpuRead) != 0;
    const bool is_writing = (buf.desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0;

    byte_range = LimitRange(byte_range, buf.desc_.size);
    ASSERT_OR_RETURN(byte_range.count > 0, "byte_range is empty.");
    ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf.GetSize(), "byte_range out of bounds.");

    // If the buffer is being written or read to in the current command list, execute it and wait for it to finish.
    const uint32_t conflicting_usage_flags = is_writing
        ? (kResourceUsageFlagWrite | kResourceUsageFlagRead)
        : kResourceUsageFlagWrite;
    if(resource_usage_map_.IsUsed(&buf, conflicting_usage_flags))
    {
        const uint32_t timeout = (command_flags & kCommandFlagDontWait) ? 0 : kTimeoutInfinite;
        const Result res = EnsureCommandListState(CommandListState::kNone, timeout);
        if(res != kOK)
            return res;
    }

    buf.is_user_mapped_ = true;

    out_data_ptr = (char*)buf.persistently_mapped_ptr_ + byte_range.first;
    return kOK;
}

void DeviceImpl::UnmapBuffer(BufferImpl& buf)
{
    assert(buf.GetDevice() == this && "Buffer does not belong to this Device.");
    assert(buf.is_user_mapped_ && "Device::UnmapBuffer called but the buffer wasn't mapped.");

    buf.is_user_mapped_ = false;
}

Result DeviceImpl::ReadBufferToMemory(BufferImpl& src_buf, Range src_byte_range, void* dst_memory,
    uint32_t command_flags)
{
    ASSERT_OR_RETURN(src_buf.GetDevice() == this, "Buffer does not belong to this Device.");
    ASSERT_OR_RETURN(!src_buf.is_user_mapped_, "Cannot call this command while the buffer is mapped.");

    src_byte_range = LimitRange(src_byte_range, src_buf.GetSize());
    if(src_byte_range.count == 0)
        return kFalse;

    ASSERT_OR_RETURN(dst_memory != nullptr, "dst_memory cannot be null.");
    ASSERT_OR_RETURN(src_byte_range.first < src_buf.GetSize()
        && src_byte_range.first + src_byte_range.count <= src_buf.GetSize(),
        "Source buffer region out of bounds.");

    // If the buffer is being written to in the current command list, execute it and wait for it to finish.
    // If it is only read, there is no hazard.
    if(resource_usage_map_.IsUsed(&src_buf, kResourceUsageFlagWrite))
    {
        const uint32_t timeout = (command_flags & kCommandFlagDontWait) ? 0 : kTimeoutInfinite;
        const Result res = EnsureCommandListState(CommandListState::kNone, timeout);
        if(res == kTimeout)
            return res;
        RETURN_IF_FAILED(res);
    }

    void* mapped_ptr = nullptr;
    HRESULT hr = MapBuffer(src_buf, src_byte_range, kBufferUsageFlagCpuRead, mapped_ptr);
    if(FAILED(hr))
        return hr;
    memcpy(dst_memory, (char*)mapped_ptr, src_byte_range.count);
    UnmapBuffer(src_buf);
    return kOK;
}

Result DeviceImpl::WriteMemoryToBuffer(ConstDataSpan src_data, BufferImpl& dst_buf, size_t dst_byte_offset,
    uint32_t command_flags)
{
    ASSERT_OR_RETURN(dst_buf.GetDevice() == this, "Buffer does not belong to this Device.");
    ASSERT_OR_RETURN(!dst_buf.is_user_mapped_, "Cannot call this command while the buffer is mapped.");

    if(src_data.size == 0)
        return kFalse;

    ASSERT_OR_RETURN(src_data.data != nullptr, "src_memory cannot be null.");
    ASSERT_OR_RETURN(src_data.size % 4 == 0, "src_data.size must be a multiple of 4 B.");
    ASSERT_OR_RETURN(dst_byte_offset < dst_buf.GetSize()
        && dst_byte_offset + src_data.size <= dst_buf.GetSize(),
        "Destination buffer region out of bounds.");

    if(dst_buf.strategy_ == BufferStrategy::kUpload)
    {
        // Use MapBuffer.

        // If the buffer is being written or read to in the current command list, execute it and wait for it to finish.
        if(resource_usage_map_.IsUsed(&dst_buf, kResourceUsageFlagWrite | kResourceUsageFlagRead))
        {
            const uint32_t timeout = (command_flags & kCommandFlagDontWait) ? 0 : kTimeoutInfinite;
            const Result res = EnsureCommandListState(CommandListState::kNone, timeout);
            if(res != kOK)
                return res;
        }

        void* mapped_ptr = nullptr;
        HRESULT hr = MapBuffer(dst_buf, Range{dst_byte_offset, src_data.size},
            kBufferUsageFlagCpuSequentialWrite, mapped_ptr);
        if(FAILED(hr))
            return hr;
        memcpy((char*)mapped_ptr, src_data.data, src_data.size);
        UnmapBuffer(dst_buf);
        return kOK;
    }
    else if(dst_buf.strategy_ == BufferStrategy::kDefault)
    {
        // Use WriteBufferImmediate.

        ASSERT_OR_RETURN(src_data.size <= 0x10000,
            "Writing to buffers in GPU memory is currently limited to 64 KB per call. It will be improved in the future.");

        const uint32_t timeout = (command_flags & kCommandFlagDontWait) ? 0 : kTimeoutInfinite;
        const Result res = EnsureCommandListState(CommandListState::kRecording, timeout);
        if(res != kOK)
            return res;

        RETURN_IF_FAILED(UseBuffer(dst_buf, D3D12_RESOURCE_STATE_COPY_DEST));

        const uint32_t param_count = uint32_t(src_data.size / 4);
        StackOrHeapVector<D3D12_WRITEBUFFERIMMEDIATE_PARAMETER, 8> params;
        {
            const uint32_t* src_data_u32 = reinterpret_cast<const uint32_t*>(src_data.data);
            D3D12_GPU_VIRTUAL_ADDRESS dst_gpu_address = dst_buf.GetResource()->GetGPUVirtualAddress();
            for(uint32_t param_index = 0; param_index < param_count
                ; ++param_index, ++src_data_u32, dst_gpu_address += sizeof(uint32_t))
            {
                params.Emplace(D3D12_WRITEBUFFERIMMEDIATE_PARAMETER{ dst_gpu_address, *src_data_u32 });
            }
            command_list_->WriteBufferImmediate(param_count, params.GetData(), nullptr);
        }
        return kOK;
    }
    else
    {
        assert(0);
        return kErrorUnexpected;
    }
}

Result DeviceImpl::SubmitPendingCommands()
{
    if(command_list_state_ == CommandListState::kRecording)
        RETURN_IF_FAILED(ExecuteRecordedCommands());
    return kOK;
}

Result DeviceImpl::WaitForGPU(uint32_t timeout_milliseconds)
{
    RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kNone, timeout_milliseconds));
    return kOK;
}

Result DeviceImpl::CopyBuffer(BufferImpl& src_buf, BufferImpl& dst_buf)
{
    ASSERT_OR_RETURN(src_buf.GetDevice() == this, "src_buf does not belong to this Device.");
    ASSERT_OR_RETURN(dst_buf.GetDevice() == this, "dst_buf does not belong to this Device.");
    ASSERT_OR_RETURN((src_buf.desc_.flags & kBufferUsageFlagCopySrc) != 0,
        "src_buf was not created with kBufferUsageFlagCopySource.");
    ASSERT_OR_RETURN((dst_buf.desc_.flags & kBufferUsageFlagCopyDst) != 0,
        "dst_buf was not created with kBufferUsageFlagCopyDest.");
    ASSERT_OR_RETURN(src_buf.GetSize() == dst_buf.GetSize(),
        "Source and destination buffers must have the same size.");

    RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kRecording));

    RETURN_IF_FAILED(UseBuffer(src_buf, D3D12_RESOURCE_STATE_COPY_SOURCE));
    RETURN_IF_FAILED(UseBuffer(dst_buf, D3D12_RESOURCE_STATE_COPY_DEST));

    command_list_->CopyResource(dst_buf.GetResource(), src_buf.GetResource());

    return kOK;
}

Result DeviceImpl::CopyBufferRegion(BufferImpl& src_buf, Range src_byte_range, BufferImpl& dst_buf, size_t dst_byte_offset)
{
    ASSERT_OR_RETURN(src_buf.GetDevice() == this, "src_buf does not belong to this Device.");
    ASSERT_OR_RETURN(dst_buf.GetDevice() == this, "dst_buf does not belong to this Device.");
    ASSERT_OR_RETURN((src_buf.desc_.flags & kBufferUsageFlagCopySrc) != 0,
        "src_buf was not created with kBufferUsageFlagCopySource.");
    ASSERT_OR_RETURN((dst_buf.desc_.flags & kBufferUsageFlagCopyDst) != 0,
        "dst_buf was not created with kBufferUsageFlagCopyDest.");
    src_byte_range = LimitRange(src_byte_range, src_buf.GetSize());
    ASSERT_OR_RETURN(src_byte_range.count >= 0 && src_byte_range.count % 4 == 0, "Size must be non-zero and a multiple of 4.");
    ASSERT_OR_RETURN(src_byte_range.first + src_byte_range.count <= src_buf.GetSize(), "Source buffer overflow.");
    ASSERT_OR_RETURN(dst_byte_offset + src_byte_range.count <= dst_buf.GetSize(), "Destination buffer overflow.");

    RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kRecording));

    RETURN_IF_FAILED(UseBuffer(src_buf, D3D12_RESOURCE_STATE_COPY_SOURCE));
    RETURN_IF_FAILED(UseBuffer(dst_buf, D3D12_RESOURCE_STATE_COPY_DEST));

    command_list_->CopyBufferRegion(dst_buf.GetResource(), dst_byte_offset,
        src_buf.GetResource(), src_byte_range.first, src_byte_range.count);

    return kOK;
}

Result DeviceImpl::ClearBufferToUintValues(BufferImpl& buf, const UintVec4& values, Range element_range)
{
    ASSERT_OR_RETURN(buf.GetDevice() == this,
        "ClearBufferToUintValues: Buffer does not belong to this Device.");
    ASSERT_OR_RETURN((buf.desc_.flags & kBufferUsageFlagShaderRWResource) != 0,
        "ClearBufferToUintValues: Buffer was not created with kBufferUsageFlagShaderRWResource.");
    ASSERT_OR_RETURN((buf.desc_.flags & (kBufferFlagTyped | kBufferFlagByteAddress)) != 0,
        "ClearBufferToUintValues: Buffer was not created with kBufferFlagTyped or kBufferFlagByteAddress.");

    if(element_range.count == 0)
        return kFalse;

    D3D12_GPU_DESCRIPTOR_HANDLE shader_visible_gpu_desc_handle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE shader_invisible_cpu_desc_handle = {};
    RETURN_IF_FAILED(BeginClearBufferToValues(buf, element_range,
        shader_visible_gpu_desc_handle, shader_invisible_cpu_desc_handle));

    command_list_->ClearUnorderedAccessViewUint(
        shader_visible_gpu_desc_handle, // ViewGPUHandleInCurrentHeap
        shader_invisible_cpu_desc_handle, // ViewCPUHandle
        buf.GetResource(), // pResource
        &values.x, // Values
        0, // NumRects
        nullptr); // pRects

    return kOK;
}

Result DeviceImpl::ClearBufferToFloatValues(BufferImpl& buf, const FloatVec4& values, Range element_range)
{
    ASSERT_OR_RETURN(buf.GetDevice() == this,
        "ClearBufferToFloatValues: Buffer does not belong to this Device.");
    ASSERT_OR_RETURN((buf.desc_.flags & kBufferUsageFlagShaderRWResource) != 0,
        "ClearBufferToFloatValues: Buffer was not created with kBufferUsageFlagShaderRWResource.");
    ASSERT_OR_RETURN((buf.desc_.flags & kBufferFlagTyped) != 0,
        "ClearBufferToFloatValues: Buffer was not created with kBufferFlagTyped.");

    if(element_range.count == 0)
        return kFalse;

    D3D12_GPU_DESCRIPTOR_HANDLE shader_visible_gpu_desc_handle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE shader_invisible_cpu_desc_handle = {};
    RETURN_IF_FAILED(BeginClearBufferToValues(buf, element_range,
        shader_visible_gpu_desc_handle, shader_invisible_cpu_desc_handle));

    command_list_->ClearUnorderedAccessViewFloat(
        shader_visible_gpu_desc_handle, // ViewGPUHandleInCurrentHeap
        shader_invisible_cpu_desc_handle, // ViewCPUHandle
        buf.GetResource(), // pResource
        &values.x, // Values
        0, // NumRects
        nullptr); // pRects

    return kOK;
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
    ASSERT_OR_RETURN(b_slot < MainRootSignature::kMaxCBVCount, "CBV slot out of bounds.");

    if(buf != nullptr)
        byte_range = LimitRange(byte_range, buf->GetSize());
    else
        byte_range = kEmptyRange;

    ASSERT_OR_RETURN(byte_range.first % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0
        && byte_range.count % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT == 0,
        "Constant buffer offset and size must be a multiple of 256 B.");

    Binding* binding = &binding_state_.cbv_bindings_[b_slot];
    if(binding->buffer == buf && binding->byte_range == byte_range)
        return kFalse;

    if(buf == nullptr)
    {
        *binding = Binding{};
        return kOK;
    }

    size_t alignment = buf->GetElementSize();
    if(alignment == 0)
        alignment = 4;

    ASSERT_OR_RETURN(byte_range.first % alignment == 0, "Buffer offset must be a multiple of element size.");
    ASSERT_OR_RETURN(byte_range.count > 0 && byte_range.count % alignment == 0,
        "Size must be greater than zero and a multiple of element size.");
    ASSERT_OR_RETURN(buf->GetDevice() == this, "Buffer does not belong to this Device.");
    ASSERT_OR_RETURN((buf->desc_.flags & kBufferUsageFlagShaderConstant) != 0,
        "BindConstantBuffer: Buffer was not created with kBufferUsageFlagShaderConstant.");
    ASSERT_OR_RETURN(byte_range.first < buf->GetSize(), "Buffer offset out of bounds.");
    ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf->GetSize(), "Buffer region out of bounds.");

    binding->buffer = buf;
    binding->byte_range = byte_range;
    binding->descriptor_index = UINT32_MAX;

    return kOK;
}

Result DeviceImpl::BindBuffer(uint32_t t_slot, BufferImpl* buf, Range byte_range)
{
    ASSERT_OR_RETURN(t_slot < MainRootSignature::kMaxSRVCount, "SRV slot out of bounds.");

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
        return kOK;
    }

    const uint32_t type_bit_count = CountBitsSet(buf->desc_.flags
        & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress));
    ASSERT_OR_RETURN(type_bit_count == 1,
        "Buffer must be one of: kBufferFlagTyped, kBufferFlagStructured, kBufferFlagByteAddress.");

    size_t alignment = buf->GetElementSize();
    if(alignment == 0)
        alignment = 4;

    ASSERT_OR_RETURN(byte_range.first % alignment == 0, "Buffer offset must be a multiple of element size.");
    ASSERT_OR_RETURN(byte_range.count > 0 && byte_range.count % alignment == 0,
        "Size must be greater than zero and a multiple of element size.");
    ASSERT_OR_RETURN(buf->GetDevice() == this, "Buffer does not belong to this Device.");
    ASSERT_OR_RETURN((buf->desc_.flags & kBufferUsageFlagShaderResource) != 0,
        "BindBuffer: Buffer was not created with kBufferUsageFlagGpuShaderResource.");
    ASSERT_OR_RETURN(byte_range.first < buf->GetSize(), "Buffer offset out of bounds.");
    ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf->GetSize(), "Buffer region out of bounds.");

    binding->buffer = buf;
    binding->byte_range = byte_range;
    binding->descriptor_index = UINT32_MAX;

    return kOK;
}

Result DeviceImpl::BindRWBuffer(uint32_t u_slot, BufferImpl* buf, Range byte_range)
{
    ASSERT_OR_RETURN(u_slot < MainRootSignature::kMaxUAVCount, "UAV slot out of bounds.");

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
        return kOK;
    }

    const uint32_t type_bit_count = CountBitsSet(buf->desc_.flags
        & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress));
    ASSERT_OR_RETURN(type_bit_count == 1,
        "Buffer must be one of: kBufferFlagTyped, kBufferFlagStructured, kBufferFlagByteAddress.");

    size_t alignment = buf->GetElementSize();
    if(alignment == 0)
        alignment = 4;
    ASSERT_OR_RETURN(byte_range.first % alignment == 0, "Buffer offset must be a multiple of element size.");
    ASSERT_OR_RETURN(byte_range.count > 0 && byte_range.count % alignment == 0,
        "Size must be greater than zero and a multiple of element size.");
    ASSERT_OR_RETURN(buf->GetDevice() == this, "Buffer does not belong to this Device.");
    ASSERT_OR_RETURN((buf->desc_.flags & kBufferUsageFlagShaderRWResource) != 0,
        "BindRWBuffer: Buffer was not created with kBufferUsageFlagShaderRWResource.");
    ASSERT_OR_RETURN(byte_range.first < buf->GetSize(), "Buffer offset out of bounds.");
    ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf->GetSize(), "Buffer region out of bounds.");

    binding->buffer = buf;
    binding->byte_range = byte_range;
    binding->descriptor_index = UINT32_MAX;

    return kOK;
}

Result DeviceImpl::Init()
{
    RETURN_IF_FAILED(env_->GetDeviceFactory()->CreateDevice(env_->GetAdapter1(),
        D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device_)));

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
    RETURN_IF_FAILED(device_->CreateCommandQueue(&cmd_queue_desc, IID_PPV_ARGS(&command_queue_)));
    SetObjectName(command_queue_, desc_.name, L"CommandQueue");

    RETURN_IF_FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
        IID_PPV_ARGS(&command_allocator_)));
    SetObjectName(command_allocator_, desc_.name, L"CommandAllocator");

    RETURN_IF_FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE,
        command_allocator_, nullptr, IID_PPV_ARGS(&command_list_)));
    SetObjectName(command_list_, desc_.name, L"CommandList");

    RETURN_IF_FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
    SetObjectName(fence_, desc_.name, L"Fence");

    HANDLE fence_event_handle = NULL;
    fence_event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    fence_event_.reset(fence_event_handle);

    RETURN_IF_FAILED(main_root_signature_->Init());

    RETURN_IF_FAILED(shader_visible_descriptor_heap_.Init(desc_.name));
    RETURN_IF_FAILED(shader_invisible_descriptor_heap_.Init(desc_.name));
    RETURN_IF_FAILED(CreateNullDescriptors());

    RETURN_IF_FAILED(CreateStaticBuffers());
    RETURN_IF_FAILED(CreateStaticShaders());

    desc_.name = nullptr;
    return kOK;
}

Result DeviceImpl::ExecuteRecordedCommands()
{
    assert(command_list_state_ == CommandListState::kRecording);

    RETURN_IF_FAILED(command_list_->Close());

    ID3D12CommandList* command_lists[] = { command_list_ };
    command_queue_->ExecuteCommandLists(1, command_lists);

    ++submitted_fence_value_;
    RETURN_IF_FAILED(command_queue_->Signal(fence_, submitted_fence_value_));

    command_list_state_ = CommandListState::kExecuting;

    return kOK;
}

Result DeviceImpl::WaitForCommandExecution(uint32_t timeout_milliseconds)
{
    assert(command_list_state_ == CommandListState::kExecuting);

    if(fence_->GetCompletedValue() < submitted_fence_value_)
    {
        RETURN_IF_FAILED(fence_->SetEventOnCompletion(submitted_fence_value_, fence_event_.get()));
        const DWORD wait_result = WaitForSingleObject(fence_event_.get(), timeout_milliseconds);
        switch(wait_result)
        {
        case WAIT_OBJECT_0:
            // Waiting succeeded, event was is signaled (and got automatically reset to unsignaled).
            break;
        case WAIT_TIMEOUT:
            return kTimeout;
        default: // Most likely WAIT_FAILED.
            return MakeResultFromLastError();
        }
    }

    command_list_state_ = CommandListState::kNone;
    resource_usage_map_.map_.clear();
    shader_usage_set_.clear();

    return kOK;
}

Result DeviceImpl::ResetCommandListForRecording()
{
    assert(command_list_state_ == CommandListState::kNone);

    binding_state_.ResetDescriptors();
    shader_invisible_descriptor_heap_.ClearDynamic();
    shader_visible_descriptor_heap_.ClearDynamic();

    RETURN_IF_FAILED(command_allocator_->Reset());
    RETURN_IF_FAILED(command_list_->Reset(command_allocator_, nullptr));

    command_list_state_ = CommandListState::kRecording;

    return kOK;
}

Result DeviceImpl::EnsureCommandListState(CommandListState desired_state, uint32_t timeout_milliseconds)
{
    if(desired_state == command_list_state_)
        return kOK;
    if(command_list_state_ == CommandListState::kRecording)
        RETURN_IF_FAILED(ExecuteRecordedCommands());
    if(desired_state == command_list_state_)
        return kOK;
    if(command_list_state_ == CommandListState::kExecuting)
        RETURN_IF_FAILED(WaitForCommandExecution(timeout_milliseconds));
    if(desired_state == command_list_state_)
        return kOK;
    if(command_list_state_ == CommandListState::kNone)
        RETURN_IF_FAILED(ResetCommandListForRecording());
    return kOK;
}

Result DeviceImpl::WaitForBufferUnused(BufferImpl* buf)
{
    ASSERT_OR_RETURN(!binding_state_.IsBufferBound(buf), "Buffer is still bound.");

    if(resource_usage_map_.map_.find(buf) != resource_usage_map_.map_.end())
        return EnsureCommandListState(CommandListState::kNone);
    return kOK;
}

Result DeviceImpl::WaitForShaderUnused(ShaderImpl* shader)
{
    if(shader_usage_set_.find(shader) != shader_usage_set_.end())
        return EnsureCommandListState(CommandListState::kNone);
    return kOK;
}

Result DeviceImpl::UseBuffer(BufferImpl& buf, D3D12_RESOURCE_STATES state)
{
    assert(command_list_state_ == CommandListState::kRecording);

    ASSERT_OR_RETURN(!buf.is_user_mapped_, "Cannot use a buffer on the GPU while it is mapped.");

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
        assert(0);
    }

    const auto it = resource_usage_map_.map_.find(&buf);
    // Buffer wasn't used in this command list before.
    // No barrier issued - rely on automatic state promotion.
    if(it == resource_usage_map_.map_.end())
    {
        resource_usage_map_.map_.emplace(&buf, ResourceUsage{usage_flags, state});
        return kOK;
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
            barrier.Transition.pResource = buf.GetResource();
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
            barrier.UAV.pResource = buf.GetResource();
            command_list_->ResourceBarrier(1, &barrier);
        }
    }

    it->second.flags |= usage_flags;
    it->second.last_state = state;

    return kOK;
}

Result DeviceImpl::UpdateRootArguments()
{
    assert(command_list_state_ == CommandListState::kRecording);

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
            RETURN_IF_FAILED(UseBuffer(*binding.buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

            if(binding.descriptor_index == UINT32_MAX)
            {
                RETURN_IF_FAILED(shader_visible_descriptor_heap_.AllocateDynamic(binding.descriptor_index));

                D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
                cbv_desc.BufferLocation = binding.buffer->GetResource()->GetGPUVirtualAddress()
                    + binding.byte_range.first;
                const size_t final_size = binding.byte_range.count;
                assert(final_size <= UINT32_MAX);
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
            RETURN_IF_FAILED(UseBuffer(*binding.buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

            if(binding.descriptor_index == UINT32_MAX)
            {
                RETURN_IF_FAILED(shader_visible_descriptor_heap_.AllocateDynamic(binding.descriptor_index));

                const uint32_t buffer_type = binding.buffer->desc_.flags
                    & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress);
                assert(CountBitsSet(buffer_type) == 1);

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
                    assert(format_desc != nullptr && format_desc->bits_per_element > 0
                        && format_desc->bits_per_element % 8 == 0);

                    const size_t element_size = format_desc->bits_per_element / 8;
                    const size_t final_size = binding.byte_range.count;
                    assert(binding.byte_range.first % element_size == 0 && final_size % element_size == 0);
                    assert(final_size / element_size <= UINT32_MAX);

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
                    assert(binding.byte_range.first % structure_size == 0 && final_size % structure_size == 0);
                    assert(structure_size <= UINT32_MAX && final_size / structure_size <= UINT32_MAX);

                    srv_desc.Format = DXGI_FORMAT_UNKNOWN;
                    srv_desc.Buffer.StructureByteStride = uint32_t(structure_size);
                    srv_desc.Buffer.FirstElement = binding.byte_range.first / structure_size;
                    srv_desc.Buffer.NumElements = uint32_t(final_size / structure_size);
                    break;
                }
                case kBufferFlagByteAddress:
                {
                    const size_t final_size = binding.byte_range.count;
                    assert(binding.byte_range.first % 4 == 0 && final_size % 4 == 0);
                    assert(final_size / 4 <= UINT32_MAX);

                    srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                    srv_desc.Buffer.StructureByteStride = 0;
                    srv_desc.Buffer.FirstElement = binding.byte_range.first / 4;
                    srv_desc.Buffer.NumElements = uint32_t(final_size / 4);
                    srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                    break;
                }
                default:
                    assert(0);
                }
                device_->CreateShaderResourceView(binding.buffer->GetResource(), &srv_desc,
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
            RETURN_IF_FAILED(UseBuffer(*binding.buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

            if(binding.descriptor_index == UINT32_MAX)
            {
                RETURN_IF_FAILED(shader_visible_descriptor_heap_.AllocateDynamic(binding.descriptor_index));

                const uint32_t buffer_type = binding.buffer->desc_.flags
                    & (kBufferFlagTyped | kBufferFlagStructured | kBufferFlagByteAddress);
                assert(CountBitsSet(buffer_type) == 1);

                D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                switch(buffer_type)
                {
                case kBufferFlagTyped:
                {
                    const Format element_format = binding.buffer->desc_.element_format;
                    const DXGI_FORMAT dxgi_format = DXGI_FORMAT(element_format);
                    const FormatDesc* format_desc = GetFormatDesc(element_format);
                    assert(format_desc != nullptr && format_desc->bits_per_element > 0
                        && format_desc->bits_per_element % 8 == 0);

                    const size_t element_size = format_desc->bits_per_element / 8;
                    const size_t final_size = binding.byte_range.count;
                    assert(binding.byte_range.first % element_size == 0 && final_size % element_size == 0);
                    assert(final_size / element_size <= UINT32_MAX);

                    uav_desc.Format = dxgi_format;
                    uav_desc.Buffer.FirstElement = binding.byte_range.first / element_size;
                    uav_desc.Buffer.NumElements = uint32_t(final_size / element_size);
                    break;
                }
                case kBufferFlagStructured:
                {
                    const size_t structure_size = binding.buffer->desc_.structure_size;
                    const size_t final_size = binding.byte_range.count;
                    assert(binding.byte_range.first % structure_size == 0 && final_size % structure_size == 0);
                    assert(structure_size <= UINT32_MAX && final_size / structure_size <= UINT32_MAX);

                    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
                    uav_desc.Buffer.StructureByteStride = uint32_t(structure_size);
                    uav_desc.Buffer.FirstElement = binding.byte_range.first / structure_size;
                    uav_desc.Buffer.NumElements = uint32_t(final_size / structure_size);
                    break;
                }
                case kBufferFlagByteAddress:
                {
                    const size_t final_size = binding.byte_range.count;
                    assert(binding.byte_range.first % 4 == 0 && final_size % 4 == 0);
                    assert(final_size / 4 <= UINT32_MAX);

                    uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                    uav_desc.Buffer.StructureByteStride = 0;
                    uav_desc.Buffer.FirstElement = binding.byte_range.first / 4;
                    uav_desc.Buffer.NumElements = uint32_t(final_size / 4);
                    uav_desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
                    break;
                }
                default:
                    assert(0);
                }
                device_->CreateUnorderedAccessView(binding.buffer->GetResource(), nullptr, &uav_desc,
                    shader_visible_descriptor_heap_.GetCpuHandleForDescriptor(binding.descriptor_index));
            }

            command_list_->SetComputeRootDescriptorTable(root_param_index,
                shader_visible_descriptor_heap_.GetGpuHandleForDescriptor(binding.descriptor_index));
        }
    }

    return kOK;
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

    return kOK;
}

Result DeviceImpl::CreateStaticShaders()
{
    Singleton& singleton = Singleton::GetInstance();

    ASSERT_OR_RETURN(singleton.static_shaders_.empty() || singleton.dev_count_ == 1,
        "Static shaders can only be used when at most 1 Device is created at a time.");

    for(StaticShader* static_shader : singleton.static_shaders_)
    {
        RETURN_IF_FAILED(static_shader->Init());
    }
    return kOK;
}

Result DeviceImpl::CreateStaticBuffers()
{
    Singleton& singleton = Singleton::GetInstance();

    ASSERT_OR_RETURN(singleton.static_buffers_.empty() || singleton.dev_count_ == 1,
        "Static buffers can only be used when at most 1 Device is created at a time.");

    for(StaticBuffer* static_buffer: singleton.static_buffers_)
    {
        RETURN_IF_FAILED(static_buffer->Init());
    }
    return kOK;
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

    ASSERT_OR_RETURN(buf.GetDevice() == this, "buf does not belong to this Device.");

    RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kRecording));

    ID3D12DescriptorHeap* const desc_heap = shader_visible_descriptor_heap_.GetDescriptorHeap();
    command_list_->SetDescriptorHeaps(1, &desc_heap);

    RETURN_IF_FAILED(UseBuffer(buf, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    uint32_t shader_visible_desc_index = UINT32_MAX;
    uint32_t shader_invisible_desc_index = UINT32_MAX;
    RETURN_IF_FAILED(shader_visible_descriptor_heap_.AllocateDynamic(shader_visible_desc_index));
    RETURN_IF_FAILED(shader_invisible_descriptor_heap_.AllocateDynamic(shader_invisible_desc_index));

    const size_t buf_size = buf.GetSize();

    DXGI_FORMAT dxgi_format;
    size_t element_size;
    if((buf.desc_.flags & kBufferFlagTyped) != 0)
    {
        const Format element_format = buf.desc_.element_format;
        dxgi_format = DXGI_FORMAT(element_format);
        const FormatDesc* format_desc = GetFormatDesc(element_format);
        RETURN_IF_FAILED(format_desc != nullptr && format_desc->bits_per_element > 0
            && format_desc->bits_per_element % 8 == 0);
        element_size = format_desc->bits_per_element / 8;
    }
    else
    {
        RETURN_IF_FAILED((buf.desc_.flags & kBufferFlagByteAddress) != 0);
        dxgi_format = DXGI_FORMAT_R32_UINT;
        element_size = sizeof(uint32_t);
    }

    element_range = LimitRange(element_range, buf_size / element_size);
    ASSERT_OR_RETURN(element_range.first * element_size < buf_size
        && (element_range.first + element_range.count) * element_size <= buf_size,
        "Element range out of bounds.");
    ASSERT_OR_RETURN(element_range.count <= UINT32_MAX, "Element count exceeds UINT32_MAX.");

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

    ID3D12Resource* const d3d12_res = buf.GetResource();
    assert(d3d12_res != nullptr);
    device_->CreateUnorderedAccessView(d3d12_res, nullptr, &uav_desc, shader_visible_cpu_desc_handle);
    device_->CreateUnorderedAccessView(d3d12_res, nullptr, &uav_desc, out_shader_invisible_cpu_desc_handle);

    return kOK;
}

Result DeviceImpl::DispatchComputeShader(ShaderImpl& shader, const UintVec3& group_count)
{
    ASSERT_OR_RETURN(shader.GetDevice() == this, "Shader does not belong to this Device.");

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
    ASSERT_OR_RETURN(group_count.x <= UINT16_MAX
        && group_count.y <= UINT16_MAX
        && group_count.z <= UINT16_MAX, "Dispatch group count cannot exceed 65535 in any dimension.");

    RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kRecording));

    ID3D12DescriptorHeap* const desc_heap = shader_visible_descriptor_heap_.GetDescriptorHeap();
    command_list_->SetDescriptorHeaps(1, &desc_heap);

    command_list_->SetPipelineState(shader.GetPipelineState());

    command_list_->SetComputeRootSignature(main_root_signature_->GetRootSignature());

    RETURN_IF_FAILED(UpdateRootArguments());

    command_list_->Dispatch(group_count.x, group_count.y, group_count.z);

    return kOK;
}

////////////////////////////////////////////////////////////////////////////////
// class ShaderCompiler

Result ShaderCompiler::Init()
{
    module_ = LoadLibrary(L"c:/Libraries/_Microsoft/dxc_2025_07_14/bin/x64/dxcompiler.dll");
    if (module_ == NULL)
        return MakeResultFromLastError();

    create_instance_proc_ = DxcCreateInstanceProc(GetProcAddress(module_, "DxcCreateInstance"));
    if(create_instance_proc_ == nullptr)
        return kErrorFail;

    RETURN_IF_FAILED(create_instance_proc_(CLSID_DxcUtils, IID_PPV_ARGS(&utils_)));
    RETURN_IF_FAILED(create_instance_proc_(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_)));

    RETURN_IF_FAILED(utils_->CreateDefaultIncludeHandler(&include_handler_));

    return kOK;
}

////////////////////////////////////////////////////////////////////////////////
// class EnvironmentImpl

EnvironmentImpl::EnvironmentImpl(Environment* interface_obj, const EnvironmentDesc& desc)
    : interface_obj_{interface_obj}
{
    Singleton& singleton = Singleton::GetInstance();
    assert(singleton.env_ == nullptr && "Only one Environment instance can be created.");
    singleton.env_ = this;
}

EnvironmentImpl::~EnvironmentImpl()
{
    assert(device_count_ == 0 && "Destroying Environment object while there are still Device objects not destroyed.");
    Singleton::GetInstance().env_ = nullptr;
}

Result EnvironmentImpl::CreateDevice(const DeviceDesc& desc, Device*& out_device)
{
    out_device = nullptr;

    auto device = std::unique_ptr<Device>{new Device{}};
    device->impl_ = new DeviceImpl(device.get(), this, desc);
    RETURN_IF_FAILED(device->impl_->Init());

    out_device = device.release();
    return kOK;
}

Result EnvironmentImpl::Init(const EnvironmentDesc& desc)
{
    ASSERT_OR_RETURN(!IsStringEmpty(desc.d3d12_dll_path),
        "EnvironmentDesc::d3d12_dll_path cannot be null or empty.");

    RETURN_IF_FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi_factory6_)));

    for(UINT adapter_index = 0; ; ++adapter_index)
    {
        CComPtr<IDXGIAdapter1> adapter1;
        HRESULT hr = dxgi_factory6_->EnumAdapterByGpuPreference(adapter_index,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter1));
        if(FAILED(hr))
            break;

        DXGI_ADAPTER_DESC1 adapter_desc = {};
        RETURN_IF_FAILED(adapter1->GetDesc1(&adapter_desc));

        selected_adapter_index_ = adapter_index;
        adapter_ = std::move(adapter1);
        break;
    }

    ASSERT_OR_RETURN(selected_adapter_index_ != UINT32_MAX, "Adapter not found.");

    RETURN_IF_FAILED(D3D12GetInterface(CLSID_D3D12SDKConfiguration, IID_PPV_ARGS(&sdk_config1_)));

    uint32_t sdk_version = desc.is_d3d12_agility_sdk_preview
        ? D3D12_PREVIEW_SDK_VERSION : D3D12_SDK_VERSION;
    RETURN_IF_FAILED(sdk_config1_->CreateDeviceFactory(sdk_version, desc.d3d12_dll_path,
        IID_PPV_ARGS(&device_factory_)));

    RETURN_IF_FAILED(shader_compiler_.Init());

    return kOK;
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
    assert(impl_ != nullptr);
    return impl_->GetDevice()->GetInterface();
}

const wchar_t* Buffer::GetName() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetName();
}

size_t Buffer::GetSize() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->desc_.size;
}

uint32_t Buffer::GetFlags() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->desc_.flags;
}

Format Buffer::GetElementFormat() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->desc_.element_format;
}

size_t Buffer::GetStructureSize() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->desc_.structure_size;
}

size_t Buffer::GetElementSize() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetElementSize();
}

void* Buffer::GetResource() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetResource();
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
    assert(impl_ != nullptr);
    return impl_->GetDevice()->GetInterface();
}

const wchar_t* Shader::GetName() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetName();
}

void* Shader::GetNativePipelineState() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetPipelineState();
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
    assert(impl_ != nullptr);
    return impl_->GetEnvironment()->GetInterface();
}

void* Device::GetNativeDevice() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetDevice();
}

Result Device::CreateBuffer(const BufferDesc& desc, Buffer*& out_buffer)
{
    assert(impl_ != nullptr);
    return impl_->CreateBuffer(desc, out_buffer);
}

Result Device::CreateBufferFromMemory(const BufferDesc& desc, ConstDataSpan initial_data,
    Buffer*& out_buffer)
{
    assert(impl_ != nullptr);
    return impl_->CreateBufferFromMemory(desc, initial_data, out_buffer);
}

Result Device::CreateBufferFromFile(const BufferDesc& desc, const wchar_t* initial_data_file_path,
    Buffer*& out_buffer)
{
    assert(impl_ != nullptr);
    return impl_->CreateBufferFromFile(desc, initial_data_file_path, out_buffer);
}

Result Device::CreateShaderFromMemory(const ShaderDesc& desc, ConstDataSpan bytecode,
    Shader*& out_shader)
{
    assert(impl_ != nullptr);
    return impl_->CreateShaderFromMemory(desc, bytecode, out_shader);
}

Result Device::CreateShaderFromFile(const ShaderDesc& desc, const wchar_t* bytecode_file_path,
    Shader*& out_shader)
{
    assert(impl_ != nullptr);
    return impl_->CreateShaderFromFile(desc, bytecode_file_path, out_shader);
}

Result Device::MapBuffer(Buffer& buf, Range byte_range, BufferFlags cpu_usage_flag, void*& out_data_ptr,
    uint32_t command_flags)
{
    assert(impl_ != nullptr && buf.GetImpl() != nullptr);
    return impl_->MapBuffer(*buf.GetImpl(), byte_range, cpu_usage_flag, out_data_ptr, command_flags);
}

void Device::UnmapBuffer(Buffer& buf)
{
    assert(impl_ != nullptr && buf.GetImpl() != nullptr);
    impl_->UnmapBuffer(*buf.GetImpl());
}

Result Device::ReadBufferToMemory(Buffer& src_buf, Range src_byte_range, void* dst_memory,
    uint32_t command_flags)
{
    assert(impl_ != nullptr && src_buf.GetImpl() != nullptr);
    return impl_->ReadBufferToMemory(*src_buf.GetImpl(), src_byte_range, dst_memory, command_flags);
}

Result Device::WriteMemoryToBuffer(ConstDataSpan src_data, Buffer& dst_buf, size_t dst_byte_offset,
    uint32_t command_flags)
{
    assert(impl_ != nullptr && dst_buf.GetImpl() != nullptr);
    return impl_->WriteMemoryToBuffer(src_data, *dst_buf.GetImpl(), dst_byte_offset, command_flags);
}

Result Device::SubmitPendingCommands()
{
    assert(impl_ != nullptr);
    return impl_->SubmitPendingCommands();
}

Result Device::WaitForGPU(uint32_t timeout_milliseconds)
{
    assert(impl_ != nullptr);
    return impl_->WaitForGPU(timeout_milliseconds);
}

Result Device::CopyBuffer(Buffer& src_buf, Buffer& dst_buf)
{
    assert(impl_ != nullptr && src_buf.GetImpl() != nullptr && dst_buf.GetImpl() != nullptr);
    return impl_->CopyBuffer(*src_buf.GetImpl(), *dst_buf.GetImpl());
}

Result Device::CopyBufferRegion(Buffer& src_buf, Range src_byte_range, Buffer& dst_buf, size_t dst_byte_offset)
{
    assert(impl_ != nullptr && src_buf.GetImpl() != nullptr && dst_buf.GetImpl() != nullptr);
    return impl_->CopyBufferRegion(*src_buf.GetImpl(), src_byte_range, *dst_buf.GetImpl(), dst_byte_offset);
}

Result Device::ClearBufferToUintValues(Buffer& buf, const UintVec4& values, Range element_range)
{
    assert(impl_ != nullptr && buf.GetImpl() != nullptr);
    return impl_->ClearBufferToUintValues(*buf.GetImpl(), values, element_range);
}

Result Device::ClearBufferToFloatValues(Buffer& buf, const FloatVec4& values, Range element_range)
{
    assert(impl_ != nullptr && buf.GetImpl() != nullptr);
    return impl_->ClearBufferToFloatValues(*buf.GetImpl(), values, element_range);
}

void Device::ResetAllBindings()
{
    assert(impl_ != nullptr);
    impl_->ResetAllBindings();
}

Result Device::BindConstantBuffer(uint32_t b_slot, Buffer* buf, Range byte_range)
{
    assert(impl_ != nullptr);
    assert(buf == nullptr || buf->GetImpl() != nullptr);
    return impl_->BindConstantBuffer(b_slot, buf ? buf->GetImpl() : nullptr, byte_range);
}

Result Device::BindBuffer(uint32_t t_slot, Buffer* buf, Range byte_range)
{
    assert(impl_ != nullptr);
    assert(buf == nullptr || buf->GetImpl() != nullptr);
    return impl_->BindBuffer(t_slot, buf ? buf->GetImpl() : nullptr, byte_range);
}

Result Device::BindRWBuffer(uint32_t u_slot, Buffer* buf, Range byte_range)
{
    assert(impl_ != nullptr);
    assert(buf == nullptr || buf->GetImpl() != nullptr);
    return impl_->BindRWBuffer(u_slot, buf ? buf->GetImpl() : nullptr, byte_range);
}

Result Device::DispatchComputeShader(Shader& shader, const UintVec3& group_count)
{
    assert(impl_ != nullptr && shader.GetImpl() != nullptr);
    return impl_->DispatchComputeShader(*shader.GetImpl(), group_count);
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticShader

StaticShader::StaticShader()
{
    Singleton& singleton = Singleton::GetInstance();
    assert(singleton.env_ == nullptr &&
        "StaticShader objects can only be created before Environment object is created.");
    singleton.static_shaders_.push_back(this);
}

StaticShader::StaticShader(const ShaderDesc& desc)
    : desc_{ desc }
{
    Singleton& singleton = Singleton::GetInstance();
    assert(singleton.env_ == nullptr &&
        "StaticShader objects can only be created before Environment object is created.");
    singleton.static_shaders_.push_back(this);
}

StaticShader::~StaticShader()
{
    assert(shader_ == nullptr
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
    assert(!shader_ && "Cannot call StaticShaderFromMemory::Set when the shader is already created.");

    desc_ = desc;
    bytecode_ = bytecode;
}

Result StaticShaderFromMemory::Init()
{
    if(!IsSet())
        return kFalse;

    Singleton& singleton = Singleton::GetInstance();
    Device* dev = singleton.first_dev_;
    assert(dev != nullptr && singleton.dev_count_ == 1);
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
    assert(!shader_ && "Cannot call StaticShaderFromFile::Set when the shader is already created.");

    desc_ = desc;
    bytecode_file_path_ = bytecode_file_path;
}

Result StaticShaderFromFile::Init()
{
    if(!IsSet())
        return kFalse;

    Singleton& singleton = Singleton::GetInstance();
    Device* dev = singleton.first_dev_;
    assert(dev != nullptr && singleton.dev_count_ == 1);
    return dev->CreateShaderFromFile(desc_, bytecode_file_path_, shader_);
}

////////////////////////////////////////////////////////////////////////////////
// Public class StaticBuffer

StaticBuffer::StaticBuffer()
{
    Singleton& singleton = Singleton::GetInstance();
    assert(singleton.env_ == nullptr &&
        "StaticBuffer objects can only be created before Environment object is created.");
    singleton.static_buffers_.push_back(this);
}

StaticBuffer::StaticBuffer(const BufferDesc& desc)
    : desc_{ desc }
{
    Singleton& singleton = Singleton::GetInstance();
    assert(singleton.env_ == nullptr &&
        "StaticBuffer objects can only be created before Environment object is created.");
    singleton.static_buffers_.push_back(this);
}

StaticBuffer::~StaticBuffer()
{
    assert(buffer_ == nullptr
        && "StaticBuffer objects lifetime must extend beyond the lifetime of the main Environment object.");

    // Intentionally not removing itself from Singleton::static_buffers_ because the order
    // of global object destruction may be undefined.
}

void StaticBuffer::Set(const BufferDesc& desc)
{
    assert(!buffer_ && "Cannot call StaticBuffer::Set when the buffer is already created.");
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
    assert(dev != nullptr && singleton.dev_count_ == 1);
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
    assert(dev != nullptr && singleton.dev_count_ == 1);
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
    assert(dev != nullptr && singleton.dev_count_ == 1);
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

void* Environment::GetNativeDXGIFactory6() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetDXGIFactory6();
}

void* Environment::GetNativeAdapter1() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetAdapter1();
}

void* Environment::GetNativeSDKConfiguration1() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetSDKConfiguration1();
}

void* Environment::GetNativeDeviceFactory() const noexcept
{
    assert(impl_ != nullptr);
    return impl_->GetDeviceFactory();
}

Result Environment::CreateDevice(const DeviceDesc& desc, Device*& out_device)
{
    assert(impl_ != nullptr);
    return impl_->CreateDevice(desc, out_device);
}

////////////////////////////////////////////////////////////////////////////////
// Public global functions

Result CreateEnvironment(const EnvironmentDesc& desc, Environment*& out_env)
{
    out_env = nullptr;

    auto env = std::unique_ptr<Environment>{new Environment{}};
    env->impl_ = new EnvironmentImpl{env.get(), desc};
    RETURN_IF_FAILED(env->impl_->Init(desc));

    out_env = env.release();
    return kOK;
}

} // namespace jd3d12
