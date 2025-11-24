// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/outliner/nau_world_outline_panel.hpp"
#include "nau/outliner/nau_outliner_drag_context.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"
#include "nau_widget_utility.hpp"
#include "nau_buttons.hpp"
#include "themes/nau_theme.hpp"
#include "nau_utils.hpp"

#include <QPainter>


// TODO: Use the data from NauTheme instead

// ** NauWorldOulineItemContextMenu

NauWorldOulineItemContextMenu::NauWorldOulineItemContextMenu(const QString& title, NauWidget* parent) :
    NauMenu(title, parent)
{
}


// ** NauWorldOutlineHeaderView

NauWorldOutlineHeaderView::NauWorldOutlineHeaderView(Qt::Orientation orientation, QWidget* parent)
    : NauHeaderView(orientation, parent)
{
    setContentMargin(8, 8, 4, 8);
    setPalette(Nau::Theme::current().paletteWorldOutline());
}

QSize NauWorldOutlineHeaderView::sortIndicatorSize() const
{
    return QSize(16, 16);
}


// ** NauWorldOutlineTableWidget

NauWorldOutlineTableWidget::NauWorldOutlineTableWidget(NauShortcutHub* shortcutHub, NauWidget* parent)
    : NauTreeWidget(parent)
    , m_shortcutHub(std::move(shortcutHub))
    , m_header(new NauWorldOutlineHeaderView(Qt::Orientation::Horizontal))
{
    setStyleSheet(styleSheet());

    setObjectName("worldOutlineTableWidget");
    setFocusPolicy(Qt::StrongFocus);
    setDragEnabled(true);
    setAcceptDrops(true);

    const auto& theme = Nau::Theme::current();
    const auto& palette = theme.paletteWorldOutline();

    m_availabilityIcon = theme.iconAvailability();
    m_visibilityIcon   = theme.iconVisibility();
    m_header->setObjectName("outlineHeaderHorizontal");
    m_header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_header->setFixedHeight(32);
    m_header->setColumnHighlighted(+Columns::Name);

    setHeader(m_header);
    setColumnCount(magic_enum::enum_count<Columns>());

    // We pass palette colors from our theme to Qt to make it draw widgets according the current theme.
    // But not all features from theme are supported by QPalette. We had to implement them via qss and coding.
    // This translation looks like redundant and hack and should be removed as soon as outline has own paint routines.
    QPalette qtPalette;
    qtPalette.setBrush(QPalette::Base, palette.brush(NauPalette::Role::Background));
    qtPalette.setBrush(QPalette::AlternateBase, palette.brush(NauPalette::Role::AlternateBackground));
    qtPalette.setColor(QPalette::Text, palette.color(NauPalette::Role::Foreground));
    setPalette(qtPalette);

    // Columns with permanent visibility are customized through the methods of the parent class
    // TODO: put back once implemented
    // setupColumn(+Columns::Visibility, "", theme.fontWorldOutlineHeaderRegular(), Nau::paintPixmap(":/UI/icons/iVisible.png", QColor(128, 128, 128)), true, 24, QHeaderView::Fixed, tr("Visibility"));
    setupColumn(+Columns::Disabled, "", theme.fontWorldOutlineHeaderRegular(), Nau::paintPixmap(":/UI/icons/iLocked.png", QColor(128, 128, 128)), true, 24, QHeaderView::Fixed, tr("Editable"));
    setupColumn(+Columns::Name, tr("Name"), theme.fontWorldOutlineHeaderSemibold(), true, 256);

    // The others are customized by their own overridden methods
    setupColumn(+Columns::Type, tr("Type"), theme.fontWorldOutlineHeaderRegular(), true, 256);
    // setupColumn(+Columns::Modified, tr("Modified"), theme.fontWorldOutlineHeaderRegular(), false);
    // setupColumn(+Columns::Tags, tr("Tags"), theme.fontWorldOutlineHeaderRegular(), false);
    // setupColumn(+Columns::Layer, tr("Layer"), theme.fontWorldOutlineHeaderRegular(), false, 50);
    setupColumn(+Columns::Guid, tr("Guid"), theme.fontWorldOutlineHeaderRegular(), false, 150, QHeaderView::Stretch);

    m_delegate = new NauTreeViewItemDelegate;
    m_delegate->setPalette(palette);
    m_delegate->setColumnHighlighted(+Columns::Name);
    m_delegate->setInteractableColumn(+Columns::Disabled);
    // m_delegate->setInteractableColumn(+Columns::Visibility);
    m_delegate->setRootColumn(+Columns::Name);
    m_delegate->setEditableColumn(+Columns::Name);
    m_delegate->setRowHeight(32);
    m_delegate->setRowContentsMargins(16, 8, 16, 8);
    m_delegate->setCellContentsMargins(4, 0, 4, 0);
    m_delegate->setRootAffect(true);
    m_delegate->setSpacing(8);
    m_delegate->setIndentation(48);

    setItemDelegate(m_delegate);

    // Note that we clear indentation of tree itself, for our delegate can handle this.
    setIndentation(0);

    // m_header->setSectionHideable(+Columns::Disabled, false);
    // m_header->setSectionHideable(+Columns::Visibility, false);

    // Setup context menu
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &NauWorldOutlineTableWidget::customMenuRequested);

    // Setup shortcuts
    // If the widget is not in focus, its hot-keys will not work!

    //m_shortcutHub->addWidgetShortcut(NauShortcutOperation::WorldOutlineCut, *this, 
    //    std::bind(&NauWorldOutlineTableWidget::cutItems, this));

    m_shortcutHub->addWidgetShortcut(NauShortcutOperation::WorldOutlineCopy, *this, 
        std::bind(&NauWorldOutlineTableWidget::copyItems, this));

    //m_shortcutHub->addWidgetShortcut(NauShortcutOperation::WorldOutlinePaste, *this, 
    //    std::bind(&NauWorldOutlineTableWidget::pasteItems, this));

    m_shortcutHub->addWidgetShortcut(NauShortcutOperation::WorldOutlineDuplicate, *this, 
        std::bind(&NauWorldOutlineTableWidget::duplicateItems, this));
    
    m_shortcutHub->addWidgetShortcut(NauShortcutOperation::WorldOutlineDelete, *this, 
        std::bind(&NauWorldOutlineTableWidget::deleteItems, this));

    m_shortcutHub->addWidgetShortcut(NauShortcutOperation::WorldOutlineRename, *this, 
        std::bind(&NauWorldOutlineTableWidget::renameItems, this));

    m_shortcutHub->addWidgetShortcut(NauShortcutOperation::WorldOutlineFocusCamera, *this, 
        std::bind(&NauWorldOutlineTableWidget::focusOnItem, this));

    m_shortcutHub->addWidgetShortcut(NauShortcutOperation::WorldOutlineSelectAll, *this, 
        std::bind(&NauWorldOutlineTableWidget::selectAll, this));

    // Setup row item change
    connect(this, &QTreeWidget::itemChanged, this, &NauWorldOutlineTableWidget::onItemChanged);

    connect(m_header, &NauHeaderView::eventColumnVisibleToggled, this, [this](int logicalIndex, bool visible) {
        std::vector<int> visibleColums;
        for (int column = 0; column < columnCount(); column++) {
            if (!isColumnHidden(column)) {
                visibleColums.push_back(column);
            }
        }
        emit eventColumnsVisibilityChanged(visibleColums);
    });

    connect(m_delegate, &NauTreeViewItemDelegate::buttonEventPressed, this, [this](const QModelIndex& index, int logicalIndex) {
        auto item = itemFromIndex(index);

        // TODO: put back once implemented
        // const bool availabilityToggleRequested = logicalIndex == +Columns::Disabled || logicalIndex == +Columns::Visibility;
        const bool availabilityToggleRequested = logicalIndex == +Columns::Disabled;
        const bool itemDisabled = !item->flags().testFlag(Qt::ItemIsEnabled);
        const bool parentItemDisabled = item->parent() && !item->parent()->flags().testFlag(Qt::ItemIsEnabled);

        if (itemDisabled && (!availabilityToggleRequested || parentItemDisabled)) {
            return;
        }

        // TODO: put back once implemented
        // if ((logicalIndex == +Columns::Disabled) || (logicalIndex == +Columns::Visibility)) {
        if (logicalIndex == +Columns::Disabled) {
            changeAllChildredCheckState(item, logicalIndex, item->checkState(logicalIndex) == Qt::CheckState::Checked ? Qt::CheckState::Unchecked : Qt::CheckState::Checked);
            m_delegate->setInteractiveColumnVisible(index, item->checkState(logicalIndex) == Qt::CheckState::Checked ? true : false);
        } else if (logicalIndex == +Columns::Name) {
            item->setExpanded(!item->isExpanded());
        } else {
            item->setExpanded(!item->isExpanded());
        }
    });

    connect(m_delegate, &NauTreeViewItemDelegate::buttonRenamePressed, this, [this](const QModelIndex& index, int logicalIndex) {

        // It looks like a hack, but it's not really.
        // We memorize the index in the model of the element we clicked on.
        // And if it has not changed, we perform the renaming action.
        static QModelIndex previousSelectedItem;

        if (previousSelectedItem.isValid() && previousSelectedItem == index) {
            renameItems();
        }

        previousSelectedItem = index;

        });
}

