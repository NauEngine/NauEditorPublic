// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Camera controller implementaion for scene editor

#pragma once

#include "nau/viewport/nau_camera_controller.hpp"
#include "nau/math/transform.h"

#include <QEasingCurve>
#include <QGenericMatrix>
#include <QVector3D>


// ** NauCameraMoveActionAbstract
//
// Movement camera action abstract

class NauCameraMoveActionAbstract
{
public:
    NauCameraMoveActionAbstract(float deltaTime)
        : m_deltaTime(deltaTime)
    {

    }
    virtual ~NauCameraMoveActionAbstract() = default;
    virtual nau::math::Transform updateCameraMatrix(const nau::math::Transform& currentTransform, const float cameraSpeed, const float sensitivity) = 0;
protected:
    const float m_deltaTime = 0;
};


// ** NauSceneEditorCameraControllerInternal
//
// Provides editor camera control methods

class NauSceneEditorCameraControllerInternal
{
public:
    NauSceneEditorCameraControllerInternal();
    ~NauSceneEditorCameraControllerInternal() = default;

    void updateMovement(NauCameraMoveActionAbstract* action);
    void startCameraBoost(float power);
    void stopCameraBoost();
    void changeCameraSpeed(float deltaSpeed);
    void changeCameraSensitivity(float sensitivity);
    void focusOn(const nau::math::mat4& matrix, int distanceMeters = 5);

    void setCameraSpeed(float speed);
    float cameraSpeed() const;
    float cameraSensitivity() const;
    void setCameraMatrix(const QMatrix4x3& matrix);
    QMatrix4x3 cameraMatrix() const;
    void setCameraFoV(int fov);
    float cameraFoV() const;
    void setCameraClippingPlanes(const QVector2D& planes);
    QVector2D cameraClippingPlanes() const;
    void setCameraAcceleration(bool acceleration);
    bool cameraAcceleration() const;
    void setCameraEasing(bool easing);
    bool cameraEasing() const;

private:
    float m_currentCameraSpeed;
    float m_currentCameraSensitivity;
    float m_cameraBoost;
    bool m_cameraAcceleration;
    bool m_cameraEasing;
};


// ** NauSceneCameraController
// Implements camera controller interface for scene editor
enum class Action {
    OrthoCameraMoving,
    FreeHorizontalCameraMoving,
    FreeCameraMoving
};

struct MovementAction {
    Action m_action;
    QVector3D m_movementDirection;
};

class NauSceneCameraController final : public NauCameraControllerInterface
{
public:
    void updateCameraMovement(float deltaTime, const NauViewportInput& input) override;
    void changeCameraSpeed(float deltaSpeed) override;
    void changeCameraSensitivity(float sensitivity) override;
    void focusOn(const nau::math::mat4& matrix, int distanceMeters = 5) override;

    bool isCameraActive(const NauViewportInput& input) const override;

    // TODO: Delete internal controller getter
    NauSceneEditorCameraControllerInternal& internalController();

private:
    std::unique_ptr<NauCameraMoveActionAbstract> makeMoveAction(Action action, float deltaTime, const QVector3D & direction, QPointF deltaMouse) const;
    float calculateCameraBoost(bool shiftPressed) const;

private:
    float m_cameraContiniousMovingTime = 0.0f;
    float m_cameraAccelerationBoostPower = 0.0f;
    float m_cameraEasingBoostPower = 0.0f;
    float m_cameraLastBoost = 0.0f;
    float m_cameraSlowdownTimeLeft = 0.0f;

    QEasingCurve m_easingCurve = QEasingCurve(QEasingCurve::OutInQuad);
    QEasingCurve m_accelerationCurve = QEasingCurve(QEasingCurve::InOutQuad);

    MovementAction m_lastMoveAction;

    NauSceneEditorCameraControllerInternal m_internalController;
};