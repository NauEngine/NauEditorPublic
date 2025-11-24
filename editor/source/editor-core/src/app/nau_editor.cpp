// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_editor.hpp"

#include "nau/app/nau_editor_services.hpp"
#include "nau/editor-engine/nau_editor_engine_services.hpp"
#include "nau/assets/nau_asset_editor_accessor.hpp"
#include "nau/scene/nau_scene_editor_interface.hpp"
#include "nau/playmode/nau_play_commands.hpp"
#include "nau/compiler/nau_source_compiler.hpp"

#include "nau/nau_editor_plugin_manager.hpp"
#include "nau/editor-engine/nau_editor_engine_services.hpp"

#include "src/assets/nau_asset_manager_impl.hpp"
#include "src/thumbnail-manager/nau_thumbnail_manger_impl.hpp"
#include "src/app/nau_editor_window.hpp"
#include "src/builds/nau_build_startup_dialog.hpp"

#include "commands/nau_commands_dialog.hpp"
#include "fileAccessor/nau_file_access.hpp"
#include "project/nau_project_manager_window.hpp"
#include "nau_asset_importer.hpp"
#include "nau_content_creator.hpp"
#include "browser/nau_project_browser_item_type_resolver.hpp"
#include "nau_log.hpp"
#include "nau_settings.hpp"

#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>

#ifdef USE_NAU_ANALYTICS
#include "analytics/source/providers/nau_analytics_provider_countly.hpp"
#include "nau_utils.hpp"
#endif


// ** NauEditor

NauEditor::NauEditor(NauProjectPtr project)
    : m_mainWindow(nullptr)
#ifdef USE_NAU_ANALYTICS
    , m_analytics(new NauAnalytics())
#endif
{
    m_project = project;
    initMainWindow();

    // Add scene file accessor
    NauFileAccess::registerGenericAccessor<NauEditorFileType::Scene, NauSceneFileAccessor>(m_project);
    NauFileAccess::registerExternalAccessors();

#ifdef USE_NAU_ANALYTICS
    // Init analytics with providers
    initAnalytics();

    // Sending base events: editor info, system info
    sendEditorInfoEvent();
    sendSystemInfoEvent();
#endif

    // apply user settings to main window
    m_mainWindow->applyUserSettings(*project);
}

NauEditor::~NauEditor()
{
#ifdef USE_NAU_ANALYTICS
    m_analytics.reset();
#endif

    m_project.reset();
}

NauProjectPtr& NauEditorInterface::currentProject()
{
    return m_project;
}

const NauEditorWindowAbstract& NauEditor::mainWindow() const noexcept
{
    return *m_mainWindow;
}

NauAssetManagerInterfacePtr NauEditor::assetManager() const noexcept
{
    return m_assetManager;
}

NauThumbnailManagerInterfacePtr NauEditor::thumbnailManager() const noexcept
{
    return m_thumbnailManager;
}

NauUndoable* NauEditor::undoRedoSystem()
{
    return this;
}

void NauEditor::showMainWindow()
{
    if (!m_mainWindow) {
        NED_CRITICAL("Trying to show uninitialized main editor window!");
        return;
    }

    m_mainWindow->show();
}

void NauEditor::switchScene(const NauSceneManager::SceneInfo& scene)
{
    const QString path = NauSceneManager::sceneToPath(scene);
    NED_TRACE("Switching to scene at {}", path);

    unloadCurrentScene();

    const QString sceneFullPath = m_project->scenes().currentSceneAbsolutePath();
    loadScene(sceneFullPath.toUtf8().constData());
}

void NauEditor::loadScene(const std::string& path)
{   
    m_sceneManager->loadScene(path);
}

void NauEditor::unloadCurrentScene()
{
    m_mainWindow->outliner()->outlinerTab().clear();
    m_commands.clear();

    m_sceneManager->unloadCurrentScene();
}

void NauEditor::postInit()
{
    initModules();

    // Configure thumbnail manager
    configureThumbnailManager();

    // Configure project browser after asset manager initialzation
    configureProjectBrowser();
}

