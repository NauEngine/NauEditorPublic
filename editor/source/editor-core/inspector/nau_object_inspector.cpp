// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "inspector/nau_object_inspector.hpp"
#include "nau/math/nau_matrix_math.hpp"
#include "themes/nau_theme.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"
#include "nau_static_text_label.hpp"
#include "nau_buttons.hpp"

#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QQuaternion>
#include <QSignalBlocker>

#include <algorithm>
#include <format>


#pragma region RESOURCE VIEW LEVEL

// ** NauResourceComboBox

NauInspectorResourceComboBox::NauInspectorResourceComboBox(NauStringViewAttributeType type, NauWidget* parent)
    : NauResourceComboBox(parent)
    , m_type(type)
{
    setIcon(Nau::Theme::current().iconResourcePlaceholder());
}

void NauInspectorResourceComboBox::showPopup()
{
    QSignalBlocker blocker(this);
    updateItems();
    NauAbstractComboBox::showPopup();
}

void NauInspectorResourceComboBox::updateItems()
{
    clear();

    // TODO: There is outdated code in this file.
    // It should be cleaned up in the future.
    //for (auto item : NauResourceSelector::resourceList(m_type)) {
    //    addItem(item);
    //}
}
#pragma endregion


#pragma region INSPECTOR PANEL LEVEL


// ** NauEntityInspectorPageHeader

NauEntityInspectorPageHeader::NauEntityInspectorPageHeader(const std::string& title, const std::string& subtitle)
    : m_title(new NauStaticTextLabel(title.c_str(), this))
{
    setFixedHeight(Height);

    auto layout = new NauLayoutVertical(this);
    auto layoutMain = new NauLayoutHorizontal();
    layoutMain->setContentsMargins(QMargins(OuterMargin, OuterMargin, OuterMargin, OuterMargin));
    layout->addLayout(layoutMain);

    // Image
    // TODO: need some asset icon generation system in future.
    // Potentially, as a part of a theme.
    auto label = new QLabel(this);
    label->setPixmap(QPixmap(":/Inspector/icons/inspector/header.png").scaledToWidth(48, Qt::SmoothTransformation));
    layoutMain->addWidget(label);

    // Text
    auto layoutTitle = new NauLayoutVertical;
    layoutTitle->setContentsMargins(QMargins(HorizontalSpacer, 0, 0, 0));
    layoutMain->addLayout(layoutTitle);
    layoutMain->addStretch(1);
    
    // Title
    layoutTitle->addStretch(1);
    m_title->setFont(Nau::Theme::current().fontInspectorHeaderTitle());
    layoutTitle->addWidget(m_title);
    
    // Subtitle
    auto labelSubtitle = new NauStaticTextLabel(subtitle.c_str(), this);
    labelSubtitle->setFont(Nau::Theme::current().fontInspectorHeaderSubtitle());
    labelSubtitle->setColor(NauColor(255, 255, 255, 128));
    layoutTitle->addWidget(labelSubtitle);
    layoutTitle->addStretch(1);

    // Bottom separator
    auto separator = new QFrame;
    separator->setStyleSheet(QString("background-color: #141414;"));
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    layout->addWidget(separator);

    // Add Button
    auto addButton = new NauPrimaryButton();
    addButton->setText(tr("Add"));
    addButton->setIcon(Nau::Theme::current().iconAddPrimaryStyle());
    addButton->setFixedHeight(NauAbstractButton::standardHeight());
    layoutMain->addWidget(addButton);
}

void NauEntityInspectorPageHeader::changeTitle(const std::string& title)
{
    m_title->setText(title.c_str());
}


// ** NauEntityInspectorPage

NauEntityInspectorPage::NauEntityInspectorPage(NauWidget* parent)
    : NauWidget(parent)
    , m_layout(new NauLayoutVertical(this))
    , m_currentObject(nullptr)
{
    connect(&m_updateUiTimer, &QTimer::timeout, this, &NauEntityInspectorPage::tick);
}

