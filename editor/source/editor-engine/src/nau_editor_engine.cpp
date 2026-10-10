// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_editor_engine.hpp"
#include "nau/shared/file_system.h"

#include "nau/nau_editor_delegates.hpp"
#include "nau/editor-engine/nau_editor_engine_services.hpp"

#include <QApplication>
#include <QStandardPaths>

// Engine
#include "nau/app/application.h"
#include "nau/app/application_services.h"
#include "nau/app/application_init_delegate.h"
#include "nau/app/global_properties.h"
#include "nau/app/platform_window.h"
#include "nau/graphics/core_graphics.h"
#include "nau/app/window_manager.h"
#include "nau/module/module_manager.h"
#include "nau/service/service_provider.h"
#include "nau/threading/event.h"
#include "nau/platform/windows/app/windows_window.h"
#include "nau/io/virtual_file_system.h"
#include <filesystem>

#include "nau/scene/components/camera_component.h"
#include "nau/scene/scene.h"
#include "nau/scene/scene_factory.h"
#include "nau/scene/scene_manager.h"
#include "nau/assets/asset_manager.h"
#include "nau/debugRenderer/debug_render_system.h"
#include "nau/physics/core_physics.h"
#include "nau/physics/physics_world.h"
#include "nau/physics/nau_phyisics_collision_channel_model.hpp"

#include "engine/nau_editor_window_manager.h"
#include "engine/virtual_file_system_utils.hpp"
#include "viewport/nau_game_viewport_controller.hpp"
#include "nau/viewport/nau_outline_manager_interface.hpp"
#include "viewport/nau_viewport_manager_impl.hpp"

#include <nau/diag/logging.h>

#include "nau/service/service_provider.h"
#include "nau/assets/asset_db.h"
#include "nau/asset_tools/asset_manager.h"

#include <thread>

#include <QFileInfo>

#if !defined(NAU_MODULES_LIST)
#define NAU_MODULES_LIST "CoreAssetFormats,CoreScene,CoreAssets,Graphics,GraphicsAssets,DebugRenderer,CoreInput,PlatformApp,Audio,Animation,Physics,PhysicsJolt,ui,VFX"
#endif


constexpr int FPS_LIMIT = 60.0f;
constexpr int MS_PER_FRAME = (int)((1.0f / FPS_LIMIT) * 1000.0f);


namespace
{
    class EditorAppInitDelegate final : public nau::ApplicationInitDelegate
    {
    public:
        template<typename F>
        EditorAppInitDelegate(F initFunc)
            : m_initFunc(std::move(initFunc))
        {

        }

        static nau::Result<> loadAdditionalModules()
        {
            using namespace nau;

            auto& globalProperties = getServiceProvider().get<GlobalProperties>();

            const auto modulesConfig = globalProperties.getValue<EditorModulesConfig>("editor/modules");
            if (!modulesConfig) {
                return ResultSuccess;
            }

            for (const auto& moduleName : modulesConfig->additionalModules) {
                NauCheckResult(loadModulesList(moduleName));
            }

            return ResultSuccess;
        }

    private:
        struct EditorModulesConfig
        {
            eastl::vector<eastl::string> additionalModules;

            NAU_CLASS_FIELDS(
                CLASS_FIELD(additionalModules)
            )
        };

        nau::Result<> configureApplication() override
        {
            using namespace nau;
            namespace fs = std::filesystem;

            const std::array nauRootDirs = {

                // [USER_HOME]/Nau
                fs::path(QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toStdWString()) / "Nau",

                // [USER_HOME]/AppData/Local/Nau/Editor
                fs::path(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdWString())
            };

            auto& globalProperties = getServiceProvider().get<GlobalProperties>();

            for (const fs::path& dir : nauRootDirs) {
                const fs::path userConfigDir = dir / "Config";
                if (!fs::exists(userConfigDir) || !fs::is_directory(userConfigDir)) {
                    continue;
                }
                for (auto entry : fs::directory_iterator{ userConfigDir }) {
                    if (!entry.is_regular_file()) {
                        continue;
                    }

                    if (strings::icaseEqual(entry.path().extension().wstring(), L".json")) {
                        if (const auto mergeResult = mergePropertiesFromFile(globalProperties, entry.path()); !mergeResult) {
                            NAU_LOG_WARNING("Fail to read configuration file:({}),({})", entry.path().string(), mergeResult.getError()->getMessage());
                        }
                    }

                }
            }

            return nau::ResultSuccess;
        }

