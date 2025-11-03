// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_widget.hpp"
#include "nau_assert.hpp"
#include "nau_editor_version.hpp"
#include "nau/nau_constants.hpp"
#include "nau_log.hpp"
#include "themes/nau_theme.hpp"

#include <QFile>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QStyleHints>
#include <QPainterPath>


// ** NauMainWindow

NauMainWindow::NauMainWindow()
{
}


// ** NauWidget

NauWidget::NauWidget(QWidget* parent)
    : QWidget(parent)
{
}

void NauWidget::closeEvent(QCloseEvent* event)
{
    emit eventWidgetClosed();
    QWidget::closeEvent(event);
}


//** NauFrame

NauFrame::NauFrame(QWidget* parent)
    : QFrame(parent)
{
}

void NauFrame::paintEvent(QPaintEvent* event)
{
    if (m_palette.empty()) {
        QFrame::paintEvent(event);
        return;
    }

    QPainter painter(this);
    painter.fillRect(event->rect(), m_palette.brush(NauPalette::Role::Background));
}

void NauFrame::setPalette(NauPalette palette)
{
    m_palette = std::move(palette);
    update();
}


//** NauWidgetAction

NauWidgetAction::NauWidgetAction(QWidget* parent)
    : QWidgetAction(parent)
{
}


//** Nau3DWidget

Nau3DWidget::Nau3DWidget(QWidget* parent)
    : QLabel(parent)
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setRenderableType(QSurfaceFormat::OpenGL);

    m_surface = new QOffscreenSurface;
    m_surface->setFormat(format);
    m_surface->create();

    m_context = new QOpenGLContext(m_surface);
    if (m_context->create()) {
        makeCurrent();
        m_context->functions()->initializeOpenGLFunctions();
        doneCurrent();
    } else {
        resetContext();
    }
}

Nau3DWidget::~Nau3DWidget() noexcept
{
    resetContext();
}

void Nau3DWidget::paintEvent(QPaintEvent* event)
{
    QLabel::paintEvent(event);
}

bool Nau3DWidget::isValid() const
{
    return (m_context != nullptr) && (m_surface != nullptr);
}

QOffscreenSurface* Nau3DWidget::surface() const
{
    return m_surface;
}

QOpenGLContext* Nau3DWidget::context() const
{
    return m_context;
}

void Nau3DWidget::resetContext()
{
    delete m_context;
    m_context = nullptr;

    delete m_surface;
    m_surface = nullptr;
}

void Nau3DWidget::makeCurrent()
{
    m_context->makeCurrent(m_surface);
}

void Nau3DWidget::doneCurrent()
{
    m_context->doneCurrent();
}

void Nau3DWidget::render()
{
    if (!isValid()) {
        return;
    }
    makeCurrent();

    QOpenGLFramebufferObject fbo{ size(), QOpenGLFramebufferObject::CombinedDepthStencil };
    fbo.bind();
    onRender();
    fbo.release();
    setPixmap(QPixmap::fromImage(fbo.toImage()));

    doneCurrent();
}


// ** NauPainter

NauPainter::NauPainter(QPaintDevice* device)
    : QPainter(device)
{
}


// ** NauMenuBar

NauMenuBar::NauMenuBar(NauWidget* parent)
    : NauWidget(parent)
    , m_bar(new QMenuBar(parent))
{
	setFocusPolicy(Qt::NoFocus);
}

void NauMenuBar::addMenu(NauMenu* menu)
{
    m_bar->addMenu(menu);
}


// ** NauDialog

NauDialog::NauDialog(QWidget* parent)
    : QDialog(parent)
{
    setMinimumSize(200, 100);
}

int NauDialog::showModal()
{
    return exec();
}

void NauDialog::showModeless()
{
    show();
}


// ** NauDialogButtonBox

NauDialogButtonBox::NauDialogButtonBox(NauDialog* parent)
    : QDialogButtonBox(parent)
{
}


// ** NauSplitter

NauSplitter::NauSplitter(NauWidget* parent)
    : QSplitter(parent)
{
}


// ** NauListView

NauListView::NauListView(QWidget* parent)
    : QListView(parent)
{
}


// ** NauListWidget

NauListWidget::NauListWidget(NauWidget* parent)
    : QListWidget(parent)
{
}


// ** NauTreeView

NauTreeView::NauTreeView(QWidget* parent)
    : QTreeView(parent)
{
}


// ** NauTreeWidget

NauTreeWidget::NauTreeWidget(NauWidget* parent)
    : QTreeWidget(parent)
{
    setAutoScroll(false);
    setAlternatingRowColors(true);
    setRootIsDecorated(false);

    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
}

void NauTreeWidget::setupColumn(int columnIdx, const QString& text, const NauFont& font,
    bool visible, int width, QHeaderView::ResizeMode resizeMode, const QString& tooltip)
{
    headerItem()->setToolTip(columnIdx, tooltip);
    headerItem()->setText(columnIdx, text);
    headerItem()->setFont(columnIdx, font);

    header()->setSectionResizeMode(columnIdx, resizeMode);
    setColumnWidth(columnIdx, width);
    setColumnHidden(columnIdx, !visible);
}

