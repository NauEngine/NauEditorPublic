// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_material_editor.hpp"

#include "nau_material_preview.hpp"
#include "nau_log.hpp"
#include "nau_assert.hpp"

#include "nau/undo-redo/nau_usd_scene_commands.hpp"

#include "nau/usd_meta_tools/usd_meta_generator.h"

#include <QFileInfo>

// TODO: Temporary. Needed for update materials in all prims with this material
#include "nau/nau_usd_scene_editor.hpp"

#include "nau/assets/asset_descriptor.h"
#include "nau/assets/asset_manager.h"
#include "nau/editor-engine/nau_editor_engine_services.hpp"
#include "nau/prim-factory/nau_usd_prim_factory.hpp"
#include "nau/scene/scene_factory.h"
#include "nau/scene/scene_manager.h"
#include "nau/scene/camera/camera_manager.h"

#include "pxr/usd/usd/attribute.h"


// ** NauMaterialEditor

NauMaterialEditor::NauMaterialEditor()
    : m_inspectorWithMaterial(nullptr)
{
}

NauMaterialEditor::~NauMaterialEditor()
{
    terminate();
}

void NauMaterialEditor::initialize(NauEditorInterface* mainEditor)
{
    m_mainEditor = mainEditor;
    m_editorDockManager = mainEditor->mainWindow().dockManager();

    m_sceneUndoRedoSystem = std::make_shared<NauUsdSceneUndoRedoSystem>(m_mainEditor->undoRedoSystem());

    auto& sceneManager = nau::getServiceProvider().get<nau::scene::ISceneManager>();
    m_coreWorld = sceneManager.createWorld();
}

void NauMaterialEditor::terminate()
{
    if (m_sceneUndoRedoSystem) {
        m_sceneUndoRedoSystem->unbindCurrentScene();
    }

    if (m_materialAsset) {
        m_materialAsset.Reset();
    }

    //TODO: reset more stuff
}

void NauMaterialEditor::postInitialize()
{
    // TODOL ui initialize
}

void NauMaterialEditor::preTerminate()
{
    // TODOL unbind ui from NauEditor window
}

void NauMaterialEditor::createAsset(const std::string& assetPath)
{
    nau::UsdMetaGenerator::instance().generateAssetTemplate(assetPath, "Material", nau::MetaArgs());
    if (!std::filesystem::exists(assetPath)) {
        NED_ERROR("Failed to create material asset.");
    }
}

bool NauMaterialEditor::openAsset(const std::string& assetPath)
{
    openEditorPanel();
    loadMaterialData(QString(assetPath.c_str()), *m_mainInspector);

    NED_DEBUG("Material asset {} opened.", assetPath);
    return true;
}

bool NauMaterialEditor::saveAsset(const std::string& assetPath)
{
    if (!m_materialAsset) {
        NED_ERROR("Trying to save non-existent material asset");
        return false;
    }

    m_materialAsset->Save();   

    // Reimport asset process
    // TODO: Temp solutiuon. Will be deleted
    auto root = m_materialAsset->GetPseudoRoot();
    auto children = root.GetAllChildren();
    if (children.empty()) {
        return false;
    }
    auto materialPrim = children.front();

    UsdProxy::UsdProxyPrim proxyPrim(materialPrim);
    auto proxyProp = proxyPrim.getProperty(pxr::TfToken("uid"));
    if (!proxyProp) {
        return false;
    }
    pxr::VtValue uidVal;
    proxyProp->getValue(&uidVal);
    auto coreMatPath = "uid:" + uidVal.Get<std::string>();
    
    // Unload asset from engine
    nau::IAssetDescriptor::Ptr asset = nau::getServiceProvider().get<nau::IAssetManager>().openAsset(nau::strings::toStringView(coreMatPath));
    nau::IAssetDescriptor::LoadState state = asset->getLoadState();

    nau::IAssetDescriptor::UnloadResult unloadResult = nau::IAssetDescriptor::UnloadResult::Unloaded;
    if (state == nau::IAssetDescriptor::LoadState::Ready) {    
        unloadResult = asset->unload();
    }
    
    // Reimport material
    m_mainEditor->assetManager()->importAsset(assetPath);

    // Load material again if needed
    state = asset->getLoadState();
    if ((unloadResult == nau::IAssetDescriptor::UnloadResult::UnloadedHasReferences) && (state == nau::IAssetDescriptor::LoadState::None)) {
        asset->load();
    }

    // TODO: Temporary. Reload materials on prims with current material
    // Need to change engine asset directly 
    auto& mainSceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
    auto translator = mainSceneEditor.sceneSynchronizer().translator();

    std::function<void(UsdTranslator::IPrimAdapter::Ptr)> updatePrimMaterial;
    std::filesystem::path currentAssetPath = m_materialAssetPath;
    updatePrimMaterial = [currentAssetPath, &updatePrimMaterial, translator](UsdTranslator::IPrimAdapter::Ptr adapter) {
        if (adapter->getType() == "AssetMesh") {
            if (pxr::VtValue materialPath; UsdProxy::UsdProxyPrim(adapter->getPrim()).getProperty("Material:assign"_tftoken)->getValue(&materialPath))
            {
                if(materialPath.CanCast<pxr::SdfAssetPath>())
                {
                    std::filesystem::path assetPath = materialPath.Get<pxr::SdfAssetPath>().GetResolvedPath();
                    if (assetPath == currentAssetPath) {
                        translator->forceUpdate(adapter->getPrim());
                    }
                }
            }
            
        }

        for (auto child : adapter->getChildren()) {
            updatePrimMaterial(child.second);
        }
    };
    updatePrimMaterial(translator->getRootAdapter());

    NED_TRACE("Material asset saved to {}.", assetPath);
    return true;
}