void NauEditor::registerAssetEditor(NauAssetEditorInterface* editor)
{
    if (editor == nullptr) {
        NED_ERROR("Trying to add non-existed editor.");
        return;
    }
    m_registeredEditors.push_back(editor);

    // Register asset accessor
    NauFileAccess::registerAssetAccessor(editor->assetType(), std::make_shared<NauAssetEditorAccessor>(editor));
}

void NauEditor::initMainWindow()
{
    m_mainWindow = std::make_unique<NauEditorWindow>();
    
    // subscribe to main window event
    m_mainWindow->connect(m_mainWindow.get(), &NauEditorWindowAbstract::eventCloseWindow, [this](QCloseEvent* event) {
        onEditorClosed(event);
    });

    m_mainWindow->setUpdateWindowTitleCallback([this]() {
        onUpdateTitlebar();
    });

    // Init other widgets
    initMainMenu();
    initToolbar();
    initProjectBrowser();
    initSceneManager();
}


#include <QPixmap>

void NauEditor::initMainMenu()
{
    NauMainMenu* mainMenu = m_mainWindow->titleBar()->menu().get();

    const auto checkBuildTools = [this]{
        NauWinDllCompilerCpp compiler;

        std::vector<std::string> logs;
        return compiler.checkBuildTool(*m_project, logs, [](const QString&){});
    };

    if (m_project->isSourcesCompiled()) {
        m_mainWindow->titleBar()->setCompilationState({NauSourceState::Success, {}, {}});
    } else if (!checkBuildTools()) {
        m_mainWindow->titleBar()->setCompilationState({NauSourceState::NoBuildTools, {}, {}});
    } else {
        m_mainWindow->titleBar()->setCompilationState({NauSourceState::CompilationError,
            m_project->compilationLogFileName(), {}});
    }

    // Save project 
    m_mainWindow->connect(mainMenu, &NauMainMenu::saveProjectRequested, [this] {
        if (NED_PASSTHROUGH_ASSERT(m_project)) {
            m_project->save();
#ifdef USE_NAU_ANALYTICS
            sendUIInteractionEvent("main_menu_save_project");
#endif
        }
    });

    // Show project contents
    m_mainWindow->connect(mainMenu, &NauMainMenu::eventReveal, [this] {
        if (NED_PASSTHROUGH_ASSERT(m_project)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_project->dir().absolutePath()));

#ifdef USE_NAU_ANALYTICS
            sendUIInteractionEvent("main_menu_show_project_contents");
#endif
        }
    });

    // New scene
    m_mainWindow->connect(mainMenu, &NauMainMenu::eventNewScene, [this] {
        if (NED_PASSTHROUGH_ASSERT(m_project)) {
            auto dialog = new NauNewSceneDialog(*m_mainWindow, m_project->scenes().defaultScenesDirectory());
            m_mainWindow->connect(dialog, &NauNewSceneDialog::eventCreateScene, &m_project->scenes(), &NauSceneManager::createScene);
            dialog->showModal();

#ifdef USE_NAU_ANALYTICS
            sendUIInteractionEvent("main_menu_new_scene");
#endif
        }
    });

    // Load scene
    m_mainWindow->connect(mainMenu, &NauMainMenu::eventLoadScene, [this] {
        if (NED_PASSTHROUGH_ASSERT(m_project)) {
            m_project->scenes().browseForScenes(*m_mainWindow, m_project->dir().absolutePath());

#ifdef USE_NAU_ANALYTICS
            sendUIInteractionEvent("main_menu_load_scene");
#endif
        }
    });

    // Recent scenes
    m_mainWindow->connect(mainMenu, &NauMainMenu::eventRecentScenes, [this](QMenu* menu) {
        if (NED_PASSTHROUGH_ASSERT(m_project)) {
            m_project->userSettings().fillRecentScenesMenu(*menu);

#ifdef USE_NAU_ANALYTICS
            sendUIInteractionEvent("main_menu_recent_scenes");
#endif
        }
    });

    // Save scene
    m_mainWindow->connect(mainMenu, &NauMainMenu::eventSaveScene, [this] {
        if (!m_sceneManager->isCurrentSceneSaved()) {
            m_sceneManager->saveCurrentScene();

#ifdef USE_NAU_ANALYTICS
            sendUIInteractionEvent("main_menu_save_scene");
#endif
        }
    });

    // Undo/redo
    m_mainWindow->connect(mainMenu, &NauMainMenu::eventUndo, [this]() { handleUndo(); });
    m_mainWindow->connect(mainMenu, &NauMainMenu::eventRedo, [this]() { handleRedo(); });

    m_mainWindow->connect(mainMenu, &NauMainMenu::eventSwitchLanguageRequested, [this](const QString& lang) {
        handleLanguageRequest(lang);
    });

     mainMenu->setEditorLanguage(NauSettings::editorLanguage());
}

