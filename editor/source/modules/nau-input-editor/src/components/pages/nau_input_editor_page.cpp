// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_input_editor_page.hpp"

#include "nau/nau_constants.hpp"
#include "nau_dock_manager.hpp"
#include "nau_log.hpp"
#include "baseWidgets/nau_static_text_label.hpp"
#include "baseWidgets/nau_widget.hpp"
#include "themes/nau_theme.hpp"

#include "magic_enum/magic_enum.hpp"
#include "nau_assert.hpp"

#include <QDirIterator>
#include <QDrag>
#include <QPropertyAnimation>
#include "scene/nau_world.hpp"


namespace InputUtils
{
    template <typename T>
    void setOrCreateAttribute(pxr::UsdPrim& prim, const std::string& attrName, const pxr::SdfValueTypeName& valueType, const T& value)
    {
        auto attr = prim.GetAttribute(pxr::TfToken(attrName));
        if (!attr) {
            attr = prim.CreateAttribute(pxr::TfToken(attrName), valueType);
        }
        attr.Set(value);
    }

    void updateBindName(const std::string& newName, pxr::UsdPrim& prim)
    {
        if (newName.empty()) {
            return;
        }

        InputUtils::setOrCreateAttribute(prim, "name", pxr::SdfValueTypeNames->String, newName);

        prim.GetStage()->Save();
    }

    template <typename T>
    bool isAttributeExistAndValid(const pxr::UsdPrim& prim, const std::string& attrName, T& value)
    {
        bool isSuccess = false;

        auto attr = prim.GetAttribute(pxr::TfToken(attrName));
        if (attr && attr.Get(&value)) {
            isSuccess = true;
        }

        return isSuccess;
    }
}


// ** NauInputEditorLineEdit

NauInputEditorLineEdit::NauInputEditorLineEdit(const QString& text, NauLayoutHorizontal& layout, NauSpoiler& parent)
    : NauLineEdit(&parent)
    , m_spoiler(&parent)
    , m_container(&layout)
    , m_expanded(false)
    , m_canTextEdit(false)
{
    constexpr int HEADER_SPACING = 8;

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setText(text);
    // TODO: can't configure it from the code
    setStyleSheet("background: #00000000; font-size: 14px; border: none;");

    m_container->setObjectName("headerContainer");
    m_container->clear();
    m_container->setSpacing(HEADER_SPACING);
    m_container->addWidget(this, 0, Qt::AlignVCenter);

    m_addButton = createButtonAt(-1, Nau::Theme::current().iconSpoilerAdd());
    m_closeButton = createButtonAt(-1, Nau::Theme::current().iconSpoilerClose());
    m_arrowButton = createButtonAt(0, Nau::Theme::current().iconSpoilerIndicator(), true);
    m_arrowButton->setCheckable(true);

    connect(m_spoiler, &NauSpoiler::eventStartedExpanding, [this](bool flag) {
        if (m_expanded != flag) {
            emit switchToggle();
        }
    });
    connect(this, &QLineEdit::editingFinished, [this] {
        emit eventBindNameChanged(this->text());
    });
    connect(m_addButton, &QObject::destroyed, [this]() { m_addButton = nullptr; });
    connect(m_closeButton, &QObject::destroyed, [this]() { m_closeButton = nullptr; });
    connect(m_arrowButton, &QObject::destroyed, [this]() { m_arrowButton = nullptr; });
}

NauMiscButton* NauInputEditorLineEdit::createButtonAt(int index, const NauIcon& icon, bool needNotifySpoiler)
{
    constexpr QSize TOOLTIP_SIZE{ 16, 16 };

    auto* button = new NauMiscButton(this);
    button->setIcon(icon);
    button->setMinimumSize(TOOLTIP_SIZE);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    if (needNotifySpoiler) {
        connect(button, &QPushButton::clicked, this, &NauInputEditorLineEdit::notifySpoiler);
    }

    m_container->insertWidget(index, button);

    return button;
}

void NauInputEditorLineEdit::setTextEdit(bool editFlag) noexcept
{
    m_canTextEdit = editFlag;
}

void NauInputEditorLineEdit::switchToggle()
{
    m_expanded = !m_expanded;
    m_arrowButton->setChecked(m_expanded);
}

bool NauInputEditorLineEdit::isOutlineDrawn() const
{
    const NauWidgetState state = stateListener().state();
    return m_canTextEdit && (state == NauWidgetState::Hover || state == NauWidgetState::Pressed);
}

void NauInputEditorLineEdit::mousePressEvent(QMouseEvent* event)
{
    if (isReadOnly()) {
        notifySpoiler();
    }
    NauLineEdit::mousePressEvent(event);
}

NauMiscButton* NauInputEditorLineEdit::arrowButton() const noexcept
{
    return m_arrowButton;
}

NauMiscButton* NauInputEditorLineEdit::addButton() const noexcept
{
    return m_addButton;
}

NauMiscButton* NauInputEditorLineEdit::closeButton() const noexcept
{
    return m_closeButton;
}

void NauInputEditorLineEdit::notifySpoiler()
{
    m_spoiler->handleToggleRequested(!m_expanded);
}

void NauInputEditorLineEdit::paintEvent(QPaintEvent* event)
{
    if (isOutlineDrawn()) {
        const NauAbstractTheme& theme = Nau::Theme::current();
        constexpr double BORDER_WIDTH = 1.0;
        constexpr double BORDER_ROUND = 2.0;
        constexpr double BORDER_OFFSET = BORDER_WIDTH * 0.5;
        const QPen pen{ theme.paletteInputSpoilerLineEdit().color(NauPalette::Role::Border, NauPalette::Hovered), BORDER_WIDTH };

        QPainterPath path;
        path.addRoundedRect(BORDER_OFFSET, BORDER_OFFSET, width() - BORDER_WIDTH, height() - BORDER_WIDTH, BORDER_ROUND, BORDER_ROUND);

        QPainter painter{ this };
        painter.setPen(pen);
        painter.drawPath(path);
    }

    QLineEdit::paintEvent(event);
}

void NauInputEditorLineEdit::enterEvent(QEnterEvent* event) {
    emit eventHoveredChanged(true);
}

void NauInputEditorLineEdit::leaveEvent(QEvent* event) {
    emit eventHoveredChanged(false);
}


// ** NauInputEditorHeaderContainer

