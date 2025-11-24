// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_dock_manager.hpp"
#include "nau_assert.hpp"
#include "themes/nau_theme.hpp"

#include <IconProvider.h>
#include <ElidingLabel.h>


// ** NauDockManager

NauDockManager::NauDockManager(QWidget* parent)
    : ads::CDockManager(parent)
{
    setStyleSheet(QString());
    iconProvider().registerCustomIcon(ads::eIcon::TabCloseIcon, Nau::Theme::current().iconClose());
    iconProvider().registerCustomIcon(ads::eIcon::AutoHideIcon, Nau::Theme::current().iconMinimize());

    // To be removed. See m_dockWidgets.
    connect(this, &ads::CDockManager::dockWidgetAdded, [this](ads::CDockWidget* widget) {
        m_dockWidgets.insert(widget);
    });
}

const std::set<ads::CDockWidget*>& NauDockManager::dockWidgets() const
{
    return m_dockWidgets;
}

ads::CDockWidget* NauDockManager::projectBrowser() const
{
    return m_projectBrowser;
}

void NauDockManager::setProjectBrowser(ads::CDockWidget* projectBrowser)
{
    m_projectBrowser = projectBrowser;
}

ads::CDockWidget* NauDockManager::inspector() const
{
    return m_inspector;
}

void NauDockManager::setInspector(ads::CDockWidget* inspector)
{
    m_inspector = inspector;
}


// ** NauDockAreaTitleBar

NauDockAreaTitleBar::NauDockAreaTitleBar(ads::CDockAreaWidget* parent)
    : ads::CDockAreaTitleBar(parent)
    , m_areaMenu(std::make_unique<NauMenu>())
    , m_addTabMenu(std::make_unique<NauMenu>())
    , m_dockManager(dynamic_cast<NauDockManager*>(parent->dockManager()))
    , m_areaWidget(dynamic_cast<ads::CDockAreaWidget*>(parent))
{
    NED_ASSERT(m_dockManager);
    NED_ASSERT(m_areaWidget);

    setObjectName("nauDockAreaTitleBar");
    setFont(Nau::Theme::current().fontDocking());
    m_addTabMenu->setTitle(tr("Add Tab"));

    createButtons();

    connect(m_areaMenu.get(), &QMenu::aboutToShow, this, &NauDockAreaTitleBar::updateMenus);
}

void NauDockAreaTitleBar::contextMenuEvent(QContextMenuEvent* event)
{
    event->accept();
    m_areaMenu->exec(event->globalPos());
}

void NauDockAreaTitleBar::createButtons()
{
    auto menuButton = new NauToolButton(this);
    menuButton->setStyleSheet(QString());
    menuButton->setObjectName("nauTabsMenuButton");
    menuButton->setAutoRaise(true);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    menuButton->setMenu(m_areaMenu.get());
    menuButton->setIcon(Nau::Theme::current().iconDockAreaMenu());
    menuButton->setFixedSize(24, 24);

    layout()->addWidget(menuButton);
}

