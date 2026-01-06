// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#pragma once

#include <jd3d12/utils.hpp>

#define JD3D12_RETURN_IF_FAILED(expr) \
    do { \
        if(jd3d12::Result res__ = (expr); jd3d12::Failed(res__)) \
            return res__; \
    } while(false)

#define JD3D12_LOG_AND_RETURN_IF_FAILED(expr) \
    do { \
        if(jd3d12::Result res__ = (expr); jd3d12::Failed(res__)) \
        { \
            JD3D12_LOG(jd3d12::kLogSeverityError, L"%s(%d) %s: %s failed with 0x%08X (%s)", L"" __FILE__, __LINE__, L"" __FUNCTION__, L"" #expr, res__, GetResultString(res__)); \
            return res__; \
        } \
    } while(false)

namespace jd3d12
{

std::wstring SVPrintF(const wchar_t* format, va_list arg_list);
std::wstring SPrintF(const wchar_t* format, ...);

LogSeverity D3d12MessageSeverityToLogSeverity(D3D12_MESSAGE_SEVERITY severity);
const wchar_t* GetD3d12MessageCategoryString(D3D12_MESSAGE_CATEGORY category);

struct FCloseDeleter
{
    void operator()(FILE* p) const noexcept
    {
        if(p != nullptr)
            fclose(p);
    }
};

struct CloseHandleDeleter
{
    typedef HANDLE pointer;
    void operator()(HANDLE handle) const noexcept
    {
        if (handle != NULL)
            CloseHandle(handle);
    }
};

// Internal.
template<typename T, size_t stack_item_max_count>
class StackOrHeapVector
{
public:
    StackOrHeapVector() = default;
    explicit StackOrHeapVector(size_t initial_count) : count_(initial_count)
    {
        if (initial_count > stack_item_max_count)
            heap_items_.resize(initial_count);
    }
    bool IsEmpty() const noexcept { return count_ == 0; }
    size_t GetCount() const noexcept { return count_; }
    const T* GetData() const noexcept { return count_ <= stack_item_max_count ? stack_items_ : heap_items_.data(); }
    T* GetData() noexcept { return count_ <= stack_item_max_count ? stack_items_ : heap_items_.data(); }
    const T& operator[](size_t index) const noexcept { return GetData()[index]; }
    T& operator[](size_t index) noexcept { return GetData()[index]; }

    void Clear()
    {
        count_ = 0;
        heap_items_.clear();
    }

    void Emplace(T&& val)
    {
        // Adding still to stack_items_.
        if(count_ < stack_item_max_count)
            stack_items_[count_] = std::move(val);
        // Moving from stack_items_ to heap_items_.
        else if(count_ == stack_item_max_count)
        {
            heap_items_.reserve(count_ * 2);
            for(size_t i = 0; i < count_; ++i)
                heap_items_.emplace_back(std::move(stack_items_[i]));
            heap_items_.emplace_back(std::move(val));
        }
        // Adding already to heap_items_.
        else
            heap_items_.emplace_back(std::move(val));
        ++count_;
    }
    template<typename U>
    void PushBack(U&& val)
    {
        // Adding still to stack_items_.
        if(count_ < stack_item_max_count)
            stack_items_[count_] = std::forward<U>(val);
        // Moving from stack_items_ to heap_items_.
        else if(count_ == stack_item_max_count)
        {
            heap_items_.reserve(count_ * 2);
            for(size_t i = 0; i < count_; ++i)
                heap_items_.emplace_back(std::move(stack_items_[i]));
            heap_items_.emplace_back(std::forward<U>(val));
        }
        // Adding already to heap_items_.
        else
            heap_items_.emplace_back(std::forward<U>(val));
        ++count_;
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept { return GetData(); }
    iterator end() noexcept { return GetData() + count_; }
    const_iterator begin() const noexcept { return GetData(); }
    const_iterator end() const noexcept { return GetData() + count_; }
    const_iterator cbegin() const noexcept { return GetData(); }
    const_iterator cend() const noexcept { return GetData() + count_; }

private:
    size_t count_ = 0;
    T stack_items_[stack_item_max_count];
    std::vector<T> heap_items_;
};

} // namespace jd3d12