void NauTreeWidget::setupColumn(int columnIdx, const QString& text, const NauFont& font,
    const QIcon& columnIcon, bool visible, int width, QHeaderView::ResizeMode resizeMode, const QString& tooltip)
{
    setupColumn(columnIdx, text, font, visible, width, resizeMode, tooltip);
    headerItem()->setIcon(columnIdx, columnIcon);
}


// ** NauTreeWidgetItem

NauTreeWidgetItem::NauTreeWidgetItem(NauTreeWidget* parent, const QStringList& strings, int type)
    : QTreeWidgetItem(parent, strings, type)
{
}


// ** NauToolButton

NauToolButton::NauToolButton(QWidget* parent)
    : QToolButton(parent)
{
    setStyleSheet("QToolButton:hover{ background-color: #1C6B97; } QToolButton:pressed{ background-color: #258ECA; } QToolButton:checked{ background-color: #1C6B97; }");
}

void NauToolButton::setShortcut(const NauKeySequence& shortcut)
{
    // Do not install a shortcut for the editor has a special class to dispatch
    // keyboard short activations between handlers.
    NED_ASSERT(false && "NauToolButton: unable to set shortcut to action at this moment. Not implemented");
}


// ** NauPushButton

NauPushButton::NauPushButton(NauWidget* parent)
    : QPushButton(parent)
{
}


// ** NauBorderlessButton

NauBorderlessButton::NauBorderlessButton(const QIcon& icon, NauMenu* menu, NauWidget* parent)
    : NauBorderlessButton(menu, parent)
{
    setIcon(icon);
}

NauBorderlessButton::NauBorderlessButton(NauMenu* menu, NauWidget* parent)
    : NauPushButton(parent)
{
    setFlat(true);
    setCursor(Qt::CursorShape::PointingHandCursor);
    setStyleSheet("::menu-indicator{ image:(:/UI/icons/maximize-button.svg); subcontrol-origin: padding; subcontrol-position: center right; }");

    if (menu) {
        setMenu(menu);
    }
}


// ** NauInspectorSubWindow

NauInspectorSubWindow::NauInspectorSubWindow(QWidget* parent)
    : NauWidget(parent)
{
    setLayout(new NauLayoutVertical(this));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout()->setContentsMargins(COMMON_MARGIN);

    m_titleContainer = new NauWidget(this);
    m_titleContainer->setLayout(new NauLayoutHorizontal(m_titleContainer));
    m_titleContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_titleContainer->setContentsMargins(COMMON_MARGIN);
    m_titleContainer->layout()->setContentsMargins(COMMON_MARGIN);
    m_titleContainer->layout()->setSpacing(0);
    layout()->addWidget(m_titleContainer);

    m_titleButton = new NauToolButton(m_titleContainer);
    m_titleButton->setText("Preview");

    // TODO: Temporary fix for the effect: "always on button"
    m_titleButton->setStyleSheet("QToolButton:checked {background-color: #282828; color: #FFFFFF; }"
                                 "QToolButton:hover {background-color: #2B3946;color: #FFFFFF;}"
    );

    m_titleButton->setMinimumHeight(TITLE_HEIGHT);
    m_titleButton->setMaximumHeight(TITLE_HEIGHT);
    m_titleButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_titleButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_titleButton->setCheckable(true);
    m_titleContainer->layout()->addWidget(m_titleButton);

    connect(m_titleButton, &NauToolButton::clicked, this, &NauInspectorSubWindow::expand);

    auto contentContainer = new NauWidget(this);
    contentContainer->setLayout(new NauLayoutHorizontal(contentContainer));
    contentContainer->setContentsMargins(CONTENT_MARGIN);
    contentContainer->layout()->setContentsMargins(COMMON_MARGIN);
    contentContainer->layout()->setSpacing(0);
    contentContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout()->addWidget(contentContainer);

    m_content = new NauScrollWidgetVertical(contentContainer);
    m_content->setContentsMargins(COMMON_MARGIN);
    contentContainer->layout()->addWidget(m_content);

    expand(false);
}

void NauInspectorSubWindow::setText(const QString& text)
{
    m_titleButton->setText(text);
}

void NauInspectorSubWindow::setContentLayout(QLayout& contentLayout)
{
    m_content->setLayout(&contentLayout);
    expand(true);
}

void NauInspectorSubWindow::expand(bool checked)
{
    m_titleButton->setChecked(checked);
    auto* container = static_cast<NauWidget*>( m_content->parent() );
    container->setVisible(checked);
}

// ** NauLineEditStateListener

NauLineEditStateListener::NauLineEditStateListener(const NauLineEdit& lineEdit)
    : m_lineEdit(&lineEdit)
    , m_currentState(NauWidgetState::Active)
    , m_isEditing(false)
{
    connect(&lineEdit, &QLineEdit::editingFinished, [this] () {
        setState(m_lineEdit->underMouse() ? NauWidgetState::Hover : NauWidgetState::Active, EditFlagState::False);
    });
}

