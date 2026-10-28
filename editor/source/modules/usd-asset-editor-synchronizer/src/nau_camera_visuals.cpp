// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_camera_visuals.hpp"

#include <EASTL/unordered_set.h>

#include <utility>

#include "nau/scene/scene_factory.h"
#include "nau/scene/scene_manager.h"
#include "nau/scene/scene_object.h"
#include "nau/service/service_provider.h"

namespace nau::editor
{
    CameraVisuals::CameraVisuals(scene::IScene& scene, TransformResolver transformResolver) :
        m_scene(scene),
        m_transformResolver(std::move(transformResolver))
    {
        for (const scene::Component* component : scene.getRoot().getComponents(true))
        {
            trackCamera(*component);
        }
        m_registration = getServiceProvider().get<scene::ISceneManagerInternal>().addSceneListener(*this);
    }

    async::Task<> CameraVisuals::activate()
    {
        scene::IScene::Ptr visualScene = getServiceProvider().get<scene::ISceneFactory>().createEmptyScene();
        visualScene->setName("Editor camera visuals");
        for (auto& [uid, visual] : m_visuals)
        {
            if (visual.camera)
            {
                visualScene->getRoot().attachChild(createVisual(visual));
            }
        }
        m_visualScene = co_await m_scene->getWorld()->addScene(eastl::move(visualScene));
        m_running = true;
    }

    async::Task<> CameraVisuals::shutdown()
    {
        m_running = false;
        m_registration.reset();
        for (auto& [uid, visual] : m_visuals)
        {
            if (visual.activation)
            {
                co_await visual.activation;
            }
        }
        if (m_visualScene)
        {
            getServiceProvider().get<scene::ISceneManager>().deactivateScene(m_visualScene);
            m_visualScene = nullptr;
        }
        m_visuals.clear();
    }

    Uid CameraVisuals::cameraUidForMesh(Uid meshUid) const
    {
        for (const auto& [uid, visual] : m_visuals)
        {
            if (visual.camera && visual.mesh && visual.mesh->getUid() == meshUid)
            {
                return uid;
            }
        }
        return NullUid;
    }

    Uid CameraVisuals::meshUidForCamera(Uid cameraUid) const
    {
        const auto visual = m_visuals.find(cameraUid);
        return visual != m_visuals.end() && visual->second.camera && visual->second.mesh
                   ? visual->second.mesh->getUid()
                   : NullUid;
    }

    scene::SceneObject::Ptr CameraVisuals::createVisual(Visual& visual)
    {
        scene::SceneObject::Ptr object = getServiceProvider().get<scene::ISceneFactory>().createSceneObject<scene::StaticMeshComponent>();
        object->setName("Editor camera mesh");
        scene::StaticMeshComponent& mesh = object->getRootComponent<scene::StaticMeshComponent>();
        visual.mesh = mesh;
        mesh.setMeshGeometry(StaticMeshAssetRef("file:/editor_camera/SM_EditorCamera_Nau.gltf+[mesh/0]"));
        mesh.setMaterial(MaterialAssetRef("file:/editor_camera/M_EditorCamera_Nau.nmat_inst_json"));
        mesh.setCastShadow(false);
        updateTransform(visual);
        return object;
    }

    void CameraVisuals::trackCamera(const scene::Component& component)
    {
        if (component.getParentObject().getScene() != m_scene.get())
        {
            return;
        }
        if (const scene::CameraComponent* camera = component.as<const scene::CameraComponent*>())
        {
            Visual& visual = m_visuals.try_emplace(camera->getUid()).first->second;
            // USD reparenting can replace a component before its old visual is
            // removed. Retain the mesh and any pending activation for this UID.
            visual.camera = *const_cast<scene::CameraComponent*>(camera);
            visual.dirty = true;
        }
    }

    void CameraVisuals::updateTransform(Visual& visual)
    {
        const math::Transform transform = m_transformResolver ? m_transformResolver(*visual.camera) : visual.camera->getWorldTransform();
        visual.mesh->setWorldTransform(transform);
        visual.dirty = false;
    }

    void CameraVisuals::updateTransforms()
    {
        if (!m_running)
        {
            return;
        }
        for (auto& [uid, visual] : m_visuals)
        {
            if (visual.camera && visual.mesh && (!visual.activation || visual.activation.isReady()))
            {
                updateTransform(visual);
            }
        }
    }

    void CameraVisuals::updateVisuals()
    {
        for (auto visualEntry = m_visuals.begin(); visualEntry != m_visuals.end();)
        {
            Visual& visual = visualEntry->second;
            if (visual.activation)
            {
                if (!visual.activation.isReady())
                {
                    ++visualEntry;
                    continue;
                }
                visual.activation = {};
            }
            if (!visual.camera)
            {
                if (visual.mesh)
                {
                    visual.mesh->getParentObject().destroy();
                }
                visualEntry = m_visuals.erase(visualEntry);
                continue;
            }

            if (!visual.mesh)
            {
                visual.activation = m_visualScene->getRoot().attachChildAsync(createVisual(visual));
            }
            else if (visual.dirty)
            {
                updateTransform(visual);
            }
            ++visualEntry;
        }
    }

    void CameraVisuals::onSceneBegin()
    {
    }

    void CameraVisuals::onSceneEnd()
    {
        if (m_running)
        {
            updateVisuals();
        }
    }

    void CameraVisuals::onAfterActivatingObjects(eastl::span<const scene::SceneObject*> objects)
    {
        for (const scene::SceneObject* object : objects)
        {
            for (const scene::Component* component : object->getComponents(true))
            {
                trackCamera(*component);
            }
        }
    }

    void CameraVisuals::onBeforeDeletingObjects(eastl::span<const scene::SceneObject*> objects)
    {
        for (const scene::SceneObject* object : objects)
        {
            for (const scene::Component* component : object->getComponents(true))
            {
                if (auto visual = m_visuals.find(component->getUid()); visual != m_visuals.end() && visual->second.camera.get() == component)
                {
                    // keep proxy alive until its graphics activation finishes
                    visual->second.camera = nullptr;
                }
            }
        }
    }

    void CameraVisuals::onAfterActivatingComponents(eastl::span<const scene::Component*> components)
    {
        for (const scene::Component* component : components)
        {
            trackCamera(*component);
        }
    }

    void CameraVisuals::onBeforeDeletingComponents(eastl::span<const scene::Component*> components)
    {
        for (const scene::Component* component : components)
        {
            if (auto visual = m_visuals.find(component->getUid()); visual != m_visuals.end() && visual->second.camera.get() == component)
            {
                visual->second.camera = nullptr;
            }
        }
    }

    void CameraVisuals::onComponentsChange(eastl::span<const scene::Component*> components)
    {
        const eastl::unordered_set<const scene::Component*> changed(components.begin(), components.end());
        for (auto& [uid, visual] : m_visuals)
        {
            if (visual.camera && changed.contains(visual.camera.get()))
            {
                visual.dirty = true;
            }
        }
    }
}  // namespace nau::editor
