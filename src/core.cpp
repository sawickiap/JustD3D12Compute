#include <jd3d12/core.hpp>

#if 0
//extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION; }
//extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }
static const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
static const char* D3D12SDKPath = ".\\D3D12\\";

namespace
{

    class Singleton
    {
    public:
        Environment* env_ = nullptr;
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

} // namespace

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

Device::Device(Environment* env, const DeviceDesc& desc)
    : DeviceObject{ this, desc }
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
        singleton.first_dev_ = this;
        singleton.dev_count_ = 1;
    }
    else
    {
        ++singleton.dev_count_;
    }
}

Device::Device()
    : shader_visible_descriptor_heap_{this, nullptr, true}
    , shader_invisible_descriptor_heap_{this, nullptr, false}
{
}

Device::~Device()
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

Result Device::CreateBuffer(const BufferDesc& desc, Buffer*& out_buffer)
{
    return CreateBufferFromMemory(desc, ConstDataSpan{nullptr, 0}, out_buffer);
}

Result Device::CreateBufferFromMemory(const BufferDesc& desc, ConstDataSpan initial_data,
    Buffer*& out_buffer)
{
    out_buffer = nullptr;

    auto buf = std::unique_ptr<Buffer>(new Buffer{this, desc});
    RETURN_IF_FAILED(buf->Init(initial_data));

    out_buffer = buf.release();
    return kOK;
}

Result Device::CreateBufferFromFile(const BufferDesc& desc, const wchar_t* initial_data_file_path,
    Buffer*& out_buffer)
{
    ASSERT_OR_RETURN(!IsStringEmpty(initial_data_file_path), "initial_data_file_path cannot be null or empty.");

    char* data_ptr = nullptr;
    size_t data_size = 0;
    RETURN_IF_FAILED(LoadFile(initial_data_file_path, data_ptr, data_size, desc.size));
    std::unique_ptr<char[]> data{ data_ptr };

    return CreateBufferFromMemory(desc, ConstDataSpan{data_ptr, data_size}, out_buffer);
}

Result Device::CreateShaderFromMemory(const ShaderDesc& desc, ConstDataSpan bytecode,
    Shader*& out_shader)
{
    out_shader = nullptr;

    ASSERT_OR_RETURN(bytecode.data != nullptr && bytecode.size > 0,
        "Shader bytecode cannot be null or empty.");

    auto shader = std::unique_ptr<Shader>(new Shader{ this, desc });
    RETURN_IF_FAILED(shader->Init(bytecode));

    out_shader = shader.release();
    return kOK;
}

