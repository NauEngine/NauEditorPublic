// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_project_browser_file_operations_menu.hpp"
#include "nau_action.hpp"
#include "nau_assert.hpp"
#include "nau_shortcut_operation.hpp"
#include "nau_project_browser_file_system_model.hpp"
#include "nau_project_browser_proxy_models.hpp"
#include "nau_log.hpp"
#include "nau_content_creator.hpp"
#include "themes/nau_theme.hpp"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>


// ** NauProjectBrowserFileOperationsMenu

NauProjectBrowserFileOperationsMenu::NauProjectBrowserFileOperationsMenu(
    NauShortcutHub* shortcutHub, QObject* parent)
    : QObject(parent)
    , m_shortcutHub(shortcutHub)
{
}

void NauProjectBrowserFileOperationsMenu::watch(QAbstractItemView* view)
{
    const auto registerAction = [this, view](const NauIcon& icon, const QString& text, NauShortcutOperation operation) {
        auto action = new NauAction(icon, text, view);
        m_views[view].actions[operation] = action;

        m_shortcutHub->addWidgetShortcut(operation, *view, std::bind(&NauAction::trigger, action));
        action->setShortcut(m_shortcutHub->getAssociatedKeySequence(operation));

        return action;
    };

    auto action = registerAction(Nau::Theme::current().iconCopy(), tr("Copy"), NauShortcutOperation::ProjectBrowserCopy);
    connect(action, &NauAction::triggered, this, [this, view] {
        NED_DEBUG("Copy triggered for {}", view->objectName());

        const QModelIndexList& selection = m_views[view].selection;
        if (selection.isEmpty()) return;

        for (const auto& index : m_views[view].selection) {
            if (isProjectDir(index)) {
                NED_ERROR("Project dir cannot be copied");
                return;
            }
        }
        emit eventCopyRequested(selection);
    });

    action = registerAction(Nau::Theme::current().iconCut(), tr("Cut"), NauShortcutOperation::ProjectBrowserCut);
    connect(action, &NauAction::triggered, this, [this, view] {
        NED_DEBUG("Cut triggered for {}", view->objectName());
        QModelIndexList editableIndexes;

        for (const auto& index : m_views[view].selection) {
            if (index.flags().testFlag(Qt::ItemIsEditable) && 
                !index.flags().testFlag(Qt::ItemFlag(NauProjectFileSystemProxyModel::ItemIsUndeletable)) &&
                !isProjectDir(index)) {
                editableIndexes << index;
            }
        }

        if (editableIndexes.isEmpty()) {
            NED_WARNING("Nothing to cut");
            return;
        }

        emit eventCutRequested(editableIndexes);
    });

    action = registerAction(Nau::Theme::current().iconDuplicate(), tr("Duplicate"), NauShortcutOperation::ProjectBrowserDuplicate);
    connect(action, &NauAction::triggered, this, [this, view] {
        NED_DEBUG("Duplicate triggered for {}", view->objectName());
        const QModelIndexList& selection = m_views[view].selection;
        if (selection.isEmpty()) return;

        for (const auto& index : selection) {
            if (isProjectDir(index)) {
                NED_ERROR("Project dir cannot be duplicated");
                return;
            }
        }

        emit eventDuplicateRequested(selection);
    });

    action = registerAction(Nau::Theme::current().iconPaste(), tr("Paste"), NauShortcutOperation::ProjectBrowserPaste);
    connect(action, &NauAction::triggered, this, [this, view] {
        NED_DEBUG("Paste triggered for {}", view->objectName());

        const ViewData& data = m_views[view];
        const bool hasFileInClipboard = QApplication::clipboard()->mimeData() && QApplication::clipboard()->mimeData()->hasUrls();
        const auto& current = data.destinationIndex;

        if (!hasFileInClipboard) {
            NED_ERROR("Paste operation declined: the clipboard is empty");
            return;
        }
        if (!isValidDir(current)) {
            NED_ERROR("Paste operation declined: invalid destination");
            return;
        }

        NED_DEBUG("Paste triggered for {}", current.data(NauProjectBrowserFileSystemModel::FilePathRole)
            .toString());

        emit eventPasteRequested(current);
    });

    action = registerAction(Nau::Theme::current().iconRename(), tr("Rename"), NauShortcutOperation::ProjectBrowserRename);
    connect(action, &NauAction::triggered, this, [this, view] {
        NED_DEBUG("Rename triggered for {}", view->objectName());
        const auto& current = m_views[view].current;

        if (!current.isValid()) {
            NED_WARNING("Nothing to rename. Aborting.");
            return;
        }

        if (isProjectDir(current)) {
            NED_WARNING("Renaming of project directory not implemented");
            return;
        }

        emit eventRenameRequested(view, current);
    });

    action = registerAction(Nau::Theme::current().iconDelete(), tr("Delete"), NauShortcutOperation::ProjectBrowserDelete);
    connect(action, &NauAction::triggered, this, [this, view] {
        NED_DEBUG("Delete triggered for {}", view->objectName());
        QModelIndexList editableIndexes;

        for (const auto& index : m_views[view].selection) {
            if (index.flags().testFlag(Qt::ItemIsEditable) && 
                !index.flags().testFlag(Qt::ItemFlag(NauProjectFileSystemProxyModel::ItemIsUndeletable)) &&
                !isProjectDir(index)) {
                editableIndexes << index;
            }
        }
        if (editableIndexes.isEmpty()) {
            NED_WARNING("Nothing to delete");
            return;
        }
        emit eventDeleteRequested(view, editableIndexes);
    });

#ifdef Q_OS_WIN
    action = registerAction(Nau::Theme::current().iconViewAssetInShell(), tr("Show in Explorer"), NauShortcutOperation::ProjectBrowserViewInShell);
    connect(action, &NauAction::triggered, this, [this, view] {
        NED_DEBUG("ViewInShell triggered for {}", view->objectName());
        const ViewData& data = m_views[view];

        QModelIndexList indexes;
        if (!data.selection.isEmpty()) {
            indexes = data.selection;
        } else if (data.current.isValid()) {
            indexes << data.current;
        } else if (data.destinationIndex.isValid()) {
            indexes  << data.destinationIndex;
        }

        if (indexes.isEmpty()) return;

        emit eventViewInShellRequested(indexes);
    });
#endif

    action = registerAction(Nau::Theme::current().iconMakeNewFolder(), tr("Create folder"), NauShortcutOperation::ProjectBrowserCreateDir);
    connect(action, &NauAction::triggered, this, [this, view] {
        NED_DEBUG("Create folder triggered for {}", view->objectName());

        const ViewData& data = m_views[view];
        const auto& current = data.destinationIndex;

        if (!isValidDir(current)) {
            NED_ERROR("Unable to create a folder");
            return;
        }

        emit eventCreateDirectoryRequested(view, current);
    });

    action = registerAction(Nau::Theme::current().iconImportAssetContext(), tr("Import asset"), NauShortcutOperation::ProjectBrowserImportAsset);
    connect(action, &NauAction::triggered, this, [this, view] {
        NED_DEBUG("Import asset triggered for {}", view->objectName().toUtf8().constData());
        emit eventImportAssetRequested();
    });

    updateViewSelectionData(view);

     connect(view->selectionModel(), &QItemSelectionModel::currentChanged,
        std::bind(&NauProjectBrowserFileOperationsMenu::updateViewSelectionData, this, view));

    connect(view->selectionModel(), &QItemSelectionModel::selectionChanged, 
        std::bind(&NauProjectBrowserFileOperationsMenu::updateViewSelectionData, this, view));

    connect(view->model(), &QAbstractItemModel::modelReset,
        std::bind(&NauProjectBrowserFileOperationsMenu::updateViewSelectionData, this, view));

    connect(view->model(), &QAbstractItemModel::layoutChanged,
        std::bind(&NauProjectBrowserFileOperationsMenu::updateViewSelectionData, this, view));
}

