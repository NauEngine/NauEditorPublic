// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/inspector/nau_inspector.hpp"
#include "nau/math/nau_matrix_math.hpp"
#include "themes/nau_theme.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"
#include "nau_static_text_label.hpp"
#include "nau_buttons.hpp"
#include "nau_utils.hpp"

#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QQuaternion>
#include <QSignalBlocker>

#include <algorithm>
#include <format>


// ** NauInspectorPageHeader

NauInspectorPageHeader::NauInspectorPageHeader(const std::string& title, const std::string& subtitle, bool hideAddButton)
    : m_title(new NauStaticTextLabel(title.c_str(), this))
    , m_icon(new QLabel(this))
    , m_subtitle(new NauStaticTextLabel(subtitle.c_str(), this))
    , m_objectCreationList(new NauObjectCreationList(nullptr))
{
    setFixedHeight(Height);

    auto layout = new NauLayoutVertical(this);
    auto layoutMain = new NauLayoutHorizontal();
    layoutMain->setContentsMargins(QMargins(OuterMargin, OuterMargin, OuterMargin, OuterMargin));
    layout->addLayout(layoutMain);

    // Image
    // TODO: need some asset icon generation system in future.
    // Potentially, as a part of a theme.
    changeIcon(":/Inspector/icons/inspector/header.png");
    layoutMain->addWidget(m_icon);

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
    m_subtitle->setFont(Nau::Theme::current().fontInspectorHeaderSubtitle());
    m_subtitle->setColor(NauColor(255, 255, 255, 128));
    layoutTitle->addWidget(m_subtitle);
    layoutTitle->addStretch(1);

    // Bottom separator
    auto separator = new QFrame;
    separator->setStyleSheet(QString("background-color: #141414;"));
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    layout->addWidget(separator);

    // Add Button
    if (!hideAddButton) {
        auto addButton = new NauPrimaryButton();
        addButton->setText(tr("Add"));
        addButton->setIcon(Nau::Theme::current().iconAddPrimaryStyle());
        addButton->setFixedHeight(NauAbstractButton::standardHeight());
        layoutMain->addWidget(addButton);

        connect(addButton, &NauAbstractButton::clicked, [=]
        {
            if (m_objectCreationList && addButton) {
                const auto parentWidgetPosition = addButton->mapToGlobal(QPointF(0, 0)).toPoint();
                const auto correctWidgetPosition = Nau::Utils::Widget::fitWidgetIntoScreen(m_objectCreationList->sizeHint(), parentWidgetPosition);
                m_objectCreationList->popup(correctWidgetPosition);
            }
        });
    }
}

void NauInspectorPageHeader::changeIcon(const std::string& resource)
{
    m_icon->setPixmap(QPixmap(resource.c_str()).scaledToWidth(48, Qt::SmoothTransformation));
}

void NauInspectorPageHeader::changeTitle(const std::string& title)
{
    m_title->setText(title.c_str());
}

void NauInspectorPageHeader::changeSubtitle(const std::string& subtitle)
{
    m_subtitle->setText(subtitle.c_str());
}

NauObjectCreationList* NauInspectorPageHeader::creationList() const
{
    return m_objectCreationList;
}


// ** NauComponentSpoiler

NauComponentSpoiler::NauComponentSpoiler(const QString& title, const int animationDuration, NauWidget* parent)
    : NauSpoiler(title, animationDuration, parent)
    , m_areaMenu(std::make_unique<NauMenu>())
{
    createButtons();
    
    connect(m_areaMenu.get(), &QMenu::aboutToShow, this, &NauComponentSpoiler::updateMenus);
}

void NauComponentSpoiler::createButtons()
{
    auto menuButton = new NauToolButton(this);
    menuButton->setStyleSheet(QString());
    menuButton->setObjectName("nauInspectorSpoilerMenuButton");
    menuButton->setAutoRaise(true);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    menuButton->setMenu(m_areaMenu.get());
    menuButton->setIcon(Nau::Theme::current().iconDockAreaMenu());
    menuButton->setFixedSize(24, 24);

    m_headerLayout->addWidget(menuButton);
}

void NauComponentSpoiler::updateMenus()
{
    m_areaMenu->clear();
    m_areaMenu->addAction(tr("Remove"), this, &NauComponentSpoiler::eventComponentRemoved);
}


// ** NauInspectorPage

NauInspectorPage::NauInspectorPage(NauWidget* parent)
    : NauWidget(parent)
    , m_layout(new NauLayoutVertical(this))
{
}

NauComponentSpoiler* NauInspectorPage::addComponent(const std::string& componentName)
{
    if (componentName.empty()) {
        return nullptr;
    }

    return m_componentSpoilers.emplace_back(new NauComponentSpoiler(tr(componentName.c_str()), 0, this));
}

void NauInspectorPage::clear()
{
    m_layout->clear();
    m_componentSpoilers.clear();
}

NauInspectorPageHeader* NauInspectorPage::addHeader(const std::string& objectName, const std::string& objectKind, bool hideAddButton)
{
    auto header = new NauInspectorPageHeader(objectName, objectKind, hideAddButton);
    m_layout->insertWidget(m_layout->count()-1, header);

    return header;
}

void NauInspectorPage::addSpoiler(NauComponentSpoiler* spoiler)
{
    m_layout->insertWidget(m_layout->count()-1, spoiler);
}

NauLayoutVertical* NauInspectorPage::mainLayout()
{
    return m_layout;
}