// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/gizmo/nau_gizmo.hpp"
#include "nau/viewport/nau_viewport_utils.hpp"
#include "nau/nau_editor_delegates.hpp"
#include "nau/editor-engine/nau_editor_engine_services.hpp"
#include "nau/debugRenderer/debug_render_system.h"
#include "nau/math/nau_matrix_math.hpp"

#include <QPoint>
#include <QMouseEvent>


static constexpr int NauAxisLenPx = 100;
static constexpr float NauGizmoPxWidth = 8;
static constexpr float NauGizmoScaleCubeWidth = 0.1;
static constexpr float NauGizmoBoxWidth = 8;
static constexpr unsigned NauFillMask = 0xAAAAAAAA;
static constexpr float NauGizmoMeter = 0.01;
static constexpr int NauGizmoCircleDensity = 40;

static const nau::math::E3DCOLOR ColorRed = {255, 0, 0};
static const nau::math::E3DCOLOR ColorLtRed = {255, 114, 118};
static const nau::math::E3DCOLOR ColorGreen = {0, 255, 0};
static const nau::math::E3DCOLOR ColorLtGreen = {144, 238, 144};
static const nau::math::E3DCOLOR ColorBlue = {0, 255, 255};
static const nau::math::E3DCOLOR ColorLtBlue = {173, 216, 230};
static const nau::math::E3DCOLOR ColorYellow = {255, 255, 0};

static bool isPointInRect(const nau::math::vec2& point, const nau::math::vec2& start, const nau::math::vec2& end, float width)
{
    const nau::math::vec2 norm = Vectormath::normalize(end - start);
    const float x = Vectormath::dot((point - start), norm);

    if (x < 0 || x > Vectormath::length(end - start)) {
        return false;
    }

    return Vectormath::length(point - (start + x * norm)) <= width;
}

static bool isPointInTriangle(const nau::math::vec2& point, const nau::math::vec2* triangle)
{
    const auto sign = [](const nau::math::vec2& p1, const nau::math::vec2& p2, const nau::math::vec2& p3) -> float
    {
        return (p1.getX() - p3.getX()) * (p2.getY() - p3.getY()) - (p2.getX() - p3.getX()) * (p1.getY() - p3.getY());
    };
    const float d1 = sign(point, triangle[0], triangle[1]);
    const float d2 = sign(point, triangle[1], triangle[2]);
    const float d3 = sign(point, triangle[2], triangle[0]);

    const bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

    return !(has_neg && has_pos);
}

static bool isGizmoOnScreen(const nau::math::mat4& basis)
{
    // Now we have axis length only in 2d basis
    // TODO: use 3d basis axis length
    constexpr float axisVisibilityLength = NauAxisLenPx / 20;
    const nau::math::vec3 ax = basis.getCol3().getXYZ() + basis.getCol0().getXYZ() * axisVisibilityLength;
    const nau::math::vec3 ay = basis.getCol3().getXYZ() + basis.getCol1().getXYZ() * axisVisibilityLength;
    const nau::math::vec3 az = basis.getCol3().getXYZ() + basis.getCol2().getXYZ() * axisVisibilityLength;

    nau::math::vec2 screen;
    if (Nau::Utils::worldToScreen(basis.getCol3().getXYZ(), screen)) {
        return true;
    }

    if (Nau::Utils::worldToScreen(ax, screen)) {
        return true;
    }

    if (Nau::Utils::worldToScreen(ay, screen)) {
        return true;
    }

    if (Nau::Utils::worldToScreen(az, screen)) {
        return true;
    }

    return false;
}

// ** NauGizmoAbstract

NauGizmoAbstract::~NauGizmoAbstract()
{
    if (isActive()) {
        deactivate();
    }
}

void NauGizmoAbstract::render()
{
    if (!m_isActive || !isGizmoOnScreen(m_basis3d)) {
        return;
    }

    update2DBasis();

    // Update 3d axes length
    const nau::math::vec3& basisPos = m_basis3d.getCol3().getXYZ();

    nau::math::mat4 camera = Nau::EditorEngine().cameraManager()->activeCamera()->getWorldTransform().getMatrix();
    const nau::math::vec3& cameraPos = camera.getCol3().getXYZ();

    m_axesLength3d = Vectormath::SSE::length(basisPos - cameraPos) / 5.0f;

    renderInternal(m_basis3d, m_hoveredAxes ? m_hoveredAxes : m_selectedAxes);
}

