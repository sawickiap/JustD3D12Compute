// Copyright (c) 2025-2026 Adam Sawicki
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, subject to the terms of the MIT License.
//
// See the LICENSE file in the project root for full license text.

#pragma once

#include <jd3d12/config.hpp>
#include <cstdint>

namespace jd3d12
{

#if !defined(JD3D12_CPP20)
    #if __cplusplus >= 202002L || _MSVC_LANG >= 202002L // C++20
        #define JD3D12_CPP20 1
    #else
        #define JD3D12_CPP20 0
    #endif
#endif

#define JD3D12_NO_COPY_NO_MOVE_CLASS(class_name) \
    class_name(const class_name&) = delete; \
    class_name(const class_name&&) = delete; \
    class_name& operator=(const class_name&) = delete; \
    class_name& operator=(const class_name&&) = delete;

#define JD3D12_VEC2_STRUCT_MEMBERS(struct_name, component_type) \
    using ComponentType = component_type; \
    static constexpr uint32_t kComponentCount = 2; \
    ComponentType x, y; \
    ComponentType& operator[](size_t index) { return (&x)[index]; } \
    ComponentType operator[](size_t index) const noexcept { return (&x)[index]; } \
    bool operator==(const struct_name& rhs) const noexcept { return x == rhs.x && y == rhs.y; } \
    bool operator!=(const struct_name& rhs) const noexcept { return !(*this == rhs); }

#define JD3D12_VEC3_STRUCT_MEMBERS(struct_name, component_type) \
    using ComponentType = component_type; \
    static constexpr uint32_t kComponentCount = 3; \
    ComponentType x, y, z; \
    ComponentType& operator[](size_t index) { return (&x)[index]; } \
    ComponentType operator[](size_t index) const noexcept { return (&x)[index]; } \
    bool operator==(const struct_name& rhs) const noexcept { return x == rhs.x && y == rhs.y && z == rhs.z; } \
    bool operator!=(const struct_name& rhs) const noexcept { return !(*this == rhs); }

#define JD3D12_VEC4_STRUCT_MEMBERS(struct_name, component_type) \
    using ComponentType = component_type; \
    static constexpr uint32_t kComponentCount = 4; \
    ComponentType x, y, z, w; \
    ComponentType& operator[](size_t index) { return (&x)[index]; } \
    ComponentType operator[](size_t index) const noexcept { return (&x)[index]; } \
    bool operator==(const struct_name& rhs) const noexcept { return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w; } \
    bool operator!=(const struct_name& rhs) const noexcept { return !(*this == rhs); }

constexpr size_t kKilobyte = 1024ull;
constexpr size_t kMegabyte = 1024ull * 1024;
constexpr size_t kGigabyte = 1024ull * 1024 * 1024;
constexpr size_t kTerabyte = 1024ull * 1024 * 1024 * 1024;

struct IntVec2
{
    JD3D12_VEC2_STRUCT_MEMBERS(IntVec2, int32_t)
};
struct IntVec3
{
    JD3D12_VEC3_STRUCT_MEMBERS(IntVec3, int32_t)
};
struct IntVec4
{
    JD3D12_VEC4_STRUCT_MEMBERS(IntVec4, int32_t)
};
struct UintVec2
{
    JD3D12_VEC2_STRUCT_MEMBERS(UintVec2, uint32_t)
};
struct UintVec3
{
    JD3D12_VEC3_STRUCT_MEMBERS(UintVec3, uint32_t)
};
struct UintVec4
{
    JD3D12_VEC4_STRUCT_MEMBERS(UintVec4, uint32_t)
};
struct FloatVec2
{
    JD3D12_VEC2_STRUCT_MEMBERS(FloatVec2, float)
};
struct FloatVec3
{
    JD3D12_VEC3_STRUCT_MEMBERS(FloatVec3, float)
};
struct FloatVec4
{
    JD3D12_VEC4_STRUCT_MEMBERS(FloatVec4, float)
};
struct Int64Vec2
{
    JD3D12_VEC2_STRUCT_MEMBERS(Int64Vec2, int64_t)
};
struct Int64Vec3
{
    JD3D12_VEC3_STRUCT_MEMBERS(Int64Vec3, int64_t)
};
struct Int64Vec4
{
    JD3D12_VEC4_STRUCT_MEMBERS(Int64Vec4, int64_t)
};
struct Uint64Vec2
{
    JD3D12_VEC2_STRUCT_MEMBERS(Uint64Vec2, uint64_t)
};
struct Uint64Vec3
{
    JD3D12_VEC3_STRUCT_MEMBERS(Uint64Vec3, uint64_t)
};
struct Uint64Vec4
{
    JD3D12_VEC4_STRUCT_MEMBERS(Uint64Vec4, uint64_t)
};
struct DoubleVec2
{
    JD3D12_VEC2_STRUCT_MEMBERS(DoubleVec2, double)
};
struct DoubleVec3
{
    JD3D12_VEC3_STRUCT_MEMBERS(DoubleVec3, double)
};
struct DoubleVec4
{
    JD3D12_VEC4_STRUCT_MEMBERS(DoubleVec4, double)
};

#undef JD3D12_VEC4_STRUCT_MEMBERS
#undef JD3D12_VEC3_STRUCT_MEMBERS
#undef JD3D12_VEC2_STRUCT_MEMBERS

inline int32_t asint(uint32_t a) { return reinterpret_cast<const int32_t&>(a); }
inline int32_t asint(float a) { return reinterpret_cast<const int32_t&>(a); }
inline uint32_t asuint(int32_t a) { return reinterpret_cast<const uint32_t&>(a); }
inline uint32_t asuint(float a) { return reinterpret_cast<const uint32_t&>(a); }
inline float asfloat(int32_t a) { return reinterpret_cast<const float&>(a); }
inline float asfloat(uint32_t a) { return reinterpret_cast<const float&>(a); }

inline double asdouble(uint32_t low_bits, uint32_t high_bits)
{
    const uint64_t u64 = (uint64_t(high_bits) << 32) | uint64_t(low_bits);
    return reinterpret_cast<const double&>(u64);
}
inline double asdouble(int64_t a) { return reinterpret_cast<const double&>(a); }
inline double asdouble(uint64_t a) { return reinterpret_cast<const double&>(a); }

inline void asuint(double a, uint32_t& out_low_bits, uint32_t& out_high_bits)
{
    const uint64_t u64 = reinterpret_cast<const uint64_t&>(a);
    out_low_bits = uint32_t(u64);
    out_high_bits = uint32_t(u64 >> 32);
}

#define JD3D12_FUNCTIONS_FOR_ALL_SCALAR_TYPES(type) \
    inline type clamp(type a, type min_val, type max_val) \
    { \
        if(a < min_val) return min_val; \
        if(a > max_val) return max_val; \
        return a; \
    }

#define JD3D12_FUNCTIONS_FOR_INTEGRAL_TYPES(type) \
    inline type DivideRoundingUp(type a, type b) \
    { \
        return (a + b - type(1)) / b; \
    }

#define JD3D12_FUNCTIONS_FOR_FLOAT_TYPES(type) \
    inline type saturate(type a) \
    { \
        return clamp(a, type(0), type(1)); \
    } \
    inline type lerp(type a, type b, type t) \
    { \
        return a + t * (b - a); \
    }

JD3D12_FUNCTIONS_FOR_ALL_SCALAR_TYPES(int32_t)
JD3D12_FUNCTIONS_FOR_ALL_SCALAR_TYPES(uint32_t)
JD3D12_FUNCTIONS_FOR_ALL_SCALAR_TYPES(float)
JD3D12_FUNCTIONS_FOR_ALL_SCALAR_TYPES(int64_t)
JD3D12_FUNCTIONS_FOR_ALL_SCALAR_TYPES(uint64_t)
JD3D12_FUNCTIONS_FOR_ALL_SCALAR_TYPES(double)

JD3D12_FUNCTIONS_FOR_INTEGRAL_TYPES(uint32_t)
JD3D12_FUNCTIONS_FOR_INTEGRAL_TYPES(int32_t)
JD3D12_FUNCTIONS_FOR_INTEGRAL_TYPES(uint64_t)
JD3D12_FUNCTIONS_FOR_INTEGRAL_TYPES(int64_t)

JD3D12_FUNCTIONS_FOR_FLOAT_TYPES(float)
JD3D12_FUNCTIONS_FOR_FLOAT_TYPES(double)

#define JD3D12_FUNCTIONS_FOR_ALL_VEC2_TYPES(vec_type) \
    inline vec_type clamp(const vec_type& a, const vec_type& min_val, const vec_type& max_val) \
    { \
        return { \
            clamp(a.x, min_val.x, max_val.x), \
            clamp(a.y, min_val.y, max_val.y) }; \
    }

#define JD3D12_FUNCTIONS_FOR_ALL_VEC3_TYPES(vec_type) \
    inline vec_type clamp(const vec_type& a, const vec_type& min_val, const vec_type& max_val) \
    { \
        return { \
            clamp(a.x, min_val.x, max_val.x), \
            clamp(a.y, min_val.y, max_val.y), \
            clamp(a.z, min_val.z, max_val.z) }; \
    }

#define JD3D12_FUNCTIONS_FOR_ALL_VEC4_TYPES(vec_type) \
    inline vec_type clamp(const vec_type& a, const vec_type& min_val, const vec_type& max_val) \
    { \
        return { \
            clamp(a.x, min_val.x, max_val.x), \
            clamp(a.y, min_val.y, max_val.y), \
            clamp(a.z, min_val.z, max_val.z), \
            clamp(a.w, min_val.w, max_val.w) }; \
    }

#define JD3D12_FUNCTIONS_FOR_ALL_VEC2_WITH_SCALAR_TYPES(vec_type, scalar_type) \
    inline vec_type clamp(const vec_type& a, const scalar_type& min_val, const scalar_type& max_val) \
    { \
        return { \
            clamp(a.x, min_val, max_val), \
            clamp(a.y, min_val, max_val) }; \
    }

#define JD3D12_FUNCTIONS_FOR_ALL_VEC3_WITH_SCALAR_TYPES(vec_type, scalar_type) \
    inline vec_type clamp(const vec_type& a, const scalar_type& min_val, const scalar_type& max_val) \
    { \
        return { \
            clamp(a.x, min_val, max_val), \
            clamp(a.y, min_val, max_val), \
            clamp(a.z, min_val, max_val) }; \
    }

#define JD3D12_FUNCTIONS_FOR_ALL_VEC4_WITH_SCALAR_TYPES(vec_type, scalar_type) \
    inline vec_type clamp(const vec_type& a, const scalar_type& min_val, const scalar_type& max_val) \
    { \
        return { \
            clamp(a.x, min_val, max_val), \
            clamp(a.y, min_val, max_val), \
            clamp(a.z, min_val, max_val), \
            clamp(a.w, min_val, max_val) }; \
    }

#define JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC2_TYPES(vec_type) \
    inline vec_type DivideRoundingUp(const vec_type& a, const vec_type& b) \
    { \
        return { \
            DivideRoundingUp(a.x, b.x), \
            DivideRoundingUp(a.y, b.y) }; \
    }

#define JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC3_TYPES(vec_type) \
    inline vec_type DivideRoundingUp(const vec_type& a, const vec_type& b) \
    { \
        return { \
            DivideRoundingUp(a.x, b.x), \
            DivideRoundingUp(a.y, b.y), \
            DivideRoundingUp(a.z, b.z) }; \
    }

#define JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC4_TYPES(vec_type) \
    inline vec_type DivideRoundingUp(const vec_type& a, const vec_type& b) \
    { \
        return { \
            DivideRoundingUp(a.x, b.x), \
            DivideRoundingUp(a.y, b.y), \
            DivideRoundingUp(a.z, b.z), \
            DivideRoundingUp(a.w, b.w) }; \
    }

#define JD3D12_FUNCTIONS_FOR_FLOAT_VEC2_TYPES(vec_type) \
    inline vec_type saturate(const vec_type& a) \
    { \
        return { \
            saturate(a.x), \
            saturate(a.y) }; \
    } \
    inline vec_type lerp(const vec_type& a, const vec_type& b, const vec_type& t) \
    { \
        return { \
            lerp(a.x, b.x, t.x), \
            lerp(a.y, b.y, t.y) }; \
    }

#define JD3D12_FUNCTIONS_FOR_FLOAT_VEC3_TYPES(vec_type) \
    inline vec_type saturate(const vec_type& a) \
    { \
        return { \
            saturate(a.x), \
            saturate(a.y), \
            saturate(a.z) }; \
    } \
    inline vec_type lerp(const vec_type& a, const vec_type& b, const vec_type& t) \
    { \
        return { \
            lerp(a.x, b.x, t.x), \
            lerp(a.y, b.y, t.y), \
            lerp(a.z, b.z, t.z) }; \
    }

#define JD3D12_FUNCTIONS_FOR_FLOAT_VEC4_TYPES(vec_type) \
    inline vec_type saturate(const vec_type& a) \
    { \
        return { \
            saturate(a.x), \
            saturate(a.y), \
            saturate(a.z), \
            saturate(a.w) }; \
    } \
    inline vec_type lerp(const vec_type& a, const vec_type& b, const vec_type& t) \
    { \
        return { \
            lerp(a.x, b.x, t.x), \
            lerp(a.y, b.y, t.y), \
            lerp(a.z, b.z, t.z), \
            lerp(a.w, b.w, t.w) }; \
    }

#define JD3D12_FUNCTIONS_FOR_FLOAT_VEC2_WITH_SCALAR_TYPES(vec_type, scalar_type) \
    inline vec_type lerp(const vec_type& a, const vec_type& b, scalar_type t) \
    { \
        return { \
            lerp(a.x, b.x, t), \
            lerp(a.y, b.y, t) }; \
    }

#define JD3D12_FUNCTIONS_FOR_FLOAT_VEC3_WITH_SCALAR_TYPES(vec_type, scalar_type) \
    inline vec_type lerp(const vec_type& a, const vec_type& b, scalar_type t) \
    { \
        return { \
            lerp(a.x, b.x, t), \
            lerp(a.y, b.y, t), \
            lerp(a.z, b.z, t) }; \
    }

#define JD3D12_FUNCTIONS_FOR_FLOAT_VEC4_WITH_SCALAR_TYPES(vec_type, scalar_type) \
    inline vec_type lerp(const vec_type& a, const vec_type& b, scalar_type t) \
    { \
        return { \
            lerp(a.x, b.x, t), \
            lerp(a.y, b.y, t), \
            lerp(a.z, b.z, t), \
            lerp(a.w, b.w, t) }; \
    }

JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC2_TYPES(UintVec2)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC2_TYPES(IntVec2)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC2_TYPES(Uint64Vec2)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC2_TYPES(Int64Vec2)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC3_TYPES(UintVec3)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC3_TYPES(IntVec3)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC3_TYPES(Uint64Vec3)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC3_TYPES(Int64Vec3)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC4_TYPES(UintVec4)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC4_TYPES(IntVec4)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC4_TYPES(Uint64Vec4)
JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC4_TYPES(Int64Vec4)

JD3D12_FUNCTIONS_FOR_ALL_VEC2_TYPES(UintVec2)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_TYPES(IntVec2)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_TYPES(FloatVec2)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_TYPES(Uint64Vec2)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_TYPES(Int64Vec2)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_TYPES(DoubleVec2)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_TYPES(UintVec3)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_TYPES(IntVec3)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_TYPES(FloatVec3)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_TYPES(Uint64Vec3)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_TYPES(Int64Vec3)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_TYPES(DoubleVec3)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_TYPES(UintVec4)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_TYPES(IntVec4)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_TYPES(FloatVec4)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_TYPES(Uint64Vec4)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_TYPES(Int64Vec4)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_TYPES(DoubleVec4)

JD3D12_FUNCTIONS_FOR_ALL_VEC2_WITH_SCALAR_TYPES(UintVec2, uint32_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_WITH_SCALAR_TYPES(IntVec2, int32_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_WITH_SCALAR_TYPES(FloatVec2, float)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_WITH_SCALAR_TYPES(Uint64Vec2, uint64_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_WITH_SCALAR_TYPES(Int64Vec2, int64_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC2_WITH_SCALAR_TYPES(DoubleVec2, double)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_WITH_SCALAR_TYPES(UintVec3, uint32_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_WITH_SCALAR_TYPES(IntVec3, int32_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_WITH_SCALAR_TYPES(FloatVec3, float)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_WITH_SCALAR_TYPES(Uint64Vec3, uint64_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_WITH_SCALAR_TYPES(Int64Vec3, int64_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC3_WITH_SCALAR_TYPES(DoubleVec3, double)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_WITH_SCALAR_TYPES(UintVec4, uint32_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_WITH_SCALAR_TYPES(IntVec4, int32_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_WITH_SCALAR_TYPES(FloatVec4, float)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_WITH_SCALAR_TYPES(Uint64Vec4, uint64_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_WITH_SCALAR_TYPES(Int64Vec4, int64_t)
JD3D12_FUNCTIONS_FOR_ALL_VEC4_WITH_SCALAR_TYPES(DoubleVec4, double)

JD3D12_FUNCTIONS_FOR_FLOAT_VEC2_TYPES(FloatVec2)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC3_TYPES(FloatVec3)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC4_TYPES(FloatVec4)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC2_TYPES(DoubleVec2)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC3_TYPES(DoubleVec3)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC4_TYPES(DoubleVec4)

JD3D12_FUNCTIONS_FOR_FLOAT_VEC2_WITH_SCALAR_TYPES(FloatVec2, float)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC2_WITH_SCALAR_TYPES(DoubleVec2, double)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC3_WITH_SCALAR_TYPES(FloatVec3, float)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC3_WITH_SCALAR_TYPES(DoubleVec3, double)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC4_WITH_SCALAR_TYPES(FloatVec4, float)
JD3D12_FUNCTIONS_FOR_FLOAT_VEC4_WITH_SCALAR_TYPES(DoubleVec4, double)

#undef JD3D12_FUNCTIONS_FOR_FLOAT_VEC4_TYPES
#undef JD3D12_FUNCTIONS_FOR_FLOAT_VEC3_TYPES
#undef JD3D12_FUNCTIONS_FOR_FLOAT_VEC2_TYPES
#undef JD3D12_FUNCTIONS_FOR_ALL_VEC4_WITH_SCALAR_TYPES
#undef JD3D12_FUNCTIONS_FOR_ALL_VEC3_WITH_SCALAR_TYPES
#undef JD3D12_FUNCTIONS_FOR_ALL_VEC2_WITH_SCALAR_TYPES
#undef JD3D12_FUNCTIONS_FOR_ALL_VEC4_TYPES
#undef JD3D12_FUNCTIONS_FOR_ALL_VEC3_TYPES
#undef JD3D12_FUNCTIONS_FOR_ALL_VEC2_TYPES
#undef JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC4_TYPES
#undef JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC3_TYPES
#undef JD3D12_FUNCTIONS_FOR_INTEGRAL_VEC2_TYPES
#undef JD3D12_FUNCTIONS_FOR_ALL_SCALAR_TYPES
#undef JD3D12_FUNCTIONS_FOR_INTEGRAL_TYPES

inline IntVec2 asint(const UintVec2& a) { return reinterpret_cast<const IntVec2&>(a); }
inline IntVec2 asint(const FloatVec2& a) { return reinterpret_cast<const IntVec2&>(a); }
inline UintVec2 asuint(const IntVec2& a) { return reinterpret_cast<const UintVec2&>(a); }
inline UintVec2 asuint(const FloatVec2& a) { return reinterpret_cast<const UintVec2&>(a); }
inline FloatVec2 asfloat(const IntVec2& a) { return reinterpret_cast<const FloatVec2&>(a); }
inline FloatVec2 asfloat(const UintVec2& a) { return reinterpret_cast<const FloatVec2&>(a); }

inline IntVec3 asint(const UintVec3& a) { return reinterpret_cast<const IntVec3&>(a); }
inline IntVec3 asint(const FloatVec3& a) { return reinterpret_cast<const IntVec3&>(a); }
inline UintVec3 asuint(const IntVec3& a) { return reinterpret_cast<const UintVec3&>(a); }
inline UintVec3 asuint(const FloatVec3& a) { return reinterpret_cast<const UintVec3&>(a); }
inline FloatVec3 asfloat(const IntVec3& a) { return reinterpret_cast<const FloatVec3&>(a); }
inline FloatVec3 asfloat(const UintVec3& a) { return reinterpret_cast<const FloatVec3&>(a); }

inline IntVec4 asint(const UintVec4& a) { return reinterpret_cast<const IntVec4&>(a); }
inline IntVec4 asint(const FloatVec4& a) { return reinterpret_cast<const IntVec4&>(a); }
inline UintVec4 asuint(const IntVec4& a) { return reinterpret_cast<const UintVec4&>(a); }
inline UintVec4 asuint(const FloatVec4& a) { return reinterpret_cast<const UintVec4&>(a); }
inline FloatVec4 asfloat(const IntVec4& a) { return reinterpret_cast<const FloatVec4&>(a); }
inline FloatVec4 asfloat(const UintVec4& a) { return reinterpret_cast<const FloatVec4&>(a); }

inline DoubleVec2 asdouble(
    uint32_t x_low_bits, uint32_t x_high_bits,
    uint32_t y_low_bits, uint32_t y_high_bits)
{
    const uint64_t u64_x = (uint64_t(x_high_bits) << 32) | uint64_t(x_low_bits);
    const uint64_t u64_y = (uint64_t(y_high_bits) << 32) | uint64_t(y_low_bits);
    return { reinterpret_cast<const double&>(u64_x), reinterpret_cast<const double&>(u64_y) };
}

inline DoubleVec2 asdouble(const Int64Vec2& v)
{
    return { asdouble(v.x), asdouble(v.y) };
}
inline DoubleVec3 asdouble(const Int64Vec3& v)
{
    return { asdouble(v.x), asdouble(v.y), asdouble(v.z) };
}
inline DoubleVec4 asdouble(const Int64Vec4& v)
{
    return { asdouble(v.x), asdouble(v.y), asdouble(v.z), asdouble(v.w) };
}
inline DoubleVec2 asdouble(const Uint64Vec2& v)
{
    return { asdouble(v.x), asdouble(v.y) };
}
inline DoubleVec3 asdouble(const Uint64Vec3& v)
{
    return { asdouble(v.x), asdouble(v.y), asdouble(v.z) };
}
inline DoubleVec4 asdouble(const Uint64Vec4& v)
{
    return { asdouble(v.x), asdouble(v.y), asdouble(v.z), asdouble(v.w) };
}

/// Returns the number of bits set to 1 in the input number.
uint32_t CountBitsSet(uint32_t a);

} // namespace jd3d12