// TODO: Requires refactoring of function parameters when API is changed
void NauEntityInspectorPage::loadEntitiesProperties(std::shared_ptr<std::vector<NauObjectPtr>> objects, const NauViewAttributes& viewAttributes)
{
    NED_ASSERT(!objects.get()->empty());

    // As long as there is no multi-select, we only display one object in the inspector.
    if (objects.get()->size() > 1) {
        clear();
        auto errorLabel = new NauLabel(tr("Multi selection in the game object inspector is temporarily unavailable!"));
        // Text font does not match the design. Correct during the redesign of the inspector.
        errorLabel->setFont(Nau::Theme::current().fontObjectInspector());
        errorLabel->setContentsMargins(ErrorLabelOuterMargin, ErrorLabelOuterMargin, ErrorLabelOuterMargin, ErrorLabelOuterMargin);
        m_layout->addWidget(errorLabel);
        m_layout->addStretch(1);
    } else {
        // TODO: Needs to be refactored when the API changes

        // Because there is no multi-select support in the inspector
        // Select the first element from the array
        NauObjectPtr preparedObject = (*objects.get())[0];

        // TODO: When the need to save Entities is no longer necessary, this should be removed
        if (auto preparedEntity = std::static_pointer_cast<NauEntity>(preparedObject); preparedObject) {
            // TODO: viewAttributes should not only work with Entities
            const NauAttributesMap viewAttribute = viewAttributes[preparedEntity->templateName.c_str()];
            loadEntityProperties(preparedEntity, viewAttribute);
        } else {
            loadEntityProperties(preparedObject, NauAttributesMap());
        }
    }
}

void NauEntityInspectorPage::loadEntityProperties(NauObjectPtr object, const NauAttributesMap& viewAttributes)
{
    clear();
    m_currentObject = object;
    
    // Setup the header
    // TODO: need either an ability to differentiate between objects or better object naming system
    std::string objectKind;
    std::string objectName;
    if (object->displayName() == "Material editor") {
        objectKind = tr("Material").toUtf8().data();
        if (object->getPropertyValue("name").canConvert<QString>()) {
            objectName = object->getPropertyValue("name").convert<QString>().toUtf8().data();
        }
    } else {
        objectKind = tr("Entity").toUtf8().data();
        objectName = object->displayName();
    };

    auto header = new NauEntityInspectorPageHeader(objectName, objectKind);
    m_layout->addWidget(header);

    // Name updates
    connect(object.get(), &NauObject::eventDisplayNameChanged, header, &NauEntityInspectorPageHeader::changeTitle);
    connect(object.get(), &NauObject::eventComponentChanged, [this, header](const std::string& componentName, const NauAnyType& value) {
        if (!componentName.compare("name")) {
            header->changeTitle(value.convert<QString>().toUtf8().data());
        }
    });

    for (auto property = object->properties().begin(); property != object->properties().end(); ++property) {
        const QString propertyName = property.key();

        const NauAnyType& propertyValue = property.value().value();

        QString propertyViewAttribute;
        if (viewAttributes.contains(propertyName)) {
            propertyViewAttribute = viewAttributes[propertyName];
        }

        NauPropertyAbstract* propertyWidget = nullptr;

        // Temporary solution to support visual attributes
        // TODO: We need to rework the way visual attributes are stored and figure out a way to pass it to the property constructor
        if (propertyValue.canConvert<QString>()) {
            if (!propertyViewAttribute.isEmpty()) {

                if (propertyViewAttribute == "ResourceSelector:Material") {
                    propertyWidget = NauPropertyFactory::createProperty(propertyValue, NauStringViewAttributeType::MATERIAL, "Material");
                }

                if (propertyViewAttribute == "ResourceSelector:Model") {
                    propertyWidget = NauPropertyFactory::createProperty(propertyValue, NauStringViewAttributeType::MODEL, "Model");
                }

                if (propertyViewAttribute == "ResourceSelector:Texture") {
                    propertyWidget = NauPropertyFactory::createProperty(propertyValue, NauStringViewAttributeType::TEXTURE, "Texture");
                }
                
            } else {
                propertyWidget = NauPropertyFactory::createProperty(propertyValue, NauStringViewAttributeType::DEFAULT, propertyName.toUtf8().constData());
            }

        } else if (propertyViewAttribute == "isEnabled=false") {
            propertyWidget = NauPropertyFactory::createProperty(propertyValue, "Disabled");
            propertyWidget->setWidgetEnabled(false);

        } else {
            propertyWidget = NauPropertyFactory::createProperty(propertyValue, propertyName.toUtf8().constData());
        }

        if (propertyWidget != nullptr) {
            propertyWidget->setValue(propertyValue);

            m_propertyMap[propertyName] = propertyWidget;

            connect(propertyWidget, &NauPropertyAbstract::eventValueChanged, this, [this, propertyName, propertyWidget]() {
                // TODO: When the need to save Entities is no longer necessary, this should be removed 
                if (auto currentEntity = std::dynamic_pointer_cast<NauEntity>(m_currentObject); currentEntity) {
                    emit eventValueChanged(currentEntity->guid, propertyName.toUtf8().constData(), propertyWidget->getValue());
                } else {
                    NED_WARNING("Currently the undo/redo system only supports working with scene objects.");
                    m_currentObject->setPropertyValue(propertyName.toUtf8().constData(), propertyWidget->getValue());
                }
            });
        }

        if (auto spoiler = findOrAddComponent(property.value().componentName().c_str()); spoiler)
        {
            // TODO: Correct the indents to those in the layout while working on the redesign of the property
            spoiler->addWidget(propertyWidget);
            m_layout->addWidget(spoiler);
        } else {
            m_layout->addWidget(propertyWidget);
        }
    }

    m_layout->addStretch(1);

    // For each propertyName, consider how many times the widget should expand only once
    for (auto& component : m_components) {
        component.second->setExpanded();
    }

    m_updateUiTimer.start(16);
}

