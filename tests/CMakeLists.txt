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

# When sanitizers are active, bake suppression file paths into the binary so
# the ASan/LSan/UBSan runtimes pick them up without requiring env var setup.
if(GPUKIT_ENABLE_SANITIZERS AND NOT MSVC)
    set(_sup_dir "${CMAKE_CURRENT_SOURCE_DIR}/sanitizer_suppressions")
    target_compile_definitions(gpukit_tests PRIVATE
        GPUKIT_LSAN_SUPPRESSIONS_PATH="${_sup_dir}/lsan.supp"
        GPUKIT_UBSAN_SUPPRESSIONS_PATH="${_sup_dir}/ubsan.supp"
    )
endif()
