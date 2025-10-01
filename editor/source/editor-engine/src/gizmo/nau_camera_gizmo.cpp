// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/gizmo/nau_camera_gizmo.hpp"
#include "nau/viewport/nau_viewport_utils.hpp"
#include "nau/nau_editor_delegates.hpp"
#include "nau/editor-engine/nau_editor_engine_services.hpp"
#include "nau/debugRenderer/debug_render_system.h"
#include "nau/math/nau_matrix_math.hpp"


static bool isPointInSphere(const nau::math::vec2& point, const nau::math::vec3& origin, float radius)
{
    using vec3_t = nau::math::vec3;

    vec3_t world, direction;
    Nau::Utils::screenToWorld(point, world, direction);

    const vec3_t length = origin - world;
    const float tca = Vectormath::SSE::dot(direction, length);
    if (tca < 0.f) {
        return false;
    }
    const float length2 = Vectormath::SSE::dot(length, length) - tca * tca;
    if (length2 > (radius * radius)) {
        return false;
    }
    const float thc = std::sqrtf(radius * radius - length2);
    if ((tca - thc < 0.f) || (tca + thc < 0.f)) {
        return false;
    }
    return true;
}

static bool isFrustumOnScreen(const nau::math::mat4& basis, const std::vector<nau::math::Point3>& frustumPoints)
{
    for (const auto& pt : frustumPoints) {
        nau::math::vec3 worldPt = basis.getCol3().getXYZ() + Vectormath::SSE::Vector3(pt.getX(), pt.getY(), pt.getZ());
        nau::math::vec2 screen;
        if (Nau::Utils::worldToScreen(worldPt, screen)) {
            return true;
        }
    }
    
    static const std::vector<std::pair<int, int>> frustumEdges = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0}, // near plane
        {4, 5}, {5, 7}, {7, 6}, {6, 4}, // far plane
        {0, 4}, {1, 5}, {2, 6}, {3, 7}  // connecting edges
    };
    
    for (const auto& edge : frustumEdges) {
        if (edge.first < frustumPoints.size() && edge.second < frustumPoints.size()) {
            nau::math::vec3 world1 = basis.getCol3().getXYZ() + Vectormath::SSE::Vector3(
                frustumPoints[edge.first].getX(), 
                frustumPoints[edge.first].getY(), 
                frustumPoints[edge.first].getZ()
            );
                
            nau::math::vec3 world2 = basis.getCol3().getXYZ() + Vectormath::SSE::Vector3(
                frustumPoints[edge.second].getX(), 
                frustumPoints[edge.second].getY(), 
                frustumPoints[edge.second].getZ()
            );
            
            if (Nau::Utils::isLineSegmentVisible(world1, world2)) {
                return true;
            }
        }
    }
    
    return false;
}

// ** NauCameraGizmo

NauCameraGizmo::NauCameraGizmo(std::vector<nau::math::Point3> frustumPoints)
    : m_frustumPoints(std::move(frustumPoints))
    , m_hoveredSphere(SphereIndex::None)
{
}

void NauCameraGizmo::setCallback(NauCameraGizmo::UpdateCallback callback)
{
    m_callback = std::move(callback);
}

nau::math::vec3 NauCameraGizmo::calculateDelta(const nau::math::vec2& pivot2d, const nau::math::vec3& ax, const nau::math::vec3& ay, const nau::math::vec3& az)
{
    return {};
}

bool NauCameraGizmo::isOnScreen() const
{
    return isFrustumOnScreen(m_basis3d, m_frustumPoints);
}