void NauEditor::initToolbar()
{
    NauMainToolbar* toolbar = m_mainWindow->toolbar();
    m_mainWindow->connect(toolbar, &NauMainToolbar::eventUndo, [this]() { handleUndo(); });
    m_mainWindow->connect(toolbar, &NauMainToolbar::eventRedo, [this]() { handleRedo(); });

    m_mainWindow->connect(toolbar, &NauMainToolbar::eventPlay, [this]() { startPlayModeRequested(); });
    m_mainWindow->connect(toolbar, &NauMainToolbar::eventPause, [this](bool pause) { pausePlayModeRequested(pause); });
    m_mainWindow->connect(toolbar, &NauMainToolbar::eventStop, [this]() { stopPlayModeRequested(); });
    m_mainWindow->connect(toolbar, &NauMainToolbar::eventExport, [this]() { openBuildWindow(); });

    m_mainWindow->connect(toolbar, &NauMainToolbar::eventHistory, [this](const NauToolButton& button) {
        showCommandHistory(button);
    });
}

void NauEditor::initProjectBrowser()
{
    auto projectBrowser = m_mainWindow->projectBrowser();
    m_mainWindow->connect(projectBrowser, &NauProjectBrowser::eventFileDoubleClicked, [this](const QString& path, NauEditorFileType type) {
        NauFileAccess::openFile(path, type);
    });
    
    m_mainWindow->connect(projectBrowser, &NauProjectBrowser::eventAddContentClicked, [this](const QString& path) {
        NauAddContentMenu addContentMenu(path);
        addContentMenu.popup(QCursor::pos());
    });

    m_mainWindow->connect(projectBrowser, &NauProjectBrowser::eventImportClicked, [this](const QString& dir) {
        QString filePath = QFileDialog::getOpenFileName(nullptr,
        QCoreApplication::translate("Asset import", "Choose file to import"), dir);
        
        QDir contentDir(NauEditor::currentProject()->defaultContentFolder());
        if (contentDir.relativeFilePath(filePath).startsWith("..")) {
            const QString sourceFilePath = filePath;
            filePath = dir + "/" + QFileInfo(filePath).fileName();
            QFile::copy(sourceFilePath, filePath);
        }

        m_assetManager->importAsset(filePath.toUtf8().constData());
    });

    m_mainWindow->toolbar();
}

void NauEditor::initSceneManager()
{
    NauSceneManager& manager = m_project->scenes();
    m_mainWindow->connect(&manager, &NauSceneManager::eventCreateScene, [this](const std::string& path) {
        m_sceneManager->createSceneFile(path);
    });
    m_mainWindow->connect(&manager, &NauSceneManager::eventLoadScene, [this](const NauSceneManager::SceneInfo& scene) {
        switchScene(scene);
    });
    m_mainWindow->connect(&manager, &NauSceneManager::eventAboutToSwitchScenes, [this] {
        if (!m_sceneManager->isCurrentSceneSaved()) {
            showUnsavedSceneDialog(false);
        }
    });
}


