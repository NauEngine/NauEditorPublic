// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Material editing classes

#pragma once

#include "nau/rtti/rtti_impl.h"
#include "nau/assets/nau_asset_editor.hpp"
#include "nau/assets/nau_asset_manager_client.hpp"
#include "nau/inspector/nau_usd_inspector_client.hpp"
#include "nau/inspector/nau_inspector.hpp"
#include "nau/undo-redo/nau_usd_scene_undo_redo.hpp"
#include "nau/app/nau_editor_interface.hpp"
#include "nau_dock_manager.hpp"
#include "nau/scene/world.h"
#include "pxr/usd/usd/stage.h"
#include "usd_translator/usd_stage_translator.h"
#include "nau/scene/camera/camera.h"

#include <memory>


class NauMaterialPreview;


// ** NauMaterialEditor
//
// Material editor instance. Include needed widgets, material editing logic etc.

class NauMaterialEditor final : public NauAssetEditorInterface
                              , public NauAssetManagerClientInterface

{
    NAU_CLASS_(NauMaterialEditor, NauAssetEditorInterface)

public:
    NauMaterialEditor();
    ~NauMaterialEditor();

    // TODO: Implement
    void initialize(NauEditorInterface* mainEditor) override;
    void terminate() override;
    void postInitialize() override;
    void preTerminate() override;

    // NauAssetEditorInterface overrides
    void createAsset(const std::string& assetPath) override;
    bool openAsset(const std::string& assetPath) override;
    bool saveAsset(const std::string& assetPath) override;

    [[nodiscard]] std::string editorName() const override;
    [[nodiscard]] NauEditorFileType assetType() const override;

    // NauAssetManagerClientInterface overrides
    void handleSourceAdded(const std::string& path) override;
    void handleSourceRemoved(const std::string &path) override;
    void resetCameraPosition();

  private:
    NauMaterialEditor(const NauMaterialEditor&) = default;
    NauMaterialEditor(NauMaterialEditor&&) = default;
    NauMaterialEditor& operator=(const NauMaterialEditor&) = default;
    NauMaterialEditor& operator=(NauMaterialEditor&&) = default;

    void openEditorPanel();
    void createEditorPanel();
    void initInspectorClient();
    void openPreviewScene();
    void refreshPreviewMeshMaterial();

    void loadMaterialData(const QString& assetPath, NauInspectorPage& inspector);
    void onMaterialUnloaded();

private:
    NauEditorInterface* m_mainEditor;
    NauInspectorPage* m_mainInspector;
    NauInspectorPage* m_inspectorWithMaterial;
    NauDockManager* m_editorDockManager;
    NauDockWidget* m_dwMaterialPropertyPanel = nullptr;
    NauWidget* m_editorPanel = nullptr;
    NauViewportContainerWidget* m_viewportContainer = nullptr;
    NauDockWidget* m_dwEditorPanel = nullptr;
    NauDockManager* m_materialEditorDockManager = nullptr;

    nau::scene::IWorld::WeakRef m_coreWorld;
    nau::scene::IScene::WeakRef m_enginePreviewScene;
    pxr::UsdStageRefPtr m_previewStage;
    std::shared_ptr<UsdTranslator::StageTranslator> m_stageTranslator;
    nau::Ptr<nau::scene::ICameraControl> m_cameraControl;

    std::shared_ptr<NauUsdInspectorClient> m_inspectorClient;
    NauUsdSceneUndoRedoSystemPtr m_sceneUndoRedoSystem;

    pxr::UsdStageRefPtr m_materialAsset;
    std::string m_materialAssetPath;
};
