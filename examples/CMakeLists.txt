find_package(SDL2 REQUIRED)

add_executable(hello_glgpu 01-hello_glgpu.cpp)
target_include_directories(hello_glgpu PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(hello_glgpu PUBLIC glgpu SDL2)

add_executable(compute_example 02-compute_example.cpp)
target_include_directories(compute_example PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(compute_example PUBLIC glgpu SDL2)
