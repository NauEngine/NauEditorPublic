// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_scene_camera_settings.hpp"

#include "nau_assert.hpp"
#include "baseWidgets/nau_label.hpp"
#include "nau_settings.hpp"
#include "baseWidgets/nau_static_text_label.hpp"
#include "baseWidgets/nau_widget_utility.hpp"

#include "inspector/nau_object_inspector.hpp"
#include "nau/math/nau_matrix_math.hpp"
#include "themes/nau_theme.hpp"
#include "nau/editor-engine/nau_editor_engine_services.hpp"

// TODO: Do not use camera controller here
// Bind UI changes to camera in NauUsdSceneEditor class
#include "../viewport/nau_scene_camera_controller.hpp"
#include "nau/nau_usd_scene_editor.hpp"
#include "nau/nau_editor_plugin_manager.hpp"

#include <QSignalBlocker>

#include "nau/shared/logger.h"


// ** NauSceneCameraViewSettings

NauSceneCameraViewSettings::NauSceneCameraViewSettings(QWidget* parent)
    : NauWidget(parent)
    , m_layout(new NauLayoutVertical(this))
    , m_clipping(new NauPropertyPoint2("", tr("Min"), tr("Max"))) // TODO: Fix the handling of the first parameter in the future
    //, m_view(new NauPropertyString(NauStringViewAttributeType::DEFAULT))
    , m_fov(new NauPropertyInt())
{
    // *** View ***

    //m_view->setLabel(tr("View"));
    //m_view->setValue(tr("Perspective"));
    //m_view->setContentsMargins(WidgetMargins);
    //m_view->setDisabled(true);

    // *** Fov ***

    m_fov->setContentsMargins(WidgetMargins);
    m_fov->setLabel(tr("FoV"));
    m_fov->setRange(CAMERA_MIN_FOV, CAMERA_MAX_FOV);
    m_fov->setParent(this);

    connect(m_fov, &NauPropertyInt::eventValueChanged, [this]()
    {
        const int fov = m_fov->getValue().convert<int>();
        // TODO: Do not use camera controller here
        // Bind UI changes to camera in NauUsdSceneEditor class
        auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
        auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
        if (sceneCameraController) {
            sceneCameraController->internalController().setCameraFoV(fov);
        }
    });

    m_fov->setValue(DefaultFov);

    // *** Clipping ***

    m_clipping->setLabel(tr("Clipping Plane"));
    m_clipping->setFixedWidth(384);
    m_clipping->setContentsMargins(WidgetMargins);
    m_clipping->setValue(QVector2D(DefaultClippingNear, DefaultClippingFar));
    m_clipping->setDisabled(false);
    m_clipping->setParent(this);

    connect(m_clipping, &NauPropertyPoint2::eventValueChanged, [this]()
    {
        const QVector2D clipping = m_clipping->getValue().convert<QVector2D>();
        // TODO: Do not use camera controller here
        // Bind UI changes to camera in NauUsdSceneEditor class
        auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
        auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
        if (sceneCameraController) {
            sceneCameraController->internalController().setCameraClippingPlanes(clipping);
        };
    });

    //m_layout->addWidget(m_view);
    //m_layout->addSpacing(12);
    m_layout->addWidget(m_fov);
    m_layout->addSpacing(12);
    m_layout->addWidget(m_clipping);
}

void NauSceneCameraViewSettings::updateView()
{
    // TODO: Do not use camera controller here
    // Bind UI changes to camera in NauUsdSceneEditor class
    auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
    auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
    if (sceneCameraController) {
        QSignalBlocker blockerFov { m_fov };
        QSignalBlocker blockerClipping { m_clipping };
        m_fov->setValue(static_cast<int>(sceneCameraController->internalController().cameraFoV()));
        m_clipping->setValue(sceneCameraController->internalController().cameraClippingPlanes());
    };
}


// ** NauSceneCameraTransformSettings

