cmake_minimum_required(VERSION 3.5.0)

project(glgpu VERSION 1.0.5
    DESCRIPTION "Low-Level, low dependency rendering interface over the Vulkan API"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
option(GL_BUILD_EXAMPLES "Build examples" ON)
option(GL_BUILD_TESTS "Build tests" ON)
option(GL_WITH_VULKAN "Enable vulkan backend" ON)
option(GL_USE_SIMD_INTRINSICS "Enable SIMD optimizations" ON)
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
set(SPIRV_REFLECT_FILES "${VENDOR_DIR}/SPIRV-Reflect/spirv_reflect.cpp")

# ------------------------------------------------------------------------------
# Target: glgpu
# ------------------------------------------------------------------------------
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h" 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(glgpu ${SOURCE_FILES} ${SPIRV_REFLECT_FILES})
add_library(glgpu::glgpu ALIAS glgpu)

# Generate and include version header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/glgpu/version.h
)

target_include_directories(glgpu PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)

target_compile_definitions(glgpu PUBLIC
    $<$<CONFIG:Debug>:GL_DEBUG_BUILD>
    $<$<CONFIG:Release>:GL_RELEASE_BUILD>
    $<$<CONFIG:RelWithDebInfo>:GL_DEBUG_BUILD;GL_RELEASE_BUILD>
    $<$<CONFIG:MinSizeRel>:GL_RELEASE_BUILD;GL_DIST_BUILD>
)

if(GL_USE_SIMD_INTRINSICS)
    target_compile_definitions(glgpu PUBLIC GL_USE_SIMD_INTRINSICS)
endif()

# Shared Library Specific Configuration
if(BUILD_SHARED_LIBS)
    target_compile_definitions(glgpu PRIVATE GLGPU_BUILD_DLL)

    set_target_properties(glgpu PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
else()
    target_compile_definitions(glgpu PUBLIC GLGPU_STATIC_BUILD)
endif()

set_target_properties(glgpu PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ------------------------------------------------------------------------------
# Includes & Linking
# ------------------------------------------------------------------------------
target_include_directories(glgpu
    PUBLIC
        # Headers for the build tree
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        # Headers for the install tree
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${VENDOR_DIR}/SPIRV-Reflect
        ${VENDOR_DIR}/vma
)

# Link Vulkan::Vulkan if available, otherwise fallback to libraries
if(TARGET Vulkan::Vulkan)
    target_link_libraries(glgpu PRIVATE Vulkan::Vulkan)
else()
    target_link_libraries(glgpu PRIVATE ${Vulkan_LIBRARIES})
    target_include_directories(glgpu PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

# Link wayland-client on Linux for Wayland surface support
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND_CLIENT wayland-client)
        if(WAYLAND_CLIENT_FOUND)
            target_link_libraries(glgpu PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
            target_include_directories(glgpu PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
        endif()
    endif()
endif()

# ------------------------------------------------------------------------------
# Installation & Exporting
# ------------------------------------------------------------------------------

# Install the library binary
install(TARGETS glgpu
    EXPORT glgpuTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install the headers
install(DIRECTORY include/ 
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install generated version header
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/glgpu/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/glgpu
)

# Install SDL2 glue
install(FILES misc/glgpu_sdl2_glue.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} 
)

# Install GLFW glue
install(FILES misc/glgpu_glfw_glue.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install ImGui glue
install(FILES misc/glgpu_imgui_glue.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

#  Create the *Targets.cmake file
install(EXPORT glgpuTargets
    FILE glgpuTargets.cmake
    NAMESPACE glgpu::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/glgpu
)

# Create *Config.cmake files
write_basic_package_version_file(
    glgpuConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/glgpuConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/glgpuConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/glgpu
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/glgpuConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/glgpuConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/glgpu
)

# ------------------------------------------------------------------------------
# Subdirectories
# ------------------------------------------------------------------------------
if(GL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(GL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