        nau::Result<> initializeApplication() override
        {
            NAU_FATAL(m_initFunc);
            return m_initFunc();
        }

    private:
        QStringList m_projectModules;
        nau::Functor<nau::Result<>()> m_initFunc;

    };
}

// ** NauEditorEngineSnapshot

void NauEditorEngineSnapshot::takeShapshot(NauEditorEngine* editorEngine)
{
    if (m_snapshotInfo) {
        NAU_FAILURE("Snapshot info already exist");
        return;
    }

    m_sourceEngine = editorEngine;
    m_snapshotInfo = std::make_unique<NauEditorEngineSnapshot::Info>();

    auto cameraControl = m_sourceEngine->cameraManager()->activeCamera();
    m_snapshotInfo->cameraTransform = cameraControl->getWorldTransform();
    m_snapshotInfo->viewportController = m_sourceEngine->viewportManager()->mainViewport()->controller();
}

void NauEditorEngineSnapshot::restoreShapshot()
{
    if (!m_snapshotInfo) {
        NAU_FAILURE("Snapshot info is empty");
        return;
    }

    auto cameraControl = m_sourceEngine->cameraManager()->activeCamera();
    cameraControl->setWorldTransform(m_snapshotInfo->cameraTransform);

    auto mainViewport = m_sourceEngine->viewportManager()->mainViewport();
    mainViewport->changeViewportController(m_snapshotInfo->viewportController);

    // Disable UI render in main viewport
    auto* coreGraphics = nau::getServiceProvider().find<nau::ICoreGraphics>();
    m_sourceEngine->runTaskSync(coreGraphics->getDefaultRenderWindow().acquire()->disableRenderStages(nau::render::NauRenderStage::NauGUIStage).detach());
}


// ** NauEditorEngine

NauEditorEngine::NauEditorEngine()
    : m_cameraManager(std::make_shared<NauEditorCameraManager>())
    , m_editMode(NauEditorMode::Editor)
{

}

NauEditorEngine::~NauEditorEngine()
{

}

bool NauEditorEngine::initialize(const std::string& rootDir, const std::string& userDllDir, const QStringList& projectModules)
{   
    namespace fs = std::filesystem;
    using namespace nau;
    using namespace std::chrono_literals;


    EditorAppInitDelegate appInitDelegate([&]() -> Result<>
        {
            nau::loadModulesList(NAU_MODULES_LIST).ignore();

            const QString projectModulesList = projectModules.join(",");
            if (!projectModulesList.isEmpty()) {

                const fs::path userDllDirPath = fs::u8path(userDllDir);
                if (fs::exists(userDllDirPath)) {
                    for (auto& module : projectModules) {
                        const fs::path dllPath = (userDllDirPath / fs::u8path(module.toStdString())).replace_extension(".dll");
                        if (fs::exists(dllPath)) {
                            NauCheckResult(getModuleManager().loadModule(module.toStdString().c_str(), nau::pathToUtf8(dllPath)));
                        }
                        else {
                            NAU_LOG_WARNING("User dll file not found:({})", nau::pathToUtf8(dllPath));
                        }
                    }
                }
                else {
                    NAU_LOG_WARNING("User dll's directory does not exists:({})", nau::pathToUtf8(userDllDirPath));
                }
            }

            NauCheckResult(EditorAppInitDelegate::loadAdditionalModules());

        // create window
        // Create viewportManager
        m_viewportManager = std::make_shared<NauViewportManager>();

            // Setup vfs
            editor::vfsUtils::configureVirtualFileSystem(getServiceProvider().get<io::IVirtualFileSystem>(), rootDir);

            return nau::ResultSuccess;
        });


    m_engineApplication = createApplication(appInitDelegate);

    // Create asset db
    auto args = std::make_unique<nau::ImportAssetsArguments>();
    args->projectPath = rootDir;

    if (std::filesystem::exists(std::filesystem::u8path(rootDir) / "assets_database/database.db"))
    {
        nau::getServiceProvider().get<nau::IAssetDB>().addAssetDB("assets_database/database.db");
    }

    int result = 1;
    for (int i = 0; i < 4 && result != 0; ++i)
    {
        result = nau::importAssets(args.get());
    }
    nau::getServiceProvider().get<nau::IAssetDB>().reloadAssetDB("assets_database/database.db");

    // Start editor engine work-cycle
    startEngineLoop(rootDir);

    // Disable world simulation
    auto& sceneManager = nau::getServiceProvider().get<nau::scene::ISceneManager>();
    sceneManager.getDefaultWorld().setSimulationPause(true);

    // Disable UI render in main viewport
    auto* coreGraphics = nau::getServiceProvider().find<nau::ICoreGraphics>();
    runTaskSync(coreGraphics->getDefaultRenderWindow().acquire()->disableRenderStages(nau::render::NauRenderStage::NauGUIStage).detach());

    return true;
}

