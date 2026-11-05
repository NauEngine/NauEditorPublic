// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/inspector/nau_usd_inspector_widgets.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"
#include "themes/nau_theme.hpp"
#include "nau/nau_constants.hpp"
#include "nau/app/nau_editor_services.hpp"

#include "baseWidgets/nau_static_text_label.hpp"

#include "nau_assert.hpp"

#include "pxr/base/gf/quaternion.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/base/gf/matrix3d.h"

#include <QFileInfo>
#include <QMatrix3x3>
#include <QGenericMatrix>

#include "nau/usd_meta_tools/usd_meta_manager.h"
#include "usd_proxy/usd_prim_proxy.h"
#include "nau/shared/file_system.h"


#pragma region ABSTRACT LEVEL

// ** NauUsdPropertyAbstract

NauUsdPropertyAbstract::NauUsdPropertyAbstract(NauWidget* parent)
    : NauWidget(parent)
{
}
#pragma endregion


#pragma region BASE LEVEL

// ** NauUsdSingleRowPropertyBase

NauUsdSingleRowPropertyBase::NauUsdSingleRowPropertyBase(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdPropertyAbstract(parent)
    , m_layout(new NauLayoutVertical(this))
    , m_contentLayout(new NauLayoutGrid())
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

    // The property name always occupies 1/3 of the width of the inspector
    m_contentLayout->addWidget(m_label,0, 0);
    m_layout->addLayout(m_contentLayout);
}

void NauUsdSingleRowPropertyBase::setWidgetEnabled(bool isEnabled)
{
    // TODO: We need to figure out why the parent function doesn't work
    //NauPropertyAbstract::setEnabled(isEnabled);
    m_contentLayout->setEnabled(isEnabled);
}


// ** NauUsdSingleTogglePropertyBase

NauUsdSingleTogglePropertyBase::NauUsdSingleTogglePropertyBase(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdSingleRowPropertyBase(propertyTitle, parent)
    , m_checkBox(new NauCheckBox(this))
{
    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_checkBox, 0, 2, 0, 4);
}


// ** NauUsdMultiValueSpinBoxPropertyBase

NauUsdMultiValueSpinBoxPropertyBase::NauUsdMultiValueSpinBoxPropertyBase(const std::string& propertyTitle, QStringList valuesNames, NauWidget* parent)
    : NauUsdSingleRowPropertyBase(propertyTitle, parent)
    , m_multiLineEdit(std::make_unique<NauMultiValueSpinBox>(parent, valuesNames))
{
    setFixedHeight(Height);
    connect(m_multiLineEdit.get(), &NauMultiValueSpinBox::eventValueChanged, this, &NauUsdMultiValueDoubleSpinBoxPropertyBase::eventValueChanged);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_multiLineEdit.get(), 0, 2, 0, 4);
}


// ** NauUsdMultiValueDoubleSpinBoxPropertyBase

NauUsdMultiValueDoubleSpinBoxPropertyBase::NauUsdMultiValueDoubleSpinBoxPropertyBase(const std::string& propertyTitle, QStringList valuesNames, NauWidget* parent)
    : NauUsdSingleRowPropertyBase(propertyTitle, parent)
    , m_multiLineEdit(std::make_unique<NauMultiValueDoubleSpinBox>(parent, valuesNames))
{
    setFixedHeight(Height);
    connect(m_multiLineEdit.get(), &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauUsdMultiValueDoubleSpinBoxPropertyBase::eventValueChanged);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_multiLineEdit.get(), 0, 2, 0, 4);
}


// ** NauUsdMultiRowPropertyBase

NauUsdMultiRowPropertyBase::NauUsdMultiRowPropertyBase(NauWidget* parent)
    : NauUsdPropertyAbstract(parent)
    , m_layout(new NauLayoutVertical(this))
{
}