bool NauLineEditStateListener::eventFilter(QObject* object, QEvent* event)
{
    NED_ASSERT(object == m_lineEdit);

    const QEvent::Type eventType = event->type();

    if (eventType == QEvent::MouseButtonPress || eventType == QEvent::MouseButtonRelease) {
        setState(NauWidgetState::Pressed, EditFlagState::True);
    } else if (eventType == QEvent::FocusIn && static_cast<QFocusEvent*>(event)->reason() == Qt::FocusReason::TabFocusReason) {
        setState(NauWidgetState::TabFocused, EditFlagState::True);
    } else if (eventType == QEvent::FocusOut || (eventType == QEvent::FocusAboutToChange)) {
        auto* focusEvent = static_cast<QFocusEvent*>(event);
        if (focusEvent->reason() != Qt::PopupFocusReason ||
            !(QApplication::activePopupWidget() && QApplication::activePopupWidget()->parentWidget() == object)) {
            if (m_lineEdit->hasAcceptableInput()) {
                setState(NauWidgetState::Active, EditFlagState::False);
            }
        }
    } else if (eventType == QEvent::Leave || eventType == QEvent::Enter) {
        const NauWidgetState outerState = m_isEditing ? NauWidgetState::Pressed : NauWidgetState::Active;
        setState(m_lineEdit->underMouse() ? NauWidgetState::Hover : outerState, EditFlagState::Pass);
    }
    return QObject::eventFilter(object, event);
}

void NauLineEditStateListener::setState(NauWidgetState widgetState, EditFlagState flagState) noexcept
{
    switch (flagState) {
    case EditFlagState::True:
        m_isEditing = !m_lineEdit->isReadOnly();
        break;
    case EditFlagState::False:
        m_isEditing = false;
        break;
    case EditFlagState::Pass:
        m_isEditing = !m_lineEdit->isReadOnly() && m_isEditing;
        break;
    }
    // Text was not changed through the UI
    if (widgetState != NauWidgetState::Pressed || m_isEditing) {
        m_currentState = widgetState;
    }
}

// ** NauLineEdit

NauLineEdit::NauLineEdit(QWidget* parent)
    : QLineEdit(parent)
    , m_listener(*this)
{
    setFont(Nau::Theme::current().fontDataContainers());
    setFixedHeight(Height);

    // TODO: Now it is not possible to use the palette for this widget as long as it has a style in main.qss.
    // If there is a style in qss, Qt does not look at the palette. Fix in the future.
    setStyleSheet("background-color: #222222");
    installEventFilter(&m_listener);
    connect(this, &QLineEdit::textChanged, [this]{
        m_listener.setState(NauWidgetState::Pressed, NauLineEditStateListener::EditFlagState::Pass);
    });
}


// ** NauSlider

NauSlider::NauSlider(QWidget* parent)
    : QSlider(parent)
{
}

void NauSlider::setPalette(const NauPalette& palette)
{
    if (palette == m_palette) {
        return;
    }

    m_palette = palette;
    update();
}

const NauPalette& NauSlider::nauPalette() const
{
    return m_palette;
}

void NauSlider::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    QSlider::enterEvent(event);
}

void NauSlider::leaveEvent(QEvent* event)
{
    m_hovered = false;
    QSlider::leaveEvent(event);
}

bool NauSlider::hovered() const
{
    return m_hovered;
}


// ** NauCheckBox

NauCheckBox::NauCheckBox(QWidget* parent)
    : QCheckBox(parent)
{
}

NauCheckBox::NauCheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent)
{
}


// ** NauComboBox

NauComboBox::NauComboBox(QWidget* parent)
    : QComboBox(parent)
{
    setFont(Nau::Theme::current().fontDataContainers());
    setFocusPolicy(Qt::StrongFocus);
}

void NauComboBox::wheelEvent(QWheelEvent* event)
{
    // Disabled scroll events so we don't spam undo/redo commands
    event->accept();
}


// ** NauSpinBox

NauSpinBox::NauSpinBox(QWidget* parent)
    : QSpinBox(parent)
    , m_lineEdit(new NauLineEdit(this))
{
    setFont(Nau::Theme::current().fontDataContainers());
    setFixedHeight(Height);
    setButtonSymbols(QAbstractSpinBox::NoButtons);

    // TODO: Now it is not possible to use the palette for this widget as long as it has a style in main.qss.
    // If there is a style in qss, Qt does not look at the palette. Fix in the future.
    setStyleSheet("background-color: #222222; border: 1px solid #222222; border-radius: 2px;");
    setFocusPolicy(Qt::StrongFocus);
    setLineEdit(m_lineEdit);
}

void NauSpinBox::wheelEvent(QWheelEvent* event)
{
    // Disabled scroll events so we don't spam undo/redo commands
    event->accept();
}

