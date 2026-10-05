cmake_minimum_required(VERSION 3.22)
project(InstalledVrmAdapter LANGUAGES CXX)
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS vrm)
enable_testing()
# Reuse conformance inputs, but obtain every header/library from installed targets.
add_executable(installedVrmAdapter ../tests.cpp)
target_link_libraries(installedVrmAdapter PRIVATE AvatarRuntime::avatarVrmAdapter)
if(WIN32)
    add_custom_command(TARGET installedVrmAdapter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:AvatarRuntime::avatarRuntime> $<TARGET_FILE_DIR:installedVrmAdapter>)
endif()
add_test(NAME installed.vrm_owner_boundary COMMAND installedVrmAdapter)
