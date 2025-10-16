// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Usd scene editor implementation. Contains selection, undo/redo system, scene manager and UI clients

#pragma once

#include "nau/nau_usd_scene_editor.hpp"
#include "nau/outliner/nau_usd_outliner_client.hpp"
#include "nau/inspector/nau_usd_inspector_client.hpp"
#include "nau/scene-manager/nau_usd_scene_manager.hpp"
#include "nau/undo-redo/nau_usd_scene_undo_redo.hpp"
#include "nau/selection/nau_usd_selection_container.hpp"
#include "nau/ui-translator/nau_usd_scene_ui_translator.hpp"
#include "viewport/nau_viewport_scene_editor_tools.hpp"
#include "viewport/nau_scene_camera_controller.hpp"

#include "widgets/nau_scene_editor_viewport_toolbar.hpp"

#include "nau/scene/nau_scene_editor_interface.hpp"
#include "nau/app/nau_editor_interface.hpp"

#include "nau/rtti/rtti_impl.h"

#include <memory>


// ** NauUsdSceneEditorSnapshot

class NauUsdSceneEditorSnapshot
{
    struct Info
    {
        QMatrix4x3 transform;
        QVector2D clipPlanes;
        float fov;
        bool easing;
        bool acceleration;
        float speed;
    };

public:
    NauUsdSceneEditorSnapshot() = default;
    ~NauUsdSceneEditorSnapshot() = default;

    void takeShapshot(class NauUsdSceneEditor* sceneEditor);
    void restoreShapshot();

private:
    std::unique_ptr<Info> m_snapshotInfo;
    class NauUsdSceneEditor* m_sceneEditor;
};
using NauUsdSceneEditorSnapshotPtr = std::unique_ptr<NauUsdSceneEditorSnapshot>;


// ** NauUsdSceneEditor

class NauUsdSceneEditor : public NauUsdSceneEditorInterface
{
    NAU_CLASS_(NauUsdSceneEditor, NauUsdSceneEditorInterface)
    friend class NauUsdSceneEditorSnapshot;
public:
    NauUsdSceneEditor();
    ~NauUsdSceneEditor();

    // Asset editor overrides
    void initialize(NauEditorInterface* mainEditor) override;
    void terminate() override;
    void postInitialize() override;
    void preTerminate() override;
    void createAsset(const std::string& assetPath) override { /* TODO */ }
    bool openAsset(const std::string& assetPath) override;
    bool saveAsset(const std::string& assetPath) override;
    std::string editorName() const override;
    NauEditorFileType assetType() const override;

    // Scene editor overrides
    std::shared_ptr<NauEditorSceneManagerInterface> sceneManager() override;
    std::shared_ptr<NauOutlinerClientInterface> outlinerClient() override;
    
    // Usd scene editor overrides
    NauUsdSelectionContainerPtr selectionContainer() const noexcept override;
    const NauUsdSceneSynchronizer& sceneSynchronizer() const noexcept override;
    NauCameraControllerInterfacePtr sceneCameraController() const noexcept override;

    void changeMode(bool isPlaymode) override;
    PXR_NS::UsdPrim createPrim(const PXR_NS::SdfPath& parentPath, const PXR_NS::TfToken& name, const PXR_NS::TfToken& typeName, bool isComponent, std::string& uniquePath) override;

private:
    NauUsdSceneEditor(const NauUsdSceneEditor&) = default;
    NauUsdSceneEditor(NauUsdSceneEditor&&) = default;
    NauUsdSceneEditor& operator=(const NauUsdSceneEditor&) = default;
    NauUsdSceneEditor& operator=(NauUsdSceneEditor&&) = default;

    void initPrimFactory();
    void initSceneManager();
    void initSelectionContainer();
    void initSceneUndoRedo();
    void initInspectorClient();
    void initOutlinerClient();
    void initViewportWidget();

    // TODO: Move to viewport client interface
    void initViewportTools();
    void selectObject(QMouseEvent* event, float dpi);

    void onSceneLoaded(pxr::UsdStageRefPtr scene, NauUsdSceneSynchronizer::SyncMode sceneSyncMode);
    void onSceneUnloaded();
    
    void addPrimsFromOther(const std::vector<PXR_NS::UsdPrim>& prims);
    void addPrimsFromOther(const std::vector<PXR_NS::UsdPrim>& prims, const PXR_NS::SdfPath& pathToAdd);
    void addPrimFromOther(const pxr::SdfPathSet& primsPaths, PXR_NS::UsdPrim other, const PXR_NS::SdfPath& parentPath);

    void removePrims(const std::vector<PXR_NS::UsdPrim>& normalizedPrimList);
    void removePrim(const PXR_NS::UsdPrim& prim);

    std::string currentCoreScenePath();

private:
    NauEditorInterface* m_mainEditor;
    std::shared_ptr<NauUsdSceneManager> m_sceneManager;
    std::shared_ptr<NauUsdOutlinerClient> m_outlinerClient;
    std::shared_ptr<NauUsdInspectorClient> m_inspectorClient;

    std::vector<std::pair<std::string, std::string>> m_outlinerObjectsList;
    std::vector<std::string> m_inspectorObjectsList;

    NauUsdSceneUndoRedoSystemPtr m_sceneUndoRedoSystem;
    NauUsdSelectionContainerPtr m_selectionContainer;

    std::unique_ptr<NauUsdSceneUITranslator> m_uiTranslator;

    // Viewport sync
    std::shared_ptr<NauUsdSceneSynchronizer> m_sceneSynchronizer;
    NauCallbackId m_transformToolCallbackId;

    // Viewport tools
    NauCameraControllerInterfacePtr m_cameraController;
    std::shared_ptr<NauSceneEditorViewportTools> m_sceneTools;

    //Widgets
    NauViewportWidget* m_viewport;
    NauSceneEditorViewportToolbar* m_toolbar;
    
    NauUsdSceneSynchronizer::SyncMode m_sceneSyncMode;
    NauUsdSceneEditorSnapshotPtr m_snapshot;
};