void NauUsdMultiRowPropertyBase::setWidgetEnabled(bool isEnabled)
{
    // TODO: We need to figure out why the parent function doesn't work
    // NauPropertyAbstract::setEnabled(isEnabled);
    m_layout->setEnabled(isEnabled);
}

#pragma endregion


#pragma region PROPERTY LEVEL

// ** NauUsdPropertyBool

NauUsdPropertyBool::NauUsdPropertyBool(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdSingleTogglePropertyBase(propertyTitle, parent)
{
    connect(m_checkBox, &QCheckBox::stateChanged, this, &NauUsdPropertyBool::eventValueChanged);
}

void NauUsdPropertyBool::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<bool>());
    m_checkBox->setChecked(value.Get<bool>());
}

PXR_NS::VtValue NauUsdPropertyBool::getValue()
{
    return PXR_NS::VtValue(m_checkBox->isChecked());
}


// ** NauUsdPropertyString

NauUsdPropertyString::NauUsdPropertyString(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdSingleRowPropertyBase(propertyTitle, parent)
    , m_lineEdit(new NauLineEdit(this))
{
    // TODO: Hack to keep the container from growing uncontrollably.
    // Fix in the future.
    m_lineEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_lineEdit->setFixedHeight(32);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_lineEdit, 0, 2, 0, 4);

    connect(m_lineEdit, &NauLineEdit::editingFinished, this, &NauUsdPropertyString::eventValueChanged);
}

void NauUsdPropertyString::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<std::string>());
    m_lineEdit->setText(value.Get<std::string>().c_str());
}

PXR_NS::VtValue NauUsdPropertyString::getValue()
{
    return PXR_NS::VtValue(std::string(m_lineEdit->text().toUtf8().constData()));
}


// ** NauUsdPropertyColor

NauUsdPropertyColor::NauUsdPropertyColor(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdSingleRowPropertyBase(propertyTitle, parent)
    , m_colorButton(new NauPushButton(this))
    , m_colorDialog(new NauColorDialog(this))
{
    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_colorButton, 0, 2, 0, 4);

    connect(m_colorButton, &NauPushButton::pressed, m_colorDialog, &NauColorDialog::colorDialogRequested);
    connect(m_colorDialog, &NauColorDialog::eventColorChanged, this, &NauUsdPropertyString::eventValueChanged);
}

PXR_NS::VtValue NauUsdPropertyColor::getValue()
{
    if (m_colorDialog->color().isValid()) {
        // TODO: Hack to change the color.
        // The use of the palette is prevented by the qss of the parent widget.
        // Fix in the future.
        m_colorButton->setStyleSheet(std::format("background-color: {}", m_colorDialog->color().name().toUtf8().constData()).c_str());
    }

    double red = m_colorDialog->color().red() / 255.0f;
    double green = m_colorDialog->color().green() / 255.0f;
    double blue = m_colorDialog->color().blue() / 255.0f;
    double alpha = m_colorDialog->color().alpha() / 255.0f;

    return PXR_NS::VtValue(pxr::GfVec4d(red, green, blue, alpha));
}

void NauUsdPropertyColor::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<pxr::GfVec4d>());
    pxr::GfVec4d usdColor = value.Get<pxr::GfVec4d>();

    int red = usdColor.data()[0] * 255;
    int green = usdColor.data()[1] * 255;
    int blue = usdColor.data()[2] * 255;
    int alpha = usdColor.data()[3] * 255;

    NauColor color = NauColor(red, green, blue, alpha);

    if (color.isValid()) {
        // TODO: Hack to change the color.
        // The use of the palette is prevented by the qss of the parent widget.
        // Fix in the future.
        m_colorButton->setStyleSheet(std::format("background-color: {}", color.name().toUtf8().constData()).c_str());
        m_colorDialog->setColor(color);
    }
}


// ** NauUsdPropertyInt

