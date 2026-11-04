// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>
#include <QRect>

#include <functional>
#include <optional>

#include "nau/async/task.h"
#include "nau/scene/components/billboard_component.h"
#include "nau/scene/components/camera_component.h"
#include "nau/scene/components/static_mesh_component.h"
#include "nau/scene/internal/scene_listener.h"
#include "nau/scene/internal/scene_manager_internal.h"
#include "nau/scene/scene.h"

namespace nau::editor
{
    // owns camera meshes in a separate editor scene without USD prims
    class CameraVisuals final : public scene::ISceneListener
    {
    public:
        using TransformResolver = std::function<math::Transform(const scene::CameraComponent&)>;

        struct PreviewCamera
        {
            Uid uid = NullUid;
            std::optional<math::Transform> worldPose;
        };

        explicit CameraVisuals(scene::IScene& scene, TransformResolver transformResolver = {});
        CameraVisuals(const CameraVisuals&) = delete;
        CameraVisuals(CameraVisuals&&) = delete;
        CameraVisuals& operator=(const CameraVisuals&) = delete;
        CameraVisuals& operator=(CameraVisuals&&) = delete;
        ~CameraVisuals() override = default;

        async::Task<> activate();
        async::Task<> shutdown();
        void updateTransforms();
        void setPreviewCamera(std::function<PreviewCamera()> resolver);
        void trackEditorBillboard(scene::BillboardComponent& billboard);

        Uid cameraUidForMesh(Uid meshUid) const;
        Uid meshUidForCamera(Uid cameraUid) const;

    private:
        struct Visual
        {
            scene::ObjectWeakRef<scene::CameraComponent> camera;
            scene::ObjectWeakRef<scene::StaticMeshComponent> mesh;
            async::Task<scene::SceneObject::WeakRef> activation;
            bool dirty = true;
        };

        void trackCamera(const scene::Component& component);
        void updateVisuals();
        scene::SceneObject::Ptr createVisual(Visual& visual);
        void updateTransform(Visual& visual);
        void updatePreview();

        void onSceneBegin() override;
        void onSceneEnd() override;
        void onAfterActivatingObjects(eastl::span<const scene::SceneObject*> objects) override;
        void onBeforeDeletingObjects(eastl::span<const scene::SceneObject*> objects) override;
        void onAfterActivatingComponents(eastl::span<const scene::Component*> components) override;
        void onBeforeDeletingComponents(eastl::span<const scene::Component*> components) override;
        void onComponentsChange(eastl::span<const scene::Component*> components) override;

    private:
        scene::IScene::WeakRef m_scene;
        scene::IScene::WeakRef m_visualScene;
        TransformResolver m_transformResolver;
        std::function<PreviewCamera()> m_resolvePreviewCamera;
        Uid m_previewCameraUid = NullUid;
        QRect m_previewRect;
        std::optional<math::Transform> m_submittedPose;
        bool m_previewDirty = false;
        bool m_running = false;
        eastl::unordered_map<Uid, Visual> m_visuals;
        eastl::vector<scene::ObjectWeakRef<scene::BillboardComponent>> m_editorBillboards;
        // unregister before destroying the state used by callbacks
        scene::SceneListenerRegistration m_registration;
    };
}  // namespace nau::editor
