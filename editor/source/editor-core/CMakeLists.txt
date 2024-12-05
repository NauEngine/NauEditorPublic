OPTION(USE_NAU_ANALYTICS "Use NauAnalytics" OFF)

nau_collect_files(PublicHeaders
  DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include/nau"
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.hpp"
)

nau_collect_files(SourceFiles
  DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src"
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.hpp" "*.cpp"
)

set(InspectorSources
    src/inspector/nau_inspector.cpp
)

set(Sources

    nau_about.hpp nau_about.cpp
    nau_action.cpp nau_action.hpp
    nau_action_widget.hpp nau_action_widget.cpp
    nau_assert.hpp
    nau_asset_importer.hpp nau_asset_importer.cpp
    nau_color.hpp nau_color.cpp
    nau_concepts.hpp
    nau_content_creator.hpp nau_content_creator.cpp
    nau_debug.hpp nau_debug.cpp
    nau_dock_manager.hpp nau_dock_manager.cpp
    nau_dock_widget.hpp nau_dock_widget.cpp
    nau_editor_version.hpp nau_editor_version.cpp
    nau_entity_creation_panel.hpp nau_entity_creation_panel.cpp
    nau_feedback.hpp nau_feedback.cpp
    nau_flow_layout.hpp nau_flow_layout.cpp
    nau_font.cpp nau_font.hpp
    nau_font_metrics.cpp nau_font_metrics.hpp
    nau_log.hpp nau_log.cpp
    nau_logger.hpp nau_logger.cpp
    nau_main_menu.hpp nau_main_menu.cpp
    nau_main_toolbar.hpp nau_main_toolbar.cpp
    nau_palette.cpp nau_palette.hpp
    nau_plus_enum.hpp
    nau_resource_view_widget.hpp nau_resource_view_widget.cpp
    nau_settings.hpp nau_settings.cpp
    nau_shortcut_hub.cpp nau_shortcut_hub.hpp
    nau_shortcut_operation.hpp
    nau_text_edit.cpp nau_text_edit.hpp
    nau_title_bar.hpp nau_title_bar.cpp
    nau_tree_view_item_delegate.hpp nau_tree_view_item_delegate.cpp
    nau_types.hpp
    nau_utils.hpp nau_utils.cpp
    nau_view_attributes.hpp nau_view_attributes.cpp
)

set(ProjectBrowserSources

    browser/nau_file_operations.cpp browser/nau_file_operations.hpp
    browser/nau_path_navigation_widget.cpp browser/nau_path_navigation_widget.hpp
    browser/nau_project_browser.cpp browser/nau_project_browser.hpp
    browser/nau_project_browser_drag_context.cpp browser/nau_project_browser_drag_context.hpp
    browser/nau_project_browser_file_operations_menu.cpp browser/nau_project_browser_file_operations_menu.hpp
    browser/nau_project_browser_file_system_model.cpp browser/nau_project_browser_file_system_model.hpp
    browser/nau_project_browser_icon_provider.cpp browser/nau_project_browser_icon_provider.hpp
    browser/nau_project_browser_info_widget.cpp browser/nau_project_browser_info_widget.hpp
    browser/nau_project_browser_item_type.cpp browser/nau_project_browser_item_type.hpp
    browser/nau_project_browser_item_type_resolver.cpp browser/nau_project_browser_item_type_resolver.hpp
    browser/nau_project_browser_list_view.cpp browser/nau_project_browser_list_view.hpp
    browser/nau_project_browser_proxy_models.cpp browser/nau_project_browser_proxy_models.hpp
    browser/nau_project_browser_styled_delegate.cpp browser/nau_project_browser_styled_delegate.hpp
    browser/nau_project_browser_summary_widget.cpp browser/nau_project_browser_summary_widget.hpp
    browser/nau_project_browser_table_view.cpp browser/nau_project_browser_table_view.hpp
    browser/nau_project_browser_tree_view.cpp browser/nau_project_browser_tree_view.hpp
    browser/nau_project_browser_view_scale_widget.cpp browser/nau_project_browser_view_scale_widget.hpp
    browser/nau_sort_order.hpp
    browser/nau_sort_type.hpp
    browser/nau_sort_type_widget.cpp browser/nau_sort_type_widget.hpp
)