void NauSpinBox::resizeEvent(QResizeEvent* event)
{
    QSpinBox::resizeEvent(event);
    if (event->size().isValid()) {
        m_lineEdit->resize(event->size());
    }
}


// ** NauDoubleSpinBox

NauDoubleSpinBox::NauDoubleSpinBox(QWidget* parent)
    : QDoubleSpinBox(parent)
    , m_lineEdit(new NauLineEdit(this))
{
    setFont(Nau::Theme::current().fontDataContainers());
    setFixedHeight(Height);
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    this->setDecimals(NAU_WIDGET_DECIMAL_PRECISION);
    // TODO: Now it is not possible to use the palette for this widget as long as it has a style in main.qss.
    // If there is a style in qss, Qt does not look at the palette. Fix in the future.
    setStyleSheet("background-color: #222222; border: 1px solid #222222; border-radius: 2px;");
    setFocusPolicy(Qt::StrongFocus);
    setLineEdit(m_lineEdit);
}

void NauDoubleSpinBox::wheelEvent(QWheelEvent* event)
{
    // Disabled scroll events so we don't spam undo/redo commands
    event->accept();
}

void NauDoubleSpinBox::resizeEvent(QResizeEvent* event)
{
    QDoubleSpinBox::resizeEvent(event);
    if (event->size().isValid()) {
        m_lineEdit->resize(event->size());
    }
}


// ** NauTimeEdit

NauTimeEdit::NauTimeEdit(QWidget* parent)
    : QTimeEdit(parent)
    , m_lineEdit(new NauLineEdit(this))
{
    setFont(Nau::Theme::current().fontDataContainers());
    setFixedHeight(Height);
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    // TODO: Now it is not possible to use the palette for this widget as long as it has a style in main.qss.
    // If there is a style in qss, Qt does not look at the palette. Fix in the future.
    setStyleSheet("background-color: #222222; border: 1px solid #222222; border-radius: 2px;");
    setFocusPolicy(Qt::StrongFocus);
    setLineEdit(m_lineEdit);
}

void NauTimeEdit::wheelEvent(QWheelEvent* event)
{
    // Disabled scroll events so we don't spam undo/redo commands
    event->accept();
}

void NauTimeEdit::resizeEvent(QResizeEvent* event)
{
    QTimeEdit::resizeEvent(event);
    if (event->size().isValid()) {
        m_lineEdit->resize(event->size());
    }
}


// ** NauMultiValueSpinBox

NauMultiValueSpinBox::NauMultiValueSpinBox(NauWidget* parent, int valuesCount)
{
    m_valueLayout = new NauLayoutGrid(this);
    m_valueLayout->setSpacing(0);

    // Needed for the math to work properly when forming a 3-column system.
    auto columnAdditionIndex = 0;

    // Form a three-column system, which consists of a column with a decorative widget,
    // as well as a column with content and a column separator:
    // |13px||Content filling||16px|

    // TODO: Move to a two-column system in the future.
    for (int i = 0; i < (valuesCount * 2) - 1; ++i) {
        if (i % 2 == 0) {
            m_valueLayout->setColumnStretch(i + columnAdditionIndex, 0);
            m_valueLayout->setColumnMinimumWidth(i + columnAdditionIndex, FirstColumnWidth);

            m_valueLayout->setColumnStretch(i + columnAdditionIndex + 1, 1);
            columnAdditionIndex += 1;
        }
        else {
            m_valueLayout->setColumnStretch(i + columnAdditionIndex, 0);
            m_valueLayout->setColumnMinimumWidth(i + columnAdditionIndex, ThirdColumnWidth);
        }
    }

    // TODO: Rewrite it with paintEvent
    std::vector<std::string> linesStyleSheets = { "background-color: #F8547B; border: 1px solid #F8547B; border-radius: 8px;" ,
                                                  "background-color: #29D46C; border: 1px solid #29D46C; border-radius: 8px;" ,
                                                  "background-color: #39B6FC; border: 1px solid #39B6FC; border-radius: 8px;"
    };

    // Indicates the current content column (in a three-column system) that we are filling in.
    auto currentContentColumnIndex = 1;
    for (int i = 0; i < valuesCount; ++i) {
        // TODO: A very dirty hack to add a decorative element to SpinBox.
        // Fix in the future for sure!
        auto decorWidget = new NauWidget();
        auto stackedLayout = new NauLayoutStacked(decorWidget);
        stackedLayout->setStackingMode(QStackedLayout::StackAll);

        auto boxFrame = new QFrame();
        boxFrame->setFrameShape(QFrame::Box);
        boxFrame->setFixedWidth(BoxFrameWidth);
        boxFrame->setStyleSheet("background-color: #222222; border: 1px solid #222222; border-top-left-radius: 1px; border-bottom-left-radius: 1px;");

        auto lineFrame = new QFrame();
        lineFrame->setFrameShape(QFrame::Box);
        lineFrame->setFixedWidth(LineFrameWidth);
        lineFrame->setFixedHeight(LineFrameHeight);

        // By design there are colors for only three values, the rest will remain gray
        lineFrame->setStyleSheet(valuesCount <= 3 ? linesStyleSheets[i].c_str() : "background-color: #282828; border: 1px solid #282828; border-radius: 8px;");

        // TODO: Rewrite to NauLineWidget
        auto lineWidget = new NauWidget();
        lineWidget->setFixedWidth(LineWidgetWidth);
        auto lineHorizontalLayout = new NauLayoutHorizontal(lineWidget);
        lineHorizontalLayout->addWidget(lineFrame);

        stackedLayout->addWidget(lineWidget);
        stackedLayout->addWidget(boxFrame);

        auto spinBox = new NauSpinBox(this);
        spinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

        // TODO: Rewrite it with paintEvent
        spinBox->setStyleSheet("background-color: #222222; border: 1px solid #222222; border-top-left-radius: 0px; border-bottom-left-radius: 0px;");

        m_valueLayout->addWidget(decorWidget, 0, currentContentColumnIndex - 1);
        m_valueLayout->addWidget(spinBox, 0, currentContentColumnIndex);
        currentContentColumnIndex += 3;

        connect(spinBox, &NauSpinBox::valueChanged, this, &NauMultiValueSpinBox::eventValueChanged);
        m_spinBoxes.push_back(spinBox);
    }
}

