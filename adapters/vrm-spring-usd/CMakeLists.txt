find_package(vrmRig CONFIG REQUIRED)
find_package(vrmSchema 0.10 CONFIG REQUIRED)
include(${PROJECT_SOURCE_DIR}/cmake/AvatarVrmSpring.cmake)
avatar_require_vrm_spring()
add_library(avatarVrmSpringUsdBinding STATIC springBoneBinding.cpp)
add_library(AvatarRuntime::avatarVrmSpringUsdBinding ALIAS avatarVrmSpringUsdBinding)
target_compile_features(avatarVrmSpringUsdBinding PUBLIC cxx_std_17)
set_target_properties(avatarVrmSpringUsdBinding PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(avatarVrmSpringUsdBinding PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(avatarVrmSpringUsdBinding PUBLIC AvatarRuntime::avatarUsdBinding vrmRig::vrmRig vrmSchema::vrmSchema)
target_compile_definitions(avatarVrmSpringUsdBinding PRIVATE AR_VRM_SPRING_OWNER_VERSION="${vrmRig_VERSION}")
if(MSVC)
    target_compile_options(avatarVrmSpringUsdBinding PRIVATE /W4 /permissive-)
endif()
install(TARGETS avatarVrmSpringUsdBinding EXPORT AvatarVrmSpringUsdBindingTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarVrmSpringUsdBindingTargets NAMESPACE AvatarRuntime::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)
if(AVATAR_BUILD_TESTS)
    add_executable(vrmSpringUsdBindingContracts tests.cpp)
    target_link_libraries(vrmSpringUsdBindingContracts PRIVATE avatarVrmSpringUsdBinding)
    if(AVATAR_BUILD_VRM_SPRING_ADAPTER)
        target_link_libraries(vrmSpringUsdBindingContracts PRIVATE avatarVrmSpringAdapter)
        target_compile_definitions(vrmSpringUsdBindingContracts PRIVATE AR_TEST_SPRING_ADAPTER)
    endif()
    add_test(NAME adapters.vrm_spring_usd COMMAND vrmSpringUsdBindingContracts)
    add_test(NAME adapters.vrm_spring_usd_installed COMMAND ${CMAKE_COMMAND}
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
        -DMOTION_DIR=${motionCore_DIR}
        -DUSD_DIR=${pxr_DIR}
        -DSCHEMA_DIR=${vrmSchema_DIR}
        -DVRM_DIR=${vrmRig_DIR}
        -DSPRING_ADAPTER_TEST=${AVATAR_BUILD_VRM_SPRING_ADAPTER}
        -DCONFIG=$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/run.cmake)
    if(WIN32)
        add_custom_command(TARGET vrmSpringUsdBindingContracts POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:vrmSpringUsdBindingContracts>)
    endif()
endif()