NauInspectorComponentSpoiler* NauEntityInspectorPage::findOrAddComponent(const std::string& componentName)
{
    if (componentName.empty()) {
        return nullptr;
    }

    if (m_components.contains(componentName)) {
        return m_components[componentName];
    } 

    m_components[componentName] = new NauInspectorComponentSpoiler(tr(componentName.c_str()), 0, this);
    return m_components[componentName];
}

void NauEntityInspectorPage::clear()
{
    m_updateUiTimer.stop();
    m_currentObject.reset();
    m_layout->clear();
    m_propertyMap.clear();
    m_components.clear();
}

void NauEntityInspectorPage::tick()
{
    // Now, when you change the scene in the editor engine, a copy of NauEntity comes with the modified flag
    // TODO: Update the inspector by setting a local flag, and remove the modified flag in nauentity
    if (m_currentObject && m_currentObject->isModified()) {
        updateProperties();
        m_currentObject->handleModified();
    }
}

void NauEntityInspectorPage::updateProperties()
{
    QSignalBlocker blocker(this);
    for (auto property = m_currentObject->properties().begin(); property != m_currentObject->properties().end(); ++property) {
        auto& propertyValue = property.value().value();

        // Usually, a situation where a widget has not been created on a property should not exist
        if (m_propertyMap.contains(property.key()) && m_propertyMap[property.key()] != nullptr) {
            m_propertyMap[property.key()]->setValue(propertyValue);
        } else {
            // But if for some reason this happened, then this is a serious error
            NED_ERROR("The corresponding handler widget has not been created for property: {}!", property.key().toUtf8().constData());
            return;
        }
    }
}


// ** NauInspectorComponentSpoiler

NauInspectorComponentSpoiler::NauInspectorComponentSpoiler(const QString& title, const int animationDuration, NauWidget* parent)
    : NauSpoiler(title, animationDuration, parent)
{
}
#pragma endregion


#pragma region ABSTRACT LEVEL

// ** NauPropertyAbstract

NauPropertyAbstract::NauPropertyAbstract(NauWidget* parent)
    : NauWidget(parent)
{
}
#pragma endregion


#pragma region BASE LEVEL

// ** NauSingleRowPropertyBase