NauMultiValueSpinBox::NauMultiValueSpinBox(NauWidget* parent, QStringList valuesNames)
{
    m_valueLayout = new NauLayoutGrid(this);
    m_valueLayout->setVerticalSpacing(VerticalSpacer);

    // Form a regular two-column system, which consists of a content column and a separator column:
    // |Content filling||16px|
    auto namesCount = (valuesNames.size() * 2) - 1;
    for (int i = 0; i < namesCount; ++i) {
        if (i % 2 == 0) {
            m_valueLayout->setColumnStretch(i, 1);
        }
        else {
            m_valueLayout->setColumnStretch(i, 0);
            m_valueLayout->setColumnMinimumWidth(i, 16);
        }
    }

    // Indicates the current content column (in a two-column system) that we are filling in.
    auto currentContentColumnIndex = 0;
    for (auto name : valuesNames) {
        auto label = new QLabel(name);
        label->setFont(Nau::Theme::current().fontDataContainersLabel());
        label->setFixedHeight(16);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

        auto spinBox = new NauSpinBox(this);
        spinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        spinBox->setAutoFillBackground(false);
        connect(spinBox, &NauSpinBox::valueChanged, this, &NauMultiValueSpinBox::eventValueChanged);
        m_spinBoxes.push_back(spinBox);

        m_valueLayout->addWidget(label, 0, currentContentColumnIndex);
        m_valueLayout->addWidget(spinBox, 1, currentContentColumnIndex);

        currentContentColumnIndex += 2;
    }
}

NauSpinBox* NauMultiValueSpinBox::operator[](int index) const
{
    return m_spinBoxes[index];
}

void NauMultiValueSpinBox::setMinimum(int min)
{
    for (const auto& spinBox : m_spinBoxes) {
        spinBox->setMinimum(min);
    }
}

void NauMultiValueSpinBox::setMaximum(int max)
{
    for (const auto& spinBox : m_spinBoxes) {
        spinBox->setMaximum(max);
    }
}


// ** NauMultiValueSpinBox