void NauProjectBrowserFileOperationsMenu::enumerateActionsFor(QAbstractItemView* view, NauMenu& menu)
{
    auto it = m_views.find(view);
    NED_ASSERT(it != m_views.end());
    const ViewData& data = m_views[view];

    const QModelIndexList& indexes = data.selection;
    const bool haveSelection = !indexes.isEmpty();
    const bool isOnlyItem = indexes.size() == 1;
    const bool hasFileInClipboard = QApplication::clipboard()->mimeData() && QApplication::clipboard()->mimeData()->hasUrls();

    bool hasProjectDir = false;
    bool allReadonly = true;
    bool allDeletable = true;
    bool isDirOnly = true;

    for (const auto& index : m_views[view].selection) {
        if (allReadonly && index.flags().testFlag(Qt::ItemIsEditable)) {
            allReadonly = false;
        }
        if (allDeletable && index.flags().testFlag(Qt::ItemFlag(NauProjectFileSystemProxyModel::ItemIsUndeletable))) {
            allDeletable = false;
        }

        const auto fileName = index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
        const QFileInfo fi{fileName};

        if (isDirOnly) {
            if (!fi.isDir()) {
                isDirOnly = false;
            }
        }

        if (!hasProjectDir && isProjectDir(index)) {
            hasProjectDir = true;
        }
    }

    if (!haveSelection) {
        const QString currentDir = data.destinationIndex
            .data(NauProjectBrowserFileSystemModel::FilePathRole).toString();

        m_addContentMenu = new NauAddContentMenu(currentDir, nullptr);
        if (!m_addContentMenu->actions().isEmpty()) {
            auto addAction = menu.addMenu(m_addContentMenu);
            addAction->setIcon(Nau::Theme::current().iconAddAsset());

        }
        menu.addAction(data.actions[NauShortcutOperation::ProjectBrowserImportAsset]);
    }

    if (!haveSelection || (isDirOnly && isOnlyItem)) {
        menu.addAction(data.actions[NauShortcutOperation::ProjectBrowserCreateDir]);
    }

    if (haveSelection && !hasProjectDir) {
        menu.addAction(data.actions[NauShortcutOperation::ProjectBrowserCopy]);

        if (!allReadonly && allDeletable) {
            menu.addAction(data.actions[NauShortcutOperation::ProjectBrowserCut]);
        }

        menu.addAction(data.actions[NauShortcutOperation::ProjectBrowserDuplicate]);
    }

    if (hasFileInClipboard) {
        menu.addAction(data.actions[NauShortcutOperation::ProjectBrowserPaste]);
    }

    if (haveSelection && !allReadonly && !hasProjectDir) {
        if (isOnlyItem) {
            menu.addAction(data.actions[NauShortcutOperation::ProjectBrowserRename]);
        }

        if (allDeletable) {
            menu.addAction(data.actions[NauShortcutOperation::ProjectBrowserDelete]);
        }
    }

#ifdef Q_OS_WIN
    menu.addAction(data.actions[NauShortcutOperation::ProjectBrowserViewInShell]);
#endif

}

void NauProjectBrowserFileOperationsMenu::updateViewSelectionData(QAbstractItemView* view)
{
    ViewData& data = m_views[view];

    QModelIndexList selectedIndexes;
    for (const auto& index : view->selectionModel()->selectedIndexes()) {
        if (index.column() == 0) {
            selectedIndexes << index;
        }
    }

    data.selection = selectedIndexes;
    data.current = view->selectionModel()->currentIndex();
    data.destinationIndex = dynamic_cast<QTreeView*>(view) ? view->currentIndex() : view->rootIndex();

}

void NauProjectBrowserFileOperationsMenu::setProjectDir(const NauDir& projectDir)
{
    m_projectDir = projectDir;
}

bool NauProjectBrowserFileOperationsMenu::isValidDir(const QModelIndex& index)
{
    if (!index.isValid())
        return false;

    return QFileInfo{index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString()}.isDir();
}

bool NauProjectBrowserFileOperationsMenu::isProjectDir(const QModelIndex& index) const
{
    return NauDir{index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString()} == m_projectDir;
}