NauSingleRowPropertyBase::NauSingleRowPropertyBase(const std::string& propertyTitle, NauWidget* parent)
    : NauPropertyAbstract(parent)
    , m_contentLayout(new NauLayoutGrid())
    , m_layout(new NauLayoutVertical(this))
    , m_label(new NauLabel(propertyTitle.c_str(), this))
{
    setFixedHeight(Height);

    // Thus we set a three-column grid, with 16 pixel indents between the columns:
    // |Content filling||16px||Content filling||16px||Content filling|
    m_contentLayout->setColumnStretch(0, 1);
    m_contentLayout->setColumnStretch(1, 0);
    m_contentLayout->setColumnStretch(2, 1);
    m_contentLayout->setColumnStretch(3, 0);
    m_contentLayout->setColumnStretch(4, 1);

    m_contentLayout->setColumnMinimumWidth(1, 16);
    m_contentLayout->setColumnMinimumWidth(3, 16);

    m_label->setFont(Nau::Theme::current().fontObjectInspector());
    m_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_label->setFixedSize(m_label->width(), 16);

    // The property name always occupies 1/3 of the width of the inspector
    m_contentLayout->addWidget(m_label,0, 0);
    m_layout->addLayout(m_contentLayout);
}

void NauSingleRowPropertyBase::setWidgetEnabled(bool isEnabled)
{
    // TODO: We need to figure out why the parent function doesn't work
    //NauPropertyAbstract::setEnabled(isEnabled);
    m_contentLayout->setEnabled(isEnabled);
}

// ** NauSingleTogglePropertyBase

NauSingleTogglePropertyBase::NauSingleTogglePropertyBase(const std::string& propertyTitle, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_checkBox(new NauCheckBox(this))
{
    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_checkBox, 0, 2, 0, 4);
}


// ** NauMultiValueSpinBoxPropertyBase

NauMultiValueSpinBoxPropertyBase::NauMultiValueSpinBoxPropertyBase(const std::string& propertyTitle, QStringList valuesNames, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_multiLineEdit(std::make_unique<NauMultiValueDoubleSpinBox>(parent, valuesNames))
{
    setFixedHeight(Height);
    
    connect(m_multiLineEdit.get(), &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauMultiValueSpinBoxPropertyBase::eventValueChanged);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_multiLineEdit.get(), 0, 2, 0, 4);
}


// ** NauMultiRowPropertyBase

NauMultiRowPropertyBase::NauMultiRowPropertyBase(NauWidget* parent)
    : NauPropertyAbstract(parent)
    , m_layout(new NauLayoutVertical(this))
{
}

void NauMultiRowPropertyBase::setWidgetEnabled(bool isEnabled)
{
    // TODO: We need to figure out why the parent function doesn't work
    // NauPropertyAbstract::setEnabled(isEnabled);
    m_layout->setEnabled(isEnabled);
}

#pragma endregion


#pragma region PROPERTY LEVEL

// ** NauPropertyBool

NauPropertyBool::NauPropertyBool(const std::string& propertyTitle, NauWidget* parent)
    : NauSingleTogglePropertyBase(propertyTitle, parent)
{
    connect(m_checkBox, &QCheckBox::stateChanged, this, &NauPropertyBool::eventValueChanged);
}

void NauPropertyBool::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<bool>());
    m_checkBox->setChecked(value.convert<bool>());
}

NauAnyType NauPropertyBool::getValue()
{
    return NauAnyType(m_checkBox->isChecked());
}


// ** NauPropertyString

NauPropertyString::NauPropertyString(NauStringViewAttributeType type, const std::string& propertyTitle, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_view(nullptr)
{
    m_view = NauStringViewsFactory::createStringView(type);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_view, 0, 2, 0, 4);

    connect(m_view, &NauStringAbstractView::eventValueChanged, this, &NauPropertyString::eventValueChanged);
}

void NauPropertyString::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<QString>());
    m_view->setValue(value.convert<QString>());
}

NauAnyType NauPropertyString::getValue()
{
    return NauAnyType(m_view->getValue());
}


// ** NauPropertyColor

