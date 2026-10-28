// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_project_browser_proxy_models.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"
#include "nau_plus_enum.hpp"
#include "nau_project_browser_drag_context.hpp"
#include "nau_project_browser_file_system_model.hpp"
#include "themes/nau_theme.hpp"

#include "magic_enum/magic_enum.hpp"
#include "magic_enum/magic_enum_iostream.hpp"

#include <QDirIterator>

// ** NauProjectFileSystemProxyModel

NauProjectFileSystemProxyModel::NauProjectFileSystemProxyModel(const NauDir& rootDir, QObject* parent)
    : QSortFilterProxyModel(parent)
    , m_rootDir(makeStandardPath(rootDir.absolutePath(), true))
{
    setRecursiveFilteringEnabled(true);
    setDynamicSortFilter(false);
}

void NauProjectFileSystemProxyModel::setWhiteListResourcesWildcard(const QStringList& whiteList)
{
    m_whiteListResources = generateRegexes(whiteList);
    invalidateFilter();
}

void NauProjectFileSystemProxyModel::setUserDefinedFilters(const QStringList& filters)
{
    m_userDefinedFilters = generateRegexes(filters);
    invalidateFilter();

    emit userDefinedFilterChanged();
}

bool NauProjectFileSystemProxyModel::hasUserDefinedFilters() const
{
    return !m_userDefinedFilters.isEmpty() || !m_userDefinedTypeFilters.isEmpty();
}

void NauProjectFileSystemProxyModel::setUserDefinedTypeFilters(const QList<NauEditorFileType>& itemTypes)
{
    m_userDefinedTypeFilters = itemTypes;
    invalidateFilter();

    emit userDefinedFilterChanged();
}

void NauProjectFileSystemProxyModel::setUndeletableFileTypes(const QList<NauEditorFileType>& itemTypes)
{
    m_undeletableTypes = itemTypes;
}

bool NauProjectFileSystemProxyModel::containsUndeletableItems(const QString& dirPath) const
{
    if (m_undeletableTypes.isEmpty() || !QFileInfo{dirPath}.isDir()) return false;

    const auto fsModel = dynamic_cast<const NauProjectBrowserFileSystemModel*>(sourceModel());
    NED_ASSERT(fsModel);

    QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        if (m_undeletableTypes.contains(fsModel->resolveItemType(it.next()))) {
            return true;
        }
    }

    return false;
}

void NauProjectFileSystemProxyModel::setBlackListResourcesWildcard(const QStringList& blackList)
{
    m_blackListResources = generateRegexes(blackList);
    invalidateFilter();
}

void NauProjectFileSystemProxyModel::setReadOnlyResourcesWildcard(const QStringList& readOnlyResources)
{
    m_readOnlyResources = generateRegexes(readOnlyResources);
}

void NauProjectFileSystemProxyModel::setAllowedDirs(QList<QString> allowedDirs)
{
    m_allowedDirs.clear();
    for (const auto& allowedDir : allowedDirs) {
        m_allowedDirs.push_back(makeStandardPath(allowedDir, true));
    }

    invalidateFilter();
}

Qt::ItemFlags NauProjectFileSystemProxyModel::flags(const QModelIndex& index) const
{
    auto resultFlags = QSortFilterProxyModel::flags(index);

    if (!m_readOnlyResources.isEmpty()) {
        const auto relPath = index.data(NauProjectBrowserFileSystemModel::FileRelativePathRole).toString();

        for (const auto& rx : m_readOnlyResources) {
            if (rx.match(relPath).hasMatch()) {
                resultFlags.setFlag(Qt::ItemIsEditable, false);
                break;
            }
        }
    }

    const auto type = static_cast<NauEditorFileType>(index
        .data(NauProjectBrowserFileSystemModel::FileItemTypeRole).toInt());

    if (type == NauEditorFileType::Model) {
        resultFlags |= Qt::ItemIsDragEnabled;
    }

    if (!m_undeletableTypes.isEmpty() && m_undeletableTypes.contains(type)) {
        resultFlags |= QFlag(ItemIsUndeletable);
    }

    return resultFlags;
}

bool NauProjectFileSystemProxyModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (index.isValid() && (index.column() == 0) && (role == Qt::EditRole) && flags(index).testFlag(Qt::ItemIsEditable)) {
        if (!QFileInfo{index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString()}.isDir()) {
            // Renaming a file doesn't lead to QTBUG-33720. So we can forward to the default implementation.
            return QSortFilterProxyModel::setData(index, value, role);
        }

        const QString newName = value.toString();
        const QString oldName = index.data().toString();

        if (newName == oldName) {
            NED_DEBUG("Old and new name is the same. Renaming is rejected");
            return true;
        }

        if (newName.isEmpty() || QDir::toNativeSeparators(newName).contains(QDir::separator())) {
            NED_ERROR("Unable to rename from {} to {}", oldName.toUtf8().constData(), newName.toUtf8().constData());
            return false;
        }

        const QModelIndex indexParent = parent(index);
        const NauDir parentPath = indexParent.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
        const bool result = NauDir().rename(parentPath.absoluteFilePath(oldName), parentPath.absoluteFilePath(newName));

        if (!result) {
            NED_ERROR("Failed to rename from {} to {} in {}", oldName.toUtf8().constData(), newName.toUtf8().constData(),
                parentPath.absolutePath().toUtf8().constData());
        } else {
            NED_TRACE("Successfully renamed from {} to {} in {}", oldName.toUtf8().constData(), newName.toUtf8().constData(),
                parentPath.absolutePath().toUtf8().constData());
        }

        if (result) {
            emit dataChanged(index, index);
            emit fileRenamed(parentPath, oldName, newName);
        }

        return result;
    }

    return QSortFilterProxyModel::setData(index, value, role);
}

void NauProjectFileSystemProxyModel::visitIndexAndChildren(const QModelIndex& parent, 
    const std::function<void(const QModelIndex&)>& sink) const
{
    if (!parent.isValid()) {
        return;
    }
    std::invoke(sink, parent);

    const int childrenCount = rowCount(parent);
    for (int childIdx = 0; childIdx < childrenCount; ++childIdx) {
        const QModelIndex childIndex = index(childIdx, 0, parent);
        visitIndexAndChildren(childIndex, sink);
    }
}

void NauProjectFileSystemProxyModel::visitChildren(const QModelIndex& parent, 
    const std::function<void(const QModelIndexList&)>& sink) const
{
    const int parentRowCount = rowCount(parent);
    for (int rowIdx = 0; rowIdx < parentRowCount; ++rowIdx) {
        const QModelIndex childIndex = index(rowIdx, 0, parent);
        const int childColCount = columnCount(childIndex);

        QModelIndexList columnIndexes;
        for (int colIdx = 0; colIdx < childColCount; ++colIdx) {
            const QModelIndex childIndex = index(rowIdx, colIdx, parent);
            columnIndexes << childIndex;
        }
        std::invoke(sink, columnIndexes);

        if (hasChildren(childIndex)) {
            visitChildren(childIndex, sink);
        }
    }
}

