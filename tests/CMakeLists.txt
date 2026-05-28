include(FetchContent)
include(CTest)

find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
message(STATUS "Catch2 not found locally. Fetching v3.4.0 from GitHub...")
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)
endif()

include(Catch)

# Add Test Executable
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(gpukit_tests ${TEST_SOURCES})

# Link Dependencies
target_link_libraries(gpukit_tests
    PRIVATE
        gpukit::gpukit
        Catch2::Catch2
)

catch_discover_tests(gpukit_tests)

# Coverage report target — only wired up when lcov + genhtml are present.
if(GPUKIT_ENABLE_COVERAGE AND NOT MSVC)
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)

    if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
        set(_cov_dir "${CMAKE_BINARY_DIR}/coverage")
        set(_cov_info "${_cov_dir}/coverage.info")

        add_custom_target(coverage
            COMMENT "Collecting coverage and generating HTML report..."
            # Reset stale counters from any previous run
            COMMAND ${LCOV_EXECUTABLE} --zerocounters --directory "${CMAKE_BINARY_DIR}"
            # Run the full test suite
            COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure -j1
            # Capture all counter data
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_cov_dir}"
            COMMAND ${LCOV_EXECUTABLE}
                --capture
                --ignore-errors inconsistent
                --directory "${CMAKE_BINARY_DIR}"
                --output-file "${_cov_info}"
            # Extract and filter the data
            COMMAND ${LCOV_EXECUTABLE}
                --extract "${_cov_info}"
                --ignore-errors inconsistent
                "${CMAKE_SOURCE_DIR}/src/*"
                --output-file "${_cov_info}"
            # Render the HTML report
            COMMAND ${GENHTML_EXECUTABLE}
                "${_cov_info}"
                --output-directory "${_cov_dir}/html"
                --title "GPUKit"
                --show-details
                --demangle-cpp
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            DEPENDS gpukit_tests
            VERBATIM
        )
        message(STATUS "GPUKit: 'coverage' target available — cmake --build build-cov --target coverage")
    else()
        message(STATUS "GPUKit: lcov/genhtml not found — install lcov for HTML coverage reports")
    endif()
endif()

# When sanitizers are active, bake suppression file paths into the binary so
# the ASan/LSan/UBSan runtimes pick them up without requiring env var setup.
if(GPUKIT_ENABLE_SANITIZERS AND NOT MSVC)
    set(_sup_dir "${CMAKE_CURRENT_SOURCE_DIR}/sanitizer_suppressions")
    target_compile_definitions(gpukit_tests PRIVATE
        GPUKIT_LSAN_SUPPRESSIONS_PATH="${_sup_dir}/lsan.supp"
        GPUKIT_UBSAN_SUPPRESSIONS_PATH="${_sup_dir}/ubsan.supp"
    )
endif()
