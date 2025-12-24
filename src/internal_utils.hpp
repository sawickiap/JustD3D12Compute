// Copyright (c) 2025 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#pragma once

#define JD3D12_RETURN_IF_FAILED(expr) \
    do { \
        if(jd3d12::Result res__ = (expr); jd3d12::Failed(res__)) \
            return res__; \
    } while(false)

namespace jd3d12
{

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
    size_t GetCount() const noexcept { return count_; }
    const T* GetData() const noexcept { return count_ <= stack_item_max_count ? stack_items_ : heap_items_.data(); }
    T* GetData() noexcept { return count_ <= stack_item_max_count ? stack_items_ : heap_items_.data(); }

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

private:
    size_t count_ = 0;
    T stack_items_[stack_item_max_count];
    std::vector<T> heap_items_;
};

} // namespace jd3d12
