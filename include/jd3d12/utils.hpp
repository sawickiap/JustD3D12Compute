// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#pragma once

#include <jd3d12/types.hpp>

namespace jd3d12
{

inline bool IsStringEmpty(const char* s) { return s == nullptr || s[0] == '\0'; }
inline bool IsStringEmpty(const wchar_t* s) { return s == nullptr || s[0] == L'\0'; }

/// Ensures the string is not null by returning static string "" if it was null.
inline const char* EnsureNonNullString(const char* s) { return s != nullptr ? s : ""; }
/// Ensures the string is not null by returning static string "" if it was null.
inline const wchar_t* EnsureNonNullString(const wchar_t* s) { return s != nullptr ? s : L""; }

/** \brief Returns true if the string is a valid identifier in HLSL language, suitable to be a function
name or a macro name - starts with a character `[A-Za-z_]` with next characters `[A-Za-z0-9_]`,
like `"MainShader"`, `"main_shader_123"`.
*/
bool IsHlslIdentifier(const wchar_t* s);

// Compatible with HRESULT.
typedef int32_t Result;

inline constexpr bool Succeeded(Result res) noexcept { return res >= 0; }
inline constexpr bool Failed(Result res) noexcept { return res < 0; }

constexpr uint32_t kTimeoutInfinite = 0xFFFFFFFFu;

constexpr int32_t kCustomResultBase = 0x213D0000u;
constexpr int32_t kCustomErrorBase  = 0xA13D0000u;

/** \brief Result codes used throughout the library, representing success or an error.

To be used with #Result type.
If you use WinAPI, DXGI, or D3D12 directly, these values are compatible with `HRESULT`.
Other codes not mentioned in this enum may also appear.
Negative values (most significant bit set) indicate failure, non-negative values indicate success.
*/
enum ResultCode : int32_t
{
    /// Main code for success.
    kSuccess                        = 0,
    /// Used to indicate that no work was done, e.g. after issuing a copy command with size=0.
    kFalse                          = 1,
    /// Returned when #kCommandFlagDontWait was used and the command didn't execute because
    /// it would need to wait long time.
    /// Also returned when timeout was used other than #kTimeoutInfinite and the time has passed
    /// before the operation completed.
    kNotReady                       = int32_t(kCustomResultBase | 0x1),
    kIncomplete                     = int32_t(kCustomResultBase | 0x2),
    kErrorTooManyObjects            = int32_t(kCustomErrorBase | 0x1),
    kErrorUnexpected                = int32_t(0x8000FFFFu),
    kErrorNotImplemented            = int32_t(0x80004001u),
    kErrorNoInterface               = int32_t(0x80004002u),
    kErrorInvalidPointer            = int32_t(0x80004003u),
    kErrorAbort                     = int32_t(0x80004004u),
    /// Main code for unspecified error.
    kErrorFail                      = int32_t(0x80004005u),
    kErrorAccessDenied              = int32_t(0x80070005u),
    kErrorInvalidHandle             = int32_t(0x80070006u),
    kErrorInvalidArgument           = int32_t(0x80070057u),
    kErrorOutOfMemory               = int32_t(0x8007000Eu),
    kErrorPending                   = int32_t(0x8000000Au),
    kErrorOutOfBounds               = int32_t(0x8000000Bu),
    kErrorChangedState              = int32_t(0x8000000Cu),
    kErrorIllegalStateChange        = int32_t(0x8000000Du),
    kErrorIllegalMethodCall         = int32_t(0x8000000Eu),
    kErrorStringNotNullTerminated   = int32_t(0x80000017u),
    kErrorIllegalDelegateAssignment = int32_t(0x80000018u),
    kErrorAsyncOperationNotStarted  = int32_t(0x80000019u),
    kErrorApplicationExiting        = int32_t(0x8000001Au),
    kErrorApplicationViewExiting    = int32_t(0x8000001Bu),
    kErrorInvalidCall               = int32_t(0x887A0001u),
    kErrorNotFound                  = int32_t(0x887A0002u),
    kErrorMoreData                  = int32_t(0x887A0003u),
    kErrorUnsupported               = int32_t(0x887A0004u),
    kErrorDeviceRemoved             = int32_t(0x887A0005u),
    kErrorDeviceHung                = int32_t(0x887A0006u),
    kErrorDeviceReset               = int32_t(0x887A0007u),
    kErrorWasStillDrawing           = int32_t(0x887A000Au),
    kErrorFrameStatisticsDisjoint   = int32_t(0x887A000Bu),
    kErrorDriverInternal            = int32_t(0x887A0020u),
    kErrorNonExclusive              = int32_t(0x887A0021u),
    kErrorNotCurrentlyAvailable     = int32_t(0x887A0022u),
    kErrorRemoteClientDisconnected  = int32_t(0x887A0023u),
    kErrorRemoteOutOfMemory         = int32_t(0x887A0024u),
    kErrorAccessLost                = int32_t(0x887A0026u),
    kErrorWaitTimeout               = int32_t(0x887A0027u),
    kErrorSessionDisconnected       = int32_t(0x887A0028u),
    kErrorCannotProtectContent      = int32_t(0x887A002Au),
    kErrorDxgiAccessDenied          = int32_t(0x887A002Bu),
    kErrorNameAlreadyExists         = int32_t(0x887A002Cu),
    kErrorSdkComponentMissing       = int32_t(0x887A002Du),
    kErrorNotCurrent                = int32_t(0x887A002Eu),
    kErrorHwProtectionOutOfMemory   = int32_t(0x887A0030u),
    kErrorDynamicCodePolicyViolation= int32_t(0x887A0031u),
    kErrorCacheCorrupt              = int32_t(0x887A0033u),
    kErrorCacheFull                 = int32_t(0x887A0034u),
    kErrorCacheHashCollision        = int32_t(0x887A0035u),
    kErrorAlreadyExists             = int32_t(0x887A0036u),
    kErrorAdapterNotFound           = int32_t(0x887E0001u),
    kErrorDriverVersionMismatch     = int32_t(0x887E0002u),
    kErrorInvalidRedist             = int32_t(0x887E0003u),
};

/** \brief Element formats used for typed buffers and pixel formats in textures.

If you use DXGI or D3D12 directly, these values are compatible with `DXGI_FORMAT` enum.
*/
enum class Format
{
    kUnknown                            = 0,
    kR32G32B32A32_Typeless              = 1,
    kR32G32B32A32_Float                 = 2,
    kR32G32B32A32_Uint                  = 3,
    kR32G32B32A32_Sint                  = 4,
    kR32G32B32_Typeless                 = 5,
    kR32G32B32_Float                    = 6,
    kR32G32B32_Uint                     = 7,
    kR32G32B32_Sint                     = 8,
    kR16G16B16A16_Typeless              = 9,
    kR16G16B16A16_Float                 = 10,
    kR16G16B16A16_Unorm                 = 11,
    kR16G16B16A16_Uint                  = 12,
    kR16G16B16A16_Snorm                 = 13,
    kR16G16B16A16_Sint                  = 14,
    kR32G32_Typeless                    = 15,
    kR32G32_Float                       = 16,
    kR32G32_Uint                        = 17,
    kR32G32_Sint                        = 18,
    kR32G8X24_Typeless                  = 19,
    kD32_Float_S8X24_Uint               = 20,
    kR32_Float_X8X24_Typeless           = 21,
    kX32_Typeless_G8X24_Uint            = 22,
    kR10G10B10A2_Typeless               = 23,
    kR10G10B10A2_Unorm                  = 24,
    kR10G10B10A2_Uint                   = 25,
    kR11G11B10_Float                    = 26,
    kR8G8B8A8_Typeless                  = 27,
    kR8G8B8A8_Unorm                     = 28,
    kR8G8B8A8_Unorm_sRGB                = 29,
    kR8G8B8A8_Uint                      = 30,
    kR8G8B8A8_Snorm                     = 31,
    kR8G8B8A8_Sint                      = 32,
    kR16G16_Typeless                    = 33,
    kR16G16_Float                       = 34,
    kR16G16_Unorm                       = 35,
    kR16G16_Uint                        = 36,
    kR16G16_Snorm                       = 37,
    kR16G16_Sint                        = 38,
    kR32_Typeless                       = 39,
    kD32_Float                          = 40,
    kR32_Float                          = 41,
    kR32_Uint                           = 42,
    kR32_Sint                           = 43,
    kR24G8_Typeless                     = 44,
    kD24_Unorm_S8_Uint                  = 45,
    kR24_Unorm_X8_Typeless              = 46,
    kX24_Typeless_G8_Uint               = 47,
    kR8G8_Typeless                      = 48,
    kR8G8_Unorm                         = 49,
    kR8G8_Uint                          = 50,
    kR8G8_Snorm                         = 51,
    kR8G8_Sint                          = 52,
    kR16_Typeless                       = 53,
    kR16_Float                          = 54,
    kD16_Unorm                          = 55,
    kR16_Unorm                          = 56,
    kR16_Uint                           = 57,
    kR16_Snorm                          = 58,
    kR16_Sint                           = 59,
    kR8_Typeless                        = 60,
    kR8_Unorm                           = 61,
    kR8_Uint                            = 62,
    kR8_Snorm                           = 63,
    kR8_Sint                            = 64,
    kA8_Unorm                           = 65,
    kR1_Unorm                           = 66,
    kR9G9B9E5_SharedExp                 = 67,
    kR8G8_B8G8_Unorm                    = 68,
    kG8R8_G8B8_Unorm                    = 69,
    kBC1_Typeless                       = 70,
    kBC1_Unorm                          = 71,
    kBC1_Unorm_sRGB                     = 72,
    kBC2_Typeless                       = 73,
    kBC2_Unorm                          = 74,
    kBC2_Unorm_sRGB                     = 75,
    kBC3_Typeless                       = 76,
    kBC3_Unorm                          = 77,
    kBC3_Unorm_sRGB                     = 78,
    kBC4_Typeless                       = 79,
    kBC4_Unorm                          = 80,
    kBC4_Snorm                          = 81,
    kBC5_Typeless                       = 82,
    kBC5_Unorm                          = 83,
    kBC5_Snorm                          = 84,
    kB5G6R5_Unorm                       = 85,
    kB5G5R5A1_Unorm                     = 86,
    kB8G8R8A8_Unorm                     = 87,
    kB8G8R8X8_Unorm                     = 88,
    kR10G10B10_XR_Bias_A2_Unorm         = 89,
    kB8G8R8A8_Typeless                  = 90,
    kB8G8R8A8_Unorm_sRGB                = 91,
    kB8G8R8X8_Typeless                  = 92,
    kB8G8R8X8_Unorm_sRGB                = 93,
    kBC6H_Typeless                      = 94,
    kBC6H_UF16                          = 95,
    kBC6H_SF16                          = 96,
    kBC7_Typeless                       = 97,
    kBC7_Unorm                          = 98,
    kBC7_Unorm_sRGB                     = 99,