bool NauProjectFileSystemProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (auto model = dynamic_cast<const NauProjectBrowserFileSystemModel*>(sourceModel())) {
        const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

        const QFileInfo fileInfo = model->fileInfo(index);
        const bool isDir = fileInfo.isDir();
        const QString filePath = makeStandardPath(model->filePath(index), isDir);
        const auto relPath = index.data(NauProjectBrowserFileSystemModel::FileRelativePathRole).toString();

        if (!m_rootDir.startsWith(filePath, Qt::CaseInsensitive) &&
            !filePath.startsWith(m_rootDir, Qt::CaseInsensitive)) {
            return false;
        }

        if (!m_allowedDirs.empty() && !relPath.isEmpty()) {
            bool found = false;
        
            for (const QString& allowedDir : m_allowedDirs) {
                if (allowedDir.startsWith(relPath, Qt::CaseInsensitive) ||
                    relPath.startsWith(allowedDir, Qt::CaseInsensitive)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }


        if (!isDir) {
            const QString fileName = fileInfo.fileName();
            bool atLeastOneSatisfies = false;
            for (const auto& rx : m_whiteListResources) {
                if (rx.match(fileName).hasMatch()) {
                    atLeastOneSatisfies = true;
                    break;
                }
            }
            if (!atLeastOneSatisfies) return false;

        }

        for (const auto& rx : m_blackListResources) {
            if (rx.match(relPath).hasMatch()) return false;
        }

        if (!isDir && !m_userDefinedTypeFilters.empty()) {
            bool satisfiesFilter = false;

            for (NauEditorFileType needleType : m_userDefinedTypeFilters) {
                const auto fileType = static_cast<NauEditorFileType>(
                    index.data(NauProjectBrowserFileSystemModel::FileItemTypeRole).toInt());

                if (fileType == needleType) {
                    satisfiesFilter = true;
                    break;
                }
            }

            if (!satisfiesFilter) return false;
        }

        if (!isDir && !m_userDefinedFilters.isEmpty() && !relPath.isEmpty()) {
            // Name of the current file must satisfy at least one of user specified filters.
            bool satisfiesFilter = false;

            for (const auto & rx : m_userDefinedFilters) {
                if (rx.match(fileInfo.fileName()).hasMatch()) {
                    satisfiesFilter = true;
                    break;
                }
            }

            if (!satisfiesFilter) return false;
        }
    }

    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

QList<QRegularExpression> NauProjectFileSystemProxyModel::generateRegexes(const QStringList& rawList)
{
    QList<QRegularExpression> result;
    for (const auto& wildcard : rawList) {
        result << QRegularExpression{QRegularExpression::wildcardToRegularExpression(
            QDir::toNativeSeparators(wildcard), QRegularExpression::UnanchoredWildcardConversion),
        QRegularExpression::CaseInsensitiveOption};
    }

    return result;
}

QString NauProjectFileSystemProxyModel::makeStandardPath(const QString& path, bool isDir)
{
    const QString filePath = NauDir::toNativeSeparators(path);
    return isDir && !filePath.endsWith(NauDir::separator()) ? filePath + NauDir::separator() : filePath;
}


// ** NauProjectBaseProxyModel

NauProjectBaseProxyModel::NauProjectBaseProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void NauProjectBaseProxyModel::setSourceModel(QAbstractItemModel* sourceModel)
{
    m_fsProxy = dynamic_cast<NauProjectFileSystemProxyModel*>(sourceModel);
    NED_ASSERT(m_fsProxy && "Unexpected type of source model");

    m_fsModel = dynamic_cast<NauProjectBrowserFileSystemModel*>(m_fsProxy->sourceModel());
    NED_ASSERT(m_fsModel && "Unexpected hirearchy of source model");

    connect(m_fsProxy, &NauProjectFileSystemProxyModel::userDefinedFilterChanged,
        this, &NauProjectBaseProxyModel::invalidateFilter);

    QSortFilterProxyModel::setSourceModel(sourceModel);
}

NauProjectBrowserFileSystemModel* NauProjectBaseProxyModel::fsModel() const
{
    return m_fsModel;
}

NauProjectFileSystemProxyModel* NauProjectBaseProxyModel::fsProxy() const
{
    return m_fsProxy;
}


// ** NauProjectTreeProxyModel

NauProjectTreeProxyModel::NauProjectTreeProxyModel(QObject* parent)
    : NauProjectBaseProxyModel(parent)
    , m_folderIcon(Nau::Theme::current().iconBrowserTreeFolder())
{
}

QVariant NauProjectTreeProxyModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::DecorationRole) {
        return m_folderIcon;
    }

    return NauProjectBaseProxyModel::data(index, role);
}

bool NauProjectTreeProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const auto index = fsProxy()->mapToSource(fsProxy()->index(sourceRow, 0, sourceParent));
    const QFileInfo fInfo = fsModel()->fileInfo(index);

    // In the tree we show only directories.
    if (!index.isValid() || !fInfo.isDir()) {
        return false;
    }

    return NauProjectBaseProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}


// ** NauProjectContentProxyModel

NauProjectContentProxyModel::NauProjectContentProxyModel(QObject* parent)
    : NauProjectBaseProxyModel(parent)
{
    setDynamicSortFilter(false);
}

void NauProjectContentProxyModel::setSortType(NauSortType type)
{
    switch (type) {
        case NauSortType::Name:
            setSortRole(Qt::DisplayRole);
            break;
        case NauSortType::Type:
            setSortRole(NauProjectBrowserFileSystemModel::FileItemTypeRole);
            break;
        case NauSortType::ModifiedTime:
            setSortRole(NauProjectBrowserFileSystemModel::FileLastModifiedRole);
            break;
        case NauSortType::Size:
            setSortRole(NauProjectBrowserFileSystemModel::FileSizeRole);
            break;
        case NauSortType::Path:
            setSortRole(NauProjectBrowserFileSystemModel::FileRelativeDirPathRole);
            break;
        default:
            NED_ASSERT(!"Not implemented sort type");
            break;
    }
}