NauUsdPropertyInt::NauUsdPropertyInt(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdMultiRowPropertyBase(parent) // propertyTitle
    , m_contentLayout(new NauLayoutGrid())
    , m_titleText(propertyTitle)
{
    // Thus we set a three-column grid, with 16 pixel indents between the columns:
    // |Content filling||16px||Content filling||16px||Content filling|
    m_contentLayout->setColumnStretch(0, 1);
    m_contentLayout->setColumnStretch(1, 0);
    m_contentLayout->setColumnStretch(2, 1);
    m_contentLayout->setColumnStretch(3, 0);
    m_contentLayout->setColumnStretch(4, 1);

    m_contentLayout->setColumnMinimumWidth(1, 16);
    m_contentLayout->setColumnMinimumWidth(3, 16);

    m_layout->addLayout(m_contentLayout);
}

void NauUsdPropertyInt::setValueInternal(const PXR_NS::VtValue& value)
{
    m_contentLayout->clear();

    if (!value.IsArrayValued()) {
        NED_ASSERT(value.IsHolding<int>());
        createRow(m_titleText, value.UncheckedGet<int>(), 0);      

    } else {

        pxr::VtArray<int> intArray = value.Get<pxr::VtArray<int>>();
        NED_ASSERT(value.IsHolding<pxr::VtArray<int>>());

        // TODO: Array support is temporarily disabled
        // Optimize array visualization in the future
        //auto arraySize = value.GetArraySize();
        //auto rowOffet = 0;
        //if (value.GetArraySize() > 100) {
        //    // TODO: Replace label with a spoiler in the future
        //    m_contentLayout->addWidget(new NauLabel(m_titleText.c_str(), this), 0, 0);
        //    m_contentLayout->addWidget(new NauLabel(tr("The size of the array is too large"), this), 1, 0);
        //    m_contentLayout->addWidget(new NauLabel(tr("The first 100 elements will be shown."), this), 2, 0);

        //    rowOffet += 3;
        //    arraySize = 100;
        //} else {
        //    // TODO: Replace label with a spoiler in the future
        //    m_contentLayout->addWidget(new NauLabel(m_titleText.c_str(), this), 0, 0);
        //    rowOffet += 1;
        //}

        //for (int i = 0; i < arraySize; ++i) {
        //    createRow(std::to_string(i).c_str(), intArray[i], i + rowOffet);
        //}
    }
}

void NauUsdPropertyInt::createRow(const std::string& titleText, int value, int row)
{
    auto labelText = new NauLabel(titleText.c_str(), this);
    labelText->setFont(Nau::Theme::current().fontObjectInspector());
    labelText->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // The property name always occupies 1/3 of the width of the inspector
    m_contentLayout->addWidget(labelText, row, 0);

    auto SpinBox = new NauSpinBox(this);
    SpinBox->setValue(value);
    SpinBox->setMinimum(m_minValue);
    SpinBox->setMaximum(m_maxValue);
    SpinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(SpinBox, row, 2);

    connect(SpinBox, &NauSpinBox::valueChanged, this, &NauUsdPropertyInt::eventValueChanged);

    m_label.push_back(labelText);
    m_spinBox.push_back(SpinBox);
}

PXR_NS::VtValue NauUsdPropertyInt::getValue()
{

    if (m_spinBox.size() == 1){
        return PXR_NS::VtValue(m_spinBox[0]->value());

    } else {
        pxr::VtArray<int> intArray;

        for (auto spinBox : m_spinBox) {
            intArray.push_back(spinBox->value());
        }

        return PXR_NS::VtValue(intArray);
    }
}

void NauUsdPropertyInt::setRange(int minimum, int maximum)
{
    m_minValue = minimum;
    m_maxValue = maximum;
}


// ** NauUsdPropertyFloat

NauUsdPropertyFloat::NauUsdPropertyFloat(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdSingleRowPropertyBase(propertyTitle, parent)
    , m_spinBox(new NauDoubleSpinBox(this))
{
    m_spinBox->setMinimum(std::numeric_limits<float>::lowest());
    m_spinBox->setMaximum(std::numeric_limits<float>::max());
    m_spinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_spinBox, 0, 2, 0, 4);

    connect(m_spinBox, &NauDoubleSpinBox::valueChanged, this, &NauUsdPropertyFloat::eventValueChanged);
}

void NauUsdPropertyFloat::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<float>());
    m_spinBox->setValue(value.Get<float>());
}