void NauGizmoAbstract::handleMouseInput(QMouseEvent* mouseEvent, float dpi)
{
    if (!m_isActive) {
        return;
    }

    const nau::math::vec2 screenPoint(mouseEvent->position().x() * dpi, mouseEvent->position().y() * dpi);

    const bool lmbPressed = mouseEvent->button() == Qt::MouseButton::LeftButton;
    switch (mouseEvent->type())
    {
    case QEvent::MouseButtonPress:
        if (lmbPressed) {
            handleMouseLeftButtonPress(screenPoint);
        }
        break;
    case QEvent::MouseButtonRelease:
        if (lmbPressed) {
            handleMouseLeftButtonRelease();
        }
        break;
    case QEvent::MouseMove:
        handleMouseMove(screenPoint);
        break;
    }
}

void NauGizmoAbstract::handleMouseLeftButtonPress(nau::math::vec2 screenPoint)
{
    if (m_isUsing) {
        return;
    }

    m_hoveredAxes = detectHoveredAxes(screenPoint);
    if (m_hoveredAxes != Axes::None) {
        startUse(screenPoint);
    }
}

void NauGizmoAbstract::handleMouseLeftButtonRelease()
{
    if (m_isUsing) {
        stopUse();
    }
}

void NauGizmoAbstract::handleMouseMove(nau::math::vec2 screenPoint)
{
    if (!m_isUsing || (m_selectedAxes == None)) {
        m_hoveredAxes = detectHoveredAxes(screenPoint);
        return;
    }

    if (m_coordinateSpace == GizmoCoordinateSpace::World) {
        m_basis3d.setUpper3x3(nau::math::mat3::identity());
    }

    const float maxDownscale = 0.05;
    m_mousePos = screenPoint;
    nau::math::vec2 pivot2D;
    Nau::Utils::worldToScreen(m_basis3d.getCol3().getXYZ(), pivot2D);
    nau::math::vec2 delta = m_mousePos - pivot2D + m_gizmoDelta;

    nau::math::vec3 ax, ay, az;
    ax = m_basis3d.getCol0().getXYZ();
    ay = m_basis3d.getCol1().getXYZ();
    az = m_basis3d.getCol2().getXYZ();

    nau::math::vec3 newDelta = calculateDelta(delta, ax, ay, az);

    update(newDelta);
}

void NauGizmoAbstract::update2DBasis()
{
    nau::math::vec2 center, axisX2d, axisY2d, axisZ2d, axisEnd;
    nau::math::vec3 axisX, axisY, axisZ;

    axisX = m_basis3d.getCol0().getXYZ();
    axisY = m_basis3d.getCol1().getXYZ();
    axisZ = m_basis3d.getCol2().getXYZ();

    const nau::math::vec3 pt = m_basis3d.getCol3().getXYZ();

    Nau::Utils::worldToScreen(pt, center);

    Nau::Utils::worldToScreen(pt + axisX, axisEnd);
    axisX2d = (axisEnd - center);
    Nau::Utils::worldToScreen(pt + axisY, axisEnd);
    axisY2d = (axisEnd - center);
    Nau::Utils::worldToScreen(pt + axisZ, axisEnd);
    axisZ2d = (axisEnd - center);

    float xLength = length(axisX2d);
    float yLength = length(axisY2d);
    float zLength = length(axisZ2d);

    float maxlen = std::max(std::max(xLength, yLength), zLength);

    if (xLength == 0) {
        xLength = 1e-5;
    }
    if (yLength == 0) {
        yLength = 1e-5;
    }
    if (zLength == 0) {
        zLength = 1e-5;
    }

    m_basis2d.center = center;
    m_basis2d.axisX = axisX2d * (NauAxisLenPx / maxlen);
    m_basis2d.axisY = axisY2d * (NauAxisLenPx / maxlen);
    m_basis2d.axisZ = axisZ2d * (NauAxisLenPx / maxlen);
}

