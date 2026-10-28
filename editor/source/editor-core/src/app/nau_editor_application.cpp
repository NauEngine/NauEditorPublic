// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/app/nau_editor_application.hpp"
#include "nau_assert.hpp"
#include "nau_debug.hpp"
#include "nau_log.hpp"
#include "nau_settings.hpp"
#include "themes/nau_theme.hpp"
#include "project/nau_project_manager_window.hpp"

#include "nau/app/nau_editor_services.hpp"
#include "nau/app/nau_splash_screen.hpp"
#include "nau/compiler/nau_source_compiler.hpp"
#include "nau/editor-engine/nau_editor_engine_services.hpp"
#include "nau/nau_editor_plugin_manager.hpp"
#include "nau/scene/nau_scene_editor_interface.hpp"
#include "nau/service/internal/service_provider_initialization.h"
#include "src/services/nau_scene_editor_service.hpp"

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>

#include "nau/shared/logger.h"
#include "nau/shared/file_system.h"
#include "nau/diag/log_subscribers.h"

#include <QStandardPaths>
#include <QLoggingCategory>

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>


#if !defined(NAU_EDITOR_MODULES_LIST)
    #define NAU_EDITOR_MODULES_LIST "NauSceneEditor,NauGuiEditor,NauAnimationClipEditor,NauAssetProcessor,"\
                                    "NauAudioEditor,NauInputEditor,NauMaterialEditor,NauVFXEditor,NauPhysicsEditor"
#endif

#define NED_BUILD_INFO(...) NauLog::buildLogger().logMessage(NauEngineLogLevel::Info, __FUNCTION__, __FILE__, static_cast<unsigned>(__LINE__), __VA_ARGS__)
#define NED_BUILD_WARNING(...) NauLog::buildLogger().logMessage(NauEngineLogLevel::Warning, __FUNCTION__, __FILE__, static_cast<unsigned>(__LINE__), __VA_ARGS__)
#define NED_BUILD_ERROR(...) NauLog::buildLogger().logMessage(NauEngineLogLevel::Error, __FUNCTION__, __FILE__, static_cast<unsigned>(__LINE__), __VA_ARGS__)

namespace Nau
{
    static bool InitializeUsdPlugins()
    {
        // TODO: Temporary mechanism for loading USD plugins
        // In the future we need to think about its development
        const std::string applicationPath = QCoreApplication::applicationDirPath().toUtf8().constData();
        const std::string pluginInfoDirectoryPath = applicationPath + "/plugins/";
        const std::string pluginInfoFilePath = pluginInfoDirectoryPath + "plugInfo.json";

        if (!QFile::exists(pluginInfoFilePath.c_str())) {
            NED_WARNING("Plugin configuration file does not exist! Check the specified path: " + pluginInfoFilePath);
            return false;
        }

        auto usdPlugins = pxr::PlugRegistry::GetInstance().RegisterPlugins(pluginInfoFilePath);

        if (usdPlugins.empty()) {
            NED_WARNING("Not a single plugin has been downloaded! Please check the file: " + pluginInfoFilePath);
            return false;
        }

        bool allLoaded = true;
        for (auto usdPlugin : usdPlugins) {
            allLoaded &= usdPlugin->Load();
        }
        return allLoaded;
    }
}


// ** NauEditorApplication

NauEditorApplication::~NauEditorApplication()
{
    if (!m_initialized) return;

    // Shutdown editor services
    shutdownServices();
    Nau::SetDefaultEditorServiceProvider(nullptr);
    
    // Delete editor window
    Nau::SetDefaultEditor(nullptr);

    // Post shut down handling
    NauLog::close();

    // Shutdown engine
    Nau::EditorEngine().terminateSync();
}