PXR_NS::VtValue NauUsdPropertyFloat::getValue()
{
    return PXR_NS::VtValue(float(m_spinBox->value()));
}

void NauUsdPropertyFloat::setRange(float minimum, float maximum)
{
    m_spinBox->setMinimum(minimum);
    m_spinBox->setMaximum(maximum);
}

void NauUsdPropertyFloat::setSingleStep(float step)
{
    m_spinBox->setSingleStep(step);
}


// ** NauUsdPropertyDouble

NauUsdPropertyDouble::NauUsdPropertyDouble(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdSingleRowPropertyBase(propertyTitle, parent)
    , m_spinBox(new NauDoubleSpinBox(this))
{
    m_spinBox->setMinimum(std::numeric_limits<double>::lowest());
    m_spinBox->setMaximum(std::numeric_limits<double>::max());
    m_spinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_spinBox, 0, 2, 0, 4);

    connect(m_spinBox, &NauDoubleSpinBox::valueChanged, this, &NauUsdPropertyFloat::eventValueChanged);
}

void NauUsdPropertyDouble::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<double>());
    m_spinBox->setValue(value.Get<double>());
}

PXR_NS::VtValue NauUsdPropertyDouble::getValue()
{
    return PXR_NS::VtValue(double(m_spinBox->value()));
}

void NauUsdPropertyDouble::setRange(double minimum, double maximum)
{
    m_spinBox->setMinimum(minimum);
    m_spinBox->setMaximum(maximum);
}

void NauUsdPropertyDouble::setSingleStep(double step)
{
    m_spinBox->setSingleStep(step);
}


// ** NauUsdPropertyInt2

NauUsdPropertyInt2::NauUsdPropertyInt2(const std::string& propertyTitle, const QString& xTitle, const QString& yTitle, NauWidget* parent)
    : NauUsdMultiValueSpinBoxPropertyBase(propertyTitle, { xTitle, yTitle }, parent)
    , m_x((*m_multiLineEdit)[0])
    , m_y((*m_multiLineEdit)[1])
{
    m_multiLineEdit->setMinimum(std::numeric_limits<int>::lowest());
    m_multiLineEdit->setMaximum(std::numeric_limits<int>::max());
}

void NauUsdPropertyInt2::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<pxr::GfVec2i>());
    pxr::GfVec2i point = value.Get<pxr::GfVec2i>();
    m_x->setValue(point.data()[0]);
    m_y->setValue(point.data()[1]);
}

PXR_NS::VtValue NauUsdPropertyInt2::getValue()
{
    return PXR_NS::VtValue(pxr::GfVec2i(m_x->value(), m_y->value()));
}


// ** NauUsdPropertyDouble2

NauUsdPropertyDouble2::NauUsdPropertyDouble2(const std::string& propertyTitle, const QString& xTitle, const QString& yTitle, NauWidget* parent)
    : NauUsdMultiValueDoubleSpinBoxPropertyBase(propertyTitle, { xTitle, yTitle }, parent)
    , m_x((*m_multiLineEdit)[0])
    , m_y((*m_multiLineEdit)[1])
{
    m_multiLineEdit->setMinimum(std::numeric_limits<float>::lowest());
    m_multiLineEdit->setMaximum(std::numeric_limits<float>::max());
}

void NauUsdPropertyDouble2::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<pxr::GfVec2d>());
    pxr::GfVec2d point = value.Get<pxr::GfVec2d>();
    m_x->setValue(point.data()[0]);
    m_y->setValue(point.data()[1]);
}

PXR_NS::VtValue NauUsdPropertyDouble2::getValue()
{
    return PXR_NS::VtValue(pxr::GfVec2d(m_x->value(), m_y->value()));
}