NauPropertyColor::NauPropertyColor(const std::string& propertyTitle, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_colorButton(new NauPushButton(this))
    , m_colorDialog(new NauColorDialog(this))
{
    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_colorButton, 0, 2, 0, 4);

    connect(m_colorButton, &NauPushButton::pressed, m_colorDialog, &NauColorDialog::colorDialogRequested);
    connect(m_colorDialog, &NauColorDialog::eventColorChanged, this, &NauPropertyString::eventValueChanged);
}

NauAnyType NauPropertyColor::getValue()
{
    return NauAnyType(m_colorDialog->color());
}

void NauPropertyColor::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<QColor>());

    QColor color = value.convert<QColor>();
    if (color.isValid()) {
        // TODO: Hack to change the color.
        // The use of the palette is prevented by the qss of the parent widget.
        // Fix in the future.
        m_colorButton->setStyleSheet(std::format("background-color: {}", color.name().toUtf8().constData()).c_str());
        m_colorDialog->setColor(color);
    }
}


// ** NauPropertyInt

NauPropertyInt::NauPropertyInt(const std::string& propertyTitle, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_spinBox(new NauSpinBox(this))
{
    m_spinBox->setMinimum(std::numeric_limits<int>::min());
    m_spinBox->setMaximum(std::numeric_limits<int>::max());
    m_spinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_spinBox, 0, 2, 0, 4);

    connect(m_spinBox, &NauSpinBox::valueChanged, this, &NauPropertyInt::eventValueChanged);
}

void NauPropertyInt::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<int>());
    m_spinBox->setValue(value.convert<int>());
}

NauAnyType NauPropertyInt::getValue()
{
    return NauAnyType(m_spinBox->value());
}

void NauPropertyInt::setRange(int minimum, int maximum)
{
    m_spinBox->setMinimum(minimum);
    m_spinBox->setMaximum(maximum);
}


// ** NauPropertyReal

NauPropertyReal::NauPropertyReal(const std::string& propertyTitle, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_spinBox(new NauDoubleSpinBox(this))
{
    m_spinBox->setMinimum(std::numeric_limits<float>::lowest());
    m_spinBox->setMaximum(std::numeric_limits<float>::max());
    m_spinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_spinBox, 0, 2, 0, 4);

    connect(m_spinBox, &NauDoubleSpinBox::valueChanged, this, &NauPropertyReal::eventValueChanged);
}

void NauPropertyReal::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<float>());
    m_spinBox->setValue(value.convert<float>());
}

NauAnyType NauPropertyReal::getValue()
{
    return NauAnyType(float(m_spinBox->value()));
}

void NauPropertyReal::setRange(float minimum, float maximum)
{
    m_spinBox->setMinimum(minimum);
    m_spinBox->setMaximum(maximum);
}

void NauPropertyReal::setSingleStep(float step)
{
    m_spinBox->setSingleStep(step);
}


// ** NauPropertyPoint2

NauPropertyPoint2::NauPropertyPoint2(const std::string& propertyTitle, const QString& xTitle, const QString& yTitle, NauWidget* parent)
    : NauMultiValueSpinBoxPropertyBase(propertyTitle, { xTitle, yTitle }, parent)
    , m_x((*m_multiLineEdit)[0])
    , m_y((*m_multiLineEdit)[1])
{
    m_multiLineEdit->setMinimum(std::numeric_limits<float>::lowest());
    m_multiLineEdit->setMaximum(std::numeric_limits<float>::max());
}

void NauPropertyPoint2::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<QVector2D>());
    QVector2D point = value.convert<QVector2D>();
    m_x->setValue(point.x());
    m_y->setValue(point.y());
}

NauAnyType NauPropertyPoint2::getValue()
{
    return NauAnyType(QVector2D(m_x->value(), m_y->value()));
}


// ** NauPropertyPoint3

NauPropertyPoint3::NauPropertyPoint3(const std::string& propertyTitle, NauWidget* parent)
    : NauMultiValueSpinBoxPropertyBase(propertyTitle, {"X", "Y", "Z"}, parent)
    , m_x((*m_multiLineEdit)[0])
    , m_y((*m_multiLineEdit)[1])
    , m_z((*m_multiLineEdit)[2])
{
    m_multiLineEdit->setMinimum(std::numeric_limits<float>::lowest());
    m_multiLineEdit->setMaximum(std::numeric_limits<float>::max());
}

