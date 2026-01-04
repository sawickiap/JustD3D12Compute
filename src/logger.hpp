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
    // Sets out_is_needed depending on whether logger is needed with given parameters.
    // Returns error if parameters are invalid.
    static Result IsNeeded(const EnvironmentDesc& env_desc, bool& out_is_needed);

    Logger();
    ~Logger();
    Result Init(const EnvironmentDesc& env_desc);

    void Log(LogSeverity severity, const wchar_t* message);
    void VLogF(LogSeverity severity, const wchar_t* format, va_list arg_list);
    void LogF(LogSeverity severity, const wchar_t* format, ...);

private:
    uint32_t severity_mask_ = 0;

    std::mutex print_stream_mutex_;
    std::unique_ptr<PrintStream> print_stream_;

    JD3D12_NO_COPY_NO_MOVE_CLASS(Logger);
};

} // namespace jd3d12