void NauProjectContentProxyModel::setDataStructure(DataStructure structure)
{
    if (structure == dataStructure()) {
        NED_WARNING("Suspicious attempt to set structure mode to already set mode {}", magic_enum::enum_name(structure));
        return;
    }

    m_dataStructure = structure;
 
    if (dataStructure() == DataStructure::Hierarchy) {
      beginResetModel();
      clearFlatInternalData();
      endResetModel();

      // it's a good UX practice to re-sort with previously picked order.
      sort(sortColumn(), m_flatData.sortOrder);
    }
}

NauProjectContentProxyModel::DataStructure NauProjectContentProxyModel::dataStructure() const
{
    return m_dataStructure;
}

QModelIndex NauProjectContentProxyModel::index(int row, int column, const QModelIndex& parent) const
{ 
  if (dataStructure() == DataStructure::Flat) {
        if (parent == m_flatData.proxiedRoot && row >= 0 && row < m_flatData.proxiedIndices.size()) {
            return m_flatData.proxiedIndices[row][column];
        }

        return QModelIndex();
    }

    return NauProjectBaseProxyModel::index(row, column, parent);
}

bool NauProjectContentProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const QModelIndex fsIndex = fsProxy()->index(sourceRow, 0, sourceParent);
    if (!fsIndex.isValid()) {
        return false;
    }

    const bool isDir = fsModel()->fileInfo(fsProxy()->mapToSource(fsIndex)).isDir();

    // Hide directories without filtered by type files.
    if (isDir && fsProxy()->hasUserDefinedFilters() && fileCountRecursively(fsIndex) == 0) {
        return false;
    }

    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

QModelIndex NauProjectContentProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (dataStructure() == DataStructure::Flat) {
        if (sourceIndex.isValid()) {
            auto proxyIt = m_flatData.proxyBySourceIndices.find(sourceIndex);
            if (proxyIt != m_flatData.proxyBySourceIndices.cend()) {
                return proxyIt.value();
            }

            if (sourceIndex == m_flatData.rootIndex) {
                return m_flatData.proxiedRoot;
            }
        }

        return QModelIndex();
    }

    return NauProjectBaseProxyModel::mapFromSource(sourceIndex);
}
 
QModelIndex NauProjectContentProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (dataStructure() == DataStructure::Flat) {
        if (proxyIndex.isValid()) {
            auto sourceIt = m_flatData.sourceByProxyIndices.find(proxyIndex);
            if (sourceIt != m_flatData.sourceByProxyIndices.cend()) {
                return sourceIt.value();
            }

            if (proxyIndex == m_flatData.proxiedRoot) {
                return m_flatData.rootIndex;
            }
        }

        return QModelIndex();
    }

    return NauProjectBaseProxyModel::mapToSource(proxyIndex);
}

QModelIndex NauProjectContentProxyModel::parent(const QModelIndex& child) const
{
    if (dataStructure() == DataStructure::Flat) {
        if (m_flatData.proxiedRoot == child) {
            return QModelIndex();
        }

        // In this mode all items have the same parent.
        return m_flatData.proxiedRoot;
    }

    return NauProjectBaseProxyModel::parent(child);
}

QModelIndex NauProjectContentProxyModel::sibling(int row, int column, const QModelIndex& idx) const
{
    if (dataStructure() == DataStructure::Flat) {
        if ((idx == m_flatData.proxiedRoot) ||
            (row < 0) ||
            (row >= m_flatData.proxiedIndices.size()) ||
            (column < 0) ||
            (column >= m_flatData.proxiedIndices[row].size())) {
            return QModelIndex();
        }

        return m_flatData.proxiedIndices[row][column];
    }

    return NauProjectBaseProxyModel::sibling(row, column, idx);
}

int NauProjectContentProxyModel::rowCount(const QModelIndex& parent) const
{
    if (dataStructure() == DataStructure::Flat) {
        if (m_flatData.proxiedRoot == parent || !parent.isValid()) {
            return m_flatData.proxiedIndices.size();
        }

        return 0;
    }

    return NauProjectBaseProxyModel::rowCount(parent);
}

