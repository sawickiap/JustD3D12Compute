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
   target_compile_options(jd3d12_tests PRIVATE /W4 /wd4189 /wd4702 /wd4100)
   set_target_properties(jd3d12_tests PROPERTIES
      VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:jd3d12_tests>"
   )
else()
   target_compile_options(jd3d12_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

file(GLOB TEST_DXIL_SHADERS CONFIGURE_DEPENDS
   "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.dxil"
)
add_custom_command(TARGET jd3d12_tests POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy_if_different
           ${TEST_DXIL_SHADERS}
           $<TARGET_FILE_DIR:jd3d12_tests>
   COMMENT "Copying test DXIL shaders to test runtime directory."
)
add_custom_command(TARGET jd3d12_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:jd3d12_tests>/D3D12
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${JD3D12_DX12_AGILITY_SDK_PATH}/build/native/bin/x64/D3D12Core.dll"
        "${JD3D12_DX12_AGILITY_SDK_PATH}/build/native/bin/x64/d3d12SDKLayers.dll"
        $<TARGET_FILE_DIR:jd3d12_tests>/D3D12
    COMMENT "Copying Direct3D 12 Agility SDK runtime DLLs."
)
include(CTest)
include(Catch)
catch_discover_tests(jd3d12_tests)
