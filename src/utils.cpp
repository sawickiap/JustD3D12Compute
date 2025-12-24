#include <jd3d12/utils.hpp>
#include "internal_utils.hpp"

namespace jd3d12
{

namespace
{

struct FormatDescRecord
{
    Format format;
    FormatDesc desc;
};
constexpr FormatDescRecord kFormatDescRecords[] = {
    { Format::kUnknown,
    { L"Unknown", Format::kUnknown, 0, 0, 0, 0 } },
    { Format::kR32G32B32A32_Typeless,
    { L"R32G32B32A32_Typeless", Format::kR32_Typeless, 128, 4, 4, 1 } },
    { Format::kR32G32B32A32_Float,
    { L"R32G32B32A32_Float", Format::kR32_Float, 128, 4, 4, 1 } },
    { Format::kR32G32B32A32_Uint,
    { L"R32G32B32A32_Uint", Format::kR32_Uint, 128, 4, 4, 1 } },
    { Format::kR32G32B32A32_Sint,
    { L"R32G32B32A32_Sint", Format::kR32_Sint, 128, 4, 4, 1 } },
    { Format::kR32G32B32_Typeless,
    { L"R32G32B32_Typeless", Format::kR32_Typeless, 96, 3, 3, 1 } },
    { Format::kR32G32B32_Float,
    { L"R32G32B32_Float", Format::kR32_Float, 96, 3, 3, 1 } },
    { Format::kR32G32B32_Uint,
    { L"R32G32B32_Uint", Format::kR32_Uint, 96, 3, 3, 1 } },
    { Format::kR32G32B32_Sint,
    { L"R32G32B32_Sint", Format::kR32_Sint, 96, 3, 3, 1 } },
    { Format::kR16G16B16A16_Typeless,
    { L"R16G16B16A16_Typeless", Format::kR16_Typeless, 64, 4, 4, 1 } },
    { Format::kR16G16B16A16_Float,
    { L"R16G16B16A16_Float", Format::kR16_Float, 64, 4, 4, 1 } },
    { Format::kR16G16B16A16_Unorm,
    { L"R16G16B16A16_Unorm", Format::kR16_Unorm, 64, 4, 4, 1 } },
    { Format::kR16G16B16A16_Uint,
    { L"R16G16B16A16_Uint", Format::kR16_Uint, 64, 4, 4, 1 } },
    { Format::kR16G16B16A16_Snorm,
    { L"R16G16B16A16_Snorm", Format::kR16_Snorm, 64, 4, 4, 1 } },
    { Format::kR16G16B16A16_Sint,
    { L"R16G16B16A16_Sint", Format::kR16_Sint, 64, 4, 4, 1 } },
    { Format::kR32G32_Typeless,
    { L"R32G32_Typeless", Format::kR32_Typeless, 64, 2, 2, 1 } },
    { Format::kR32G32_Float,
    { L"R32G32_Float", Format::kR32_Float, 64, 2, 2, 1 } },
    { Format::kR32G32_Uint,
    { L"R32G32_Uint", Format::kR32_Uint, 64, 2, 2, 1 } },
    { Format::kR32G32_Sint,
    { L"R32G32_Sint", Format::kR32_Sint, 64, 2, 2, 1 } },
    { Format::kR32G8X24_Typeless,
    { L"R32G8X24_Typeless", Format::kUnknown, 64, 3, 2, 0 } },
    { Format::kD32_Float_S8X24_Uint,
    { L"D32_Float_S8X24_Uint", Format::kUnknown, 64, 3, 2, 0 } },
    { Format::kR32_Float_X8X24_Typeless,
    { L"R32_Float_X8X24_Typeless", Format::kUnknown, 64, 3, 1, 0 } },
    { Format::kX32_Typeless_G8X24_Uint,
    { L"X32_Typeless_G8X24_Uint", Format::kUnknown, 64, 3, 1, 0 } },
    { Format::kR10G10B10A2_Typeless,
    { L"R10G10B10A2_Typeless", Format::kUnknown, 32, 4, 4, 0 } },
    { Format::kR10G10B10A2_Unorm,
    { L"R10G10B10A2_Unorm", Format::kUnknown, 32, 4, 4, 0 } },
    { Format::kR10G10B10A2_Uint,
    { L"R10G10B10A2_Uint", Format::kUnknown, 32, 4, 4, 0 } },
    { Format::kR11G11B10_Float,
    { L"R11G11B10_Float", Format::kUnknown, 32, 3, 3, 0 } },
    { Format::kR8G8B8A8_Typeless,
    { L"R8G8B8A8_Typeless", Format::kR8_Typeless, 32, 4, 4, 1 } },
    { Format::kR8G8B8A8_Unorm,
    { L"R8G8B8A8_Unorm", Format::kR8_Unorm, 32, 4, 4, 1 } },
    { Format::kR8G8B8A8_Unorm_sRGB,
    { L"R8G8B8A8_Unorm_sRGB", Format::kR8_Unorm, 32, 4, 4, 1 } },
    { Format::kR8G8B8A8_Uint,
    { L"R8G8B8A8_Uint", Format::kR8_Uint, 32, 4, 4, 1 } },
    { Format::kR8G8B8A8_Snorm,
    { L"R8G8B8A8_Snorm", Format::kR8_Snorm, 32, 4, 4, 1 } },
    { Format::kR8G8B8A8_Sint,
    { L"R8G8B8A8_Sint", Format::kR8_Sint, 32, 4, 4, 1 } },
    { Format::kR16G16_Typeless,
    { L"R16G16_Typeless", Format::kR16_Typeless, 32, 2, 2, 1 } },
    { Format::kR16G16_Float,
    { L"R16G16_Float", Format::kR16_Float, 32, 2, 2, 1 } },
    { Format::kR16G16_Unorm,
    { L"R16G16_Unorm", Format::kR16_Unorm, 32, 2, 2, 1 } },
    { Format::kR16G16_Uint,
    { L"R16G16_Uint", Format::kR16_Uint, 32, 2, 2, 1 } },
    { Format::kR16G16_Snorm,
    { L"R16G16_Snorm", Format::kR16_Snorm, 32, 2, 2, 1 } },
    { Format::kR16G16_Sint,
    { L"R16G16_Sint", Format::kR16_Sint, 32, 2, 2, 1 } },
    { Format::kR32_Typeless,
    { L"R32_Typeless", Format::kR32_Typeless, 32, 1, 1, 1 } },
    { Format::kD32_Float,
    { L"D32_Float", Format::kR32_Float, 32, 1, 1, 0 } },
    { Format::kR32_Float,
    { L"R32_Float", Format::kR32_Float, 32, 1, 1, 1 } },
    { Format::kR32_Uint,
    { L"R32_Uint", Format::kR32_Uint, 32, 1, 1, 1 } },
    { Format::kR32_Sint,
    { L"R32_Sint", Format::kR32_Sint, 32, 1, 1, 1 } },
    { Format::kR24G8_Typeless,
    { L"R24G8_Typeless", Format::kUnknown, 32, 2, 2, 0 } },
    { Format::kD24_Unorm_S8_Uint,
    { L"D24_Unorm_S8_Uint", Format::kUnknown, 32, 2, 2, 0 } },
    { Format::kR24_Unorm_X8_Typeless,
    { L"R24_Unorm_X8_Typeless", Format::kUnknown, 32, 2, 1, 0 } },
    { Format::kX24_Typeless_G8_Uint,
    { L"X24_Typeless_G8_Uint", Format::kUnknown, 32, 2, 1, 0 } },
    { Format::kR8G8_Typeless,
    { L"R8G8_Typeless", Format::kR8_Typeless, 16, 2, 2, 1 } },
    { Format::kR8G8_Unorm,
    { L"R8G8_Unorm", Format::kR8_Unorm, 16, 2, 2, 1 } },
    { Format::kR8G8_Uint,
    { L"R8G8_Uint", Format::kR8_Uint, 16, 2, 2, 1 } },
    { Format::kR8G8_Snorm,
    { L"R8G8_Snorm", Format::kR8_Snorm, 16, 2, 2, 1 } },
    { Format::kR8G8_Sint,
    { L"R8G8_Sint", Format::kR8_Sint, 16, 2, 2, 1 } },
    { Format::kR16_Typeless,
    { L"R16_Typeless", Format::kR16_Typeless, 16, 1, 1, 1 } },
    { Format::kR16_Float,
    { L"R16_Float", Format::kR16_Float, 16, 1, 1, 1 } },
    { Format::kD16_Unorm,
    { L"D16_Unorm", Format::kD16_Unorm, 16, 1, 1, 0 } },
    { Format::kR16_Unorm,
    { L"R16_Unorm", Format::kR16_Unorm, 16, 1, 1, 1 } },
    { Format::kR16_Uint,
    { L"R16_Uint", Format::kR16_Uint, 16, 1, 1, 1 } },
    { Format::kR16_Snorm,
    { L"R16_Snorm", Format::kR16_Snorm, 16, 1, 1, 1 } },
    { Format::kR16_Sint,
    { L"R16_Sint", Format::kR16_Sint, 16, 1, 1, 1 } },
    { Format::kR8_Typeless,
    { L"R8_Typeless", Format::kR8_Typeless, 8, 1, 1, 1 } },
    { Format::kR8_Unorm,
    { L"R8_Unorm", Format::kR8_Unorm, 8, 1, 1, 1 } },
    { Format::kR8_Uint,
    { L"R8_Uint", Format::kR8_Uint, 8, 1, 1, 1 } },
    { Format::kR8_Snorm,
    { L"R8_Snorm", Format::kR8_Snorm, 8, 1, 1, 1 } },
    { Format::kR8_Sint,
    { L"R8_Sint", Format::kR8_Sint, 8, 1, 1, 1 } },
    { Format::kA8_Unorm,
    { L"A8_Unorm", Format::kR8_Unorm, 8, 1, 1, 0 } },
    { Format::kR1_Unorm,
    { L"R1_Unorm", Format::kUnknown, 1, 1, 1, 0 } },
    { Format::kR9G9B9E5_SharedExp,
    { L"R9G9B9E5_SharedExp", Format::kUnknown, 32, 3, 3, 0 } },
    { Format::kR8G8_B8G8_Unorm,
    { L"R8G8_B8G8_Unorm", Format::kUnknown, 32, 4, 4, 0 } },
    { Format::kG8R8_G8B8_Unorm,
    { L"G8R8_G8B8_Unorm", Format::kUnknown, 32, 4, 4, 0 } },
    { Format::kBC1_Typeless,
    { L"BC1_Typeless", Format::kUnknown, 4, 4, 4, 0 } },
    { Format::kBC1_Unorm,
    { L"BC1_Unorm", Format::kUnknown, 4, 4, 4, 0 } },
    { Format::kBC1_Unorm_sRGB,
    { L"BC1_Unorm_sRGB", Format::kUnknown, 4, 4, 4, 0 } },
    { Format::kBC2_Typeless,
    { L"BC2_Typeless", Format::kUnknown, 8, 4, 4, 0 } },
    { Format::kBC2_Unorm,
    { L"BC2_Unorm", Format::kUnknown, 8, 4, 4, 0 } },
    { Format::kBC2_Unorm_sRGB,
    { L"BC2_Unorm_sRGB", Format::kUnknown, 8, 4, 4, 0 } },
    { Format::kBC3_Typeless,
    { L"BC3_Typeless", Format::kUnknown, 8, 4, 4, 0 } },
    { Format::kBC3_Unorm,
    { L"BC3_Unorm", Format::kUnknown, 8, 4, 4, 0 } },
    { Format::kBC3_Unorm_sRGB,
    { L"BC3_Unorm_sRGB", Format::kUnknown, 8, 4, 4, 0 } },
    { Format::kBC4_Typeless,
    { L"BC4_Typeless", Format::kUnknown, 4, 1, 1, 0 } },
    { Format::kBC4_Unorm,
    { L"BC4_Unorm", Format::kUnknown, 4, 1, 1, 0 } },
    { Format::kBC4_Snorm,
    { L"BC4_Snorm", Format::kUnknown, 4, 1, 1, 0 } },
    { Format::kBC5_Typeless,
    { L"BC5_Typeless", Format::kUnknown, 8, 2, 2, 0 } },
    { Format::kBC5_Unorm,
    { L"BC5_Unorm", Format::kUnknown, 8, 2, 2, 0 } },
    { Format::kBC5_Snorm,
    { L"BC5_Snorm", Format::kUnknown, 8, 2, 2, 0 } },
    { Format::kB5G6R5_Unorm,
    { L"B5G6R5_Unorm", Format::kUnknown, 16, 3, 3, 0 } },
    { Format::kB5G5R5A1_Unorm,
    { L"B5G5R5A1_Unorm", Format::kUnknown, 16, 4, 4, 0 } },
    { Format::kB8G8R8A8_Unorm,
    { L"B8G8R8A8_Unorm", Format::kR8_Unorm, 32, 4, 4, 0 } },
    { Format::kB8G8R8X8_Unorm,
    { L"B8G8R8X8_Unorm", Format::kR8_Unorm, 32, 4, 3, 0 } },
    { Format::kR10G10B10_XR_Bias_A2_Unorm,
    { L"R10G10B10_XR_Bias_A2_Unorm", Format::kUnknown, 32, 4, 4, 0 } },
    { Format::kB8G8R8A8_Typeless,
    { L"B8G8R8A8_Typeless", Format::kR8_Typeless, 32, 4, 4, 0 } },
    { Format::kB8G8R8A8_Unorm_sRGB,
    { L"B8G8R8A8_Unorm_sRGB", Format::kR8_Unorm, 32, 4, 4, 0 } },
    { Format::kB8G8R8X8_Typeless,
    { L"B8G8R8X8_Typeless", Format::kR8_Typeless, 32, 4, 3, 0 } },
    { Format::kB8G8R8X8_Unorm_sRGB,
    { L"B8G8R8X8_Unorm_sRGB", Format::kR8_Unorm, 32, 4, 3, 0 } },
    { Format::kBC6H_Typeless,
    { L"BC6H_Typeless", Format::kUnknown, 8, 3, 3, 0 } },
    { Format::kBC6H_UF16,
    { L"BC6H_UF16", Format::kUnknown, 8, 3, 3, 0 } },
    { Format::kBC6H_SF16,
    { L"BC6H_SF16", Format::kUnknown, 8, 3, 3, 0 } },
    { Format::kBC7_Typeless,
    { L"BC7_Typeless", Format::kUnknown, 8, 4, 4, 0 } },
    { Format::kBC7_Unorm,
    { L"BC7_Unorm", Format::kUnknown, 8, 4, 4, 0 } },
    { Format::kBC7_Unorm_sRGB,
    { L"BC7_Unorm_sRGB", Format::kUnknown, 8, 4, 4, 0 } },

    { Format::kB4G4R4A4_Unorm,
    { L"B4G4R4A4_Unorm", Format::kUnknown, 16, 4, 4, 0 } },
    { Format::kA4B4G4R4_Unorm,
    { L"A4B4G4R4_Unorm", Format::kUnknown, 16, 4, 4, 0 } },
};

} // anonymous namespace

Result MakeResultFromLastError()
{
    return HRESULT_FROM_WIN32(GetLastError());
}

const wchar_t* GetResultString(Result res)
{
    switch(res)
    {
    case kOK: return L"OK";
    case kFalse: return L"False";
    case kNotReady: return L"Not ready";
    case kTimeout: return L"Timeout";
    case kIncomplete: return L"Incomplete";
    case kErrorTooManyObjects: return L"Too many objects";
    case kErrorUnexpected: return L"Unexpected error";
    case kErrorNotImplemented: return L"Not implemented";
    case kErrorNoInterface: return L"No interface";
    case kErrorInvalidPointer: return L"Invalid pointer";
    case kErrorAbort: return L"Abort";
    case kErrorFail: return L"Fail";
    case kErrorAccessDenied: return L"Access denied";
    case kErrorInvalidHandle: return L"Invalid handle";
    case kErrorInvalidArgument: return L"Invalid argument";
    case kErrorOutOfMemory: return L"Out of memory";
    case kErrorPending: return L"Pending";
    case kErrorOutOfBounds: return L"Out of bounds";
    case kErrorChangedState: return L"Changed state";
    case kErrorIllegalStateChange: return L"Illegal state change";
    case kErrorIllegalMethodCall: return L"Illegal method call";
    case kErrorStringNotNullTerminated: return L"String not null-terminated";
    case kErrorIllegalDelegateAssignment: return L"Illegal delegate assignment";
    case kErrorAsyncOperationNotStarted: return L"Async operation not started";
    case kErrorApplicationExiting: return L"Application exiting";
    case kErrorApplicationViewExiting: return L"Application view exiting";
    case kErrorInvalidCall: return L"Invalid call";
    case kErrorNotFound: return L"Not found";
    case kErrorMoreData: return L"More data";
    case kErrorUnsupported: return L"Unsupported";
    case kErrorDeviceRemoved: return L"Device removed";
    case kErrorDeviceHung: return L"Device hung";
    case kErrorDeviceReset: return L"Device reset";
    case kErrorWasStillDrawing: return L"Was still drawing";
    case kErrorFrameStatisticsDisjoint: return L"Frame statistics disjoint";
    case kErrorDriverInternal: return L"Driver internal";
    case kErrorNonExclusive: return L"Non-exclusive";
    case kErrorNotCurrentlyAvailable: return L"Not currently available";
    case kErrorRemoteClientDisconnected: return L"Remote client disconnected";
    case kErrorRemoteOutOfMemory: return L"Remote out of memory";
    case kErrorAccessLost: return L"Access lost";
    case kErrorWaitTimeout: return L"Wait timeout";
    case kErrorSessionDisconnected: return L"Session disconnected";
    case kErrorCannotProtectContent: return L"Cannot protect content";
    case kErrorDxgiAccessDenied: return L"Access denied";
    case kErrorNameAlreadyExists: return L"Name already exists";
    case kErrorSdkComponentMissing: return L"SDK component missing";
    case kErrorNotCurrent: return L"Not current";
    case kErrorHwProtectionOutOfMemory: return L"Hardware protection out of memory";
    case kErrorDynamicCodePolicyViolation: return L"Dynamic code policy violation";
    case kErrorCacheCorrupt: return L"Cache corrupt";
    case kErrorCacheFull: return L"Cache full";
    case kErrorCacheHashCollision: return L"Cache hash collision";
    case kErrorAlreadyExists: return L"Already exists";
    case kErrorAdapterNotFound: return L"Adapter not found";
    case kErrorDriverVersionMismatch: return L"Driver version mismatch";
    case kErrorInvalidRedist: return L"Invalid redistributable";
    }
    return L"";
}

const FormatDesc* GetFormatDesc(Format format)
{
    constexpr Format kLastLinearIndexFormat = Format::kBC7_Unorm_sRGB;

    if(format <= kLastLinearIndexFormat)
    {
        const size_t index = size_t(format);
        assert(kFormatDescRecords[index].format == format);
        return &kFormatDescRecords[index].desc;
    }

    for(size_t i = size_t(kLastLinearIndexFormat) + 1; i < std::size(kFormatDescRecords); ++i)
    {
        if(kFormatDescRecords[i].format == format)
            return &kFormatDescRecords[i].desc;
    }

    return nullptr;
}

Result LoadFile(const wchar_t* path, char*& out_data, size_t& out_size, size_t max_size)
{
    out_data = nullptr;
    out_size = 0;

    std::unique_ptr<HANDLE, CloseHandleDeleter> file;
    file.reset(CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.get() == INVALID_HANDLE_VALUE)
        return MakeResultFromLastError();

    LARGE_INTEGER li_size = {};
    if (!GetFileSizeEx(file.get(), &li_size))
        return MakeResultFromLastError();
    const size_t size = size_t(li_size.QuadPart);

    if(size == 0)
        return kFalse;
    if (size > max_size)
        return kErrorOutOfBounds;

    std::unique_ptr<char[]> data = std::unique_ptr<char[]>(new char[size]);

    // Read in a loop because ReadFile takes a DWORD for size (32-bit).
    size_t total_read = 0;
    while (total_read < size)
    {
        const size_t remaining = size - total_read;
        const DWORD to_read = DWORD(remaining > UINT32_MAX ? UINT32_MAX : remaining);
        DWORD bytes_read = 0;
        if (!ReadFile(file.get(), data.get() + total_read, to_read, &bytes_read, nullptr))
            return MakeResultFromLastError();
        if (bytes_read == 0)
            return kErrorUnexpected;
        total_read += bytes_read;
    }

    out_data = data.release();
    out_size = size;
    return kOK;
}

} // namespace jd3d12
