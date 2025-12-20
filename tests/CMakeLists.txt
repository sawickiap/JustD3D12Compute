add_executable(jd3d12_tests)

target_sources(jd3d12_tests PRIVATE
    test_main.cpp
)

target_link_libraries(jd3d12_tests PRIVATE
    jd3d12::jd3d12
    Catch2::Catch2WithMain
)

target_compile_features(jd3d12_tests PRIVATE cxx_std_17)

if (MSVC)
   target_compile_options(jd3d12_tests PRIVATE /W4 /wd4189 /wd4702 /wd4100)
else()
   target_compile_options(jd3d12_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
include(Catch)
catch_discover_tests(jd3d12_tests)
