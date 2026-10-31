# Standalone entry for `ost library build`: the core's targets, tests and install
# rules stay in the repository root, whose optional adapters default to OFF.
cmake_minimum_required(VERSION 3.22)
project(avatarRuntimeLibrary LANGUAGES NONE)
enable_testing()
set(AVATAR_BUILD_TESTS ON CACHE BOOL "Build contract and direct execution tests")
add_subdirectory(../.. root)