void NauGizmoAbstract::startUse(nau::math::vec2 screenPoint)
{
    m_isUsing = true;
    m_selectedAxes = m_hoveredAxes;

    m_movedDelta = nau::math::vec3(0, 0, 0);

    m_planeNormal = nau::math::vec3(0, 0, 0);

    m_startPos = m_basis3d.getCol3().getXYZ();
    m_startPos2d = screenPoint;

    m_gizmoDelta = m_basis2d.center - m_startPos2d;

    startedUsing.broadcast();
}

void NauGizmoAbstract::stopUse()
{
    m_isUsing = false;
    m_selectedAxes = Axes::None;
    stoppedUsing.broadcast();
}

void NauGizmoAbstract::update(const nau::math::vec3& delta)
{
    update3DBasis(delta);
    deltaUpdated.broadcast(delta);
}

void NauGizmoAbstract::activate(const nau::math::mat4& basis)
{
    if (m_isActive) {
        return;
    }

    setBasis(basis);

    m_renderGizmoCallbackId = NauEditorEngineDelegates::onRenderDebug.addCallback([this]() {
        render();
    });

    m_isActive = true;
}

void NauGizmoAbstract::deactivate()
{
    if (!m_isActive) {
        return;
    }

    m_isActive = false;
    NauEditorEngineDelegates::onRenderDebug.deleteCallback(m_renderGizmoCallbackId);
}

void NauGizmoAbstract::setBasis(const nau::math::mat4& basis)
{
    m_basis3d = basis;
    if (m_coordinateSpace == GizmoCoordinateSpace::Local) {
        m_basis3d.setCol0(Vectormath::SSE::normalize(m_basis3d.getCol0()));
        m_basis3d.setCol1(Vectormath::SSE::normalize(m_basis3d.getCol1()));
        m_basis3d.setCol2(Vectormath::SSE::normalize(m_basis3d.getCol2()));
    } else {
        m_basis3d.setUpper3x3(nau::math::mat3::identity());
    }

}

nau::math::mat4 NauGizmoAbstract::basis() const
{
    return m_basis3d;
}

GizmoCoordinateSpace NauGizmoAbstract::coordinateSpace() const
{
    return m_coordinateSpace;
}

void NauGizmoAbstract::setCoordinateSpace(GizmoCoordinateSpace space)
{
    m_coordinateSpace = space;
}


// ** NauTranslateGizmo

void NauTranslateGizmo::update3DBasis(const nau::math::vec3& delta)
{
    m_basis3d.setCol3(
        nau::math::vec4(m_basis3d.getCol3().getXYZ() + delta, 0));
}

nau::math::vec3 NauTranslateGizmo::calculateDelta(const nau::math::vec2& delta, const nau::math::vec3& ax, const nau::math::vec3& ay, const nau::math::vec3& az)
{
    m_movedDelta = {0, 0, 0};

    nau::math::vec3 mouseWorld;
    nau::math::vec3 mouseDir;
    nau::math::vec2 startPosScreen;
    Nau::Utils::worldToScreen(m_startPos, startPosScreen);

    const nau::math::vec3 pivotPosition = m_basis3d.getCol3().getXYZ();

    Nau::Utils::screenToWorld(m_mousePos - (m_startPos2d - startPosScreen), mouseWorld, mouseDir);

    if (m_planeNormal == nau::math::vec3(0, 0, 0)) {
        m_planeNormal = ax;
 
        switch (m_selectedAxes & AxisXYZ) {
            case AxisXYZ:
            case AxisXY: 
                m_planeNormal = az;
                break;
            case AxisXZ: 
                m_planeNormal = ay;
                break;
            case AxisYZ: 
                m_planeNormal = ax;
                break;
            case AxisX: 
                m_planeNormal = fabsf(Vectormath::SSE::dot(ay, mouseDir)) > fabsf(Vectormath::SSE::dot(az, mouseDir)) ? ay : az;
                break;
            case AxisY: 
                m_planeNormal = fabsf(Vectormath::SSE::dot(ax, mouseDir)) > fabsf(Vectormath::SSE::dot(az, mouseDir)) ? ax : az;
                break;
            case AxisZ: 
                m_planeNormal = fabsf(Vectormath::SSE::dot(ax, mouseDir)) > fabsf(Vectormath::SSE::dot(ay, mouseDir)) ? ax : ay;
                break;
        }
    }

    if (fabsf(Vectormath::SSE::dot(m_planeNormal, mouseDir)) > 0.05) {
        m_movedDelta = (Vectormath::SSE::dot(m_planeNormal, pivotPosition - mouseWorld) / Vectormath::SSE::dot(m_planeNormal, mouseDir)) * mouseDir + mouseWorld;
        m_movedDelta -= pivotPosition;
    }

    switch (m_selectedAxes & AxisXYZ) {
        case AxisX: 
            m_movedDelta = Vectormath::SSE::dot(m_movedDelta, ax) * ax;
            break;
        case AxisY: 
            m_movedDelta = Vectormath::SSE::dot(m_movedDelta, ay) * ay;
            break;
        case AxisZ:
            m_movedDelta = Vectormath::SSE::dot(m_movedDelta, az) * az;
            break;
    }

    if (Vectormath::SSE::dot((pivotPosition + m_movedDelta - mouseWorld), mouseDir) < 0) {
        m_movedDelta = nau::math::vec3(0, 0, 0);
    }

    return m_movedDelta;
}

