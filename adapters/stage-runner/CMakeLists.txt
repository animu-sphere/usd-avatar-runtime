find_package(runtimeCore 0.1 CONFIG REQUIRED)
add_library(avatarStageRunnerSession STATIC physicsSession.cpp)
add_library(AvatarRuntime::avatarStageRunnerSession ALIAS avatarStageRunnerSession)
target_compile_features(avatarStageRunnerSession PUBLIC cxx_std_20)
set_target_properties(avatarStageRunnerSession PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(avatarStageRunnerSession PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(avatarStageRunnerSession PUBLIC AvatarRuntime::avatarRuntime runtimeCore::runtimeCore)
if(MSVC)
    target_compile_options(avatarStageRunnerSession PRIVATE /W4 /permissive-)
endif()
install(TARGETS avatarStageRunnerSession EXPORT AvatarStageRunnerSessionTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarStageRunnerSessionTargets NAMESPACE AvatarRuntime::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)
if(AVATAR_BUILD_TESTS)
    add_executable(stageRunnerSessionContracts tests.cpp)
    target_link_libraries(stageRunnerSessionContracts PRIVATE AvatarRuntime::avatarStageRunnerSession)
    add_test(NAME adapters.stage_runner_session COMMAND stageRunnerSessionContracts)
    if(WIN32)
        add_custom_command(TARGET stageRunnerSessionContracts POST_BUILD COMMAND ${CMAKE_COMMAND} -E
            copy_if_different $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:stageRunnerSessionContracts>)
    endif()
    add_test(NAME adapters.stage_runner_installed COMMAND ${CMAKE_COMMAND}
        "-DROOT_BINARY=${PROJECT_BINARY_DIR}" "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DruntimeCore_DIR=${runtimeCore_DIR}" "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}" "-DCONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/installed.cmake")
endif()
