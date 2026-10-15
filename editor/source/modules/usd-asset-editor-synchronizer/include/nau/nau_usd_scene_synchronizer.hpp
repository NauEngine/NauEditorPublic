// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Provides bidirectional synchronization between UsdStage and nau engine scene

#pragma once

#include "nau/nau_usd_scene_synchronizer_config.hpp"
#include "usd_translator/usd_translator.h"

#include "nau/scene/internal/scene_listener.h"
#include "nau/scene/internal/scene_manager_internal.h"

#include "memory"
#include <EASTL/unique_ptr.h>
#include <vector>

namespace nau::editor
{
    class CameraVisuals;
}

// ** NauEngineSceneListener

class NAU_USD_SCENE_SYNCHRONIZER_API NauEngineSceneListener final : public nau::scene::ISceneListener
{
public:
    NauEngineSceneListener(pxr::UsdStageRefPtr editorScene, nau::scene::IScene::WeakRef engineScene);

private:
    void onSceneBegin() override;
    void onSceneEnd() override;
    void onAfterActivatingObjects(eastl::span<const nau::scene::SceneObject*> objects) override;
    void onBeforeDeletingObjects(eastl::span<const nau::scene::SceneObject*> objects) override;
    void onAfterActivatingComponents(eastl::span<const nau::scene::Component*> components) override;
    void onBeforeDeletingComponents(eastl::span<const nau::scene::Component*> components) override;
    void onComponentsChange(eastl::span<const nau::scene::Component*> components) override;

    void onActivateObject(const nau::scene::SceneObject* object);

private:
    std::unordered_map<nau::Uid, pxr::SdfPath> m_objectsMap;

    pxr::UsdStageRefPtr m_editorCurrentScene;
    nau::scene::IScene::WeakRef m_engineCurrentScene;
};


// ** NauUsdSceneSynchronizer
// TODO: Make an interface for the scene synchronizer

class NAU_USD_SCENE_SYNCHRONIZER_API NauUsdSceneSynchronizer
{
public:
    enum class SyncMode
    {
        None,
        FromEditor,
        FromEngine
    };

    // TODO: Get the list from the engine
    enum BillboardTypes
    {
        // Lights
        SphereLight,
        DirectionalLightComponent,
        // VFX
        VFXInstance,
        // Camera
        CameraComponent,
        // Audio
        AudioComponentEmitter,
        AudioComponentListener,
        AudioEmitter,

        // In case magic_enim fails to bring a value
        Unrecognized
    };

    NauUsdSceneSynchronizer();
    ~NauUsdSceneSynchronizer();

    void startSyncFromEditorScene(pxr::UsdStageRefPtr editorScene);
    void startSyncFromEngineScene(pxr::UsdStageRefPtr emptyEditorScene, const std::string& coreScenePath);
    void stopCameraVisuals();

    nau::scene::SceneObject::WeakRef sceneObjectByPrimPath(const pxr::SdfPath& path) const;
    pxr::SdfPath primFromSceneObject(const nau::Uid& uid);
    pxr::SdfPath primFromCameraMesh(const nau::Uid& uid) const;
    std::vector<nau::Uid> cameraMeshUidsForPrim(const pxr::SdfPath& path) const;

    // TODO: Move to controller
    void focusOnObject(const pxr::SdfPath& primPath) const;
    void addBillboard(const pxr::SdfPath& primPath, const std::string& primType) const;

    UsdTranslator::StageTranslator* translator() const;

private:
    void startSyncFromEditorSceneInternal(pxr::UsdStageRefPtr editorScene);
    void stopSyncFromEditorSceneInternal();

    void startSyncFromEngineSceneInternal(pxr::UsdStageRefPtr emptyEditorScene, const std::string& editorScene);
    void stopSyncFromEngineSceneInternal();

    void unloadEngineScene();

private:
    pxr::UsdStageRefPtr m_editorCurrentScene;
    nau::scene::IScene::WeakRef m_engineCurrentScene;

    std::shared_ptr<UsdTranslator::StageTranslator> m_sceneTranslator;
    eastl::unique_ptr<nau::editor::CameraVisuals> m_cameraVisuals;
    std::unique_ptr<UsdProxy::StageObjectChangedWatcher> m_cameraTransformWatcher;

    std::shared_ptr<NauEngineSceneListener> m_sceneListener;
    nau::scene::SceneListenerRegistration m_sceneListenerReg;

    SyncMode m_syncMode;

    std::unordered_map<BillboardTypes, std::string> m_billboardIcons;
};
