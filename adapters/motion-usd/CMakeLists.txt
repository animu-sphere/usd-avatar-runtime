find_package(motionUsd 0.5.4 CONFIG REQUIRED)
avatar_require_motion_usd_inputs()
add_library(avatarMotionUsdBinding STATIC readStageClip.cpp stageClip.cpp)
add_library(AvatarRuntime::avatarMotionUsdBinding ALIAS avatarMotionUsdBinding)
target_compile_features(avatarMotionUsdBinding PUBLIC cxx_std_17)
target_compile_definitions(avatarMotionUsdBinding PRIVATE AR_MOTION_USD_VERSION="${motionUsd_VERSION}")
set_target_properties(avatarMotionUsdBinding PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(avatarMotionUsdBinding PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(avatarMotionUsdBinding PUBLIC AvatarRuntime::avatarUsdBinding motionUsd::motionUsd)
if(MSVC)
    target_compile_options(avatarMotionUsdBinding PRIVATE /W4 /permissive-)
endif()
install(TARGETS avatarMotionUsdBinding EXPORT AvatarMotionUsdBindingTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarMotionUsdBindingTargets NAMESPACE AvatarRuntime:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)
if(AVATAR_BUILD_TESTS)
    add_executable(motionUsdBindingContracts tests.cpp)
    target_link_libraries(motionUsdBindingContracts PRIVATE avatarMotionUsdBinding)
    if(WIN32)
        add_custom_command(TARGET motionUsdBindingContracts POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:motionUsdBindingContracts>)
    endif()
    add_test(NAME adapters.motion_usd_clip COMMAND motionUsdBindingContracts)
    add_test(NAME adapters.motion_usd_installed COMMAND ${CMAKE_COMMAND}
        -DROOT_BINARY=${PROJECT_BINARY_DIR} -DCONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer
        -DGENERATOR=${CMAKE_GENERATOR} -DPLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM} -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DRC_COMPILER=${CMAKE_RC_COMPILER} -DMT=${CMAKE_MT}
        -DRETARGET_DIR=${motionRetarget_DIR} -DMOTION_DIR=${motionCore_DIR}
        -DMOTION_USD_DIR=${motionUsd_DIR} -DUSD_DIR=${pxr_DIR}
        -DCONFIG=$<CONFIG> -P ${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/run.cmake)
endif()