void NauWorldOutlineTableWidget::changeAllChildredCheckState(QTreeWidgetItem* item, int logicalIndex, Qt::CheckState state)
{
    // Do something with item
    item->setCheckState(logicalIndex, state);
    if (item->parent()) {
        if (state == Qt::Unchecked || state != item->parent()->checkState(logicalIndex)) {
            item->setIcon(logicalIndex, logicalIndex == +Columns::Disabled
                ? Nau::Theme::current().iconAvailability()
                : Nau::Theme::current().iconVisibility());
        } else {
            item->setIcon(logicalIndex, Nau::Theme::current().iconInvisibleChild());
        }
    }

    auto flags = item->flags();
    flags.setFlag(Qt::ItemIsEnabled,
        item->checkState(+Columns::Disabled) == Qt::Unchecked);
        // TODO: put back once implemented
        // && item->checkState(+Columns::Visibility) == Qt::Unchecked);

    item->setFlags(flags);

    m_delegate->setInteractiveColumnVisible(indexFromItem(item), false);

    for (int i = 0; i < item->childCount(); ++i) {
        changeAllChildredCheckState(item->child(i), logicalIndex, state);
    }
}

void NauWorldOutlineTableWidget::setColumnVisibility(std::vector<int> visibleColumns)
{
    for (int columnIndex = 0; columnIndex < columnCount(); ++columnIndex) {
        const bool visible = std::find(visibleColumns.begin(), visibleColumns.end(), columnIndex) != visibleColumns.end();
        setColumnHidden(columnIndex, !visible);
    }
}

