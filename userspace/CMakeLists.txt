# Userspace programs and servers will be added here as they are implemented.
# Keep this target present so the top-level build has a stable userspace entry.
add_custom_target(userspace
    COMMAND "${CMAKE_COMMAND}" -E echo
        "No userspace programs yet.")
