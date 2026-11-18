// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Widgets for physics components

#pragma once

#include "nau\inspector\nau_usd_inspector_widgets.hpp"
#include "nau\inspector\nau_usd_property_factory.hpp"

#include <QScrollArea>


// ** NauInspectorPhysicsMaterialAsset

class NAU_USD_ASSET_EDITOR_COMMON_API NauInspectorPhysicsMaterialAsset : public NauAssetProperty
{
    Q_OBJECT

public:
    explicit NauInspectorPhysicsMaterialAsset(NauWidget* parent = nullptr);

    static NauUsdPropertyAbstract* Create(const std::string&, const std::string&);
};


// ** NauInspectorPhysicsCollisionButton

class NAU_USD_ASSET_EDITOR_COMMON_API NauInspectorPhysicsCollisionButton : public NauUsdSingleRowPropertyBase
{
    Q_OBJECT

public:
    explicit NauInspectorPhysicsCollisionButton(NauWidget* parent = nullptr);

    static NauUsdPropertyAbstract* Create(const std::string& propertyTitle, const std::string& metaInfo);

    PXR_NS::VtValue getValue() override;

protected:
    virtual void setValueInternal(const PXR_NS::VtValue& value);

private:
    NauMenu* m_popupMenu = nullptr;
};


// ** NauPhysicsChannelComboBox
// 
// ComboBox widget for working with physics channels.

class NAU_USD_ASSET_EDITOR_COMMON_API NauPhysicsChannelComboBox: public NauComboBox
{
public:
    NauPhysicsChannelComboBox(NauWidget* parent = nullptr);

protected:
    void showPopup() override;

private:
    void fillItems();
};


// ** NauInspectorPhysicsCollisionSelector

class NAU_USD_ASSET_EDITOR_COMMON_API NauInspectorPhysicsCollisionSelector : public NauUsdSingleRowPropertyBase
{
    Q_OBJECT

public:
    explicit NauInspectorPhysicsCollisionSelector(NauWidget* parent = nullptr);

    static NauUsdPropertyAbstract* Create(const std::string& propertyTitle, const std::string& metaInfo);

    PXR_NS::VtValue getValue() override;

protected:
    virtual void setValueInternal(const PXR_NS::VtValue& value);

private:
    NauPhysicsChannelComboBox* m_selector = nullptr;
};


// ** NauPhysicsCollisionSettingsWidget

class NauPhysicsCollisionSettingsWidget : public NauFrame
{
    Q_OBJECT
public:
    NauPhysicsCollisionSettingsWidget(QWidget* parent = nullptr);

signals:
    void eventSaveRequested();
    void eventCancelRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void setupUi();
    void rebuildMutableUi();
    void recreateContentArea();

    NauFrame* createAddPanel();
    NauFrame* createHeaderPanel();
    NauFrame* createButtonPanel();
    NauScrollWidgetVertical* createContentScrollArea();

    void updateAllCheckbox(std::vector<NauCheckBox*> channelCheckboxes, NauCheckBox* allCheckBox);

    void showEvent(QShowEvent *event) override;

private:
    NauScrollWidgetVertical* m_contentScrollArea = nullptr;

    // Used to attach the properties to checkboxes to distinguish their which channel they control.
    static const char* const m_propertyChannelId;
    static const char* const m_propertyGroupChannelId;
};


namespace Nau
{
    // Adds the physics property widgets to NauUsdPropertyFactory.
    // It logs, so it must run after NauLog::init(), not from a static initializer.
    NAU_USD_ASSET_EDITOR_COMMON_API void RegisterPhysicsPropertyWidgets();
}
