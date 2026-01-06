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
#include "internal_utils.hpp"

// The place where you use this macro must have `GetLogger()` available.
#define JD3D12_LOG(severity, format, ...) \
    do { \
        Logger* const logger = GetLogger(); \
        if(logger != nullptr) \
            logger->LogF(severity, format, __VA_ARGS__); \
    } while(false)

namespace jd3d12
{

class PrintStream;

class Logger
{
public:
    Logger();
    ~Logger();
    Result Init(const EnvironmentDesc& env_desc);

    Logger* GetLogger() noexcept { return this; }

    void Log(LogSeverity severity, const wchar_t* message);
    void VLogF(LogSeverity severity, const wchar_t* format, va_list arg_list);
    void LogF(LogSeverity severity, const wchar_t* format, ...);

private:
    uint32_t severity_mask_ = 0;

    LogCallback callback_ = nullptr;
    void* callback_context_ = nullptr;

    std::mutex print_streams_mutex_;
    StackOrHeapVector<std::unique_ptr<PrintStream>, 4> print_streams_;

    JD3D12_NO_COPY_NO_MOVE_CLASS(Logger);
};

} // namespace jd3d12
