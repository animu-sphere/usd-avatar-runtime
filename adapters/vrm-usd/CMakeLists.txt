find_package(vrmSchema 0.9.0 CONFIG REQUIRED)
add_library(avatarVrmUsdBinding STATIC humanoidBinding.cpp)
add_library(AvatarRuntime::avatarVrmUsdBinding ALIAS avatarVrmUsdBinding)
target_compile_features(avatarVrmUsdBinding PUBLIC cxx_std_17)
set_target_properties(avatarVrmUsdBinding PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(avatarVrmUsdBinding PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(avatarVrmUsdBinding PUBLIC AvatarRuntime::avatarUsdBinding vrmSchema::vrmSchema)
if(MSVC)
    target_compile_options(avatarVrmUsdBinding PRIVATE /W4 /permissive-)
endif()
install(TARGETS avatarVrmUsdBinding EXPORT AvatarVrmUsdBindingTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarVrmUsdBindingTargets NAMESPACE AvatarRuntime::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)

if(AVATAR_BUILD_VRM_ADAPTER)
    add_library(avatarVrmExpressionUsdBinding STATIC expressionBinding.cpp)
    add_library(AvatarRuntime::avatarVrmExpressionUsdBinding ALIAS avatarVrmExpressionUsdBinding)
    target_compile_features(avatarVrmExpressionUsdBinding PUBLIC cxx_std_17)
    set_target_properties(avatarVrmExpressionUsdBinding PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(avatarVrmExpressionUsdBinding PUBLIC avatarVrmUsdBinding avatarVrmAdapter usdShade)
    if(MSVC)
        target_compile_options(avatarVrmExpressionUsdBinding PRIVATE /W4 /permissive-)
    endif()
    install(TARGETS avatarVrmExpressionUsdBinding EXPORT AvatarVrmExpressionUsdBindingTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(EXPORT AvatarVrmExpressionUsdBindingTargets NAMESPACE AvatarRuntime::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)
    add_library(avatarVrmLookAtUsdBinding STATIC lookAtBinding.cpp)
    add_library(AvatarRuntime::avatarVrmLookAtUsdBinding ALIAS avatarVrmLookAtUsdBinding)
    target_compile_features(avatarVrmLookAtUsdBinding PUBLIC cxx_std_17)
    set_target_properties(avatarVrmLookAtUsdBinding PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(avatarVrmLookAtUsdBinding PUBLIC avatarVrmUsdBinding avatarVrmAdapter)
    if(MSVC)
        target_compile_options(avatarVrmLookAtUsdBinding PRIVATE /W4 /permissive-)
    endif()
    install(TARGETS avatarVrmLookAtUsdBinding EXPORT AvatarVrmLookAtUsdBindingTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(EXPORT AvatarVrmLookAtUsdBindingTargets NAMESPACE AvatarRuntime::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)
endif()

if(AVATAR_BUILD_TESTS)
    add_executable(vrmUsdBindingContracts tests.cpp)
    target_link_libraries(vrmUsdBindingContracts PRIVATE avatarVrmUsdBinding)
    if(AVATAR_BUILD_MOTION_ADAPTER)
        target_compile_definitions(vrmUsdBindingContracts PRIVATE AR_TEST_MOTION)
        target_link_libraries(vrmUsdBindingContracts PRIVATE avatarMotionAdapter)
    endif()
    add_test(NAME adapters.vrm_usd_humanoid COMMAND vrmUsdBindingContracts)
    if(AVATAR_BUILD_VRM_ADAPTER)
        add_executable(vrmExpressionUsdBindingContracts expressionTests.cpp)
        target_link_libraries(vrmExpressionUsdBindingContracts PRIVATE avatarVrmExpressionUsdBinding avatarVrmLookAtUsdBinding)
        add_test(NAME adapters.vrm_usd_expression COMMAND vrmExpressionUsdBindingContracts)
        add_executable(vrmLookAtUsdBindingContracts lookAtTests.cpp)
        target_link_libraries(vrmLookAtUsdBindingContracts PRIVATE avatarVrmLookAtUsdBinding)
        add_test(NAME adapters.vrm_usd_lookat COMMAND vrmLookAtUsdBindingContracts)
    endif()
    add_test(NAME adapters.vrm_usd_installed COMMAND ${CMAKE_COMMAND}
        -DROOT_BINARY=${PROJECT_BINARY_DIR}
        -DCONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer
        -DGENERATOR=${CMAKE_GENERATOR}
        -DPLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DRC_COMPILER=${CMAKE_RC_COMPILER}
        -DMT=${CMAKE_MT}
        -DRETARGET_DIR=${motionRetarget_DIR}
        -DSAMPLING_DIR=${motionSampling_DIR}
        -DMOTION_DIR=${motionCore_DIR}
        -DUSD_DIR=${pxr_DIR}
        -DSCHEMA_DIR=${vrmSchema_DIR}
        -DVRM_DIR=${vrmRig_DIR}
        -DWITH_LOOKAT=${AVATAR_BUILD_VRM_ADAPTER}
        -DWITH_MOTION=${AVATAR_BUILD_MOTION_ADAPTER}
        -DCONFIG=$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/run.cmake)
    if(WIN32)
        add_custom_command(TARGET vrmUsdBindingContracts POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:vrmUsdBindingContracts>)
    endif()
endif()
