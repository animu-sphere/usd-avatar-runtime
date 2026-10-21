find_package(physicsUsd 0.1 CONFIG REQUIRED)
add_library(avatarPhysicsUsdResources STATIC sceneResources.cpp)
add_library(AvatarRuntime::avatarPhysicsUsdResources ALIAS avatarPhysicsUsdResources)
target_compile_features(avatarPhysicsUsdResources PUBLIC cxx_std_17)
set_target_properties(avatarPhysicsUsdResources PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(avatarPhysicsUsdResources PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_link_libraries(avatarPhysicsUsdResources PUBLIC physicsUsd::physicsUsd)
if(MSVC)
    target_compile_options(avatarPhysicsUsdResources PRIVATE /W4 /permissive-)
endif()
install(TARGETS avatarPhysicsUsdResources EXPORT AvatarPhysicsUsdResourcesTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarPhysicsUsdResourcesTargets NAMESPACE AvatarRuntime::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)
if(AVATAR_BUILD_TESTS)
    add_executable(physicsUsdResourceContracts tests.cpp)
    target_link_libraries(physicsUsdResourceContracts PRIVATE AvatarRuntime::avatarPhysicsUsdResources)
    add_test(NAME adapters.physics_usd_resources COMMAND physicsUsdResourceContracts)
    add_test(NAME adapters.physics_usd_installed COMMAND ${CMAKE_COMMAND}
        "-DROOT_BINARY=${PROJECT_BINARY_DIR}" "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DDEPENDENCY_PREFIX=${CMAKE_PREFIX_PATH}" "-DGENERATOR=${CMAKE_GENERATOR}"
        "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}" "-DCONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/installed.cmake")
endif()
