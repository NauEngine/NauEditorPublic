// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_title_bar.hpp"
#include "nau_font_metrics.hpp"
#include "nau_log.hpp"
#include "themes/nau_theme.hpp"
#include "magic_enum/magic_enum.hpp"

#include <QPainter>


// ** NauTitleBarSystemButton

class NauTitleBar::NauTitleBarSystemButton : public QAbstractButton
{
public:
    NauTitleBarSystemButton(const NauIcon& icon, NauWidget* parent)
        : QAbstractButton(parent)
    {
        setIcon(icon);
        setFixedSize(Size, Size);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setRenderHint(QPainter::TextAntialiasing);
    
        const NauIcon::State state = isChecked() ? NauIcon::On : NauIcon::Off;
        if (!isEnabled()) {
            icon().paint(&painter, event->rect(), Qt::AlignCenter, QIcon::Disabled, state);
            return;
        }
        if (!m_hovered) {
            icon().paint(&painter, event->rect(), Qt::AlignCenter, QIcon::Normal, state);
            return;
        }

        icon().paint(&painter, event->rect(), Qt::AlignCenter,
            isDown() ? QIcon::Selected : QIcon::Active, state);
    }

    void enterEvent([[maybe_unused]] QEnterEvent* event) override
    {
        m_hovered = true;
        update();
    }

    void leaveEvent([[maybe_unused]] QEvent* event) override
    {
        m_hovered = false;
        update();
    }

    static constexpr int Size = 16;
    static constexpr int Gap = 8;

private:
    bool m_hovered = false;
};


// ** NauTitleBarMenuItem

NauTitleBarMenuItem::NauTitleBarMenuItem(const QString& name,
    const NauMenu* menu, NauWidget* parent)
    : NauWidget(parent)
    , m_name(name)
    , m_menu(menu)
{
    setObjectName("NauTitleBarMenuItem");
    setFixedHeight(NauTitleBar::Height - NauTitleBar::ResizeOffset);

    const NauFont itemFont = Nau::Theme::current().fontTitleBarMenuItem();
    const NauFontMetrics fm{itemFont};

    // Calculate width
    setFixedWidth(fm.horizontalAdvance(m_name) + VerticalPadding * 2);
    setFont(itemFont);

    // Menu
    connect(menu, &QMenu::aboutToHide, [this] {
        m_menuIsShown = false;
        m_showNextMenu = !this->rect().contains(mapFromGlobal(QCursor::pos()));  // Prevent menu from reappearing when the menu is shown and with click menu title to close it
        update();
    });
    connect(menu, &QMenu::aboutToShow, [this] {
        m_menuIsShown = true;
        update();
    });
}

void NauTitleBarMenuItem::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Base
    if (m_menuIsShown) {
        painter.fillRect(rect().marginsAdded(QMargins(0, NauTitleBar::ResizeOffset, 0, 0)), QColor(0x424242));
    }

    // Name
    painter.setPen(m_hover || m_menuIsShown ? Qt::white : QColor(128, 128, 128));
    QRect textRect = rect();
    textRect.moveTop(-NauTitleBar::ResizeOffset * 0.5);
    painter.drawText(textRect, Qt::AlignCenter, m_name);
}

void NauTitleBarMenuItem::enterEvent(QEnterEvent* event)
{
    m_hover = true;
    setCursor(Qt::ArrowCursor);
    update();
}

void NauTitleBarMenuItem::leaveEvent(QEvent* event)
{
    m_hover = false;
    update();
}

void NauTitleBarMenuItem::mousePressEvent(QMouseEvent* event)
{
}

void NauTitleBarMenuItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_showNextMenu) {
        emit eventPressed();
    } else {
        m_showNextMenu = true;
    }
}


// ** NauTitleBarMenu

