// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Viewport camera setting window

#pragma once

#include "baseWidgets/nau_widget.hpp"
#include "inspector/nau_object_inspector.hpp"

#include <QObject>
#include <QJsonObject>
#include <QTimer>


// ** NauSceneCameraViewSettings
//
// Viewport camera settings for View, Fov, Clipping planes

class NauSceneCameraViewSettings : public NauWidget
{
    Q_OBJECT

    friend class NauSceneCameraSettingsWidget;

public:
    NauSceneCameraViewSettings(QWidget* parent = nullptr);

    void updateView();

private:
    NauLayoutVertical* m_layout;

    NauPropertyPoint2* m_clipping;
    //NauPropertyString* m_view;
    NauPropertyInt* m_fov;

private:
    inline static constexpr float DefaultClippingNear = 0.f;
    inline static constexpr float DefaultClippingFar = 1000.f;
    inline static constexpr int DefaultFov = 90;

    inline static constexpr QSize ClippingWidgetSize = { 88, 32 };
    inline static constexpr QMargins WidgetMargins = { 16, 0, 16, 0 };
};


// ** NauSceneCameraMovementSettings
//
// Viewport camera settings for Speed, Easing, Acceleration

class NauSceneCameraMovementSettings : public NauWidget
{
    Q_OBJECT

    friend class NauSceneCameraSettingsWidget;

public:
    NauSceneCameraMovementSettings(QWidget* parent = nullptr);

    void updateMovement();

private:
    NauLayoutVertical* m_layout;

    NauPropertyReal* m_speed;
    NauPropertyReal* m_sensitivity;
    NauPropertyBool* m_easing;
    NauPropertyBool* m_acceleration;

private:
    inline static constexpr float DefaultSpeed = 1.f;
    inline static constexpr float DefaultSensitivity = 0.5f;
    inline static constexpr bool DefaultEasing = false;
    inline static constexpr bool DefaultAcceleration = false;
    inline static constexpr QMargins WidgetMargins = { 16, 0, 16, 0 };
};


// ** NauSceneCameraTransformSettings
//
// Viewport camera settings for Position, Rotation

class NauSceneCameraTransformSettings : public NauWidget
{
    Q_OBJECT

    friend class NauSceneCameraSettingsWidget;

public:
    NauSceneCameraTransformSettings(NauWidget* parent = nullptr);

    void updateTransform();
    void updateCameraTransform();

private:
    NauLayoutVertical* m_layout;
    NauLayoutGrid* m_contentLayout;

    NauMultiValueDoubleSpinBox* m_position;
    NauMultiValueDoubleSpinBox* m_rotation;
    NauToolButton* m_revertPositionButton;
    NauToolButton* m_revertRotationButton;
    QMatrix4x3 m_transformCache;

private:
    inline static constexpr int OuterMargin = 6;
    inline static constexpr int VerticalSpacer = 12;
    inline static constexpr int MultiValueSpinBoxSize = 352;
    inline static constexpr QSize ButtonSize = { 16, 16 };
};


// ** NauSceneCameraHeaderWidget
//
// Viewport camera settings header widget

class NauSceneCameraHeaderWidget : public NauWidget
{
    Q_OBJECT

public:
    NauSceneCameraHeaderWidget(NauWidget* parent = nullptr);

private:
    NauLayoutHorizontal* m_layout;

    NauLabel* m_headerLabel;
    NauToolButton* m_helpButton;
};


// ** NauSceneCameraSettingsWidget
//
// Viewport camera settings widget

class NauSceneCameraSettingsWidget : public NauWidget
{
    Q_OBJECT

public:
    NauSceneCameraSettingsWidget(NauWidget* parent);
    ~NauSceneCameraSettingsWidget();

    void updateSettings(bool force = false);

    QJsonObject save() const;
    void load(const QJsonObject& data);
    void setControlButton(NauToolButton* button);

signals:
    void onViewChanged();
    void onTransformChanged();
    void onMovementChanged();
    void close();

private:
    [[nodiscard]]
    bool buttonClicked() const noexcept;

    void closeEvent(QCloseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QTimer m_updateUiTimer;

    NauToolButton* m_button;
    NauLayoutVertical* m_layout;

    NauSceneCameraHeaderWidget* m_header;
    //NauPropertyString* m_preset;
    NauSceneCameraViewSettings* m_view;
    NauSceneCameraTransformSettings* m_transform;
    NauSceneCameraMovementSettings* m_movement;
};