Result Device::CreateShaderFromFile(const ShaderDesc& desc, const wchar_t* bytecode_file_path,
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

Result Device::MapBuffer(Buffer& buf, Range byte_range, BufferFlags cpu_usage_flag, void*& out_data_ptr,
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

void Device::UnmapBuffer(Buffer& buf)
{
    assert(buf.GetDevice() == this && "Buffer does not belong to this Device.");
    assert(buf.is_user_mapped_ && "Device::UnmapBuffer called but the buffer wasn't mapped.");

    buf.is_user_mapped_ = false;
}

Result Device::ReadBufferToMemory(Buffer& src_buf, Range src_byte_range, void* dst_memory,
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

Result Device::WriteMemoryToBuffer(ConstDataSpan src_data, Buffer& dst_buf, size_t dst_byte_offset,
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

Result Device::SubmitPendingCommands()
{
    if(command_list_state_ == CommandListState::kRecording)
        RETURN_IF_FAILED(ExecuteRecordedCommands());
    return kOK;
}

Result Device::WaitForGPU(uint32_t timeout_milliseconds)
{
    RETURN_IF_FAILED(EnsureCommandListState(CommandListState::kNone, timeout_milliseconds));
    return kOK;
}

Result Device::CopyBuffer(Buffer& src_buf, Buffer& dst_buf)
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

Result Device::CopyBufferRegion(Buffer& src_buf, Range src_byte_range, Buffer& dst_buf, size_t dst_byte_offset)
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

Result Device::ClearBufferToUintValues(Buffer& buf, const UintVec4& values, Range element_range)
{
    ASSERT_OR_RETURN(buf.GetDevice() == this,
        "ClearBufferToUintValues: Buffer does not belong to this Device.");
    ASSERT_OR_RETURN((buf.desc_.flags & kBufferUsageFlagGpuReadWrite) != 0,
        "ClearBufferToUintValues: Buffer was not created with kBufferUsageFlagGpuReadWrite.");
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

Result Device::ClearBufferToFloatValues(Buffer& buf, const FloatVec4& values, Range element_range)
{
    ASSERT_OR_RETURN(buf.GetDevice() == this,
        "ClearBufferToFloatValues: Buffer does not belong to this Device.");
    ASSERT_OR_RETURN((buf.desc_.flags & kBufferUsageFlagGpuReadWrite) != 0,
        "ClearBufferToFloatValues: Buffer was not created with kBufferUsageFlagGpuReadWrite.");
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

void Device::ResetAllBindings()
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

Result Device::BindConstantBuffer(uint32_t b_slot, Buffer* buf, Range byte_range)
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
    ASSERT_OR_RETURN((buf->desc_.flags & kBufferUsageFlagGpuConstant) != 0,
        "BindConstantBuffer: Buffer was not created with kBufferUsageFlagGpuConstant.");
    ASSERT_OR_RETURN(byte_range.first < buf->GetSize(), "Buffer offset out of bounds.");
    ASSERT_OR_RETURN((buf->desc_.flags & kBufferUsageFlagGpuConstant) != 0,
        "Buffer usage does not match binding usage.");
    ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf->GetSize(), "Buffer region out of bounds.");

    binding->buffer = buf;
    binding->byte_range = byte_range;
    binding->descriptor_index = UINT32_MAX;

    return kOK;
}

Result Device::BindBuffer(uint32_t t_slot, Buffer* buf, Range byte_range)
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
    ASSERT_OR_RETURN((buf->desc_.flags & (kBufferUsageFlagGpuReadOnly | kBufferUsageFlagGpuReadWrite)) != 0,
        "BindBuffer: Buffer was not created with kBufferUsageFlagGpuReadOnly or kBufferUsageFlagGpuReadWrite.");
    ASSERT_OR_RETURN(byte_range.first < buf->GetSize(), "Buffer offset out of bounds.");
    ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf->GetSize(), "Buffer region out of bounds.");

    binding->buffer = buf;
    binding->byte_range = byte_range;
    binding->descriptor_index = UINT32_MAX;

    return kOK;
}

Result Device::BindRWBuffer(uint32_t u_slot, Buffer* buf, Range byte_range)
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
    ASSERT_OR_RETURN((buf->desc_.flags & kBufferUsageFlagGpuReadWrite) != 0,
        "BindRWBuffer: Buffer was not created with kBufferUsageFlagGpuReadWrite.");
    ASSERT_OR_RETURN(byte_range.first < buf->GetSize(), "Buffer offset out of bounds.");
    ASSERT_OR_RETURN(byte_range.first + byte_range.count <= buf->GetSize(), "Buffer region out of bounds.");

    binding->buffer = buf;
    binding->byte_range = byte_range;
    binding->descriptor_index = UINT32_MAX;

    return kOK;
}

Result Device::Init()
{
    RETURN_IF_FAILED(env_->GetDeviceFactory()->CreateDevice(env_->GetAdapter(),
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

Result Device::ExecuteRecordedCommands()
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

Result Device::WaitForCommandExecution(uint32_t timeout_milliseconds)
{
    assert(command_list_state_ == CommandListState::kExecuting);

    if(fence_->GetCompletedValue() < submitted_fence_value_)
    {
        CHECK_HR(fence_->SetEventOnCompletion(submitted_fence_value_, fence_event_.get()));
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

Result Device::ResetCommandListForRecording()
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

Result Device::EnsureCommandListState(CommandListState desired_state, uint32_t timeout_milliseconds)
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

Result Device::WaitForBufferUnused(Buffer* buf)
{
    ASSERT_OR_RETURN(!binding_state_.IsBufferBound(buf), "Buffer is still bound.");

    if(resource_usage_map_.map_.find(buf) != resource_usage_map_.map_.end())
        return EnsureCommandListState(CommandListState::kNone);
    return kOK;
}

Result Device::WaitForShaderUnused(Shader* shader)
{
    if(shader_usage_set_.find(shader) != shader_usage_set_.end())
        return EnsureCommandListState(CommandListState::kNone);
    return kOK;
}

Result Device::UseBuffer(Buffer& buf, D3D12_RESOURCE_STATES state)
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

Result Device::UpdateRootArguments()
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

Result Device::CreateNullDescriptors()
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

Result Device::CreateStaticShaders()
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

Result Device::CreateStaticBuffers()
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

void Device::DestroyStaticShaders()
{
    Singleton& singleton = Singleton::GetInstance();
    for(size_t i = singleton.static_shaders_.size(); i--; )
    {
        StaticShader* const static_shader = singleton.static_shaders_[i];
        delete static_shader->shader_;
        static_shader->shader_ = nullptr;
    }
}

void Device::DestroyStaticBuffers()
{
    Singleton& singleton = Singleton::GetInstance();
    for(size_t i = singleton.static_buffers_.size(); i--; )
    {
        StaticBuffer* const static_buffer = singleton.static_buffers_[i];
        delete static_buffer->buffer_;
        static_buffer->buffer_ = nullptr;
    }
}

Result Device::BeginClearBufferToValues(Buffer& buf, Range element_range,
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

Environment::Environment()
{
    Singleton& singleton = Singleton::GetInstance();
    assert(singleton.env_ == nullptr && "Only one Environment instance can be created.");
    singleton.env_ = this;
}

Environment::~Environment()
{
    assert(device_count_ == 0 && "Destroying Environment object while there are still Device objects not destroyed.");
    Singleton::GetInstance().env_ = nullptr;
}

Result Environment::CreateDevice(const DeviceDesc& desc, Device*& out_device)
{
    out_device = nullptr;

    auto device = std::unique_ptr<Device>(new Device{ this, desc });
    RETURN_IF_FAILED(device->Init());

    out_device = device.release();
    return kOK;
}

Result Environment::Init()
{
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

        wprintf(L"Selecting adapter: %s\n", adapter_desc.Description);
        selected_adapter_index_ = adapter_index;
        adapter_ = std::move(adapter1);
        break;
    }

    ASSERT_OR_RETURN(selected_adapter_index_ != UINT32_MAX, "Adapter not found.");

    RETURN_IF_FAILED(D3D12GetInterface(CLSID_D3D12SDKConfiguration, IID_PPV_ARGS(&sdk_config1_)));

    RETURN_IF_FAILED(sdk_config1_->CreateDeviceFactory(D3D12_SDK_VERSION, D3D12SDKPath,
        IID_PPV_ARGS(&device_factory_)));

    RETURN_IF_FAILED(shader_compiler_.Init());

    return kOK;
}

Result CreateEnvironment(Environment*& out_env)
{
    out_env = nullptr;

    auto env = std::unique_ptr<Environment>(new Environment{});
    RETURN_IF_FAILED(env->Init());

    out_env = env.release();
    return kOK;
}

Result Buffer::InitParameters(size_t initial_data_size)
{
    ASSERT_OR_RETURN((desc_.flags &
        (kBufferUsageMaskCpu | kBufferUsageMaskCopy | kBufferUsageMaskGpu)) != 0,
        "At least one usage flag must be specified - a buffer with no usage flags makes no sense.");
    ASSERT_OR_RETURN(CountBitsSet(desc_.flags & kBufferUsageMaskCpu) <= 1,
        "kBufferUsageFlagCpu* are mutually exclusive - you can specify at most 1.");
    ASSERT_OR_RETURN(CountBitsSet(desc_.flags & (kBufferUsageFlagGpuReadOnly | kBufferUsageFlagGpuReadWrite)) <= 1,
        "kBufferUsageFlagGpuReadOnly, kBufferUsageFlagGpuReadWrite are mutually exclusive - you can specify at most 1.");

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
    if((desc_.flags & kBufferUsageFlagGpuReadWrite) != 0)
    {
        strategy_ = BufferStrategy::kDefault;
        // kBufferUsageFlagCpuSequentialWrite is allowed.
        ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCpuRead) == 0,
            "kBufferUsageFlagCpuRead cannot be used with kBufferUsageFlagGpuReadWrite.");
    }
    else if((desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0)
    {
        strategy_ = BufferStrategy::kUpload;

        ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCopyDst) == 0,
            "BufferUsageFlagCopyDst cannot be used with kBufferUsageFlagCpuSequentialWrite.");
        ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagGpuReadWrite) == 0,
            "kBufferUsageFlagGpuReadWrite cannot be used with kBufferUsageFlagCpuSequentialWrite.");
    }
    else if((desc_.flags & kBufferUsageFlagCpuRead) != 0)
    {
        strategy_ = BufferStrategy::kReadback;

        ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCopySrc) == 0,
            "kBufferUsageFlagCopySrc cannot be used with kBufferUsageFlagCpuRead.");
        ASSERT_OR_RETURN((desc_.flags & kBufferUsageMaskGpu) == 0,
            "kBufferUsageFlagGpu* cannot be used with kBufferUsageFlagCpuRead.");
    }
    else
    {
        strategy_ = BufferStrategy::kDefault;
    }

    return kOK;
}