NauMultiValueDoubleSpinBox::NauMultiValueDoubleSpinBox(NauWidget* parent, int valuesCount)
{
    m_valueLayout = new NauLayoutGrid(this);
    m_valueLayout->setVerticalSpacing(VerticalSpacer);

    // Needed for the math to work properly when forming a 3-column system.
    auto columnAdditionIndex = 0;

    // Form a three-column system, which consists of a column with a decorative widget,
    // as well as a column with content and a column separator:
    // |13px||Content filling||16px|

    // TODO: Move to a two-column system in the future.
    for (int i = 0; i < (valuesCount * 2) - 1; ++i) {
        if (i % 2 == 0) {
            m_valueLayout->setColumnStretch(i + columnAdditionIndex, 0);
            m_valueLayout->setColumnMinimumWidth(i + columnAdditionIndex, FirstColumnWidth);

            m_valueLayout->setColumnStretch(i + columnAdditionIndex + 1, 1);
            columnAdditionIndex += 1;
        } else {
            m_valueLayout->setColumnStretch(i + columnAdditionIndex, 0);
            m_valueLayout->setColumnMinimumWidth(i + columnAdditionIndex, ThirdColumnWidth);
        }
    }

    // TODO: Rewrite it with paintEvent
    std::vector<std::string> linesStyleSheets = { "background-color: #F8547B; border: 1px solid #F8547B; border-radius: 8px;" ,
                                                  "background-color: #29D46C; border: 1px solid #29D46C; border-radius: 8px;" ,
                                                  "background-color: #39B6FC; border: 1px solid #39B6FC; border-radius: 8px;" 
    };

    // Indicates the current content column (in a three-column system) that we are filling in.
    auto currentContentColumnIndex = 1;
    for (int i = 0; i < valuesCount; ++i) {
        // TODO: A very dirty hack to add a decorative element to SpinBox.
        // Fix in the future for sure!
        auto decorWidget = new NauWidget();
        auto stackedLayout = new NauLayoutStacked(decorWidget);
        stackedLayout->setStackingMode(QStackedLayout::StackAll);

        auto boxFrame = new QFrame();
        boxFrame->setFrameShape(QFrame::Box);
        boxFrame->setFixedWidth(BoxFrameWidth);
        boxFrame->setStyleSheet("background-color: #222222; border: 1px solid #222222; border-top-left-radius: 1px; border-bottom-left-radius: 1px;");

        auto lineFrame = new QFrame();
        lineFrame->setFrameShape(QFrame::Box);
        lineFrame->setFixedWidth(LineFrameWidth);
        lineFrame->setFixedHeight(LineFrameHeight);

        // By design there are colors for only three values, the rest will remain gray
        lineFrame->setStyleSheet(valuesCount <= 3 ? linesStyleSheets[i].c_str() : "background-color: #282828; border: 1px solid #282828; border-radius: 8px;");

        // TODO: Rewrite to NauLineWidget
        auto lineWidget = new NauWidget();
        lineWidget->setFixedWidth(LineWidgetWidth);
        auto lineHorizontalLayout = new NauLayoutHorizontal(lineWidget);
        lineHorizontalLayout->addWidget(lineFrame);

        stackedLayout->addWidget(lineWidget);
        stackedLayout->addWidget(boxFrame);

        auto spinBox = new NauDoubleSpinBox(this);
        spinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

        // TODO: Rewrite it with paintEvent
        spinBox->setStyleSheet("background-color: #222222; border: 1px solid #222222; border-top-left-radius: 0px; border-bottom-left-radius: 0px;");
        
        m_valueLayout->addWidget(decorWidget, 0, currentContentColumnIndex-1);
        m_valueLayout->addWidget(spinBox, 0, currentContentColumnIndex);
        currentContentColumnIndex += 3;

        connect(spinBox, &NauDoubleSpinBox::valueChanged, this, &NauMultiValueDoubleSpinBox::eventValueChanged);
        m_spinBoxes.push_back(spinBox);
    }
}

NauMultiValueDoubleSpinBox::NauMultiValueDoubleSpinBox(NauWidget* parent, QStringList valuesNames)
{
    m_valueLayout = new NauLayoutGrid(this);
    m_valueLayout->setVerticalSpacing(VerticalSpacer);

    // Form a regular two-column system, which consists of a content column and a separator column:
    // |Content filling||16px|
    auto namesCount = (valuesNames.size() * 2) - 1;
    for (int i = 0; i < namesCount; ++i) {
        if (i % 2 == 0) {
            m_valueLayout->setColumnStretch(i, 1);
        }
        else {
            m_valueLayout->setColumnStretch(i, 0);
            m_valueLayout->setColumnMinimumWidth(i, 16);
        }
    }

    // Indicates the current content column (in a two-column system) that we are filling in.
    auto currentContentColumnIndex = 0;
    for (auto name : valuesNames) {


        auto label = new QLabel(name);
        label->setFont(Nau::Theme::current().fontDataContainersLabel());
        label->setFixedHeight(16);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

        auto spinBox = new NauDoubleSpinBox(this);
        spinBox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        spinBox->setAutoFillBackground(false);
        connect(spinBox, &NauDoubleSpinBox::valueChanged, this, &NauMultiValueDoubleSpinBox::eventValueChanged);
        m_spinBoxes.push_back(spinBox);

        m_valueLayout->addWidget(label, 0, currentContentColumnIndex);
        m_valueLayout->addWidget(spinBox, 1, currentContentColumnIndex);

        currentContentColumnIndex += 2;
    }
}

NauDoubleSpinBox* NauMultiValueDoubleSpinBox::operator[](int index) const
{
    return m_spinBoxes[index];
}

void NauMultiValueDoubleSpinBox::setMinimum(double min)
{
    for (const auto& spinBox : m_spinBoxes) {
        spinBox->setMinimum(min);
    }
}

void NauMultiValueDoubleSpinBox::setMaximum(double max)
{
    for (const auto& spinBox : m_spinBoxes) {
        spinBox->setMaximum(max);
    }
}

void NauMultiValueDoubleSpinBox::setDecimals(int decimalPrecision)
{
    for (const auto& spinBox : m_spinBoxes) {
        spinBox->setDecimals(decimalPrecision);
    }
}

void NauMultiValueDoubleSpinBox::disableSpinbox(int idx)
{
    if (m_spinBoxes.size() >= (idx + 1)) {
        m_spinBoxes[idx]->setDisabled(true);
    }
}


// ** NauColorDialog

