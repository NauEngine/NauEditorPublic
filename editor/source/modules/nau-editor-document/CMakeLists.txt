set(TargetName NauEditorDocument)

nau_collect_files(PublicHeaders
  DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include/nau"
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.hpp"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Headers" FILES ${PublicHeaders})

# The editor document interface, without pxr. Header-only for now.
# The USD backend that implements it lives in NauUsdAssetEditorCommon (src/document-usd).
add_library(${TargetName} INTERFACE ${PublicHeaders})

target_include_directories(${TargetName} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

target_link_libraries(${TargetName} INTERFACE
    NauKernel
    NauEditorEngine
)

set_target_properties (${TargetName} PROPERTIES FOLDER "${NauEditorFolder}/source/modules")