D3D12_RESOURCE_STATES Buffer::GetInitialState(D3D12_HEAP_TYPE heap_type)
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

Buffer::Buffer(Device* device, const BufferDesc& desc)
    : DeviceObject{device, desc.name}
    , desc_{desc}
{
    ++device_->buffer_count_;
}

Buffer::~Buffer()
{
    if(device_ == nullptr) // Empty object, created using default constructor.
        return;

    HRESULT hr = device_->WaitForBufferUnused(this);
    assert(SUCCEEDED(hr) && "Failed to wait for buffer unused in Buffer destructor.");

    assert(!is_user_mapped_ && "Destroying buffer that is still mapped - missing call to Device::UnmapBuffer.");

    --device_->buffer_count_;
}

size_t Buffer::GetElementSize() const noexcept
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

Result Buffer::Init(ConstDataSpan initial_data)
{
    if(initial_data.size > 0)
    {
        ASSERT_OR_RETURN(initial_data.data != nullptr,
            "When initial_data.size > 0, initial_data pointer cannot be null.");
    }

    RETURN_IF_FAILED(InitParameters(initial_data.size));
    assert(strategy_ != BufferStrategy::kNone);

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    if((desc_.flags & kBufferUsageFlagGpuReadWrite) != 0)
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
    CHECK_HR(device_->GetDevice()->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE,
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

Result Buffer::WriteInitialData(ConstDataSpan initial_data)
{
    if(initial_data.size == 0)
        return kFalse;

    ASSERT_OR_RETURN((desc_.flags & kBufferUsageFlagCpuSequentialWrite) != 0,
        "Buffer doesn't have kBufferUsageFlagCpuSequentialWrite but initial data was specified.");

    assert(persistently_mapped_ptr_ != nullptr);
    memcpy(persistently_mapped_ptr_, initial_data.data, initial_data.size);

    return kOK;
}

bool ResourceUsageMap::IsUsed(Buffer* buf, uint32_t usage_flags) const
{
    const auto it = map_.find(buf);
    if(it == map_.end())
        return false;
    return (it->second.flags & usage_flags) != 0;
}

Shader::~Shader()
{
    if(device_ == nullptr) // Empty object, created using default constructor.
        return;

    HRESULT hr = device_->WaitForShaderUnused(this);
    assert(SUCCEEDED(hr) && "Failed to wait for shader unused in Shader destructor.");
    --device_->shader_count_;
}

Shader::Shader(Device* device, const ShaderDesc& desc)
    : DeviceObject{device, desc.name}
    , desc_{desc}
{
    ++device_->shader_count_;
}

Result Shader::Init(ConstDataSpan bytecode)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = device_->main_root_signature_->GetRootSignature();
    pso_desc.CS.pShaderBytecode = bytecode.data;
    pso_desc.CS.BytecodeLength = bytecode.size;
    RETURN_IF_FAILED(device_->GetDevice()->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state_)));

    SetObjectName(pipeline_state_, desc_.name);
    desc_.name = nullptr;

    return kOK;
}

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

    RETURN_IF_FAILED(device_->GetDevice()->CreateRootSignature(0, root_sig_blob->GetBufferPointer(),
        root_sig_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature_)));
    SetObjectName(root_signature_, L"Main root signature");

    return kOK;
}

