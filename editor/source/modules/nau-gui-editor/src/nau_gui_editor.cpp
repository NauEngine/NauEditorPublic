// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_gui_editor.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"
#include "nau/prim-factory/nau_usd_prim_creator.hpp"
#include "nau/scene/scene_manager.h"

#include <unordered_set>

// TODO: Temporary solution.
// There is a bug in Debug Render that occurs when rendering ui and debug lines at the same time.
// Remove this code when the problem is fixed.
#include "nau/editor-engine/nau_editor_engine_services.hpp"


// ** NauGuiEditorSnapshot

void NauGuiEditorSnapshot::takeShapshot(NauGuiEditor* editor)
{
    if (m_snapshotInfo) {
        NAU_FAILURE("Snapshot info already exist");
        return;
    }

    m_editor = editor;
    m_snapshotInfo = std::make_unique<NauGuiEditorSnapshot::Info>();

    m_snapshotInfo->scenePath = m_editor->sceneManager()->currentScenePath();
}

void NauGuiEditorSnapshot::restoreShapshot()
{
    if (!m_snapshotInfo) {
        NAU_FAILURE("Snapshot info is empty");
        return;
    }

    if (m_snapshotInfo->scenePath.empty()) {
        return;
    }

    m_editor->openScene(m_snapshotInfo->scenePath);
}


// ** NauGuiEditor

NauGuiEditor::NauGuiEditor()
    : m_editorPanel(nullptr)
    , m_dwEditorPanel(nullptr)
{
}

NauGuiEditor::~NauGuiEditor()
{
    terminate();
}

void NauGuiEditor::initialize(NauEditorInterface* mainEditor)
{
    m_mainEditor = mainEditor;
    m_editorDockManger = m_mainEditor->mainWindow().dockManager();

    // Init editor systems
    initPrimFactory();
    initSceneManager();
    initSceneUndoRedo();

    // Create world for ui
    auto& sceneManager = nau::getServiceProvider().get<nau::scene::ISceneManager>();
    m_coreWorld = sceneManager.createWorld();
}

void NauGuiEditor::terminate()
{
    if (m_sceneEditorSynchronizer) {
        m_sceneEditorSynchronizer.reset();
    }

    if (m_selectionContainer) {
        m_selectionContainer->clear(false);
    }

    if (m_sceneUndoRedoSystem) {
        m_sceneUndoRedoSystem->unbindCurrentScene();
    }

    if (m_uiTranslator) {
        m_uiTranslator.reset();
    }
}

void NauGuiEditor::postInitialize()
{   
}

void NauGuiEditor::preTerminate()
{
    // TODO: unbind ui from NauEditor window
}

void NauGuiEditor::createAsset(const std::string& assetPath)
{
    if (!assetPath.empty()) {
        auto stage = PXR_NS::UsdStage::CreateInMemory(assetPath);
        auto prim = stage->DefinePrim(PXR_NS::SdfPath("/UI"), PXR_NS::TfToken("UI"));
        stage->SetDefaultPrim(prim);
        stage->GetRootLayer()->Export(assetPath);
    }
}

bool NauGuiEditor::openAsset(const std::string& assetPath)
{
    openEditorPanel();

    return openScene(assetPath);
}

bool NauGuiEditor::saveAsset(const std::string& assetPath)
{
    return m_sceneManager->saveCurrentScene();
}

std::string NauGuiEditor::editorName() const
{
    return "UI Editor";
}

NauEditorFileType NauGuiEditor::assetType() const
{
    return NauEditorFileType::UI;
}

std::shared_ptr<NauEditorSceneManagerInterface> NauGuiEditor::sceneManager()
{
    return m_sceneManager;
}

std::shared_ptr<NauOutlinerClientInterface> NauGuiEditor::outlinerClient()
{
    return m_outlinerClient;
}

void NauGuiEditor::handleSourceAdded(const std::string& sourcePath)
{
}

void NauGuiEditor::handleSourceRemoved(const std::string& sourcePath)
{
    if (m_assetPath == sourcePath) {
        if (m_sceneManager->currentScene()) {
            m_sceneManager->unloadCurrentScene();
        }

        m_worldOutline->outlinerTab().clear();
        m_inspector->clear();
    }
}