bool NauEditorApplication::initialize(int argc, char* argv[])
{
    if (!initQtApp(argc, argv)) {
        return false;
    }

    // Setup logging
    nau::logger::init(nau::pathToUtf8(std::filesystem::current_path()), true);

    // Init editor service provider
    Nau::SetDefaultEditorServiceProvider(nau::createServiceProvider());

    // Load project
    NauProjectPtr project = loadProject();
    if (!project) {
        return false;   // User must've closed the window
    }

    if (!Nau::InitializeUsdPlugins()) {
        return false;
    }

    m_initialized = true;

    // Init Editor
    return initEditor(project);
}

int NauEditorApplication::execute()
{
    Nau::Editor().showMainWindow();
    return m_app->exec();
}

NauProjectPtr NauEditorApplication::loadProject()
{
    NauProjectPtr project(nullptr);
    NauProjectManagerWindow projectWindow(nullptr);
    projectWindow.connect(&projectWindow, &NauProjectManagerWindow::eventLoadProject, [&project](NauProjectPtr loadedProject) {
        project = loadedProject;
    });

    if ((projectWindow.showModal() == NauDialog::Rejected)) {
        return nullptr;
    }

    // Set user settings
    // Assuming nothing went wrong, update the recent project path
    NED_ASSERT(project->path().isValid());
    NauSettings::addRecentProjectPath(project->path());  

    return project;
}

bool NauEditorApplication::initQtApp(int argc, char* argv[])
{
    #ifdef QT_DEBUG
        qInstallMessageHandler(Nau::DebugTools::QtMessageHandler);
    #endif

    // Debug systems(engine's and editor's) must know current user local app data folder.
    // That dir depends on ApplicationName() and OrganizationName in NauApp. So we have to create NauApp at first.
    m_app = std::make_unique<NauApp>(argc, argv);

    // Needed to disable fromIccProfile warnings when creating previews for jpeg textures
    QLoggingCategory::setFilterRules("qt.gui.icc.warning=false\n");

    NauLog::init();
    Nau::Theme::current().applyGlobalStyle(*m_app);

    m_app->setLanguage(NauSettings::editorLanguage());
    return true;
}

