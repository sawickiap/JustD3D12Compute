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
// class definitions

// Abstract base class.
// Derived class must implement at least first or second version of Print method
// and share inherited ones with:
// using PrintStream::Print;
class PrintStream
{
public:
    virtual ~PrintStream() = default;

    // Default implementation copies to a temporary null-terminated string and rediects it to Print(str).
    virtual void Print(const wchar_t* str, size_t str_len);
    // Default implementation calculates length and redirects to Print(str, str_len).
    virtual void Print(const wchar_t* str);
    // Default implementation redirects to Print(str, str_len).
    virtual void Print(const std::wstring& str);
    // Default implementation formats string in memory and redirects it to Print(str, str_len).
    virtual void VPrintF(const wchar_t* format, va_list arg_list);
    // Redirects to Print(format, arg_list).
    void PrintF(const wchar_t* format, ...);

    virtual void Flush() { }

protected:
    PrintStream() = default;

private:
    JD3D12_NO_COPY_NO_MOVE_CLASS(PrintStream);
};

// Prints to standard output or standard error.
class StandardOutputPrintStream : public PrintStream
{
public:
    StandardOutputPrintStream(bool use_standard_error)
        : use_standard_error_{use_standard_error}
    {
    }
    using PrintStream::Print;
    void Print(const wchar_t* str, size_t str_len) override;
    void Print(const wchar_t* str) override;
    void VPrintF(const wchar_t* format, va_list arg_list) override;
    void Flush() override;

private:
    const bool use_standard_error_ = false;

    JD3D12_NO_COPY_NO_MOVE_CLASS(StandardOutputPrintStream);
};

// Prints to file.
class FilePrintStream : public PrintStream
{
public:
    FilePrintStream() = default;
    Result Init(const wchar_t* file_path);
    ~FilePrintStream() = default;

    using PrintStream::Print;
    void Print(const wchar_t* str, size_t str_len) override;
    void Print(const wchar_t* str) override;
    void VPrintF(const wchar_t* format, va_list arg_list) override;
    void Flush() override;

private:
    std::unique_ptr<FILE, FCloseDeleter> file_;

    JD3D12_NO_COPY_NO_MOVE_CLASS(FilePrintStream);
};

// Prints to OutputDebugString.
class DebugPrintStream : public PrintStream
{
public:
    DebugPrintStream() = default;
    using PrintStream::Print;
    void Print(const wchar_t* str) override;

private:
    JD3D12_NO_COPY_NO_MOVE_CLASS(DebugPrintStream);
};

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
// class StandardOutputPrintStream

void StandardOutputPrintStream::Print(const wchar_t* str, size_t str_len)
{
    JD3D12_ASSERT(str_len <= INT_MAX);
    ::fwprintf(use_standard_error_ ? stderr : stdout, L"%.*s", (int)str_len, str);
}

void StandardOutputPrintStream::Print(const wchar_t* str)
{
    ::fwprintf(use_standard_error_ ? stderr : stdout, L"%s", str);
}

void StandardOutputPrintStream::VPrintF(const wchar_t* format, va_list arg_list)
{
    ::vfwprintf(use_standard_error_ ? stderr : stdout, format, arg_list);
}

void StandardOutputPrintStream::Flush()
{
    fflush(use_standard_error_ ? stderr : stdout);
}

////////////////////////////////////////////////////////////////////////////////
// class FilePrintStream

Result FilePrintStream::Init(const wchar_t* file_path)
{
    FILE* f = nullptr;
    errno_t e = _wfopen_s(&f, file_path, L"wb");
    if(e != 0 || f == nullptr)
        return kErrorFail;
    file_.reset(f);

    fputws(L"\ufeff", f);

    return kSuccess;
}

void FilePrintStream::Print(const wchar_t* str, size_t str_len)
{
    JD3D12_ASSERT(str_len <= INT_MAX);
    ::fwprintf(file_.get(), L"%.*s", (int)str_len, str);
}

void FilePrintStream::Print(const wchar_t* str)
{
    ::fwprintf(file_.get(), L"%s", str);
}

void FilePrintStream::VPrintF(const wchar_t* format, va_list arg_list)
{
    ::vfwprintf(file_.get(), format, arg_list);
}

void FilePrintStream::Flush()
{
    fflush(file_.get());
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

    JD3D12_ASSERT_OR_RETURN(((env_desc.flags & kEnvironmentFlagLogFile) != 0)
        == !IsStringEmpty(env_desc.log_file_path),
        "EnvironmentDesc::log_file_path should be specified if and only if kEnvironmentFlagLogFile is specified.");

    out_is_needed = log_bits_set > 0;
    return kSuccess;
}

Logger::Logger()
{
    // Empty.
}

Logger::~Logger()
{
    // Empty.
}

Result Logger::Init(const EnvironmentDesc& env_desc)
{
    severity_mask_ = env_desc.log_severity;

    if((env_desc.flags & kEnvironmentFlagLogStandardOutput) != 0)
    {
        print_stream_ = std::make_unique<StandardOutputPrintStream>(false);
    }
    else if((env_desc.flags & kEnvironmentFlagLogStandardError) != 0)
    {
        print_stream_ = std::make_unique<StandardOutputPrintStream>(true);
    }
    else if((env_desc.flags & kEnvironmentFlagLogDebug) != 0)
    {
        print_stream_ = std::make_unique<DebugPrintStream>();
    }
    else if((env_desc.flags & kEnvironmentFlagLogFile) != 0)
    {
        auto file_print_stream = std::make_unique<FilePrintStream>();
        JD3D12_RETURN_IF_FAILED(file_print_stream->Init(env_desc.log_file_path));
        print_stream_ = std::move(file_print_stream);
    }

    return kSuccess;
}

void Logger::Log(LogSeverity severity, const wchar_t* message)
{
    if((severity & severity_mask_) == 0)
        return;
    if(!print_stream_)
        return;

    const wchar_t* const severity_str = GetLogSeverityString(severity);

    std::lock_guard<std::mutex> lock(print_stream_mutex_);
    print_stream_->PrintF(L"[%s] %s\n", severity_str, message);

    if(severity >= kLogSeverityWarning)
        print_stream_->Flush();
}

void Logger::VLogF(LogSeverity severity, const wchar_t* format, va_list arg_list)
{
    if((severity & severity_mask_) == 0)
        return;
    if(!print_stream_)
        return;

    Log(severity, SVPrintF(format, arg_list).c_str());
}

void Logger::LogF(LogSeverity severity, const wchar_t* format, ...)
{
    va_list arg_list;
    va_start(arg_list, format);
    VLogF(severity, format, arg_list);
    va_end(arg_list);
}

} // namespace jd3d12