// ** NauUsdPropertyInt3

NauUsdPropertyInt3::NauUsdPropertyInt3(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdMultiValueSpinBoxPropertyBase(propertyTitle, { "X", "Y", "Z" }, parent)
    , m_x((*m_multiLineEdit)[0])
    , m_y((*m_multiLineEdit)[1])
    , m_z((*m_multiLineEdit)[2])
{
    m_multiLineEdit->setMinimum(std::numeric_limits<int>::lowest());
    m_multiLineEdit->setMaximum(std::numeric_limits<int>::max());
}

void NauUsdPropertyInt3::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<pxr::GfVec3i>());
    pxr::GfVec3i point = value.Get<pxr::GfVec3i>();
    m_x->setValue(point.data()[0]);
    m_y->setValue(point.data()[1]);
    m_z->setValue(point.data()[2]);
}

PXR_NS::VtValue NauUsdPropertyInt3::getValue()
{
    return PXR_NS::VtValue(pxr::GfVec3i(m_x->value(), m_y->value(), m_z->value()));
}


// ** NauUsdPropertyDouble3

NauUsdPropertyDouble3::NauUsdPropertyDouble3(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdMultiValueDoubleSpinBoxPropertyBase(propertyTitle, {"X", "Y", "Z"}, parent)
    , m_x((*m_multiLineEdit)[0])
    , m_y((*m_multiLineEdit)[1])
    , m_z((*m_multiLineEdit)[2])
{
    m_multiLineEdit->setMinimum(std::numeric_limits<float>::lowest());
    m_multiLineEdit->setMaximum(std::numeric_limits<float>::max());
}

void NauUsdPropertyDouble3::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<pxr::GfVec3d>());
    pxr::GfVec3d point = value.Get<pxr::GfVec3d>();
    m_x->setValue(point.data()[0]);
    m_y->setValue(point.data()[1]);
    m_z->setValue(point.data()[2]);
}

PXR_NS::VtValue NauUsdPropertyDouble3::getValue()
{
    return PXR_NS::VtValue(pxr::GfVec3d(m_x->value(), m_y->value(), m_z->value()));
}


// ** NauUsdPropertyInt4

NauUsdPropertyInt4::NauUsdPropertyInt4(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdMultiValueSpinBoxPropertyBase(propertyTitle, { "X", "Y", "Z", "W" }, parent)
    , m_x((*m_multiLineEdit)[0])
    , m_y((*m_multiLineEdit)[1])
    , m_z((*m_multiLineEdit)[2])
    , m_w((*m_multiLineEdit)[3])
{
    m_multiLineEdit->setMinimum(std::numeric_limits<int>::lowest());
    m_multiLineEdit->setMaximum(std::numeric_limits<int>::max());
}

void NauUsdPropertyInt4::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<pxr::GfVec4i>());
    pxr::GfVec4i point = value.Get<pxr::GfVec4i>();
    m_x->setValue(point.data()[0]);
    m_y->setValue(point.data()[1]);
    m_z->setValue(point.data()[2]);
    m_w->setValue(point.data()[3]);
}

PXR_NS::VtValue NauUsdPropertyInt4::getValue()
{
    return PXR_NS::VtValue(pxr::GfVec4i(m_x->value(), m_y->value(), m_z->value(), m_w->value()));
}


// ** NauUsdPropertyPoint4

NauUsdPropertyDouble4::NauUsdPropertyDouble4(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdMultiValueDoubleSpinBoxPropertyBase(propertyTitle, {"X", "Y", "Z", "W"}, parent)
    , m_x((*m_multiLineEdit)[0])
    , m_y((*m_multiLineEdit)[1])
    , m_z((*m_multiLineEdit)[2])
    , m_w((*m_multiLineEdit)[3])
{
    m_multiLineEdit->setMinimum(std::numeric_limits<float>::lowest());
    m_multiLineEdit->setMaximum(std::numeric_limits<float>::max());
}

void NauUsdPropertyDouble4::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<pxr::GfVec4d>());
    pxr::GfVec4d point = value.Get<pxr::GfVec4d>();
    m_x->setValue(point.data()[0]);
    m_y->setValue(point.data()[1]);
    m_z->setValue(point.data()[2]);
    m_w->setValue(point.data()[3]);
}