bool NauEditorApplication::initEditor(NauProjectPtr project)
{
    NED_ASSERT(m_app);
    NED_ASSERT(project);

    // Must be equal to count of calling NauEditorSplashScreen::advance.
    static const int splashStepCount = 11;

    NauEditorSplashScreen splashScreen{project->displayName(), splashStepCount};
    splashScreen.show();
    splashScreen.advance(tr("Loading project..."));

    NED_BUILD_INFO("Loading project {}", project->displayName());
    if (m_app->ignoreProjectModules()) {
        NED_BUILD_WARNING("Ignoring project modules");
    }

    NauWinDllCompilerCpp compiler;
    std::vector<std::string> logStrings;
    bool cmakeInPath = compiler.checkCmakeInPath(*project, logStrings, [](const QString& msg) {});
    if (cmakeInPath) {
        for (auto& str : logStrings) {
            NED_BUILD_INFO(str);
        }
    } else {
        NED_BUILD_ERROR("No CMake in path!");
    }

    bool buildToolExists = compiler.checkBuildTool(*project, logStrings, [](const QString& msg) {});
    if (buildToolExists) {
        NED_BUILD_INFO("Build tool check success");
        for (auto& str : logStrings) {
            NED_BUILD_INFO(str);
        }
    } else {
        NED_BUILD_ERROR("Build tool check failed!");
    }

    if (buildToolExists && !m_app->ignoreProjectModules()) {
        NED_BUILD_INFO("Compiling...");
        splashScreen.advance(tr("Compiling project sources"));
        project->removeDlls();
        project->markSourcesDirty();

        NauWinDllCompilerCpp compiler;

        if (compiler.compileProjectSource(*project, logStrings, [&splashScreen](const QString& msg) {
                splashScreen.showMessage(msg);
            })) {
            NED_BUILD_INFO("Compile project source success");
            project->markSourcesClean();
        } else {
            NED_BUILD_ERROR("Compile project source failed!");
            for (auto& str : logStrings) {
                NED_BUILD_ERROR(str);
            }
        }
    } else {
        splashScreen.advance(tr("Skip project source compilation"));
    }

    // Report user modules status
    NED_BUILD_INFO("Project modules:");
    std::vector<NauProject::DLLInfo> dllInfos;
    project->getDLLInfo(dllInfos);
    for (auto& dllInfo : dllInfos) {
        if (dllInfo.m_exists) {
            NED_BUILD_INFO("Module {}", dllInfo.m_module);
            NED_BUILD_INFO("  DLL: {}", dllInfo.m_dllPath);
            NED_BUILD_INFO(" TIME: {}", dllInfo.m_dllDateTime.toString());
            NED_BUILD_INFO(" SIZE: {}", dllInfo.m_dllSize);
        } else {
            NED_BUILD_ERROR("Module {}", dllInfo.m_module);
            NED_BUILD_ERROR("Missing DLL: {}", dllInfo.m_dllPath);
        }
    }

    // Init engine
    splashScreen.advance(tr("Engine framework initialization..."));

    const QString rootDir = project->path().root().absolutePath();
    const QStringList projectModules = m_app->ignoreProjectModules() || !project->isSourcesCompiled() ? QStringList() : project->modules();
    if (projectModules.empty()) {
        NED_BUILD_INFO("No user defined project modules will be loaded.");
    } else {
        NED_BUILD_INFO("Project modules that will be loaded:{}", projectModules.join(","));
    }
    
    // Init engine with editor wrapper
    splashScreen.advance(tr("Engine warming up..."));
    Nau::SetCurrentEditorEngine(Nau::CreateEditorEngine());
    if (!Nau::EditorEngine().initialize(rootDir.toUtf8().constData(), project->dllPath().toStdString(), projectModules)) {
        NED_BUILD_ERROR("EditorEngine initialize failed!");
        return false;
    }
    NED_BUILD_INFO("EditorEngine initialize success");

    splashScreen.advance(tr("Initialize logging subsystem..."));
    NauLog::initEngineLog();
    NauLog::editorLogModel().setLevelIcons(Nau::Theme::current().iconLogs());

    // Initialize editor
    splashScreen.advance(tr("Editor initialization..."));
    Nau::SetDefaultEditor(Nau::CreateEditor(project));

    // Load editor modules
    splashScreen.advance(tr("Load editor modules..."));
    Nau::EditorModules::LoadEditorModules(NAU_EDITOR_MODULES_LIST);

    // Startup services
    splashScreen.advance(tr("Start up services..."));
    startupServices();

    // Post init proccess. Scene & asset editors modules initialization
    splashScreen.advance(tr("Post initialize process services..."));
    Nau::Editor().postInit();

    // Load recent scene
    splashScreen.advance(tr("Loading scene <b>%1</b>").arg(project->scenes().currentSceneName()));
    Nau::Editor().switchScene(project->scenes().mainScene());

    // Close splash screen
    splashScreen.close();
    return true;
}


bool NauEditorApplication::startupServices()
{
    // Add services
    Nau::EditorServiceProvider().addService<NauSceneEditorService>();

    // Init services
    auto& serviceProviderInit = Nau::EditorServiceProvider().as<nau::core_detail::IServiceProviderInitialization&>();

    // TODO: check preInit result
    Nau::EditorEngine().runTaskSync(serviceProviderInit.preInitServices());
    // TODO: check init result
    Nau::EditorEngine().runTaskSync(serviceProviderInit.initServices());

    return true;
}

bool NauEditorApplication::shutdownServices()
{
    auto& serviceProviderShutdown = Nau::EditorServiceProvider().as<nau::core_detail::IServiceProviderInitialization&>();
    Nau::EditorEngine().runTaskSync(serviceProviderShutdown.shutdownServices());

    return true;
}