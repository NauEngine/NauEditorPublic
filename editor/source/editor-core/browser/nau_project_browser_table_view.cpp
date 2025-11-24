// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_project_browser_table_view.hpp"
#include "nau_project_browser_proxy_models.hpp"
#include "nau_header_view.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"
#include "themes/nau_theme.hpp"

#include <QDrag>
#include <QPainter>
#include <QPaintEvent>


// ** NauProjectBrowserListView

NauProjectBrowserTableView::NauProjectBrowserTableView(std::shared_ptr<NauProjectBrowserFileOperationsMenu> fileMenu, NauSplitter* parent)
    : NauTreeView(parent)
    , m_fileOperationMenu(std::move(fileMenu))
    , m_noFilterResultMessage(tr("There are no results matching the specified filters"))
{
    NED_ASSERT(m_fileOperationMenu);

    auto header = new NauHeaderView(Qt::Horizontal, this);
    header->setPalette(Nau::Theme::current().paletteLogger());
    
    header->setObjectName("outlineHeaderHorizontal");
    header->setFont(Nau::Theme::current().fontWorldOutlineHeaderRegular());
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setFixedHeight(32);
    header->setStretchLastSection(true);
    header->setCascadingSectionResizes(true);
    header->setMinimumSectionSize(40);
    header->setSectionHideable(+NauProjectBrowserFileSystemModel::Column::RelativePath, false);

    connect(header, &NauHeaderView::sectionClicked, [this, header](int logicalIndex) {
        if (auto proxy = dynamic_cast<NauProjectContentProxyModel*>(model())) {
            switch (logicalIndex)
            {
            case +NauProjectBrowserFileSystemModel::Column::Name:
                proxy->setSortRole(Qt::DisplayRole);
                break;
            case +NauProjectBrowserFileSystemModel::Column::Type:
                proxy->setSortRole(NauProjectBrowserFileSystemModel::FileItemTypeRole);
                break;
            case +NauProjectBrowserFileSystemModel::Column::Size:
                proxy->setSortRole(NauProjectBrowserFileSystemModel::FileSizeRole);
                break;
            case +NauProjectBrowserFileSystemModel::Column::ModifiedTime:
                proxy->setSortRole(NauProjectBrowserFileSystemModel::FileLastModifiedRole);
                break;
            case +NauProjectBrowserFileSystemModel::Column::RelativePath:
                proxy->setSortRole(NauProjectBrowserFileSystemModel::FileRelativeDirPathRole);
                break;
            default:
                NED_ASSERT(!"Not implemented sort type");
            }
            proxy->sort(logicalIndex, header->sortIndicatorOrder());
        }
    });
    
    setObjectName("contentTableView");
    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setIndentation(0);
    setHeader(header);

    setExpandsOnDoubleClick(false);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::EditKeyPressed);
    setSelectionBehavior(SelectionBehavior::SelectRows);

    // Don't allow this viewer to sort by itself. We handle column's click and resort by ourselves.
    setSortingEnabled(false);
    header->setSortIndicatorShown(true);
    header->setSectionsClickable(true);
    header->setSectionHidden(+NauProjectBrowserFileSystemModel::Column::RelativePath, true);

    setColumnWidth(+NauProjectBrowserFileSystemModel::Column::Name, 200);
    setColumnWidth(+NauProjectBrowserFileSystemModel::Column::Type, 100);
    setColumnWidth(+NauProjectBrowserFileSystemModel::Column::Size, 100);
    setColumnWidth(+NauProjectBrowserFileSystemModel::Column::ModifiedTime, 100);
}

void NauProjectBrowserTableView::setDataAndSelectionModel(QAbstractItemModel& model, QItemSelectionModel& selectionModel)
{
    NauTreeView::setModel(&model);
    NauTreeView::setSelectionModel(&selectionModel);

    m_fileOperationMenu->watch(this);
}

void NauProjectBrowserTableView::contextMenuEvent(QContextMenuEvent* event)
{
    NauMenu menu;

    m_fileOperationMenu->enumerateActionsFor(this, menu);

    menu.exec(event->globalPos());
}

void NauProjectBrowserTableView::focusInEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusInEvent(event);
    const QItemSelectionModel* model = selectionModel();

    if (model && currentIndex().isValid()) {
        setAttribute(Qt::WA_InputMethodEnabled, (currentIndex().flags() & Qt::ItemIsEditable));
    } else if (!currentIndex().isValid()) {
        setAttribute(Qt::WA_InputMethodEnabled, false);
    }

    if (viewport()) {
        viewport()->update();
    }
}

void NauProjectBrowserTableView::mousePressEvent(QMouseEvent* event)
{
    NauTreeView::mousePressEvent(event);

    if (!indexAt(event->pos()).isValid()) {
        selectionModel()->clear();
    }
}

void NauProjectBrowserTableView::startDrag(Qt::DropActions supportedActions)
{
    QModelIndexList indexes = selectedIndexes();
    QModelIndexList dragableIndexes{};
    for (const QModelIndex& index : indexes) {
        if (index.flags() & Qt::ItemIsDragEnabled) {
            dragableIndexes << index;
        }
    }

    if (dragableIndexes.count() == 0) {
        NED_TRACE("Project browser list: no items to drag");
        return;
    }

    QMimeData *data = model()->mimeData(indexes);
    if (!data) {
        NED_WARNING("Project browser list: no mime data to be attached to a drag");
        return;
    }

    auto drag = new QDrag(this);
    drag->setMimeData(data);

    // Here we have to generate some dummy pixmap to be attached to the mouse pointer.
    // This pixmap are scaled inside Qt's library, so an empty one will lead to warning message in log
    // and eventually our assertion failure will be triggered.
    QPixmap stubPx{1, 1};
    stubPx.fill(Qt::transparent);

    drag->setPixmap(stubPx);
    drag->exec(supportedActions, Qt::IgnoreAction);
}

void NauProjectBrowserTableView::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.fillRect(event->rect(), Nau::Theme::current().paletteProjectBrowser().brush(NauPalette::Role::Background));

    if (auto proxy = dynamic_cast<const NauProjectContentProxyModel*>(model())) {
        const bool filterAppliedWithNoResult = proxy->fsProxy()->hasUserDefinedFilters() &&
            (!rootIndex().isValid() || !model()->hasChildren(rootIndex()));

        if (filterAppliedWithNoResult) {
            painter.setPen(QPen(Qt::gray));

            QRect textRect{QPoint{0, 0}, m_noFilterResultMessage.size().toSize()};
            textRect.moveCenter(event->rect().center());

            painter.drawStaticText(textRect.topLeft(), m_noFilterResultMessage);
            return;
        }
    }

    NauTreeView::paintEvent(event);
}