NauSceneCameraTransformSettings::NauSceneCameraTransformSettings(NauWidget* parent)
    : NauWidget(parent)
    , m_layout(new NauLayoutVertical(this))
    , m_contentLayout(new NauLayoutGrid())
    , m_position(new NauMultiValueDoubleSpinBox(this, 3))
    , m_rotation(new NauMultiValueDoubleSpinBox(this, 3))
    , m_revertPositionButton(new NauToolButton(this))
    , m_revertRotationButton(new NauToolButton(this))
{
    const auto setupSpinBoxSizes = [](NauMultiValueDoubleSpinBox* spinBox) {
        for (int i = 0; i < 3; ++i) {
            (*spinBox)[i]->setFixedWidth(94);
        }
    };

    m_position->setFixedWidth(MultiValueSpinBoxSize);
    m_rotation->setFixedWidth(MultiValueSpinBoxSize);

    setContentsMargins(16, 0, 16, 0);
    m_layout->addLayout(m_contentLayout);

    // The current limit is set by the size of the data type that is used
    m_position->setMinimum(NAU_POSITION_MIN_LIMITATION);
    m_position->setMaximum(NAU_POSITION_MAX_LIMITATION);

    m_rotation->setMinimum(std::numeric_limits<float>::lowest());
    m_rotation->setMaximum(std::numeric_limits<float>::max());

    // This is a temporary solution to keep the real values accurate enough when working with the editor and to avoid nulling the transform matrix
    // TODO: Fix the matrix nulling in another more correct way.
    // Most likely on the engine side
    constexpr int transformDecimalPrecision = 2;
    m_position->setDecimals(transformDecimalPrecision);
    m_rotation->setDecimals(transformDecimalPrecision);

    constexpr int zSpinboxIdx = 2;
    m_rotation->disableSpinbox(zSpinboxIdx);

    setupSpinBoxSizes(m_position);
    setupSpinBoxSizes(m_rotation);

    auto labelPosition = new NauStaticTextLabel(tr("Position"));
    labelPosition->setFont(Nau::Theme::current().fontObjectInspector());
    m_contentLayout->addWidget(labelPosition, 0, 0, Qt::AlignLeft);
    m_revertPositionButton->setIcon(Nau::Theme::current().iconUndoAction());
    m_revertPositionButton->setIconSize(ButtonSize);
    m_contentLayout->addWidget(m_revertPositionButton, 0, 1, Qt::AlignRight);
    m_contentLayout->addItem(new QSpacerItem(0, OuterMargin), 1, 0);
    m_contentLayout->addWidget(m_position, 2, 0, Qt::AlignLeft);
    m_contentLayout->addItem(new QSpacerItem(0, VerticalSpacer), 3, 0);

    auto labelRotation = new NauStaticTextLabel(tr("Rotation"));
    labelRotation->setFont(Nau::Theme::current().fontObjectInspector());;
    m_contentLayout->addWidget(labelRotation, 4, 0, Qt::AlignLeft);
    m_revertRotationButton->setIcon(Nau::Theme::current().iconUndoAction());
    m_revertRotationButton->setIconSize(ButtonSize);
    m_contentLayout->addWidget(m_revertRotationButton, 4, 1, Qt::AlignRight);
    m_contentLayout->addItem(new QSpacerItem(0, OuterMargin), 5, 0);
    m_contentLayout->addWidget(m_rotation, 6, 0, Qt::AlignLeft);

    connect(m_position, &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauSceneCameraTransformSettings::updateCameraTransform);
    connect(m_rotation, &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauSceneCameraTransformSettings::updateCameraTransform);

    connect(m_revertPositionButton, &NauToolButton::clicked, [this] {
        QMatrix3x3 trsComposition;

        QSignalBlocker blocker{ m_position };
        for (int i = 0; i < 3; ++i) {
            trsComposition.data()[i] = 0.0f;
            trsComposition.data()[i + 3] = ((*m_rotation)[i]->value());
            trsComposition.data()[i + 6] = 1.0f;
            (*m_position)[i]->setValue(0.f);
        }

        m_transformCache = NauMathMatrixUtils::TRSCompositionToMatrix(trsComposition);
        
        Nau::EditorEngine().cameraManager()->activeCamera()->setTranslation(nau::math::vec3::zero());
    });

    connect(m_revertRotationButton, &NauToolButton::clicked, [this] {
        QMatrix3x3 trsComposition;

        QSignalBlocker blocker{ m_rotation };
        for (int i = 0; i < 3; ++i) {
            trsComposition.data()[i] = ((*m_position)[i]->value());
            trsComposition.data()[i + 3] = 0.0f;
            trsComposition.data()[i + 6] = 1.0f;
            (*m_rotation)[i]->setValue(0.f);
        }

        m_transformCache = NauMathMatrixUtils::TRSCompositionToMatrix(trsComposition);

        Nau::EditorEngine().cameraManager()->activeCamera()->setRotation(nau::math::quat::identity());
    });
}