void NauWorldOutlineTableWidget::clearTable()
{
    clear();
}

const int NauWorldOutlineTableWidget::getCurrentRowIndex() const
{
    if (const auto item = this->currentItem(); item) {
        return indexOfTopLevelItem(item);
    } else {
        NED_ASSERT("There is no object in the given table row!");
        return NauObjectGUID::invalid();
    }
}

const QModelIndexList NauWorldOutlineTableWidget::getSelectedRowsWithGuids() const
{
    // Get all cells of the selected rows that contain unique entity identifiers
    return this->selectionModel()->selectedRows(+Columns::Guid);
}

void NauWorldOutlineTableWidget::customMenuRequested(QPoint pos)
{
    auto menu = new NauWorldOulineItemContextMenu(tr("Edit"), dynamic_cast<NauWidget*>(parentWidget()));

    // Create actions
    auto actionCut = menu->addAction(Nau::Theme::current().iconCut(), tr("Cut"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::WorldOutlineCut), this, &NauWorldOutlineTableWidget::cutItems);

    auto actionCopy = menu->addAction(Nau::Theme::current().iconCopy(), tr("Copy"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::WorldOutlineCopy), this, &NauWorldOutlineTableWidget::copyItems);

    auto actionPaste = menu->addAction(Nau::Theme::current().iconPaste(), tr("Paste"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::WorldOutlinePaste), this, &NauWorldOutlineTableWidget::pasteItems);

    // TODO: It's going down
    auto actionDuplicate = menu->addAction(Nau::Theme::current().iconDuplicate(), tr("Duplicate"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::WorldOutlineDuplicate), this, &NauWorldOutlineTableWidget::duplicateItems);

    auto actionDelete = menu->addAction(Nau::Theme::current().iconDelete(), tr("Delete"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::WorldOutlineDelete), this, &NauWorldOutlineTableWidget::deleteItems);

    auto actionRename = menu->addAction(Nau::Theme::current().iconRename(), tr("Rename"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::WorldOutlineRename), this, &NauWorldOutlineTableWidget::renameItems);

    auto actionFocus = menu->addAction(Nau::Theme::current().iconFocusCameraOnObject(), tr("Focus Camera"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::WorldOutlineFocusCamera), this, &NauWorldOutlineTableWidget::focusOnItem);

    auto actionSelectAll = menu->addAction(Nau::Theme::current().iconFocusCameraOnObject(), tr("Select All"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::WorldOutlineSelectAll), this, &NauTableWidget::selectAll);

    // Enable actions
    const bool anyObjectsSelected = selectedItems().size() > 0;
    const bool singleObjectSelected = getSelectedRowsWithGuids().size() == 1;

    // TODO: Enable actions in outliner client?
    // TODO: Fix cut operation
    actionCut->setEnabled(false);
    actionCopy->setEnabled(anyObjectsSelected);
    actionPaste->setEnabled(true/*!m_entityBuffer.empty()*/);
    // TODO: It's going down
    actionDuplicate->setEnabled(anyObjectsSelected);
    actionDelete->setEnabled(anyObjectsSelected);
    actionRename->setEnabled(singleObjectSelected);
    actionFocus->setEnabled(singleObjectSelected);

    // Show the menu
    menu->popup(viewport()->mapToGlobal(pos));
}

