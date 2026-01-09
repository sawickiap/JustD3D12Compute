# Copyright (c) 2025-2026 Adam Sawicki
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, subject to the terms of the MIT License.
#
# See the LICENSE file in the project root for full license text.

add_executable(jd3d12_tests)

target_sources(jd3d12_tests PRIVATE
    test_main.cpp
)

target_link_libraries(jd3d12_tests PRIVATE
    jd3d12::jd3d12
    Catch2::Catch2
)

target_compile_features(jd3d12_tests PRIVATE cxx_std_17)
target_compile_definitions(jd3d12_tests PRIVATE UNICODE _UNICODE)

if (MSVC)
   target_compile_options(jd3d12_tests PRIVATE /W4 /wd4189 /wd4702 /wd4100 /permissive- /Zc:__cplusplus)
   set_target_properties(jd3d12_tests PROPERTIES
      VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:jd3d12_tests>"
   )
else()
   target_compile_options(jd3d12_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

file(GLOB TEST_SHADERS CONFIGURE_DEPENDS
   "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.hlsl"
)
add_custom_command(TARGET jd3d12_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:jd3d12_tests>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TEST_SHADERS}
        $<TARGET_FILE_DIR:jd3d12_tests>/shaders
    COMMENT "Copying test HLSL shaders to test runtime directory."
)
add_custom_command(TARGET jd3d12_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:jd3d12_tests>/D3D12
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${JD3D12_DX12_AGILITY_SDK_PATH}/build/native/bin/x64/D3D12Core.dll"
        "${JD3D12_DX12_AGILITY_SDK_PATH}/build/native/bin/x64/d3d12SDKLayers.dll"
        "${JD3D12_DXC_PATH}/bin/x64/dxcompiler.dll"
        "${JD3D12_DXC_PATH}/bin/x64/dxil.dll"
        $<TARGET_FILE_DIR:jd3d12_tests>/D3D12
    COMMENT "Copying Direct3D 12 Agility SDK runtime DLLs."
)
include(CTest)
include(Catch)
catch_discover_tests(jd3d12_tests)
