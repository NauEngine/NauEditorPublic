// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_usd_scene_editor_impl.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"

#include "nau/service/service_provider.h"
#include "nau/scene/components/component.h"
#include "nau/scene/internal/scene_manager_internal.h"

#include "nau/prim-factory/nau_usd_prim_creator.hpp"
#include "nau/selection/nau_object_selection.hpp"

#include "viewport/nau_viewport_scene_editor_tools.hpp"
#include "viewport/nau_viewport_scene_drag_drop_tools.hpp"
#include "nau/viewport/nau_base_viewport_controller.hpp"
#include "nau/selection/nau_object_selection.hpp"
#include "nau/nau_editor_delegates.hpp"
#include "nau/service/service_provider.h"
#include "nau/graphics/core_graphics.h"
#include "nau/render/render_window.h"
#include "nau_log.hpp"

#include "widgets/nau_scene_editor_viewport_toolbar.hpp"

#include "nau/editor-engine/nau_editor_engine_services.hpp"
#include "viewport/nau_outline_manager.hpp"

#include <unordered_set>


// ** NauUsdSceneEditorSnapshot

void NauUsdSceneEditorSnapshot::takeShapshot(NauUsdSceneEditor* sceneEditor)
{
    if (m_snapshotInfo) {
        NAU_FAILURE("Snapshot info already exist");
        return;
    }

    m_sceneEditor = sceneEditor;
    m_snapshotInfo = std::make_unique<NauUsdSceneEditorSnapshot::Info>();

    auto cameraController = dynamic_cast<NauSceneCameraController*>(m_sceneEditor->m_cameraController.get());
    if (!cameraController) {
        return;
    }

    m_snapshotInfo->fov = cameraController->internalController().cameraFoV();
    m_snapshotInfo->transform = cameraController->internalController().cameraMatrix();
    m_snapshotInfo->clipPlanes = cameraController->internalController().cameraClippingPlanes();
    m_snapshotInfo->easing = cameraController->internalController().cameraEasing();
    m_snapshotInfo->acceleration = cameraController->internalController().cameraAcceleration();
    m_snapshotInfo->speed = cameraController->internalController().cameraSpeed();
}

void NauUsdSceneEditorSnapshot::restoreShapshot()
{
    if (!m_snapshotInfo) {
        NAU_FAILURE("Snapshot info is empty");
        return;
    }

    auto cameraController = dynamic_cast<NauSceneCameraController*>(m_sceneEditor->m_cameraController.get());
    if (!cameraController) {
        return;
    }

    cameraController->internalController().setCameraFoV(m_snapshotInfo->fov);
    cameraController->internalController().setCameraMatrix(m_snapshotInfo->transform);
    cameraController->internalController().setCameraClippingPlanes(m_snapshotInfo->clipPlanes);
    cameraController->internalController().setCameraEasing(m_snapshotInfo->easing);
    cameraController->internalController().setCameraAcceleration(m_snapshotInfo->acceleration);
    cameraController->internalController().setCameraSpeed(m_snapshotInfo->speed);
}


// ** NauUsdSceneEditor

NauUsdSceneEditor::NauUsdSceneEditor()
    : m_viewport(nullptr)
{
}

NauUsdSceneEditor::~NauUsdSceneEditor()
{
    terminate();
}

void NauUsdSceneEditor::initialize(NauEditorInterface* mainEditor)
{
    m_mainEditor = mainEditor;

    // Init editor systems
    initPrimFactory();
    initSceneManager();
    initSelectionContainer();
    initSceneUndoRedo();

    m_sceneSynchronizer = std::make_shared<NauUsdSceneSynchronizer>();

    auto&& outlineManager = nau::rtti::createInstanceSingleton<NauOutlineManager>(m_sceneSynchronizer);
    nau::getServiceProvider().addService(outlineManager);
}

void NauUsdSceneEditor::postInitialize()
{
    const auto descriptors = nau::getServiceProvider().findClasses<const nau::scene::Component>();
    for (const auto& descriptor : descriptors) {
        NauUsdPrimFactory::instance().addCreator(descriptor->getClassName().c_str(),
            std::make_shared<NauUsdPrimComponentCreator>(descriptor->getClassName()));
    }

    initOutlinerClient();
    initInspectorClient();
    initViewportWidget();
    initViewportTools();
}

