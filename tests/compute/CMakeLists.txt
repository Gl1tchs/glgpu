add_executable(gpukit_compute_tests
    test_entry.cpp
    test_tensor.cpp
    test_kernel.cpp
    test_stream.cpp
)

target_include_directories(gpukit_compute_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..  # for test_common.h
)

target_link_libraries(gpukit_compute_tests
    PRIVATE
        gpukit::compute
        Catch2::Catch2
)

catch_discover_tests(gpukit_compute_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

if(GPUKIT_ENABLE_SANITIZERS AND NOT MSVC)
    set(_sup_dir "${CMAKE_SOURCE_DIR}/tests/sanitizer_suppressions")
    target_compile_definitions(gpukit_compute_tests PRIVATE
        GPUKIT_LSAN_SUPPRESSIONS_PATH="${_sup_dir}/lsan.supp"
        GPUKIT_UBSAN_SUPPRESSIONS_PATH="${_sup_dir}/ubsan.supp"
    )
endif()