set(FileAccessorSources

    fileAccessor/nau_file_access.cpp fileAccessor/nau_file_access.hpp
    fileAccessor/nau_file_accessor.hpp
    fileAccessor/nau_visual_studio_code_accessor.cpp fileAccessor/nau_visual_studio_code_accessor.hpp
)

set(FilterSources

    filter/nau_filter_checkbox.cpp filter/nau_filter_checkbox.hpp
    filter/nau_filter_item_widget.cpp filter/nau_filter_item_widget.hpp
    filter/nau_filter_widget.cpp filter/nau_filter_widget.hpp
    filter/nau_search_widget.cpp filter/nau_search_widget.hpp
)

set(EntityInspectorSources

    inspector/nau_object_inspector.cpp inspector/nau_object_inspector.hpp
)

set(SceneSources
    
    scene/nau_scene_settings_panel.cpp scene/nau_scene_settings_panel.hpp
    scene/nau_world.cpp scene/nau_world.hpp
)

set(ViewportSources

    viewport/nau_viewport_container_widget.cpp viewport/nau_viewport_container_widget.hpp
)

set(ProjectSources

    project/nau_new_project_window.cpp project/nau_new_project_window.hpp
    project/nau_project.cpp project/nau_project.hpp
    project/nau_project_manager_window.cpp project/nau_project_manager_window.hpp
    project/nau_project_path.cpp project/nau_project_path.hpp
    project/nau_recent_project_view.cpp project/nau_recent_project_view.hpp
    project/nau_project_user_settings.cpp project/nau_project_user_settings.hpp
)


set(LogSources

    log/nau_log_model.hpp log/nau_log_model.cpp
    log/nau_log_proxy_model.hpp log/nau_log_proxy_model.cpp
    log/nau_log_toolbar.hpp log/nau_log_toolbar.cpp
    log/nau_log_widget.hpp log/nau_log_widget.cpp
    log/nau_log_status_bar.hpp log/nau_log_status_bar.cpp
    log/nau_log_tree_view_item_delegate.hpp log/nau_log_tree_view_item_delegate.cpp
    log/nau_logger_details_panel.hpp log/nau_logger_details_panel.cpp
    log/nau_log_constants.hpp log/nau_log_constants.cpp
    log/nau_log_source_model.hpp log/nau_log_source_model.cpp
    log/nau_log_source_model_items.hpp log/nau_log_source_model_items.cpp
)


set(ThemeSources

    themes/nau_abstract_theme.hpp
    themes/nau_default_theme.cpp themes/nau_default_theme.hpp
    themes/nau_theme.cpp themes/nau_theme.hpp
    themes/nau_widget_style.hpp
)

set(CommandsSources

    commands/nau_commands.hpp
    commands/nau_commands.cpp
    commands/nau_commands_dialog.hpp
    commands/nau_commands_dialog.cpp
)

set(BaseWidgetSources

    baseWidgets/nau_buttons.hpp
    baseWidgets/nau_buttons.cpp
    baseWidgets/nau_icon.hpp
    baseWidgets/nau_icon.cpp
    baseWidgets/nau_label.hpp
    baseWidgets/nau_label.cpp
    baseWidgets/nau_menu.hpp
    baseWidgets/nau_menu.cpp 
    baseWidgets/nau_slider_value.hpp
    baseWidgets/nau_slider_value.cpp
    baseWidgets/nau_spoiler.hpp
    baseWidgets/nau_spoiler.cpp
    baseWidgets/nau_static_text_label.hpp
    baseWidgets/nau_static_text_label.cpp 
    baseWidgets/nau_toolbar.hpp 
    baseWidgets/nau_toolbar.cpp
    baseWidgets/nau_widget.hpp
    baseWidgets/nau_widget.cpp
    baseWidgets/nau_widget_utility.hpp 
    baseWidgets/nau_widget_utility.cpp
    baseWidgets/nau_header_view.hpp 
    baseWidgets/nau_header_view.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Headers" FILES ${PublicHeaders})

source_group("Sources" FILES ${Sources})
source_group("Sources\\inspector" FILES ${EntityInspectorSources})
source_group("Sources\\browser" FILES ${ProjectBrowserSources})
source_group("Sources\\fileAccessor" FILES ${FileAccessorSources})
source_group("Sources\\filter" FILES ${FilterSources})
source_group("Sources\\inspector" FILES ${InspectorSources})
source_group("Sources\\project" FILES ${ProjectSources})
source_group("Sources\\scene" FILES ${SceneSources})
source_group("Sources\\viewport" FILES ${ViewportSources})
source_group("Sources\\themes" FILES ${ThemeSources})
source_group("Sources\\commands" FILES ${CommandsSources})
source_group("Sources\\baseWidgets" FILES ${BaseWidgetSources})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "Sources" FILES ${SourceFiles})

