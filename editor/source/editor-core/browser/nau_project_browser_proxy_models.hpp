// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// A set of proxy models utilizes in the project browser.

#pragma once

#include "baseWidgets/nau_widget_utility.hpp"
#include "nau_sort_type.hpp"
#include "baseWidgets/nau_icon.hpp"
#include "nau_project_browser_file_system_model.hpp"
#include "nau_project_browser_item_type.hpp"

#include <QCollator>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStringList>

#include <limits>


// ** NauProjectFileSystemProxyModel

class NAU_EDITOR_API NauProjectFileSystemProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum
    {
        ItemIsUndeletable = std::underlying_type_t<Qt::ItemFlag>(1) << 
            std::numeric_limits<std::underlying_type<Qt::ItemFlag>::type>::digits
    };

    NauProjectFileSystemProxyModel(const NauDir& rootDir, QObject* parent = nullptr);

    // Set a list of wildcards(glob) patterns that are not visible in project browser specified by a user.
    // Emits userDefinedFilterChanged().
    void setUserDefinedFilters(const QStringList& filters);

    // Set a list of allowed item types.
    // Emits userDefinedFilterChanged().
    void setUserDefinedTypeFilters(const QList<NauEditorFileType>& itemTypes);

    // Marks specified types as undeletables. Undeletable ones can be modified, renamed,
    // but are not permitted to move or delete.
    void setUndeletableFileTypes(const QList<NauEditorFileType>& itemTypes);

    // Returns true if specified directory contains (recursively) any item of undeletable type.
    // Flags of a directory don't take its content into account, so check it before moving or deleting.
    bool containsUndeletableItems(const QString& dirPath) const;

    // Has this model any user defined filters currently.
    // See setUserDefinedTypeFilters() and setUserDefinedFilters().
    bool hasUserDefinedFilters() const;

    // Set a white list of wildcards(glob) patterns for files that are visible in project browser.
    // e.g.: *.blk, *.bin
    void setWhiteListResourcesWildcard(const QStringList& hiddenResources);

    // Set a list of wildcards(glob) patterns that are not visible in project browser.
    // Paths must be relative to project root directory.
    // e.g.: .log/*, *.tmp
    void setBlackListResourcesWildcard(const QStringList& blackList);

    // Set a list of wildcards(glob) patterns that are not permitted to edit (rename, delete, move etc).
    // Paths must be relative to project root directory.
    // e.g.: *.dll, bin/*.exe, //qmake//*
    void setReadOnlyResourcesWildcard(const QStringList& readOnlyResources);

    // Only specified directories will pass the filter.
    void setAllowedDirs(QList<QString> allowedDirs);

    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;

    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

    // We have to implement renaming by ourselves for QTBUG-33720.
    // Underlying fs model looses the monitoring the directory after renaming.
    // So we rename a directory via OS API.
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // Traverse over specified parent and its descendants and calls sink for every node.
    void visitIndexAndChildren(const QModelIndex& parent, const std::function<void(const QModelIndex&)>& sink) const;
    
    // Traverse over children of specified parent calls sink for them with list of column indexes.
    void visitChildren(const QModelIndex& parent, const std::function<void(const QModelIndexList&)>& sink) const;

signals:
    void fileRenamed(const NauDir& path, const QString& oldName, const QString& newName);

    // Emitted every time when a filter by file types changes.
    // See setUserDefinedTypeFilters() and setUserDefinedFilters().
    void userDefinedFilterChanged();

private:
    static QList<QRegularExpression> generateRegexes(const QStringList& rawList);
    static QString makeStandardPath(const QString& path, bool isDir);

private:
    const QString m_rootDir;

    QList<NauEditorFileType> m_userDefinedTypeFilters;
    QList<NauEditorFileType> m_undeletableTypes;
    QList<QRegularExpression> m_userDefinedFilters;
    QList<QRegularExpression> m_whiteListResources;
    QList<QRegularExpression> m_blackListResources;
    QList<QRegularExpression> m_readOnlyResources;
    QList<QString> m_allowedDirs;
};


// ** NauProjectBaseProxyModel

class NAU_EDITOR_API NauProjectBaseProxyModel : public QSortFilterProxyModel
{
public:
    NauProjectBaseProxyModel(QObject* parent);

    virtual void setSourceModel(QAbstractItemModel *sourceModel) override;

    NauProjectBrowserFileSystemModel* fsModel() const;
    NauProjectFileSystemProxyModel* fsProxy() const;

private:
    NauProjectBrowserFileSystemModel* m_fsModel = nullptr;
    NauProjectFileSystemProxyModel* m_fsProxy = nullptr;
};


// ** NauProjectTreeProxyModel

class NAU_EDITOR_API NauProjectTreeProxyModel : public NauProjectBaseProxyModel
{
public:
    NauProjectTreeProxyModel(QObject* parent = nullptr);

    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
private:
    NauIcon m_folderIcon;
};


// ** NauProjectContentProxyModel

class NAU_EDITOR_API NauProjectContentProxyModel : public NauProjectBaseProxyModel
{
public:
    enum class DataStructure
    {
        // A structure of data corresponding hierarchy of the file system.
        Hierarchy,

        // A flat model data of hierarchy of file system.
        // In this mode, only files of the root are provided.
        // See setCurrentRootIndex.
        Flat
    };

    NauProjectContentProxyModel(QObject* parent = nullptr);

    void setSortType(NauSortType type);

    // Changes the structure of internal data.
    // Emits modelAboutToBeReset() before changing and modelReset after.
    // See StructureMode.
    void setDataStructure(DataStructure scructure);
    DataStructure dataStructure() const;

    // In StructureMode::Flat mode only child files(recursively) in this rootIndex are listed.
    void setCurrentRootIndex(QModelIndex rootIndex);

    int fileCountRecursively(const QModelIndex& parent) const;

    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const;
    virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const;

    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual QModelIndex sibling(int row, int column, const QModelIndex &idx) const;
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
    virtual QMimeData* mimeData(const QModelIndexList& indexes) const override;

private:
    void buildFlatInternalData();
    void clearFlatInternalData();
    QModelIndex createMappedIndex(QModelIndex source, int rowIdx, int colIdx, void* userData = nullptr);

    // Views expect us to emit some signals before and after sorting.
    // But sometimes we sort during model reset, for example switching between structure mode.
    // In that emitting layoutAboutToChange/layoutChanged is overhead.
    // See StructureMode, setStructureMode().
    void performSort(bool emitSignals);

private:
    struct FlatModeData
    {
        Qt::SortOrder sortOrder = Qt::AscendingOrder;

        QModelIndex rootIndex;
        QModelIndex proxiedRoot;

        // Internal proxied indices in the flat mode.
        QVector<QModelIndexList> proxiedIndices;

        // Bidirectional mappings source->to our index and vice versa.
        QHash<QModelIndex, QModelIndex> proxyBySourceIndices;
        QHash<QModelIndex, QModelIndex> sourceByProxyIndices;
    };

private:
    QCollator m_collator;
    DataStructure m_dataStructure = DataStructure::Hierarchy;

    FlatModeData m_flatData;
};
