find_package(motionRetarget 0.5.3 CONFIG REQUIRED)
find_package(motionUsd 0.5.3 CONFIG REQUIRED)
include(${PROJECT_SOURCE_DIR}/cmake/AvatarMotionUsdReading.cmake)
avatar_require_motion_usd_reading()
find_package(pxr CONFIG REQUIRED)
add_library(avatarUsdBinding STATIC skeletonBinding.cpp)
add_library(AvatarRuntime::avatarUsdBinding ALIAS avatarUsdBinding)
target_compile_features(avatarUsdBinding PUBLIC cxx_std_17)
target_compile_definitions(avatarUsdBinding PRIVATE AR_MOTION_USD_VERSION="${motionUsd_VERSION}")
set_target_properties(avatarUsdBinding PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(avatarUsdBinding PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(avatarUsdBinding PUBLIC AvatarRuntime::avatarRuntime
    motionRetarget::motionRetarget motionUsd::motionUsd usdSkel usdGeom)
if(MSVC)
    target_compile_options(avatarUsdBinding PRIVATE /W4 /permissive-)
endif()
install(TARGETS avatarUsdBinding EXPORT AvatarUsdBindingTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarUsdBindingTargets NAMESPACE AvatarRuntime::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)

if(AVATAR_BUILD_TESTS)
    add_executable(usdBindingContracts tests.cpp)
    target_link_libraries(usdBindingContracts PRIVATE avatarUsdBinding)
    if(AVATAR_BUILD_MOTION_ADAPTER)
        target_compile_definitions(usdBindingContracts PRIVATE AR_TEST_MOTION)
        target_link_libraries(usdBindingContracts PRIVATE avatarMotionAdapter)
        if(AVATAR_BUILD_VRM_ADAPTER)
            target_compile_definitions(usdBindingContracts PRIVATE AR_TEST_VRM)
            target_link_libraries(usdBindingContracts PRIVATE avatarVrmAdapter)
        endif()
    endif()
    add_test(NAME adapters.usd_skeleton COMMAND usdBindingContracts)
    add_test(NAME adapters.usd_installed COMMAND ${CMAKE_COMMAND}
        -DROOT_BINARY=${PROJECT_BINARY_DIR}
        -DCONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer
        -DGENERATOR=${CMAKE_GENERATOR}
        -DPLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DRC_COMPILER=${CMAKE_RC_COMPILER}
        -DMT=${CMAKE_MT}
        -DRETARGET_DIR=${motionRetarget_DIR}
        -DMOTION_USD_DIR=${motionUsd_DIR}
        -DSAMPLING_DIR=${motionSampling_DIR}
        -DMOTION_DIR=${motionCore_DIR}
        -DUSD_DIR=${pxr_DIR}
        -DVRM_DIR=${vrmRig_DIR}
        -DWITH_MOTION=${AVATAR_BUILD_MOTION_ADAPTER}
        -DWITH_VRM=${AVATAR_BUILD_VRM_ADAPTER}
        -DCONFIG=$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/run.cmake)
    if(WIN32)
        add_custom_command(TARGET usdBindingContracts POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:usdBindingContracts>)
    endif()
endif()
