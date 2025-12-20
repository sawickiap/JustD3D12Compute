add_executable(jd3d12_example)

target_sources(jd3d12_example PRIVATE
    main.cpp
)

target_link_libraries(jd3d12_example PRIVATE
    jd3d12::jd3d12
)

target_compile_features(jd3d12_example PRIVATE cxx_std_17)

if (MSVC)
   target_compile_options(jd3d12_example PRIVATE /W4 /wd4189 /wd4702 /wd4100)
else()
   target_compile_options(jd3d12_example PRIVATE -Wall -Wextra -Wpedantic)
endif()
