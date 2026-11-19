set(TargetName NauUsdAssetEditorCommon)

nau_collect_files(Sources
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/src
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.cpp" "*.hpp"
)

nau_collect_files(PublicHeaders
  DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include/nau"
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.hpp"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Sources" FILES ${Sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Headers" FILES ${PublicHeaders})

add_library(
    ${TargetName} SHARED
    ${Sources}
    ${PublicHeaders}
)

target_include_directories(${TargetName} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    PRIVATE
    "${CMAKE_SOURCE_DIR}/editor/external/magic_enum/include"
)

target_compile_definitions(${TargetName} PRIVATE
    NAU_USD_ASSET_EDITOR_COMMON_BUILD_DLL
)

target_link_options(${TargetName}
    PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/FORCE:MULTIPLE>

    # Note that since MSVC 2019, /DEBUG:FULL linking times have improved significantly,
    # /DEBUG:FASTLINK isn't always faster than /DEBUG:FULL.
    # We'd rather have more clear debug picture than a small chance of faster building.
    PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/DEBUG:FULL>

    # OPT:REF can reduce the size of executable, for it eliminates functions and data that are never referenced.
    PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/OPT:REF>

    # OPT:NOICF  makes it easier to read stack traces and set breakpoints in functions.
    PRIVATE $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/OPT:ICF>
)

if(MSVC)
    target_compile_options(${TargetName} PRIVATE "-wd4595" "$<$<CONFIG:Release>:/Zi>")
endif()

set_target_properties(${TargetName} PROPERTIES AUTOMOC TRUE)

target_link_libraries(${TargetName} PRIVATE
    Qt6::Widgets
    advanceddocking
    NauEditorCore
    NauEditorEngine
    NauEditorEngineLog
    NauFramework

    NauAssetProcessor
    NauEditorPluginManager
    UsdMetaTools

    usd
    usdUtils
    usdGeom
    UsdProxy
    NauPhysicsSchema
)

# The USD backend of the editor document interface is in src/document-usd
target_link_libraries(${TargetName} PUBLIC
    NauEditorDocument
)

nau_add_compile_options(${TargetName} ENABLE_RTTI)

set_target_properties (${TargetName} PROPERTIES FOLDER "${NauEditorFolder}/source/modules")

set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/NauEditor)
set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/NauEditor)
