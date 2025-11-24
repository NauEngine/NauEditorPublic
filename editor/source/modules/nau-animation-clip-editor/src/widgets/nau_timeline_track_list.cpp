// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/widgets/nau_timeline_track_list.hpp"
#include "nau/widgets/nau_common_timeline_widgets.hpp"

#include "baseWidgets/nau_buttons.hpp"
#include "nau/nau_constants.hpp"
#include "themes/nau_theme.hpp"
#include "nau_entity_creation_panel.hpp"
#include "nau_tree_view_item_delegate.hpp"
#include "nau_assert.hpp"
#include "nau_utils.hpp"

#include <QScrollBar>


enum NauTimelineTrackListConstants
{
    VISIBLE_COLUMN_INDEX = 0,
    VISIBLE_COLUMN_FIXED_WIDTH = 16,
    LOCK_COLUMN_INDEX = 1,
    LOCK_COLUMN_FIXED_WIDTH = 16,
    NAME_COLUMN_INDEX = 2,
    VALUE_COLUMN_INDEX = 3,
    VALUE_COLUMN_FIXED_WIDTH = 80,
    OPTION_COLUMN_INDEX = 4,
    OPTION_COLUMN_FIXED_WIDTH = 16,
    FIXED_FILLED_COLUMN_WIDTH = VISIBLE_COLUMN_FIXED_WIDTH + LOCK_COLUMN_FIXED_WIDTH + VALUE_COLUMN_FIXED_WIDTH + OPTION_COLUMN_FIXED_WIDTH + 70,
};


// ** NauTimelineTrackHandler

NauTimelineTrackHandler::NauTimelineTrackHandler(NauTimelineTreeWidget* treeWidget, int propertyIndex)
    : m_treeWidget(treeWidget)
    , m_propertyIndex(propertyIndex)
{
}

void NauTimelineTrackHandler::addWidgetToItem(NauTreeWidgetItem* item, int column, QWidget* widget)
{
    auto* container = new NauWidget(m_treeWidget);
    container->setLayout(new NauLayoutHorizontal);
    container->layout()->setContentsMargins(4, 4, 4, 4);
    container->layout()->addWidget(widget);
    m_treeWidget->setItemWidget(item, column, container);
}

NauTreeWidgetItem* NauTimelineTrackHandler::createTreeItem(NauTreeWidgetItem* parent, const QString& name)
{
    const auto& theme = Nau::Theme::current();
    const NauIcon TYPE_ICON{ ":/animationEditor/icons/animationEditor/vTypedIconEmpty.svg" };
    const NauIcon lockIcon = theme.iconAvailability();
    const NauIcon visibleIcon = theme.iconVisibility();

    auto* child = new NauTreeWidgetItem(nullptr, {});
    child->setIcon(VISIBLE_COLUMN_INDEX, visibleIcon);
    child->setIcon(LOCK_COLUMN_INDEX, lockIcon);
    child->setIcon(NAME_COLUMN_INDEX, TYPE_ICON);
    child->setText(NAME_COLUMN_INDEX, name);
    if (parent != nullptr) {
        parent->addChild(child);
    }
    return child;
}


// ** NauTimelineTrackVec3Handler

NauTimelineTrackVec3Handler::NauTimelineTrackVec3Handler(NauTimelineTreeWidget* treeWidget, int propertyIndex, const NauAnimationProperty* property)
    : NauTimelineTrackHandler(treeWidget, propertyIndex)
    , m_property(property)
{
    m_components.fill(nullptr);
    m_widgets.fill(nullptr);
    m_blockers.fill(nullptr);
}

NauTimelineTrackVec3Handler::~NauTimelineTrackVec3Handler() noexcept
{
    setBlockEditingSignals(false);
}

