find_package(vrmRig CONFIG REQUIRED)
add_library(avatarVrmAdapter STATIC expressionAdapter.cpp)
add_library(AvatarRuntime::avatarVrmAdapter ALIAS avatarVrmAdapter)
target_compile_features(avatarVrmAdapter PUBLIC cxx_std_17)
set_target_properties(avatarVrmAdapter PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(avatarVrmAdapter PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(avatarVrmAdapter PUBLIC AvatarRuntime::avatarRuntime vrmRig::vrmRig)
target_compile_definitions(avatarVrmAdapter PRIVATE AR_VRM_PROVIDER_VERSION="${vrmRig_VERSION}")
if(MSVC)
    target_compile_options(avatarVrmAdapter PRIVATE /W4 /permissive-)
endif()
install(TARGETS avatarVrmAdapter EXPORT AvatarVrmAdapterTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarVrmAdapterTargets NAMESPACE AvatarRuntime::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)

if(AVATAR_BUILD_TESTS)
    add_executable(vrmAdapterContracts tests.cpp)
    target_link_libraries(vrmAdapterContracts PRIVATE avatarVrmAdapter)
    add_test(NAME adapters.vrm_expression COMMAND vrmAdapterContracts)
    add_test(NAME adapters.vrm_installed COMMAND ${CMAKE_COMMAND}
        -DROOT_BINARY=${PROJECT_BINARY_DIR}
        -DCONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer
        -DGENERATOR=${CMAKE_GENERATOR}
        -DPLATFORM=${CMAKE_GENERATOR_PLATFORM}
        -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
        -DCXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DRC_COMPILER=${CMAKE_RC_COMPILER}
        -DMT=${CMAKE_MT}
        -DVRM_DIR=${vrmRig_DIR}
        -DMOTION_DIR=${motionCore_DIR}
        -DUSD_DIR=${pxr_DIR}
        -DCONFIG=$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/installed_consumer/run.cmake)
    if(WIN32)
        add_custom_command(TARGET vrmAdapterContracts POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:vrmAdapterContracts>)
    endif()
endif()
