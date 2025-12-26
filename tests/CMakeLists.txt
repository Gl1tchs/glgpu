include(FetchContent)

find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found locally. Fetching from GitHub...")

    include(FetchContent)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.11.0
    )

    FetchContent_MakeAvailable(Catch2)

    # For catch_discover_tests
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

add_executable(glgpu_tests ${TEST_SOURCES})

target_include_directories(glgpu_tests PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries(glgpu_tests PRIVATE
    Catch2::Catch2WithMain
    glgpu
)

# CTest integration
include(CTest)
include(Catch)
catch_discover_tests(glgpu_tests)

