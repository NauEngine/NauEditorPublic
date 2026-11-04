// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Managing the outline in the editor.

#pragma once

#include "nau/viewport/nau_outline_manager_interface.hpp"
#include "nau/math/dag_color.h"
#include "nau/scene/scene_object.h"
#include "nau/selection/nau_usd_selection_container.hpp"
#include "nau/nau_usd_scene_synchronizer.hpp"

#include <vector>


// ** NauOutlineManager

class NauOutlineManager : public NauOutlineManagerInterface
{
    NAU_CLASS_(NauOutlineManager, NauOutlineManagerInterface)

public:
    explicit NauOutlineManager(std::shared_ptr<NauUsdSceneSynchronizer> synchronizer);

    void setWidth(float width);
    void setColor(const nau::math::Color4& color);
    void setHighlightObjects(const NauUsdPrimsSelection& selection);
    void reset() noexcept;

    void enableOutline(bool flag) override;

private:
    std::vector<nau::scene::SceneObject::WeakRef> m_objectList;
    std::vector<nau::Uid> m_cameraMeshUids;
    std::shared_ptr<NauUsdSceneSynchronizer> m_sceneEditorSynchronizer;
    nau::math::Color4 m_color;
    float m_width;
    bool m_enabled;
};
