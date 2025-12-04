// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Panel designed to accommodate the basic entities of the engine

#pragma once

#include "nau/nau_editor_config.hpp"

#include "baseWidgets/nau_widget.hpp"
#include "baseWidgets/nau_label.hpp"
#include "scene/nau_world.hpp"
#include "nau_action.hpp"


// ** NauTabsType
//
// An enum that is used to describe the available tab classes

// TODO: Start using the "MagicEnum" library.
// And then the enumeration should be semi-automatically built
// according to the names of the corresponding tab classes.
enum class NauTabsType
{
    MeshTab,            // Tab that contains known base primitives
    LightTab,
    AllTemplatesTab,    // A tab that contains all spawnable entity templates
};


#pragma region PANEL

class NauEntityCreationBasicTab;
class NauEntityCreationTabBar;
class NauEntityCreationBasicTab;
class NauEntityCreationMeshTab;
class NauEntityCreationTemplatesTab;

// ** NauPlaceEntityPanel
//
// Panel for placing basic entities on the stage

class NauEntityCreationPanel : public NauWidget
{
    Q_OBJECT

public:
    NauEntityCreationPanel(NauWidget* parent = nullptr);
    void addNewTab(const std::string& tabToolTip, const std::string& tabName, const std::string& iconPath, NauTabsType tabType);

signals:
    void eventCreateEntity(const std::string& templateName, const std::string& displayTemplateName, const std::shared_ptr<NauPropertiesContainer>& overrides) const;
    void updateTemplatesInfo(const NauTemplatesData& templatesInfo);

private:
    NauEntityCreationTabBar* m_tabBar;

    NauLayoutVertical* m_mainLayout;
    NauLayoutStacked* m_contentLayout;
};
#pragma endregion


#pragma region TabBar

// ** NauEntityCreationTabButton
//
// Base class for all tab switch buttons

class NauEntityCreationTabButton : public NauToolButton
{
    Q_OBJECT

public:
    NauEntityCreationTabButton(const std::string& tabToolTip, const std::string& tabName, const std::string& iconPath, QWidget* parent = nullptr);

signals:
    void eventShowTab(const std::string& tabName) const;

private:
};


// ** NauPlaceEntityTabBar
//
// Tab manager for the base entity placement panel

class NauEntityCreationTabBar : public NauWidget
{
    Q_OBJECT

public:
    NauEntityCreationTabBar(NauWidget* parent = nullptr);

    void addTabButton(const std::string& tabToolTip, const std::string& tabName, const std::string& iconPath);
    void setCurrentIndex(int requestedIndex);

signals:
    void eventShowTab(int requestedTabIndex) const;

private:
    void tabPressed(int requestedTabIndex, const std::string& tabName);
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<NauEntityCreationTabButton*> m_tabButtons;
    int m_currentTabIndex;

    NauLayoutVertical* m_mainLayout;
    NauLayoutHorizontal* m_buttonsLayout;

    NauLabel* m_tabNameLabel;
};
#pragma endregion


#pragma region TABS

// ** NauPlaceEntityBasicTab
//
// Base class for a tab

// TODO: Ideally, the class should be abstract...
class NauEntityCreationBasicTab : public NauWidget
{
    Q_OBJECT

public:
    NauEntityCreationBasicTab(NauWidget* parent = nullptr);

signals:
    void eventCreateEntity(const std::string& templateName, const std::string& displayTemplateName, const std::shared_ptr<NauPropertiesContainer>& overrides) const;

protected:
    NauLayoutVertical* m_layout;
};


// ** NauPlaceEntityMeshTab
//
// Tab with basic geometric objects

class NauEntityCreationMeshTab : public NauEntityCreationBasicTab
{
    Q_OBJECT

public:
    NauEntityCreationMeshTab(NauWidget* parent = nullptr);
};


// ** NauEntityCreationLightTab
//
// Tab with basic lighting objects

class NauEntityCreationLightTab : public NauEntityCreationBasicTab
{
    Q_OBJECT

public:
    NauEntityCreationLightTab(NauWidget* parent = nullptr);
};


// ** NauPlaceEntityTemplatesTab
//
// Tab with all project entities

class NauEntityCreationTemplatesTab : public NauEntityCreationBasicTab
{
    Q_OBJECT

public:
    NauEntityCreationTemplatesTab(NauWidget* parent = nullptr);
    void updateTemplatesList(const NauTemplatesData& templatesInfo);

};
#pragma endregion


#pragma region FACTORY

// ** NauTabsFactory
//
// Factory that creates the given tab class based on the enum

class NauTabsFactory : public QObject
{
    Q_OBJECT

public:
    NauTabsFactory() = default;

    static NauEntityCreationBasicTab* createEntityCreationTab(NauTabsType tabType, NauEntityCreationPanel* parent);
};
#pragma endregion


#pragma region BUTTON

// ** NauPlaceEntityButton
//
// Entity spawn button

class NauEntityCreationButton : public NauWidget
{
    Q_OBJECT

public:
    NauEntityCreationButton(const std::string& meshName, const std::string& templateName, const std::string& displayTemplateName,
        const std::shared_ptr<NauPropertiesContainer>& overrides = std::make_shared<NauPropertiesContainer>(), const std::string& iconPath = "", NauWidget* parent = nullptr);

signals:
    void eventCreateEntity(const std::string& templateName, const std::string& displayTemplateName, const std::shared_ptr<NauPropertiesContainer>& overrides) const;

public:
    const std::string templateName;
    const std::string displayTemplateName;
    const std::shared_ptr<NauPropertiesContainer> overridesMap;

private:
    void buttonPressed();

private:
    NauLayoutStacked* m_layout;
    NauToolButton* m_button;
};
#pragma endregion


// ** NauObjectCreationList
// Temp creation panel

class NAU_EDITOR_API NauObjectCreationList : public NauMenu
{
    Q_OBJECT
public:
    explicit NauObjectCreationList(NauWidget* parent = nullptr);

    void initTypesList(const std::vector<std::pair<std::string, std::string>> &types);

    void updateTypeList(const std::string& typeName, bool state);

signals:
    void eventCreateObject(const std::string& typeName);

private:
    // create separate class for creation funcs
    void createObject(const std::string& path);

private:
    std::vector<std::pair<std::string, std::string>> m_typesList;
    std::unordered_map<std::string, NauAction*> m_actionMap;
};
