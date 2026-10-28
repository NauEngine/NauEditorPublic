// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Applies QActions to the QAbstractItemView, and lets get a relative
// to the current state of view a list of menu items.

#pragma once

#include "baseWidgets/nau_widget_utility.hpp"
#include "nau_shortcut_hub.hpp"

#include <QAction>
#include <QAbstractItemView>
#include <QModelIndexList>
#include <QHash>

class NauAddContentMenu;


// ** NauProjectBrowserFileOperationsMenu

class NAU_EDITOR_API NauProjectBrowserFileOperationsMenu : public QObject
{
    Q_OBJECT
public:

    // The shortcuts map will be applied for all watched views.
    NauProjectBrowserFileOperationsMenu(NauShortcutHub* shortcutHub, QObject* parent = nullptr);

    // Starts to watch after specified view and creates QActions for all operations.
    // Adds actions to the view, if shortcut specified.
    void watch(QAbstractItemView* view);

    // Fills specified menu with a relevant list of QActions depending of the state of specified view.
    // UB if view is unknown, i.e. was not added via watch(...).
    void enumerateActionsFor(QAbstractItemView* view, NauMenu& menu);

    // Force to update internal data for the specified view.
    void updateViewSelectionData(QAbstractItemView* view);

    // Specified directories are not permitted to copy, cut, duplicate, rename or delete.
    void setProtectedDirs(const QList<NauDir>& protectedDirs);

signals:
    void eventCopyRequested(const QModelIndexList& indexes);
    void eventPasteRequested(const QModelIndex& parent);
    void eventCutRequested(const QModelIndexList& indexes);
    void eventRenameRequested(QAbstractItemView* view, const QModelIndex& index);
    void eventDeleteRequested(QAbstractItemView* view, const QModelIndexList& indexes);
    void eventDuplicateRequested(const QModelIndexList& indexes);
    void eventViewInShellRequested(const QModelIndexList& indexes);
    void eventCreateDirectoryRequested(QAbstractItemView* view, const QModelIndex& index);
    void eventImportAssetRequested();

private:
    static bool isValidDir(const QModelIndex& index);
    bool isProtectedDir(const QModelIndex& index) const;

private:
    NauShortcutHub* const m_shortcutHub;
    QPointer<NauAddContentMenu> m_addContentMenu;

    struct ViewData
    {
        QPersistentModelIndex destinationIndex;
        QPersistentModelIndex current;
        QModelIndexList selection;

        // Actions by ShortcutOperation attached to the view.
        QHash<NauShortcutOperation, QAction*> actions;
    };

    QHash<QAbstractItemView*, ViewData> m_views;
    QList<NauDir> m_protectedDirs;
};