NauGizmoAbstract::Axes NauTranslateGizmo::detectHoveredAxes(nau::math::vec2 screenPoint)
{
    nau::math::vec2 p1, p2;
    Nau::Utils::worldToScreen(m_basis3d.getCol3().getXYZ(), p1);

    Nau::Utils::worldToScreen(m_basis3d.getCol3().getXYZ() + Vectormath::SSE::normalize(m_basis3d.getCol0().getXYZ()) * m_axesLength3d, p2);
    if (isPointInRect(screenPoint, p1, p2, NauGizmoPxWidth)) {
        return Axes::AxisX;
    }

    Nau::Utils::worldToScreen(m_basis3d.getCol3().getXYZ() + Vectormath::SSE::normalize(m_basis3d.getCol1().getXYZ()) * m_axesLength3d, p2);
    if (isPointInRect(screenPoint, p1, p2, NauGizmoPxWidth)) {
        return Axes::AxisY;
    }

    Nau::Utils::worldToScreen(m_basis3d.getCol3().getXYZ() + Vectormath::SSE::normalize(m_basis3d.getCol2().getXYZ()) * m_axesLength3d, p2);
    if (isPointInRect(screenPoint, p1, p2, NauGizmoPxWidth)) {
        return Axes::AxisZ;
    }

    return Axes::None;
}

void NauTranslateGizmo::renderInternal(const nau::math::mat4& basis, int selectedAxes)
{
    auto& dr = nau::getDebugRenderer();

    nau::math::E3DCOLOR lineColor = (m_hoveredAxes & AxisX) ? ColorYellow : ColorLtRed;
    dr.drawArrow(
        nau::math::Point3(basis.getCol3().getXYZ()),
        nau::math::Point3(basis.getCol3().getXYZ() + Vectormath::SSE::normalize(basis.getCol0().getXYZ()) * m_axesLength3d),
        lineColor,
        basis.getCol1().getXYZ(),
        1);

    lineColor = (m_hoveredAxes & AxisY) ? ColorYellow : ColorLtGreen;
    dr.drawArrow(
        nau::math::Point3(basis.getCol3().getXYZ()),
        nau::math::Point3(basis.getCol3().getXYZ() + Vectormath::SSE::normalize(basis.getCol1().getXYZ()) * m_axesLength3d),
        lineColor,
        basis.getCol2().getXYZ(),
        1);

    lineColor = (m_hoveredAxes & AxisZ) ? ColorYellow : ColorLtBlue;
    dr.drawArrow(
        nau::math::Point3(basis.getCol3().getXYZ()),
        nau::math::Point3(basis.getCol3().getXYZ() + Vectormath::SSE::normalize(basis.getCol2().getXYZ()) * m_axesLength3d),
        lineColor,
        basis.getCol0().getXYZ(),
        1);
}