void NauSceneCameraTransformSettings::updateTransform()
{
    // TODO: Do not use camera controller here
    // Bind UI changes to camera in NauUsdSceneEditor class
    auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
    auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
    if (!sceneCameraController) {
        return;
    }

    const QMatrix4x3 cameraMatrix = sceneCameraController->internalController().cameraMatrix();
    if (cameraMatrix == m_transformCache) {
        return;
    }
    const QMatrix3x3 trsComposition = NauMathMatrixUtils::MatrixToTRSComposition(cameraMatrix);

    QSignalBlocker positionBlocker(m_position);
    QSignalBlocker rotationBlocker(m_rotation);
    for (int i = 0; i < 3; ++i) {
        (*m_position)[i]->setValue(trsComposition.data()[i]);
        (*m_rotation)[i]->setValue(trsComposition.data()[i + 3]);
    }
    m_transformCache = cameraMatrix;
}

void NauSceneCameraTransformSettings::updateCameraTransform()
{
    QMatrix3x3 trsComposition;

    for (int i = 0; i < 3; ++i) {
        trsComposition.data()[i] = static_cast<float>((*m_position)[i]->value());
        trsComposition.data()[i + 3] = static_cast<float>((*m_rotation)[i]->value());
        trsComposition.data()[i + 6] = 1.f;
    }

    m_transformCache = NauMathMatrixUtils::TRSCompositionToMatrix(trsComposition);

    // TODO: Do not use camera controller here
    // Bind UI changes to camera in NauUsdSceneEditor class
    auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
    auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
    if (sceneCameraController) {
        sceneCameraController->internalController().setCameraMatrix(m_transformCache);
    }
}


// ** NauSceneCameraMovementSettings