NauColorDialog::NauColorDialog(NauWidget* parent)
    : NauWidget(parent)
{
}

void NauColorDialog::colorDialogRequested()
{
    QColorDialog dlg(m_color, this);
    dlg.setOptions(QColorDialog::ColorDialogOptions());
    dlg.adjustSize();

    if (dlg.exec() == NauDialog::Accepted) {
        m_color = dlg.selectedColor();
        emit eventColorChanged(m_color);
    }
}

void NauColorDialog::setColor(const QColor& currentColor)
{
    m_color = currentColor;
}

QColor NauColorDialog::color() const
{
    return m_color;
}


// ** NauTableHeaderIconDelegate

NauTableHeaderIconDelegate::NauTableHeaderIconDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void NauTableHeaderIconDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    // By default, the picture with an empty line was positioned in the middle between them.
    // Now if there is no text, the picture will be positioned in the middle of the cell.
    if (option->features.testFlag(QStyleOptionViewItem::HasDecoration))
    {
        QSize decorationSize = option->decorationSize;
        // TODO: Hack with padding
        decorationSize.setWidth(option->rect.width() + m_pixmapLeftPadding);
        option->decorationSize = decorationSize;
    }
}

void NauTableHeaderIconDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    QStyledItemDelegate::paint(painter, option, index);
}


// ** NauProxyStyle

NauProxyStyle::NauProxyStyle(QStyle* style)
    : QProxyStyle(style)
{
}

// ** NauTableWidgetItem

NauTableWidgetItem::NauTableWidgetItem()
    : QTableWidgetItem()
{
}

NauTableWidgetItem::NauTableWidgetItem(const QIcon& icon)
    : QTableWidgetItem()
{
    // This process of setting the picture is needed so that tableItem doesn't get the role: Qt::DisplayRole
    // Qt::DisplayRole breaks the icon positioning process
    setIcon(icon);
}

NauTableWidgetItem::NauTableWidgetItem(const QString& text)
    : QTableWidgetItem(text)
{
}

NauTableWidgetItem::NauTableWidgetItem(const QIcon& icon, const QString& text)
    : QTableWidgetItem(icon, text)
{
}


// ** NauTableProxySyle

NauTableProxySyle::NauTableProxySyle(QStyle* style)
    : NauProxyStyle(style)
{
}


// ** NauNoFocusNauTableProxyStyle

NauNoFocusNauTableProxyStyle::NauNoFocusNauTableProxyStyle(QStyle* style)
    : NauTableProxySyle(style)
{
}

void NauNoFocusNauTableProxyStyle::drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const
{
    if (element == QStyle::PE_FrameFocusRect) {
        return;
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
}


// ** NauTableHeaderContextMenu

NauTableHeaderContextMenu::NauTableHeaderContextMenu(const QString& title, NauWidget* widget)
    : NauMenu(title, widget)
{
}


NauCheckBox* NauTableHeaderContextMenu::addAction(const QString& title, bool isChecked)
{
    NauCheckBox* checkBox = new NauCheckBox(title, this);
    checkBox->setChecked(isChecked);

    QWidgetAction* action = new QWidgetAction(this);
    action->setDefaultWidget(checkBox);
    NauMenu::addAction(action);

    return checkBox;
}


// ** NauTableWidget

NauTableWidget::NauTableWidget(QWidget* parent)
    : QTableWidget(parent)
{
    // Remove the standard table grid 
    setShowGrid(false);

    // Necessary for correct work of selection on hovering
    setMouseTracking(true);

    // Disabling auto scrolling
    setAutoScroll(false);

    // Setup per pixel scrolling
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    setFont(Nau::Theme::current().fontTableWidgetBase());

    // This delegate fixes the error of incorrectly positioning an icon in a View-Item that had no text.
    setItemDelegate(new NauTableHeaderIconDelegate());

    // Customization of the timing allocation process in the table
    setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);

    // Need to avoid drawing a dotted border around the cell
    setStyle(new NauNoFocusNauTableProxyStyle());

    setAlternatingRowColors(true);

    setStyleSheet(
        "QTableView"
        "{"
            "background-color: #282828;"
            "alternate-background-color: #242424;"
        "}"

        "QTableView::item:selected"
        "{"
            "background-color : rgba(000, 000, 000, 000);"
        "}"
    );

}

void NauTableWidget::addColumn(const QString& titleName, bool visibilityStatus, int size, QHeaderView::ResizeMode mode, const QString& tooltip)
{
    QTableWidgetItem* header = new QTableWidgetItem(titleName);
    header->setToolTip(tooltip);
    addColumnHeaderItem(header, visibilityStatus, size, mode);
}

void NauTableWidget::addColumn(const QString& titleName, const QIcon& columnIcon, bool visibilityStatus, int size, QHeaderView::ResizeMode mode, const QString& tooltip)
{
    QTableWidgetItem* header = new QTableWidgetItem(columnIcon, titleName, Qt::DecorationRole);
    header->setToolTip(tooltip);
    addColumnHeaderItem(header, visibilityStatus, size, mode);
}