void NauGuiEditor::createEditorPanel()
{
    if (m_editorPanel) {
        return;
    }
    // Docking widget and the main toolbar

    m_editorPanel = new NauWidget;
    auto layout = new NauLayoutHorizontal(m_editorPanel);
    const QSize minSize{ 1280, 720 };
    m_editorPanel->setMinimumSize(minSize);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_editorPanel->setLayout(layout);

    auto shortcutHub = m_mainEditor->mainWindow().shortcutHub();
    m_worldOutline = new NauWorldOutlinerWidget(shortcutHub, m_editorPanel);
    m_viewportContainer = new NauViewportContainerWidget(m_editorPanel);
    m_inspector = new NauInspectorPage(m_editorPanel);

    layout->addWidget(m_worldOutline, Qt::AlignLeft);
    layout->addWidget(m_viewportContainer, Qt::AlignCenter);
    layout->addWidget(m_inspector, Qt::AlignRight);

    m_guiEditorDockManger = new NauDockManager(m_editorPanel);
    layout->addWidget(m_guiEditorDockManger);

    // Window docking setup
    auto dwSceneViewPort = new NauDockWidget(QObject::tr("Viewport"), nullptr);
    dwSceneViewPort->setWidget(m_viewportContainer);
    dwSceneViewPort->setFeature(ads::CDockWidget::DockWidgetAloneHasNoTitleBar, true);
    auto centralArea = m_guiEditorDockManger->addDockWidget(ads::CenterDockWidgetArea, dwSceneViewPort);
   
    auto dwInspector = new NauDockWidget(QObject::tr("Inspector"), nullptr);
    dwInspector->setWidget(m_inspector);
    dwInspector->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContentMinimumSize);
    m_guiEditorDockManger->addDockWidget(ads::RightDockWidgetArea, dwInspector);

    auto dwSceneHierarchy = new NauDockWidget(QObject::tr("Outliner"), nullptr);
    dwSceneHierarchy->setWidget(m_worldOutline);
    dwSceneHierarchy->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);
    m_guiEditorDockManger->addDockWidget(ads::LeftDockWidgetArea, dwSceneHierarchy);

    // Add to global dock manager
    m_dwEditorPanel = new NauDockWidget(QObject::tr(editorName().c_str()), nullptr);
    m_dwEditorPanel->setStyleSheet("background-color: #282828");
    m_dwEditorPanel->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);

    m_dwEditorPanel->setWidget(m_editorPanel);
    m_editorDockManger->addDockWidgetFloating(m_dwEditorPanel);
    m_dwEditorPanel->resize(1280, 720);
    m_dwEditorPanel->toggleView(true);
    
    loadDefaultUiPerspective();
}

void NauGuiEditor::openEditorPanel()
{
    if (m_editorPanel) {
        m_dwEditorPanel->toggleView(true);
        return;
    }

    createEditorPanel();

    // TODO: ui initialize
    initSelectionContainer();
    initOutlinerClient();
    initInspectorClient();

    m_sceneEditorSynchronizer = std::make_unique<NauUsdUIAssetEditorSynchronizer>(m_sceneUndoRedoSystem, m_selectionContainer);

    // Init asset manager
    auto assetManager = m_mainEditor->assetManager();
    assetManager->addClient({ NauEditorFileType::UI }, this);

    // Viewport initialize
    auto viewportManager = Nau::EditorEngine().viewportManager();
    auto viewport = viewportManager->createViewport(editorName().data());
    m_viewportContainer->setViewport(viewport);
    viewport->changeViewportController(std::make_shared<NauBaseEditorViewportController>(viewport, nullptr, nullptr, nullptr));
}


bool NauGuiEditor::openScene(const std::string& assetPath)
{
    m_assetPath = assetPath;
    if(m_sceneManager->currentScene()) {
        m_sceneManager->unloadCurrentScene();
    }

    return m_sceneManager->loadScene(assetPath);
}