// ** NauRotateGizmo

void NauRotateGizmo::startUse(nau::math::vec2 screenPoint)
{
    NauGizmoAbstract::startUse(screenPoint);

    // Reset previous moved delta every using
    m_prevMovedDelta = {0, 0, 0};

    if (!(m_selectedAxes & AxisXYZ)) {
        return;
    }

    m_startRotAngle = 0;
    constexpr int rotationDistanceMultiplier = 32;

    nau::math::vec3 pos;
    nau::math::vec3 dx;
    nau::math::vec3 dy;
    int delta = NauGizmoPxWidth;
    int count = 0;
    float distance = NauGizmoPxWidth * NauGizmoPxWidth * rotationDistanceMultiplier;
    float minDistance = distance;

    nau::math::vec2 pos2d;
    Nau::Utils::worldToScreen(pos, pos2d);

    m_rotAngle = m_startRotAngle;
    m_startPos2d = pos2d;    
}

void NauRotateGizmo::stopUse()
{
    NauGizmoAbstract::stopUse();
    m_rotAngle = m_startRotAngle;
    if (m_coordinateSpace == GizmoCoordinateSpace::World) {
        m_basis3d.setUpper3x3(nau::math::mat3::identity());
    }
}

void NauRotateGizmo::update3DBasis(const nau::math::vec3& delta)
{
    if (m_coordinateSpace == GizmoCoordinateSpace::World) {
        m_basis3d.setUpper3x3(nau::math::mat3::identity());
    }
    else {
        const nau::math::vec3 inverseDelta = -delta;
        nau::math::mat3 deltaRotateMatrix = nau::math::mat3::rotationZYX({
            inverseDelta.getX(),
            inverseDelta.getY(),
            inverseDelta.getZ()
            });

        Vectormath::SSE::length(m_basis3d.getCol0());

        const float sx = Vectormath::SSE::length(m_basis3d.getCol0());
        const float sy = Vectormath::SSE::length(m_basis3d.getCol1());
        const float sz = Vectormath::SSE::length(m_basis3d.getCol2());

        nau::math::mat3 rotateMatrix;
        rotateMatrix.setCol0(m_basis3d.getCol0().getXYZ() / sx);
        rotateMatrix.setCol1(m_basis3d.getCol1().getXYZ() / sy);
        rotateMatrix.setCol2(m_basis3d.getCol2().getXYZ() / sz);
        rotateMatrix *= deltaRotateMatrix;

        nau::math::mat4 newTransform;
        newTransform.setCol0(nau::math::vec4(rotateMatrix.getCol0(), 0));
        newTransform.setCol1(nau::math::vec4(rotateMatrix.getCol1(), 0));
        newTransform.setCol2(nau::math::vec4(rotateMatrix.getCol2(), 0));
        newTransform.setCol3(m_basis3d.getCol3());
        NauMathMatrixUtils::orthonormalize(newTransform);

        newTransform[0] *= sx;
        newTransform[1] *= sy;
        newTransform[2] *= sz;

        m_basis3d = newTransform;
    }
}

nau::math::vec3 NauRotateGizmo::calculateDelta(const nau::math::vec2& delta, const nau::math::vec3& ax, const nau::math::vec3& ay, const nau::math::vec3& az)
{
    float angleRotated;
    m_movedDelta =  nau::math::vec3(0, 0, 0);
    angleRotated = nau::math::dot(delta, m_rotateDir) / NauAxisLenPx;

    m_rotAngle = angleRotated + m_startRotAngle;

    if (m_selectedAxes & AxisX) {
        m_movedDelta.setX(angleRotated);
    }

    if (m_selectedAxes & AxisY) {
        m_movedDelta.setY(angleRotated);
    }

    if (m_selectedAxes & AxisZ) {
        m_movedDelta.setZ(angleRotated);
    }

    nau::math::vec3 calculatedDelta = m_movedDelta - m_prevMovedDelta;
    m_prevMovedDelta = m_movedDelta;

    return calculatedDelta;
}