PXR_NS::VtValue NauUsdPropertyDouble4::getValue()
{
    return PXR_NS::VtValue(pxr::GfVec4d(m_x->value(), m_y->value(), m_z->value(), m_w->value()));
}


// ** NauUsdPropertyMatrix

// TODO: propertyTitle is now not used in any way and has been added for uniform work with properties
// Fix it later
NauUsdPropertyMatrix::NauUsdPropertyMatrix(const std::string& propertyTitle, NauWidget* parent)
    : NauUsdMultiRowPropertyBase(parent)
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

    m_scale->setMinimum(NAU_SCALE_MIN_LIMITATION);
    m_scale->setMaximum(NAU_SCALE_MAX_LIMITATION);

    // This is a temporary solution to keep the real values accurate enough when working with the editor and to avoid nulling the transform matrix
    // TODO: Fix the matrix nulling in another more correct way.
    // Most likely on the engine side
    constexpr int transformDecimalPrecision = 2;
    m_position->setDecimals(transformDecimalPrecision);
    m_rotation->setDecimals(transformDecimalPrecision);
    m_scale->setDecimals(transformDecimalPrecision);

    connect(m_position.get(), &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauUsdMultiValueDoubleSpinBoxPropertyBase::eventValueChanged);
    connect(m_rotation.get(), &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauUsdMultiValueDoubleSpinBoxPropertyBase::eventValueChanged);
    connect(m_scale.get(), &NauMultiValueDoubleSpinBox::eventValueChanged, this, &NauUsdMultiValueDoubleSpinBoxPropertyBase::eventValueChanged);

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

void NauUsdPropertyMatrix::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<pxr::GfMatrix4d>());
    const pxr::GfMatrix4d& matrix = value.Get<pxr::GfMatrix4d>();

    const QMatrix3x3 trsComposition = NauUsdEditorMathUtils::gfMatrixToTrsComposition(matrix);

    for (int i = 0; i < 3; ++i) {
        (*m_position)[i]->setValue(trsComposition.data()[i]);
        (*m_rotation)[i]->setValue(trsComposition.data()[i + 3]);
        (*m_scale)[i]->setValue(trsComposition.data()[i + 6]);
    }
}

PXR_NS::VtValue NauUsdPropertyMatrix::getValue()
{
    // TODO: Work with mathematics will be translated into functions and types from Qt

    QMatrix3x3 trsComposition;

    for (int i = 0; i < 3; ++i) {
        trsComposition.data()[i] = ((*m_position)[i]->value());
        trsComposition.data()[i + 3] = ((*m_rotation)[i]->value());
        trsComposition.data()[i + 6] = ((*m_scale)[i]->value());
    }

    pxr::GfMatrix4d matrix = NauUsdEditorMathUtils::trsCompositionToGfMatrix(trsComposition);

    return PXR_NS::VtValue(matrix);
}
#pragma endregion


// ** NauReferenceProperty

NauReferenceProperty::NauReferenceProperty(const std::string& propertyTitle /*= "Reference Mesh Property"*/, const std::string& metaInfo /*= ""*/, NauWidget* parent /*= nullptr*/)
    : NauUsdSingleRowPropertyBase(propertyTitle, parent)
    , m_comboBox(new NauInspectorUSDResourceComboBox(NauInspectorUSDResourceComboBox::ShowPrimNameMode, this))
{
    m_assetType = magic_enum::enum_cast<NauEditorFileType>(metaInfo).value_or(NauEditorFileType::Unrecognized);

    m_comboBox->setFixedHeight(32);
    m_comboBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_comboBox->setAssetType(m_assetType);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_comboBox, 0, 2, 0, 4);

    connect(m_comboBox, QOverload<>::of(&NauInspectorUSDResourceComboBox::eventSelectionChanged), this, &NauAssetProperty::eventValueChanged);
}