void NauPropertyPoint3::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<QVector3D>());
    QVector3D point = value.convert<QVector3D>();
    m_x->setValue(point.x());
    m_y->setValue(point.y());
    m_z->setValue(point.z());
}

NauAnyType NauPropertyPoint3::getValue()
{
    return NauAnyType(QVector3D(m_x->value(), m_y->value(), m_z->value()));
}


// ** NauPropertyPoint4

NauPropertyPoint4::NauPropertyPoint4(const std::string& propertyTitle, NauWidget* parent)
    : NauMultiValueSpinBoxPropertyBase(propertyTitle, {"X", "Y", "Z", "W"}, parent)
    , m_x((*m_multiLineEdit)[0])
    , m_y((*m_multiLineEdit)[1])
    , m_z((*m_multiLineEdit)[2])
    , m_w((*m_multiLineEdit)[3])
{
    m_multiLineEdit->setMinimum(std::numeric_limits<float>::lowest());
    m_multiLineEdit->setMaximum(std::numeric_limits<float>::max());
}

void NauPropertyPoint4::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<QVector4D>());
    QVector4D point = value.convert<QVector4D>();
    m_x->setValue(point.x());
    m_y->setValue(point.y());
    m_z->setValue(point.z());
    m_w->setValue(point.w());
}

NauAnyType NauPropertyPoint4::getValue()
{
    return NauAnyType(QVector4D(m_x->value(), m_y->value(), m_z->value(), m_w->value()));
}


// ** NauPropertyMatrix

// TODO: propertyTitle is now not used in any way and has been added for uniform work with properties
// Fix it later
NauPropertyMatrix::NauPropertyMatrix(const std::string& propertyTitle, NauWidget* parent)
    : NauMultiRowPropertyBase(parent)
{
    m_position = std::make_unique<NauMultiValueDoubleSpinBox>(this, 3);
    m_rotation = std::make_unique<NauMultiValueDoubleSpinBox>(this, 3);
    m_scale = std::make_unique<NauMultiValueDoubleSpinBox>(this, 3);

    // The current limit is set by the size of the data type that is used
    m_position->setMinimum(NAU_POSITION_MIN_LIMITATION);
    m_position->setMaximum(NAU_POSITION_MAX_LIMITATION);

    m_rotation->setMinimum(std::numeric_limits<float>::lowest());
    m_rotation->setMaximum(std::numeric_limits<float>::max());

    // In the engine there is an artificial limitation associated with the scale.
    // If, after scaling, some of the parts of the mesh go beyond the boundaries of 1e10f,
    // then a stop will work.What this is connected with is not clear.

    // TODO: return negative values
    m_scale->setMinimum(NAU_SCALE_MIN_LIMITATION);
    m_scale->setMaximum(NAU_SCALE_MAX_LIMITATION);

    // This is a temporary solution to keep the real values accurate enough when working with the editor and to avoid nulling the transform matrix
    // TODO: Fix the matrix nulling in another more correct way.
    // Most likely on the engine side
    constexpr int transformDecimalPrecision = 2;
    m_position->setDecimals(transformDecimalPrecision);
    m_rotation->setDecimals(transformDecimalPrecision);
    m_scale->setDecimals(transformDecimalPrecision);

    connect(m_position.get(), &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauMultiValueSpinBoxPropertyBase::eventValueChanged);
    connect(m_rotation.get(), &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauMultiValueSpinBoxPropertyBase::eventValueChanged);
    connect(m_scale.get(), &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauMultiValueSpinBoxPropertyBase::eventValueChanged);

    auto labelPosition = new NauStaticTextLabel(tr("Position"));
    labelPosition->setFont(Nau::Theme::current().fontObjectInspector());
    labelPosition->setColor(NauColor(128, 128, 128));

    m_layout->addWidget(labelPosition, Qt::AlignLeft);
    m_layout->addItem(new QSpacerItem(0, OuterMargin));
    m_layout->addWidget(m_position.get(), Qt::AlignLeft);
    m_layout->addItem(new QSpacerItem(0, VerticalSpacer));

    auto labelRotation = new NauStaticTextLabel(tr("Rotation"));
    labelRotation->setFont(Nau::Theme::current().fontObjectInspector());;
    labelRotation->setColor(NauColor(128, 128, 128));

    m_layout->addWidget(labelRotation, Qt::AlignLeft);
    m_layout->addItem(new QSpacerItem(0, OuterMargin));
    m_layout->addWidget(m_rotation.get(), Qt::AlignLeft);
    m_layout->addItem(new QSpacerItem(0, VerticalSpacer));

    auto labelScale = new NauStaticTextLabel(tr("Scale"));
    labelScale->setFont(Nau::Theme::current().fontObjectInspector());
    labelScale->setColor(NauColor(128, 128, 128));

    m_layout->addWidget(labelScale, Qt::AlignLeft);
    m_layout->addItem(new QSpacerItem(0, OuterMargin));
    m_layout->addWidget(m_scale.get(), Qt::AlignLeft);
}

