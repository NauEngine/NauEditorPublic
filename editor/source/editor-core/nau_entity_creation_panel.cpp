// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_entity_creation_panel.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"

#include <QTimer>


#pragma region PANEL

// ** NauPlaceEntityPanel

NauEntityCreationPanel::NauEntityCreationPanel(NauWidget* parent)
    : NauWidget(parent)
    , m_tabBar(new NauEntityCreationTabBar(this))
    , m_mainLayout(new NauLayoutVertical(this))
    , m_contentLayout(new NauLayoutStacked())
{
    setWindowFlags(Qt::Popup);

    // Setup layouts
    m_mainLayout->addWidget(m_tabBar);
    m_mainLayout->addLayout(m_contentLayout);

    // Setup tab

    // TODO: The architecture is designed in such a way that in the future it is not expected to create tabs manually.
    // They should be created automatically according to some meta-information,
    // for example, from a file, and then connected with each other using internal indexing.
    // Therefore, it is worth considering where and how to store this very meta information.

    addNewTab("Shapes tab", "SHAPES", ":/UI/icons/iMeshTab.png", NauTabsType::MeshTab);
    addNewTab("Light tab", "LIGHT", ":/UI/icons/iLightTab.png", NauTabsType::LightTab);
    addNewTab("All Templates tab", "ALL TEMPLATES", ":/UI/icons/iAllTemplateTab.png", NauTabsType::AllTemplatesTab);

    m_tabBar->setCurrentIndex(0);
}

void NauEntityCreationPanel::addNewTab(const std::string& tabToolTip, const std::string& tabName, const std::string& iconPath, NauTabsType tabType)
{
    // Setup tab bar button
    m_tabBar->addTabButton(tabToolTip, tabName, iconPath);
    connect(m_tabBar, &NauEntityCreationTabBar::eventShowTab, this, [this](int requestedTabIndex) { m_contentLayout->setCurrentIndex(requestedTabIndex); });

    // Setup tab
    auto tab = NauTabsFactory::createEntityCreationTab(tabType, this);
    m_contentLayout->addWidget(tab);
    connect(tab, &NauEntityCreationBasicTab::eventCreateEntity, this, &NauEntityCreationPanel::eventCreateEntity);
}
#pragma endregion


#pragma region TabBar

// ** NauPlaceEntityTabBar

NauEntityCreationTabButton::NauEntityCreationTabButton(const std::string& tabToolTip, const std::string& tabName, const std::string& iconPath, QWidget* parent)
{
    setToolTip(tr(tabToolTip.c_str()));
    setIcon(QIcon(iconPath.c_str()));
    setIconSize(QSize(24, 24));
    setCheckable(true);

    connect(this, &NauEntityCreationTabButton::pressed, this, [this, tabName]() { emit eventShowTab(tabName); });
}


// ** NauPlaceEntityTabBar

NauEntityCreationTabBar::NauEntityCreationTabBar(NauWidget* parent)
    : NauWidget(parent)
    , m_mainLayout(new NauLayoutVertical(this))
    , m_buttonsLayout(new NauLayoutHorizontal())
    , m_tabNameLabel(new NauLabel("", this))
    , m_currentTabIndex(0)
{
    // Setup layouts
    m_buttonsLayout->setSpacing(12);
    m_buttonsLayout->setAlignment(Qt::AlignCenter);

    m_mainLayout->setSpacing(6);
    m_mainLayout->setContentsMargins(0, 6, 0, 6);
    m_mainLayout->setAlignment(Qt::AlignCenter);
    m_mainLayout->addLayout(m_buttonsLayout);
    m_mainLayout->addWidget(m_tabNameLabel);

    // Setup label
    m_tabNameLabel->setFont(QFont(m_tabNameLabel->fontInfo().family(), 9, QFont::Bold));
    m_tabNameLabel->setAlignment(Qt::AlignCenter);
}

void NauEntityCreationTabBar::addTabButton(const std::string& tabToolTip, const std::string& tabName, const std::string& iconPath)
{
    m_tabButtons.push_back(new NauEntityCreationTabButton(tabToolTip, tabName, iconPath, this));
    m_buttonsLayout->addWidget(m_tabButtons.back());

    int itemIndex = static_cast<int>(m_tabButtons.size()) - 1;
    connect(m_tabButtons.back(), &NauEntityCreationTabButton::eventShowTab, this, [this, itemIndex](const std::string& tabName) { tabPressed(itemIndex, tabName); });
}

void NauEntityCreationTabBar::setCurrentIndex(int requestedIndex)
{
    NED_ASSERT(m_tabButtons.size() - 1 > requestedIndex);

    // Maybe it's a hack, but simple and concise
    m_tabButtons[requestedIndex]->pressed();
}

