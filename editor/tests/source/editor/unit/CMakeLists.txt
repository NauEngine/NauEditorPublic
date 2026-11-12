cmake_minimum_required(VERSION 3.13)

project(NauEditorUnitTests)
enable_testing()

IF (NOT DEFINED NAU_EDITOR_SOURCE_DIR)
    set(NAU_EDITOR_SOURCE_DIR "${CMAKE_SOURCE_DIR}/editor/source/editor-core")
ENDIF()

IF (NOT DEFINED NAU_EXTERNAL_DIR)
    set(NAU_EXTERNAL_DIR "${CMAKE_SOURCE_DIR}/editor/external")
ENDIF()

IF (NOT DEFINED NAU_EDITOR_ENGINE_SOURCE_DIR)
    set(NAU_EDITOR_ENGINE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/editor/source/editor-engine")
ENDIF()

IF (NOT DEFINED NAU_EDITOR_UNIT_TEST_RESOURCE_DIR)
    set(NAU_EDITOR_UNIT_TEST_RESOURCE_DIR "${CMAKE_SOURCE_DIR}/editor/tests/source/editor/unit/resources/tests")
ENDIF()


# Needed for making this project without NauEditor
IF (NOT Qt6_FOUND)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
ENDIF()

IF (NOT GTest_FOUND)
    set(gtest_force_shared_crt on)
    add_subdirectory(${NAU_EXTERNAL_DIR}/googletest BUILD EXCLUDE_FROM_ALL)

    set_target_properties(gtest PROPERTIES FOLDER "external")
    set_target_properties(gtest_main PROPERTIES FOLDER "external")
    set_target_properties(gmock_main PROPERTIES FOLDER "external")
ENDIF()

set(Sources
    nau_main.cpp
    
    nau_commands_tests.cpp
    nau_file_operations_tests.cpp
    nau_project_browser_item_type_resolver_tests.cpp
    nau_settings_tests.cpp
    nau_editor_version_tests.cpp
    nau_test_demo.cpp
)

set(EditorSources
    ${NAU_EDITOR_SOURCE_DIR}/commands/nau_commands.cpp ${NAU_EDITOR_SOURCE_DIR}/commands/nau_commands.hpp
    ${NAU_EDITOR_SOURCE_DIR}/browser/nau_file_operations.cpp ${NAU_EDITOR_SOURCE_DIR}/browser/nau_file_operations.hpp
    ${NAU_EDITOR_SOURCE_DIR}/browser/nau_project_browser_item_type_resolver.cpp ${NAU_EDITOR_SOURCE_DIR}/browser/nau_project_browser_item_type_resolver.hpp
    ${NAU_EDITOR_SOURCE_DIR}/baseWidgets/nau_widget_utility.cpp ${NAU_EDITOR_SOURCE_DIR}/baseWidgets/nau_widget_utility.hpp
    ${NAU_EDITOR_SOURCE_DIR}/nau_settings.cpp ${NAU_EDITOR_SOURCE_DIR}/nau_settings.hpp
    ${NAU_EDITOR_SOURCE_DIR}/project/nau_project_path.cpp ${NAU_EDITOR_SOURCE_DIR}/project/nau_project_path.hpp
    ${NAU_EDITOR_SOURCE_DIR}/nau_editor_version.cpp ${NAU_EDITOR_SOURCE_DIR}/nau_editor_version.hpp
)

set(Headers
    nau_settings_tests.hpp
    nau_gtest_printers.hpp
)

set(ResourceFiles ${NAU_EDITOR_UNIT_TEST_RESOURCE_DIR}/nau_editor_unit_tests.qrc)

qt6_add_resources(Sources ${ResourceFiles})

add_executable(NauEditorUnitTests ${Sources} ${EditorSources} ${Headers} ${ResourceFiles})

target_include_directories(NauEditorUnitTests PRIVATE
 "${NAU_EDITOR_SOURCE_DIR}"
 "${NAU_EDITOR_SOURCE_DIR}/project"
 "${NAU_EDITOR_SOURCE_DIR}/baseWidgets"
 "${NAU_EDITOR_SOURCE_DIR}/include"
 "${NAU_EDITOR_ENGINE_SOURCE_DIR}/include"
 "${NAU_EXTERNAL_DIR}/magic_enum/include"
)

target_compile_definitions(NauEditorUnitTests PRIVATE
    NAU_UNIT_TESTS
    NAU_EDITOR_STATIC_RUNTIME
)

target_compile_features(NauEditorUnitTests PUBLIC cxx_std_20)

target_link_libraries(NauEditorUnitTests Qt6::Widgets  GTest::gtest_main GTest::gmock_main)

set_target_properties(NauEditorUnitTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/NauEditorUnitTests)
set_target_properties(NauEditorUnitTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/NauEditorUnitTests)

include(GoogleTest)
gtest_discover_tests(NauEditorUnitTests)