bool NauWorldOutlineTableWidget::cutItems()
{
    const bool copyResult = copyItems();
    const bool deleteResult = deleteItems();

    return copyResult && deleteResult;
}

bool NauWorldOutlineTableWidget::copyItems()
{
    emit eventCopy();

    return true;
}

bool NauWorldOutlineTableWidget::pasteItems()
{
    emit eventPaste();

    return true;
}

bool NauWorldOutlineTableWidget::duplicateItems()
{
    // TODO: It's going down
    emit eventDuplicate();

    return true;
}

bool NauWorldOutlineTableWidget::deleteItems()
{
    emit eventDelete();

    return true;
}

bool NauWorldOutlineTableWidget::renameItems()
{
    if (!currentItem()->flags().testFlag(Qt::ItemIsEnabled)) {
        NED_DEBUG("Outliner: Ignoring an attempt to edit disabled item");
        return false;
    }
    m_nameBeforeRenaming = currentItem()->text(+Columns::Name);
    this->editItem(currentItem(), +Columns::Name);

    return true;
}

bool NauWorldOutlineTableWidget::focusOnItem()
{
    emit eventFocus(currentItem());
    return true;
}

void NauWorldOutlineTableWidget::onItemChanged(QTreeWidgetItem* item)
{ 
    // Renaming action
    QString newObjectName = item->text(+Columns::Name);
    if (newObjectName.isEmpty()) {
        newObjectName = m_nameBeforeRenaming;
        item->setText(+Columns::Name, m_nameBeforeRenaming);
    }

    emit eventRename(item, newObjectName);
}

void NauWorldOutlineTableWidget::mousePressEvent(QMouseEvent* event)
{
    // It is necessary to check that when you click under the cursor,
    // something is located. When clicking on an empty space,
    // the variable will become invalid.
    if (!indexAt(event->pos()).isValid()) {
        this->clearSelection();
        return;
    }

    QTreeWidget::mousePressEvent(event);
}

void NauWorldOutlineTableWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    // It is necessary to check that when you click under the cursor,
    // something is located. When clicking on an empty space,
    // the variable will become invalid.
    if (!indexAt(event->pos()).isValid()) {
        this->clearSelection();
        return;
    }

    // If you double right click, there may be an error as there will be no selected rows
    if (event->button() == Qt::LeftButton) {
        focusOnItem();
    }
}

void NauWorldOutlineTableWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (const auto& outlineCtx = NauOutlinerDragContext::fromMimeData(*event->mimeData())) {
        event->setAccepted(!outlineCtx->guids().empty());
        return;
    }

    NauTreeWidget::dragEnterEvent(event);
}

void NauWorldOutlineTableWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (const auto& outlineCtx = NauOutlinerDragContext::fromMimeData(*event->mimeData())) {
        const QModelIndex index = indexAt(event->pos());

        if (!index.isValid()) {
            event->accept();
            return;
        }

        const auto destinationGuid = itemFromIndex(index)->text(+Columns::Guid);
        const auto& movingGuids = outlineCtx->guids();

        // User tries to drop item on itself.
        auto itGuid = std::find(movingGuids.begin(), movingGuids.end(), destinationGuid);

        event->setAccepted(itGuid == movingGuids.end());
        return;
    }

    NauTreeWidget::dragMoveEvent(event);
}

void NauWorldOutlineTableWidget::dropEvent(QDropEvent* event)
{
     if (const auto& outlineCtx = NauOutlinerDragContext::fromMimeData(*event->mimeData())) {
        event->accept();

        emit eventMove(indexAt(event->pos()), outlineCtx->guids());
        return;
    }

    NauTreeWidget::dropEvent(event);
}

