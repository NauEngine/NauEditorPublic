// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_outline_manager.hpp"

#include "nau/service/service_provider.h"
#include "nau/render/render_window.h"
#include "nau/graphics/core_graphics.h"
#include "nau/scene/components/static_mesh_component.h"

#include <algorithm>

// ** NauOutlineManager

const nau::math::Color4 NullOutlineColor{ 0.f, 0.f, 0.f, 0.f };

NauOutlineManager::NauOutlineManager(std::shared_ptr<NauUsdSceneSynchronizer> synchronizer)
    : m_sceneEditorSynchronizer(std::move(synchronizer))
    , m_color(NullOutlineColor)
    , m_width(0.f)
    , m_enabled(false)
{}

void NauOutlineManager::setWidth(float width)
{
    m_width = std::max(0.f, width);
    enableOutline(m_enabled);
}

void NauOutlineManager::setColor(const nau::math::Color4& color)
{
    m_color = color;
    enableOutline(m_enabled);
}

void NauOutlineManager::setHighlightObjects(const NauUsdPrimsSelection& selection)
{
    auto& graphics = nau::getServiceProvider().get<nau::ICoreGraphics>();
    for (auto object : m_objectList) {
        if (!object) {
            continue;
        }
        if (auto* mesh = object->findFirstComponent<nau::scene::StaticMeshComponent>()) {
            graphics.setObjectHighlight(mesh->getUid(), false);
        }
    }
    m_objectList.clear();
    for (nau::Uid uid : m_cameraMeshUids)
    {
        graphics.setObjectHighlight(uid, false);
    }
    m_cameraMeshUids.clear();
    if (m_sceneEditorSynchronizer == nullptr) {
        return;
    }

    m_objectList.reserve(selection.size());
    for (const pxr::UsdPrim& selectedPrim : selection) {
        const std::string& primPath = selectedPrim.GetPath().GetString();
        if (nau::scene::SceneObject::WeakRef object = m_sceneEditorSynchronizer->sceneObjectByPrimPath(selectedPrim.GetPath())) {
            if (auto* mesh = object->findFirstComponent<nau::scene::StaticMeshComponent>()) {
                graphics.setObjectHighlight(mesh->getUid(), true);
                m_objectList.emplace_back(object);
            }
        }
        for (nau::Uid uid : m_sceneEditorSynchronizer->cameraMeshUidsForPrim(selectedPrim.GetPath()))
        {
            graphics.setObjectHighlight(uid, true);
            m_cameraMeshUids.push_back(uid);
        }
    }
}

void NauOutlineManager::reset() noexcept
{
    m_sceneEditorSynchronizer.reset();
}

void NauOutlineManager::enableOutline(bool flag)
{
    auto& graphics = nau::getServiceProvider().get<nau::ICoreGraphics>();
    if (flag) {
        graphics.getDefaultRenderWindow().acquire()->setOutlineWidth(m_width);
        graphics.getDefaultRenderWindow().acquire()->setOutlineColor(m_color);
    } else {
        graphics.getDefaultRenderWindow().acquire()->setOutlineWidth(0.f);
        graphics.getDefaultRenderWindow().acquire()->setOutlineColor(NullOutlineColor);
    }
    m_enabled = flag;
}