NauTitleBarMenu::NauTitleBarMenu(NauShortcutHub* shortcutHub, NauWidget* parent)
    : NauWidget(parent)
    , m_menu(std::make_unique<NauMainMenu>(shortcutHub, nullptr))
{
    setObjectName("NauTitleBarMenu");
    auto layout = new NauLayoutHorizontal(this);
    int menuWidth = 0;
    for (auto& [_, menu] : m_menu->m_menuByItem) {
        const auto menuTitle = menu->title();
        auto menuWidget = new NauTitleBarMenuItem(menuTitle, menu, this);
        layout->addWidget(menuWidget);
        connect(menuWidget, &NauTitleBarMenuItem::eventPressed, [this, menu, menuWidget] {
            QPoint popupPos = menuWidget->pos();
            popupPos.setY(popupPos.y() + height());
            menu->popup(mapToGlobal(popupPos));
        });
        menuWidth += menuWidget->width();
    }
    setFixedWidth(menuWidth);
}

std::unique_ptr<NauMainMenu>& NauTitleBarMenu::menu()
{
    return m_menu;
}

void NauTitleBarMenu::enterEvent(QEnterEvent* event)
{
    setCursor(Qt::ArrowCursor);
    update();
}


// ** NauTitleBar

NauTitleBar::NauTitleBar(NauShortcutHub* shortcutHub, QWidget* parent)
    : NauWidget(parent)
    , m_buttonMinimize(new NauTitleBarSystemButton(Nau::Theme::current().iconMinimize(), this))
    , m_buttonClose(new NauTitleBarSystemButton(Nau::Theme::current().iconClose(), this))
    , m_buttonWindowState(new NauTitleBarSystemButton(Nau::Theme::current().iconWindowState(), this))
    , m_compilationState(new NauTitleBarCompilationState(this))
    , m_menu(new NauTitleBarMenu(shortcutHub, this))
    , m_logoIcon(Nau::Theme::current().iconTitleBarLogo())
    , m_palette(Nau::Theme::current().paletteTitleBar())
{
    setFont(Nau::Theme::current().fontTitleBarTitle());
    setFixedHeight(NauTitleBar::Height);
    setMouseTracking(true);
    m_buttonWindowState->setCheckable(true);

    // System buttons
    connect(m_buttonMinimize, &NauTitleBarSystemButton::clicked, this, &NauTitleBar::eventMinimize);
    connect(m_buttonWindowState, &NauTitleBarSystemButton::clicked, this, [this](bool maximized)
    {
        m_buttonWindowState->setChecked(!maximized);
        emit eventToggleWindowStateRequested(maximized);
    });
    connect(m_buttonClose, &NauTitleBarSystemButton::clicked, this, &NauTitleBar::eventClose);

    // Main menu
    m_menu->setFixedHeight(NauTitleBar::Height - NauTitleBar::ResizeOffset);
    m_menu->show();
    m_menu->raise();
}

std::unique_ptr<NauMainMenu>& NauTitleBar::menu()
{
    return m_menu->menu();
}

void NauTitleBar::updateTitle(const QString& projectName, const QString& sceneName)
{
    m_projectName = projectName + "/";
    m_sceneName = sceneName;
    m_sceneNameBoundingRect = fontMetrics().tightBoundingRect(m_sceneName);
    m_projectNameBoundingRect = fontMetrics().tightBoundingRect(m_projectName);

    update();
}

void NauTitleBar::setMaximized(bool maximized)
{
    m_buttonWindowState->setChecked(maximized);
}

bool NauTitleBar::maximized() const
{
    return m_buttonWindowState->isChecked();
}

bool NauTitleBar::moveRequestPending() const
{
    return m_windowManipulationState == WindowManipulationState::Moving;
}

void NauTitleBar::resetMoveAndResizeRequests()
{
    m_windowManipulationState = WindowManipulationState::None;
}

void NauTitleBar::setCompilationState(const NauSourceStateManifold& state)
{
    m_compilationState->setCompilationState(state);
}