void NauProjectContentProxyModel::sort(int column, Qt::SortOrder order)
{
    m_flatData.sortOrder = order;

    if (dataStructure() == DataStructure::Flat) {
        performSort(/*emitSignals*/ true);
        return;
    };
    
    NauProjectBaseProxyModel::sort(column, order);
}

QVariant NauProjectContentProxyModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return sourceModel()->headerData(section, orientation, role);
}

int NauProjectContentProxyModel::fileCountRecursively(const QModelIndex& parent) const
{
    int result = 0;
    const auto accumulator = [&result](const QModelIndex& index) {
        const QString absPath = index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
        if (!QFileInfo(absPath).isDir()) {
            ++result;
        }
    };

    fsProxy()->visitIndexAndChildren(parent, accumulator);
    return result;
}

void NauProjectContentProxyModel::setCurrentRootIndex(QModelIndex rootIndex)
{
    m_flatData.rootIndex = rootIndex;

    if (dataStructure() == DataStructure::Flat) {
        beginResetModel();

        clearFlatInternalData();
        buildFlatInternalData();

        endResetModel();
    }
}

bool NauProjectContentProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QFileInfo lhsInfo{ left.data(NauProjectBrowserFileSystemModel::FilePathRole).toString() };
    const QFileInfo rhsInfo{ right.data(NauProjectBrowserFileSystemModel::FilePathRole).toString() };

    // We want directories always go first.
    if (lhsInfo.isDir() ^ rhsInfo.isDir()) return lhsInfo.isDir();

    if (sortRole() == Qt::DisplayRole) {
        return m_collator.compare(lhsInfo.fileName(), rhsInfo.fileName()) < 0;
    }

    if (sortRole() == NauProjectBrowserFileSystemModel::FileItemTypeRole) {
        const auto lhsType = left.data(NauProjectBrowserFileSystemModel::FileItemTypeRole).toInt();
        const auto rhsType = right.data(NauProjectBrowserFileSystemModel::FileItemTypeRole).toInt();
        if (lhsType == rhsType) {
            return m_collator.compare(lhsInfo.fileName(), rhsInfo.fileName()) < 0;
        }

        return lhsType < rhsType;
    }

    if (sortRole() == NauProjectBrowserFileSystemModel::FileSizeRole) {
        const qint64 sizeDifference = 
            left.siblingAtColumn(+NauProjectBrowserFileSystemModel::Column::Name).data(NauProjectBrowserFileSystemModel::FileSizeRole).toLongLong() - 
            right.siblingAtColumn(+NauProjectBrowserFileSystemModel::Column::Name).data(NauProjectBrowserFileSystemModel::FileSizeRole).toLongLong();

        if (sizeDifference == 0) {
            return m_collator.compare(lhsInfo.fileName(), rhsInfo.fileName()) < 0;
        }

        return sizeDifference < 0;
    }

    if (sortRole() == NauProjectBrowserFileSystemModel::FileLastModifiedRole) {
        if (lhsInfo.lastModified() == rhsInfo.lastModified())
            return m_collator.compare(lhsInfo.fileName(), rhsInfo.fileName()) < 0;

        return lhsInfo.lastModified() < rhsInfo.lastModified();
    }

    if (sortRole() == NauProjectBrowserFileSystemModel::FileRelativeDirPathRole) {
        const auto lhs = left.data(NauProjectBrowserFileSystemModel::FileRelativeDirPathRole).toString();
        const auto rhs = right.data(NauProjectBrowserFileSystemModel::FileRelativeDirPathRole).toString();

        return m_collator.compare(lhs, rhs) < 0;
    }

    NED_ASSERT(!"Not implemented sort role");
    return NauProjectBaseProxyModel::lessThan(left, right);
}

QMimeData* NauProjectContentProxyModel::mimeData(const QModelIndexList& indexes) const
{
    std::vector<std::pair<NauEditorFileType, QString>> data;

    std::function<void(const QModelIndex&)> visitor;
    visitor = [&visitor, &data](const QModelIndex& index) {
        const int rowCount = index.model()->rowCount(index);
        if (rowCount == 0) {
            const auto type = index.data(NauProjectBrowserFileSystemModel::FileItemTypeRole).toInt();
            const auto fileName = index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();

            data.emplace_back(std::make_pair(static_cast<NauEditorFileType>(type), fileName));
            return;
        }

        for (int idx = 0; idx < rowCount; ++idx) {
            visitor(index.model()->index(idx, 0, index));
        }
    };

    for (const QModelIndex& index : indexes) {
        visitor(index);
    }

    NauProjectBrowserDragContext context{ data };
    auto mimeData = new QMimeData();
    context.toMimeData(*mimeData);

    return mimeData;
}