void NauPropertyMatrix::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<QMatrix4x3>());
    const QMatrix4x3& matrix = value.convert<QMatrix4x3>();

    const QMatrix3x3 trsComposition = NauMathMatrixUtils::MatrixToTRSComposition(matrix);

    for (int i = 0; i < 3; ++i) {
        (*m_position)[i]->setValue(trsComposition.data()[i]);
        (*m_rotation)[i]->setValue(trsComposition.data()[i + 3]);
        (*m_scale)[i]->setValue(trsComposition.data()[i + 6]);
    }
}

NauAnyType NauPropertyMatrix::getValue()
{
    // TODO: Work with mathematics will be translated into functions and types from Qt
    QMatrix3x3 trsComposition;

    for (int i = 0; i < 3; ++i) {
        trsComposition.data()[i] = ((*m_position)[i]->value());
        trsComposition.data()[i + 3] = ((*m_rotation)[i]->value());
        trsComposition.data()[i + 6] = ((*m_scale)[i]->value());
    }

    QMatrix4x3 matrix = NauMathMatrixUtils::TRSCompositionToMatrix(trsComposition);

    return NauAnyType(matrix);
}


// ** NauPropertyRangedInt

NauPropertyRangedInt::NauPropertyRangedInt(const std::string& propertyTitle, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_slider(new NauSliderIntValue(this))
{
    m_slider->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_slider.get(), 0, 2, 0, 4);

    connect(m_slider.get(), &NauAbstractSlider::eventRangeChanged, this, &NauPropertyRangedInt::eventValueChanged);
}


NauAnyType NauPropertyRangedInt::getValue()
{
    return m_slider->rangedValue();
}


void NauPropertyRangedInt::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<NauRangedValue<int>>());
    m_slider->setRangedValue(value.convert<NauRangedValue<int>>());
}


// ** NauPropertyRangedFloat

NauPropertyRangedFloat::NauPropertyRangedFloat(const std::string& propertyTitle, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_slider(new NauSliderFloatValue(this))
{
    m_slider->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_slider.get(), 0, 2, 0, 4);

    connect(m_slider.get(), &NauAbstractSlider::eventRangeChanged, this, &NauPropertyRangedFloat::eventValueChanged);
}


NauAnyType NauPropertyRangedFloat::getValue()
{
    return m_slider->rangedValue();
}


void NauPropertyRangedFloat::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<NauRangedValue<float>>());
    m_slider->setRangedValue(value.convert<NauRangedValue<float>>());
}


// ** NauPropertyRangedInt2

NauPropertyRangedInt2::NauPropertyRangedInt2(const std::string& propertyTitle, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_slider(new NauSliderIntPair(this))
{
    m_slider->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_slider.get(), 0, 2, 0, 4);

    connect(m_slider.get(), &NauSliderIntPair::eventRangeChanged, this, &NauPropertyRangedInt2::eventValueChanged);
}