DeviceObject::DeviceObject(Device* device, const wchar_t* name)
    : device_{device}
{
    assert(device != nullptr);

    if ((device->desc_.flags & kDeviceFlagDisableNameStoring) == 0 && !IsStringEmpty(name))
        name_ = name;
}

DeviceObject::DeviceObject(Device* device, const DeviceDesc& desc)
    : device_{device}
{
    assert(device != nullptr);

    // This constructor is inteded for Device class constructor, where device->desc_ is not yet initialized.
    if ((desc.flags & kDeviceFlagDisableNameStoring) == 0 && !IsStringEmpty(desc.name))
        name_ = desc.name;
}

Result DescriptorHeap::Init(const wchar_t* device_name)
{
    assert(device_);
    constexpr D3D12_DESCRIPTOR_HEAP_TYPE heap_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    ID3D12Device* const d3d12_dev = device_->GetDevice();

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

Result Device::DispatchComputeShader(Shader& shader, const UintVec3& group_count)
{
    ASSERT_OR_RETURN(shader.GetDevice() == this, "Shader does not belong to this Device.");

    if(group_count.x == 0 || group_count.y == 0 || group_count.z == 0)
        return kFalse;

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

bool BindingState::IsBufferBound(Buffer* buf)
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

Result ShaderCompiler::Init()
{
    module_ = LoadLibrary(L"f:/Libraries/_Microsoft/dxc_2025_07_14/bin/x64/dxcompiler.dll");
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
#endif