PXR_NS::VtValue NauReferenceProperty::getValue()
{
    auto dataUid = m_comboBox->getCurrentUid();
    auto dataPrimPath = m_comboBox->getCurrentPrimPath();

    PXR_NS::SdfReference value;
    value.SetAssetPath("uid:"+toString(dataUid));
    value.SetPrimPath(pxr::SdfPath(dataPrimPath));
    return PXR_NS::VtValue(value);
}

void NauReferenceProperty::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<PXR_NS::SdfReference>());
    auto assetVal = value.Get<PXR_NS::SdfReference>();

    if (assetVal.GetAssetPath() == "") {
        return;
    }

    auto& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();

    std::string uidStr = "";
    if (assetVal.GetAssetPath().starts_with("uid:")) {
        uidStr = assetVal.GetAssetPath().substr(4);
    }

    auto uid = nau::Uid::parseString(uidStr);

    std::string newAssetPath = assetVal.GetAssetPath();

    if (assetVal.GetAssetPath().starts_with("../")) {
        newAssetPath = assetVal.GetAssetPath().substr(3);
    }

    if (!uid) {
        uid = assetDb.getUidFromNausdPath(newAssetPath.c_str());
    }

    if (!uid) {
        uid = assetDb.getUidFromSourcePath(newAssetPath.c_str());
    }

    if (!uid) {
        return;
    }

    auto& assetProcessor = Nau::EditorServiceProvider().get<NauAssetFileProcessorInterface>();

    std::string sourcePath = nau::FileSystemExtensions::resolveToNativePathContentFolder(newAssetPath.c_str());

    if (sourcePath.empty()) {
        sourcePath = nau::FileSystemExtensions::resolveToNativePathContentFolder(assetDb.getNausdPathFromUid(*uid).c_str());
    }

    auto stage = pxr::UsdStage::Open(sourcePath);

    if (!stage) {
        return;
    }

    auto prim = stage->GetPrimAtPath(assetVal.GetPrimPath());
    const auto metaInfo = nau::UsdMetaManager::instance().getPrimInfo(prim);

    m_comboBox->setText(QString(metaInfo.name.c_str()));
    m_comboBox->setCurrentData(*uid, assetVal.GetPrimPath().GetAsString());
}


// ** NauReferenceMeshProperty

NauAssetProperty::NauAssetProperty(const std::string& propertyTitle /*= "AssetMesh Property"*/,
    const std::string& metaInfo /*= ""*/, bool clearable, NauWidget* parent /*= nullptr*/)
    : NauUsdSingleRowPropertyBase(propertyTitle, parent)
    , m_comboBox(new NauInspectorUSDResourceComboBox(NauInspectorUSDResourceComboBox::ShowFileNameMode, this))
{
    m_assetType = magic_enum::enum_cast<NauEditorFileType>(metaInfo).value_or(NauEditorFileType::Unrecognized);

    m_comboBox->setFixedHeight(32);
    m_comboBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_comboBox->setAssetType(m_assetType);

    // Controllers typically occupy 2/3 of the width of the inspector
    m_contentLayout->addWidget(m_comboBox, 0, 2, 0, 4);

    if (clearable) {
        m_comboBox->addClearSelectionButton();
    }
    
    connect(m_comboBox, QOverload<const QString&>::of(&NauInspectorUSDResourceComboBox::eventSelectionChanged), this, &NauAssetProperty::eventValueChanged);
}

PXR_NS::VtValue NauAssetProperty::getValue()
{
    return PXR_NS::VtValue(PXR_NS::SdfAssetPath(("uid:"+toString(m_comboBox->getCurrentUid())).c_str()));
}