void NauEntityCreationTabBar::tabPressed(int requestedTabIndex, const std::string& tabName)
{
    m_tabButtons[m_currentTabIndex]->setChecked(false);
    m_tabButtons[m_currentTabIndex]->setEnabled(true);

    m_tabButtons[requestedTabIndex]->setChecked(true);
    m_tabButtons[requestedTabIndex]->setEnabled(false);

    m_currentTabIndex = requestedTabIndex;

    m_tabNameLabel->setText(tr(tabName.c_str()));
    emit eventShowTab(requestedTabIndex);
}

void NauEntityCreationTabBar::paintEvent(QPaintEvent* event)
{
    QStyleOption styleOption;
    styleOption.initFrom(this);

    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &styleOption, &painter, this);

    QWidget::paintEvent(event);
}
#pragma endregion


#pragma region TABS

// ** NauPlaceEntityBasicTab

NauEntityCreationBasicTab::NauEntityCreationBasicTab(NauWidget* parent)
    : NauWidget(parent)
    , m_layout(new NauLayoutVertical(this))
{
    // Setup layout
    m_layout->setContentsMargins(6, 6, 6, 0);
    m_layout->setSpacing(6);
}


// ** NauPlaceEntityMeshTab

NauEntityCreationMeshTab::NauEntityCreationMeshTab(NauWidget* parent)
    : NauEntityCreationBasicTab(parent)
{
    // Setup buttons for known entities
    // TODO: Prepare normally named templates
    NauPropertiesContainer overrides;
    overrides["ri_extra__name"] = NauObjectProperty("ri_extra__name", (QString("box_base")), "Static Mesh");
    auto cubeOverridesPtr = std::make_shared<NauPropertiesContainer>(overrides);
    auto cubeButton = new NauEntityCreationButton("Box", "static_mesh", "Box", cubeOverridesPtr, ":/UI/icons/iCubeMesh.png");

    overrides["ri_extra__name"] = NauObjectProperty("ri_extra__name", (QString("plane_base")), "Static Mesh");
    auto planeOverridesPtr = std::make_shared<NauPropertiesContainer>(overrides);
    auto planeButton = new NauEntityCreationButton("Plane", "static_mesh", "Plane", planeOverridesPtr, ":/UI/icons/iPlaneMesh.png");

    overrides["ri_extra__name"] = NauObjectProperty("ri_extra__name", (QString("cone_base")), "Static Mesh");
    auto coneOverridesPtr = std::make_shared<NauPropertiesContainer>(overrides);
    auto coneButton = new NauEntityCreationButton("Cone", "static_mesh", "Cone", coneOverridesPtr, ":/UI/icons/iConeMesh.png");

    overrides["ri_extra__name"] = NauObjectProperty("ri_extra__name", (QString("cylinder_base")), "Static Mesh");
    auto cylinderOverridesPtr = std::make_shared<NauPropertiesContainer>(overrides);
    auto cylinderButton = new NauEntityCreationButton("Cylinder", "static_mesh", "Cylinder", cylinderOverridesPtr, ":/UI/icons/iCylinderMesh.png");

    overrides["ri_extra__name"] = NauObjectProperty("ri_extra__name", (QString("sphere_base")), "Static Mesh");
    auto sphereOverridesPtr = std::make_shared<NauPropertiesContainer>(overrides);
    auto sphereButton = new NauEntityCreationButton("Sphere", "static_mesh", "Sphere", sphereOverridesPtr, ":/UI/icons/iSphereMesh.png");

    connect(cubeButton, &NauEntityCreationButton::eventCreateEntity, this, &NauEntityCreationBasicTab::eventCreateEntity);
    connect(planeButton, &NauEntityCreationButton::eventCreateEntity, this, &NauEntityCreationBasicTab::eventCreateEntity);
    connect(coneButton, &NauEntityCreationButton::eventCreateEntity, this, &NauEntityCreationBasicTab::eventCreateEntity);
    connect(cylinderButton, &NauEntityCreationButton::eventCreateEntity, this, &NauEntityCreationBasicTab::eventCreateEntity);
    connect(sphereButton, &NauEntityCreationButton::eventCreateEntity, this, &NauEntityCreationBasicTab::eventCreateEntity);

    // Setup layout
    m_layout->addWidget(cubeButton);
    m_layout->addWidget(planeButton);
    m_layout->addWidget(coneButton);
    m_layout->addWidget(cylinderButton);
    m_layout->addWidget(sphereButton);

    m_layout->addStretch(1);
}


// ** NauEntityCreationLightTab

NauEntityCreationLightTab::NauEntityCreationLightTab(NauWidget* parent)
{
    // Setup buttons for known entities
    // TODO: Prepare normally named templates
    auto omniLightButton = new NauEntityCreationButton("Omni Light", "nOmniLight", "Omni Light", std::make_shared<NauPropertiesContainer>(), ":/UI/icons/iOmniLight.png");
    auto spotLightButton = new NauEntityCreationButton("Spot Light", "nSpotLight", "Spot Light", std::make_shared<NauPropertiesContainer>(), ":/UI/icons/iSpotLight.png");

    connect(omniLightButton, &NauEntityCreationButton::eventCreateEntity, this, &NauEntityCreationBasicTab::eventCreateEntity);
    connect(spotLightButton, &NauEntityCreationButton::eventCreateEntity, this, &NauEntityCreationBasicTab::eventCreateEntity);

    // Setup layout
    m_layout->addWidget(omniLightButton);
    m_layout->addWidget(spotLightButton);

    m_layout->addStretch(1);
}