void NauDockAreaTitleBar::updateMenus()
{
    m_areaMenu->clear();
    m_addTabMenu->clear();

    const auto dockArea = tabBar()->currentTab()->dockAreaWidget();
    const bool isAutoHide = dockArea->isAutoHide();
    const bool isTopLevelArea = dockArea->isTopLevelArea();
    const bool hasMultiplyTab = dockArea->openDockWidgetsCount() > 1;

    if (!isTopLevelArea && !isAutoHide) {
        connect(m_areaMenu->addAction(Nau::Theme::current().iconUndock(), tr("Undock Window")), &QAction::triggered, this, [this] {
            button(ads::TitleBarButtonUndock)->click();
        });

        connect(m_areaMenu->addAction(Nau::Theme::current().iconMinimize(), tr("Minimize Window")),
            SIGNAL(triggered()), SLOT(onAutoHideDockAreaActionClicked()));

        connect(m_areaMenu->addAction(Nau::Theme::current().iconWindowState(), tr("Maximize Window")), &QAction::triggered, [this] {
            button(ads::TitleBarButtonUndock)->click();
            if (auto floating = tabBar()->currentTab()->dockWidget()->floatingDockContainer()) {
                floating->showMaximized();
            }
        });
    }

    connect(m_areaMenu->addAction(Nau::Theme::current().iconClose(), tr("Close Window")),
        &QAction::triggered, button(ads::TitleBarButtonClose), &QAbstractButton::click);

    if (!isAutoHide) {
        m_areaMenu->addSeparator();
        const auto& widgets = m_dockManager->dockWidgets();

        for (ads::CDockWidget* dockWidget : widgets) {

            // Skip widget we already have.
            if (dockWidget->dockAreaWidget()->titleBar() == this && !dockWidget->isClosed()) {
                continue;
            }

            connect(m_addTabMenu->addAction(dockWidget->windowTitle()), &QAction::triggered, [this, dockWidget] {
                if (auto dockAreaWidget = dynamic_cast<ads::CDockAreaWidget*>(parentWidget())) {
                    m_dockManager->removeDockWidget(dockWidget);
                    m_dockManager->addDockWidgetTabToArea(dockWidget, dockAreaWidget);
                    dockWidget->toggleViewAction()->setChecked(true);
                }
            });
        }

        if (!m_addTabMenu->actions().empty()) {
            m_areaMenu->addMenu(m_addTabMenu.get());
        }
    }

    if (hasMultiplyTab) {
        connect(m_areaMenu->addAction(tr("Close Inactive Tabs")), &QAction::triggered, [this] {
            tabBar()->currentTab()->closeOtherTabsRequested();
        });
    }

    // TODO make this part of Undo/Redo subsystem.
    // m_areaMenu->addAction(tr("Reopen Closed Tabs"));
}


// ** NauDockAreaTabBar

NauDockAreaTabBar::NauDockAreaTabBar(ads::CDockAreaWidget* parent)
    :  ads::CDockAreaTabBar(parent)
{
    setObjectName("NauDockAreaTabBar");
}


// ** NauDockWidgetTab

NauDockWidgetTab::NauDockWidgetTab(ads::CDockWidget* parent)
    : ads::CDockWidgetTab(parent)
    , m_palette(Nau::Theme::current().paletteDocking())
{
    setObjectName("NauDockWidgetTab");
    layout()->setContentsMargins(16, 12, 16, 12);

    // ADS builds tab layout as following: [label-spaceitem-close-spaceitem].
    if (auto hLayout = dynamic_cast<QBoxLayout*>(layout())) {
        if (auto item = dynamic_cast<QSpacerItem*>(hLayout->itemAt(1))) {
            item->changeSize(m_gap, 1, QSizePolicy::Expanding, QSizePolicy::Minimum);
        }
        if (auto item = dynamic_cast<QSpacerItem*>(hLayout->itemAt(hLayout->count() - 1))) {
            item->changeSize(0, 1, QSizePolicy::Fixed, QSizePolicy::Minimum);
        }
    }

    connect(this, &NauDockWidgetTab::activeTabChanged, [this] {
        if (isActiveTab()) {
            resetFlashing();
            setBadgeTextVisible(false);
        }

        updateUi();
    });

    if (auto adsNativeLabel = findChild<ads::CElidingLabel*>()) {
        // We have our own painting of label so hide ADS label. Shrinking to zero height(instead of hiding)
        // makes layout taking into account width of ADS label so size of this tab is appropriate.
        adsNativeLabel->setFixedHeight(0);
    }

    updateUi();
}

void NauDockWidgetTab::setFlashingEnabled(bool enabled)
{
    if (!enabled) {
        resetFlashing();
        return;
    }

    m_backgroundFlasher = engageColorAnimator(
        m_palette.color(NauPalette::Role::BackgroundHeader, NauPalette::Flashing),
        m_palette.color(NauPalette::Role::AlternateBackgroundHeader, NauPalette::Flashing));
    
    m_foregroundFlasher = engageColorAnimator(
        m_palette.color(NauPalette::Role::ForegroundHeader, {}, NauPalette::Category::Inactive),
        m_palette.color(NauPalette::Role::ForegroundHeader, {}, NauPalette::Category::Active));
}

