cmake_minimum_required(VERSION 3.13)

project(NauEditorUsdTests)
enable_testing()

IF (NOT DEFINED NAU_EXTERNAL_DIR)
    set(NAU_EXTERNAL_DIR "${CMAKE_SOURCE_DIR}/editor/external")
ENDIF()


# Needed for making this project without NauEditor
IF (NOT Qt6_FOUND)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
ENDIF()

# GTest targets are normally added by NauEditorUnitTests
IF (NOT TARGET gtest)
    set(gtest_force_shared_crt on)
    add_subdirectory(${NAU_EXTERNAL_DIR}/googletest BUILD EXCLUDE_FROM_ALL)

    set_target_properties(gtest PROPERTIES FOLDER "external")
    set_target_properties(gtest_main PROPERTIES FOLDER "external")
    set_target_properties(gmock_main PROPERTIES FOLDER "external")
ENDIF()

set(Sources
    nau_main.cpp

    nau_usd_scene_snapshot.cpp
    nau_usd_undo_commands_tests.cpp
)

set(Headers
    nau_usd_scene_snapshot.hpp
    nau_usd_undo_commands_tests.hpp
    nau_gtest_printers.hpp
)

add_executable(NauEditorUsdTests ${Sources} ${Headers})

# Unlike NauEditorUnitTests, editor code is linked from the editor modules, not compiled in,
# so NAU_UNIT_TESTS and NAU_EDITOR_STATIC_RUNTIME are not defined: the editor log must stay real
# for the modules that write to it.
target_compile_features(NauEditorUsdTests PUBLIC cxx_std_20)

nau_add_compile_options(NauEditorUsdTests ENABLE_RTTI)

if(MSVC)
    target_compile_options(NauEditorUsdTests PRIVATE "-wd4595")

    # NauUsdAssetEditorCommon logs from static initializers (property widget registration),
    # which crashes before NauLog::init(). The editor loads it as a plugin after logging is up;
    # here it is delay-loaded so it is loaded on first use, after the test environment initialized the log.
    target_link_options(NauEditorUsdTests PRIVATE "/DELAYLOAD:$<TARGET_FILE_NAME:NauUsdAssetEditorCommon>")
    target_link_libraries(NauEditorUsdTests PRIVATE delayimp)
endif()

target_link_libraries(NauEditorUsdTests PRIVATE
    Qt6::Widgets
    NauEditorCore
    NauEditorEngine
    NauEditorEngineLog
    NauFramework

    NauUsdAssetEditorCommon
    UsdProxy
    usd
    usdGeom

    GTest::gtest
)

# Unlike NauEditorUnitTests, runs next to the editor binaries: the tests need the editor's DLLs, usd/ and plugins/ folders
set_target_properties(NauEditorUsdTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/NauEditor)
set_target_properties(NauEditorUsdTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/NauEditor)

include(GoogleTest)
gtest_discover_tests(NauEditorUsdTests)