bool NauEditorEngine::terminateSync()
{
    terminateEditorSystems();

    stopEngine();

    using namespace std::chrono_literals;

    // wait until the editor engine workcycle ends
    while (m_editorTickTimer.isActive()) {
        // continue workcycle
        qApp->processEvents();
    }

    return true;
}

void NauEditorEngine::tick()
{
    using namespace nau;

    m_editorDrawTask = async::Task<>::makeResolved();

    if (!m_engineApplication->step()) {
        stopEngineLoop();
        return;
    }

    if (m_engineApplication->isClosing()) {
        return;
    }

    m_viewportManager->mainViewport()->onFrame();

    auto drawAllEditorStuffInRenderThread = []() -> nau::async::Task<> {
        ASYNC_SWITCH_EXECUTOR(getServiceProvider().get<nau::ICoreGraphics>().getPreRenderExecutor());

        auto drawAllEditorStuffInMainThread = []() -> nau::async::Task<> {
            ASYNC_SWITCH_EXECUTOR(getApplication().getExecutor());
            //We need all of this so we could "lock" Render and Main thread at the same time, 
            //and than quikly do all required stuff for DebugRenderer.
            //This way we avoid deadlock from courutines, and creating a "mutex" for DebugRenderer and NauEditor.
            //TODO: remove all of this and create EditorRenderer (or some another solution).

            nau::getDebugRenderer().clear();

            NauEditorEngineDelegates::onRenderDebug.broadcast();

            if (physics::ICorePhysics* corePhysics = getServiceProvider().find<physics::ICorePhysics>()) {
                auto physWorld = corePhysics->getDefaultPhysicsWorld();
                if (physWorld) {
                    physWorld->drawDebug(getDebugRenderer());
                }
            }
        };
        co_await drawAllEditorStuffInMainThread();
    };

    if (m_editorDrawTask.isReady()) {
        //If everything was drawn, start again
        m_editorDrawTask = drawAllEditorStuffInRenderThread();
    }
}

std::shared_ptr<NauViewportManagerInterface> NauEditorEngine::viewportManager() const
{
    NAU_ASSERT(m_viewportManager);
    return m_viewportManager;
}

std::shared_ptr<NauEditorCameraManager> NauEditorEngine::cameraManager() const
{
    return m_cameraManager;
}

void NauEditorEngine::startPlay()
{
    if (m_editMode == NauEditorMode::Play) {
        NAU_LOG_WARNING("Play mode already started");
        return;
    }

    auto& sceneManager = nau::getServiceProvider().get<nau::scene::ISceneManager>();
    sceneManager.getDefaultWorld().setSimulationPause(false);

    // Make engine data snapshot
    if (m_engineSnapshot) {
        NAU_LOG_ERROR("The previous engine snapshot was not cleared");
    }
    m_engineSnapshot = std::make_unique<NauEditorEngineSnapshot>();
    m_engineSnapshot->takeShapshot(this);

    // Change active camera position
    auto activeCamera = m_cameraManager->activeCamera();
    // TODO: Get camera transform from spawn point
    activeCamera->setTransform(nau::math::Transform::identity());

    // Set game viewport controller
    auto mainViewport = viewportManager()->mainViewport();
    mainViewport->changeViewportController(std::make_shared<NauGameViewportController>(mainViewport));

    //Resize on start
    const float dpi = mainViewport->devicePixelRatioF();
    const int newWidth = mainViewport->width() * dpi;
    const int newHeight = mainViewport->height() * dpi;
    mainViewport->controller()->updateEngineRenderer(newWidth, newHeight);

    nau::getServiceProvider().get<NauOutlineManagerInterface>().enableOutline(false);

    auto* coreGraphics = nau::getServiceProvider().find<nau::ICoreGraphics>();
    runTaskSync(coreGraphics->getDefaultRenderWindow().acquire()->enableRenderStages(nau::render::NauRenderStage::NauGUIStage).detach());

    // Change current editor mode
    m_editMode = NauEditorMode::Play;
}

