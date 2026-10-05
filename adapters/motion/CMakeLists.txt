find_package(motionSampling CONFIG REQUIRED)
find_package(motionRetarget 0.5.3 CONFIG REQUIRED)
add_library(avatarMotionAdapter STATIC clipPoseAdapter.cpp)
add_library(AvatarRuntime::avatarMotionAdapter ALIAS avatarMotionAdapter)
target_compile_features(avatarMotionAdapter PUBLIC cxx_std_17)
set_target_properties(avatarMotionAdapter PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(avatarMotionAdapter PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(avatarMotionAdapter PUBLIC AvatarRuntime::avatarRuntime
    motionSampling::motionSampling motionRetarget::motionRetarget)
target_compile_definitions(avatarMotionAdapter PRIVATE
    AR_MOTION_RETARGET_VERSION="${motionRetarget_VERSION}"
    AR_MOTION_SAMPLING_VERSION="${motionSampling_VERSION}")
if(MSVC)
    target_compile_options(avatarMotionAdapter PRIVATE /W4 /permissive-)
endif()
install(TARGETS avatarMotionAdapter EXPORT AvatarMotionAdapterTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarMotionAdapterTargets NAMESPACE AvatarRuntime::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)

if(AVATAR_BUILD_TESTS)
    add_executable(motionAdapterContracts tests.cpp)
    target_link_libraries(motionAdapterContracts PRIVATE avatarMotionAdapter)
    if(AVATAR_BUILD_VRM_ADAPTER)
        target_compile_definitions(motionAdapterContracts PRIVATE AR_TEST_VRM)
        target_link_libraries(motionAdapterContracts PRIVATE avatarVrmAdapter)
    endif()
    add_test(NAME adapters.motion_pose COMMAND motionAdapterContracts)
    add_test(NAME adapters.motion_installed COMMAND ${CMAKE_COMMAND}
        -DROOT_BINARY=${PROJECT_BINARY_DIR}
        -DCONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer
        -DGENERATOR=${CMAKE_GENERATOR}
        -DPLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DRC_COMPILER=${CMAKE_RC_COMPILER}
        -DMT=${CMAKE_MT}
        -DSAMPLING_DIR=${motionSampling_DIR}
        -DRETARGET_DIR=${motionRetarget_DIR}
        -DMOTION_DIR=${motionCore_DIR}
        -DUSD_DIR=${pxr_DIR}
        -DVRM_DIR=${vrmRig_DIR}
        -DWITH_VRM=${AVATAR_BUILD_VRM_ADAPTER}
        -DCONFIG=$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/run.cmake)
    if(WIN32)
        add_custom_command(TARGET motionAdapterContracts POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:motionAdapterContracts>)
    endif()
endif()