    kB4G4R4A4_Unorm                     = 115,
    kA4B4G4R4_Unorm                     = 191,
};

/** \brief Severity level for logging.
*/
enum LogSeverity : uint16_t
{
    kLogSeverityDebug           = 0x0001,
    kLogSeverityInfo            = 0x0002,
    kLogSeverityD3d12Message    = 0x0004,
    kLogSeverityD3d12Info       = 0x0008,
    kLogSeverityWarning         = 0x0010,
    kLogSeverityD3d12Warning    = 0x0020,
    kLogSeverityError           = 0x0040,
    kLogSeverityD3d12Error      = 0x0080,
    kLogSeverityD3d12Corruption = 0x0100,
    kLogSeverityAssert          = 0x0200,
    kLogSeverityCrash           = 0x0400,

    kLogSeverityAll                = 0xFFFF,
    kLogSeverityMinDebug           = 0x0FFF,
    kLogSeverityMinInfo            = 0x0FFE,
    kLogSeverityMinD3d12Message    = 0x0FFC,
    kLogSeverityMinD3d12Info       = 0x0FF8,
    kLogSeverityMinWarning         = 0x0FF0,
    kLogSeverityMinD3d12Warning    = 0x0FE0,
    kLogSeverityMinError           = 0x0FC0,
    kLogSeverityMinD3d12Error      = 0x0F80,
    kLogSeverityMinD3d12Corruption = 0x0F00,
    kLogSeverityMinAssert          = 0x0E00,
    kLogSeverityMinCrash           = 0x0C00,
};

struct FormatDesc
{
    /// String name of the format, like "R8G8B8A8_Unorm" for kR8G8B8A8_Unorm.
    const wchar_t* name;
    /// Format of a single component. For example, for kR16G16B16A16_Float or kR16G16_Float it will be kR16_Float.
    /// If components have different sizes or don't map to one of the "R#" types (e.g. having 1, 10, 11 bits),
    /// it is set to kUnknown and is_simple = 0.
    Format component_format;
    /// Size of an element, in bits.
    unsigned bits_per_element : 16;
    /// Number of components the entire format has, among RGBA.
    unsigned component_count : 4;
    /// Number of components that are active in this format, i.e. not masked as "X".
    unsigned active_component_count : 4;
    /// 1 if the format is simple, so that its binary structure can be fully inferred from the members
    /// of this structure.
    /// 0 for non-typical formats like block-compressed (BC), SharedExp, having its components
    /// interpreted as "D", "S", "A", "X", or out for order ("BGRA") instead of simply "RGBA".
    /// 1 for "sRGB" and "Typeless" formats.
    /// 0 for any types that have active_component_count < component_count.
    unsigned is_simple : 1;
};

/** \brief Returns last error returned by a WinAPI function, converted to correct #Result.

Use it after calling a WinAPI function that declares its error state can be fetched using
`GetLastError` function.
*/
Result MakeResultFromLastError();

/** \brief Returns string representation of the given Result code.

For example, for `kErrorOutOfMemory` returns "Out of memory";

For unknown code, returns empty string "". The function never returns null.

Returned pointer shouldn't be freed - it points to a static constant string.
*/
const wchar_t* GetResultString(Result res);

/** \brief Returns string representation of the given logging severity level.

Returned string is upper case. For example, #kLogSeverityWarning returns "WARNING",
#kLogSeverityD3d12Error returns "D3D12 ERROR".

Only one bit flag should be used as input. When multiple bits are set, a string
is returned representing the highest severity among specified bits.

For unknown values, empty string "" is returned. The function never returns null.

Returned pointer shouldn't be freed - it points to a static constant string.
*/
const wchar_t* GetLogSeverityString(LogSeverity severity);

/** \brief Returns the description of given format.

It returns non-null if and only if `format` is among available items in the #Format enum.
Returned pointer shouldn't be freed. It points to an internal, static constant.
*/
const FormatDesc* GetFormatDesc(Format format);

struct Range
{
    size_t first;
    size_t count;