void NauCameraGizmo::renderInternal(const nau::math::mat4& transform, int selectedAxes)
{
    if (m_callback) {
        m_frustumPoints = std::move(m_callback());
    }

    using color_t = nau::math::E3DCOLOR;
    using point_t = nau::math::Point3;
    using vec3_t  = nau::math::vec3;

    constexpr float DrawTime = 2.f;
    constexpr int DrawDensity = 16;

    const color_t ColorFrustum0{ 128, 128, 128 };
    const color_t ColorPlane0{ 255, 255, 255 };
    const color_t ColorCorner0{ 255, 255, 255 };

    const color_t ColorFrustum1{ 255, 215, 0, 128 };
    const color_t ColorPlane1{ 255, 215, 0 };
    const color_t ColorCorner1{ 255, 215, 0 };
    const point_t cameraPosition = point_t(transform.getTranslation());

    color_t frustumColor = ColorFrustum0;
    color_t planeColor = ColorPlane0;
    if (m_hoveredSphere == SphereIndex::CameraPosition) {
        frustumColor = ColorFrustum1;
        planeColor = ColorPlane1;
    } else if (m_hoveredSphere != SphereIndex::None) {
        frustumColor = ColorFrustum0;
        planeColor = ColorPlane1;
    }

    std::array<point_t, 8> frustumPoints;
    std::memcpy(frustumPoints.data(), m_frustumPoints.data(), sizeof(frustumPoints));
    for(auto& vec : frustumPoints) {
        vec = transform * vec;
    }

    nau::math::mat4 sphereTransform = transform;

    auto& dr = nau::getDebugRenderer();
    const auto drawPlaneEdge = [&](const point_t& begin, const point_t& end) {
        dr.drawLine(begin, end, planeColor, DrawTime);
        sphereTransform.setTranslation(vec3_t(begin));
        const bool isHoveredCorner = (&begin - frustumPoints.data()) == m_hoveredSphere;
        const color_t cornerColor = isHoveredCorner ? ColorCorner1 : ColorCorner0;
        dr.drawSphere(m_axesLength3d * 0.03, cornerColor, sphereTransform, DrawDensity, DrawTime);
    };

    // Camera
    sphereTransform.setTranslation(vec3_t(cameraPosition));
    dr.drawSphere(m_axesLength3d * 0.06, ColorCorner0, sphereTransform, DrawDensity, DrawTime);

    // Edges
    dr.drawLine(cameraPosition, frustumPoints[4], frustumColor, DrawTime);
    dr.drawLine(cameraPosition, frustumPoints[5], frustumColor, DrawTime);
    dr.drawLine(cameraPosition, frustumPoints[6], frustumColor, DrawTime);
    dr.drawLine(cameraPosition, frustumPoints[7], frustumColor, DrawTime);

    // Near plane
    drawPlaneEdge(frustumPoints[0], frustumPoints[1]);
    drawPlaneEdge(frustumPoints[1], frustumPoints[3]);
    drawPlaneEdge(frustumPoints[3], frustumPoints[2]);
    drawPlaneEdge(frustumPoints[2], frustumPoints[0]);

    // Far plane
    drawPlaneEdge(frustumPoints[4], frustumPoints[5]);
    drawPlaneEdge(frustumPoints[5], frustumPoints[7]);
    drawPlaneEdge(frustumPoints[7], frustumPoints[6]);
    drawPlaneEdge(frustumPoints[6], frustumPoints[4]);
}

static bool isPointInRect(const nau::math::vec2& point, const nau::math::vec2& start, const nau::math::vec2& end, float width)
{
    const nau::math::vec2 norm = Vectormath::normalize(end - start);
    const float x = Vectormath::dot((point - start), norm);

    if (x < 0 || x > Vectormath::length(end - start)) {
        return false;
    }

    return Vectormath::length(point - (start + x * norm)) <= width;
}

NauGizmoAbstract::Axes NauCameraGizmo::detectHoveredAxes(nau::math::vec2 screenPoint)
{
    using vec3_t  = nau::math::vec3;
    
    const vec3_t cameraPosition = m_basis3d.getTranslation();
    m_hoveredSphere = SphereIndex::None;

    if (isPointInSphere(screenPoint, cameraPosition, m_axesLength3d * 0.06)) {
        m_hoveredSphere = CameraPosition;
        return Axes::None;
    }
    std::array<nau::math::Point3, 8> frustumPoints;
    std::memcpy(frustumPoints.data(), m_frustumPoints.data(), sizeof(frustumPoints));
    for (auto& vec : frustumPoints) {
        vec = m_basis3d * vec;
    }
    const auto cornerCheck = [&screenPoint, radius = (m_axesLength3d * 0.03)](const nau::math::Point3& point) {
        return isPointInSphere(screenPoint, vec3_t(point), radius);
    };
    if (auto it = std::find_if(frustumPoints.begin(), frustumPoints.end(), cornerCheck); it != frustumPoints.end()) {
        m_hoveredSphere = static_cast<SphereIndex>(it - frustumPoints.begin());
        return Axes::None;
    }
    return Axes::None;
}