QMimeData* NauWorldOutlineTableWidget::mimeData(const QList<QTreeWidgetItem*>& items) const
{
    std::vector<QString> dragGuids;
    for (const auto& item : items) {
        const bool itemDraggable = item->flags().testFlag(Qt::ItemIsDragEnabled) &&
            // To-Do: Support grag&drop for nested items
            item->childCount() == 0;
        
        if (itemDraggable) {
            dragGuids.push_back(item->text(+Columns::Guid));
        }
    }

    const NauOutlinerDragContext context{ dragGuids };
    auto mime = new QMimeData;
    context.toMimeData(*mime);

    return mime;
}

// TODO:
// - At the moment there are performance issues. 
// - Not enough code comments
// - Create a separate static class for generic methods and move that method there
// - Cover with unit-tests
std::string NauWorldOutlineTableWidget::generateNumericalDisplayName(const std::string& entityName)
{
    // It's not working at the moment!

    // The entityName is built according to the following formula: entityNameBody + "_" + entityNameIndex
    // Next, using the formula, parse displayName and get the necessary information:
    std::string entityNameBody = entityName;
    std::reverse(entityNameBody.begin(), entityNameBody.end());

    const std::string delimiter = "_";
    std::string entityNameIndex = entityNameBody.substr(0, entityNameBody.find(delimiter));

    int index = std::atoi(entityNameIndex.c_str());

    // The first object with this name will be without a numeric index
    if (index != 0) {
        entityNameBody.erase(0, entityNameBody.find(delimiter) + delimiter.length());
    } else {
        ++index;
    }

    std::reverse(entityNameBody.begin(), entityNameBody.end());
    std::reverse(entityNameIndex.begin(), entityNameIndex.end());

    // Looking for the nearest free index from "index"
    std::string indexedEntityName = entityNameBody;

    // TODO: Uncomment at the moment when it will be rewritten to work with the table
    
    //QList<QTreeWidgetItem*> items = this->findItems(QString(indexedEntityName.c_str()), Qt::MatchFlag::MatchContains);

    //while (!items.isEmpty()) {
    //    // TODO: Study the performance of std::format and use it instead of string concatenation if it performs well
    //    indexedEntityName = entityNameBody + "_" + std::to_string(index);
    //    items = this->findItems(QString(indexedEntityName.c_str()), Qt::MatchFlag::MatchContains);
    //    ++index;
    //}

    // The data model will change automatically
    return indexedEntityName;
}


// ** NauWorldOutlinerWidgetHeader

#include "nau_content_creator.hpp"

NauWorldOutlinerWidgetHeader::NauWorldOutlinerWidgetHeader(NauWidget* parent)
    : m_objectCreationList(nullptr)
{
    setFixedHeight(Height);
    setFocusPolicy(Qt::StrongFocus);

    const auto& theme = Nau::Theme::current();

    auto layout = new NauLayoutHorizontal(this);
    layout->setContentsMargins(QMargins(OuterMargin, OuterMargin, OuterMargin, OuterMargin));
    layout->setSpacing(16);

    // Filter Widget
    auto filterWidget = new NauFilterWidget(nullptr, this);
    layout->addWidget(filterWidget, Qt::AlignLeft);

    for (auto column : magic_enum::enum_names<NauWorldOutlineTableWidget::Columns>()) {
        filterWidget->addFilterParam(theme.iconResourcePlaceholder(), column.data(), true);  // Filters at first startup are enabled
    }

    for (auto enumm : magic_enum::enum_values<NauWorldOutlineTableWidget::Columns>()) {
        m_currentFilters.push_back(enumm);
    }

    filterWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(filterWidget, &NauFilterWidget::eventChangeFilterRequested, [this](const QList<NauFilterWidgetAction*>& filters) {
        m_currentFilters.clear();

        for (NauFilterWidgetAction* filter : filters) {
            auto columnEnumValue = magic_enum::enum_cast<NauWorldOutlineTableWidget::Columns>(filter->text().toUtf8().constData());
            m_currentFilters.push_back(columnEnumValue.value());
        }

     });

    // Search Widget
    auto searchWidget = new NauSearchWidget(this);
    layout->addWidget(searchWidget);
    searchWidget->setPlaceholderText(tr("Search in outliner..."));
    connect(searchWidget, &QLineEdit::textChanged, this, &NauWorldOutlinerWidgetHeader::eventSearchFilterChanged);

    // Add Button
    m_addButton = new NauPrimaryButton();
    m_addButton->setIcon(Nau::Theme::current().iconAddPrimaryStyle());
    m_addButton->setFixedHeight(NauAbstractButton::standardHeight());
    layout->addWidget(m_addButton);

    setTabOrder(searchWidget, m_addButton);

    m_objectCreationList = new NauObjectCreationList(nullptr);
    connect(m_objectCreationList, &NauObjectCreationList::eventCreateObject, [this](const std::string& typeName) {
        emit eventCreateObject(typeName);
    });

    connect(m_addButton, &NauAbstractButton::clicked, [=]
    {
        auto objectCreationList = creationList();
        if (objectCreationList && m_addButton) {
            const auto parentWidgetPosition = m_addButton->mapToGlobal(QPointF(0, 0)).toPoint();
            const auto correctWidgetPosition = Nau::Utils::Widget::fitWidgetIntoScreen(objectCreationList->sizeHint(), parentWidgetPosition);
            objectCreationList->popup(correctWidgetPosition);
        }
    });
}

