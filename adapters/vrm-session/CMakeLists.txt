add_library(avatarVrmSession STATIC session.cpp)
add_library(AvatarRuntime::avatarVrmSession ALIAS avatarVrmSession)
target_compile_features(avatarVrmSession PUBLIC cxx_std_17)
set_target_properties(avatarVrmSession PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(avatarVrmSession PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(avatarVrmSession PUBLIC avatarVrmUsdBinding avatarVrmSpringUsdBinding
    avatarVrmSpringAdapter avatarMotionUsdBinding avatarMotionAdapter)
install(TARGETS avatarVrmSession EXPORT AvatarVrmSessionTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarVrmSessionTargets NAMESPACE AvatarRuntime:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)
configure_package_config_file(${PROJECT_SOURCE_DIR}/cmake/AvatarVrmSessionConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/AvatarVrmSessionConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarVrmSession)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AvatarVrmSessionConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AvatarVrmSessionConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AvatarVrmSessionConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarVrmSession)
add_executable(avatarVrmSessionCheck check.cpp)
target_link_libraries(avatarVrmSessionCheck PRIVATE avatarVrmSession)
if(WIN32)
    add_custom_command(TARGET avatarVrmSessionCheck POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:avatarVrmSessionCheck>)
endif()
install(TARGETS avatarVrmSessionCheck RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(AVATAR_BUILD_TESTS)
    add_executable(vrmSessionContracts check.cpp tests.cpp)
    target_compile_definitions(vrmSessionContracts PRIVATE AR_VRM_SESSION_TEST_ENTRY)
    target_link_libraries(vrmSessionContracts PRIVATE avatarVrmSession)
    add_test(NAME hosts.vrm_session COMMAND vrmSessionContracts ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
    avatar_register_vrm_schema(hosts.vrm_session)
    if(PROJECT_IS_TOP_LEVEL)
        add_test(NAME hosts.vrm_session_installed COMMAND ${CMAKE_COMMAND}
            -DROOT_BINARY=${PROJECT_BINARY_DIR}
            -DCONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer
            -DGENERATOR=${CMAKE_GENERATOR} -DPLATFORM=${CMAKE_GENERATOR_PLATFORM}
            -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM} -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DRC_COMPILER=${CMAKE_RC_COMPILER} -DMT=${CMAKE_MT}
            -DMOTION_DIR=${motionCore_DIR} -DRETARGET_DIR=${motionRetarget_DIR}
            -DSAMPLING_DIR=${motionSampling_DIR} -DMOTION_USD_DIR=${motionUsd_DIR}
            -DUSD_DIR=${pxr_DIR} -DSCHEMA_DIR=${vrmSchema_DIR} -DVRM_DIR=${vrmRig_DIR}
            -DCONFIG=$<CONFIG> -P ${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/run.cmake)
    endif()
    if(WIN32)
        add_custom_command(TARGET vrmSessionContracts POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:vrmSessionContracts>)
    endif()
endif()
