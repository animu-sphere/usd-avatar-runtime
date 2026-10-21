cmake_minimum_required(VERSION 3.22)
project(StageRunnerInstalledConsumer LANGUAGES CXX)
find_package(AvatarRuntime 0.1.0 EXACT CONFIG REQUIRED COMPONENTS stage_runner)
foreach(unexpected IN ITEMS mmdControl::mmdControl physicsCore::physicsCore physicsJolt::physicsJolt
    Toon::AvatarState usd AvatarRuntime::avatarMmdAdapter
    AvatarRuntime::avatarPhysicsUsdResources physicsUsd::physicsUsd)
    if(TARGET ${unexpected})
        message(FATAL_ERROR "Session-only lookup imported ${unexpected}")
    endif()
endforeach()
add_executable(sessionConsumer ../tests.cpp)
target_link_libraries(sessionConsumer PRIVATE AvatarRuntime::avatarStageRunnerSession)
if(WIN32)
    add_custom_command(TARGET sessionConsumer POST_BUILD COMMAND ${CMAKE_COMMAND} -E
        copy_if_different $<TARGET_FILE:AvatarRuntime::avatarRuntime> $<TARGET_FILE_DIR:sessionConsumer>)
endif()
enable_testing()
add_test(NAME installed.stage_runner_session COMMAND sessionConsumer)