void NauTitleBar::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Base
    painter.fillRect(rect(), m_palette.brush(NauPalette::Role::Background));

    // Logo
    const auto iconRect = QRect(0, 0, height(), height());
    m_logoIcon.paint(&painter, iconRect);

    // Scene name
    painter.setPen(m_palette.color(NauPalette::Role::Foreground));
    QRect rectSceneName = m_sceneNameBoundingRect;
    const int sceneNameX = width() - NauTitleBarSystemButton::Gap * 3 - NauTitleBarSystemButton::Size * 4 - 
        rectSceneName.width() - m_compilationState->width() - 2 * SourceStateSpace;

    rectSceneName.moveTo(sceneNameX, 0);
    rectSceneName.setHeight(height());
    painter.drawText(rectSceneName, Qt::AlignCenter, m_sceneName);

    // Project name
    QRect rectProjectName = m_projectNameBoundingRect;
    const int projectNameOffset = NauTitleBarSystemButton::Gap;
    const int projectNameX = sceneNameX - rectProjectName.width() - projectNameOffset;
    rectProjectName.moveTo(projectNameX, 0);
    rectProjectName.setHeight(height());
    painter.setPen(m_palette.color(NauPalette::Role::ForegroundBrightText));
    painter.drawText(rectProjectName, Qt::AlignCenter, m_projectName);
}

void NauTitleBar::enterEvent(QEnterEvent* event)
{
    update();
}

void NauTitleBar::resizeEvent(QResizeEvent* event)
{
    // Menu
    m_menu->move(height(), NauTitleBar::ResizeOffset);

    // System buttons
    const int hCenter = (height() - NauTitleBarSystemButton::Size) * 0.5;
    m_buttonClose    ->move(width() - NauTitleBarSystemButton::Gap * 1 - NauTitleBarSystemButton::Size * 1, hCenter);
    m_buttonWindowState ->move(width() - NauTitleBarSystemButton::Gap * 2 - NauTitleBarSystemButton::Size * 2, hCenter);
    m_buttonMinimize ->move(width() - NauTitleBarSystemButton::Gap * 3 - NauTitleBarSystemButton::Size * 3, hCenter);

    auto stateGeometry = m_compilationState->geometry();
    stateGeometry.moveCenter(rect().center());
    stateGeometry.moveRight(m_buttonMinimize->geometry().left() - NauTitleBarSystemButton::Gap - SourceStateSpace);
    m_compilationState->setGeometry(stateGeometry);

    update();
}

void NauTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->buttons() == Qt::MouseButton::LeftButton) {
        if (resizingAcceptable(event->pos())) {
            m_windowManipulationState = WindowManipulationState::Resizing;
        } else {
            m_windowManipulationState = WindowManipulationState::Moving;
        }
    }

    NauWidget::mousePressEvent(event);
}

void NauTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_windowManipulationState == WindowManipulationState::Resizing) {
        emit eventResizeWindowRequested();
    } else if (m_windowManipulationState == WindowManipulationState::Moving) {
        emit eventMoveWindowRequested();
    } else {
        setCursor(resizingAcceptable(event->pos()) ? Qt::SizeVerCursor : Qt::ArrowCursor);

        NauWidget::mouseMoveEvent(event);
    }
}

void NauTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    event->accept();
    emit eventToggleWindowStateRequested(!m_buttonWindowState->isChecked());
}

void NauTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    resetMoveAndResizeRequests();
    NauWidget::mouseReleaseEvent(event);
}

bool NauTitleBar::resizingAcceptable(const QPoint& point) const
{
    const bool onTopEdge = point.y() >= 0 && point.y() <= NauTitleBar::ResizeOffset;
    return onTopEdge && !maximized();
}


// ** NauTitleBarCompilationState

NauTitleBarCompilationState::NauTitleBarCompilationState(NauWidget* parent)
    : NauWidget(parent)
    , m_palette(Nau::Theme::current().paletteCompilationStateTitleBar())
    , m_stateIcons(Nau::Theme::current().iconsScriptsState())
    , m_text(tr("Code Status"))
    , m_contentMargins{8, 4, 8, 4}
    , m_iconSize{16, 16}
    , m_tooltip(new NauTitleBarTooltip(nullptr))
{
    setFixedSize(100, 24);
    setMouseTracking(true);
    hide();

    connect(m_tooltip, &NauTitleBarTooltip::eventClosed, [this]{
        m_hover = false;
        update();
    });
}