void NauEditor::initModules()
{
    initAssetManagerModule();
    initSceneEditorModule();

    // Init Editors from Modules
    auto assetEditors = Nau::EditorServiceProvider().getAll<NauAssetEditorInterface>();
    for (auto assetEditor : assetEditors) {
        registerAssetEditor(assetEditor);
        assetEditor->postInitialize();
    }
}

void NauEditor::terminateModules()
{
    for (auto& editor : m_registeredEditors) {
        editor->preTerminate();
    }
}

void NauEditor::initSceneEditorModule()
{
    auto& sceneEditor = Nau::EditorServiceProvider().get<NauSceneEditorInterface>();

    m_sceneManager = sceneEditor.sceneManager();
}

void NauEditor::initAssetManagerModule()
{
    auto& assetProcessor = Nau::EditorServiceProvider().get<NauAssetFileProcessorInterface>();
    m_assetManager = std::make_shared<NauAssetManager>(&assetProcessor);
    m_assetManager->initialize(*m_project);

    NauFileAccess::setAssetManager(m_assetManager.get());
}

void NauEditor::configureProjectBrowser()
{
    // TODO: Refactor project browser configure
    auto projectBrowser = m_mainWindow->projectBrowser();
    projectBrowser->setAssetTypeResolvers({m_assetManager->typeResolver(), std::make_shared<NauProjectBrowserItemTypeResolver>()});
    projectBrowser->setProject(*m_project);
    projectBrowser->setAssetFileFilter(m_assetManager->assetFileFilter());

    QObject::connect(projectBrowser, &NauProjectBrowser::eventSourceChanged, [this] {
        NED_DEBUG("Sources of project has been changed. Mark as dirty to recompile at next startup");

        m_mainWindow->titleBar()->setCompilationState({ NauSourceState::RecompilationRequired, {}, {} });
        m_project->markSourcesDirty();
    });
}

void NauEditor::configureThumbnailManager()
{
    const std::filesystem::path projectPath = m_project->path().root().absolutePath().toUtf8().constData();
    // TODO: Store thumbnails not in the project?
    const std::filesystem::path thumbnailsFolder = projectPath / ".editor" / "thumbnails";
    m_thumbnailManager = std::make_shared<NauThumbnailManager>(thumbnailsFolder, m_assetManager->typeResolver());
}

void NauEditor::handleUndo()
{
    NED_DEBUG("Trying to undo");

    if (!undo()) {
        NED_TRACE("Nothing to undo");
    }
}

void NauEditor::handleRedo()
{
    NED_DEBUG("Trying to redo");

    if (!redo()) {
        NED_TRACE("Nothing to redo");
    }
}

void NauEditor::handleLanguageRequest(const QString& lang)
{
     const auto result  = QMessageBox::question(m_mainWindow.get(), qApp->applicationDisplayName(),
        QCoreApplication::translate("NauEditor", "The changes will take effect after restarting the editor.\nProceed?"),
        QMessageBox::Yes, QMessageBox::No);

     if (result != QMessageBox::Yes) {
        return;
     }

     NauSettings::setEditorLanguage(lang);
}

void NauEditor::startPlayModeRequested()
{
    NauPlayCommands::startPlay();
    NED_DEBUG("Play Mode started");
}

void NauEditor::stopPlayModeRequested()
{
    NED_DEBUG("Play Mode stopped");

    NauPlayCommands::stopPlay();
}

void NauEditor::pausePlayModeRequested(const bool isPaused) const
{
    if (isPaused) {
        NED_DEBUG("Play Mode paused");
    } else {
        NED_DEBUG("Play Mode resumed");
    }

    NauPlayCommands::pausePlay(isPaused);
}

void NauEditor::openBuildWindow()
{
    NED_TRACE("Launch project requested");
    NED_ASSERT(m_project);

    NauBuildStartupDailog buildDialog(*m_project, m_mainWindow.get());

    if (buildDialog.exec() != QDialog::Accepted) {
        NED_TRACE("Launch project rejected");
        return;
    }

    m_mainWindow->logger()->switchTab(m_launchLoggerName);
}