bool NauRotateGizmo::isPointInEllipse(const nau::math::vec3& center, const nau::math::vec3& a, const nau::math::vec3& b, float width, const nau::math::vec2& point,
    float start, float end)
{
    const int elipseSegCount = 30;
    const float elipseStep = 2 * std::numbers::pi / elipseSegCount;

    nau::math::vec3 prev = center + a * cos(start) + b * sin(start);
    nau::math::vec3 next;
    nau::math::vec2 prev2D;
    nau::math::vec2 next2D;

    for (float t = start; t < end; t += elipseStep) {
        next = center + a * cos(t) + b * sin(t);
        Nau::Utils::worldToScreen(prev, prev2D);
        Nau::Utils::worldToScreen(next, next2D);
        if (isPointInRect(point, prev2D, next2D, width)) {
            m_rotateDir = normalize(next2D - prev2D);
            return true;
        }

        prev = next;
    }

    next = center + a * cos(end) + b * sin(end);
    Nau::Utils::worldToScreen(prev, prev2D);
    Nau::Utils::worldToScreen(next, next2D);
    return isPointInRect(point, prev2D, next2D, width);
}

NauGizmoAbstract::Axes NauRotateGizmo::detectHoveredAxes(nau::math::vec2 screenPoint)
{
    if (isPointInEllipse(m_basis3d.getCol3().getXYZ(), m_basis3d.getCol2().getXYZ() * m_axesLength3d,
            m_basis3d.getCol1().getXYZ() * m_axesLength3d, NauGizmoPxWidth, screenPoint)) {
        return Axes::AxisX;
    }

    if (isPointInEllipse(m_basis3d.getCol3().getXYZ(), m_basis3d.getCol0().getXYZ() * m_axesLength3d,
            m_basis3d.getCol2().getXYZ() * m_axesLength3d, NauGizmoPxWidth, screenPoint)) {
        return Axes::AxisY;
    }

    if (isPointInEllipse(m_basis3d.getCol3().getXYZ(), m_basis3d.getCol1().getXYZ() * m_axesLength3d,
            m_basis3d.getCol0().getXYZ() * m_axesLength3d, NauGizmoPxWidth, screenPoint)) {
        return Axes::AxisZ;
    }

    return Axes::None;
}

void NauRotateGizmo::renderInternal(const nau::math::mat4& basis, int selectedAxes)
{
    auto& dr = nau::getDebugRenderer();

    nau::math::E3DCOLOR color = (selectedAxes == AxisX) ? ColorYellow : ColorLtRed;
    nau::math::mat4 rot = nau::math::mat4::identity().rotationY(std::numbers::pi /2);
    dr.drawCircle(m_axesLength3d, color, basis * rot, NauGizmoCircleDensity, 1.0f);

    color = (selectedAxes == AxisY) ? ColorYellow : ColorLtGreen;
    rot = nau::math::mat4::identity().rotationX(std::numbers::pi / 2);
    dr.drawCircle(m_axesLength3d, color, basis * rot, NauGizmoCircleDensity, 1.0f);

    color = (selectedAxes == AxisZ) ? ColorYellow : ColorLtBlue;
    dr.drawCircle(m_axesLength3d, color, basis, NauGizmoCircleDensity, 1.0f);
}


// ** NauScaleGizmo

void NauScaleGizmo::setBasis(const nau::math::mat4& basis)
{
    m_basis3d = basis;

    m_basis3d.setCol0(Vectormath::SSE::normalize(m_basis3d.getCol0()));
    m_basis3d.setCol1(Vectormath::SSE::normalize(m_basis3d.getCol1()));
    m_basis3d.setCol2(Vectormath::SSE::normalize(m_basis3d.getCol2()));
}

void NauScaleGizmo::startUse(nau::math::vec2 screenPoint)
{
    NauGizmoAbstract::startUse(screenPoint);
    m_scale = nau::math::vec3(1, 1, 1);
}

void NauScaleGizmo::stopUse()
{
    NauGizmoAbstract::stopUse();
    m_scale = nau::math::vec3(1, 1, 1);
}

