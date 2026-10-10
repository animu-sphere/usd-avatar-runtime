add_executable(avatarMotionCheck main.cpp)
if(AVATAR_MOTION_CHECK_TOON)
    find_package(Toon CONFIG REQUIRED COMPONENTS AvatarState)
    # The optional canonical RGB handoff is additive to the owner's package.
    # Package version alone does not prove this installed header supports it.
    function(avatar_require_toon_rgb)
        include(CheckCXXSourceCompiles)
        set(CMAKE_CXX_STANDARD 20)
        set(CMAKE_REQUIRED_LIBRARIES Toon::AvatarState)
        set(CMAKE_REQUIRED_INCLUDES ${PROJECT_SOURCE_DIR}/include)
        # The installed header pins the runtime ABI revision it was built for.
        unset(AR_TOON_RUNTIME_ABI CACHE)
        check_cxx_source_compiles([[
            #include <toon/fast/avatar_state.hpp>
            int main() { return 0; }
        ]] AR_TOON_RUNTIME_ABI)
        if(NOT AR_TOON_RUNTIME_ABI)
            message(FATAL_ERROR "Motion check Toon parity requires installed Toon AvatarState built for this runtime's ABI revision. Rebuild/install the renderer adapter against these headers.")
        endif()
        unset(AR_TOON_RGB_BINDING CACHE)
        check_cxx_source_compiles([[
            #include <toon/fast/avatar_state.hpp>
            int main() { return int(Toon::AvatarMaterialField::BaseColorRgb); }
        ]] AR_TOON_RGB_BINDING)
        if(NOT AR_TOON_RGB_BINDING)
            message(FATAL_ERROR "Motion check Toon parity requires installed Toon AvatarState with BaseColorRgb support. Rebuild/install the renderer adapter; older same-version packages are insufficient.")
        endif()
    endfunction()
    avatar_require_toon_rgb()
    target_compile_definitions(avatarMotionCheck PRIVATE AR_CHECK_TOON)
    target_link_libraries(avatarMotionCheck PRIVATE Toon::AvatarState)
endif()
target_link_libraries(avatarMotionCheck PRIVATE avatarMotionUsdBinding avatarVrmUsdBinding avatarMotionAdapter)
if(AVATAR_BUILD_VRM_ADAPTER)
    target_compile_definitions(avatarMotionCheck PRIVATE AR_CHECK_VRM)
    target_link_libraries(avatarMotionCheck PRIVATE avatarVrmAdapter avatarVrmLookAtUsdBinding avatarVrmExpressionUsdBinding)
endif()
if(AVATAR_BUILD_TESTS AND AVATAR_BUILD_VRM_ADAPTER)
    add_executable(avatarMotionCheckContracts main.cpp tests.cpp)
    target_compile_definitions(avatarMotionCheckContracts PRIVATE AR_CHECK_VRM AR_CHECK_TEST_ENTRY)
    target_link_libraries(avatarMotionCheckContracts PRIVATE avatarMotionUsdBinding avatarMotionAdapter
        avatarVrmLookAtUsdBinding avatarVrmExpressionUsdBinding)
    add_test(NAME tools.motion_vrm_check COMMAND avatarMotionCheckContracts ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
    if(AVATAR_MOTION_CHECK_TOON)
        target_compile_definitions(avatarMotionCheckContracts PRIVATE AR_CHECK_TOON)
        target_link_libraries(avatarMotionCheckContracts PRIVATE Toon::AvatarState)
    endif()
endif()
if(WIN32)
    add_custom_command(TARGET avatarMotionCheck POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:avatarMotionCheck>)
    if(TARGET avatarMotionCheckContracts)
        add_custom_command(TARGET avatarMotionCheckContracts POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:avatarMotionCheckContracts>)
    endif()
endif()
install(TARGETS avatarMotionCheck RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