void NauWorldOutlinerWidgetHeader::singleObjectCreation(const std::string& objectName)
{
    disconnect(m_addButton);
    connect(m_addButton, &NauAbstractButton::clicked, [this, objectName] 
    {
        emit eventCreateObject(objectName);
    });
}

const std::vector<NauWorldOutlineTableWidget::Columns>& NauWorldOutlinerWidgetHeader::currentFilters()
{
    return m_currentFilters;
}

NauObjectCreationList* NauWorldOutlinerWidgetHeader::creationList() const
{
    return m_objectCreationList;
}


void NauWorldOutlinerWidgetHeader::updateCreationList(const std::string& typeName, bool state) const
{
    m_objectCreationList->updateTypeList(typeName, state);
}

// ** NauWorldOutlinerWidget

NauWorldOutlinerWidget::NauWorldOutlinerWidget(NauShortcutHub* shortcutHub, QWidget* parent)
    : NauWidget(parent)
    , m_mainLayout(new NauLayoutVertical(this))
    , m_tableTab(nullptr)
    , m_header(new NauWorldOutlinerWidgetHeader(this))
{
    m_shortcutHub = shortcutHub;

    m_mainLayout->addWidget(m_header);

    m_stackedLayout = new NauLayoutStacked();
    m_mainLayout->addLayout(m_stackedLayout);

    createOutlinerTab();
    m_stackedLayout->addWidget(m_tableTab);

    connect(m_header, &NauWorldOutlinerWidgetHeader::eventSearchFilterChanged, this, &NauWorldOutlinerWidget::updateFilterData);

    setTabOrder(m_header, m_tableTab);

    // TODO Add looping of header->table->header widgets
    // https://doc.qt.io/qt-6/qwidget.html (focusNextPrevChild)
}

NauWorldOutlinerWidgetHeader& NauWorldOutlinerWidget::getHeaderWidget() const
{
    return *m_header;
}

void NauWorldOutlinerWidget::updateCreationList(const std::string& objectTypeName, bool state)
{
    m_header->updateCreationList(objectTypeName, state);
}

NauWorldOutlineTableWidget& NauWorldOutlinerWidget::outlinerTab()
{
    return *m_tableTab;
}

void NauWorldOutlinerWidget::updateFilterData(const QString& filter)
{
    // TODO: In the future to do it through: QSortFilterProxyModel
    // The column is irrelevant. Here we just take all elements from the table.
    auto itemsList = m_tableTab->findItems("", Qt::MatchContains, +NauWorldOutlineTableWidget::Columns::Name);

    // First, let's hide all the elements of the table
    for (QTreeWidgetItem* item : itemsList) {
        item->setHidden(true);
    }

    // And then for each open filter, we'll open element by element.
    for (auto columns : m_header->currentFilters()) {
        auto matchingItemsList = m_tableTab->findItems(filter, Qt::MatchContains, +columns);

        for (auto item : matchingItemsList) {
            // Thus, if at least one filter has a match, the item should be displayed.
            item->setHidden(false); 
        }
    }
}

void NauWorldOutlinerWidget::createOutlinerTab()
{
    m_tableTab = new NauWorldOutlineTableWidget(m_shortcutHub, this);
    m_stackedLayout->addWidget(m_tableTab);
}
