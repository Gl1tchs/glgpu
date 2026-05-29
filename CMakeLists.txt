cmake_minimum_required(VERSION 3.5.0)

project(gpukit
    VERSION 1.4.8
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
option(GPUKIT_BUILD_COMPUTE_MODULE "Build the high-level compute module" ON)

# SIMD intrinsics are x86/x86_64 only; default OFF on every other architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|x86|i686|AMD64)$" AND NOT ANDROID)
    option(GPUKIT_USE_SIMD_INTRINSICS "Enable SIMD optimizations" ON)
else()
    option(GPUKIT_USE_SIMD_INTRINSICS "Enable SIMD optimizations" OFF)
endif()
option(GPUKIT_ENABLE_POSITION_INDEPENDENT_CODE "Enable PIC" OFF)

# Disable runtime shader compilation by default on Android
if(ANDROID)
    option(GPUKIT_ENABLE_SHADER_COMPILATION "Enable runtime GLSL->SPIRV via shaderc" OFF)
else()
    option(GPUKIT_ENABLE_SHADER_COMPILATION "Enable runtime GLSL->SPIRV via shaderc" ON)
endif()

# Use {fmt} instead of std::format. Auto-ON for Android and required when building with C++17.
if(ANDROID)
    option(GPUKIT_USE_FMT "Use {fmt} for string formatting" ON)
else()
    option(GPUKIT_USE_FMT "Use {fmt} for string formatting" OFF)
endif()

set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard version 17+")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_STANDARD LESS 20 AND NOT GPUKIT_USE_FMT)
    set(GPUKIT_USE_FMT ON CACHE BOOL "Use {fmt} for string formatting" FORCE)
    message(STATUS "GPUKit: C++17 detected — GPUKIT_USE_FMT automatically enabled")
endif()

if (GPUKIT_ENABLE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(VK_BOOTSTRAP_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)
endif()

option(GPUKIT_ENABLE_COVERAGE "Enable gcov code coverage instrumentation (Clang/GCC only)" OFF)
if(GPUKIT_ENABLE_COVERAGE)
    if(MSVC)
        message(WARNING "GPUKit: GPUKIT_ENABLE_COVERAGE is not supported with MSVC — ignoring")
    else()
        message(STATUS "GPUKit: Code coverage enabled")
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
endif()

option(GPUKIT_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan on Clang/GCC" OFF)
if(GPUKIT_ENABLE_SANITIZERS)
    if(MSVC)
        # MSVC ASan: available since VS 2019 16.4. UBSan has no MSVC equivalent.
        # /INCREMENTAL:NO is required — ASan is incompatible with incremental linking.
        message(STATUS "GPUKit: AddressSanitizer enabled (MSVC)")
        add_compile_options(/fsanitize=address)
        add_link_options(/INCREMENTAL:NO)
    else()
        message(STATUS "GPUKit: AddressSanitizer + UBSan enabled")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)

        # Generate CTestCustom.cmake so CTest passes suppression files to every test.
        # Suppressions cover known false positives in vendored code and GPU drivers.
        set(_sup_dir "${CMAKE_CURRENT_SOURCE_DIR}/tests/sanitizer_suppressions")
        file(WRITE "${CMAKE_BINARY_DIR}/CTestCustom.cmake"
            "set(CTEST_ENVIRONMENT\n"
            "    \"ASAN_OPTIONS=detect_leaks=1:suppressions=${_sup_dir}/lsan.supp\"\n"
            "    \"UBSAN_OPTIONS=suppressions=${_sup_dir}/ubsan.supp:print_stacktrace=1\"\n"
            ")\n"
        )
    endif()
endif()

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------
find_package(Vulkan REQUIRED)

if(GPUKIT_ENABLE_SHADER_COMPILATION)
    # Try Vulkan SDK's bundled shaderc first
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
        find_package(Vulkan QUIET OPTIONAL_COMPONENTS shaderc_combined)
    endif()

    if(TARGET Vulkan::shaderc_combined)
        set(GPUKIT_SHADERC_TARGET Vulkan::shaderc_combined)
        message(STATUS "GPUKit: Found shaderc via Vulkan SDK (CMake component)")
    else()
        # Fall back to a standalone shaderc installation
        find_library(GPUKIT_SHADERC_LIB NAMES shaderc_combined shaderc)
        find_path(GPUKIT_SHADERC_INCLUDE NAMES shaderc/shaderc.hpp shaderc/shaderc.h)

        if(GPUKIT_SHADERC_LIB AND GPUKIT_SHADERC_INCLUDE)
            add_library(_gpukit_shaderc_imported UNKNOWN IMPORTED)
            set_target_properties(_gpukit_shaderc_imported PROPERTIES
                IMPORTED_LOCATION "${GPUKIT_SHADERC_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${GPUKIT_SHADERC_INCLUDE}"
            )

            find_library(GPUKIT_GLSLANG_LIB NAMES glslang)
            find_library(GPUKIT_SPIRV_LIB NAMES SPIRV)
            find_library(GPUKIT_SPIRV_TOOLS_LIB NAMES SPIRV-Tools)
            find_library(GPUKIT_SPIRV_TOOLS_OPT_LIB NAMES SPIRV-Tools-opt)

            set(_shaderc_extra "")
            foreach(_dep GPUKIT_GLSLANG_LIB GPUKIT_SPIRV_LIB GPUKIT_SPIRV_TOOLS_LIB GPUKIT_SPIRV_TOOLS_OPT_LIB)
                if(${_dep})
                    list(APPEND _shaderc_extra "${${_dep}}")
                endif()
            endforeach()

            if(_shaderc_extra)
                set_target_properties(_gpukit_shaderc_imported PROPERTIES
                    INTERFACE_LINK_LIBRARIES "${_shaderc_extra}"
                )
            endif()

            set(GPUKIT_SHADERC_TARGET _gpukit_shaderc_imported)
            message(STATUS "GPUKit: Found shaderc library at ${GPUKIT_SHADERC_LIB}")
        else()
            set(GPUKIT_SHADERC_TARGET "")
            set(GPUKIT_USE_GLSLC_BINARY ON)
            message(STATUS "GPUKit: shaderc not found — falling back to glslc binary")
        endif()
    endif()
endif()

set(VENDOR_DIR ${CMAKE_CURRENT_LIST_DIR}/external)
set(SPIRV_REFLECT_FILES "${VENDOR_DIR}/SPIRV-Reflect/spirv_reflect.cpp")

# ------------------------------------------------------------------------------
# GpuKit target
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

# Link wayland-client on Linux (desktop only) for Wayland surface support
if(UNIX AND NOT APPLE AND NOT ANDROID)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND_CLIENT wayland-client)
        if(WAYLAND_CLIENT_FOUND)
            target_link_libraries(gpukit PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
            target_include_directories(gpukit PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
            target_compile_definitions(gpukit PRIVATE GPUKIT_HAS_WAYLAND)
        endif()
    endif()
endif()

if(GPUKIT_USE_FMT)
    find_package(fmt QUIET)
    if(fmt_FOUND)
        message(STATUS "GPUKit: Found system {fmt} ${fmt_VERSION}")
    else()
        include(FetchContent)
        FetchContent_Declare(fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG        10.2.1
            GIT_SHALLOW    ON
        )
        set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(fmt)
        message(STATUS "GPUKit: {fmt} not found on system — fetched via FetchContent")
    endif()
    target_link_libraries(gpukit PUBLIC fmt::fmt-header-only)
    target_compile_definitions(gpukit PUBLIC GPUKIT_USE_FMT_FORMAT)
endif()

if(ANDROID)
    target_link_libraries(gpukit PRIVATE log)
endif()

# Shader compilation backend
if(GPUKIT_ENABLE_SHADER_COMPILATION)
    if(GPUKIT_SHADERC_TARGET)
        target_link_libraries(gpukit PRIVATE ${GPUKIT_SHADERC_TARGET})
        target_compile_definitions(gpukit PRIVATE GPUKIT_HAS_SHADERC)
        # shaderc_combined may not be self-contained (e.g. Arch Linux); probe and
        # add glslang / SPIRV-Tools explicitly so the linker can resolve all symbols.
        find_library(GPUKIT_GLSLANG_LIB        NAMES glslang)
        find_library(GPUKIT_SPIRV_LIB           NAMES SPIRV)
        find_library(GPUKIT_SPIRV_TOOLS_LIB     NAMES SPIRV-Tools)
        find_library(GPUKIT_SPIRV_TOOLS_OPT_LIB NAMES SPIRV-Tools-opt)
        foreach(_dep GPUKIT_GLSLANG_LIB GPUKIT_SPIRV_LIB
                     GPUKIT_SPIRV_TOOLS_LIB GPUKIT_SPIRV_TOOLS_OPT_LIB)
            if(${_dep})
                target_link_libraries(gpukit PRIVATE "${${_dep}}")
            endif()
        endforeach()
    elseif(GPUKIT_USE_GLSLC_BINARY)
        target_compile_definitions(gpukit PRIVATE GPUKIT_HAS_GLSLC_BINARY)
    endif()
endif()

if(NOT ANDROID)
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

    # Install Android glue (NDK-only; not compiled on desktop)
    install(FILES misc/gpukit_android_glue.h
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
endif() # NOT ANDROID

# ------------------------------------------------------------------------------
# Subdirectories
# ------------------------------------------------------------------------------
if(GPUKIT_BUILD_COMPUTE_MODULE)
    add_subdirectory(compute)
endif()

if(GPUKIT_BUILD_EXAMPLES AND NOT ANDROID)
    add_subdirectory(examples)
endif()

if(GPUKIT_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()