void NauEditor::showCommandHistory(const NauToolButton& button)
{
    NED_DEBUG("Showing command stack pop-up");

    auto historyView = new NauCommandStackPopup(m_commands);
    const auto buttonPosition = button.mapToGlobal(QPoint(0, 0));

    const int popupX = buttonPosition.x() - historyView->size().width() + button.size().width();
    const int popupY = buttonPosition.y() + button.size().height() * 1.5;

    historyView->show();
    historyView->raise();
    historyView->move(popupX, popupY);
}

void NauEditor::showProjectDialog()
{
    NED_DEBUG("Opening project manager dialog");

    auto windowProject = new NauProjectManagerWindow(m_mainWindow.get());
    m_mainWindow->connect(windowProject, &NauProjectManagerWindow::eventLoadProject, [this](NauProjectPtr project) {
        openProject(project);
    });

    if ((windowProject->showModal() == NauDialog::Rejected) && (m_project == nullptr)) {
        // Temporary.
        // No project currently was loaded and a user have just rejected to open a new one.
        // So we close the editor itself to protect a user from broken flow.
        m_mainWindow->close();
    }
}

bool NauEditor::showUnsavedProjectDialog()
{
    const auto result  = QMessageBox::question(m_mainWindow.get(), 
        QCoreApplication::translate("NauEditor", "Project Has Been Modified"),
        QCoreApplication::translate("NauEditor", "Save changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save
    );

    if (result == QMessageBox::Save) m_project->save();

    return result != QMessageBox::Cancel;
}

bool NauEditor::showUnsavedSceneDialog(bool showCancel)
{
    QMessageBox::StandardButtons buttons = QMessageBox::Save | QMessageBox::Discard;
    if (showCancel) {
        buttons |= QMessageBox::Cancel;
    }

    const auto result = QMessageBox::question(m_mainWindow.get(),
        QCoreApplication::translate("NauEditor", "Scene Has Been Modified"),
        QCoreApplication::translate("NauEditor", "Save changes?"),
        buttons,
        QMessageBox::Save
    );

    if (result == QMessageBox::Save) {
        m_sceneManager->saveCurrentScene();
    }

    return result != QMessageBox::Cancel;
}

void NauEditor::onEditorClosed(QCloseEvent* event)
{
    if (!m_sceneManager->isCurrentSceneSaved()) {
        if (!showUnsavedSceneDialog()) {  // User canceled close operation
            return event->ignore();
        }
    }

    if (m_project && m_project->isDirty()) {  // Any unsaved progress?
        if (!showUnsavedProjectDialog()) {  // User canceled close operation
            return event->ignore();
        }
    }

    m_mainWindow->saveUserSettings(m_project);
    
    // TODO:
    // if (m_project && m_mainWindow && m_mainWindow->viewport()) {
    //     m_project->saveProjectThumbnail(m_mainWindow->viewport()->viewportHandle());
    // }

    // Needed to unbind the module interface from the NauEditorWindow.
    // Otherwise they will be deleted when the dock manager is removed.
    terminateModules();

    m_mainWindow->closeWindow(event);
}

void NauEditor::onUpdateTitlebar()
{
    if (!m_project) {
        return;
    }

    // Show asterisks if either scene or project are modified
    const QString sceneTitle = m_project->scenes().currentSceneName() + (m_sceneManager && !m_sceneManager->isCurrentSceneSaved() ? "*" : "");
    const QString projectTitle = m_project->displayName() + (m_project->isDirty() ? "*" : "");
    m_mainWindow->titleBar()->updateTitle(projectTitle, sceneTitle);
}

void NauEditor::openProject(NauProjectPtr project)
{
    // TODO: Implement new project opening while editor is running
    m_project = project;
}


#ifdef USE_NAU_ANALYTICS
void NauEditor::initAnalytics()
{
    // Process GUID
    QString deviceId = NauSettings::deviceId();
    const bool userExist = !deviceId.isEmpty();

    if (!userExist) {
        // There may be collisions. Consider the MAC address
        deviceId = NauObjectGUID(QDateTime::currentSecsSinceEpoch());
        NauSettings::setDeviceId(deviceId);
    }

    // Read credentials
    QString errorMessage;
    QJsonObject credentials = Nau::Utils::File::readJSONFile(":/NauEditor/credentials.json", errorMessage);
    if (!errorMessage.isEmpty()) {
        NED_ERROR("Cannot read credentials: {}", errorMessage);
        return;
    }

    // Process Countly credentials
    QJsonObject countlyCredentials = credentials["countly"].toObject();
    initCountlyAnalyticsProvider(countlyCredentials, deviceId, userExist);
}

void NauEditor::initCountlyAnalyticsProvider(const QJsonObject& countlyCredentials, const QString & deviceId, bool isUserExist)
{
    if (countlyCredentials["app_key"].isNull() || countlyCredentials["host"].isNull() || countlyCredentials["port"].isNull()) {
        NED_ERROR("Cannot parse countly credentials.");
        return;
    }

    const std::string appKey = countlyCredentials["app_key"].toString().toUtf8().constData();
    const std::string host = countlyCredentials["host"].toString().toUtf8().constData();
    const int port = countlyCredentials["port"].toInt();

    const std::string pathToDatabase = countlyCredentials["data_base_path"].toString().toUtf8().constData();

    m_analytics->addAnalyticsProvider(std::make_unique<NauAnalyticsProviderCountly>(appKey, host, port, deviceId.toUtf8().constData(), isUserExist, pathToDatabase));
}

void NauEditor::sendEditorInfoEvent()
{
    std::map<std::string, std::string> analytic_event_02;

    analytic_event_02["device_id"] = NauSettings::deviceId().toUtf8().constData();
    analytic_event_02["editor_version"] = QString("%1").arg(QApplication::applicationVersion()).toUtf8().constData();

    m_analytics->sendEvent("analytic_event_02", analytic_event_02);
}

void NauEditor::sendSystemInfoEvent()
{
    std::map<std::string, std::string> analytic_event_04;

    const std::string os = QSysInfo::productType().toUtf8().constData();
    const std::string osVersion = QSysInfo::productVersion().toUtf8().constData();
    const std::string primaryScreenResolution = Nau::Utils::Conversion::QSizeToQString(QApplication::primaryScreen()->size()).toUtf8().constData();
    const std::string appVersion = QString("%1").arg(QApplication::applicationVersion()).toUtf8().constData();

    analytic_event_04["os_name"] = os;
    analytic_event_04["os_version"] = osVersion;
    analytic_event_04["os_pretty_name"] = QSysInfo::prettyProductName().toUtf8().constData();
    analytic_event_04["primary_screen_resolution"] = primaryScreenResolution;
    analytic_event_04["system_language"] = QLocale::system().name().toUtf8().constData();

    m_analytics->sendEvent("analytic_event_04", analytic_event_04);
}

void NauEditor::sendUIInteractionEvent(const std::string& uiName)
{
    m_analytics->sendEvent(uiName);
}
#endif


// ** Editor service functions

namespace Nau
{ 
    namespace
    {
        std::unique_ptr<NauEditorInterface>& EditorInstanceRef()
        {
            static std::unique_ptr<NauEditorInterface> m_editor;
            return m_editor;
        }
    }

    std::unique_ptr<NauEditorInterface> CreateEditor(NauProjectPtr project)
    {
        auto editor = std::make_unique<NauEditor>(project);
        NED_DEBUG("NauEditor instance created");

        return editor;
    }

    void SetDefaultEditor(std::unique_ptr<NauEditorInterface>&& editor)
    {
        NAU_FATAL(!editor || !EditorInstanceRef(), "Editor already set");
        EditorInstanceRef() = std::move(editor);
    }

    NauEditorInterface& Editor()
    {
        auto& instance = EditorInstanceRef();
        NAU_FATAL(instance);
        return *instance;
    }
}