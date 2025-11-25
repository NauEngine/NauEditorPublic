// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_transform_tools.hpp"
#include "nau/nau_constants.hpp"
#include "nau/math/nau_matrix_math.hpp"
#include "nau/math/math.h"

#include "nau/utils/nau_usd_editor_utils.hpp"

#include <algorithm>


// ** NauTransformTool

NauTransformTool::NauTransformTool(std::unique_ptr<NauGizmoAbstract>&& gizmo, NauUsdSelectionContainerPtr container)
    : m_gizmo(std::move(gizmo))
    , m_applyDeltaCallbackId()
    , m_stoppedUsingCallbackId()
    , m_container(container)
{
    init();
}

NauTransformTool::~NauTransformTool()
{
    terminate();
}

void NauTransformTool::handleMouseInput(QMouseEvent* mouseEvent, float dpi)
{
    m_gizmo->handleMouseInput(mouseEvent, dpi);
}

void NauTransformTool::updateBasis()
{
    if (m_gizmo->isActive()) {
        auto primTransform = NauUsdPrimUtils::worldPrimTransform(m_container->selection().back());
        const nau::math::mat4 transform = NauUsdEditorMathUtils::gfMatrixToNauMatrix(primTransform);
        m_gizmo->setBasis(transform);
    }
}

void NauTransformTool::setCoordinateSpace(GizmoCoordinateSpace space)
{
    if (m_gizmo) {
        m_gizmo->setCoordinateSpace(space);
        updateBasis();
    }
}

void NauTransformTool::applyDeltaToSelection(nau::math::vec3 delta)
{
    for (auto selected : m_container->selection()) {
        auto primTransform = NauUsdPrimUtils::worldPrimTransform(selected);
        const nau::math::mat4 oldTransform = NauUsdEditorMathUtils::gfMatrixToNauMatrix(primTransform);
        const pxr::GfMatrix4d newTransform = NauUsdEditorMathUtils::nauMatrixToGfMatrix(transform(oldTransform, delta));
        NauUsdPrimUtils::setPrimWorldTransform(selected, newTransform);
    }
}

void NauTransformTool::init()
{
    auto selection = m_container->selection();

    // set gizmo pivot if there is a selected object
    if (!selection.empty()) {
        // get pivot from first object from selection
        auto primTransform = NauUsdPrimUtils::worldPrimTransform(selection.back());
        const nau::math::mat4 transform = NauUsdEditorMathUtils::gfMatrixToNauMatrix(primTransform);
        m_gizmo->activate(transform);
    }

    m_startedUsingCallbackId = m_gizmo->startedUsing.addCallback([this]() {
        startUse();
    });
    
    // Called on every gizmo update, doesn't generate an undo step
    m_applyDeltaCallbackId = m_gizmo->deltaUpdated.addCallback([this](nau::math::vec3 delta) {
        applyDeltaToSelection(delta);
    });

    // Called at the end of gizmo interactions, generates an undo step
    m_stoppedUsingCallbackId = m_gizmo->stoppedUsing.addCallback([this]() {
        stopUse();
    });

    auto selectionChangedLambda = [this](const NauUsdPrimsSelection& selection) {
        onSelectionChanged(selection);
    };
    m_selectionChangedCallbackId = m_container->selectionChangedDelegate.addCallback(selectionChangedLambda);
}

void NauTransformTool::terminate()
{
    m_gizmo->deactivate();
    m_gizmo->startedUsing.deleteCallback(m_startedUsingCallbackId);
    m_gizmo->deltaUpdated.deleteCallback(m_applyDeltaCallbackId);
    m_gizmo->stoppedUsing.deleteCallback(m_stoppedUsingCallbackId);
    m_container->selectionChangedDelegate.deleteCallback(m_selectionChangedCallbackId);
}

void NauTransformTool::startUse()
{
    m_originalSelectedObjectsTransform.clear();

    for (auto selected : m_container->selection()) {
        m_originalSelectedObjectsTransform[selected.GetPrimPath().GetString()] = NauUsdPrimUtils::localPrimTransform(selected);;
    }
}

void NauTransformTool::stopUse()
{
    for (auto selected : m_container->selection()) {
        const auto currentTransform = NauUsdPrimUtils::localPrimTransform(selected);
        const auto originalTransform = m_originalSelectedObjectsTransform[selected.GetPrimPath().GetString()];
        
        NauTransformTool::StopUsingDelegate.broadcast(selected.GetPath(), originalTransform, currentTransform);
    }

    m_originalSelectedObjectsTransform.clear();
}

void NauTransformTool::onSelectionChanged(const NauUsdPrimsSelection& selection)
{
    m_gizmo->deactivate();
    if (!selection.empty()) {
        auto primTransform = NauUsdPrimUtils::worldPrimTransform(selection.back());
        const nau::math::mat4 transform = NauUsdEditorMathUtils::gfMatrixToNauMatrix(primTransform);
        m_gizmo->activate(transform);
        
    }
}


// ** NauTranslateTool

NauTranslateTool::NauTranslateTool(NauUsdSelectionContainerPtr container)
    : NauTransformTool(std::make_unique<NauTranslateGizmo>(), container)
{
}

