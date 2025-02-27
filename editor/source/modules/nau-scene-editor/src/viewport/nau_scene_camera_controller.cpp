// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_scene_camera_controller.hpp"

#include "nau/nau_constants.hpp"
#include "nau/math/nau_matrix_math.hpp"

#include "nau/editor-engine/nau_editor_engine_services.hpp"
#include "nau/viewport/nau_camera_manager.hpp"


// ** NauCameraZoomAction
//
// Zoom camera action

class NauCameraZoomAction : public NauCameraMoveActionAbstract
{
public:
    NauCameraZoomAction(float deltaTime, float zoomDelta, bool useCameraSpeed)
        : NauCameraMoveActionAbstract(deltaTime)
        , m_zoomDelta(zoomDelta)
        , m_useCameraSpeed(useCameraSpeed)
    {
    }
    virtual nau::math::Transform updateCameraMatrix(const nau::math::Transform& currentTransform, const float cameraSpeed, const float sensitivity) override;
private:
    const float m_zoomDelta = 0;
    const bool m_useCameraSpeed = false;
};


// ** NauCameraOrthoMoveAction
//
// Ortho moving camera action

class NauCameraOrthoMoveAction : public NauCameraMoveActionAbstract
{
public:
    NauCameraOrthoMoveAction(float deltaTime, const QVector3D& movementDir, const QPointF& mouseDelta)
        : NauCameraMoveActionAbstract(deltaTime)
        , m_movementDirection(movementDir.x(), movementDir.y(), movementDir.z())
        , m_mouseDelta(static_cast<float>(mouseDelta.x()), static_cast<float>(mouseDelta.y()))
    {
    }
    virtual nau::math::Transform updateCameraMatrix(const nau::math::Transform& currentTransform, const float cameraSpeed, const float sensitivity) override;

private:
    nau::math::vec3 m_movementDirection;
    nau::math::vec2 m_mouseDelta;
};


// ** NauCameraFlyMoveAction
//
// Fly movement camera action

class NauCameraFlyMoveAction : public NauCameraMoveActionAbstract
{
public:
    NauCameraFlyMoveAction(float deltaTime, const QVector3D& movementDir, const QPointF& mouseDelta)
        : NauCameraMoveActionAbstract(deltaTime)
        , m_movementDirection(movementDir.x(), movementDir.y(), movementDir.z())
        , m_mouseDelta(static_cast<float>(mouseDelta.x()), static_cast<float>(mouseDelta.y()))
    {
    }
    virtual nau::math::Transform updateCameraMatrix(const nau::math::Transform& currentTransform, const float cameraSpeed, const float sensitivity) override;
private:
    nau::math::vec3 m_movementDirection;
    nau::math::vec2 m_mouseDelta;
};


// ** NauCameraHorizontalMoveAction
//
// Horizontal movement camera action

class NauCameraHorizontalMoveAction : public NauCameraMoveActionAbstract
{
public:
    NauCameraHorizontalMoveAction(float deltaTime, const QVector3D& movementDir, const QPointF& mouseDelta)
        : NauCameraMoveActionAbstract(deltaTime)
        , m_movementDirection(movementDir.x(), movementDir.y(), movementDir.z())
        , m_mouseDelta(static_cast<float>(mouseDelta.x()), static_cast<float>(mouseDelta.y()))
    {
    }
    virtual nau::math::Transform updateCameraMatrix(const nau::math::Transform& currentTransform, const float cameraSpeed, const float sensitivity) override;
private:
    nau::math::vec3 m_movementDirection;
    nau::math::vec2 m_mouseDelta;
};

namespace Nau
{
    static nau::math::quat rotateQuaternion(const nau::math::quat& cameraRotation, const nau::math::vec2& mouseDelta, const float sensitivity) noexcept
    {
        using quat_t = nau::math::quat;
        using vec3_t = nau::math::vec3;
        
        const float mouseInputX = -mouseDelta.getX() * sensitivity;
        const float mouseInputY = -mouseDelta.getY() * sensitivity;
        const quat_t rotationX{ quat_t::rotationX(mouseInputY) };
        const quat_t rotationY{ quat_t::rotationY(mouseInputX) };
        const quat_t cameraRotationWithX = cameraRotation * rotationX;

        const float THRESHOLD = std::sin(PI / 180.f * 6.f);
        const vec3_t upDirection = nau::math::rotate(cameraRotationWithX, vec3_t::yAxis());
        if (Vectormath::SSE::dot(upDirection, vec3_t::yAxis()) < THRESHOLD) {
            return rotationY * cameraRotation;
        }

        return rotationY * cameraRotationWithX;
    }

