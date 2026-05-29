add_library(gpukit_compute
    src/kernel.cpp
    src/stream.cpp
    src/tensor.cpp
)
add_library(gpukit::compute ALIAS gpukit_compute)

target_compile_features(gpukit_compute PUBLIC cxx_std_17)

target_include_directories(gpukit_compute PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(gpukit_compute PUBLIC gpukit::gpukit)

if(NOT ANDROID)
    install(TARGETS gpukit_compute
        EXPORT gpukitTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/gpukit/compute
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gpukit
    )
endif()
