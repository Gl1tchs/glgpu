find_package(SDL2 REQUIRED)

macro(add_example example_name source_file)
    add_executable(${example_name} ${source_file})

    target_link_libraries(${example_name} 
        PRIVATE 
            glgpu::glgpu 
            SDL2::SDL2
    )

    target_include_directories(${example_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../misc
    )
endmacro()

# Add Examples
add_example(hello_glgpu "01-hello_glgpu.cpp")
add_example(compute_example "02-compute_example.cpp")

