// Copyright (c) 2025-2026 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#include "internal_utils.hpp"

namespace jd3d12
{

std::wstring SVPrintF(const wchar_t* format, va_list arg_list)
{
    size_t len = (size_t)_vscwprintf(format, arg_list);
    if(len == 0)
        return {};
    StackOrHeapVector<wchar_t, 256> buf{len + 1};
    vswprintf_s(buf.GetData(), len + 1, format, arg_list);
    return std::wstring{ buf.GetData(), len };
}

std::wstring SPrintF(const wchar_t* format, ...)
{
    va_list arg_list;
    va_start(arg_list, format);
    std::wstring result = SVPrintF(format, arg_list);
    va_end(arg_list);
    return result;
}

LogSeverity D3d12MessageSeverityToLogSeverity(D3D12_MESSAGE_SEVERITY severity)
{
    switch(severity)
    {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION:
        return kLogSeverityD3d12Corruption;
    case D3D12_MESSAGE_SEVERITY_ERROR:
        return kLogSeverityD3d12Error;
    case D3D12_MESSAGE_SEVERITY_WARNING:
        return kLogSeverityD3d12Warning;
    case D3D12_MESSAGE_SEVERITY_INFO:
        return kLogSeverityD3d12Info;
    default: // D3D12_MESSAGE_SEVERITY_MESSAGE
        return kLogSeverityD3d12Message;
    }
}

const wchar_t* GetD3d12MessageCategoryString(D3D12_MESSAGE_CATEGORY category)
{
    switch(category)
    {
    case D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED:
        return L"APPLICATION_DEFINED";
    case D3D12_MESSAGE_CATEGORY_MISCELLANEOUS:
        return L"MISCELLANEOUS";
    case D3D12_MESSAGE_CATEGORY_INITIALIZATION:
        return L"INITIALIZATION";
    case D3D12_MESSAGE_CATEGORY_CLEANUP:
        return L"CLEANUP";
    case D3D12_MESSAGE_CATEGORY_COMPILATION:
        return L"COMPILATION";
    case D3D12_MESSAGE_CATEGORY_STATE_CREATION:
        return L"STATE_CREATION";
    case D3D12_MESSAGE_CATEGORY_STATE_SETTING:
        return L"STATE_SETTING";
    case D3D12_MESSAGE_CATEGORY_STATE_GETTING:
        return L"STATE_GETTING";
    case D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION:
        return L"RESOURCE_MANIPULATION";
    case D3D12_MESSAGE_CATEGORY_EXECUTION:
        return L"EXECUTION";
    case D3D12_MESSAGE_CATEGORY_SHADER:
        return L"SHADER";
    default:
        return L"";
    }
}

} // namespace jd3d12
