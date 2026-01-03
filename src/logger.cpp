// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.
//
// Code in files "logger.*" is based on:
// https://github.com/sawickiap/MISC/tree/master/PrintStream

#include "logger.hpp"
#include "internal_utils.hpp"
#include <jd3d12/utils.hpp>

namespace jd3d12
{

////////////////////////////////////////////////////////////////////////////////
// class PrintStream

void PrintStream::Print(const wchar_t* str)
{
    Print(str, wcslen(str));
}

void PrintStream::Print(const wchar_t* str, size_t str_len)
{
    if(str_len == 0)
        return;

    // String is null-terminated.
    if(str[str_len - 1] == 0)
        Print(str);
    else
    {
        StackOrHeapVector<wchar_t, 128> buf(str_len + 1);
        wchar_t* buf_data = buf.GetData();
        memcpy(buf_data, str, str_len * sizeof(wchar_t));
        buf_data[str_len] = 0;
        Print(buf_data);
    }
}

void PrintStream::Print(const std::wstring& str)
{
    Print(str.c_str(), str.length());
}

void PrintStream::VPrintF(const wchar_t* format, va_list arg_list)
{
    size_t dst_len = (size_t)::_vscwprintf(format, arg_list);
    if(dst_len)
    {
        StackOrHeapVector<wchar_t, 128> buf(dst_len + 1);
        wchar_t* buf_data = buf.GetData();
        ::vswprintf_s(buf_data, dst_len + 1, format, arg_list);
        Print(buf_data, dst_len);
    }

}

void PrintStream::PrintF(const wchar_t* format, ...)
{
    va_list arg_list;
    va_start(arg_list, format);
    VPrintF(format, arg_list);
    va_end(arg_list);
}

////////////////////////////////////////////////////////////////////////////////
// class ConsolePrintStream

void ConsolePrintStream::Print(const wchar_t* str, size_t str_len)
{
    JD3D12_ASSERT(str_len <= INT_MAX);
    ::wprintf(L"%.*s", (int)str_len, str);
}

void ConsolePrintStream::Print(const wchar_t* str)
{
    ::wprintf(L"%s", str);
}

void ConsolePrintStream::VPrintF(const wchar_t* format, va_list arg_list)
{
    ::vwprintf(format, arg_list);
}

////////////////////////////////////////////////////////////////////////////////
// class FilePrintStream

FilePrintStream::FilePrintStream() :
    file_(nullptr)
{
}

FilePrintStream::FilePrintStream(const wchar_t* file_path, const wchar_t* mode) :
    file_(nullptr)
{
    Open(file_path, mode);
}

FilePrintStream::~FilePrintStream()
{
    Close();
}

bool FilePrintStream::Open(const wchar_t* file_path, const wchar_t* mode)
{
    Close();
    bool success = _wfopen_s(&file_, file_path, mode) == 0;
    if(!success)
    {
        file_ = nullptr;
        // Handle error somehow.
        JD3D12_ASSERT(0);
    }
    return success;
}

void FilePrintStream::Close()
{
    if(file_)
    {
        fclose(file_);
        file_ = nullptr;
    }
}

void FilePrintStream::Print(const wchar_t* str, size_t str_len)
{
    if(IsOpened())
    {
        JD3D12_ASSERT(str_len <= INT_MAX);
        ::fwprintf(file_, L"%.*s", (int)str_len, str);
    }
    else
        JD3D12_ASSERT(0);
}

void FilePrintStream::Print(const wchar_t* str)
{
    if(IsOpened())
        ::fwprintf(file_, L"%s", str);
    else
        JD3D12_ASSERT(0);
}

void FilePrintStream::VPrintF(const wchar_t* format, va_list arg_list)
{
    if(IsOpened())
        ::vfwprintf(file_, format, arg_list);
    else
        JD3D12_ASSERT(0);
}

////////////////////////////////////////////////////////////////////////////////
// class DebugPrintStream

void DebugPrintStream::Print(const wchar_t* str)
{
    OutputDebugStringW(str);
}

////////////////////////////////////////////////////////////////////////////////
// class Logger

Result Logger::IsNeeded(const EnvironmentDesc& env_desc, bool& out_is_needed)
{
    out_is_needed = false;

    if((env_desc.flags & kEnvironmentMaskLog) == 0 || env_desc.log_severity == 0)
        return kSuccess;

    const uint32_t log_bits_set = CountBitsSet(env_desc.flags & kEnvironmentMaskLog);
    JD3D12_ASSERT_OR_RETURN(log_bits_set <= 1, "At most one kEnvironmentFlagLog* can be specified.");

    if((env_desc.flags & kEnvironmentFlagLogFile) != 0)
    {
        JD3D12_ASSERT_OR_RETURN(!IsStringEmpty(env_desc.log_file_path),
            "When kEnvironmentFlagLogFile is specified, EnvironmentDesc::log_file_path cannot be empty.");
    }

    out_is_needed = log_bits_set > 0;
    return kSuccess;
}

Result Logger::Init(const EnvironmentDesc& env_desc)
{
    severity_mask_ = env_desc.log_severity;

    if((env_desc.flags & kEnvironmentFlagLogStandardOutput) != 0)
    {
        print_stream_ = std::make_unique<ConsolePrintStream>();
    }
    else if((env_desc.flags & kEnvironmentFlagLogStandardError) != 0)
    {
        // TODO
    }
    else if((env_desc.flags & kEnvironmentFlagLogDebug) != 0)
    {
        print_stream_ = std::make_unique<DebugPrintStream>();
    }
    else if((env_desc.flags & kEnvironmentFlagLogFile) != 0)
    {
        print_stream_ = std::make_unique<FilePrintStream>(env_desc.log_file_path, L"w");
    }

    return kSuccess;
}

void Logger::Log(LogSeverity severity, const wchar_t* format, ...)
{
    if((severity & severity_mask_) == 0)
        return;
    if(!print_stream_)
        return;

    va_list arg_list;
    va_start(arg_list, format);
    const std::wstring msg = SVPrintF(format, arg_list);
    va_end(arg_list);

    const wchar_t* const severity_str = GetLogSeverityString(severity);

    print_stream_->PrintF(L"[%s] %s\n", severity_str, msg.c_str());
}

} // namespace jd3d12