NauAnyType NauPropertyRangedInt2::getValue()
{
    return m_slider->rangedPair();
}


void NauPropertyRangedInt2::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<NauRangedPair<int>>());
    m_slider->setRangedPair(value.convert<NauRangedPair<int>>());
}


// ** NauPropertyRangedFloat2

NauPropertyRangedFloat2::NauPropertyRangedFloat2(const std::string& propertyTitle, NauWidget* parent)
    : NauSingleRowPropertyBase(propertyTitle, parent)
    , m_slider(new NauSliderFloatPair(this))
{
    m_slider->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_slider.get(), 0, 2, 0, 4);

    connect(m_slider.get(), &NauSliderFloatPair::eventRangeChanged, this, &NauPropertyRangedFloat2::eventValueChanged);
}


NauAnyType NauPropertyRangedFloat2::getValue()
{
    return m_slider->rangedPair();
}


void NauPropertyRangedFloat2::setValueInternal(const NauAnyType& value)
{
    NED_ASSERT(value.canConvert<NauRangedPair<float>>());
    m_slider->setRangedPair(value.convert<NauRangedPair<float>>());
}
#pragma endregion


#pragma region VIEWS LEVEL

// ** NauStringAbstractView

NauStringAbstractView::NauStringAbstractView(NauWidget* parent)
    : NauWidget(parent)
{
}


// ** NauStringLineView

NauStringLineView::NauStringLineView(NauWidget* parent)
    : NauStringAbstractView(parent)
    , m_lineEdit(new NauLineEdit())
{
    // TODO: Hack to keep the container from growing uncontrollably.
    // Fix in the future.
    m_lineEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_lineEdit->setFixedHeight(32);

    auto layout = new NauLayoutStacked(this);
    layout->addWidget(m_lineEdit);

    connect(m_lineEdit, &NauLineEdit::editingFinished, this, &NauStringLineView::eventValueChanged);
}

QString NauStringLineView::getValue() const
{
    return m_lineEdit->text();
}

void NauStringLineView::setValue(const QString& value)
{
    m_lineEdit->setText(value);
}


// ** NauStringResourceSelectorView

NauStringResourceSelectorView::NauStringResourceSelectorView(const NauStringViewAttributeType type, NauWidget* parent)
    : NauStringAbstractView(parent)
    , m_comboBox(new NauInspectorResourceComboBox(type, this))
    , m_type(type)
{
    m_comboBox->setFixedHeight(32);
    m_comboBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // TODO: There is outdated code in this file.
    // It should be cleaned up in the future.
    //for (auto item : NauResourceSelector::resourceList(type)) {
    //    m_comboBox->addItem(item);
    //}

    auto layout = new NauLayoutVertical(this);
    layout->addWidget(m_comboBox);

    connect(m_comboBox, QOverload<const QString&>::of(&NauInspectorResourceComboBox::eventSelectionChanged), this, &NauStringResourceSelectorView::eventValueChanged);
}

QString NauStringResourceSelectorView::getValue() const
{
    return m_comboBox->text();
}

void NauStringResourceSelectorView::setValue(const QString& value)
{
    m_comboBox->setText(value);
}

void NauStringResourceSelectorView::clearItems()
{
    m_comboBox->clear();
}

void NauStringResourceSelectorView::updateItems()
{
    m_comboBox->updateItems();
}

NauAbstractComboBox* NauStringResourceSelectorView::getComboBox() const noexcept
{
    return m_comboBox;
}

// ** NauStringAbstractView

NauStringAbstractView* NauStringViewsFactory::createStringView(NauStringViewAttributeType type)
{
    switch (type)
    {
    case NauStringViewAttributeType::DEFAULT:
        return new NauStringLineView();
    case NauStringViewAttributeType::MODEL:
        return new NauStringResourceSelectorView(type);
    case NauStringViewAttributeType::MATERIAL:
        return new NauStringResourceSelectorView(type);
    case NauStringViewAttributeType::TEXTURE:
        return new NauStringResourceSelectorView(type);
    default:
        break;
    }

    return nullptr;
}

#pragma endregion