NauTreeWidgetItem* NauTimelineTrackVec3Handler::createTreeItem(NauTreeWidgetItem* parent, const QString& name)
{
    auto* item = NauTimelineTrackHandler::createTreeItem(parent, name);

    constexpr std::array suffixes{ ".x", ".y", ".z" };
    const bool isReadOnly = m_property->isReadOnly();

    for (int index = 0; index < 3; ++index) {
        m_components[index] = NauTimelineTrackHandler::createTreeItem(item, name + suffixes[index]);

        m_widgets[index] = new NauDoubleSpinBox(m_treeWidget);
        m_widgets[index]->setFixedHeight(24);
        m_widgets[index]->setDecimals(NAU_WIDGET_DECIMAL_PRECISION);
        m_widgets[index]->setMinimum(NAU_POSITION_MIN_LIMITATION);
        m_widgets[index]->setMaximum(NAU_POSITION_MAX_LIMITATION);
        m_widgets[index]->setReadOnly(isReadOnly);

        addWidgetToItem(m_components[index], VALUE_COLUMN_INDEX, m_widgets[index]);

        connect(m_widgets[index], &NauDoubleSpinBox::editingFinished, [this, index]() {
            nau::math::vec3 data;
            for (int index = 0; index < m_widgets.size(); ++index) {
                data.setElem(index, m_widgets[index]->value());
            }
            QSignalBlocker blocker{ m_widgets[index] };
            m_widgets[index]->clearFocus();
            emit eventEditingFinished(m_propertyIndex, data);
        });
    }
    return item;
}

void NauTimelineTrackVec3Handler::setBlockEditingSignals(bool flag)
{
    if (m_widgets[0] == nullptr) {
        return;
    }
    if (flag && (m_blockers[0] != nullptr)) {
        return;
    }
    int index = 0;
    for (auto* widget: m_widgets) {
        if (flag) {
            m_blockers[index] = new QSignalBlocker(widget);
        } else {
            delete m_blockers[index];
            m_blockers[index] = nullptr;
        }
        ++index;
    }
}

void NauTimelineTrackVec3Handler::updateValue(float time)
{
    if (m_property->timeSamples().empty()) {
        return;
    }
    auto vec = std::get<nau::math::vec3>(m_property->dataForTime(time).variant);
    int index = 0;
    for (auto* widget: m_widgets) {
        widget->setValue(vec[index++]);
    }
}


// ** NauTimelineTrackQuatHandler

NauTimelineTrackQuatHandler::NauTimelineTrackQuatHandler(NauTimelineTreeWidget* treeWidget, int propertyIndex, const NauAnimationProperty* property)
    : NauTimelineTrackHandler(treeWidget, propertyIndex)
    , m_property(property)
{
    m_components.fill(nullptr);
    m_widgets.fill(nullptr);
    m_blockers.fill(nullptr);
}

NauTimelineTrackQuatHandler::~NauTimelineTrackQuatHandler() noexcept
{
    setBlockEditingSignals(false);
}

NauTreeWidgetItem* NauTimelineTrackQuatHandler::createTreeItem(NauTreeWidgetItem* parent, const QString& name)
{
    auto* item = NauTimelineTrackHandler::createTreeItem(parent, name);

    constexpr std::array suffixes{ ".x", ".y", ".z", ".w" };
    const bool isReadOnly = m_property->isReadOnly();

    for (int elemIndex = 0; elemIndex < 4; ++elemIndex) {
        m_components[elemIndex] = NauTimelineTrackHandler::createTreeItem(item, name + suffixes[elemIndex]);

        m_widgets[elemIndex] = new NauDoubleSpinBox(m_treeWidget);
        m_widgets[elemIndex]->setFixedHeight(24);
        m_widgets[elemIndex]->setDecimals(6);
        m_widgets[elemIndex]->setMinimum(-1.);
        m_widgets[elemIndex]->setMaximum( 1.);
        m_widgets[elemIndex]->setReadOnly(isReadOnly);

        addWidgetToItem(m_components[elemIndex], VALUE_COLUMN_INDEX, m_widgets[elemIndex]);

        connect(m_widgets[elemIndex], &NauDoubleSpinBox::editingFinished, [this, elemIndex]() {
            const float value = static_cast<float>(m_widgets[elemIndex]->value());
            const float coef = std::sqrtf(1.f - value * value);

            nau::math::quat data{ 0.f, 0.f, 0.f, 0.f };

            if (coef < 0.00001f) {
                data.setElem(elemIndex, 1.f);
            } else {
                for (int index = 0; index < m_widgets.size(); ++index) {
                    data.setElem(index, m_widgets[index]->value() / coef);
                }
                data.setElem(elemIndex, value);
            }

            data = Vectormath::SSE::normalize(data);
            int index = 0;
            for (auto* widget : m_widgets) {
                widget->setValue(data[index++]);
            }
            
            QSignalBlocker blocker{ m_widgets[elemIndex] };
            m_widgets[elemIndex]->clearFocus();
            emit eventEditingFinished(m_propertyIndex, data);
        });
    }
    return item;
}

