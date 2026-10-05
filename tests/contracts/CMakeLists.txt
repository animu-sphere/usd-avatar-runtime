# Provider, C consumer and runtime are compiled into separate binary targets.
add_library(contractProvider SHARED provider.c)
target_include_directories(contractProvider PRIVATE ${PROJECT_SOURCE_DIR}/include)
set_target_properties(contractProvider PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)

add_executable(cAbiConsumer c_consumer.c)
target_include_directories(cAbiConsumer PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(cAbiConsumer PRIVATE avatarRuntime contractProvider)
set_target_properties(cAbiConsumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
add_test(NAME contracts.c_provider_consumer COMMAND cAbiConsumer)

add_executable(runtimeContracts runtime_contracts.cpp)
target_link_libraries(runtimeContracts PRIVATE avatarRuntime)
# This also checks public header compatibility with the C++20 `requires` keyword.
target_compile_features(runtimeContracts PRIVATE cxx_std_20)
add_test(NAME contracts.runtime COMMAND runtimeContracts)
add_test(NAME contracts.installed_consumer COMMAND ${CMAKE_COMMAND}
    -DROOT_BINARY=${PROJECT_BINARY_DIR}
    -DCONSUMER_SOURCE=${PROJECT_SOURCE_DIR}/tests/installed_consumer
    -DGENERATOR=${CMAKE_GENERATOR}
    -DPLATFORM=${CMAKE_GENERATOR_PLATFORM}
    -DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
    -DC_COMPILER=${CMAKE_C_COMPILER}
    -DRC_COMPILER=${CMAKE_RC_COMPILER}
    -DMT=${CMAKE_MT}
    -DCONFIG=$<CONFIG>
    -P ${PROJECT_SOURCE_DIR}/tests/installed_consumer/run.cmake)

if(WIN32)
    add_custom_command(TARGET cAbiConsumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:cAbiConsumer>)
    add_custom_command(TARGET runtimeContracts POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:avatarRuntime> $<TARGET_FILE_DIR:runtimeContracts>)
endif()