void NauTableWidget::changeColumnVisibility(int column, bool visibilityStatus)
{
    visibilityStatus ? QTableWidget::showColumn(column) : QTableWidget::hideColumn(column);
}

void NauTableWidget::addColumnHeaderItem(QTableWidgetItem* header, bool visibilityStatus, int size, QHeaderView::ResizeMode mode)
{
    const int columnCount = QTableWidget::columnCount();

    insertColumn(columnCount);
    setHorizontalHeaderItem(columnCount, header);
    setColumnWidth(columnCount, size);
    horizontalHeader()->setSectionResizeMode(columnCount, mode);
    changeColumnVisibility(columnCount, visibilityStatus);
}


// ** NauLineWidget

NauLineWidget::NauLineWidget(const QColor& lineColor, int lineWidth, Qt::Orientation orientation, QWidget* parent)
    : NauWidget(parent)
    , m_lineColor(lineColor)
    , m_lineWidth(lineWidth)
    , m_orientation(orientation)
{
}

void NauLineWidget::setOffset(int offset)
{
    m_offset = offset;
}

void NauLineWidget::paintEvent(QPaintEvent*)
{
    QPoint point1;
    QPoint point2;
    const QSize size = this->size();
    switch (m_orientation)
    {
    case Qt::Orientation::Vertical:
        point1 = { size.width() / 2, m_offset };
        point2 = { size.width() / 2, size.height() - m_offset };
        break;
    case Qt::Orientation::Horizontal:
        point1 = { m_offset, size.height() / 2};
        point2 = { size.width() - m_offset, size.height() / 2 };
        break;
    }

    QPainter painter(this);
    painter.setPen(QPen(m_lineColor, m_lineWidth));
    painter.drawLine(point1, point2);
}


// ** NauSpacer

NauSpacer::NauSpacer(Qt::Orientation orientation, int size, QWidget* parent)
    : NauFrame(parent)
{
    if (orientation == Qt::Orientation::Horizontal) {
        setFixedHeight(size);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setObjectName("horizontalSpacer");
    } else {
        setFixedWidth(size);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        setObjectName("verticalSpacer");
    }

    setPalette(Nau::Theme::current().paletteSpacerWidget());
}


// ** NauRadioButton

NauRadioButton::NauRadioButton(NauWidget* parent)
    : QRadioButton(parent)

{
}

NauRadioButton::NauRadioButton(const QString& text, NauWidget* parent)
    : QRadioButton(text, parent)
{
}


// ** NauProgressBar

NauProgressBar::NauProgressBar(QWidget* parent)
    : QProgressBar(parent)
{
}


// ** NauToogleButton

NauToogleButton::NauToogleButton(NauWidget* parent)
    : QCheckBox(parent)
{
    const auto& theme = Nau::Theme::current();
    m_styleMap = theme.styleToogleButton().styleByState;

    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

void NauToogleButton::setState(NauWidgetState state)
{
    m_currentState = state;
    update();
}

void NauToogleButton::setStateStyle(NauWidgetState state, NauWidgetStyle::NauStyle style)
{
    m_styleMap[state] = style;
}

void NauToogleButton::setChecked(bool isChecked)
{
    QCheckBox::setChecked(isChecked);
    updateWidgetState();
}

void NauToogleButton::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw the background
    QRect rect = this->rect();

    painter.setPen(m_styleMap[m_currentState].outlinePen);

    painter.setBrush(m_styleMap[m_currentState].background);
    painter.drawRoundedRect(rect, rect.height() * 0.5, rect.width() * 0.5);

    // Draw the toggle circle
    int diameter = rect.height() - 2 * Radius;
    int xPosition = isChecked() ? rect.width() - diameter - Offset : Offset;
    QRect circleRect(xPosition, Offset, diameter, diameter);

    painter.setBrush(m_styleMap[m_currentState].textColor);
    painter.drawEllipse(circleRect);
}

void NauToogleButton::mousePressEvent(QMouseEvent* event)
{
    toggle();
    updateWidgetState();
}

void NauToogleButton::toggle()
{
    setChecked(!isChecked());
    emit toggled(isChecked());
    update();
}

void NauToogleButton::updateWidgetState()
{
    if (isChecked()) {
        setState(NauWidgetState::Pressed);
    } else {
        setState(NauWidgetState::Active);
    }
}

QPainterPath NauToogleButton::getOutlinePath(NauWidgetState state)
{
    QPainterPath path;
    auto& style = m_styleMap[state];

    if (style.outlinePen.widthF() > 0.0) {
        const QRectF backgroundRect = rect() - 0.5 * style.outlinePen.widthF() * QMarginsF(1.0, 1.0, 1.0, 1.0);
        path.addRoundedRect(backgroundRect, style.radiusSize.width(), style.radiusSize.height());
    }

    return path;
}

// TODO: Make Nau wrappers for the relevant components:
//    QScrollbar
//    QTimer