void NauUsdSceneEditor::preTerminate()
{
    // Unlink viewport from container
    m_viewport->setParent(nullptr);
}

void NauUsdSceneEditor::terminate()
{
    m_sceneSynchronizer.reset();

    nau::getServiceProvider().get<NauOutlineManager>().reset();
    m_selectionContainer->clear(false);
    m_sceneUndoRedoSystem->unbindCurrentScene();
    m_uiTranslator.reset();
}

bool NauUsdSceneEditor::openAsset(const std::string& assetPath)
{
    // TODO: Implement
    return true;
}

bool NauUsdSceneEditor::saveAsset(const std::string& assetPath)
{
    // TODO: Implement
    return false;
}

NauUsdSelectionContainerPtr NauUsdSceneEditor::selectionContainer() const noexcept
{
    return m_selectionContainer;
}

const NauUsdSceneSynchronizer& NauUsdSceneEditor::sceneSynchronizer() const noexcept
{
    return *m_sceneSynchronizer;
}

NauCameraControllerInterfacePtr NauUsdSceneEditor::sceneCameraController() const noexcept
{
    return m_cameraController;
}

void NauUsdSceneEditor::changeMode(bool isPlaymode)
{
    // TODO: Do not unload scene. Use new world instance for playmode
    onSceneUnloaded();

    if (isPlaymode) {
        auto emptyStage = pxr::UsdStage::CreateInMemory();
        onSceneLoaded(emptyStage, NauUsdSceneSynchronizer::SyncMode::FromEngine);
        m_snapshot = std::make_unique<NauUsdSceneEditorSnapshot>();
        m_snapshot->takeShapshot(this);
    } else {
        onSceneLoaded(m_sceneManager->currentScene(), NauUsdSceneSynchronizer::SyncMode::FromEditor);
    }

    if (!isPlaymode && m_snapshot) {
        m_snapshot->restoreShapshot();
        m_snapshot.reset();
    }
}

std::string NauUsdSceneEditor::editorName() const
{
    return QObject::tr("Scene").toUtf8().constData();
}

NauEditorFileType NauUsdSceneEditor::assetType() const
{
    return NauEditorFileType::Scene;
}