nau::math::vec3 NauScaleGizmo::calculateDelta(const nau::math::vec2& delta, const nau::math::vec3& ax, const nau::math::vec3& ay, const nau::math::vec3& az)
{
    m_movedDelta = nau::math::vec3(1, 1, 1);
    float len2;

    const NauBasis2D& scaleBasis = basis2d();

    if (m_selectedAxes & AxisX) {
        len2 = nau::math::lengthSqr(scaleBasis.axisX);
        if (len2 > 1e-5)
            m_movedDelta.setX(m_movedDelta.getX() + nau::math::dot(scaleBasis.axisX, delta) / len2);
    }

    if (m_selectedAxes & AxisY) {
        len2 = nau::math::lengthSqr(scaleBasis.axisY);
        if (len2 > 1e-5)
            m_movedDelta.setY(m_movedDelta.getY() + nau::math::dot(scaleBasis.axisY, delta) / len2);
    }

    if (m_selectedAxes & AxisZ) {
        len2 = nau::math::lengthSqr(scaleBasis.axisZ);
        if (len2 > 1e-5)
            m_movedDelta.setZ(m_movedDelta.getZ() + nau::math::dot(scaleBasis.axisZ, delta) / len2);
    }

    const float maxDownscale = 0.05;

    if (m_movedDelta.getX() < maxDownscale) {
        m_movedDelta.setX(maxDownscale);
    }
    if (m_movedDelta.getY() < maxDownscale) {
        m_movedDelta.setY(maxDownscale);
    }
    if (m_movedDelta.getZ() < maxDownscale) {
        m_movedDelta.setZ(maxDownscale);
    }

    float dMax;

    switch (m_selectedAxes) {
        case AxisX | AxisY:
            dMax = std::max(m_movedDelta.getX(), m_movedDelta.getY());
            m_movedDelta.setX(dMax);
            m_movedDelta.setY(dMax);
            break;
        case AxisY | AxisZ:
            dMax = std::max(m_movedDelta.getY(), m_movedDelta.getZ());
            m_movedDelta.setY(dMax);
            m_movedDelta.setZ(dMax);
            break;
        case AxisX | AxisZ:
            dMax = std::max(m_movedDelta.getZ(), m_movedDelta.getX());
            m_movedDelta.setZ(dMax);
            m_movedDelta.setX(dMax);
            break;
        case AxisX | AxisY | AxisZ:
            m_movedDelta.setX(m_movedDelta.getY());
            m_movedDelta.setZ(m_movedDelta.getY());
            break;
    }

    nau::math::vec3 calculatedDelta = m_movedDelta - (m_scale - nau::math::vec3(1,1,1));
    m_scale = m_movedDelta;

    return calculatedDelta;
}

void NauScaleGizmo::renderInternal(const nau::math::mat4& basis, int selectedAxes)
{
    auto& dr = nau::getDebugRenderer();

    nau::math::E3DCOLOR lineColor = (m_hoveredAxes & AxisX) ? ColorYellow : ColorLtRed;
    nau::math::BBox3 box;

    dr.drawLine(
        nau::math::Point3(basis.getCol3().getXYZ()),
        nau::math::Point3(basis.getCol3().getXYZ() + Vectormath::SSE::normalize(basis.getCol0().getXYZ()) * m_axesLength3d),
        lineColor,
        1);

    box.makecube(basis.getCol3().getXYZ() + Vectormath::SSE::normalize(basis.getCol0().getXYZ()) * m_axesLength3d, NauGizmoScaleCubeWidth * m_axesLength3d);
    dr.drawBoundingBox(box, lineColor, 1);

    lineColor = (m_hoveredAxes & AxisY) ? ColorYellow : ColorLtGreen;
    dr.drawLine(
        nau::math::Point3(basis.getCol3().getXYZ()),
        nau::math::Point3(basis.getCol3().getXYZ() + Vectormath::SSE::normalize(basis.getCol1().getXYZ()) * m_axesLength3d),
        lineColor,
        1);

    box.makecube(basis.getCol3().getXYZ() + Vectormath::SSE::normalize(basis.getCol1().getXYZ()) * m_axesLength3d, NauGizmoScaleCubeWidth * m_axesLength3d);
    dr.drawBoundingBox(box, lineColor, 1);

    lineColor = (m_hoveredAxes & AxisZ) ? ColorYellow : ColorLtBlue;
    dr.drawLine(
        nau::math::Point3(basis.getCol3().getXYZ()),
        nau::math::Point3(basis.getCol3().getXYZ() + Vectormath::SSE::normalize(basis.getCol2().getXYZ()) * m_axesLength3d),
        lineColor,
        1);

    box.makecube(basis.getCol3().getXYZ() + Vectormath::SSE::normalize(basis.getCol2().getXYZ()) * m_axesLength3d, NauGizmoScaleCubeWidth * m_axesLength3d);
    dr.drawBoundingBox(box, lineColor, 1);
}