void NauTitleBarCompilationState::setCompilationState(const NauSourceStateManifold& stateData)
{
    m_icon = NauIcon();
    auto itIcon = m_stateIcons.find(stateData.state);
    if (itIcon != m_stateIcons.end()) {
        m_icon = itIcon->second;
    }

    QString title;
    QString details;
    if (Nau::fillCompilationStateInfo(stateData, title, details)) {
        m_tooltip->setContent(title, details);

        show();
        update();
    } else {
        NED_WARNING("Failed to fetch info about compilation state <{}> for title bar state widget", 
            magic_enum::enum_name<NauSourceState>(stateData.state));
        hide();
    }
}

void NauTitleBarCompilationState::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const NauPalette::State state = m_hover ? NauPalette::Hovered : NauPalette::Normal;
    const QIcon::Mode qtState = m_hover ? QIcon::Mode::Active: QIcon::Mode::Normal;

    painter.fillRect(event->rect(), m_palette.brush(NauPalette::Role::Background, state));

    painter.setBrush(m_palette.brush(NauPalette::Role::AlternateBackground, state));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(event->rect(), 12, 12);

    const QRect contentRect = event->rect() - m_contentMargins;
    QRect iconRect{QPoint(0, 0), m_iconSize};
    iconRect.moveTopRight(contentRect.topRight());
    m_icon.paint(&painter, iconRect, Qt::AlignCenter, qtState);

    painter.setPen(QPen{m_palette.color(NauPalette::Role::Foreground, state)});
    painter.drawStaticText(contentRect.topLeft(), m_text);
}

void NauTitleBarCompilationState::enterEvent(QEnterEvent* event)
{
    m_hover = true;
    updateTooltipUi();
    m_tooltip->show();

    NauWidget::enterEvent(event);
}

void NauTitleBarCompilationState::leaveEvent(QEvent* event)
{
    m_hover = false;
    m_tooltip->hide();

    NauWidget::leaveEvent(event);
}

void NauTitleBarCompilationState::mousePressEvent(QMouseEvent* event)
{
    NauWidget::mousePressEvent(event);
    event->accept();
}

void NauTitleBarCompilationState::mouseReleaseEvent(QMouseEvent* event)
{
    NauWidget::mouseReleaseEvent(event);
    event->accept();
}

void NauTitleBarCompilationState::updateTooltipUi()
{
    QRect toolTipGeometry = m_tooltip->geometry();
    toolTipGeometry.moveTopRight(parentWidget()->mapToGlobal(geometry().bottomRight()));

    m_tooltip->move(toolTipGeometry.topLeft() + QPoint(0, 8));
}


// ** NauTitleBarTooltip

NauTitleBarTooltip::NauTitleBarTooltip(QWidget* parent)
    : NauFrame(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setObjectName("TitleBarCompilationStateTooltip");
    setFixedWidth(240);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setStyleSheet("color:white;");

    auto layout = new NauLayoutVertical(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    m_title = new NauLabel(this);
    QFont fnt = Nau::Theme::current().fontTitleBarTitle();
    fnt.setPointSize(14);
    m_title->setFont(fnt);

    m_content = new NauLabel(this);
    m_content->setFont(Nau::Theme::current().fontTitleBarTitle());
    m_content->setOpenExternalLinks(true);
    m_content->setWordWrap(true);
    m_content->setScaledContents(true);
    m_content->setFixedWidth(208);
    m_content->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    layout->addWidget(m_title);
    layout->addWidget(m_content);

    NauPalette pal;
    pal.setBrush(NauPalette::Role::Background, Nau::Theme::current()
        .paletteCompilationStateTitleBar().brush(NauPalette::Role::BackgroundHeader));
    setPalette(pal);
}

void NauTitleBarTooltip::setContent(const QString& title, const QString& content)
{
    m_title->setText(title);
    m_content->setText(content);
}

void NauTitleBarTooltip::closeEvent(QCloseEvent* event)
{
    NauFrame::closeEvent(event);
    emit eventClosed();
}