    static nau::math::vec3 rotateDirection(const nau::math::quat& cameraRotation, const nau::math::vec3& direction) noexcept
    {
        using mat3_t = nau::math::mat3;

        const auto [pitch, yaw, roll] = NauMathMatrixUtils::convertQuaternionToRadiansYX(cameraRotation);
        const mat3_t rotationMat{ mat3_t::rotationY(yaw) * mat3_t::rotationX(pitch) };
        return rotationMat * direction;
    }
}

// ** NauCameraZoomAction

nau::math::Transform NauCameraZoomAction::updateCameraMatrix(const nau::math::Transform& currentTransform, const float cameraSpeed, const float sensitivity)
{
    using vec3_t = nau::math::vec3;

    float deltaForward = m_zoomDelta * m_deltaTime * 10;
    if (m_useCameraSpeed) {
        deltaForward *= cameraSpeed;
    }
    const vec3_t direction{ Nau::rotateDirection(currentTransform.getRotation(), vec3_t::zAxis() * deltaForward)};
    const vec3_t position = currentTransform.getTranslation() + direction;

    return { currentTransform.getRotation(), position, currentTransform.getScale() };
}


// ** NauCameraOrthoMoveAction

nau::math::Transform NauCameraOrthoMoveAction::updateCameraMatrix(const nau::math::Transform& currentTransform, const float cameraSpeed, const float sensitivity)
{
    using vec3_t = nau::math::vec3;

    const float force = m_deltaTime * cameraSpeed * 10;
    const vec3_t distance{ m_movementDirection * force };
    const vec3_t deltaOrtho{ vec3_t{ m_mouseDelta.getX(), -m_mouseDelta.getY(), 0.f } * m_deltaTime * cameraSpeed };
    const vec3_t direction{ -Nau::rotateDirection(currentTransform.getRotation(), distance - deltaOrtho)};
    const vec3_t cameraPosition{ currentTransform.getTranslation() + direction};

    return { currentTransform.getRotation(), cameraPosition, currentTransform.getScale()};
}


// ** NauCameraFlyMoveAction

nau::math::Transform NauCameraFlyMoveAction::updateCameraMatrix(const nau::math::Transform& currentTransform, const float cameraSpeed, const float sensitivity)
{
    using quat_t = nau::math::quat;
    using vec3_t = nau::math::vec3;

    const quat_t newCameraRotation = Nau::rotateQuaternion(currentTransform.getRotation(), -m_mouseDelta, sensitivity);
    const float force = m_deltaTime * cameraSpeed * 10;
    const vec3_t direction{ Nau::rotateDirection(newCameraRotation, m_movementDirection * force) };
    const vec3_t cameraPosition{ currentTransform.getTranslation() + direction};

    return { newCameraRotation, cameraPosition, currentTransform.getScale()};
}


// ** NauCameraHorizontalMoveAction

nau::math::Transform NauCameraHorizontalMoveAction::updateCameraMatrix(const nau::math::Transform& currentTransform, const float cameraSpeed, const float sensitivity)
{
    using quat_t = nau::math::quat;
    using vec3_t = nau::math::vec3;
    using mat3_t = nau::math::mat3;

    vec3_t cameraPosition{ currentTransform.getTranslation()};
    {
        const mat3_t rotationMatrix{ currentTransform.getRotation()};
        const float deltaForward = m_movementDirection.getZ() * m_deltaTime * cameraSpeed * 10;
        const vec3_t globalUpVec = vec3_t{ rotationMatrix.getCol2() }.setY(0.f);
        cameraPosition += globalUpVec * deltaForward;
    }
    const quat_t newCameraRotation = Nau::rotateQuaternion(currentTransform.getRotation(), -m_mouseDelta, sensitivity);

    return { newCameraRotation, cameraPosition, currentTransform.getScale()};
}


// ** NauSceneCameraController