void NauProjectContentProxyModel::buildFlatInternalData()
{
    // We attach `this` to index just to distinguish root index from the other proxied ones.
    m_flatData.proxiedRoot = createMappedIndex(m_flatData.rootIndex, 0, 0, reinterpret_cast<void*>(this));

    const auto navigator = [this](const QModelIndexList& indexes) {
        const QString absPath = indexes.front().data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
        if (!QFileInfo(absPath).isDir()) {
            m_flatData.proxiedIndices.push_back({});
            const int rowIdx = m_flatData.proxiedIndices.size() - 1;

            for (int colIdx = 0; colIdx < indexes.count(); ++colIdx) {
                m_flatData.proxiedIndices.back().push_back(createMappedIndex(indexes[colIdx], rowIdx, colIdx, nullptr));
            }
        }
    };

    fsProxy()->visitChildren(m_flatData.rootIndex, navigator);
    performSort(false);
}

void NauProjectContentProxyModel::clearFlatInternalData()
{
    m_flatData.proxiedIndices.clear();
    m_flatData.proxyBySourceIndices.clear();
    m_flatData.sourceByProxyIndices.clear();
}

QModelIndex NauProjectContentProxyModel::createMappedIndex(QModelIndex source, int rowIdx, int colIdx, void* userData)
{
    const QModelIndex proxiedIndex = createIndex(rowIdx, colIdx, userData);

    m_flatData.proxyBySourceIndices[source] = proxiedIndex;
    m_flatData.sourceByProxyIndices[proxiedIndex] = source;

    return proxiedIndex;
}

void NauProjectContentProxyModel::performSort(bool emitSignals)
{
    if (emitSignals) {
        emit layoutAboutToBeChanged();
    }

    QVector<QModelIndexList> newProxiedIndices;
    QHash<QModelIndex, QModelIndex> newProxyBySourceIndices;
    QHash<QModelIndex, QModelIndex> newSourceByProxyIndices;

    QVector<QModelIndexList> tmpCurrentProxiedIndices = m_flatData.proxiedIndices;

    std::stable_sort(std::begin(tmpCurrentProxiedIndices), std::end(tmpCurrentProxiedIndices),
        [this](const auto& left, const auto& right) {
        return m_flatData.sortOrder == Qt::AscendingOrder
            ? lessThan(left.front(), right.front())
            : lessThan(right.front(), left.front());
    });

    for (int row = 0; row < tmpCurrentProxiedIndices.size(); ++row) {
        QModelIndexList newRow{};
        for (int col = 0; col < tmpCurrentProxiedIndices[row].size(); ++ col) {
            const QModelIndex sourceIndex = mapToSource(tmpCurrentProxiedIndices[row][col]);
            const QModelIndex newProxyIndex = createIndex(row, col, nullptr);

            newProxyBySourceIndices[sourceIndex] = newProxyIndex;
            newSourceByProxyIndices[newProxyIndex] = sourceIndex;
            newRow << newProxyIndex;
        }
        newProxiedIndices.push_back(newRow);
    }

    if (emitSignals) {
        for (int row = 0; row < tmpCurrentProxiedIndices.size(); ++row) {
            changePersistentIndexList(m_flatData.proxiedIndices[row], newProxiedIndices[row]);
        }
    }

    m_flatData.proxiedIndices = std::move(newProxiedIndices);
    m_flatData.proxyBySourceIndices = std::move(newProxyBySourceIndices);
    m_flatData.sourceByProxyIndices = std::move(newSourceByProxyIndices);

    // Root index doesn't precipitate in sorting.
    m_flatData.proxyBySourceIndices[m_flatData.rootIndex] = m_flatData.proxiedRoot;
    m_flatData.sourceByProxyIndices[m_flatData.proxiedRoot] = m_flatData.rootIndex;

    if (emitSignals) {
        emit layoutChanged();
    }
}
