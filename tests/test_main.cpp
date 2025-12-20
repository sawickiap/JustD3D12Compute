#include <jd3d12/jd3d12.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace jd3d12;

TEST_CASE("DivideRoundingUp scalar", "[types]")
{
    const uint32_t count = 100;
    const uint32_t group_count = 8;
    CHECK(DivideRoundingUp(count, group_count) == 13);
}

TEST_CASE("DivideRoundingUp vector", "[types]")
{
    UintVec3 count_v = { 100, 50, 25 };
    UintVec3 group_count_v = { 8, 16, 4 };
    UintVec3 expected_result = { 13, 4, 7 };
    CHECK(DivideRoundingUp(count_v, group_count_v) == expected_result);
}

TEST_CASE("Clamp scalar", "[types]")
{
    float a = 1.5f, b = 10.5f, c = 1e7f;
    CHECK(clamp(b, a, c) == b);
    CHECK(clamp(a, b, c) == b);
    CHECK(clamp(c, a, b) == b);
}

TEST_CASE("Clamp vector with scalar min/max", "[types]")
{
    FloatVec2 v1 = { 1.0f, 5.0f };
    float min_val = 2.0f;
    float max_val = 4.0f;
    FloatVec2 expected_result = { 2.0f, 4.0f };
    CHECK(clamp(v1, min_val, max_val) == expected_result);
}

TEST_CASE("Clamp vector with vector min/max", "[types]")
{
    FloatVec2 v1 = { 1.0f, 5.0f };
    FloatVec2 min_val = { 2.0f, 2.0f };
    FloatVec2 max_val = { 4.0f, 4.0f };
    FloatVec2 expected_result = { 2.0f, 4.0f };
    CHECK(clamp(v1, min_val, max_val) == expected_result);
}

TEST_CASE("Saturate float and double", "[types]")
{
    {
        float a = -0.5f;
        float b = 0.5f;
        float c = 2.0f;
        CHECK(saturate(a) == 0.0f);
        CHECK(saturate(b) == 0.5f);
        CHECK(saturate(c) == 1.0f);
    }

    {
        double a = -0.5;
        double b = 0.5;
        double c = 2.0;
        CHECK(saturate(a) == 0.0);
        CHECK(saturate(b) == 0.5);
        CHECK(saturate(c) == 1.0);
    }
}

TEST_CASE("Saturate vector", "[types]")
{
    FloatVec3 v = { -1.0f, 0.5f, 2.0f };
    FloatVec3 expected = { 0.0f, 0.5f, 1.0f };
    CHECK(saturate(v) == expected);
}

TEST_CASE("Lerp scalar", "[types]")
{
    float a = 10.f, b = 20.f;
    CHECK(lerp(a, b, 0.f) == a);
    CHECK(lerp(a, b, 1.f) == b);
    CHECK(lerp(a, b, 0.5f) == 15.f);
    CHECK(lerp(a, b, 2.f) == 30.f);
}

TEST_CASE("Lerp vector DoubleVec2 with scalar and vector t", "[types]")
{
    {
        DoubleVec2 a = { 0., 10. }, b = { 10., 20. };
        DoubleVec2 expected = { 0., 10. };
        CHECK(lerp(a, b, 0.f) == expected);
        expected = { 10., 20. };
        CHECK(lerp(a, b, 1.f) == expected);
        expected = { 5., 15. };
        CHECK(lerp(a, b, 0.5f) == expected);
        expected = { 20., 30. };
        CHECK(lerp(a, b, 2.f) == expected);
    }

    {
        DoubleVec2 a = { 0., 10. }, b = { 10., 20. }, t = { 0.5, 1.0 };
        DoubleVec2 expected = { 5., 20. };
        CHECK(lerp(a, b, t) == expected);
    }
}

TEST_CASE("Bitcast conversions asuint/asfloat", "[types]")
{
    {
        float a = 1.5f;
        CHECK(asuint(a) == 0x3fc00000u);
    }

    {
        uint32_t u = 0x44268000u;
        CHECK(asfloat(u) == 666.f);
    }

    {
        FloatVec3 fv = { 0.f, 1.f, -10.f };
        UintVec3 expected = { 0x00000000u, 0x3f800000u, 0xc1200000u };
        CHECK(asuint(fv) == expected);
    }
}

TEST_CASE("Format description", "[utils][format]")
{
    const FormatDesc* desc = GetFormatDesc(Format::kR16G16_Snorm);
    REQUIRE(desc != nullptr);
    CHECK(desc->name == std::wstring(L"R16G16_Snorm"));
    CHECK(desc->component_format == Format::kR16_Snorm);
    CHECK(desc->bits_per_element == 32);
    CHECK(desc->component_count == 2);
    CHECK(desc->active_component_count == 2);
    CHECK(desc->is_simple == 1);
}
