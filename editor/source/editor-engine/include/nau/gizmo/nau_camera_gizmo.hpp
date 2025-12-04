// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Camera gizmo implementation

#pragma once

#include "nau_gizmo.hpp"

#include <vector>


// ** NauCameraGizmo

class NAU_EDITOR_ENGINE_API NauCameraGizmo : public NauGizmoAbstract
{
public:
    using UpdateCallback = std::function<std::vector<nau::math::Point3>()>;

    explicit NauCameraGizmo(std::vector<nau::math::Point3> frustumPoints);

    void setCallback(UpdateCallback callback);

protected:
    enum SphereIndex {
        None = -1,
        NearLeftBottom,
        NearRightBottm,
        NearLeftTop,
        NearRightTop,
        FarLeftBottom,
        FarRightBottm,
        FarLeftTop,
        FarRightTop,
        CameraPosition
    };

    virtual nau::math::vec3 calculateDelta(const nau::math::vec2& pivot2d, const nau::math::vec3& ax, const nau::math::vec3& ay, const nau::math::vec3& az) override;
    virtual void renderInternal(const nau::math::mat4& transform, int selectedAxes) override;
    virtual Axes detectHoveredAxes(nau::math::vec2 screenPoint) override;
    virtual bool isOnScreen() const override;

private:
    std::vector<nau::math::Point3> m_frustumPoints;
    SphereIndex m_hoveredSphere;
    UpdateCallback m_callback;
};