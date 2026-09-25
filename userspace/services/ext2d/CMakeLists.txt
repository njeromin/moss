add_executable(ext2d main.c)
target_link_libraries(ext2d PRIVATE libfsd libc userspace_options userspace_crt)
set_target_properties(ext2d PROPERTIES SUFFIX .elf
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/userspace")
set_property(TARGET ext2d APPEND PROPERTY LINK_DEPENDS "${USERSPACE_LINKER_SCRIPT}")
