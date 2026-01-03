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

#include <jd3d12/core.hpp>
#include <jd3d12/utils.hpp>
#include <jd3d12/types.hpp>

#define JD3D12_LOG(severity, format, ...) \
    do { \
        Logger* const logger = GetLogger(); \
        if(logger != nullptr) \
            logger->Log(severity, format, __VA_ARGS__); \
    } while(false)

namespace jd3d12
{

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

protected:
    PrintStream() = default;

private:
    JD3D12_NO_COPY_NO_MOVE_CLASS(PrintStream);
};

// Prints to standard output.
class ConsolePrintStream : public PrintStream
{
public:
    ConsolePrintStream() = default;
    using PrintStream::Print;
    virtual void Print(const wchar_t* str, size_t str_len);
    virtual void Print(const wchar_t* str);
    virtual void VPrintF(const wchar_t* format, va_list arg_list);

private:
    JD3D12_NO_COPY_NO_MOVE_CLASS(ConsolePrintStream);
};

// Prints to file.
class FilePrintStream : public PrintStream
{
public:
    // Initializes object with empty state.
    FilePrintStream();
    // Opens file during initialization.
    FilePrintStream(const wchar_t* file_path, const wchar_t* mode);
    // Automatically closes file.
    ~FilePrintStream();

    // mode: Like in fopen, e.g. "wb", "a".
    bool Open(const wchar_t* file_path, const wchar_t* mode);
    void Close();
    bool IsOpened() const { return file_ != nullptr; }

    using PrintStream::Print;
    virtual void Print(const wchar_t* str, size_t str_len);
    virtual void Print(const wchar_t* str);
    virtual void VPrintF(const wchar_t* format, va_list arg_list);

private:
    FILE* file_;

    JD3D12_NO_COPY_NO_MOVE_CLASS(FilePrintStream);
};

// Prints to OutputDebugString.
class DebugPrintStream : public PrintStream
{
public:
    DebugPrintStream() = default;
    using PrintStream::Print;
    virtual void Print(const wchar_t* str);

private:
    JD3D12_NO_COPY_NO_MOVE_CLASS(DebugPrintStream);
};

class Logger
{
public:
    // Sets out_is_needed depending on whether logger is needed with given parameters.
    // Returns error if parameters are invalid.
    static Result IsNeeded(const EnvironmentDesc& env_desc, bool& out_is_needed);

    Logger() = default;
    ~Logger() = default;
    Result Init(const EnvironmentDesc& env_desc);

    void Log(LogSeverity severity, const wchar_t* format, ...);

private:
    uint32_t severity_mask_ = 0;
    std::unique_ptr<PrintStream> print_stream_;

    JD3D12_NO_COPY_NO_MOVE_CLASS(Logger);
};

} // namespace jd3d12
