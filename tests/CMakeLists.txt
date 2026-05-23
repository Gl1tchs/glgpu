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

add_executable(glgpu_tests ${TEST_SOURCES})

# Link Dependencies
target_link_libraries(glgpu_tests 
    PRIVATE
        glgpu::glgpu 
        Catch2::Catch2
)

catch_discover_tests(glgpu_tests)