std::string NauMaterialEditor::editorName() const
{
    return "Material editor";
}

NauEditorFileType NauMaterialEditor::assetType() const
{
    return NauEditorFileType::Material;
}

void NauMaterialEditor::handleSourceAdded(const std::string& path)
{
    // Unused
}

void NauMaterialEditor::handleSourceRemoved(const std::string& assetPath)
{
    if (m_materialAssetPath == assetPath)
    {
        if (m_materialAsset) {
            onMaterialUnloaded();
        }

        if (m_inspectorWithMaterial) {
            m_inspectorWithMaterial->clear();
        }
    }
}

void NauMaterialEditor::resetCameraPosition()
{
  m_cameraControl->setWorldTransform(nau::math::Transform(
      Vectormath::Quat(30, -30, 0), Vectormath::Vector3(1.f, 2.f, 2.f)));
}

void NauMaterialEditor::openEditorPanel()
{
    if (m_dwEditorPanel) {
        m_dwEditorPanel->toggleView(true);
        resetCameraPosition();
        return;
    }

    createEditorPanel();
    createPreviewScene();
    initInspectorClient();

    auto viewportManager = Nau::EditorEngine().viewportManager();
    auto viewport = viewportManager->createViewport(editorName().data());
    m_viewportContainer->setViewport(viewport);
    viewport->changeViewportController(std::make_shared<NauBaseEditorViewportController>(viewport, nullptr, nullptr, nullptr));
    viewportManager->setViewportRendererWorld(editorName().data(), m_coreWorld->getUid());
    resetCameraPosition();
}

void NauMaterialEditor::createEditorPanel()
{
    if (m_editorPanel) {
        return;
    }

    m_editorPanel = new NauWidget;
    auto layout = new NauLayoutHorizontal(m_editorPanel);
    const QSize minSize{1280, 720};
    m_editorPanel->setMinimumSize(minSize);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_editorPanel->setLayout(layout);

    m_inspectorWithMaterial = new NauInspectorPage(m_editorPanel);
    m_viewportContainer = new NauViewportContainerWidget(m_editorPanel);

    layout->addWidget(m_viewportContainer, Qt::AlignCenter);
    layout->addWidget(m_inspectorWithMaterial, Qt::AlignRight);

    m_materialEditorDockManager = new NauDockManager(m_editorPanel);
    layout->addWidget(m_materialEditorDockManager);

    // Viewport
    auto dwSceneViewPort = new NauDockWidget(QObject::tr("Viewport"), nullptr);
    dwSceneViewPort->setWidget(m_viewportContainer);
    dwSceneViewPort->setFeature(ads::CDockWidget::DockWidgetAloneHasNoTitleBar, true);
    m_materialEditorDockManager->addDockWidget(ads::CenterDockWidgetArea, dwSceneViewPort);

    // Inspector
    auto dwInspector = new NauDockWidget(QObject::tr("Material Properties"), nullptr);
    dwInspector->setWidget(m_inspectorWithMaterial);
    dwInspector->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContentMinimumSize);
    m_materialEditorDockManager->addDockWidget(ads::RightDockWidgetArea, dwInspector);

    // Add to global dock manager
    m_dwEditorPanel = new NauDockWidget(QObject::tr(editorName().c_str()), nullptr);
    m_dwEditorPanel->setStyleSheet("background-color: #282828");
    m_dwEditorPanel->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);

    m_dwEditorPanel->setWidget(m_editorPanel);
    m_editorDockManager->addDockWidgetFloating(m_dwEditorPanel);
    m_dwEditorPanel->resize(1280, 720);
    m_dwEditorPanel->toggleView(true);
}

