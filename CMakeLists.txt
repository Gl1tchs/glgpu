cmake_minimum_required(VERSION 3.15...4.0)
project(glgpu)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
option(GL_BUILD_TESTBED "Build sanbbox application" ON)
option(GL_WITH_VULKAN "Enable vulkan backend" ON)
option(GL_ENABLE_POSITION_INDEPENDENT_CODE "Enable PIC" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (GL_ENABLE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(VK_BOOTSTRAP_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)
endif()

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

set(VENDOR_DIR ${CMAKE_CURRENT_LIST_DIR}/external)

add_subdirectory(${VENDOR_DIR}/vk-bootstrap)

set(SPIRV_REFLECT_FILES
    "${VENDOR_DIR}/SPIRV-Reflect/spirv_reflect.h"
    "${VENDOR_DIR}/SPIRV-Reflect/spirv_reflect.cpp"
)

# ------------------------------------------------------------------------------
# Target: glgpu
# ------------------------------------------------------------------------------
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_LIST_DIR}/include/*.h" "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")

add_library(glgpu ${SOURCE_FILES} ${SPIRV_REFLECT_FILES})

target_include_directories(glgpu
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${VENDOR_DIR}/SPIRV-Reflect
    ${VENDOR_DIR}/vk-bootstrap/src
    ${VENDOR_DIR}/vma
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(glgpu
    PRIVATE
    vk-bootstrap::vk-bootstrap
    ${Vulkan_LIBRARIES}
)

target_precompile_headers(glgpu PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include/pch.h)

# ------------------------------------------------------------------------------
# Target: testbed
# ------------------------------------------------------------------------------
if(GL_BUILD_TESTBED)
    add_subdirectory(testbed)
endif()