void NauAssetProperty::setValueInternal(const PXR_NS::VtValue& value)
{
    NED_ASSERT(value.IsHolding<PXR_NS::SdfAssetPath>());
    auto assetVal = value.Get<PXR_NS::SdfAssetPath>();

    if (assetVal.GetAssetPath() == "") {
        return;
    }

    auto& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();

    std::string uidStr = "";
    if (assetVal.GetAssetPath().starts_with("uid:")) {
        uidStr = assetVal.GetAssetPath().substr(4);
    }

    auto uid = nau::Uid::parseString(uidStr);

    if (!uid) {
        uid = assetDb.getUidFromNausdPath(assetVal.GetAssetPath().c_str());
    }

    if (!uid) {
        uid = assetDb.getUidFromSourcePath(assetVal.GetAssetPath().c_str());
    }

    if (!uid) {
        return;
    }

    m_comboBox->setText(QString(assetDb.getSourcePathFromUid(*uid).c_str()));
    m_comboBox->setCurrentData(*uid, "/Asset");
}


// ** NauInspectorUSDResourceComboBox

NauInspectorUSDResourceComboBox::NauInspectorUSDResourceComboBox(ShowTypes type /*= ShowFileNameMode*/, NauWidget* parent /*= nullptr*/)
    : NauResourceComboBox(parent)
    , m_viewType(type)
{
    setIcon(Nau::Theme::current().iconResourcePlaceholder());
}

void NauInspectorUSDResourceComboBox::setAssetType(NauEditorFileType assetType)
{
    m_assetTypes.clear();
    m_assetTypes.insert(assetType);
}

void NauInspectorUSDResourceComboBox::setAssetTypes(std::set<NauEditorFileType> assetTypes)
{
    m_assetTypes = assetTypes;
}

void NauInspectorUSDResourceComboBox::showPopup()
{
    QSignalBlocker blocker(this);
    updateItems();  // TODO: Optimize! No need to do this every single time
    NauAbstractComboBox::showPopup();
}

void NauInspectorUSDResourceComboBox::updateItems()
{
    clear();
    auto& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();

    for (const auto& assetType : m_assetTypes) {

        const auto& assetList = assetDb.findAssetMetaInfoByKind(magic_enum::enum_name(assetType).data());
        for (const auto& assetMetaInfo : assetList) {

            switch (m_viewType) {

            case ShowTypes::ShowFileNameMode:
                // Asset database is not cleaned up when an asset is deleted, so skip missing ones
                if (!QFileInfo::exists(QString::fromStdString(nau::FileSystemExtensions::resolveToNativePathContentFolder(assetMetaInfo.nausdPath.c_str())))) {
                    break;
                }
                addItem(QString(assetMetaInfo.sourcePath.c_str()), assetMetaInfo.uid, "/Asset", assetType);
                break;
            case ShowTypes::ShowPrimNameMode: {
                auto& assetProcessor = Nau::EditorServiceProvider().get<NauAssetFileProcessorInterface>();
                const std::string sourcePath = nau::FileSystemExtensions::resolveToNativePathContentFolder(assetMetaInfo.nausdPath.c_str());
                auto primsPath = assetProcessor.getAssetPrimsPath(sourcePath);

                auto stage = pxr::UsdStage::Open(sourcePath);

                if (!stage) {
                    break;
                }

                for (auto primPath : primsPath) {
                    auto prim = stage->GetPrimAtPath(pxr::SdfPath(primPath));
                    const auto metaInfo = nau::UsdMetaManager::instance().getPrimInfo(prim);

                    // TODO: Fix me Later
                    // Add "group"
                    if (metaInfo.type == "mesh") {
                        addItem(QString(metaInfo.name.c_str()), assetMetaInfo.uid, primPath, assetType);
                    }
                    if (metaInfo.type == "vfx") {
                        addItem(QString(assetMetaInfo.sourcePath.c_str()), assetMetaInfo.uid, primPath, assetType);
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    }
}
