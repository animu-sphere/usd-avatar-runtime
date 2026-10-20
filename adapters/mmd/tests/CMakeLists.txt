option(AVATAR_MMD_JOLT_SMOKE "Test MMD callbacks with installed Jolt and generated PMX/VMD" OFF)
add_executable(mmdAdapterContracts contracts.cpp)
target_link_libraries(mmdAdapterContracts PRIVATE avatarMmdAdapter)
add_test(NAME adapters.mmd COMMAND mmdAdapterContracts)
add_test(NAME adapters.mmd_installed COMMAND ${CMAKE_COMMAND}
    "-DROOT_BINARY=${PROJECT_BINARY_DIR}"
    "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DDEPENDENCY_PREFIX=${CMAKE_PREFIX_PATH}"
    "-DGENERATOR=${CMAKE_GENERATOR}" "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}" "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
    "-DCONFIG=$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/installed.cmake")
if(WIN32)
    add_custom_command(TARGET mmdAdapterContracts POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:mmdAdapterContracts>)
endif()
if(AVATAR_MMD_JOLT_SMOKE)
    find_package(physicsJolt 0.1 CONFIG REQUIRED)
    find_package(physicsUsd 0.1 CONFIG REQUIRED)
    if(NOT physicsJolt_BACKEND_AVAILABLE)
        message(FATAL_ERROR "MMD Jolt smoke requires an available installed backend")
    endif()
    set(AVATAR_MMD_PMX "" CACHE FILEPATH "Generated coupling-modes.pmx")
    set(AVATAR_MMD_VMD "" CACHE FILEPATH "Generated sample.vmd")
    if(NOT EXISTS "${AVATAR_MMD_PMX}" OR NOT EXISTS "${AVATAR_MMD_VMD}")
        message(FATAL_ERROR "Supply generated AVATAR_MMD_PMX and AVATAR_MMD_VMD for the opt-in smoke")
    endif()
    target_link_libraries(mmdAdapterContracts PRIVATE physicsJolt::physicsJolt physicsUsd::physicsUsd)
    target_compile_definitions(mmdAdapterContracts PRIVATE AVATAR_MMD_JOLT_SMOKE)
    add_test(NAME adapters.mmd_jolt COMMAND mmdAdapterContracts "${AVATAR_MMD_PMX}" "${AVATAR_MMD_VMD}")
endif()