NauSceneCameraMovementSettings::NauSceneCameraMovementSettings(QWidget* parent)
    : NauWidget(parent)
    , m_layout(new NauLayoutVertical(this))
    , m_speed(new NauPropertyReal())
    , m_sensitivity(new NauPropertyReal())
    , m_easing(new NauPropertyBool())
    , m_acceleration(new NauPropertyBool())
{
    // *** Speed ***
    m_speed->setContentsMargins(WidgetMargins);
    m_speed->setLabel(tr("Speed"));
    m_speed->setRange(CAMERA_MIN_SPEED, CAMERA_MAX_SPEED);
    m_speed->setSingleStep(CAMERA_MIN_SPEED);
    m_speed->setParent(this);

    connect(m_speed, &NauPropertyReal::eventValueChanged, [this]()
    {
        const float speed = m_speed->getValue().convert<float>();
        // TODO: Do not use camera controller here
        // Bind UI changes to camera in NauUsdSceneEditor class
        auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
        auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
        if (sceneCameraController) {
            sceneCameraController->changeCameraSpeed(speed);
        }
    });

    m_speed->setValue(DefaultSpeed);

    // *** Sensitivity ***
    m_sensitivity->setContentsMargins(WidgetMargins);
    m_sensitivity->setLabel(tr("Sensitivity"));
    m_sensitivity->setRange(0.0f, 1.0f);
    m_sensitivity->setSingleStep(0.01f);
    m_sensitivity->setParent(this);

    connect(m_sensitivity, &NauPropertyReal::eventValueChanged, [this]()
    {
        const float sensitivity = m_sensitivity->getValue().convert<float>();
        const float convertedSensitivity = sensitivity * CAMERA_SENSITIVITY_MULTIPLIER;
        // TODO: Do not use camera controller here
        // Bind UI changes to camera in NauUsdSceneEditor class
        auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
        auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
        if (sceneCameraController) {
            sceneCameraController->changeCameraSensitivity(convertedSensitivity);
        }
    });
    
    m_sensitivity->setValue(DefaultSensitivity);

    // *** Easing ***
    m_easing->setLabel("Easing");
    m_easing->setParent(this);
    m_easing->setContentsMargins(WidgetMargins);

    connect(m_easing, &NauPropertyBool::eventValueChanged, [this]()
    {
        const bool easing = m_easing->getValue().convert<bool>();
        // TODO: Do not use camera controller here
        // Bind UI changes to camera in NauUsdSceneEditor class
        auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
        auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
        if (sceneCameraController) {
            sceneCameraController->internalController().setCameraEasing(easing);
        }
    });

    m_easing->setValue(DefaultEasing);

    // *** Acceleration ***

    m_acceleration->setLabel("Acceleration");
    m_acceleration->setParent(this);
    m_acceleration->setContentsMargins(WidgetMargins);

    connect(m_acceleration, &NauPropertyBool::eventValueChanged, [this]()
    {
        const bool acceleration = m_acceleration->getValue().convert<bool>();
        // TODO: Do not use camera controller here
        // Bind UI changes to camera in NauUsdSceneEditor class
        auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
        auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
        if (sceneCameraController) {
            sceneCameraController->internalController().setCameraAcceleration(acceleration);
        }
    });

    m_acceleration->setValue(DefaultAcceleration);

    m_layout->addWidget(m_speed);
    m_layout->addWidget(m_sensitivity);
    m_layout->addWidget(m_easing);
    m_layout->addWidget(m_acceleration);
}

void NauSceneCameraMovementSettings::updateMovement()
{
    // TODO: Do not use camera controller here
    // Bind UI changes to camera in NauUsdSceneEditor class
    auto& sceneEditor = Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
    auto sceneCameraController = dynamic_cast<NauSceneCameraController*>(sceneEditor.sceneCameraController().get());
    if (!sceneCameraController) {
        return;
    }

    QSignalBlocker speedBlocker(m_speed);
    QSignalBlocker sensitivityBlocker(m_sensitivity);
    QSignalBlocker easingBlocker(m_easing);
    QSignalBlocker accelerationBlocker(m_acceleration);

    const float speed = sceneCameraController->internalController().cameraSpeed();
    m_speed->setValue(speed);

    const float sensitivity = sceneCameraController->internalController().cameraSensitivity();
    m_sensitivity->setValue(sensitivity * 10.0f);

    const bool easing = sceneCameraController->internalController().cameraEasing();
    m_easing->setValue(easing);

    const bool acceleration = sceneCameraController->internalController().cameraAcceleration();
    m_acceleration->setValue(acceleration);
}


// ** NauSceneCameraSettingsWidget

