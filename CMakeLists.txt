cmake_minimum_required(VERSION 3.5.0)

project(gpukit
    VERSION 1.3.0
    DESCRIPTION "Low-Level, low dependency rendering interface over the Vulkan API"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------
option(GPUKIT_BUILD_EXAMPLES "Build examples" ON)
option(GPUKIT_BUILD_TESTS "Build tests" ON)
option(GPUKIT_USE_SIMD_INTRINSICS "Enable SIMD optimizations" ON)
option(GPUKIT_ENABLE_POSITION_INDEPENDENT_CODE "Enable PIC" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (GPUKIT_ENABLE_POSITION_INDEPENDENT_CODE)
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
# Target: gpukit
# ------------------------------------------------------------------------------
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(gpukit ${SOURCE_FILES} ${SPIRV_REFLECT_FILES})
add_library(gpukit::gpukit ALIAS gpukit)

# Generate and include version header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/misc/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/gpukit/version.h
)

target_include_directories(gpukit PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)

target_compile_definitions(gpukit PUBLIC
    $<$<CONFIG:Debug>:GPUKIT_DEBUG_BUILD>
    $<$<CONFIG:Release>:GPUKIT_RELEASE_BUILD>
    $<$<CONFIG:RelWithDebInfo>:GPUKIT_DEBUG_BUILD;GPUKIT_RELEASE_BUILD>
    $<$<CONFIG:MinSizeRel>:GPUKIT_RELEASE_BUILD;GPUKIT_DIST_BUILD>
)

if(GPUKIT_USE_SIMD_INTRINSICS)
    target_compile_definitions(gpukit PUBLIC GPUKIT_USE_SIMD_INTRINSICS)
endif()

# Shared Library Specific Configuration
if(BUILD_SHARED_LIBS)
    target_compile_definitions(gpukit PRIVATE GPUKIT_BUILD_DLL)

    set_target_properties(gpukit PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
else()
    target_compile_definitions(gpukit PUBLIC GPUKIT_STATIC_BUILD)
endif()

set_target_properties(gpukit PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ------------------------------------------------------------------------------
# Includes & Linking
# ------------------------------------------------------------------------------
target_include_directories(gpukit
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
    target_link_libraries(gpukit PRIVATE Vulkan::Vulkan)
else()
    target_link_libraries(gpukit PRIVATE ${Vulkan_LIBRARIES})
    target_include_directories(gpukit PRIVATE ${Vulkan_INCLUDE_DIRS})
endif()

# Link wayland-client on Linux for Wayland surface support
if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND_CLIENT wayland-client)
        if(WAYLAND_CLIENT_FOUND)
            target_link_libraries(gpukit PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
            target_include_directories(gpukit PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
        endif()
    endif()
endif()

# ------------------------------------------------------------------------------
# Installation & Exporting
# ------------------------------------------------------------------------------

# Install the library binary
install(TARGETS gpukit
    EXPORT gpukitTargets
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
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/gpukit/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gpukit
)

# Install SDL2 glue
install(FILES misc/gpukit_sdl2_glue.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install GLFW glue
install(FILES misc/gpukit_glfw_glue.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install ImGui glue
install(FILES misc/gpukit_imgui_glue.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

#  Create the *Targets.cmake file
install(EXPORT gpukitTargets
    FILE gpukitTargets.cmake
    NAMESPACE gpukit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gpukit
)

# Create *Config.cmake files
write_basic_package_version_file(
    gpukitConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gpukitConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/gpukitConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gpukit
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/gpukitConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/gpukitConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/gpukit
)

# ------------------------------------------------------------------------------
# Subdirectories
# ------------------------------------------------------------------------------
if(GPUKIT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(GPUKIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