void NauGuiEditor::initPrimFactory()
{
    auto& factory = NauUsdPrimFactory::instance();

    factory.addCreator("NauGuiNode",  std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("NauGuiDrawNode",  std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("NauGuiLabel",   std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("NauGuiButton",  std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("NauGuiLayer",   std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("NauGuiScroll",  std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("NauGuiSprite",  std::make_shared<NauDefaultUsdPrimCreator>());
}

void NauGuiEditor::initSceneManager()
{
    m_sceneManager = std::make_shared<NauUsdSceneManager>();

    // Subscribe to scene loading
    m_sceneManager->currentSceneChanged.addCallback([this](pxr::UsdStageRefPtr currentScene) {
        if (currentScene != nullptr) {
            onSceneLoaded(currentScene);
        } else {
            onSceneUnloaded();
        }
    });
}

void NauGuiEditor::initSelectionContainer()
{
    m_selectionContainer = std::make_shared<NauUsdSelectionContainer>();
}

void NauGuiEditor::initSceneUndoRedo()
{
    m_sceneUndoRedoSystem = std::make_shared<NauUsdSceneUndoRedoSystem>(m_mainEditor->undoRedoSystem());
}

void NauGuiEditor::initInspectorClient()
{
    auto inspectorWidget = m_inspector;
    m_inspectorClient = std::make_shared<NauUsdInspectorClient>(inspectorWidget);

    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventComponentAdded, [this](const PXR_NS::SdfPath& parentPath, const PXR_NS::TfToken typeName) {
        const bool isComponent = true;
        createPrim(parentPath, typeName, typeName, isComponent);

        saveAsset();
    });

    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventComponentRemoved, [this](const PXR_NS::SdfPath& path) {
        m_sceneUndoRedoSystem->addCommand<NauCommandRemoveUsdPrim>(path.GetString());

        saveAsset();
    });

    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventPropertyChanged, [this](const PXR_NS::SdfPath& path, const PXR_NS::TfToken propName, const PXR_NS::VtValue& value) {
        NauUITranslatorNotificationBlock block(m_uiTranslator.get());
        m_sceneUndoRedoSystem->addCommand<NauCommandChangeUsdPrimProperty>(path, propName, value);

        saveAsset();
    });

    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventAssetReferenceChanged, [this](const PXR_NS::SdfPath& path, const PXR_NS::VtValue& value) {
        NauUITranslatorNotificationBlock block(m_uiTranslator.get());
        m_sceneUndoRedoSystem->addCommand<NauCommandChangeUsdPrimAssetReference>(path, value);

        saveAsset();
    });

    m_selectionContainer->selectionChangedDelegate.addCallback([this](const NauUsdPrimsSelection& selection) {
        if (selection.empty()) {
            m_inspectorClient->clear();
            return;
        }

        const bool hideAddButton = true;
        m_inspectorClient->buildFromPrim(selection.back(), hideAddButton);
    });
}

void NauGuiEditor::initOutlinerClient()
{
    auto outlinerWidget = m_worldOutline;
    auto tab = &outlinerWidget->outlinerTab();

    // Create client
    m_outlinerClient = std::make_shared<NauGuiOutlinerClient>(outlinerWidget, *tab, m_selectionContainer);

    // Subscribe usd scene to usd outliner client
    m_outlinerClient->connect(m_outlinerClient.get(), &NauGuiOutlinerClient::eventPrimDeleteRequested, [this]() {
        const NauUsdNormalizedContainer normalizedContainer(m_selectionContainer->selection());
        m_sceneUndoRedoSystem->groupBegin(normalizedContainer.count());
        removePrims(normalizedContainer.data());
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauGuiOutlinerClient::eventPrimRenameRequested, [this](const std::string& path, const std::string& newName) {
        m_sceneUndoRedoSystem->addCommand<NauCommandRenameUsdPrim>(path, newName);

        saveAsset();
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauGuiOutlinerClient::eventPrimCreateRequested, [this](const std::string& typeName) {
        const bool isComponent = false;
        createPrim(pxr::SdfPath(m_selectionContainer->lastSelectedPath()), pxr::TfToken(typeName), pxr::TfToken(typeName), isComponent);
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauGuiOutlinerClient::eventPrimsPasteRequested, [this](const std::vector<PXR_NS::UsdPrim>& primsToPaste) {
        const NauUsdNormalizedContainer normalizedContainer(primsToPaste);
        m_sceneUndoRedoSystem->groupBegin(normalizedContainer.count());
        addPrimsFromOther(normalizedContainer.data(), PXR_NS::SdfPath(m_selectionContainer->lastSelectedPath()));
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauGuiOutlinerClient::eventPrimsDuplicateRequested, [this]() {
        const NauUsdNormalizedContainer normalizedContainer(m_selectionContainer->selection());
        m_sceneUndoRedoSystem->groupBegin(normalizedContainer.count());
        addPrimsFromOther(normalizedContainer.data());
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauGuiOutlinerClient::eventFocusOnPrim, [this](const std::string& path) {
        m_sceneEditorSynchronizer->focusOnObject(pxr::SdfPath(path));
    });

    // Init outliner selection
    outlinerWidget->connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::itemSelectionChanged, [this, outlinerWidget] {
        //Redraw the widget
        outlinerWidget->outlinerTab().update();
        
        // Selecting objects on the scene
        const auto& table = outlinerWidget->outlinerTab();
        const auto& selected = table.getSelectedRowsWithGuids();
        
        std::vector<std::string> selectedPaths;
        
        // TODO: Try to do it without cast
        auto usdOutlinerClient = dynamic_cast<NauGuiOutlinerClient*>(this->outlinerClient().get());
        
        for (const auto& item : selected) {
            selectedPaths.push_back(usdOutlinerClient->pathFromModelIndex(item));
        }
        
        // TODO: Try to do it without cast
        auto usdSceneManager = dynamic_cast<NauUsdSceneManager*>(this->sceneManager().get());
        m_selectionContainer->updateFromPaths(usdSceneManager->currentScene(), selectedPaths);
    });

    // Selection from usd scene
    m_selectionContainer->selectionChangedDelegate.addCallback([this](const NauUsdPrimsSelection& selection) {
        NauWorldOutlinerWidget* outlineWidget = m_outlinerClient->outlinerWidget();
        QSignalBlocker blocker(outlineWidget->outlinerTab());
        outlineWidget->outlinerTab().clearSelection();

        if (selection.empty()) {
            return;
        }

        for (const pxr::UsdPrim& selectedPrim : selection) {
            QTreeWidgetItem* item = m_outlinerClient->itemFromPath(selectedPrim.GetPath().GetString());
            if (item) {
                item->setSelected(true);
            }
        }

        saveAsset();
    });

    // Init outliner shortcuts

    m_mainEditor->mainWindow().connect(&outlinerWidget->outlinerTab(), & NauWorldOutlineTableWidget::eventRename, [this](QTreeWidgetItem* item, const QString& newName) {
        m_outlinerClient->renameItems(item, newName.toUtf8().constData());
    });

    m_mainEditor->mainWindow().connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventDelete, [this]() {
        m_outlinerClient->deleteItems();
    });

    m_mainEditor->mainWindow().connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventCopy, [this]() {
        m_outlinerClient->copyItems();
    });

    m_mainEditor->mainWindow().connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventPaste, [this]() {
        m_outlinerClient->pasteItems();
    });

    m_mainEditor->mainWindow().connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventDuplicate, [this]() {
        m_outlinerClient->duplicateItems();
    });

    m_mainEditor->mainWindow().connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventFocus, [this](QTreeWidgetItem* item) {
        m_outlinerClient->focusOnItem(item);
    });

        // Connect to object creation when the editor is started
    outlinerWidget->connect(&outlinerWidget->getHeaderWidget(), &NauWorldOutlinerWidgetHeader::eventCreateObject, [this](const std::string& typeName) {
        m_outlinerClient->createItem(typeName);
    });

    // Bind viewport shortcut operations to outliner
    // TODO: Bind this operations to scene, will be fixed in
    //m_editorWindow->connect(m_editorWindow->viewport(), &NauViewportContainerWidget::eventDeleteRequested, &outlinerWidget->getTableWidget("GuiScene"), &NauWorldOutlineTableWidget::deleteItems);
    //m_editorWindow->connect(m_editorWindow->viewport(), &NauViewportContainerWidget::eventDuplicateRequested, &outlinerWidget->getTableWidget("GuiScene"), &NauWorldOutlineTableWidget::duplicateItems);
    //m_editorWindow->connect(m_editorWindow->viewport(), &NauViewportContainerWidget::eventCopyRequested, &outlinerWidget->getTableWidget("GuiScene"), &NauWorldOutlineTableWidget::copyItems);
    //m_editorWindow->connect(m_editorWindow->viewport(), &NauViewportContainerWidget::eventCutRequested, &outlinerWidget->getTableWidget("GuiScene"), &NauWorldOutlineTableWidget::cutItems);
    //m_editorWindow->connect(m_editorWindow->viewport(), &NauViewportContainerWidget::eventPasteRequested, &outlinerWidget->getTableWidget("GuiScene"), &NauWorldOutlineTableWidget::pasteItems);
}

void NauGuiEditor::onSceneLoaded(pxr::UsdStageRefPtr scene)
{
    auto editorTitle = std::format("{} - {}", editorName().c_str(), scene->GetRootLayer()->GetDisplayName());
    m_dwEditorPanel->setWindowTitle(editorTitle.c_str());

    m_selectionContainer->clear(false);

    m_sceneUndoRedoSystem->bindCurrentScene(scene);

    m_outlinerClient->updateItemsFromScene(scene);

    // Subscribe to sceneChanges
    m_uiTranslator.reset();
    m_uiTranslator = std::make_unique<NauUsdSceneUITranslator>(scene);
    m_uiTranslator->addClient(m_outlinerClient);
    m_uiTranslator->addClient(m_inspectorClient);

    m_scene = scene;
    m_sceneEditorSynchronizer->onEditorSceneChanged(scene, m_coreWorld);

    auto viewportManager = Nau::EditorEngine().viewportManager();
    viewportManager->setViewportRendererWorld(editorName().data(), m_coreWorld->getUid());

    m_primChangeListener = std::make_unique<AttributeChangeListener>(scene, m_inspectorClient);
}

void NauGuiEditor::onSceneUnloaded()
{
    if (!m_sceneEditorSynchronizer) {
        return;
    }

    m_sceneEditorSynchronizer->onEditorSceneChanged(nullptr, m_coreWorld);
    m_scene.Reset();

    m_selectionContainer->clear(false);
    m_sceneUndoRedoSystem->unbindCurrentScene();
    m_uiTranslator.reset();
    m_primChangeListener.reset();

    m_outlinerClient->clearItems();
    m_inspectorClient->clear();
}

PXR_NS::UsdPrim NauGuiEditor::createPrim(const PXR_NS::SdfPath& parentPath, const PXR_NS::TfToken& name, const PXR_NS::TfToken& typeName, bool isComponent)
{
    NauUITranslatorNotificationAccumulator accumulator(m_uiTranslator.get());

    // TODO: Try to do it without cast
    NauUsdSceneManager* usdSceneManager = dynamic_cast<NauUsdSceneManager*>(this->sceneManager().get());
    const std::string uniquePath = NauUsdSceneUtils::generateUniquePrimPath(usdSceneManager->currentScene(), parentPath.GetString(), name);

    // TODO: Get transform from engine (take the transform in front of the camera)
    PXR_NS::GfMatrix4d initialTransform;
    initialTransform.SetIdentity();

    const std::string displayName = name;
    m_sceneUndoRedoSystem->addCommand<NauCommandCreateUsdPrim>(uniquePath, typeName, name, initialTransform, isComponent);

    saveAsset();

    return usdSceneManager->currentScene()->GetPrimAtPath(PXR_NS::SdfPath(uniquePath));
}

void NauGuiEditor::addPrimsFromOther(const std::vector<PXR_NS::UsdPrim>& prims)
{
    // Need to avoid infinite recursion, if we added a copied objects as childs of one of copied objects
    pxr::SdfPathSet primPaths;
    NauUsdEditorUtils::collectPrimsWithChildrensPaths(prims, primPaths);
    for (auto prim : prims) {
        PXR_NS::SdfPath parentPath = prim.GetParent().IsValid() ? prim.GetParent().GetPath() : PXR_NS::SdfPath::EmptyPath();
        addPrimFromOther(primPaths, prim, parentPath);
    }

    saveAsset();
}

void NauGuiEditor::addPrimsFromOther(const std::vector<PXR_NS::UsdPrim>& prims, const PXR_NS::SdfPath& pathToAdd)
{
    // Need to avoid infinite recursion, if we added a copied objects as childs of one of copied objects
    pxr::SdfPathSet primPaths;
    NauUsdEditorUtils::collectPrimsWithChildrensPaths(prims, primPaths);
    for (auto prim : prims) {
        addPrimFromOther(primPaths, prim, pathToAdd);
    }

    saveAsset();
}

void NauGuiEditor::addPrimFromOther(const pxr::SdfPathSet& primsPaths, PXR_NS::UsdPrim other, const PXR_NS::SdfPath& parentPath)
{
    // Need to avoid infinite recursion, if we added a copied objects as childs of one of copied objects
    if (!primsPaths.contains(other.GetPath())) {
        return;
    }

    PXR_NS::UsdPrim newPrim = createPrim(parentPath, other.GetName(), other.GetTypeName(), NauUsdPrimUtils::isPrimComponent(other));
    for (auto childPrim : other.GetAllChildren()) {
        addPrimFromOther(primsPaths, childPrim, newPrim.GetPath());
    }

    saveAsset();
}

void NauGuiEditor::removePrims(const std::vector<PXR_NS::UsdPrim>& normalizedPrimList)
{
    for (const pxr::UsdPrim& prim : normalizedPrimList) {
        removePrim(prim);
    }

    saveAsset();
}

void NauGuiEditor::removePrim(const PXR_NS::UsdPrim& prim)
{
    for (auto childPrim : prim.GetAllChildren()) {
        removePrim(childPrim);
    }
    m_sceneUndoRedoSystem->addCommand<NauCommandRemoveUsdPrim>(prim.GetPath().GetString());

    saveAsset();
}

void NauGuiEditor::loadDefaultUiPerspective()
{
    // TODO: clean up
    // Splitters are 4 pixels wide by design.
    constexpr int HandleWidth = 4;

    // 1. Align this window at center of 1st screen.
    const QRect screenRect = QGuiApplication::screens().at(0)->geometry();

    QRect myGeometry = m_dwEditorPanel->geometry();
    myGeometry.moveCenter(screenRect.center());
    m_dwEditorPanel->setGeometry(myGeometry);

    auto const applyProportion = [this](const QList<int>&sizes, std::vector<float> const& proportion) {
        static const float eps = 0.001f;
        NED_ASSERT(std::abs(std::accumulate(proportion.begin(), proportion.end(), 0.0f) - 1.0f) < eps);

        if (sizes.size() != proportion.size()) return sizes;

        QList<int> result = sizes;

        const int sum = std::accumulate(std::begin(sizes), std::end(sizes), 0);
        for (size_t idx = 0; idx < proportion.size(); ++idx) {
            result[idx] = proportion[idx] * sum;
        }

        return result;
    };

    // 2. Set the width of the windows when the editor is first opened to match the application's resolution.
    
    // TODO: This code is the beginning of a potential system that will be responsible for scaling the editor
    // interface to the right proportions.

    // In the future we will build a scaling curve, 
    // where the x - axis will be the resolution of the side,
    // and the y - axis will be the scaling of this or that element.

    // Thus we will get a universal system that will
    // not simply scale 1:1 windows and will not accumulate
    // a math error on the most common screen resolutions.

    std::unordered_map<int, QList<int>> typicalResolutionSplitterSizes;
    typicalResolutionSplitterSizes[1280] = { 340, 600  - 2 * HandleWidth, 340 };
    typicalResolutionSplitterSizes[1366] = { 340, 686  - 2 * HandleWidth, 340 };
    typicalResolutionSplitterSizes[1920] = { 424, 1072 - 2 * HandleWidth, 424 };
    typicalResolutionSplitterSizes[2560] = { 532, 1496 - 2 * HandleWidth, 532 };
    typicalResolutionSplitterSizes[3840] = { 532, 2776 - 2 * HandleWidth, 532 };

    // 3. Set proportion of top level widgets(Scenes, Viewport, Inspector).
    auto topLevelArea = m_guiEditorDockManger->dockArea(0);
    if (!topLevelArea) return;

    if (typicalResolutionSplitterSizes.contains(screenRect.width())) {
        m_guiEditorDockManger->setSplitterSizes(topLevelArea, typicalResolutionSplitterSizes[screenRect.width()]);
    } else {
        static const std::vector<float> topLevelWidgetsProportion{0.222f, 0.556f, 0.222f};
        m_guiEditorDockManger->setSplitterSizes(topLevelArea, applyProportion(m_guiEditorDockManger->splitterSizes(topLevelArea), topLevelWidgetsProportion));
    }
}


void NauGuiEditor::startPlay()
{
    m_snapshot = std::make_unique<NauGuiEditorSnapshot>();
    m_snapshot->takeShapshot(this);

    onSceneUnloaded();
}

void NauGuiEditor::stopPlay()
{
    m_snapshot->restoreShapshot();
    m_snapshot.reset();
}