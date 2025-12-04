// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_project_browser_tree_view.hpp"
#include "nau_assert.hpp"
#include "themes/nau_theme.hpp"

#include <QPainter>


// ** NauProjectBrowserTreeView

NauProjectBrowserTreeView::NauProjectBrowserTreeView(
    std::shared_ptr<NauProjectBrowserFileOperationsMenu> fileMenu, NauSplitter* parent)
    : NauTreeView(parent)
    , m_fileOperationMenu(std::move(fileMenu))
{
    NED_ASSERT(m_fileOperationMenu);
    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
}

void NauProjectBrowserTreeView::setModel(QAbstractItemModel* model)
{
    NauTreeView::setModel(model);
    m_fileOperationMenu->watch(this);
}

void NauProjectBrowserTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    if (!indexAt(event->pos()).isValid()) {
        emit eventContextMenuRequestedOnEmptySpace();
    }

    NauMenu menu;

    m_fileOperationMenu->enumerateActionsFor(this, menu);

    menu.exec(event->globalPos());
}

void NauProjectBrowserTreeView::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.fillRect(event->rect(), Nau::Theme::current().paletteProjectBrowser().brush(NauPalette::Role::Background));

    NauTreeView::paintEvent(event);
}