    bool operator==(Range& rhs) const noexcept { return first == rhs.first && count == rhs.count; }
    bool operator!=(Range& rhs) const noexcept { return first != rhs.first || count != rhs.count; }
};
constexpr Range kEmptyRange = { 0, 0 };
constexpr Range kFullRange = { 0, SIZE_MAX };

struct ConstDataSpan
{
    const void* data;
    size_t size;

    bool operator==(const ConstDataSpan& rhs) const noexcept { return data == rhs.data && size == rhs.size; }
    bool operator!=(const ConstDataSpan& rhs) const noexcept { return data != rhs.data || size != rhs.size; }
};
struct DataSpan
{
    void* data;
    size_t size;

    bool operator==(const DataSpan& rhs) const noexcept { return data == rhs.data && size == rhs.size; }
    bool operator!=(const DataSpan& rhs) const noexcept { return data != rhs.data || size != rhs.size; }
    operator ConstDataSpan() const noexcept { return ConstDataSpan{ data, size }; }
};
constexpr DataSpan kEmptyDataSpan = { nullptr, 0 };

template<typename T>
struct ArraySpan
{
    T* data;
    size_t count;
    bool operator==(const ArraySpan<T>& rhs) const noexcept { return data == rhs.data && count == rhs.count; }
    bool operator!=(const ArraySpan<T>& rhs) const noexcept { return data != rhs.data || count != rhs.count; }
};

// If range.end == SIZE_MAX, limits it to real_size.
inline Range LimitRange(Range range, size_t real_size)
{
    if(range.count == SIZE_MAX)
        range.count = real_size - range.first;
    return range;
}

/** \brief Loads binary data from file.

out_data pointer needs to be freed using operator delete[].
If the file was successfully opened but empty, returned out_data = null, out_size = 0,
and the function returns kFalse.
If the file size exceeds `max_size`, returned out_data = null, out_size = 0,
and the function returns kErrorOutOfBounds.
*/
Result LoadFile(const wchar_t* path, char*& out_data, size_t& out_size, size_t max_size = SIZE_MAX);

} // namespace jd3d12