// ** NauPlaceEntityTemplatesTab

NauEntityCreationTemplatesTab::NauEntityCreationTemplatesTab(NauWidget* parent)
    : NauEntityCreationBasicTab(parent)
{
}

void NauEntityCreationTemplatesTab::updateTemplatesList(const NauTemplatesData& templatesInfo)
{
    m_layout->clear();

    // TODO: In the future, we don't have to directly access the file to get some data.
    // Widgets should not have any logic.
    // All data must be encapsulated in a data model from which the widget will read data.
    for (const auto& templateInfo : templatesInfo.getTemplatesInfo()) {
        if (templateInfo.second.canBeCreatedFromEditor()) {
            auto button = new NauEntityCreationButton(templateInfo.first, templateInfo.first, templateInfo.second.displayName());
            connect(button, &NauEntityCreationButton::eventCreateEntity, this, &NauEntityCreationBasicTab::eventCreateEntity);
            m_layout->addWidget(button);
        }
    }

    m_layout->addStretch(1);
}
#pragma endregion


#pragma region FACTORY
NauEntityCreationBasicTab* NauTabsFactory::createEntityCreationTab(NauTabsType tabType, NauEntityCreationPanel* parent)
{
    switch (tabType)
    {
    case NauTabsType::MeshTab:
    {
        return new NauEntityCreationMeshTab(parent);
    }
    case NauTabsType::LightTab:
    {
        return new NauEntityCreationLightTab(parent);
    }
    case NauTabsType::AllTemplatesTab:
    {
        auto templatesTab = new NauEntityCreationTemplatesTab(parent);
        connect(parent, &NauEntityCreationPanel::updateTemplatesInfo, templatesTab, &NauEntityCreationTemplatesTab::updateTemplatesList);

        return templatesTab;
    }
    }

    return nullptr;
}
#pragma endregion


#pragma region BUTTON

// ** NauPlaceEntityButton

NauEntityCreationButton::NauEntityCreationButton(const std::string& meshName, const std::string& templateName,
    const std::string& displayTemplateName, const std::shared_ptr<NauPropertiesContainer>& overrides, const std::string& iconPath, NauWidget* parent)
    : NauWidget(parent)
    , templateName(templateName)
    , displayTemplateName(displayTemplateName)
    , overridesMap(overrides)
    , m_layout(new NauLayoutStacked(this))
    , m_button(new NauToolButton(this))
{
    // Setup entity button
    m_button->setFixedHeight(50);
    m_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_button->setToolTip(tr("Spawn entity from this template"));

    m_button->setText(tr(meshName.c_str()));
    m_button->setFont(QFont(m_button->fontInfo().family(), 12, QFont::Bold));
    m_button->setStyleSheet("QToolButton {border: 2px solid #8f8f91;} QToolButton:hover {background-color: #2B3946; color: #ffffff;}");

    // This widget works in two modes.
    // In preset mode, when it uses which image.
    // In generative mode, when it is not possible to get any image for an entity.

    // TODO: Make it possible to receive images from third-party resources.
    // For example, getting the path to an image from .blk
    if (iconPath != "") {
        m_button->setIcon(QIcon(iconPath.c_str()));
        m_button->setIconSize(QSize(32, 32));
    }

    connect(m_button, &NauToolButton::clicked, this, &NauEntityCreationButton::buttonPressed);

    // Setup layout
    m_layout->setStackingMode(QStackedLayout::StackAll);
    m_layout->addWidget(m_button);
}

void NauEntityCreationButton::buttonPressed()
{
    emit eventCreateEntity(templateName, displayTemplateName, overridesMap);
}
#pragma endregion


// ** NauObjectCreationList

NauObjectCreationList::NauObjectCreationList(NauWidget* parent)
    : NauMenu(tr("Add object"), parent)
{

}

void NauObjectCreationList::initTypesList(const std::vector<std::pair<std::string, std::string>>& types)
{
    m_actionMap.clear();
    clear();

    m_typesList = types;
    for (auto& typeInfo : m_typesList) {
        if (typeInfo.first.empty()) {
            addSeparator();
            continue;
        }

        m_actionMap[typeInfo.first] = addAction(typeInfo.second.c_str(), [this, &typeInfo]() { createObject(typeInfo.first); });
    }
}

void NauObjectCreationList::updateTypeList(const std::string& typeName, bool state)
{
    m_actionMap[typeName]->setDisabled(state);
}

void NauObjectCreationList::createObject(const std::string& typeName)
{
    // TODO: Dirty hack
    // Fix in future updates
    if (typeName == "nau::scene::EnvironmentComponent") {
        updateTypeList(typeName, true);
    }

    emit eventCreateObject(typeName);
}
