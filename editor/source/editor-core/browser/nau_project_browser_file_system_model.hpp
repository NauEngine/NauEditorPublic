// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// A file system model(in terms of QT's model-view) for the project browser.

#pragma once

#include "baseWidgets/nau_widget_utility.hpp"
#include "browser/nau_project_browser_item_type.hpp"
#include "nau_sort_type.hpp"

#include <QFileSystemModel>


// ** NauProjectBrowserFileSystemModel

class NAU_EDITOR_API NauProjectBrowserFileSystemModel : public QFileSystemModel
{
    Q_OBJECT
public:
    enum NauFsModelRoles
    {
        FileSizeRole = FilePermissions + 1,

        // Relative to the root project directory with native separators and with leading separator.
        // e.g "/resources/ui/some_ui.data".
        // e.g "\content\hq\res".
        FileRelativePathRole,

        FileLastModifiedRole,

        // See NauEditorFileType.
        FileItemTypeRole,

        // Relative to the root project directory with native separators and with leading separator.
        // e.g "/resources/ui".
        // e.g "\content\hq\res".
        FileRelativeDirPathRole,
    };

    enum class Column
    {
        Name,
        Type,
        Size,
        ModifiedTime,
        RelativePath,
    };

    explicit NauProjectBrowserFileSystemModel(NauDir projectDir,
        std::vector<std::shared_ptr<NauProjectBrowserItemTypeResolverInterface>> itemTypeResolvers,
        QObject* parent = nullptr);

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    NauEditorFileType resolveItemType(const QString& filePath) const;

    static Column sortTypeToColumn(NauSortType type);
    static NauSortType columnToSortType(Column column);

private:
    const NauDir m_projectDir;
    std::vector<std::shared_ptr<NauProjectBrowserItemTypeResolverInterface>> m_itemTypeResolvers;
};