set(resource_files ${CMAKE_SOURCE_DIR}/resources/editor/nau_editor.qrc)
set(platform_specific_resources "")

qt6_add_resources(resource_cpp_sources ${resource_files})

add_library(
    NauEditorCore SHARED
    ${PublicHeaders}
    ${SourceFiles}
    ${EntityInspectorSources}
    ${FileAccessorSources}
    ${FilterSources}
    ${InspectorSources}
    ${ProjectBrowserSources}
    ${ProjectSources}
    ${SceneSources}
    ${Sources}
    ${ViewportSources}
    ${ThemeSources}
    ${CommandsSources}
    ${AnalyticsSources}
    ${BaseWidgetSources}
    ${LogSources}

    ${resource_cpp_sources}
    ${resource_files}

    ${platform_specific_resources}
)

target_include_directories(NauEditorCore 
 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
 PRIVATE
 "${CMAKE_CURRENT_LIST_DIR}"
 "${CMAKE_CURRENT_LIST_DIR}/browser"
 "${CMAKE_CURRENT_LIST_DIR}/project"
 "${CMAKE_CURRENT_LIST_DIR}/viewport"
 "${CMAKE_CURRENT_LIST_DIR}/baseWidgets"

 "${CMAKE_CURRENT_LIST_DIR}/analytics/source"

 "${CMAKE_SOURCE_DIR}/editor/external/libzip/lib"
 "${CMAKE_BINARY_DIR}/editor/external/libzip"
 "${CMAKE_SOURCE_DIR}/editor/external/libcurl/include/"

 "${CMAKE_SOURCE_DIR}/editor/external/ads/src"
 "${CMAKE_SOURCE_DIR}/editor/external/magic_enum/include"
)

target_link_directories(NauEditorCore PRIVATE
 "${PXR_LIB_DIR}"
)

target_compile_definitions(NauEditorCore PRIVATE
    NAU_EDITOR_BUILD_DLL
)
target_link_options(NauEditorCore
    PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/FORCE:MULTIPLE>
    PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/DELAYLOAD:WinPixEventRuntime.dll>
    # via USD
    PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/NODEFAULTLIB:${PXR_NODEFAULTLIBS}>

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
    target_compile_options(NauEditorCore PRIVATE "-wd4595" "$<$<CONFIG:Release>:/Zi>")
endif()



target_link_libraries(NauEditorCore PRIVATE Qt6::Widgets Qt6::Network Qt6::Svg Qt6::OpenGL advanceddocking openssl zlibstatic zip libcurl
    NauEditorEngine
    NauEditorEngineLog
    NauEditorPluginManager
    NauAnalytics
    PlatformApp
    NauKernel
    CoreScene
    CoreAssets
    ui
    usd
    usdUtils
    UsdMetaTools
    ProjectTool
    Shared
)

nau_add_compile_options(NauEditorCore ENABLE_RTTI)

set_target_properties(NauEditorCore PROPERTIES
    AUTOMOC ON
    COMPILE_WARNING_AS_ERROR OFF
)

IF (USE_NAU_ANALYTICS)
    target_compile_definitions(NauEditorCore PRIVATE USE_NAU_ANALYTICS)
ENDIF()

set_target_properties(NauEditorCore PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/NauEditor)
set_target_properties(NauEditorCore PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/NauEditor)

if (WIN32)
    set_target_properties(NauEditorCore PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${NAU_ROOT_DIR};${PXR_ENV_PATHS};%PATH%;${PXR_DEBUGER_ENV};${PXR_DEBUGER_PLUGINPATH}")
endif()

if (NOT COCOS2DX_ROOT_PATH)
  set(COCOS2DX_ROOT_PATH ${NAU_ROOT_DIR}/engine/core/modules/ui/cocos2d-x)
endif()