void NauTimelineTrackQuatHandler::setBlockEditingSignals(bool flag)
{
    if (m_widgets[0] == nullptr) {
        return;
    }
    if (flag && (m_blockers[0] != nullptr)) {
        return;
    }
    int index = 0;
    for (auto* widget : m_widgets) {
        if (flag) {
            m_blockers[index] = new QSignalBlocker(widget);
        } else {
            delete m_blockers[index];
            m_blockers[index] = nullptr;
        }
        ++index;
    }
}

void NauTimelineTrackQuatHandler::updateValue(float time)
{
    if (m_property->timeSamples().empty()) {
        return;
    }
    auto vec = std::get<nau::math::quat>(m_property->dataForTime(time).variant);
    int index = 0;
    for (auto* widget : m_widgets) {
        widget->setValue(vec[index++]);
    }
}


// ** NauTimelineTrackListHeader

NauTimelineTrackListHeader::NauTimelineTrackListHeader(NauWidget* parent)
    : NauWidget(parent)
    , m_propertyListMenu(new NauMenu(this))
    , m_animationSelector(new NauComboBox(this))
    , m_addPropertyButton(new NauPrimaryButton(this))
{
    constexpr QSize BUTTON_SIZE{ 122, 24 };
    constexpr QSize BUTTON_ROUND{ 12, 12 };
    constexpr QSize TOOLS_CONTAINER_SIZE{ 68, 32 };

    const QString BUTTON_STYLE_SHEET = "background-color: 0#00000000";
    const NauIcon icon{ ":/animationEditor/icons/animationEditor/vPlaceholder.svg" };

    /// TODO: delete after completion of work on custom combobox
    m_animationSelector->setObjectName("NauTL_headerBox");
    m_animationSelector->setStyleSheet("QComboBox#NauTL_headerBox { background-color: #222222; }"
                                       "QComboBox#NauTL_headerBox::down-arrow { image: url(:/inputEditor/icons/inputEditor/vArrowDown.svg); }");
    m_animationSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_animationSelector->setFixedHeight(24);

    m_addPropertyButton->setText(QObject::tr("Add Property"));
    m_addPropertyButton->setIcon(Nau::Theme::current().iconAddPrimaryStyle());
    m_addPropertyButton->setContentsMargins(8, 4, 8, 4);
    m_addPropertyButton->setFixedSize(BUTTON_SIZE);
    m_addPropertyButton->setRound(BUTTON_ROUND);

    auto createToolButton = [this, &BUTTON_STYLE_SHEET, &icon]() {
        auto* button = new NauToolButton(this);
        button->setIcon(icon);
        button->setStyleSheet(BUTTON_STYLE_SHEET);
        return button;
    };
    auto&& icons = Nau::Theme::current().iconsTimelineTrackListHeader();
    NED_ASSERT(icons.size() >= 2);

    auto* addKeyFrameButton = createToolButton();
    addKeyFrameButton->setIcon(icons[0]);
    addKeyFrameButton->setToolTip(tr("Add keyframe"));
    auto* addEventButton = createToolButton();
    addEventButton->setIcon(icons[1]);
    addEventButton->setDisabled(true);

    auto* container = new NauWidget(this);
    container->setFixedSize(TOOLS_CONTAINER_SIZE);
    container->setLayout(new NauLayoutHorizontal);
    container->layout()->setSpacing(4);
    container->layout()->addWidget(addKeyFrameButton);
    container->layout()->addWidget(addEventButton);

    auto* layout = new NauLayoutHorizontal;
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(24);
    layout->addWidget(m_animationSelector);
    layout->addWidget(container);
    layout->addWidget(m_addPropertyButton);

    setLayout(layout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(48);

    connect(m_addPropertyButton, &NauAbstractButton::clicked, [this] {
        if (m_propertyListMenu->children().empty()) {
            return;
        }
        const auto parentWidgetPosition = m_addPropertyButton->mapToGlobal(QPointF(0, 0)).toPoint();
        const auto correctWidgetPosition = Nau::Utils::Widget::fitWidgetIntoScreen(m_propertyListMenu->sizeHint(), parentWidgetPosition);
        m_propertyListMenu->popup(correctWidgetPosition);
    });
    connect(m_animationSelector, &QComboBox::currentIndexChanged, this, &NauTimelineTrackListHeader::eventClipSwitched);
    connect(addKeyFrameButton, &NauAbstractButton::clicked, this, &NauTimelineTrackListHeader::eventAddKeyframe);

    setDisabled(true);
}

void NauTimelineTrackListHeader::setProperties(NauAnimationPropertyListPtr properties)
{
    m_properties = std::move(properties);
    setDisabled(m_properties->empty());
    updatePropertyList();
}

void NauTimelineTrackListHeader::setClipNameList(const NauAnimationNameList& nameList, int currentNameIndex)
{
    QSignalBlocker blocker{ m_animationSelector };
    m_animationSelector->clear();
    for (const std::string& name: nameList) {
        m_animationSelector->addItem(name.c_str());
    }
    m_animationSelector->setCurrentIndex(currentNameIndex);
}

std::string NauTimelineTrackListHeader::selectedClipName() const
{
    return std::string{ m_animationSelector->currentText().toUtf8().constData() };
}

void NauTimelineTrackListHeader::paintEvent(QPaintEvent* event)
{
    const NauPalette palette = Nau::Theme::current().paletteTimelineTrackList();
    QPainter painter{ this };
    painter.fillRect(QRectF(0, 0, width(), height()), palette.color(NauPalette::Role::BackgroundHeader));
}

void NauTimelineTrackListHeader::updatePropertyList()
{
    m_propertyListMenu->clear();

    int propertyIndex = -1;
    m_properties->forEach([this, &propertyIndex](const NauAnimationProperty& property) {
        ++propertyIndex;
        if (!property.selected()) {
            auto* action = m_propertyListMenu->addAction(property.name().c_str());
            connect(action, &QAction::triggered, [this, propertyIndex]() {
                emit eventAddedProperty(propertyIndex);
            });
        }
    });
}


// ** NauTimelineTrackList

NauTimelineTrackList::NauTimelineTrackList(NauWidget* parent)
    : NauWidget(parent)
    , m_header(new NauTimelineTrackListHeader(this))
    , m_propertiesTree(new NauTimelineTreeWidget(this))
    , m_currentOptionItem(nullptr)
    , m_selectedPropertyItem(nullptr)
{
    const auto& theme = Nau::Theme::current();
    const auto palette = theme.paletteTimelineTrackList();

    QPalette qtPalette;
    qtPalette.setBrush(QPalette::Base, palette.brush(NauPalette::Role::Background));
    qtPalette.setBrush(QPalette::AlternateBase, palette.brush(NauPalette::Role::AlternateBackground));
    qtPalette.setColor(QPalette::Text, palette.color(NauPalette::Role::Foreground));

    auto* delegate = new NauTreeViewItemDelegate(m_propertiesTree);
    delegate->setPalette(palette);
    delegate->setColumnHighlighted(NAME_COLUMN_INDEX);
    delegate->setRowHeight(32);
    delegate->setRowContentsMargins(16, 8, 8, 8);
    delegate->setCellContentsMargins(4, 0, 4, 0);
    delegate->setSpacing(8);
    delegate->setIndentation(32);
    delegate->setRootAffect(true);
    delegate->setRootColumn(2);

    m_propertiesTree->setItemDelegate(delegate);
    m_propertiesTree->setPalette(qtPalette);
    m_propertiesTree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_propertiesTree->setFrameShape(QFrame::NoFrame);
    m_propertiesTree->setStyleSheet("background-color: #00000000; padding: 0px");
    m_propertiesTree->setMouseTracking(true);
    m_propertiesTree->setHeaderHidden(true);
    m_propertiesTree->setColumnCount(5);
    m_propertiesTree->setIndentation(0);
    m_propertiesTree->header()->setStretchLastSection(false);

    auto* layout = new NauLayoutVertical;
    layout->addWidget(m_header);
    layout->addWidget(m_propertiesTree);

    setLayout(layout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(m_propertiesTree, &NauTimelineTreeWidget::eventItemHover, [this](const QModelIndex& index) {
        auto* eventItem = static_cast<NauTreeWidgetItem*>(m_propertiesTree->itemFromIndex(index));
        if (eventItem == m_currentOptionItem) {
            return;
        }
        if (auto it = m_options.find(m_currentOptionItem); it != m_options.end()) {
            it->second->hide();
        }
        if (auto it = m_options.find(eventItem); it != m_options.end()) {
            it->second->show();
        }
        m_currentOptionItem = eventItem;
    });
    connect(m_propertiesTree, &NauTimelineTreeWidget::itemPressed, [this](QTreeWidgetItem* item) {
        if (item == nullptr) {
            m_selectedPropertyItem = nullptr;
            return;
        }
        auto it = m_propertyDictionary.find(item);
        if (it == m_propertyDictionary.end()) {
            it = m_propertyDictionary.find(static_cast<NauTreeWidgetItem*>(item->parent()));
        }
        m_selectedPropertyItem = it->first;
    });
    connect(m_propertiesTree, &NauTimelineTreeWidget::itemCollapsed, [this](QTreeWidgetItem* item) {
        emit eventItemExpanded(m_propertyDictionary.at(item), false);
    });
    connect(m_propertiesTree, &NauTimelineTreeWidget::itemExpanded, [this](QTreeWidgetItem* item) {
        emit eventItemExpanded(m_propertyDictionary.at(item), true);
    });
    connect(m_header, &NauTimelineTrackListHeader::eventAddKeyframe, [this] {
        if (m_selectedPropertyItem == nullptr) {
            return;
        }
        emit eventAddKeyframe(m_propertyDictionary.at(m_selectedPropertyItem));
    });
}

void NauTimelineTrackList::setCurrentTime(float time)
{
    for (auto& handler : m_trackHandlerList) {
        handler->updateValue(time);
    }
}

void NauTimelineTrackList::updateTrackList(NauAnimationPropertyListPtr propertyList, float time)
{
    const bool refilled = propertyList->refilled();
    m_header->setProperties(propertyList);
    if (refilled) {
        for (auto& handler : m_trackHandlerList) {
            handler->setBlockEditingSignals(true);
        }
        m_propertiesTree->clear();
        m_selectedPropertyItem = nullptr;
        m_propertyDictionary.clear();
        m_trackHandlerList.clear();
        m_options.clear();
    }

    int propertyIndex = -1;
    int itemIndex = 0;
    propertyList->forEach([this, &propertyIndex, &itemIndex](const NauAnimationProperty& property) {
        ++propertyIndex;
        if (property.selected()) {
            addTrack(&property, propertyIndex, itemIndex++);
        }
    });
    setCurrentTime(time);
}

NauTimelineTrackListHeader& NauTimelineTrackList::headerWidget() noexcept
{
    NED_ASSERT(m_header != nullptr);
    return *m_header;
}

void NauTimelineTrackList::addTrack(const NauAnimationProperty* property, int propertyIndex, int index)
{
    const NauAnimationPropertyListPtr propertyList = m_header->propertyList();
    auto propertyChecker = [propertyIndex](const auto& pair) {
        return propertyIndex == pair.second;
    };
    if (std::any_of(m_propertyDictionary.begin(), m_propertyDictionary.end(), propertyChecker)) {
        return;
    }

    NauTimelineTrackHandler* handler = nullptr;
    // TODO: Make abstract factory
    if (property->type() == NauAnimationTrackDataType::Vec3) {
        handler = m_trackHandlerList.emplace_back(std::make_unique<NauTimelineTrackVec3Handler>(m_propertiesTree, propertyIndex, property)).get();
    } else if (property->type() == NauAnimationTrackDataType::Quat) {
        handler = m_trackHandlerList.emplace_back(std::make_unique<NauTimelineTrackQuatHandler>(m_propertiesTree, propertyIndex, property)).get();
    }
    if (handler == nullptr) {
        return;
    }
    auto* treeItem = handler->createTreeItem(nullptr, property->name().c_str());
    m_propertiesTree->insertTopLevelItem(index, treeItem);

    // todo: get icon from figma
    const QPixmap OPTION_ICON{ ":/animationEditor/icons/animationEditor/vOption.svg" };
    auto* option = new NauMiscButton(m_propertiesTree);
    option->setFixedSize(16, 16);
    option->setIcon(OPTION_ICON);
    handler->addWidgetToItem(treeItem, OPTION_COLUMN_INDEX, option);
    m_options[treeItem] = option;

    auto* list = new NauMenu();
    list->setParent(option);
    connect(list->addAction(tr("Delete property")), &QAction::triggered, [this, propertyIndex]() {
        emit eventDeleteProperty(propertyIndex);
    });
    connect(option, &NauAbstractButton::clicked, [list] {
        const QPoint parentWidgetPosition = list->mapToGlobal(QPointF(0, 0)).toPoint();
        const QPoint correctWidgetPosition = Nau::Utils::Widget::fitWidgetIntoScreen(list->sizeHint(), parentWidgetPosition);
        list->popup(correctWidgetPosition);
    });

    connect(handler, &NauTimelineTrackHandler::eventEditingFinished, this, &NauTimelineTrackList::eventPropertyChanged);

    m_propertyDictionary.emplace(treeItem, propertyIndex);

    emit eventItemExpanded(propertyIndex, treeItem->isExpanded());
}

void NauTimelineTrackList::paintEvent(QPaintEvent* event)
{
    const NauPalette palette = Nau::Theme::current().paletteTimelineTrackList();
    QPainter painter{ this };
    painter.fillRect(QRectF(0, 0, width(), height()), palette.color(NauPalette::Role::Background));
}

void NauTimelineTrackList::resizeEvent(QResizeEvent* event)
{
    NauWidget::resizeEvent(event);
    if (!event->size().isValid()) {
        return;
    }
    auto* scroll = m_propertiesTree->verticalScrollBar();
    const bool isShown = scroll->isVisible();
    const int scrollWidth = isShown * scroll->width();
    const int newWidth = event->size().width();
    const int nameColumnWidth = newWidth - scrollWidth - FIXED_FILLED_COLUMN_WIDTH;
    m_propertiesTree->setColumnWidth(VISIBLE_COLUMN_INDEX, VISIBLE_COLUMN_FIXED_WIDTH);
    m_propertiesTree->setColumnWidth(LOCK_COLUMN_INDEX, LOCK_COLUMN_FIXED_WIDTH);
    m_propertiesTree->setColumnWidth(NAME_COLUMN_INDEX, nameColumnWidth);
    m_propertiesTree->setColumnWidth(VALUE_COLUMN_INDEX, VALUE_COLUMN_FIXED_WIDTH);
    m_propertiesTree->setColumnWidth(OPTION_COLUMN_INDEX, OPTION_COLUMN_FIXED_WIDTH);
}