void NauSceneCameraController::updateCameraMovement(float deltaTime, const NauViewportInput& input)
{
    const bool shiftPressed = input.isKeyDown(Qt::Key_Shift);
    const bool ctrlPressed = input.isKeyDown(Qt::Key_Control);
    const bool altPressed = input.isKeyDown(Qt::Key_Alt);

    const bool rmbPressed = input.isMouseButtonDown(Qt::MouseButton::RightButton);
    const bool mmbPressed = input.isMouseButtonDown(Qt::MouseButton::MiddleButton);

    const bool cameraWheelZooming = input.deltaWheel() != 0;
    const bool cameraAltZooming = altPressed && input.deltaMouse().isNull() && rmbPressed;

    if (m_cameraSlowdownTimeLeft > 0.0f) {
        auto moveAction = makeMoveAction(m_lastMoveAction.m_action, deltaTime, m_lastMoveAction.m_movementDirection, QPointF());

        QEasingCurve easing(QEasingCurve::OutInQuad);
        const float stoppingBoostPower = std::clamp(m_cameraSlowdownTimeLeft / EASING_DURATION, 0.0f, 1.0f);

        if (moveAction) {
            m_internalController.startCameraBoost(m_cameraLastBoost * stoppingBoostPower);
            m_internalController.updateMovement(moveAction.get());
            m_internalController.stopCameraBoost();
        }

        m_cameraSlowdownTimeLeft -= deltaTime;
    }

    // If camera inactive or we didn`t use wheel zooming
    // Do not update the camera matrix
    if (!(isCameraActive(input) || cameraAltZooming || cameraWheelZooming)) {
        if (m_cameraContiniousMovingTime > 0.0f) {
            if (m_internalController.cameraEasing()) {
                m_cameraSlowdownTimeLeft = EASING_DURATION;
            }
            m_cameraContiniousMovingTime = 0.0f;
            m_cameraAccelerationBoostPower = 0.0f;
            m_cameraEasingBoostPower = 0.0f;
        }
        return;
    }

    std::unique_ptr<NauCameraMoveActionAbstract> moveAction;
    if (cameraWheelZooming || cameraAltZooming) {
        float deltaZoom = input.deltaWheel();
        if (cameraAltZooming) {
            deltaZoom += input.deltaMouse().x() - input.deltaMouse().y();
        }
        moveAction = std::make_unique<NauCameraZoomAction>(deltaTime, -deltaZoom, cameraAltZooming);
    } else {
        float forwardDirection = 0;
        float rightDirection = 0;
        float upDirection = 0;
        
        if (input.isKeyDown(Qt::Key_W)) {
            forwardDirection -= 1.0;
        }
        if (input.isKeyDown(Qt::Key_S)) {
            forwardDirection += 1.0;
        }
        if (input.isKeyDown(Qt::Key_D)) {
            rightDirection += 1.0;
        }
        if (input.isKeyDown(Qt::Key_A)) {
            rightDirection -= 1.0;
        }
        if (input.isKeyDown(Qt::Key_E)) {
            upDirection += 1.0;
        }
        if (input.isKeyDown(Qt::Key_Q)) {
            upDirection -= 1.0;
        }

        QVector3D cameraMovementDirection(rightDirection, upDirection, forwardDirection);

        if (cameraMovementDirection.isNull()) {
            if (m_cameraContiniousMovingTime > 0.0f) {
                if (m_internalController.cameraEasing()) {
                    m_cameraSlowdownTimeLeft = EASING_DURATION;
                }
                m_cameraContiniousMovingTime = 0.0f;
                m_cameraAccelerationBoostPower = 0.0f;
                m_cameraEasingBoostPower = 0.0f;
            } else {
                m_cameraEasingBoostPower = 1.0f;
            }
        }

        if (cameraMovementDirection.isNull() && input.deltaMouse().isNull()) {
            return;
        }

        Action action;
        if (isCameraActive(input)) {
            if (mmbPressed) {
                action = Action::OrthoCameraMoving;
            }

            if (rmbPressed && ctrlPressed) {
                action = Action::FreeHorizontalCameraMoving;
            }

            if (rmbPressed && !ctrlPressed) {
                action = Action::FreeCameraMoving;
            }
        }

        moveAction = makeMoveAction(action, deltaTime, cameraMovementDirection, input.deltaMouse());

        m_lastMoveAction = { action, cameraMovementDirection };

        if (!cameraMovementDirection.isNull()) {
            m_cameraSlowdownTimeLeft = 0.0f;
            m_cameraContiniousMovingTime += deltaTime;
            if (m_internalController.cameraEasing()) {
                if (m_cameraContiniousMovingTime - deltaTime < EASING_DURATION) {
                    m_cameraEasingBoostPower = m_easingCurve.valueForProgress(m_cameraContiniousMovingTime / EASING_DURATION);
                }
            }

            if (m_internalController.cameraAcceleration()) {
                const float accelerationTime = m_cameraContiniousMovingTime - ACCELERATION_START_THRESHOLD;
                if (accelerationTime > 0 && (accelerationTime - deltaTime) < ACCELERATION_DURATION) {
                    const float accelerationBoostPercent = m_accelerationCurve.valueForProgress(accelerationTime / ACCELERATION_DURATION);
                    m_cameraAccelerationBoostPower = std::clamp(accelerationBoostPercent * (SHIFT_BOOST_POWER - 1.0f), 0.0f, (SHIFT_BOOST_POWER - 1.0f));
                }
            }
        }
    }

    if (moveAction) {
        const float boostPower = calculateCameraBoost(shiftPressed);
        m_cameraLastBoost = boostPower;
        m_internalController.startCameraBoost(boostPower);
        m_internalController.updateMovement(moveAction.get());
        m_internalController.stopCameraBoost();
    }
}