nau::math::mat4 NauTranslateTool::transform(const nau::math::mat4& transform, const nau::math::vec3& delta) const
{
    nau::math::mat4 newTransform = transform;
    newTransform.setCol3({
        std::clamp(newTransform[3][0] + delta.getX(), NAU_POSITION_MIN_LIMITATION, NAU_POSITION_MAX_LIMITATION),
        std::clamp(newTransform[3][1] + delta.getY(), NAU_POSITION_MIN_LIMITATION, NAU_POSITION_MAX_LIMITATION),
        std::clamp(newTransform[3][2] + delta.getZ(), NAU_POSITION_MIN_LIMITATION, NAU_POSITION_MAX_LIMITATION),
        newTransform[3][3]
    });

    return newTransform;
}


// ** NauRotateTool

NauRotateTool::NauRotateTool(NauUsdSelectionContainerPtr container)
    : NauTransformTool(std::make_unique<NauRotateGizmo>(), container)
{
}

nau::math::mat4 NauRotateTool::transform(const nau::math::mat4& transform, const nau::math::vec3& delta) const
{
    const nau::math::vec3 inverseDelta = -delta;
    nau::math::mat3 deltaRotateMatrix = nau::math::mat3::rotationZYX({
        inverseDelta.getX(),
        inverseDelta.getY(),
        inverseDelta.getZ()
    });

    Vectormath::SSE::length(transform.getCol0());

    const float sx = Vectormath::SSE::length(transform.getCol0());
    const float sy = Vectormath::SSE::length(transform.getCol1());
    const float sz = Vectormath::SSE::length(transform.getCol2());

    nau::math::mat3 rotateMatrix;
    rotateMatrix.setCol0(transform.getCol0().getXYZ() / sx);
    rotateMatrix.setCol1(transform.getCol1().getXYZ() / sy);
    rotateMatrix.setCol2(transform.getCol2().getXYZ() / sz);

    if (m_gizmo->coordinateSpace() == GizmoCoordinateSpace::Local)
        rotateMatrix *= deltaRotateMatrix;
    else
        rotateMatrix = deltaRotateMatrix * rotateMatrix;

    nau::math::mat4 newTransform;
    newTransform.setCol0(nau::math::vec4(rotateMatrix.getCol0(), 0));
    newTransform.setCol1(nau::math::vec4(rotateMatrix.getCol1(), 0));
    newTransform.setCol2(nau::math::vec4(rotateMatrix.getCol2(), 0));
    newTransform.setCol3(transform.getCol3());
    NauMathMatrixUtils::orthonormalize(newTransform);

    newTransform[0] *= sx;
    newTransform[1] *= sy;
    newTransform[2] *= sz;
    return newTransform;
}

// ** NauScaleTool

NauScaleTool::NauScaleTool(NauUsdSelectionContainerPtr container)
    : NauTransformTool(std::make_unique<NauScaleGizmo>(), container)
{
}

nau::math::mat4 NauScaleTool::transform(const nau::math::mat4& transform, const nau::math::vec3& delta) const
{
    // Scale differs from other operations in that its increment is multiplied by the current value
    // Therefore, it cannot be zero

    // TODO: Change the scale increment mechanism from multiplication to addition
    if (delta.getX() == 0 || delta.getY() == 0 || delta.getX() == 0) {
        return transform;
    }

    nau::math::vec3 deltaNormal = delta - nau::math::vec3(1, 1, 1);

    // Get the current size
    float scale[3] = {
        Vectormath::SSE::length(transform.getCol0()),
        Vectormath::SSE::length(transform.getCol1()),
        Vectormath::SSE::length(transform.getCol2())
    };

    nau::math::mat3 deltaScaleMatrix = {
        nau::math::vec3::zero(),
        nau::math::vec3::zero(),
        nau::math::vec3::zero()
    };

    // If the scale along one of the axes is greater than the limit, then we do not apply the change to it
    // Using this formula, we calculate the available increment factor, after which the final value will definitely not exceed the specified limits 
    deltaScaleMatrix[0][0] = std::clamp(scale[0] + deltaNormal.getX(), NAU_SCALE_MIN_LIMITATION, NAU_SCALE_MAX_LIMITATION);
    deltaScaleMatrix[1][1] = std::clamp(scale[1] + deltaNormal.getY(), NAU_SCALE_MIN_LIMITATION, NAU_SCALE_MAX_LIMITATION);
    deltaScaleMatrix[2][2] = std::clamp(scale[2] + deltaNormal.getZ(), NAU_SCALE_MIN_LIMITATION, NAU_SCALE_MAX_LIMITATION);

    nau::math::mat4 normalizedTransform = transform;
    NauMathMatrixUtils::orthonormalize(normalizedTransform);

    nau::math::mat3 axesMatrix;
    axesMatrix.setCol0(normalizedTransform.getCol0().getXYZ());
    axesMatrix.setCol1(normalizedTransform.getCol1().getXYZ());
    axesMatrix.setCol2(normalizedTransform.getCol2().getXYZ());

    nau::math::mat3 newAxesMatrix = axesMatrix * deltaScaleMatrix;
    nau::math::mat4 newTransform = transform;
    newTransform.setCol0(nau::math::vec4(newAxesMatrix.getCol0()));
    newTransform.setCol1(nau::math::vec4(newAxesMatrix.getCol1()));
    newTransform.setCol2(nau::math::vec4(newAxesMatrix.getCol2()));
    return newTransform;
}