void NauEditorEngine::stopPlay()
{
    if (m_editMode != NauEditorMode::Play) {
        NAU_LOG_WARNING("Cannot stop. Play mode is not running");
        return;
    }

    // Make engine data snapshot
    if (m_engineSnapshot) {
        m_engineSnapshot->restoreShapshot();
        m_engineSnapshot.reset();
    }
    else {
        NAU_LOG_WARNING("Nothing to restore in editor engine. Snapshot is empty");
    }

    auto& sceneManager = nau::getServiceProvider().get<nau::scene::ISceneManager>();
    sceneManager.getDefaultWorld().setSimulationPause(true);

    nau::getServiceProvider().get<NauOutlineManagerInterface>().enableOutline(true);

    // Change current editor mode
    m_editMode = NauEditorMode::Editor;
}

void NauEditorEngine::pausePlay(const bool isPaused)
{
    if (m_editMode != NauEditorMode::Play) {
        NAU_LOG_WARNING("Cannot pause. Play mode is not running");
        return;
    }

    auto& sceneManager = nau::getServiceProvider().get<nau::scene::ISceneManager>();
    sceneManager.getDefaultWorld().setSimulationPause(isPaused);

    nau::getServiceProvider().get<NauOutlineManagerInterface>().enableOutline(isPaused);
}

// Perhaps the workcycle needs to be removed from the editor engine
void NauEditorEngine::startEngineLoop(const std::string& rootDir)
{
    NAU_ASSERT(m_engineApplication);

    m_engineApplication->startupOnCurrentThread();

    Nau::getPhysicsCollisionChannelModel().initialize(rootDir);
    Nau::getPhysicsCollisionChannelModel().applySettingsToPhysicsWorld();

    auto tickLambda = [this]() {
        this->tick();
        };
    m_editorTickTimer.connect(&m_editorTickTimer, &QTimer::timeout, tickLambda);
    m_editorTickTimer.start(MS_PER_FRAME);
}

void NauEditorEngine::stopEngineLoop()
{
    NAU_ASSERT(m_engineApplication);

    m_editorTickTimer.stop();

}

void NauEditorEngine::stopEngine()
{
    NAU_ASSERT(m_engineApplication);

    m_engineApplication->stop();
}

void NauEditorEngine::terminateEditorSystems()
{
    // Disable events that are processed by engine systems
    NauEditorEngineDelegates::onRenderDebug.disableBroadcast();
}


// ** Editor engine service functions

namespace Nau
{
    namespace
    {
        // Private NauEditorEngineInterface instance. Can be obtained as reference via function EditorEngine
        std::unique_ptr<NauEditorEngineInterface>& CurrentEditorEngineInstance()
        {
            static std::unique_ptr<NauEditorEngineInterface> m_editorEngine;
            return m_editorEngine;
        }
    }

    // NauEditorEngine instance creation function
    std::unique_ptr<NauEditorEngineInterface> CreateEditorEngine()
    {
        auto editor = std::make_unique<NauEditorEngine>();

        return editor;
    }

    // Setting current editorEngine instance. Cannot be setted twice. Can be nullptr
    // This is the main instance that is used throughout the editor
    void SetCurrentEditorEngine(std::unique_ptr<NauEditorEngineInterface>&& editorEngine)
    {
        NAU_FATAL(!editorEngine || !CurrentEditorEngineInstance(), "Editor engine already set");
        CurrentEditorEngineInstance() = std::move(editorEngine);
    }

    // Getting a reference to the editorEngine current(main) instance
    NauEditorEngineInterface& EditorEngine()
    {
        auto& instance = CurrentEditorEngineInstance();
        NAU_FATAL(instance);
        return *instance;
    }
}
