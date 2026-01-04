// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#pragma once

#ifndef JD3D12_ASSERT
    #include <cassert>
    /** \brief Predefine this macro to replace the implementation of assert used internally by the library
    with your own.

    Define it as empty for maximum performance.
    It is used only for checking inconsistent state of the library internals, not developer's input.
    */
    #define JD3D12_ASSERT(expr) assert(expr)
#endif

#ifndef JD3D12_ASSERT_OR_RETURN
    /** \brief Predefine this macro to replace the implementation of a check used internally by the library
    to validate developer's input, like the parameters passed to the library functions.

    Define it as empty for maximum performance.

    Default implementation performs #JD3D12_ASSERT and, in case of failure, logs the message and calls
    `return kErrorInvalidArgument`.
    */
    #define JD3D12_ASSERT_OR_RETURN(expr, message) \
        do { \
            const bool ok__ = (expr); \
            JD3D12_ASSERT(ok__ && message); \
            if(!ok__) \
            { \
                JD3D12_LOG(kLogSeverityAssert, L"%s(%d): Assertion %s failed: %s", L"" __FILE__, __LINE__, L"" #expr, (const wchar_t*)(message)); \
                return kErrorInvalidArgument; \
            } \
        } while(false)
#endif
