cmake_minimum_required(VERSION 3.22)
project(UsdAvatarRuntime VERSION 0.1.0 LANGUAGES C CXX)

option(AVATAR_BUILD_TESTS "Build contract and direct execution tests" ${PROJECT_IS_TOP_LEVEL})
option(AVATAR_BUILD_VRM_ADAPTER "Build the optional vrmRig registration adapter" OFF)
option(AVATAR_BUILD_MOTION_ADAPTER "Build the optional motion sampling/retarget adapter" OFF)
option(AVATAR_BUILD_USD_BINDING "Build the optional USD skeleton binding adapter" OFF)
add_library(avatarRuntime SHARED libs/avatarRuntime/runtime.cpp)
add_library(AvatarRuntime::avatarRuntime ALIAS avatarRuntime)
target_compile_features(avatarRuntime PRIVATE cxx_std_17)
target_compile_definitions(avatarRuntime PRIVATE AR_BUILDING_LIBRARY)
target_include_directories(avatarRuntime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
set_target_properties(avatarRuntime PROPERTIES CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
if(MSVC)
    target_compile_options(avatarRuntime PRIVATE /W4 /permissive-)
else()
    target_compile_options(avatarRuntime PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS avatarRuntime EXPORT AvatarRuntimeTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT AvatarRuntimeTargets NAMESPACE AvatarRuntime::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)
configure_package_config_file(cmake/AvatarRuntimeConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/AvatarRuntimeConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AvatarRuntimeConfigVersion.cmake
    VERSION ${PROJECT_VERSION} COMPATIBILITY ExactVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AvatarRuntimeConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AvatarRuntimeConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AvatarRuntime)

if(AVATAR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/contracts)
endif()

if(AVATAR_BUILD_VRM_ADAPTER)
    add_subdirectory(adapters/vrm)
endif()

if(AVATAR_BUILD_MOTION_ADAPTER)
    add_subdirectory(adapters/motion)
endif()

if(AVATAR_BUILD_USD_BINDING)
    add_subdirectory(adapters/usd)
endif()