void NauDockWidgetTab::setBadgeTextVisible(bool visible)
{
    m_badgeVisible = visible;
    updateUi();
}

void NauDockWidgetTab::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), m_backgroundBrush);
    const QRect contentRect = event->rect() - layout()->contentsMargins();

    static const int closeButtonSize = 16;

    QRect textRect = contentRect;
    textRect.setRight(contentRect.right() - closeButtonSize - m_gap);
    
    QPen pen = painter.pen();
    pen.setColor(m_foregroundColor);
    painter.setPen(pen);
    painter.drawText(textRect, fontMetrics().elidedText(text(), Qt::ElideRight, textRect.width()));

    if (m_badgeVisible) {
        painter.setBrush(Qt::red);
        painter.setPen(Qt::NoPen);

        QRect badgeRect{QPoint(0, 0), m_badgeSize};
        badgeRect.moveCenter(textRect.topRight());
        badgeRect.moveLeft(textRect.left() + fontMetrics().horizontalAdvance(text()));

        painter.drawEllipse(badgeRect);
    }
}

void NauDockWidgetTab::updateUi()
{
    if (m_backgroundFlasher && m_foregroundFlasher && !isActiveTab()) {
        m_backgroundBrush = m_backgroundFlasher->currentValue().value<QColor>();
        m_foregroundColor = m_foregroundFlasher->currentValue().value<QColor>();

    } else {
        m_backgroundBrush = isActiveTab()
            ? m_palette.brush(NauPalette::Role::BackgroundHeader, {}, NauPalette::Category::Active)
            : m_palette.brush(NauPalette::Role::BackgroundHeader, {}, NauPalette::Category::Inactive);

        m_foregroundColor = isActiveTab()
            ? m_palette.color(NauPalette::Role::ForegroundHeader, {}, NauPalette::Category::Active)
            : m_palette.color(NauPalette::Role::ForegroundHeader, {}, NauPalette::Category::Inactive);
    }

	update();
}

void NauDockWidgetTab::resetFlashing()
{
    m_foregroundFlasher.reset();
    m_backgroundFlasher.reset();
}

std::unique_ptr<QVariantAnimation> NauDockWidgetTab::engageColorAnimator(const NauColor& fromColor, const NauColor& toColor)
{
    auto animation = std::make_unique<QVariantAnimation>();
    animation->setDuration(m_durationMsecs);
    animation->setStartValue(fromColor);
    animation->setEndValue(toColor);
    animation->start();

    connect(animation.get(), &QVariantAnimation::valueChanged, this, &NauDockWidgetTab::updateUi);
    connect(animation.get(), &QVariantAnimation::finished, [this, animation = animation.get()] {
        animation->setDirection(animation->direction() == QAbstractAnimation::Forward
            ? QAbstractAnimation::Backward
            : QAbstractAnimation::Forward);
        animation->start();
    });

    return std::move(animation);
}

void NauDockWidgetTab::contextMenuEvent(QContextMenuEvent* event)
{
    event->accept();

    NauMenu menu(this);
    if (!dockAreaWidget()->isTopLevelArea()) {
        menu.addAction(tr("Undock Tab"), this, SLOT(detachDockWidget()));
        menu.addSeparator();
    }
    
    menu.addAction(tr("Close Tab"), this, SIGNAL(closeRequested()));
    if (dockAreaWidget()->openDockWidgetsCount() > 1) {
        menu.addAction(tr("Close Other Tabs"), this, &CDockWidgetTab::closeOtherTabsRequested);
    }

    menu.exec(event->globalPos());
}


// ** NauAdsComponentsFactory

ads::CDockAreaTitleBar* NauAdsComponentsFactory::createDockAreaTitleBar(ads::CDockAreaWidget* dockArea) const
{
    return new NauDockAreaTitleBar(dockArea);
}

ads::CDockAreaTabBar* NauAdsComponentsFactory::createDockAreaTabBar(ads::CDockAreaWidget* dockArea) const
{
    return new NauDockAreaTabBar(dockArea);
}

ads::CDockWidgetTab* NauAdsComponentsFactory::createDockWidgetTab(ads::CDockWidget* dockWidget) const
{
    return new NauDockWidgetTab(dockWidget);
}
