# Copyright (c) 2025 Adam Sawicki
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, subject to the terms of the MIT License.
#
# See the LICENSE file in the project root for full license text.

cmake_minimum_required(VERSION 3.26)

project(JustD3D12Compute
   VERSION 0.1.0
   DESCRIPTION "C++ library offering a high-level, convenient, safe API for GPU-accelerated computations using compute shaders in Direct3D 12, written in HLSL language."
   LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(JD3D12_BUILD_EXAMPLES "Build example applications" OFF)
option(JD3D12_BUILD_TESTS "Build unit tests" OFF)

set(JD3D12_DX12_AGILITY_SDK_PATH "" CACHE STRING
   "Path to the root directory of the DirectX 12 Agility SDK (e.g. \"C:/Libraries/microsoft.direct3d.d3d12.1.618.3\")"
)
if (JD3D12_DX12_AGILITY_SDK_PATH STREQUAL "")
   message(FATAL_ERROR
      "JD3D12_DX12_AGILITY_SDK_PATH is not set."
      " Please set it to the root directory of the DirectX 12 Agility SDK, e.g.:"
      " -DJD3D12_DX12_AGILITY_SDK_PATH=C:/Libraries/microsoft.direct3d.d3d12.1.618.3"
   )
endif()
set(JD3D12_DX12_AGILITY_SDK_INCLUDE_DIR "${JD3D12_DX12_AGILITY_SDK_PATH}/build/native/include" )
if (NOT EXISTS "${JD3D12_DX12_AGILITY_SDK_INCLUDE_DIR}/d3d12.h")
   message(FATAL_ERROR
      "Invalid JD3D12_DX12_AGILITY_SDK_PATH: \"${JD3D12_DX12_AGILITY_SDK_PATH}\"."
      " Expected file not found: \"build/native/include/d3d12.h\"."
   )
endif()
message(STATUS "Using DirectX 12 Agility SDK from \"${JD3D12_DX12_AGILITY_SDK_PATH}\".")

set(JD3D12_DXC_PATH "" CACHE STRING
   "Path to the root directory of the DirectX Shader Compiler (DXC) (e.g. \"C:/Libraries/dxc_2025_07_14\")"
)
if (JD3D12_DXC_PATH STREQUAL "")
   message(FATAL_ERROR
      "JD3D12_DXC_PATH is not set."
      " Please set it to the root directory of the DirectX Shader Compiler (DXC), e.g.:"
      " -DJD3D12_DXC_PATH=C:/Libraries/dxc_2025_07_14"
   )
endif()
set(JD3D12_DXC_INCLUDE_DIR "${JD3D12_DXC_PATH}/inc" )
if (NOT EXISTS "${JD3D12_DXC_INCLUDE_DIR}/dxcapi.h")
   message(FATAL_ERROR
      "Invalid JD3D12_DXC_PATH: \"${JD3D12_DXC_PATH}\"."
      " Expected file not found: \"inc/dxcapi.h\"."
   )
endif()
message(STATUS "Using DirectX Shader Compiler from \"${JD3D12_DXC_PATH}\".")

# ------------------------------------------------------------------------------
# Dependencies

if (JD3D12_BUILD_TESTS)
   include(FetchContent)
   message(STATUS "Fetching Catch2 library...")
   FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.11.0
   )
   FetchContent_MakeAvailable(Catch2)
endif()

# ------------------------------------------------------------------------------
# Library target

add_library(jd3d12)
add_library(jd3d12::jd3d12 ALIAS jd3d12)
target_sources(jd3d12 PUBLIC FILE_SET HEADERS BASE_DIRS include FILES
   "include/jd3d12/jd3d12.hpp"
   "include/jd3d12/types.hpp"
   "include/jd3d12/core.hpp"
   "include/jd3d12/utils.hpp"
   "include/jd3d12/config.hpp"
)
target_sources(jd3d12 PRIVATE
   "src/precompiled_header.cpp"
   "src/jd3d12.cpp"
   "src/types.cpp"
   "src/utils.cpp"
   "src/core.cpp"
   "src/internal_utils.cpp"
)
target_sources(jd3d12 PRIVATE FILE_SET internal_headers TYPE HEADERS BASE_DIRS src FILES
   "src/precompiled_header.hpp"
   "src/internal_utils.hpp"
)
target_link_libraries(jd3d12 PRIVATE "d3d12" "dxgi" "dxguid")
target_include_directories(jd3d12 PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
   $<INSTALL_INTERFACE:include>
)
target_include_directories(jd3d12 PRIVATE
   "${JD3D12_DX12_AGILITY_SDK_INCLUDE_DIR}"
   "${JD3D12_DXC_INCLUDE_DIR}"
)
target_compile_features(jd3d12 PUBLIC cxx_std_17)
target_compile_definitions(jd3d12 PRIVATE UNICODE _UNICODE)
target_precompile_headers(jd3d12 PRIVATE src/precompiled_header.hpp)
if (MSVC)
   target_compile_options(jd3d12 PRIVATE /W4 /wd4189 /wd4702 /wd4100 /permissive- /Zc:__cplusplus)
else()
   target_compile_options(jd3d12 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ------------------------------------------------------------------------------
# Other projects

if (JD3D12_BUILD_EXAMPLES)
   add_subdirectory(examples)
endif()

if (JD3D12_BUILD_TESTS)
   enable_testing()
   add_subdirectory(tests)
endif()
