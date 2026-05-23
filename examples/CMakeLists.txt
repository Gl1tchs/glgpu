find_package(SDL2 REQUIRED)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)

# ---------------------------------------------------------------------------
# Macros
# ---------------------------------------------------------------------------

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

macro(add_imgui_example example_name source_file)
    add_executable(${example_name} ${source_file})

    target_link_libraries(${example_name}
        PRIVATE
            glgpu::glgpu
            SDL2::SDL2
            imgui
    )

    target_include_directories(${example_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../misc
    )

    target_compile_definitions(${example_name}
        PRIVATE
            GLGPU_IMGUI_BACKEND_VULKAN
            GLGPU_IMGUI_BACKEND_SDL2
    )
endmacro()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
add_example(hello_glgpu "01-hello_glgpu.cpp")
add_example(compute_example "02-compute_example.cpp")
add_imgui_example(imgui_example "03-imgui_example.cpp")
add_example(hello_triangle "04-hello_triangle.cpp")
add_example(bindless_example "05-bindless_example.cpp")