bool NauSceneCameraController::isCameraActive(const NauViewportInput& input) const
{
    const bool mmbPressed = input.isMouseButtonDown(Qt::MouseButton::MiddleButton);
    const bool rmbPressed = input.isMouseButtonDown(Qt::MouseButton::RightButton);

    return mmbPressed || rmbPressed;
}

NauSceneEditorCameraControllerInternal& NauSceneCameraController::internalController()
{
    return m_internalController;
}

void NauSceneCameraController::changeCameraSpeed(float deltaSpeed)
{
    m_internalController.changeCameraSpeed(deltaSpeed);
}

void NauSceneCameraController::changeCameraSensitivity(float sensitivity)
{
    m_internalController.changeCameraSensitivity(sensitivity);
}

void NauSceneCameraController::focusOn(const nau::math::mat4& matrix, int distanceMeters )
{
    m_internalController.focusOn(matrix, distanceMeters);
}

std::unique_ptr<NauCameraMoveActionAbstract> NauSceneCameraController::makeMoveAction(Action action, float deltaTime, const QVector3D& direction, QPointF deltaMouse) const
{
    switch (action)
    {
    case Action::OrthoCameraMoving:
        return std::make_unique<NauCameraOrthoMoveAction>(deltaTime, direction, deltaMouse);
    case Action::FreeHorizontalCameraMoving:
        return std::make_unique<NauCameraHorizontalMoveAction>(deltaTime, direction, deltaMouse);
    case Action::FreeCameraMoving:
        return std::make_unique<NauCameraFlyMoveAction>(deltaTime, direction, deltaMouse);
    default:
        return nullptr;
    }
}

float NauSceneCameraController::calculateCameraBoost(bool shiftPressed) const
{
    const float baseBoostPower = m_internalController.cameraEasing() ? m_cameraEasingBoostPower : 1.0f;
    float finalBoostPower = baseBoostPower;

    if (shiftPressed) {
        finalBoostPower *= SHIFT_BOOST_POWER;
    }

    if (m_internalController.cameraAcceleration()) {
        finalBoostPower += m_cameraAccelerationBoostPower;
    }

    return finalBoostPower;
}


// ** NauSceneEditorCameraControllerInternal

NauSceneEditorCameraControllerInternal::NauSceneEditorCameraControllerInternal()
    : m_currentCameraSpeed(1.0f)
    , m_currentCameraSensitivity(0.005f)
    , m_cameraBoost(1.0f)
    , m_cameraAcceleration(false)
    , m_cameraEasing(false)
{

}

void NauSceneEditorCameraControllerInternal::changeCameraSpeed(float deltaSpeed)
{
    m_currentCameraSpeed = std::clamp(deltaSpeed, CAMERA_MIN_SPEED, CAMERA_MAX_SPEED);
}

void NauSceneEditorCameraControllerInternal::changeCameraSensitivity(float sensitivity)
{
    m_currentCameraSensitivity = std::clamp(sensitivity, CAMERA_MIN_SENSITIVITY, CAMERA_MAX_SENSITIVITY);
}

void NauSceneEditorCameraControllerInternal::setCameraSpeed(float speed)
{
    m_currentCameraSpeed = std::clamp(speed, CAMERA_MIN_SPEED, CAMERA_MAX_SPEED);
}

float NauSceneEditorCameraControllerInternal::cameraSpeed() const
{
    return m_currentCameraSpeed * m_cameraBoost;
}

float NauSceneEditorCameraControllerInternal::cameraSensitivity() const
{
    return m_currentCameraSensitivity;
}

void NauSceneEditorCameraControllerInternal::setCameraMatrix(const QMatrix4x3& matrix)
{
    auto cameraManager = Nau::EditorEngine().cameraManager();
    if ((cameraManager == nullptr) || (cameraManager->activeCamera() == nullptr)) {
        return;
    }
    auto cameraObject = cameraManager->activeCamera();
    nau::math::Transform3 tf3 = nau::math::Transform3::identity();

    NauMathMatrixUtils::convertQMatrixToNauTransform(matrix, tf3);
    cameraObject->setTranslation(tf3.getTranslation());
    cameraObject->setRotation(nau::math::quat{ tf3.getUpper3x3() });
}

