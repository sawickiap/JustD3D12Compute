// Copyright (c) 2025 Adam Sawicki
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

} // namespace jd3d12
