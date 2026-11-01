# Standalone entry for `ost library build`: the core and the session's adapters in
# one prefix, because AvatarRuntimeConfig.cmake finds components beside itself.
cmake_minimum_required(VERSION 3.22)
project(avatarMmdSessionLibrary LANGUAGES NONE)
enable_testing()
foreach(option AVATAR_BUILD_TESTS AVATAR_BUILD_MMD_SESSION AVATAR_BUILD_MMD_ADAPTER AVATAR_BUILD_USD_BINDING
        AVATAR_BUILD_STAGE_RUNNER_SESSION AVATAR_BUILD_PHYSICS_USD_RESOURCES)
    set(${option} ON CACHE BOOL "Selected by the avatarMmdSession package")
endforeach()
add_subdirectory(../.. root)
