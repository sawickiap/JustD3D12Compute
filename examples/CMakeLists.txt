# Copyright (c) 2025 Adam Sawicki
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, subject to the terms of the MIT License.
#
# See the LICENSE file in the project root for full license text.

add_executable(jd3d12_example)

target_sources(jd3d12_example PRIVATE
    main.cpp
)

target_link_libraries(jd3d12_example PRIVATE
    jd3d12::jd3d12
)

target_compile_features(jd3d12_example PRIVATE cxx_std_17)
target_compile_definitions(jd3d12_example PRIVATE UNICODE _UNICODE)

add_custom_command(TARGET jd3d12_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:jd3d12_example>/D3D12
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${JD3D12_DX12_AGILITY_SDK_PATH}/build/native/bin/x64/D3D12Core.dll"
        "${JD3D12_DX12_AGILITY_SDK_PATH}/build/native/bin/x64/d3d12SDKLayers.dll"
        $<TARGET_FILE_DIR:jd3d12_example>/D3D12
    COMMENT "Copying Direct3D 12 Agility SDK runtime DLLs."
)

if (MSVC)
   target_compile_options(jd3d12_example PRIVATE /W4 /wd4189 /wd4702 /wd4100 /permissive- /Zc:__cplusplus)
   set_target_properties(jd3d12_example PROPERTIES
      VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:jd3d12_example>"
   )
else()
   target_compile_options(jd3d12_example PRIVATE -Wall -Wextra -Wpedantic)
endif()
