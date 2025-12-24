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

    Default implementation performs #JD3D12_ASSERT and, in case of failure, logs the message (TODO) calls
    `return kErrorInvalidArgument`.
    */
    #define JD3D12_ASSERT_OR_RETURN(expr, message) \
        do { \
            const bool ok__ = (expr); \
            JD3D12_ASSERT(ok__ && message); \
            if(!ok__) return kErrorInvalidArgument; \
        } while(false)
#endif
