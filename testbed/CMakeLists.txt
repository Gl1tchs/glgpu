find_package(SDL2 REQUIRED)

add_executable(gltestbed testbed.cpp)

target_include_directories(gltestbed PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(gltestbed PUBLIC glgpu SDL2)


add_executable(compute_demo compute_demo.cpp)

target_include_directories(compute_demo PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(compute_demo PUBLIC glgpu SDL2)

