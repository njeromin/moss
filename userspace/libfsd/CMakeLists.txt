add_library(libfsd STATIC
    src/server.c
    src/client.c
)

target_include_directories(libfsd PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(libfsd PUBLIC
    libc
    userspace_options
    common
)
