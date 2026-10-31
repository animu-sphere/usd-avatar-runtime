# Standalone entry for `ost library build`: the core and the session's adapters in
# one prefix, because AvatarRuntimeConfig.cmake finds components beside itself.
cmake_minimum_required(VERSION 3.22)
project(avatarVrmSessionLibrary LANGUAGES NONE)
enable_testing()
foreach(option AVATAR_BUILD_TESTS AVATAR_BUILD_VRM_SESSION AVATAR_BUILD_VRM_ADAPTER AVATAR_BUILD_USD_BINDING
        AVATAR_BUILD_VRM_USD_BINDING AVATAR_BUILD_VRM_SPRING_USD_BINDING
        AVATAR_BUILD_VRM_SPRING_ADAPTER AVATAR_BUILD_MOTION_USD_BINDING
        AVATAR_BUILD_MOTION_ADAPTER)
    set(${option} ON CACHE BOOL "Selected by the avatarVrmSession package")
endforeach()
add_subdirectory(../.. root)
