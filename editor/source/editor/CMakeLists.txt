set(TargetName NauEditor)

set(Sources
    nau_main.cpp
    nau_run_guard.hpp nau_run_guard.cpp
)

source_group("Sources" FILES ${Sources})

set(resource_files ${CMAKE_SOURCE_DIR}/resources/editor/nau_editor.qrc)
set(platform_specific_resources "")


if (WIN32)
    set(platform_specific_resources ${CMAKE_SOURCE_DIR}/resources/editor/win/nau_editor.rc)
endif()

qt6_add_resources(resource_cpp_sources ${resource_files})

add_executable(${TargetName} WIN32
    ${Sources}
    ${platform_specific_resources}
    
)

target_include_directories(${TargetName} PRIVATE
 "${CMAKE_CURRENT_SOURCE_DIR}"
 "${CMAKE_SOURCE_DIR}/editor/external/ads/src" 
 "${CMAKE_SOURCE_DIR}/editor/external/magic_enum/include"
)

target_link_directories(${TargetName} PRIVATE
 "${PXR_LIB_DIR}"
)

target_link_libraries(${TargetName} PRIVATE
    Qt6::Widgets Qt6::Network Qt6::Svg Qt6::OpenGL advanceddocking
    NauEditorCore
    NauEditorEngine
    NauEditorEngineLog
    NauFramework

    NauAssetMetadata
    NauUsdFormat
    NauComponentSchema
    NauAnimationClipAsset
    NauInputSchema
    NauAudioSchema
    NauGuiSchema
    NauPhysicsSchema
    NauVFXSchema
    UsdTranslatorWrapper
)

nau_add_compile_options(NauEditor ENABLE_RTTI)

target_link_options(${TargetName}
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

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NauEditor)

set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug/NauEditor)
set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release/NauEditor)
set(Qt6_deploy_libs Qt6OpenGL Qt6Network Qt6Widgets Qt6Core Qt6Gui Qt6Svg)

add_dependencies(NauEditor SceneBaseSample BuildToolCmd NauUsdFormat NauAssetMetadata NauAssetProcessor NauSceneEditor NauGuiEditor NauAudioEditor NauAnimationClipEditor NauInputEditor NauMaterialEditor NauVFXEditor NauPhysicsEditor)

if (WIN32)
    set_target_properties(${TargetName} PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=$<TARGET_FILE_DIR:NauKernel>;${PXR_ENV_PATHS};%PATH%;${PXR_DEBUGER_ENV};${PXR_DEBUGER_PLUGINPATH}")
    
    add_custom_command(TARGET NauEditor POST_BUILD

        #QtDeploy
        # This part should be reworked at using standart qt6_generate_deploy_app_script.
        # Today this looks like imposible becouse CMAKE_INSTALL_PREFIX expect absolute path in configuration time but have only buildtime generatated path
        # https://doc.qt.io/qt-6/qt-generate-deploy-app-script.html
        COMMAND Qt6::windeployqt $<TARGET_FILE:NauEditor> --$<$<CONFIG:DEBUG>:debug>$<$<CONFIG:RELEASE>:release> --no-system-d3d-compiler --translations en,ru
        COMMAND Qt6::windeployqt $<TARGET_FILE:NauEditorCore> --$<$<CONFIG:DEBUG>:debug>$<$<CONFIG:RELEASE>:release> --no-system-d3d-compiler --translations en,ru
        
        #copy shader cache from engine to project template
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NAU_ROOT_DIR}/samples/sceneBase/content/shaders/cache ${CMAKE_SOURCE_DIR}/projects/base/content/shaders/cache
        
        # Projects
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/projects/base $<TARGET_FILE_DIR:NauEditor>/base

        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/translations/editor_ru.qm $<TARGET_FILE_DIR:NauEditor>/translations/editor_ru.qm
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/translations/editor_en.qm $<TARGET_FILE_DIR:NauEditor>/translations/editor_en.qm
        
        #copy engine dll`s
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NAU_ROOT_DIR}/bin/$<CONFIG> $<TARGET_FILE_DIR:NauEditor>
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/bin/$<CONFIG> $<TARGET_FILE_DIR:NauEditor>

        #copy usd dll's
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PXR_LIB_DIR}/$<CONFIG> $<TARGET_FILE_DIR:NauEditor>
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PXR_LIB_DIR} $<TARGET_FILE_DIR:NauEditor>
        COMMAND ${CMAKE_COMMAND} -E copy ${PXR_CMAKE_DIR}/Python/python310.dll $<TARGET_FILE_DIR:NauEditor>
        COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:NauEditor>/release $<TARGET_FILE_DIR:NauEditor>/relwithdebuginfo $<TARGET_FILE_DIR:NauEditor>/debug 
        
        #copy usd plugin 
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PXR_CMAKE_DIR}/plugin/$<CONFIG>/usd $<TARGET_FILE_DIR:NauEditor>/plugins
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PXR_CMAKE_DIR}/plugin/usd $<TARGET_FILE_DIR:NauEditor>/plugins
        
        #copy projects templates
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NAU_ROOT_DIR}/project_templates $<TARGET_FILE_DIR:NauEditor>/project_templates

        #copy sdk
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NAU_ROOT_DIR}/include $<TARGET_FILE_DIR:NauEditor>/NauEngineSDK/include
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NAU_ROOT_DIR}/lib $<TARGET_FILE_DIR:NauEditor>/NauEngineSDK/lib
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NAU_ROOT_DIR}/bin $<TARGET_FILE_DIR:NauEditor>/NauEngineSDK/bin
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NAU_ROOT_DIR}/cmake $<TARGET_FILE_DIR:NauEditor>/NauEngineSDK/cmake
    )

    foreach(Qt6Lib ${Qt6_deploy_libs})
        add_custom_command(TARGET NauEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${Qt6_DIR}/../../../bin/${Qt6Lib}$<$<CONFIG:DEBUG>:d>.dll $<TARGET_FILE_DIR:NauEditor>
        )
    endforeach(Qt6Lib ${Qt6_deploy_libs})
endif()

if (WIN32)
    set(CONFIG_PLUGIN_PATH "$<$<CONFIG:DEBUG>:${CMAKE_BINARY_DIR}/Debug/NauEditor>$<$<NOT:$<CONFIG:DEBUG>>:${CMAKE_BINARY_DIR}/Release/NauEditor>")
    
    set_target_properties(${TargetName}
        PROPERTIES VS_DEBUGGER_ENVIRONMENT
        "PATH=%PATH%;${CONFIG_PLUGIN_PATH}/plugins;${PXR_ENV_PATHS};${PXR_DEBUGER_ENV};
        ${PXR_DEBUGER_PLUGINPATH}${CONFIG_PLUGIN_PATH}/plugins;")
endif()