void NauMaterialEditor::initInspectorClient()
{
    m_inspectorClient = std::make_shared<NauUsdInspectorClient>(m_inspectorWithMaterial);
    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventPropertyChanged, [this](const PXR_NS::SdfPath& path, const PXR_NS::TfToken propName, const PXR_NS::VtValue& value) {
        m_sceneUndoRedoSystem->addCommand<NauCommandChangeUsdPrimProperty>(path, propName, value);

        saveAsset(m_materialAssetPath);
    });

    m_inspectorClient->connect(m_inspectorClient.get(), &NauUsdInspectorClient::eventAssetReferenceChanged, [this](const PXR_NS::SdfPath& path, const PXR_NS::VtValue& value) {
        m_sceneUndoRedoSystem->addCommand<NauCommandChangeUsdPrimAssetReference>(path, value);

        saveAsset(m_materialAssetPath);
    });
}

void NauMaterialEditor::createPreviewScene()
{
    // TODO: move to other class?

    m_previewStage = pxr::UsdStage::CreateInMemory("Material.usda");
    auto cubePath = pxr::SdfPath("/PreviewMesh");
    pxr::GfMatrix4d transform;
    transform.SetIdentity();
    auto prim = NauUsdPrimFactory::instance().createPrim(
        m_previewStage, cubePath, pxr::TfToken("NauAssetMesh"), "NauAssetMesh",
        transform, false);
    m_previewStage->SetDefaultPrim(prim);

    auto sceneCreateTask = [this]() -> nau::async::Task<>
    {
        auto engineScene = nau::getServiceProvider()
                               .get<nau::scene::ISceneFactory>()
                               .createEmptyScene();
        engineScene->setName("MaterialPreviewScene");
        m_stageTranslator = std::make_unique<UsdTranslator::StageTranslator>();
        m_stageTranslator->setSource(m_previewStage);
        m_stageTranslator->setTarget(*engineScene);
        co_await m_stageTranslator->initScene();
        m_stageTranslator->follow();

        // Create camera
        auto& cameraManager =
            nau::getServiceProvider().get<nau::scene::ICameraManager>();

        // Create detached camera in preview world
        m_cameraControl = cameraManager.createDetachedCamera(m_coreWorld->getUid());
        cameraManager.setMainCamera(m_cameraControl->getCameraUid());

        // Set initial camera params
        // Will be overwritten
        m_cameraControl->setClipNearPlane(0.1);
        m_cameraControl->setClipFarPlane(1000);
        m_cameraControl->setFov(90);
        m_cameraControl->setCameraName("Preview.Camera");

        auto enginePreviewScene =
            co_await m_coreWorld->addScene(std::move(engineScene));
        m_enginePreviewScene = enginePreviewScene;
    };
    auto result = Nau::EditorEngine().runTaskSync(sceneCreateTask().detach());
}

void NauMaterialEditor::refreshPreviewMeshMaterial()
{
    if (m_previewStage)
    {
        auto previewMeshPrim = m_previewStage->GetDefaultPrim();
        auto prop = previewMeshPrim.GetProperty("Material:assign"_tftoken);
        if (previewMeshPrim)
        {
            auto materialAttr =
                previewMeshPrim.GetAttribute("Material:assign"_tftoken);
            if (!materialAttr)
            {
                materialAttr = previewMeshPrim.CreateAttribute(
                    "Material:assign"_tftoken, pxr::SdfValueTypeNames->Asset, false);
            }
            pxr::SdfAssetPath materialSdfPath(m_materialAssetPath);
            materialAttr.Set(materialSdfPath);
            if (m_stageTranslator)
            {
                m_stageTranslator->forceUpdate(previewMeshPrim);
            }
        }
    }
}

void NauMaterialEditor::loadMaterialData(const QString& assetPath, NauInspectorPage& inspector)
{
    if (m_materialAsset) {
        onMaterialUnloaded();
    }
    m_inspectorWithMaterial = &inspector;
    m_materialAssetPath = assetPath.toUtf8().constData();

    m_materialAsset = pxr::UsdStage::Open(m_materialAssetPath);

    if (!m_materialAsset) {
        NED_ERROR("Error while opening material asset at path {}", m_materialAssetPath);
        return;
    }

    m_sceneUndoRedoSystem->bindCurrentScene(m_materialAsset);


    auto rootPrim = m_materialAsset->GetPseudoRoot();
    auto children = rootPrim.GetAllChildren();
    if (children.empty()) {
        NED_ERROR("Invalid material");
        return;
    }

    // TODO: Now we can build only from one NauMaterialPipline
    auto materialPipelinePrim = children.front();
    m_inspectorClient->buildFromMaterial(materialPipelinePrim);
    refreshPreviewMeshMaterial();
}

void NauMaterialEditor::onMaterialUnloaded()
{
    m_materialAsset.Reset();

    if (m_sceneUndoRedoSystem) {
        m_sceneUndoRedoSystem->unbindCurrentScene();
    }

    if (m_inspectorClient) {
        m_inspectorClient->clear();
    }
}