QMatrix4x3 NauSceneEditorCameraControllerInternal::cameraMatrix() const
{
    auto cameraManager = Nau::EditorEngine().cameraManager();
    if ((cameraManager == nullptr) || (cameraManager->activeCamera() == nullptr)) {
        QMatrix4x3 result;
        result.setToIdentity();
        return result;
    }
    auto cameraObject = cameraManager->activeCamera();
    auto matrix = cameraObject->getWorldTransform().getMatrix();

    auto col0 = matrix.getCol0().getXYZ();
    auto col1 = matrix.getCol1().getXYZ();
    auto col2 = matrix.getCol2().getXYZ();
    auto col3 = matrix.getCol3().getXYZ();
    return NauMathMatrixUtils::convertNauTransformToQMatrix(nau::math::Transform3(col0, col1, col2, col3));
}

float NauSceneEditorCameraControllerInternal::cameraFoV() const
{
    auto cameraManager = Nau::EditorEngine().cameraManager();
    if ((cameraManager == nullptr) || (cameraManager->activeCamera() == nullptr)) {
        return CAMERA_DEFAULT_FOV;
    }
    return cameraManager->activeCamera()->getFov();
}

void NauSceneEditorCameraControllerInternal::setCameraFoV(int fov)
{
    auto cameraManager = Nau::EditorEngine().cameraManager();
    if ((cameraManager == nullptr) || (cameraManager->activeCamera() == nullptr)) {
        return;
    }
    const auto newFov = static_cast<float>(std::clamp(fov, CAMERA_MIN_FOV, CAMERA_MAX_FOV));
    cameraManager->activeCamera()->setFov(newFov);
}

void NauSceneEditorCameraControllerInternal::setCameraClippingPlanes(const QVector2D& planes)
{
    auto cameraManager = Nau::EditorEngine().cameraManager();
    if ((cameraManager == nullptr) || (cameraManager->activeCamera() == nullptr)) {
        return;
    }
    cameraManager->activeCamera()->setClipNearPlane(planes[0]);
    cameraManager->activeCamera()->setClipFarPlane(planes[1]);
}

QVector2D NauSceneEditorCameraControllerInternal::cameraClippingPlanes() const
{
    QVector2D result{};
    auto cameraManager = Nau::EditorEngine().cameraManager();
    if ((cameraManager != nullptr) && (cameraManager->activeCamera() != nullptr)) {
        result.setX(cameraManager->activeCamera()->getClipNearPlane());
        result.setY(cameraManager->activeCamera()->getClipFarPlane());
    }
    return result;
}

void NauSceneEditorCameraControllerInternal::setCameraAcceleration(bool acceleration)
{
    m_cameraAcceleration = acceleration;
}

bool NauSceneEditorCameraControllerInternal::cameraAcceleration() const
{
    return m_cameraAcceleration;
}

void NauSceneEditorCameraControllerInternal::setCameraEasing(bool easing)
{
    m_cameraEasing = easing;
}

bool NauSceneEditorCameraControllerInternal::cameraEasing() const
{
    return m_cameraEasing;
}

void NauSceneEditorCameraControllerInternal::focusOn(const nau::math::mat4& matrix, int distanceMeters)
{
    auto cameraManager = Nau::EditorEngine().cameraManager();
    auto activeCamera = cameraManager->activeCamera();
    if (activeCamera == nullptr) {
        return;
    }
    const nau::math::vec3 objectPosition = matrix.getTranslation();
    const nau::math::vec3 eyeDirection = Vectormath::SSE::rotate(activeCamera->getRotation(), nau::math::vec3::zAxis());

    activeCamera->setTranslation(objectPosition + static_cast<float>(distanceMeters) * eyeDirection);
}

void NauSceneEditorCameraControllerInternal::updateMovement(NauCameraMoveActionAbstract* action)
{
    auto cameraManager = Nau::EditorEngine().cameraManager();
    auto activeCamera = cameraManager->activeCamera();
    if (activeCamera == nullptr) {
        return;
    }
    
    const auto newCameraMatrix = action->updateCameraMatrix(activeCamera->getTransform(), cameraSpeed(), m_currentCameraSensitivity);

    activeCamera->setTranslation(newCameraMatrix.getTranslation());
    activeCamera->setRotation(newCameraMatrix.getRotation());
}

void NauSceneEditorCameraControllerInternal::startCameraBoost(float power)
{
    m_cameraBoost = power;
}

void NauSceneEditorCameraControllerInternal::stopCameraBoost()
{
    m_cameraBoost = 1.0;
}