NauSceneCameraSettingsWidget::NauSceneCameraSettingsWidget(NauWidget* parent)
    : NauWidget(parent)
    , m_button(nullptr)
    , m_layout(new NauLayoutVertical(this))
    , m_header(new NauSceneCameraHeaderWidget(this))
    //, m_preset(new NauPropertyString(NauStringViewAttributeType::DEFAULT))
    , m_view(new NauSceneCameraViewSettings(this))
    , m_transform(new NauSceneCameraTransformSettings(this))
    , m_movement(new NauSceneCameraMovementSettings(this))
{
    setWindowFlags(Qt::Popup);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFixedWidth(400);
    setContentsMargins(8, 0, 8, 16);

    constexpr QColor lineColor(27, 27, 27, 127);
    constexpr int lineWidth(1);
    constexpr QSize lineSize = { 384, 32 };
    constexpr int lineOffset = 16;

    auto lineAfterPreset = new NauLineWidget(lineColor, lineWidth, Qt::Orientation::Horizontal, this);
    lineAfterPreset->setFixedSize(lineSize);
    lineAfterPreset->setOffset(lineOffset);

    auto lineAfterView = new NauLineWidget(lineColor, lineWidth, Qt::Orientation::Horizontal, this);
    lineAfterView->setFixedSize(lineSize);
    lineAfterView->setOffset(lineOffset);

    auto lineAfterTransform = new NauLineWidget(lineColor, lineWidth, Qt::Orientation::Horizontal, this);
    lineAfterTransform->setFixedSize(lineSize);
    lineAfterTransform->setOffset(lineOffset);

    // *** Preset ***
    //m_preset->setLabel(tr("Preset"));
    //m_preset->setContentsMargins(16, 0, 16, 0);
    //m_preset->setValue(tr("Default"));
    //m_preset->setDisabled(true);

    m_layout->addWidget(m_header);
    //m_layout->addWidget(m_preset);
    //m_layout->addWidget(lineAfterPreset);
    m_layout->addWidget(m_view);
    m_layout->addWidget(lineAfterView);
    m_layout->addWidget(m_transform);
    m_layout->addWidget(lineAfterTransform);
    m_layout->addWidget(m_movement);

    connect(&m_updateUiTimer, &QTimer::timeout, [this] { updateSettings(); });
    m_updateUiTimer.start(16);
}

NauSceneCameraSettingsWidget::~NauSceneCameraSettingsWidget()
{
    m_updateUiTimer.stop();
}

void NauSceneCameraSettingsWidget::updateSettings(bool force)
{
    if (!this->isVisible() && !force) {
        return;
    }

    // Temp hack
    // TODO: Link camera settings with Scene Editor
    auto controller = Nau::EditorEngine().viewportManager()->mainViewport()->controller();
    if (!controller) {
        return;
    }

    // If playmode
    // TODO: Deactivate camera setting in playmode
    if (auto editorController = dynamic_cast<NauBaseEditorViewportController*>(controller.get()); !editorController) {
        return;
    }

    QSignalBlocker transformBlocker(m_transform);
    m_transform->updateTransform();
    QSignalBlocker movementBlocker(m_movement);
    m_movement->updateMovement();
    QSignalBlocker viewBlocker(m_view);
    m_view->updateView();
}

QJsonObject NauSceneCameraSettingsWidget::save() const
{
    QJsonObject result;

    // FoV
    result["fov"] = m_view->m_fov->getValue().convert<int>();

    // Clipping
    QJsonObject clippingJSON;
    const QVector2D clipping = m_view->m_clipping->getValue().convert<QVector2D>();
    clippingJSON["near"] = clipping.x();
    clippingJSON["far"]  = clipping.y();
    result["clipping"] = clippingJSON;

    // Position
    QJsonObject positionJSON;
    positionJSON["x"] = (*m_transform->m_position)[0]->value();
    positionJSON["y"] = (*m_transform->m_position)[1]->value();
    positionJSON["z"] = (*m_transform->m_position)[2]->value();
    result["position"] = positionJSON;

    // Rotation
    QJsonObject rotationJSON;
    rotationJSON["x"] = (*m_transform->m_rotation)[0]->value();
    rotationJSON["y"] = (*m_transform->m_rotation)[1]->value();
    rotationJSON["z"] = (*m_transform->m_rotation)[2]->value();
    result["rotation"] = rotationJSON;

    // Speed
    result["speed"] = m_movement->m_speed->getValue().convert<float>();

    // Sensitivity
    result["sensitivity"] = m_movement->m_sensitivity->getValue().convert<float>();

    // Easing
    result["easing"] = m_movement->m_easing->getValue().convert<bool>();

    // Acceleration
    result["acceleration"] = m_movement->m_acceleration->getValue().convert<bool>();

    return result;    
}