NauInputEditorHeaderContainer::NauInputEditorHeaderContainer(const QString& text, NauSpoiler& parent)
    : NauWidget(&parent)
    , m_layout(new NauLayoutHorizontal)
    , m_lineEdit(new NauInputEditorLineEdit(text, *m_layout, parent))
{
    setLayout(m_layout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

NauInputEditorLineEdit* NauInputEditorHeaderContainer::lineEdit() const noexcept
{
    return m_lineEdit;
}


// ** NauInputEditorSpoiler

NauInputEditorSpoiler::NauInputEditorSpoiler(const QString& text, int duration, bool hasHeaderLine, NauWidget* parent)
    : NauSpoiler(text, duration, parent)
    , m_headerContainerWidget(new NauInputEditorHeaderContainer(text, *this))
{
    delete m_contentLayout;
    m_contentLayout = new NauInputEditorSpoilerContentLayout;

    m_headerLayout->clear();
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    m_headerLayout->setSpacing(0);
    m_headerLayout->addWidget(m_headerContainerWidget);

    m_contentArea->setStyleSheet("background: #00000000;");
    m_contentArea->setLayout(m_contentLayout);

    if (!hasHeaderLine) {
        delete m_headerLine;
        m_headerLine = nullptr;
    }
}

void NauInputEditorSpoiler::setText(const QString& text)
{
    if (m_headerContainerWidget && m_headerContainerWidget->lineEdit()) {
        m_headerContainerWidget->lineEdit()->setText(text);
    }
}


// ** NauSpoilerContentLayout

int NauInputEditorSpoilerContentLayout::calculateHeight() const noexcept
{
    const auto margins = contentsMargins();
    int height = spacing() * std::max(0, count() - 1) + margins.top() + margins.bottom();

    for (int index = 0, count = this->count(); index < count; ++index) {
        if (auto* widget = itemAt(index)->widget()) {
            height += widget->height();
        }
    }
    return height;
}

QSize NauInputEditorSpoilerContentLayout::sizeHint() const
{
    const int width = NauLayoutVertical::sizeHint().width();
    const int height = calculateHeight();
    return { width, height };
}


// ** NauInputEditorTab

class NauInputEditorTab : public NauWidget
{
public:
    NauInputEditorTab(NauInputEditorTabs& bar, int tabIndex, const QString& text)
        : NauWidget(&bar)
        , m_label(new NauStaticTextLabel(text, this))
        , m_bar(&bar)
        , m_index(tabIndex)
    {
        constexpr QPoint LABEL_POSITION{ 16, 12 };

        m_label->move(LABEL_POSITION);
        m_label->setFont(Nau::Theme::current().fontInputTab());
    }

    void paintEvent(QPaintEvent* event) override
    {
        const NauAbstractTheme& theme = Nau::Theme::current();
        const NauPalette palette = theme.paletteInputBindTab();
        const NauPalette::State state = m_bar->currentIndex() == m_index ? NauPalette::Selected : NauPalette::Normal;

        m_label->setColor(palette.color(NauPalette::Role::Text, state));
        updateTab(true);

        QPainter painter{ this };
        painter.fillRect(0, 0, width(), height(), palette.color(NauPalette::Role::Background, state));
    }

    void resizeEvent(QResizeEvent* event) override
    {
        updateTab(false);
        NauWidget::resizeEvent(event);
    }

private:
    void updateTab(bool needResize)
    {
        const int tabSize = m_bar->width() / m_bar->count();
        const int positionX = tabSize * m_index;
        move(positionX, 0);
        if (needResize) {
            resize(tabSize, m_bar->height());
        }
    }

private:
    NauStaticTextLabel* m_label;
    NauInputEditorTabs* m_bar;
    const int m_index;
};


// ** NauInputEditorTabs

NauInputEditorTabs::NauInputEditorTabs(QWidget* parent)
    : QTabBar(parent)
{
    setObjectName("NauIE_tabBar");
    setStyleSheet("QTabBar#NauIE_tabBar:tab{ min-height: 40px; border: none }");
}

QSize NauInputEditorTabs::minimumTabSizeHint(int index) const
{
    return tabSizeHint(index);
}

QSize NauInputEditorTabs::tabSizeHint(int index) const
{
    auto* parent = parentWidget();
    QSize tabSize = QTabBar::tabSizeHint(index);
    tabSize.setWidth(parent->width() / std::max(1, count()));
    return tabSize;
}


// ** NauInputEditorTabContent

NauInputEditorTabContent::NauInputEditorTabContent()
    : NauInputEditorSpoiler("", 0, false, nullptr)
{
    constexpr QMargins TAB_MARGINS{ 0, 8, 0, 8 };

    m_contentLayout->setContentsMargins(TAB_MARGINS);
    m_headerContainerWidget->hide();
}


// ** NauInputEditorTabWidget

NauInputEditorTabWidget::NauInputEditorTabWidget(QWidget* parent)
    : QTabWidget(parent)
    /// TODO: create custom QTabBar::paintEvent
    , m_signalsTab(new NauInputEditorTabContent())
    , m_modifiersTab(new NauInputEditorTabContent())
    , m_noSignalsLabelContainer(nullptr)
    , m_noModifiersLabelContainer(nullptr)
{
    updateNoSignalsLabel(Reason::Delete);
    updateNoModifiersLabel(Reason::Delete);

    setObjectName("NauIE_TabWidget");
    setUsesScrollButtons(false);
    setDocumentMode(false);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setLayout(new NauLayoutHorizontal);

    // Adding a listener to the resizing event
    setupTab(m_signalsTab.get());
    setupTab(m_modifiersTab.get());

    auto* tabBar = new NauInputEditorTabs(this);
    setTabBar(tabBar);

    auto createTab = [this, tabBar](NauInputEditorTabContent* tab, const QString& text) {
        const int index = addTab(tab, "");
        tabBar->setTabButton(index, QTabBar::LeftSide, new NauInputEditorTab(*tabBar, index, text));
    };
    
    createTab(m_signalsTab.get(), tr("Signals"));
    createTab(m_modifiersTab.get(), tr("Modifiers"));
}

void NauInputEditorTabWidget::addSignalView(NauInputEditorSignalView* view)
{
    addView(view, m_signalsTab.get(), m_noSignalsLabelContainer, tr("No Signals"));
}

void NauInputEditorTabWidget::deleteSignalView(const NauInputEditorSignalView* view)
{
    deleteView(view, m_signalsTab.get(), m_noSignalsLabelContainer, tr("No Signals"));
}

void NauInputEditorTabWidget::addModifierView(NauInputEditorModifierView* view)
{
    addView(view, m_modifiersTab.get(), m_noModifiersLabelContainer, tr("No Modifiers"));
}

void NauInputEditorTabWidget::deleteModifierView(const NauInputEditorModifierView* view)
{
    deleteView(view, m_modifiersTab.get(), m_noModifiersLabelContainer, tr("No Modifiers"));
}

void NauInputEditorTabWidget::setupTab(NauInputEditorTabContent* tab)
{
    tab->installEventFilter(parent());
    tab->setExpanded();
}

void NauInputEditorTabWidget::updateNoLabel(Reason reason, NauWidget*& labelContainer, NauInputEditorTabContent* tab, const QString& text)
{
    if (reason == Reason::Add && labelContainer) {
        hideAndDeleteLabel(labelContainer, tab);
    } else if (tab->userWidgetsCount() < 1) {
        showNoItemsLabel(labelContainer, tab, text);
    }
}

void NauInputEditorTabWidget::hideAndDeleteLabel(NauWidget*& labelContainer, NauInputEditorTabContent* tab)
{
    tab->removeWidget(labelContainer);
    labelContainer->hide();
    labelContainer->deleteLater();
    labelContainer = nullptr;
}

void NauInputEditorTabWidget::showNoItemsLabel(NauWidget*& labelContainer, NauInputEditorTabContent* tab, const QString& text)
{
    constexpr QPoint LABEL_POSITION{ 16, 12 };
    constexpr int LABEL_MIN_HEIGHT = 40;
    const NauAbstractTheme& theme = Nau::Theme::current();

    labelContainer = new NauWidget(nullptr);
    labelContainer->setFixedHeight(LABEL_MIN_HEIGHT);

    auto* label = new NauStaticTextLabel(text, labelContainer);
    label->setFont(theme.fontInputLabel());
    label->setColor(theme.paletteInputGeneric().color(NauPalette::Role::Text));
    label->move(LABEL_POSITION);

    tab->addWidget(labelContainer);
}

void NauInputEditorTabWidget::updateNoSignalsLabel(Reason reason)
{
    updateNoLabel(reason, m_noSignalsLabelContainer, m_signalsTab.get(), tr("No Signals"));
}

void NauInputEditorTabWidget::updateNoModifiersLabel(Reason reason)
{
    updateNoLabel(reason, m_noModifiersLabelContainer, m_modifiersTab.get(), tr("No Modifiers"));
}

void NauInputEditorTabWidget::addView(NauInputEditorBaseView* view, NauInputEditorTabContent* tabContent, NauWidget*& labelContainer, const QString& noItemsText)
{
    // Adding a listener to the resizing event
    view->installEventFilter(tabContent);
    view->setExpanded();
    tabContent->addWidget(view);
    updateNoLabel(Reason::Add, labelContainer, tabContent, noItemsText);
}

void NauInputEditorTabWidget::deleteView(const NauInputEditorBaseView* view, NauInputEditorTabContent* tabContent, NauWidget*& labelContainer, const QString& noItemsText)
{
    tabContent->removeWidget(view);
    updateNoLabel(Reason::Delete, labelContainer, tabContent, noItemsText);
}


// ** NauInputEditorTabWidgetContainer

NauInputEditorTabWidgetContainer::NauInputEditorTabWidgetContainer(NauWidget* parent)
    : NauWidget(parent)
    , m_tabWidget(new NauInputEditorTabWidget(this))
{
    // Adding a listener to the resizing event
    installEventFilter(parent);
    connect(m_tabWidget, &QTabWidget::currentChanged, [this]() {
        updateWidgetSize(true);
    });
}

NauInputEditorTabWidget* NauInputEditorTabWidgetContainer::tabWidget() const noexcept
{
    return m_tabWidget;
}

void NauInputEditorTabWidgetContainer::updateWidgetSize(bool needResize)
{
    if (m_tabWidget->currentWidget() == nullptr) {
        return;
    }
    const int contentHeight = m_tabWidget->currentWidget()->height();
    const int tabHeight = m_tabWidget->tabBar()->height();
    const QSize newSize = QSize{ width(), tabHeight + contentHeight };
    if (needResize) {
        setMinimumHeight(newSize.height());
        resize(newSize);
    }
    m_tabWidget->setMinimumHeight(newSize.height());
    m_tabWidget->resize(newSize);
    m_tabWidget->tabBar()->resize(newSize.width(), tabHeight);
}

bool NauInputEditorTabWidgetContainer::eventFilter(QObject* tab, QEvent* event)
{
    if ((event->type() == QEvent::Resize) && (tab == m_tabWidget->currentWidget())) {
        updateWidgetSize(true);
    }
    return QObject::eventFilter(tab, event);
}

void NauInputEditorTabWidgetContainer::resizeEvent(QResizeEvent* event)
{
    updateWidgetSize(false);
    QWidget::resizeEvent(event);
}


// ** NauInputEditorPageHeader

NauInputEditorPageHeader::NauInputEditorPageHeader(const QString& title, const QString& subtitle, NauWidget* parent)
    : NauWidget(parent)
{
    constexpr int WIDGET_HEIGHT = 80;
    constexpr int OUTER_MARGIN = 16;
    constexpr int ICON_SIZE = 48;
    constexpr int TEXT_WIDTH = 600;
    constexpr int SPACING = 16;
    const NauAbstractTheme& theme = Nau::Theme::current();

    setFixedHeight(WIDGET_HEIGHT);

    auto* layout = new NauLayoutVertical(this);
    auto* layoutMain = new NauLayoutHorizontal();
    layoutMain->setContentsMargins(QMargins(OUTER_MARGIN, OUTER_MARGIN, OUTER_MARGIN, OUTER_MARGIN));
    layoutMain->setSpacing(SPACING);
    layout->addLayout(layoutMain);

    // Image
    auto* label = new QLabel(this);
    label->setPixmap(Nau::Theme::current().iconActionEditor().pixmap(ICON_SIZE));
    layoutMain->addWidget(label);

    // Text
    auto* container = new NauWidget(this);
    container->setMinimumHeight(ICON_SIZE);
    container->setMaximumHeight(ICON_SIZE);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layoutMain->addWidget(container);

    // Title
    m_title = new NauStaticTextLabel(title, container);
    m_title->setFont(theme.fontInputHeaderTitle());
    m_title->setColor(theme.paletteInputGeneric().color(NauPalette::Role::TextHeader));
    m_title->move(0, 8);
    m_title->setFixedWidth(TEXT_WIDTH);

    // Subtitle
    auto* labelSubtitle = new NauStaticTextLabel(subtitle, container);
    labelSubtitle->setFont(theme.fontInputHeaderSubtitle());
    labelSubtitle->setColor(theme.paletteInputGeneric().color(NauPalette::Role::Text));
    labelSubtitle->move(0, 34);
    labelSubtitle->setFixedWidth(TEXT_WIDTH);

    // Bottom separator
    auto* separator = new QFrame;
    separator->setStyleSheet("background-color: #141414;");
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    layout->addWidget(separator);
}

void NauInputEditorPageHeader::setTitle(const std::string& title)
{
    m_title->setText(title.c_str());
}


// ** NauInputEditorBaseView

constexpr NauInputEditorBaseView::FieldLayoutInfo::FieldLayoutInfo() noexcept
    : HEIGHT(32)
    , SPACING(16)
    , LABEL_PART(3)
    , VALUE_PART(5)
{}

NauInputEditorBaseView::NauInputEditorBaseView(const pxr::UsdPrim& prim, NauWidget* parent /*= nullptr*/)
    : NauInputEditorSpoiler(QString(), 0, false, parent)
    , m_prim(prim)
    , m_hovered(true)
{
    initializeWidgetFactory();

    constexpr QMargins VIEW_MARGINS{ 40, 12, 16, 12 };
    constexpr QMargins VIEW_HEADER_MARGINS{ 16, 4, 16, 4 };
    constexpr int VIEW_LINE_EDIT_HEIGHT = 32;
    constexpr int VIEW_CONTENT_SPACING = 12;
    constexpr int VIEW_HEADER_SPACING = 4;
    constexpr int VIEW_HEADER_HEIGHT = VIEW_LINE_EDIT_HEIGHT + VIEW_HEADER_MARGINS.top() + VIEW_HEADER_MARGINS.bottom();

    setAttribute(Qt::WA_Hover);

    m_contentLayout->setSpacing(VIEW_CONTENT_SPACING);
    m_contentLayout->setContentsMargins(VIEW_MARGINS);

    // Header replace
    m_headerContainerWidget->setMaximumHeight(VIEW_HEADER_HEIGHT);
    m_containerLayout = m_headerContainerWidget->layout();
    m_containerLayout->setContentsMargins(VIEW_HEADER_MARGINS);
    m_containerLayout->setSpacing(VIEW_HEADER_SPACING);
    m_bindNameWidget = m_headerContainerWidget->lineEdit();
    m_bindNameWidget->setMinimumHeight(VIEW_LINE_EDIT_HEIGHT);
    m_bindNameWidget->setMinimumHeight(VIEW_LINE_EDIT_HEIGHT);

    // Delete button setup
    auto* deleteButton = m_bindNameWidget->closeButton();
    connect(deleteButton, &QPushButton::clicked, [this] {
        emit eventViewDeleteRequire(this);
    });
    updateHover(false);

    // Hide Add button
    delete m_bindNameWidget->addButton();

    std::string name;
    if (InputUtils::isAttributeExistAndValid(m_prim, "name", name)) {
        setText(QString::fromStdString(name));
    }

    // Setup signal name editor
    auto* lineEdit = m_headerContainerWidget->lineEdit();
    lineEdit->setTextEdit(true);
    lineEdit->setText(QString::fromStdString(name));

    connect(lineEdit, &NauInputEditorLineEdit::eventBindNameChanged, [this, lineEdit](const QString& newName) {
        InputUtils::updateBindName(newName.toStdString(), m_prim);
        setText(newName);
    });

    connect(lineEdit, &NauInputEditorLineEdit::eventHoveredChanged, [this](bool /*isHovered*/) {
        update();
    });
}

pxr::UsdPrim NauInputEditorBaseView::prim() const noexcept
{
    return m_prim;
}

void NauInputEditorBaseView::updateHover(bool flag)
{
    if (m_hovered != flag) {
        m_headerContainerWidget->lineEdit()->closeButton()->setVisible(flag);
        m_hovered = flag;
    }
}

void NauInputEditorBaseView::addParamWidget(const QString& paramName, QWidget* widget)
{
    const NauAbstractTheme& theme = Nau::Theme::current();
    constexpr FieldLayoutInfo INFO;

    auto* containerWidget = new NauWidget(this);
    auto* layout = new NauLayoutHorizontal;
    containerWidget->setLayout(layout);
    containerWidget->setMinimumHeight(INFO.HEIGHT);
    containerWidget->setMaximumHeight(INFO.HEIGHT);

    auto* label = new NauStaticTextLabel(paramName, containerWidget);
    label->setColor(theme.paletteInputGeneric().color(NauPalette::Role::Text));
    layout->setSpacing(INFO.SPACING);
    layout->addWidget(label, INFO.LABEL_PART, Qt::AlignLeft | Qt::AlignVCenter);

    layout->addWidget(widget, INFO.VALUE_PART);
    addWidget(containerWidget);
    m_paramsWidgets.push_back(containerWidget);
}

bool NauInputEditorBaseView::event(QEvent* event)
{
    if ((event->type() == QEvent::HoverEnter) || (event->type() == QEvent::HoverLeave)) {
        updateHover(event->type() == QEvent::HoverEnter);
    }
    return NauSpoiler::event(event);
}

void NauInputEditorBaseView::paintEvent(QPaintEvent* event)
{
    // We only draw the border. This class is a container, so the rest is drawn in child widgets.
    auto* lineEdit = m_headerContainerWidget->lineEdit();
    if (m_hovered && !m_expanded && !lineEdit->isOutlineDrawn()) {
        const NauAbstractTheme& theme = Nau::Theme::current();
        constexpr double BORDER_WIDTH = 1.0;
        constexpr double BORDER_OFFSET = BORDER_WIDTH * 0.5;
        const QPen pen{ theme.paletteInputSignalView().color(NauPalette::Role::Border, NauPalette::Hovered), BORDER_WIDTH };

        QPainterPath path;
        path.addRect(BORDER_OFFSET, BORDER_OFFSET, width() - BORDER_WIDTH, height() - BORDER_WIDTH);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(pen);
        painter.drawPath(path);
    }
}

void NauInputEditorBaseView::initializeWidgetFactory()
{
    m_widgetFactory["Delay"] = [=](const pxr::UsdPrim& prim) {
        float pressingDelay = 0.0f;
        auto pressingDelayAttr = prim.GetAttribute(pxr::TfToken("pressingDelay"));
        if (pressingDelayAttr && pressingDelayAttr.Get(&pressingDelay)) {
            auto* pressingDelayWidget = new NauDoubleSpinBox();
            pressingDelayWidget->setRange(0, std::numeric_limits<float>::max());
            pressingDelayWidget->setValue(pressingDelay);

            QObject::connect(pressingDelayWidget, &NauDoubleSpinBox::valueChanged, [=](float value) {
                auto pressingDelayAttr = prim.GetAttribute(pxr::TfToken("pressingDelay"));
                pressingDelayAttr.Set(value);
                prim.GetStage()->Save();
                emit eventBindUpdateRequired();
            });

            addParamWidget("Pressing Delay", pressingDelayWidget);
        }
    };

    m_widgetFactory["Multiple"] = [=](const pxr::UsdPrim& prim) {
        int pressingCount = 0;
        auto pressingCountAttr = prim.GetAttribute(pxr::TfToken("pressingCount"));
        if (pressingCountAttr && pressingCountAttr.Get(&pressingCount)) {
            auto* pressingCountWidget = new NauSpinBox();
            pressingCountWidget->setRange(0, std::numeric_limits<int>::max());
            pressingCountWidget->setValue(pressingCount);

            QObject::connect(pressingCountWidget, &NauSpinBox::valueChanged, [=](int value) {
                auto pressingCountAttr = prim.GetAttribute(pxr::TfToken("pressingCount"));
                pressingCountAttr.Set(value);
                prim.GetStage()->Save();
                emit eventBindUpdateRequired();
            });

            addParamWidget("Pressing Count", pressingCountWidget);
        }

        float pressingInterval = 0.0f;
        auto pressingIntervalAttr = prim.GetAttribute(pxr::TfToken("pressingInterval"));
        if (pressingIntervalAttr && pressingIntervalAttr.Get(&pressingInterval)) {
            auto* pressingIntervalWidget = new NauDoubleSpinBox();
            pressingIntervalWidget->setRange(0, std::numeric_limits<float>::max());
            pressingIntervalWidget->setValue(pressingInterval);

            QObject::connect(pressingIntervalWidget, &NauDoubleSpinBox::valueChanged, [=](float value) {
                auto pressingIntervalAttr = prim.GetAttribute(pxr::TfToken("pressingInterval"));
                pressingIntervalAttr.Set(value);
                prim.GetStage()->Save();
                emit eventBindUpdateRequired();
            });

            addParamWidget("Pressing Delay", pressingIntervalWidget);
        }
        
        return nullptr;
    };

    m_widgetFactory["Value"] = [=](const pxr::UsdPrim& prim) {
        float value = 0.0f;
        auto valueAttr = prim.GetAttribute(pxr::TfToken("value"));
        if (valueAttr && valueAttr.Get(&value)) {
            auto* widget = new NauDoubleSpinBox();
            widget->setRange(0, std::numeric_limits<float>::max());
            widget->setValue(value);

            QObject::connect(widget, &NauDoubleSpinBox::valueChanged, [=](float newValue) {
                auto valueAttr = prim.GetAttribute(pxr::TfToken("value"));
                valueAttr.Set(newValue);
                prim.GetStage()->Save();
                emit eventBindUpdateRequired();
            });

            addParamWidget("Value", widget);
        }
        return nullptr;
    };
}


// ** NauInputEditorSignalView

NauInputEditorSignalView::NauInputEditorSignalView(const pxr::UsdPrim& signalPrim, NauInputBindType bindType, NauWidget* parent)
    : NauInputEditorBaseView(signalPrim, parent)
{
    m_inputSystem = &nau::getServiceProvider().get<nau::IInputSystem>();

    setupDigitalBindings();

    auto* keyBox = createSignalInfo(tr("Source"), magic_enum::enum_names<NauInputTriggerCondition>());
    setupSourceKey(signalPrim, keyBox);
}

NauIcon NauInputEditorSignalView::iconByDeviceName(const QString& name) noexcept
{
    static std::unordered_map<NauInputDevice, NauIcon> icons{
        { NauInputDevice::Unknown, Nau::Theme::current().iconUnknown() },
        { NauInputDevice::Mouse, Nau::Theme::current().iconMouse() },
        { NauInputDevice::Keyboard, Nau::Theme::current().iconKeyboard() },
    };

    auto deviceValue = magic_enum::enum_cast<NauInputDevice>(name.toUtf8().data());
    NauInputDevice device = deviceValue ? deviceValue.value() : NauInputDevice::Unknown;
    return icons[device];
}

void NauInputEditorSignalView::updateParamsWidgets(const QString& conditionName)
{
    const NauAbstractTheme& theme = Nau::Theme::current();
    constexpr FieldLayoutInfo INFO;

    for (auto* widget : m_paramsWidgets) {
        removeWidget(widget);
        widget->hide();
        widget->deleteLater();
    }

    m_paramsWidgets.clear();

    auto it = m_widgetFactory.find(conditionName);
    if (it != m_widgetFactory.end()) {
        it->second(m_prim);
    }
}

NauComboBox* NauInputEditorSignalView::createSignalInfo(const QString& name, const auto& values)
{
    constexpr FieldLayoutInfo INFO;

    auto* widget = new NauWidget(this);
    widget->setMinimumHeight(INFO.HEIGHT);
    widget->setMaximumHeight(INFO.HEIGHT);
    addWidget(widget);

    auto* layout = new NauLayoutHorizontal;
    layout->setSpacing(INFO.SPACING);
    widget->setLayout(layout);

    const NauAbstractTheme& theme = Nau::Theme::current();
    auto* label = new NauStaticTextLabel(name, widget);
    label->setFont(theme.fontInputLabel());
    label->setColor(theme.paletteInputGeneric().color(NauPalette::Role::Text));
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(label, INFO.LABEL_PART, Qt::AlignLeft | Qt::AlignVCenter);

    auto* box = new NauComboBox(widget);
    box->setObjectName("NauIE_signalBox");
    box->setStyleSheet("QComboBox#NauIE_signalBox { background-color: #222222; }"
        "QComboBox#NauIE_signalBox::down-arrow { image: url(:/inputEditor/icons/inputEditor/vArrowDown.svg); }");
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(box, INFO.VALUE_PART);

    for (const auto& value : values) {
        box->addItem(value.data());
    }
    return box;
}

void NauInputEditorSignalView::createButtonsInfo(const QString& deviceName, NauComboBox* keyComboBox)
{
    if (!m_inputSystem) {
        return;
    }

    keyComboBox->clear();
    for (const auto& device : m_inputSystem->getDevices()) {
        auto allowedDevices = device->getName();
        if (strcmp(allowedDevices.c_str(), deviceName.toLower().toStdString().c_str()) != 0) {
            continue;
        }

        const int num = device->getKeysNum();
        for (unsigned i = 0; i < num; ++i) {
            if (!device->getKeyName(i).empty()) {
                keyComboBox->addItem(device->getKeyName(i).c_str());
            }
        }
    }
}

void NauInputEditorSignalView::setupComboBoxFromPrim(NauComboBox* comboBox, const pxr::TfToken& attrToken)
{
    std::string currentValue;
    if (InputUtils::isAttributeExistAndValid(m_prim, attrToken, currentValue)) {
        comboBox->setCurrentText(QString::fromStdString(currentValue));
    }

    connect(comboBox, &QComboBox::currentTextChanged, [this, attrToken](const QString& newValue) {
        auto attr = m_prim.GetAttribute(attrToken);
        attr.Set(newValue.toStdString());
        m_prim.GetStage()->Save();
        emit eventBindUpdateRequired();
    });
}

void NauInputEditorSignalView::setupSourceKey(const pxr::UsdPrim& signalPrim, NauComboBox* keyBox)
{
    std::string deviceName;
    if (InputUtils::isAttributeExistAndValid(signalPrim, "device", deviceName)) {
        createButtonsInfo(QString::fromStdString(deviceName).toLower(), keyBox);
    }

    std::string source;
    if (InputUtils::isAttributeExistAndValid(signalPrim, "source", source) && source.empty()) {
        InputUtils::setOrCreateAttribute(m_prim, "source", pxr::SdfValueTypeNames->String, keyBox->currentText().toStdString());
    }

    setupComboBoxFromPrim(keyBox, pxr::TfToken("source"));
}

void NauInputEditorSignalView::setupDigitalBindings()
{
    auto* conditionBox = createSignalInfo(tr("Type"), magic_enum::enum_names<NauInputTriggerCondition>());
    setupComboBoxFromPrim(conditionBox, pxr::TfToken("condition"));
    connect(conditionBox, &QComboBox::currentTextChanged, this, &NauInputEditorSignalView::updateParamsWidgets);
    updateParamsWidgets(conditionBox->currentText());

    auto* triggerBox = createSignalInfo(tr("Event"), magic_enum::enum_names<NauInputTrigger>());
    setupComboBoxFromPrim(triggerBox, pxr::TfToken("trigger"));
}

void NauInputEditorSignalView::setupAnotherBindings()
{
    auto* axisBox = createSignalInfo(tr("Axis"), magic_enum::enum_names<NauInputAxis>());
    setupComboBoxFromPrim(axisBox, pxr::TfToken("axis"));

    auto* typeBox = createSignalInfo(tr("Type"), magic_enum::enum_names<NauInputSignalType>());
    setupComboBoxFromPrim(typeBox, pxr::TfToken("signalType"));
}


// ** NauInputEditorModifierView

NauInputEditorModifierView::NauInputEditorModifierView(const pxr::UsdPrim& modifierPrim, NauWidget* parent /*= nullptr*/)
    : NauInputEditorBaseView(modifierPrim, parent)
{
    auto it = m_widgetFactory.find("Value");
    if (it != m_widgetFactory.end()) {
        it->second(m_prim);
    }
}


// ** NauInputEditorBindView

NauInputEditorBindView::NauInputEditorBindView(const pxr::UsdPrim& bindPrim, NauWidget* parent)
    : NauInputEditorSpoiler(QString(), 0, false, parent)
    , m_bindPrim(bindPrim)
    , m_tabWidget(new NauInputEditorTabWidgetContainer(this))
    , m_signalsCount(0)
{
    std::string bindName;
    if (InputUtils::isAttributeExistAndValid(m_bindPrim, "name", bindName)) {
        setText(QString::fromStdString(bindName));
    }

    std::string bindType;
    if (InputUtils::isAttributeExistAndValid(m_bindPrim, "type", bindType)) {
        m_bindType = magic_enum::enum_cast<NauInputBindType>(bindType).value();
        m_signalsCount = signalsCount(bindType);
    }

    constexpr QMargins BIND_VIEW_CONTENT_MARGINS{ 16, 0, 16, 0 };
    constexpr QMargins BIND_VIEW_HEADER_MARGINS{ 16, 12, 16, 12 };
    constexpr int BIND_VIEW_LINE_EDIT_HEIGHT = 32;
    constexpr int BIND_VIEW_HEADER_SPACING = 4;
    constexpr int BIND_VIEW_HEADER_HEIGHT = BIND_VIEW_LINE_EDIT_HEIGHT + BIND_VIEW_HEADER_MARGINS.top() + BIND_VIEW_HEADER_MARGINS.bottom();

    m_headerLayout->setContentsMargins(BIND_VIEW_HEADER_MARGINS);
    m_headerLayout->setSpacing(BIND_VIEW_HEADER_SPACING);
    m_contentLayout->setContentsMargins(BIND_VIEW_CONTENT_MARGINS);

    m_headerContainerWidget->setMaximumHeight(BIND_VIEW_HEADER_HEIGHT);
    auto* lineEdit = m_headerContainerWidget->lineEdit();
    lineEdit->setMinimumHeight(BIND_VIEW_LINE_EDIT_HEIGHT);
    lineEdit->setMaximumHeight(BIND_VIEW_LINE_EDIT_HEIGHT);
    lineEdit->setTextEdit(true);

    // Delete button setup
    auto* deleteButton = lineEdit->closeButton();
    connect(deleteButton, &QAbstractButton::clicked, [this] {
        emit eventDeleteBindRequested();
    });

    // Add button setup
    auto* addButton = lineEdit->addButton();
    auto* menu = new NauMenu;
    setupSignalActions(menu);
    addButton->setMenu(menu);

    // Adding a listener to the resizing event
    installEventFilter(parent);
    setExpanded();

    processRelationship(pxr::TfToken("signalArray"), [this](const pxr::UsdPrim& signalPrim) {
        addSignalWidget(signalPrim);
    });

    processRelationship(pxr::TfToken("modifierArray"), [this](const pxr::UsdPrim& modifierPrim) {
        addModifierWidget(modifierPrim);
    });

    connect(lineEdit, &NauInputEditorLineEdit::eventBindNameChanged, [this, lineEdit](const QString& newName) {
        InputUtils::updateBindName(newName.toStdString(), m_bindPrim);
        setText(newName);
    });

    connect(m_tabWidget->tabWidget(), &NauInputEditorTabWidget::currentChanged, [this, addButton, menu](int index) {
        menu->clear();

        if (index == 0) { // Signals tab
            setupSignalActions(menu);
        } else if (index == 1) { // Modifiers tab
            setupModifierActions(menu);
        }

        addButton->setMenu(menu);
    });

    // Setup tabs
    addWidget(m_tabWidget);
}

pxr::UsdPrim NauInputEditorBindView::bindPrim() const noexcept
{
    return m_bindPrim;
}

void NauInputEditorBindView::addSignal(const QString& deviceName)
{
    if (!m_bindPrim || isSignalLimitReached()) {
        return;
    }

    int signalId = NauObjectGUID(QDateTime::currentSecsSinceEpoch());
    auto signalPath = pxr::SdfPath("/Signals/Signal_" + std::to_string(signalId));

    auto stage = m_bindPrim.GetStage();
    if (!stage->GetPrimAtPath(signalPath)) {
        auto signalPrim = stage->DefinePrim(signalPath, pxr::TfToken("Signal"));
        
        setupSignalAttributes(signalPrim, signalId, deviceName);
        addPrimToRelationship(signalPrim, "signalArray");

        addSignalWidget(signalPrim);
        stage->Save();
        emit eventBindUpdateRequested();
    }
}

void NauInputEditorBindView::addModifier(const QString& modifierName)
{
    if (!m_bindPrim) {
        return;
    }

    int modifierId = NauObjectGUID(QDateTime::currentSecsSinceEpoch());
    auto modifierPath = pxr::SdfPath("/Modifiers/Modifier_" + std::to_string(modifierId));

    auto stage = m_bindPrim.GetStage();
    if (!stage->GetPrimAtPath(modifierPath)) {
        auto modifierPrim = stage->DefinePrim(modifierPath, pxr::TfToken("Modifier"));

        setupModifierAttributes(modifierPrim, modifierId, modifierName);
        addPrimToRelationship(modifierPrim, "modifierArray");

        addModifierWidget(modifierPrim);
        stage->Save();
        emit eventBindUpdateRequested();
    }
}

int NauInputEditorBindView::signalsCount(const std::string& bindType) const noexcept
{
    const auto typeValue = magic_enum::enum_cast<NauInputBindType>(bindType);

    if (typeValue.has_value()) {
        switch (typeValue.value()) {
        case NauInputBindType::Digital:
        case NauInputBindType::Analog:
            return 1;
        case NauInputBindType::Axis:
            return 2;
        }
    }

    return 0;
}

bool NauInputEditorBindView::isSignalLimitReached() const
{
    auto signalArrayRel = m_bindPrim.GetRelationship(pxr::TfToken("signalArray"));
    pxr::SdfPathVector signalPaths;

    if (signalArrayRel) {
        signalArrayRel.GetTargets(&signalPaths);
    }

    return signalPaths.size() >= m_signalsCount;
}

void NauInputEditorBindView::setupSignalActions(NauMenu* menu)
{
    for (const auto& device : magic_enum::enum_names<NauInputDevice>()) {
        QString deviceName{ device.data() };
        const auto* action = menu->addAction(NauInputEditorSignalView::iconByDeviceName(deviceName), deviceName);
        connect(action, &QAction::triggered, [this, name = std::move(deviceName)]() {
            addSignal(name);
        });
    }
}

void NauInputEditorBindView::setupModifierActions(NauMenu* menu)
{
    for (const auto& modifier : magic_enum::enum_names<NauInputModifier>()) {
        QString modifierName{ modifier.data() };
        const auto* action = menu->addAction(modifierName);
        connect(action, &QAction::triggered, [this, name = std::move(modifierName)]() {
            addModifier(name);
        });
    }
}

void NauInputEditorBindView::addView(NauInputEditorBaseView* view, const std::string& arrayToken, std::function<void(NauInputEditorTabWidget*, NauInputEditorBaseView*)> addFunc, std::function<void(NauInputEditorTabWidget*, const NauInputEditorBaseView*)> deleteFunc)
{
    connect(view, &NauInputEditorBaseView::eventViewDeleteRequire, [this, view, arrayToken, deleteFunc]() {
        auto viewPath = view->prim().GetPath();

        auto arrayRel = m_bindPrim.GetRelationship(pxr::TfToken(arrayToken));
        if (arrayRel) {
            arrayRel.RemoveTarget(viewPath);
        }

        m_bindPrim.GetStage()->RemovePrim(viewPath);
        m_bindPrim.GetStage()->Save();
        emit eventBindUpdateRequested();

        deleteFunc(m_tabWidget->tabWidget(), view);
        view->hide();
        view->deleteLater();
    });

    connect(view, &NauInputEditorBaseView::eventBindUpdateRequired, [this]() {
        emit eventBindUpdateRequested();
    });

    addFunc(m_tabWidget->tabWidget(), view);
}

void NauInputEditorBindView::addSignalWidget(const pxr::UsdPrim& signalPrim)
{
    auto* signalView = new NauInputEditorSignalView(signalPrim, m_bindType, this);
    
    addView(signalView, "signalArray",
        [](NauInputEditorTabWidget* tabWidget, NauInputEditorBaseView* view) {
            tabWidget->addSignalView(static_cast<NauInputEditorSignalView*>(view));
        },
        [](NauInputEditorTabWidget* tabWidget, const NauInputEditorBaseView* view) {
            tabWidget->deleteSignalView(static_cast<const NauInputEditorSignalView*>(view));
        });
}

void NauInputEditorBindView::addModifierWidget(const pxr::UsdPrim& modifierPrim)
{
    auto* modifierView = new NauInputEditorModifierView(modifierPrim, this);

    addView(modifierView, "modifierArray",
        [](NauInputEditorTabWidget* tabWidget, NauInputEditorBaseView* view) {
            tabWidget->addModifierView(static_cast<NauInputEditorModifierView*>(view));
        },
        [](NauInputEditorTabWidget* tabWidget, const NauInputEditorBaseView* view) {
            tabWidget->deleteModifierView(static_cast<const NauInputEditorModifierView*>(view));
        });
}

void NauInputEditorBindView::processRelationship(const pxr::TfToken& relationshipToken, const std::function<void(const pxr::UsdPrim&)>& addWidgetCallback)
{
    auto arrayRel = m_bindPrim.GetRelationship(relationshipToken);
    if (!arrayRel) {
        return;
    }

    std::vector<pxr::SdfPath> paths;
    arrayRel.GetTargets(&paths);

    auto stage = m_bindPrim.GetStage();
    for (const auto& path : paths) {
        auto prim = stage->GetPrimAtPath(path);
        if (prim) {
            addWidgetCallback(prim);
        }
    }
}

void NauInputEditorBindView::setupSignalAttributes(pxr::UsdPrim& prim, int signalId, const QString& deviceName)
{
    InputUtils::setOrCreateAttribute(prim, "id", pxr::SdfValueTypeNames->Int, signalId);
    InputUtils::setOrCreateAttribute(prim, "device", pxr::SdfValueTypeNames->String, deviceName.toStdString());
    InputUtils::setOrCreateAttribute(prim, "name", pxr::SdfValueTypeNames->String, deviceName.toStdString());
    InputUtils::setOrCreateAttribute(prim, "condition", pxr::SdfValueTypeNames->String, magic_enum::enum_name(NauInputTriggerCondition::Single).data());
    InputUtils::setOrCreateAttribute(prim, "trigger", pxr::SdfValueTypeNames->String, magic_enum::enum_name(NauInputTrigger::Pressed).data());
    InputUtils::setOrCreateAttribute(prim, "source", pxr::SdfValueTypeNames->String, std::string());
    InputUtils::setOrCreateAttribute(prim, "axis", pxr::SdfValueTypeNames->String, magic_enum::enum_name(NauInputAxis::AxisX).data());
    InputUtils::setOrCreateAttribute(prim, "signalType", pxr::SdfValueTypeNames->String, magic_enum::enum_name(NauInputSignalType::Positive).data());
    InputUtils::setOrCreateAttribute(prim, "pressingCount", pxr::SdfValueTypeNames->Int, 0);
    InputUtils::setOrCreateAttribute(prim, "pressingDelay", pxr::SdfValueTypeNames->Float, 0.0f);
    InputUtils::setOrCreateAttribute(prim, "pressingInterval", pxr::SdfValueTypeNames->Float, 0.0f);
}

void NauInputEditorBindView::setupModifierAttributes(pxr::UsdPrim& prim, int modifierId, const QString& modifierName)
{
    InputUtils::setOrCreateAttribute(prim, "id", pxr::SdfValueTypeNames->Int, modifierId);
    InputUtils::setOrCreateAttribute(prim, "name", pxr::SdfValueTypeNames->String, modifierName.toStdString());
    InputUtils::setOrCreateAttribute(prim, "type", pxr::SdfValueTypeNames->String, modifierName.toStdString());
    InputUtils::setOrCreateAttribute(prim, "value", pxr::SdfValueTypeNames->Float, 0.0f);
}

void NauInputEditorBindView::addPrimToRelationship(const pxr::UsdPrim& prim, const std::string& relationshipName)
{
    auto rel = m_bindPrim.GetRelationship(pxr::TfToken(relationshipName));
    
    if (!rel) {
        rel = m_bindPrim.CreateRelationship(pxr::TfToken(relationshipName), /* custom = */ false);
    }

    rel.AddTarget(prim.GetPath());
}


// ** NauInputEditorBindListView

NauInputEditorBindListView::NauInputEditorBindListView(NauWidget* parent)
    : NauInputEditorSpoiler(tr("Binds"), 0, true, parent)
    , m_comboBox(new NauComboBox(this))
{
    constexpr QMargins BINDS_LIST_CONTENT_MARGINS{ 0, 0, 0, 16 };
    constexpr QMargins BINDS_LIST_HEADER_MARGINS{ 16, 16, 16, 16 };
    constexpr int BINDS_LIST_LINE_EDIT_HEIGHT = 24;
    constexpr int BINDS_LIST_HEADER_SPACING = 8;
    constexpr int BINDS_LIST_HEADER_HEIGHT = BINDS_LIST_LINE_EDIT_HEIGHT + BINDS_LIST_HEADER_MARGINS.top() + BINDS_LIST_HEADER_MARGINS.bottom();

    lineEdit()->setStyleSheet("background: #00000000; font-size: 14px; border: none;");
    lineEdit()->setMinimumHeight(BINDS_LIST_LINE_EDIT_HEIGHT);
    lineEdit()->setMaximumHeight(BINDS_LIST_LINE_EDIT_HEIGHT);
    lineEdit()->setReadOnly(true);

    m_contentLayout->setContentsMargins(BINDS_LIST_CONTENT_MARGINS);
    m_headerContainerWidget->setMaximumHeight(BINDS_LIST_HEADER_HEIGHT);

    // Header replace
    auto* containerLayout = m_headerContainerWidget->layout();
    containerLayout->setContentsMargins(BINDS_LIST_HEADER_MARGINS);
    containerLayout->setSpacing(BINDS_LIST_HEADER_SPACING);

    // Delete button setup
    auto* bindNameWidget = m_headerContainerWidget->lineEdit();
    delete bindNameWidget->closeButton();

    // Add button setup
    const auto* addButton = bindNameWidget->addButton();
    connect(addButton, &QAbstractButton::clicked, [this, addButton] {
        m_comboBox->move(addButton->pos());
        m_comboBox->showPopup();
    });

    // Combobox setup
    constexpr auto bindTypes = magic_enum::enum_names<NauInputBindType>();
    for (const auto& device : bindTypes) {
        m_comboBox->addItem(device.data());
    }
    m_comboBox->hide();
    connect(m_comboBox, &NauComboBox::textActivated, [this](const QString& text) {
        emit eventAddBindRequested(text);
    });
}

NauInputEditorLineEdit* NauInputEditorBindListView::lineEdit() const noexcept
{
    return m_headerContainerWidget->lineEdit();
}


// ** NauInputEditorPage

NauInputEditorPage::NauInputEditorPage(NauWidget* parent)
    : NauWidget(parent)
    , m_isAdded(false)
    , m_layout(new NauLayoutVertical(this))
    , m_processAssetButton(new NauPrimaryButton())
    , m_editorHeader(new NauInputEditorPageHeader("ActionFileName", tr("Action"), this))
    , m_bindsEditorHeader(new NauInputEditorBindListView(this))
    , m_noBindingsLabelContainer(nullptr)
{
    m_layout->addWidget(m_editorHeader, Qt::AlignTop);
    m_layout->addWidget(m_bindsEditorHeader, Qt::AlignTop);
    m_layout->addWidget(m_processAssetButton, Qt::AlignTop);
    m_layout->addStretch(1);
    m_bindsEditorHeader->setExpanded();

    connect(m_bindsEditorHeader, &NauInputEditorBindListView::eventAddBindRequested, [this](const QString& bindType) {
        addBind(bindType);
    });

    connect(m_processAssetButton, &NauPrimaryButton::clicked, this, [=]()
        {
            emit eventProcessAsset(m_isAdded);
        });
}

void NauInputEditorPage::setName(const std::string& actionName)
{
    m_editorHeader->setTitle(actionName);
}

void NauInputEditorPage::setStage(const pxr::UsdStagePtr& stage)
{
    m_bindsEditorHeader->setExpanded();

    for (auto* widget : m_bindsEditorHeader->removeWidgets()) {
        widget->hide();
        widget->deleteLater();
    }

    updateNoBindingsLabel();

    m_stage = stage;
    loadBindsFromStage();
}

void NauInputEditorPage::setIsAdded(bool isAdded)
{
    m_isAdded = isAdded;

    QString text = !m_isAdded ? QString(tr("Add Asset")) : QString(tr("Remove Asset"));
    m_processAssetButton->setText(text);
}

void NauInputEditorPage::loadBindsFromStage()
{
    auto rootPrim = m_stage->GetPrimAtPath(pxr::SdfPath("/Input"));
    if (!rootPrim) {
        NED_ERROR("Cannot find input prim");
        return;
    }

    auto bindArrayRel = rootPrim.GetRelationship(pxr::TfToken("bindArray"));
    if (!bindArrayRel) {
        NED_ERROR("Cannot find bindArray");
        return;
    }

    pxr::SdfPathVector bindPaths;
    bindArrayRel.GetTargets(&bindPaths);
    for (const auto& bindPath : bindPaths) {
        auto bindPrim = m_stage->GetPrimAtPath(bindPath);
        addBindWidget(bindPrim);
    }
}

void NauInputEditorPage::removeBindFromStage(const pxr::SdfPath& bindPath)
{
    m_stage->RemovePrim(bindPath);

    auto rootPrim = m_stage->GetPrimAtPath(pxr::SdfPath("/Input"));
    if (!rootPrim) {
        NED_ERROR("Cannot find root input prim");
        return;
    }

    auto bindArrayRel = rootPrim.GetRelationship(pxr::TfToken("bindArray"));
    if (bindArrayRel) {
        bindArrayRel.RemoveTarget(bindPath);
    }
}

void NauInputEditorPage::removeRelatedPrims(const pxr::UsdPrim& prim, const pxr::TfToken& relationshipToken)
{
    auto relationship = prim.GetRelationship(relationshipToken);
    if (!relationship) {
        NED_ERROR("Cannot find relationship");
        return;
    }

    pxr::SdfPathVector paths;
    relationship.GetTargets(&paths);

    for (const auto& path : paths) {
        if (m_stage->GetPrimAtPath(path)) {
            m_stage->RemovePrim(path);
        }
    }
}

void NauInputEditorPage::addBindWidget(const pxr::UsdPrim& bindPrim)
{
    auto* bindWidget = new NauInputEditorBindView(bindPrim, m_bindsEditorHeader);
    m_bindsEditorHeader->addWidget(bindWidget);
    updateNoBindingsLabel();

    connect(bindWidget, &NauInputEditorBindView::eventDeleteBindRequested, [this, bindWidget]() {
        m_bindsEditorHeader->removeWidget(bindWidget);

        if (m_stage) {
            removeRelatedPrims(bindWidget->bindPrim(), pxr::TfToken("signalArray"));
            removeRelatedPrims(bindWidget->bindPrim(), pxr::TfToken("modifierArray"));

            auto bindPath = bindWidget->bindPrim().GetPath();
            removeBindFromStage(bindPath);

            m_stage->Save();
            emit eventInputUpdateRequired();
        }

        bindWidget->hide();
        bindWidget->deleteLater();
        updateNoBindingsLabel();
        });

    connect(bindWidget, &NauInputEditorBindView::eventBindUpdateRequested, [this]() {
        emit eventInputUpdateRequired();
        });
}

void NauInputEditorPage::updateNoBindingsLabel()
{
    if (m_noBindingsLabelContainer == nullptr && m_bindsEditorHeader->userWidgetsCount() == 0) {
        constexpr QPoint LABEL_POSITION{ 32, 12 };
        constexpr int LABEL_HEIGHT = 44;
        constexpr int LABEL_FONT_SIZE = 14;
        const NauAbstractTheme& theme = Nau::Theme::current();

        NauFont font = theme.fontInputLabel();
        font.setPixelSize(LABEL_FONT_SIZE);

        m_noBindingsLabelContainer = new NauWidget(nullptr);
        m_noBindingsLabelContainer->setFixedHeight(LABEL_HEIGHT);
        auto* noBindingsLabel = new NauStaticTextLabel(tr("No bindings"), m_noBindingsLabelContainer);
        noBindingsLabel->setFont(font);
        noBindingsLabel->setColor(theme.paletteInputGeneric().color(NauPalette::Role::Text));
        noBindingsLabel->move(LABEL_POSITION);
        m_bindsEditorHeader->addWidget(m_noBindingsLabelContainer);

        return;
    }

    if (m_noBindingsLabelContainer != nullptr && m_bindsEditorHeader->userWidgetsCount() > 1) {
        m_bindsEditorHeader->removeWidget(m_noBindingsLabelContainer);
        m_noBindingsLabelContainer->hide();
        m_noBindingsLabelContainer->deleteLater();
        m_noBindingsLabelContainer = nullptr;
    }
}

void NauInputEditorPage::addBind(const QString& bindType)
{
    if (!m_stage) {
        return;
    }

    int bindId = NauObjectGUID(QDateTime::currentSecsSinceEpoch());
    auto bindPath = pxr::SdfPath("/Binds/Bind_" + std::to_string(bindId));
    
    if (!m_stage->GetPrimAtPath(bindPath)) {
        auto bindPrim = createBindPrim(bindPath, bindId, bindType);
        addBindWidget(bindPrim);
        addBindToRootRelationship(bindPrim);

        m_stage->Save();
    }
}

pxr::UsdPrim NauInputEditorPage::createBindPrim(const pxr::SdfPath& bindPath, int bindId, const QString& bindType)
{
    auto bindPrim = m_stage->DefinePrim(bindPath, pxr::TfToken("Bind"));

    InputUtils::setOrCreateAttribute(bindPrim, "id", pxr::SdfValueTypeNames->Int, bindId);
    InputUtils::setOrCreateAttribute(bindPrim, "name", pxr::SdfValueTypeNames->String, bindType.toStdString());
    InputUtils::setOrCreateAttribute(bindPrim, "type", pxr::SdfValueTypeNames->String, bindType.toStdString());

    return bindPrim;
}

void NauInputEditorPage::addBindToRootRelationship(const pxr::UsdPrim& bindPrim)
{
    auto rootPrim = m_stage->GetPrimAtPath(pxr::SdfPath("/Input"));
    if (rootPrim) {
        auto bindArrayRel = rootPrim.GetRelationship(pxr::TfToken("bindArray"));
        if (!bindArrayRel) {
            bindArrayRel = rootPrim.CreateRelationship(pxr::TfToken("bindArray"), /* custom = */ false);
        }
        bindArrayRel.AddTarget(bindPrim.GetPath());
    }
}
