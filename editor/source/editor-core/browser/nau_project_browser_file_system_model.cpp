// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_project_browser_file_system_model.hpp"
#include "nau_project_browser_item_type.hpp"
#include "nau_assert.hpp"
#include "nau_plus_enum.hpp"
#include "themes/nau_theme.hpp"

#include "magic_enum/magic_enum.hpp"

// ** NauProjectBrowserFileSystemModel

NauProjectBrowserFileSystemModel::NauProjectBrowserFileSystemModel(NauDir projectDir,
    std::vector<std::shared_ptr<NauProjectBrowserItemTypeResolverInterface>> itemTypeResolvers, QObject* parent)
    : QFileSystemModel(parent)
    , m_projectDir(std::move(projectDir))
    , m_itemTypeResolvers(std::move(itemTypeResolvers))
{
}

QVariant NauProjectBrowserFileSystemModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        switch (index.column())
        {
        case +Column::Type: {
            if (fileInfo(index).isDir()) {
                return tr("Folder");
            }
            const QVariant itemTypeData = NauProjectBrowserFileSystemModel::data(index, NauFsModelRoles::FileItemTypeRole);
            if (itemTypeData.isValid()) {
                const auto itemType = static_cast<NauEditorFileType>(itemTypeData.toInt());
                if (itemType != NauEditorFileType::Unrecognized) {
                    return Nau::itemTypeToString(itemType);
                }
            }
            return {};
        }
        case +Column::Size: {
            const auto sizeData = NauProjectBrowserFileSystemModel::data(index.siblingAtColumn(+Column::Name), NauFsModelRoles::FileSizeRole);
            if (sizeData.isValid()) {
                return QLocale().formattedDataSize(sizeData.toLongLong(), 2, QLocale::DataSizeTraditionalFormat);
            }

            return {};
        }
        case +Column::ModifiedTime:
            return NauProjectBrowserFileSystemModel::data(index, NauFsModelRoles::FileLastModifiedRole);
        case +Column::RelativePath:
            return NauProjectBrowserFileSystemModel::data(index, NauFsModelRoles::FileRelativeDirPathRole);
        default: break;
        }
    }
    if (role == NauFsModelRoles::FileSizeRole) {
        long long size = 0;

        if (!hasChildren(index)) {
            return fileInfo(index).size();
        } else {
            for (int idx = 0; idx < rowCount(index); ++idx) {
                const auto child = this->index(idx, 0, index);
                if (hasChildren(child)) {
                    size += data(child, NauProjectBrowserFileSystemModel::FileSizeRole).toLongLong();
                } else {
                    size += fileInfo(child).size();
                }
            }
        }
        return size;
    }

    if (role == NauFsModelRoles::FileRelativePathRole) {
        const QString absFilePath = fileInfo(index).absoluteFilePath();
        const QString absProjectPath = m_projectDir.absolutePath();
        const auto projectPathSize = absProjectPath.size();

        if (!absFilePath.startsWith(absProjectPath, Qt::CaseInsensitive)) {
            return QString();
        }

        return QDir::toNativeSeparators(absFilePath.mid(projectPathSize));
    }
    if (role == NauFsModelRoles::FileRelativeDirPathRole) {
        const QString absFilePath = fileInfo(index).absolutePath();
        const QString absProjectPath = m_projectDir.absolutePath();
        const auto projectPathSize = absProjectPath.size();

        if (!absFilePath.startsWith(absProjectPath, Qt::CaseInsensitive)) {
            return QString();
        }
        const auto result = QDir::toNativeSeparators(absFilePath.mid(projectPathSize));
        if (result.isEmpty()) {
            return QDir::separator();
        }

        return result;
    }
    
    if (role == NauFsModelRoles::FileLastModifiedRole) {
        return fileInfo(index).lastModified();
    }

    if (role == NauFsModelRoles::FileItemTypeRole) {
        NauEditorFileType type = NauEditorFileType::Unrecognized;
        for (const auto& resolver : m_itemTypeResolvers) {
            const NauEditorFileType resolveAttempt = resolver->resolve(fileInfo(index).filePath()).type;
            if (resolveAttempt != NauEditorFileType::Unrecognized) {
                type = resolveAttempt;
                break;
            }
        }

        return static_cast<std::underlying_type_t<NauEditorFileType>>(type);
    }

    if (role == Qt::FontRole) {
        return index.column() == +Column::Name
            ? Nau::Theme::current().fontProjectBrowserPrimary()
            : Nau::Theme::current().fontProjectBrowserSecondary();
    }

    if (role == Qt::ToolTipRole) {
        QFileInfo fileInfo(filePath(index));
        return fileInfo.fileName();
    }

    return QFileSystemModel::data(index, role);
}

QVariant NauProjectBrowserFileSystemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole) {
        switch (section)
        {
        case +Column::Name: return tr("Name", "Column 'Name'");
        case +Column::Type: return tr("Type", "Column 'Type'");
        case +Column::Size: return tr("Size", "Column 'Size'");
        case +Column::ModifiedTime: return tr("Modified", "Column 'Modified'");
        case +Column::RelativePath: return tr("Path", "Column 'Path'");
        }
    } else if (role == Qt::FontRole) {
        return section == +Column::Name
            ? Nau::Theme::current().fontProjectBrowserPrimary()
            : Nau::Theme::current().fontProjectBrowserSecondary();
    }

    return QVariant();
}

int NauProjectBrowserFileSystemModel::columnCount(const QModelIndex& parent) const
{
    return magic_enum::enum_count<Column>();
}

NauProjectBrowserFileSystemModel::Column NauProjectBrowserFileSystemModel::sortTypeToColumn(NauSortType type)
{
    switch (type)
    {
    case NauSortType::Name: return Column::Name;
    case NauSortType::Type: return Column::Type;
    case NauSortType::ModifiedTime: return Column::ModifiedTime;
    case NauSortType::Size: return Column::Size;
    case NauSortType::Path: return Column::RelativePath;
    }

    NED_ASSERT(!"Unknown sort type");
    return {};
}

NauSortType NauProjectBrowserFileSystemModel::columnToSortType(Column column)
{
    switch (column)
    {
    case Column::Name: return NauSortType::Name;
    case Column::Type: return NauSortType::Type;
    case Column::ModifiedTime: return NauSortType::ModifiedTime;
    case Column::Size: return NauSortType::Size;
    case Column::RelativePath: return NauSortType::Path;
    }

    NED_ASSERT(!"Unknown column ");
    return {};
}