void NauSceneCameraSettingsWidget::load(const QJsonObject& data)
{
    // FoV
    m_view->m_fov->setValue(data["fov"].toInt());

    // Clipping
    const QJsonObject clippingJSON = data["clipping"].toObject();
    const QVector2D clipping = { 
        static_cast<float>(clippingJSON["near"].toDouble()), 
        static_cast<float>(clippingJSON["far"].toDouble())
    };
    m_view->m_clipping->setValue(clipping);

    // Position
    const QJsonObject positionJSON = data["position"].toObject();
    (*m_transform->m_position)[0]->setValue(positionJSON["x"].toDouble());
    (*m_transform->m_position)[1]->setValue(positionJSON["y"].toDouble());
    (*m_transform->m_position)[2]->setValue(positionJSON["z"].toDouble());

    // Rotation
    const QJsonObject rotationJSON = data["rotation"].toObject();
    (*m_transform->m_rotation)[0]->setValue(rotationJSON["x"].toDouble());
    (*m_transform->m_rotation)[1]->setValue(rotationJSON["y"].toDouble());
    (*m_transform->m_rotation)[2]->setValue(rotationJSON["z"].toDouble());

    // Speed
    m_movement->m_speed->setValue(static_cast<float>(data["speed"].toDouble()));

    // Sensitivity
    m_movement->m_sensitivity->setValue(static_cast<float>(data["sensitivity"].toDouble()));

    // Easing
    m_movement->m_easing->setValue(data["easing"].toBool());

    // Acceleration
    m_movement->m_acceleration->setValue(data["acceleration"].toBool());
}

void NauSceneCameraSettingsWidget::setControlButton(NauToolButton* button)
{
    m_button = button;
}

bool NauSceneCameraSettingsWidget::buttonClicked() const noexcept
{
    if (m_button == nullptr) {
        return false;
    }
    const QMargins buttonMargin = m_button->contentsMargins();
    const QPoint cursorPosition = m_button->mapFromGlobal(QCursor::pos());
    const QPoint buttonPosition = m_button->pos() - QPoint(buttonMargin.left(), buttonMargin.top());
    const auto [width, height] = m_button->size() + QSize(buttonMargin.right(), buttonMargin.bottom());
    return (cursorPosition.x() > buttonPosition.x()) && (cursorPosition.y() > buttonPosition.y()) &&
           (cursorPosition.x() < (buttonPosition.x() + width)) && (cursorPosition.y() < (buttonPosition.y() + height));
}

void NauSceneCameraSettingsWidget::closeEvent(QCloseEvent* event)
{
    if (buttonClicked()) {
        event->ignore();
        return;
    }
    setFocus();
    NauWidget::closeEvent(event);
    emit close();
}

void NauSceneCameraSettingsWidget::mouseReleaseEvent(QMouseEvent* event)
{
    NauWidget::mouseReleaseEvent(event);
    if (buttonClicked()) {
        m_button->click();
    }
}


// ** NauSceneCameraHeaderWidget

NauSceneCameraHeaderWidget::NauSceneCameraHeaderWidget(NauWidget* parent)
    : m_layout(new NauLayoutHorizontal(this))
    , m_headerLabel(new NauLabel(tr("Viewport Camera Settings"), this))
    , m_helpButton(new NauToolButton(this))
{
    setContentsMargins(16, 16, 16, 16);
    QFont font = Nau::Theme::current().fontObjectInspectorSpoiler();
    m_headerLabel->setFont(font);

    m_helpButton->setIcon(Nau::Theme::current().iconQuestionMark());
    m_helpButton->setIconSize(QSize(16, 16));
    m_helpButton->setDisabled(true);

    m_layout->addWidget(m_headerLabel, Qt::AlignLeft);
    m_layout->addWidget(m_helpButton, Qt::AlignRight);
}
