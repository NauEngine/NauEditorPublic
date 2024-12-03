project(NauEditorEngine)

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

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Sources" FILES ${Sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Headers" FILES ${PublicHeaders})

qt6_add_resources(resource_cpp_sources ${resource_files})

add_library(
    NauEditorEngine SHARED
    ${PublicHeaders}
    ${Sources}
    
    ${resource_cpp_sources}
    ${resource_files}
)

target_include_directories(NauEditorEngine
 PUBLIC
 $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)

target_precompile_headers(NauEditorEngine PRIVATE src/pch.h)

target_compile_definitions(NauEditorEngine PRIVATE
    NAU_EDITOR_ENGINE_BUILD_DLL
)
target_link_options(NauEditorEngine
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
    target_compile_options(NauEditorEngine PRIVATE "-wd4595" "$<$<CONFIG:Release>:/Zi>")
endif()


target_link_libraries(NauEditorEngine 

    PRIVATE 
    Qt6::Widgets
    NauFramework
    PlatformApp
    Graphics 
    CoreAssets
    CoreAssetFormats
    CoreInput
    GraphicsAssets
    DebugRenderer
    imgui
    Physics
    PhysicsJolt
    ui
    AssetTool
)

nau_add_compile_options(NauEditorEngine ENABLE_RTTI)

set_target_properties(NauEditorEngine PROPERTIES AUTOMOC TRUE)

set_target_properties(NauEditorEngine PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/NauEditor)
set_target_properties(NauEditorEngine PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/NauEditor)

#target_compile_definitions(ui  PRIVATE UI_ELEMENT_DEBUG)

if (TARGET NauKernel_Import AND TARGET NauFramework_Import)
  add_dependencies(${TargetName} NauKernel_Import NauFramework_Import)
endif()
