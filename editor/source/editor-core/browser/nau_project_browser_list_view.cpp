// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_project_browser_list_view.hpp"
#include "nau_project_browser_proxy_models.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"

#include <QDrag>
#include <QPainter>
#include <QPaintEvent>


// ** NauProjectBrowserListView

NauProjectBrowserListView::NauProjectBrowserListView(std::shared_ptr<NauProjectBrowserFileOperationsMenu> fileMenu, NauSplitter* parent)
    : NauListView(parent)
    , m_fileOperationMenu(std::move(fileMenu))
    , m_noFilterResultMessage(tr("There are no results matching the specified filters"))
{
    NED_ASSERT(m_fileOperationMenu);
    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setObjectName("contentListView");
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(NauProjectBrowserListView::ExtendedSelection);
    setResizeMode(NauProjectBrowserListView::Adjust);
    setEditTriggers(NauProjectBrowserListView::EditKeyPressed);
    setSelectionBehavior(SelectionBehavior::SelectRows);

}

void NauProjectBrowserListView::setDataAndSelectionModel(QAbstractItemModel& model, QItemSelectionModel& selectionModel)
{
    NauListView::setModel(&model);
    NauListView::setSelectionModel(&selectionModel);

    m_fileOperationMenu->watch(this);
}

void NauProjectBrowserListView::contextMenuEvent(QContextMenuEvent* event)
{
    NauMenu menu;

    m_fileOperationMenu->enumerateActionsFor(this, menu);

    menu.exec(event->globalPos());
}

void NauProjectBrowserListView::focusInEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusInEvent(event);
    const QItemSelectionModel* model = selectionModel();
    bool currentIndexValid = currentIndex().isValid();

    if (model && currentIndexValid) {
        setAttribute(Qt::WA_InputMethodEnabled, (currentIndex().flags() & Qt::ItemIsEditable));
    } else if (!currentIndexValid) {
        setAttribute(Qt::WA_InputMethodEnabled, false);
    }

    if (viewport()) {
        viewport()->update();
    }
}

void NauProjectBrowserListView::mousePressEvent(QMouseEvent* event)
{
    NauListView::mousePressEvent(event);

    if (!indexAt(event->pos()).isValid())
        selectionModel()->clear();
}

void NauProjectBrowserListView::startDrag(Qt::DropActions supportedActions)
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

void NauProjectBrowserListView::paintEvent(QPaintEvent* event)
{
    if (auto proxy = dynamic_cast<const NauProjectContentProxyModel*>(model())) {
        const bool filterAppliedWithNoResult = proxy->fsProxy()->hasUserDefinedFilters() &&
            (!rootIndex().isValid() || !model()->hasChildren(rootIndex()));

        if (filterAppliedWithNoResult) {
            QPainter painter(viewport());
            painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
            painter.setPen(QPen(Qt::gray));

            QRect textRect{QPoint{0, 0}, m_noFilterResultMessage.size().toSize()};
            textRect.moveCenter(event->rect().center());

            painter.drawStaticText(textRect.topLeft(), m_noFilterResultMessage);
            return;
        }
    }

    NauListView::paintEvent(event);
}