NauGizmoAbstract::Axes NauScaleGizmo::detectHoveredAxes(nau::math::vec2 screenPoint)
{
    auto& dr = nau::getDebugRenderer();

    nau::math::vec2 rectPoint1, rectPoint2;
    nau::math::vec2 center;
    Nau::Utils::worldToScreen(m_basis3d.getCol3().getXYZ(), center);
    nau::math::vec3 axis;
    nau::math::vec3 normalizedAxis;
    const float scaleCubeHalfWidth = NauGizmoScaleCubeWidth * m_axesLength3d / 2;

    axis = m_basis3d.getCol3().getXYZ() + Vectormath::SSE::normalize(m_basis3d.getCol0().getXYZ()) * m_axesLength3d;
    Nau::Utils::worldToScreen(axis, rectPoint2);
    if (isPointInRect(screenPoint, center, rectPoint2, NauGizmoPxWidth)) {
        return Axes::AxisX;
    }

    normalizedAxis = Vectormath::SSE::normalize(axis);
    axis -= normalizedAxis * scaleCubeHalfWidth;
    Nau::Utils::worldToScreen(axis, rectPoint1);
    Nau::Utils::worldToScreen(axis + normalizedAxis * NauGizmoScaleCubeWidth * m_axesLength3d, rectPoint2);
    if (isPointInRect(screenPoint, rectPoint1, rectPoint2, NauGizmoPxWidth + NauGizmoScaleCubeWidth * m_axesLength3d)) {
        return Axes::AxisX;
    }

    axis = m_basis3d.getCol3().getXYZ() + Vectormath::SSE::normalize(m_basis3d.getCol1().getXYZ()) * m_axesLength3d;
    Nau::Utils::worldToScreen(axis, rectPoint2);
    if (isPointInRect(screenPoint, center, rectPoint2, NauGizmoPxWidth)) {
        return Axes::AxisY;
    }

    normalizedAxis = Vectormath::SSE::normalize(axis);
    axis -= normalizedAxis * scaleCubeHalfWidth;
    Nau::Utils::worldToScreen(axis, rectPoint1);
    Nau::Utils::worldToScreen(axis + normalizedAxis * NauGizmoScaleCubeWidth * m_axesLength3d, rectPoint2);
    if (isPointInRect(screenPoint, rectPoint1, rectPoint2, NauGizmoPxWidth + NauGizmoScaleCubeWidth * m_axesLength3d)) {
        return Axes::AxisY;
    }

    axis = m_basis3d.getCol3().getXYZ() + Vectormath::SSE::normalize(m_basis3d.getCol2().getXYZ()) * m_axesLength3d;
    Nau::Utils::worldToScreen(axis, rectPoint2);
    if (isPointInRect(screenPoint, center, rectPoint2, NauGizmoPxWidth)) {
        return Axes::AxisZ;
    }

    normalizedAxis = Vectormath::SSE::normalize(axis);
    axis -= normalizedAxis * scaleCubeHalfWidth;
    Nau::Utils::worldToScreen(axis, rectPoint1);
    Nau::Utils::worldToScreen(axis + normalizedAxis * NauGizmoScaleCubeWidth * m_axesLength3d, rectPoint2);
    if (isPointInRect(screenPoint, rectPoint1, rectPoint2, NauGizmoPxWidth + NauGizmoScaleCubeWidth * m_axesLength3d)) {
        return Axes::AxisZ;
    }

    return Axes::None;
}