// TODO: Move to other class (common class for usd editing NauUsdEditor)
void NauUsdSceneEditor::initPrimFactory()
{
    auto& factory = NauUsdPrimFactory::instance();

    factory.addCreator("Xform", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("SphereLight", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("AudioEmitter", std::make_shared<NauDefaultUsdPrimCreator>());
    // TODO:
    // factory.addCreator("AudioListener", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("NauAssetVFX", std::make_shared<NauResourceUsdPrimCreator>("../particles/baseVFX.usda.nausd", pxr::SdfPath("/Root/VFX")), "VFXInstance");
    factory.addCreator("AnimationController", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("AnimationSkeleton", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("RigidBodyCube", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("RigidBodySphere", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("RigidBodyCapsule", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("RigidBodyCylinder", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("RigidBodyConvexHull", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("RigidBodyMesh", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator("NauAssetMesh", std::make_shared<NauResourceUsdPrimCreator>("../meshes/cube.usda.nausd", pxr::SdfPath("/Root/Cube")), "Mesh");
}

void NauUsdSceneEditor::initSceneManager()
{
    m_sceneManager = std::make_shared<NauUsdSceneManager>();

    // Subscribe to scene loading from editor
    m_sceneManager->currentSceneChanged.addCallback([this](pxr::UsdStageRefPtr currentScene) {
        if (currentScene != nullptr) {
            onSceneLoaded(currentScene, NauUsdSceneSynchronizer::SyncMode::FromEditor);
        } else {
            onSceneUnloaded();
        }
    });
}

void NauUsdSceneEditor::initSelectionContainer()
{
    m_selectionContainer = std::make_shared<NauUsdSelectionContainer>();
}

void NauUsdSceneEditor::initSceneUndoRedo()
{     
    m_sceneUndoRedoSystem = std::make_shared<NauUsdSceneUndoRedoSystem>(m_mainEditor->undoRedoSystem());
}

void NauUsdSceneEditor::initInspectorClient()
{
    auto inspectorWidget = m_mainEditor->mainWindow().inspector();
    m_inspectorClient = std::make_shared<NauUsdInspectorClient>(inspectorWidget);

    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventComponentAdded, [this](const PXR_NS::SdfPath& parentPath, const PXR_NS::TfToken typeName) {
        const bool isComponent = true;
        std::string uniquePath = "";
        createPrim(parentPath,  typeName, typeName, isComponent, uniquePath);
    });
    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventComponentRemoved, [this](const PXR_NS::SdfPath& path) {
        m_sceneUndoRedoSystem->addCommand<NauCommandRemoveUsdPrim>(path.GetString());
    });

    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventPropertyChanged, [this](const PXR_NS::SdfPath& path, const PXR_NS::TfToken propName, const PXR_NS::VtValue& value) {
        // TODO: Remove "if" when the broadcast from the engine scene to usd is completed 
        if (m_uiTranslator) {
            NauUITranslatorNotificationBlock block(m_uiTranslator.get());
            m_sceneUndoRedoSystem->addCommand<NauCommandChangeUsdPrimProperty>(path, propName, value);
        }
    });

    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventAssetReferenceChanged, [this](const PXR_NS::SdfPath& path, const PXR_NS::VtValue& value) {
        // TODO: Remove "if" when the broadcast from the engine scene to usd is completed 
        if (m_uiTranslator) {
            NauUITranslatorNotificationBlock block(m_uiTranslator.get());
            m_sceneUndoRedoSystem->addCommand<NauCommandChangeUsdPrimAssetReference>(path, value);
        }
    });

    m_selectionContainer->selectionChangedDelegate.addCallback([this](const NauUsdPrimsSelection& selection) {
        if (selection.empty()) {
            m_inspectorClient->clear();
            return;
        }

        m_inspectorClient->buildFromPrim(selection.back());
    });
}

void NauUsdSceneEditor::selectObject(QMouseEvent* event, float dpi)
{
    auto& graphcis = nau::getServiceProvider().get<nau::ICoreGraphics>();
    const auto [x, y] = event->position();
    nau::Uid objectUid = *Nau::EditorEngine().runTaskSync(graphcis.getDefaultRenderWindow().acquire()->requestUidByCoords(x * dpi, y * dpi));

    const bool isMultiplySelection = event->modifiers() & Qt::Modifier::CTRL;
    if (!objectUid) {
        if (!isMultiplySelection) {
            m_selectionContainer->clear();
        }
        return;
    }

    auto& sceneManager = nau::getServiceProvider().get<nau::scene::ISceneManagerInternal>();
    if (auto* component = sceneManager.findComponent(objectUid)) {
        objectUid = component->getParentObject().getUid();
    }

    pxr::SdfPath primPath = m_sceneSynchronizer->primFromSceneObject(objectUid);
    if (primPath.IsEmpty()) {
        NED_CRITICAL("Selection failed. Object exist in engine scene and unsyncronized with editor scene!");
        m_selectionContainer->clear();
        return;
    }

    if (!isMultiplySelection) {
        const bool notify = false;
        m_selectionContainer->clear(notify);
    }

    m_selectionContainer->addFromPath(m_sceneManager->currentScene(), primPath);
}

void NauUsdSceneEditor::initOutlinerClient()
{
    auto outlinerWidget = m_mainEditor->mainWindow().outliner();

    // Create client
    m_outlinerClient = std::make_shared<NauUsdOutlinerClient>(outlinerWidget, outlinerWidget->outlinerTab(), m_selectionContainer);

    // Subscribe usd scene to usd outliner client
    m_outlinerClient->connect(m_outlinerClient.get(), &NauUsdOutlinerClient::eventPrimDeleteRequested, [this]() {
        const NauUsdNormalizedContainer normalizedContainer(m_selectionContainer->selection());
        m_sceneUndoRedoSystem->groupBegin(normalizedContainer.count());
        removePrims(normalizedContainer.data());
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauUsdOutlinerClient::eventPrimRenameRequested, [this](const std::string& path, const std::string& newName) {
        m_sceneUndoRedoSystem->addCommand<NauCommandRenameUsdPrim>(path, newName);
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauUsdOutlinerClient::eventPrimCreateRequested, [this](const std::string& typeName) {
        const bool isComponent = false;
        std::string uniquePath = "";
        createPrim(pxr::SdfPath(m_selectionContainer->lastSelectedPath()), pxr::TfToken(typeName), pxr::TfToken(typeName), isComponent, uniquePath);
        m_sceneSynchronizer->addBillboard(pxr::SdfPath(uniquePath), typeName);
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauUsdOutlinerClient::eventBillboardCreateRequested, [this](const std::string& typeName, const std::string& path) {
        m_sceneSynchronizer->addBillboard(pxr::SdfPath(path), typeName);
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauUsdOutlinerClient::eventPrimsPasteRequested, [this](const std::vector<PXR_NS::UsdPrim>& primsToPaste) {
        const NauUsdNormalizedContainer normalizedContainer(primsToPaste);
        m_sceneUndoRedoSystem->groupBegin(normalizedContainer.count());
        addPrimsFromOther(normalizedContainer.data(), PXR_NS::SdfPath(m_selectionContainer->lastSelectedPath()));
    });

    m_outlinerClient->connect(m_outlinerClient.get(), &NauUsdOutlinerClient::eventPrimsDuplicateRequested, [this]() {
        const NauUsdNormalizedContainer normalizedContainer(m_selectionContainer->selection());
        m_sceneUndoRedoSystem->groupBegin(normalizedContainer.count());
        addPrimsFromOther(normalizedContainer.data());
    });
    
    m_outlinerClient->connect(m_outlinerClient.get(), &NauUsdOutlinerClient::eventPrimReparentRequested, [this]
        (const std::string& sourcePath, const std::string& destinationPath) {
        m_sceneUndoRedoSystem->addCommand<NauCommandReparentUsdPrim>(sourcePath, destinationPath);
    });


    m_outlinerClient->connect(m_outlinerClient.get(), &NauUsdOutlinerClient::eventFocusOnPrim, [this](const std::string& path) {
        m_sceneSynchronizer->focusOnObject(pxr::SdfPath(path));
    });

    // Init outliner selection
    outlinerWidget->connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::itemSelectionChanged, [this, outlinerWidget]
    {
        //Redraw the widget
        outlinerWidget->outlinerTab().update();

        // Selecting objects on the scene
        const auto& table = outlinerWidget->outlinerTab();
        const auto& selected = table.getSelectedRowsWithGuids();

        std::vector<std::string> selectedPaths;

        // TODO: Try to do it without cast
        auto usdOutlinerClient = dynamic_cast<NauUsdOutlinerClient*>(this->outlinerClient().get());

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
    });

    m_selectionContainer->selectionChangedDelegate.addCallback([](const NauUsdPrimsSelection& selection) {
        nau::getServiceProvider().get<NauOutlineManager>().setHighlightObjects(selection);
    });

    // Sunscribe client to outliner tab
    //Init outliner shortcuts
    outlinerWidget->connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventRename, [this](QTreeWidgetItem* item, const QString& newName) {
        m_outlinerClient->renameItems(item, newName.toUtf8().constData());
    });

    outlinerWidget->connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventDelete, [this]() {
        m_outlinerClient->deleteItems();
    });

    outlinerWidget->connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventCopy, [this]() {
        m_outlinerClient->copyItems();
    });

    outlinerWidget->connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventPaste, [this]() {
        m_outlinerClient->pasteItems();
    });

    outlinerWidget->connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventDuplicate, [this]() {
        m_outlinerClient->duplicateItems();
    });

    outlinerWidget->connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventFocus, [this](QTreeWidgetItem* item) {
        m_outlinerClient->focusOnItem(item);
    });
    
    outlinerWidget->connect(&outlinerWidget->outlinerTab(), &NauWorldOutlineTableWidget::eventMove,
        m_outlinerClient.get(), &NauOutlinerClientInterface::moveItems);

    // Connect to object creation when the editor is started
    outlinerWidget->connect(&outlinerWidget->getHeaderWidget(), &NauWorldOutlinerWidgetHeader::eventCreateObject, [this](const std::string& typeName) {
        m_outlinerClient->createItem(typeName);
    });
}

void NauUsdSceneEditor::initViewportWidget()
{
    const NauEditorWindowAbstract& mainWindow = m_mainEditor->mainWindow();
    NauViewportContainerWidget* viewportContainer = mainWindow.viewport();
    
    m_viewport = Nau::EditorEngine().viewportManager()->mainViewport();
    m_toolbar = new NauSceneEditorViewportToolbar(*m_viewport, mainWindow.shortcutHub());
    auto& outlineManager = nau::getServiceProvider().get<NauOutlineManager>();
    outlineManager.enableOutline(true);
    outlineManager.setWidth(2.f);
    outlineManager.setColor({ 1.f, 0.5f, 0.f, 1.f });

    viewportContainer->setViewport(m_viewport, mainWindow.shortcutHub(), m_toolbar);

    viewportContainer->connect(viewportContainer, &NauViewportContainerWidget::eventFocusRequested, [this]() {
        if (auto&& path = m_selectionContainer->lastSelectedPath(); !path.empty()) {
            m_sceneSynchronizer->focusOnObject(pxr::SdfPath(path));
        }
    });

    // Subscribe to events
    mainWindow.connect(mainWindow.toolbar(), &NauMainToolbar::eventPlay, m_toolbar, &NauSceneEditorViewportToolbar::handlePlaymodeOn);
    mainWindow.connect(mainWindow.toolbar(), &NauMainToolbar::eventStop, m_toolbar, &NauSceneEditorViewportToolbar::handlePlaymodeOff);
    mainWindow.connect(m_viewport, &NauViewportWidget::eventEscPressed, mainWindow.toolbar(), &NauMainToolbar::handleStop);

    // Bind viewport shortcut operations to outliner
    // TODO: Bind this operations to scene
    mainWindow.connect(mainWindow.viewport(), &NauViewportContainerWidget::eventDeleteRequested, &mainWindow.outliner()->outlinerTab(), &NauWorldOutlineTableWidget::deleteItems);
    mainWindow.connect(mainWindow.viewport(), &NauViewportContainerWidget::eventDuplicateRequested, &mainWindow.outliner()->outlinerTab(), &NauWorldOutlineTableWidget::duplicateItems);
    mainWindow.connect(mainWindow.viewport(), &NauViewportContainerWidget::eventCopyRequested, &mainWindow.outliner()->outlinerTab(), &NauWorldOutlineTableWidget::copyItems);

    //connect(m_viewport, &NauViewportContainerWidget::eventCutRequested, &mainWindow.outliner()->getDefaultOutlinerTab(), &NauWorldOutlineTableWidget::cutItems);
    mainWindow.connect(mainWindow.viewport(), &NauViewportContainerWidget::eventPasteRequested, &mainWindow.outliner()->outlinerTab(), &NauWorldOutlineTableWidget::pasteItems);

    // Subscribe to toolbar settings changing
    m_mainEditor->mainWindow().connect(m_toolbar, &NauSceneEditorViewportToolbar::eventCoordinateSpaceChanged, [this](bool isLocal) {
        if (m_sceneTools) {
            m_sceneTools->setSceneToolsCoordinateSpace(isLocal ? GizmoCoordinateSpace::Local : GizmoCoordinateSpace::World);
        }
    });
    
}

// Temp solution
// TODO: Need normal editor activation/deactivation
void NauUsdSceneEditor::initViewportTools()
{
    m_sceneTools = std::make_shared<NauSceneEditorViewportTools>(m_selectionContainer);
    m_cameraController = std::make_shared<NauSceneCameraController>();
    auto dragDropTools = std::make_shared<NauSceneDragDropTools>();

    auto viewportController = std::make_shared<NauBaseEditorViewportController>(m_viewport, m_sceneTools, m_cameraController, dragDropTools);
    viewportController->enableDrawGrid(true);
    viewportController->setSelectionCallback([this](QMouseEvent* event, float dpi) {
         selectObject(event, dpi);
    });

    m_viewport->changeViewportController(viewportController);

    NauTransformTool::StopUsingDelegate.deleteCallback(m_transformToolCallbackId);
    m_transformToolCallbackId = NauTransformTool::StopUsingDelegate.addCallback([this](const pxr::SdfPath& primPath, const pxr::GfMatrix4d& originalTransform, const pxr::GfMatrix4d& newTransform) {
        NauUITranslatorNotificationBlock block(m_uiTranslator.get());
        const pxr::TfToken transformToken("xformOp:transform");
        
        const pxr::VtValue originalTransformValue(originalTransform);
        const pxr::VtValue newTransformValue(newTransform);

        m_sceneUndoRedoSystem->addCommand<NauCommandChangeUsdPrimProperty, false>(primPath, transformToken, originalTransformValue, newTransformValue);
    });
}

void NauUsdSceneEditor::onSceneLoaded(pxr::UsdStageRefPtr scene, NauUsdSceneSynchronizer::SyncMode syncMode)
{
    m_selectionContainer->clear(false);
    m_uiTranslator.reset();
    m_outlinerClient->clearItems();

    m_sceneUndoRedoSystem->bindCurrentScene(scene);
    m_outlinerClient->updateItemsFromScene(scene);

    m_uiTranslator = std::make_unique<NauUsdSceneUITranslator>(scene);
    m_uiTranslator->addClient(m_outlinerClient);
    m_uiTranslator->addClient(m_inspectorClient);

    m_sceneSyncMode = syncMode;

    if (m_sceneSyncMode == NauUsdSceneSynchronizer::SyncMode::FromEditor) {
        m_sceneSynchronizer->startSyncFromEditorScene(scene);
    } else {
        const std::string coreScenePath = currentCoreScenePath();
        m_sceneSynchronizer->startSyncFromEngineScene(scene, coreScenePath);
    }
}

void NauUsdSceneEditor::onSceneUnloaded()
{
    m_selectionContainer->clear(false);
    m_sceneUndoRedoSystem->unbindCurrentScene();
    m_uiTranslator.reset();

    if (m_sceneSyncMode == NauUsdSceneSynchronizer::SyncMode::FromEditor) {
        m_sceneSynchronizer->startSyncFromEditorScene(nullptr);
    } else {
        m_sceneSynchronizer->startSyncFromEngineScene(nullptr, "");
    }

    m_sceneSyncMode = NauUsdSceneSynchronizer::SyncMode::None;
}

std::shared_ptr<NauEditorSceneManagerInterface> NauUsdSceneEditor::sceneManager()
{
    return m_sceneManager;
}

std::shared_ptr<NauOutlinerClientInterface> NauUsdSceneEditor::outlinerClient()
{
    return m_outlinerClient;
}

PXR_NS::UsdPrim NauUsdSceneEditor::createPrim(const PXR_NS::SdfPath& parentPath, const PXR_NS::TfToken& name, const PXR_NS::TfToken& typeName, bool isComponent, std::string& uniquePath)
{
    NauUITranslatorNotificationAccumulator accumulator(m_uiTranslator.get());

    // TODO: Try to do it without cast
    auto usdSceneManager = dynamic_cast<NauUsdSceneManager*>(this->sceneManager().get());
    uniquePath = NauUsdSceneUtils::generateUniquePrimPath(usdSceneManager->currentScene(), parentPath.GetString(), name);
        
    // Take the transform in front of the camera
    auto activeCamera = Nau::EditorEngine().cameraManager()->activeCamera();
    const nau::math::Transform cameraTransform = activeCamera->getWorldTransform();
    nau::math::Transform spawnTransform = cameraTransform;
    const nau::math::vec3 eyeDirection = Vectormath::SSE::rotate(activeCamera->getRotation(), nau::math::vec3::zAxis());
    spawnTransform.setTranslation(cameraTransform.getTranslation() - eyeDirection * 15.0f);
    spawnTransform.setRotation(nau::math::Quat::identity());

    // Convert to local transform
    auto initialTransform = NauUsdEditorMathUtils::nauMatrixToGfMatrix(spawnTransform.getMatrix());
    auto parentPrim = usdSceneManager->currentScene()->GetPrimAtPath(parentPath);
    if (parentPrim && parentPrim.IsValid()) {
        initialTransform = NauUsdPrimUtils::relativeTransform(parentPrim, initialTransform);
    }

    const std::string displayName = name;
    m_sceneUndoRedoSystem->addCommand<NauCommandCreateUsdPrim>(uniquePath, typeName, name, initialTransform, isComponent);

    return usdSceneManager->currentScene()->GetPrimAtPath(PXR_NS::SdfPath(uniquePath));
}

void NauUsdSceneEditor::addPrimsFromOther(const std::vector<PXR_NS::UsdPrim>& prims)
{
    // Need to avoid infinite recursion, if we added a copied objects as childs of one of copied objects
    pxr::SdfPathSet primPaths;
    NauUsdEditorUtils::collectPrimsWithChildrensPaths(prims, primPaths);
    for (auto prim : prims) {
        PXR_NS::SdfPath parentPath = prim.GetParent().IsValid() ? prim.GetParent().GetPath() : PXR_NS::SdfPath::EmptyPath();
        addPrimFromOther(primPaths, prim, parentPath);
    }
}

void NauUsdSceneEditor::addPrimsFromOther(const std::vector<PXR_NS::UsdPrim>& prims, const PXR_NS::SdfPath& pathToAdd)
{
    // Need to avoid infinite recursion, if we added a copied objects as childs of one of copied objects
    pxr::SdfPathSet primPaths;
    NauUsdEditorUtils::collectPrimsWithChildrensPaths(prims, primPaths);
    for (auto prim : prims) {
        addPrimFromOther(primPaths, prim, pathToAdd);
    }
}

void NauUsdSceneEditor::addPrimFromOther(const pxr::SdfPathSet& primsPaths, PXR_NS::UsdPrim other, const PXR_NS::SdfPath& parentPath)
{
    // Need to avoid infinite recursion, if we added a copied objects as childs of one of copied objects
    if (!primsPaths.contains(other.GetPath())) {
        return;
    }

    std::string uniquePath = "";
    PXR_NS::UsdPrim newPrim = createPrim(parentPath, other.GetName(), other.GetTypeName(), NauUsdPrimUtils::isPrimComponent(other), uniquePath);

    for (auto childPrim : other.GetAllChildren()) {
        addPrimFromOther(primsPaths, childPrim, newPrim.GetPath());
    }
}

void NauUsdSceneEditor::removePrims(const std::vector<PXR_NS::UsdPrim>& normalizedPrimList)
{
    for (const pxr::UsdPrim& prim : normalizedPrimList) {
        removePrim(prim);
    }
}

void NauUsdSceneEditor::removePrim(const PXR_NS::UsdPrim& prim)
{
    for (auto childPrim : prim.GetAllChildren()) {
        removePrim(childPrim);
    }

    auto proxyPrim = UsdProxy::UsdProxyPrim(prim);

    auto type = proxyPrim.getProperty(pxr::TfToken("componentTypeName"));

    if (type && type->isValid()) {
        PXR_NS::VtValue value;
        type->getValue(&value);

        auto engineTypeName = value.Get<std::string>();

        // TODO: Dirty hack
        // Fix in future updates
        if (engineTypeName == "nau::scene::EnvironmentComponent") {
            auto outlinerWidget = m_mainEditor->mainWindow().outliner();
            outlinerWidget->updateCreationList(engineTypeName, false);
        }
    }

    m_sceneUndoRedoSystem->addCommand<NauCommandRemoveUsdPrim>(prim.GetPath().GetString());
}

std::string NauUsdSceneEditor::currentCoreScenePath()
{
    // read scene uid
    const std::string sceneMetaFilePath = std::format("{}.nausd", m_sceneManager->currentScene()->GetRootLayer()->GetRealPath());
    auto sceneMetaStage = pxr::UsdStage::Open(sceneMetaFilePath);
    auto root = sceneMetaStage->GetPseudoRoot();
    auto children = root.GetAllChildren();
    if (children.empty()) {
        return "";
    }
    auto sceneMetaPrim = children.front();

    UsdProxy::UsdProxyPrim proxyPrim(sceneMetaPrim);
    auto proxyProp = proxyPrim.getProperty(pxr::TfToken("uid"));
    if (!proxyProp) {
        return "";
    }

    pxr::VtValue uidVal;
    proxyProp->getValue(&uidVal);
    auto coreSceneUid = "uid:" + uidVal.Get<std::string>();

    return coreSceneUid;
}
