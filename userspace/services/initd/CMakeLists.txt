add_executable(initd main.c)
target_link_libraries(initd PRIVATE userspace_options userspace_crt)
set_target_properties(initd PROPERTIES SUFFIX .elf
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/userspace")
set_property(TARGET initd APPEND PROPERTY LINK_DEPENDS "${USERSPACE_LINKER_SCRIPT}")
