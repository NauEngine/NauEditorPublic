// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Gizmos implementations

#pragma once

#include "nau/nau_editor_engine_api.hpp"
#include "nau/nau_delegate.hpp"
#include "nau/math/dag_e3dColor.h"
#include "nau/math/math.h"

#include <numbers>


class QMouseEvent;


// ** GizmoCoordinateSpace

enum class GizmoCoordinateSpace
{
    Local,
    World
};


// ** NauBasis2D
//
// Gizmo basis data in 2d space

struct NauBasis2D
{
    nau::math::vec2 center = nau::math::vec2(0, 0);
    nau::math::vec2 axisX = nau::math::vec2(0 ,0);
    nau::math::vec2 axisY = nau::math::vec2(0, 0);
    nau::math::vec2 axisZ = nau::math::vec2(0, 0);
};


// ** NauGizmoAbstract
//
// Base gizmo class. Implements input processing, rendering and interaction
// Need to implement a child class with the render, axes detection and delta calculation functions implementation to create a gizmo.
// Deactivated by default, to activate need a starting point where the gizmo will be created (activate function)

class NAU_EDITOR_ENGINE_API NauGizmoAbstract
{
public:
    enum Axes
    {
        None = 0,
        AxisX = 1,
        AxisY = 2,
        AxisZ = 4,
        AxisXY = AxisX | AxisY,
        AxisXZ = AxisX | AxisZ,
        AxisYZ = AxisY | AxisZ,
        AxisXYZ = AxisX | AxisY | AxisZ
    };

    NauGizmoAbstract() = default;
    virtual ~NauGizmoAbstract();

    void handleMouseInput(QMouseEvent* mouseEvent, float dpi);

    void activate(const nau::math::mat4& basis);
    void deactivate();

    bool isUsing() const { return m_isUsing; }
    bool isActive() const { return m_isActive; }

    virtual void setBasis(const nau::math::mat4& basis);
    nau::math::mat4 basis() const;

    GizmoCoordinateSpace coordinateSpace() const;
    void setCoordinateSpace(GizmoCoordinateSpace space);

protected:
    virtual void startUse(nau::math::vec2 screenPoint);
    virtual void update(const nau::math::vec3& delta);
    virtual void stopUse();
    
    virtual void update3DBasis(const nau::math::vec3& delta) { }
    virtual nau::math::vec3 calculateDelta(const nau::math::vec2& delta, const nau::math::vec3& ax, const nau::math::vec3& ay, const nau::math::vec3& az) = 0;

    virtual void renderInternal(const nau::math::mat4& basis, int selectedAxes) = 0;
    virtual Axes detectHoveredAxes(nau::math::vec2 screenPoint) = 0;

    virtual bool isOnScreen() const;

    const NauBasis2D& basis2d() { return m_basis2d; }

private:

    // Input handling
    void handleMouseMove(nau::math::vec2 screenPoint);
    void handleMouseLeftButtonPress(nau::math::vec2 screenPoint);
    void handleMouseLeftButtonRelease();

    void update2DBasis();
    void render();

public:
    NauDelegate<> startedUsing;
    NauDelegate<nau::math::vec3> deltaUpdated;
    NauDelegate<> stoppedUsing;

protected:
    GizmoCoordinateSpace m_coordinateSpace = GizmoCoordinateSpace::Local;

    bool m_isActive = false;
    bool m_isUsing = false;
    Axes m_hoveredAxes = Axes::None;
    Axes m_selectedAxes = Axes::None;

    nau::math::mat4 m_basis3d;
    nau::math::vec3 m_movedDelta = nau::math::vec3(0, 0, 0);

    nau::math::vec2 m_gizmoDelta = nau::math::vec2(0, 0);
    nau::math::vec2 m_mousePos = nau::math::vec2(0, 0);
    nau::math::vec2 m_startPos2d = nau::math::vec2(0, 0);
    nau::math::vec3 m_startPos = nau::math::vec3(0, 0, 0);
    nau::math::vec3 m_planeNormal = nau::math::vec3(0, 1, 0);

    // Needed for to keep 3d axes size
    // Need update every render tick
    // It is necessary for the ellipse drawing now. Until we implement 3d gizmo rendering, we can just leave it as a variable
    float m_axesLength3d = 0;

private:
    NauBasis2D m_basis2d;
    NauCallbackId m_renderGizmoCallbackId;
};


// ** NauTranslateGizmo
//
// Implements a translate specific rendering, delta calculation

class NAU_EDITOR_ENGINE_API NauTranslateGizmo : public NauGizmoAbstract
{
protected:
    virtual void update3DBasis(const nau::math::vec3& delta) override;
    virtual nau::math::vec3 calculateDelta(const nau::math::vec2& pivot2d, const nau::math::vec3& ax, const nau::math::vec3& ay, const nau::math::vec3& az) override;
    virtual void renderInternal(const nau::math::mat4& transform, int selectedAxes) override;
    virtual Axes detectHoveredAxes(nau::math::vec2 screenPoint) override;
};


// ** NauRotateGizmo
//
// Implements a rotate specific rendering, delta calculation

class NAU_EDITOR_ENGINE_API NauRotateGizmo : public NauGizmoAbstract
{
protected:
    virtual void startUse(nau::math::vec2 screenPoint) override;

    virtual void update3DBasis(const nau::math::vec3& delta) override;
    virtual nau::math::vec3 calculateDelta(const nau::math::vec2& pivot2d, const nau::math::vec3& ax, const nau::math::vec3& ay, const nau::math::vec3& az) override;

    virtual void renderInternal(const nau::math::mat4& basis, int selectedAxes) override;
    virtual Axes detectHoveredAxes(nau::math::vec2 screenPoint) override;

private:
    bool isPointInEllipse(const nau::math::vec3& center, const nau::math::vec3& a, const nau::math::vec3& b, float width, const nau::math::vec2& point, float start = 0,
        float end = 2 * std::numbers::pi);

private:
    nau::math::vec3 m_prevMovedDelta = nau::math::vec3(0, 0, 0);
    nau::math::vec2 m_rotateDir = nau::math::vec2(0, 0);
    float m_rotAngle = 0;
    float m_startRotAngle = 0;
};


// ** NauScaleGizmo
//
// Implements a scale specific rendering, delta calculation

class NAU_EDITOR_ENGINE_API NauScaleGizmo : public NauGizmoAbstract
{
public:
    void setBasis(const nau::math::mat4& basis) override;

protected:
    virtual void startUse(nau::math::vec2 screenPoint) override;
    virtual void stopUse() override;

    virtual nau::math::vec3 calculateDelta(const nau::math::vec2& pivot2d, const nau::math::vec3& ax, const nau::math::vec3& ay, const nau::math::vec3& az) override;

    virtual void renderInternal(const nau::math::mat4& basis, int selectedAxes) override;
    virtual Axes detectHoveredAxes(nau::math::vec2 screenPoint) override;

private:
    nau::math::vec3 m_scale = nau::math::vec3(1, 1, 1);
};
