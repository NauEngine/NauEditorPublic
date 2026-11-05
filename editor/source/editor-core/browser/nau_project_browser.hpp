// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Window that allows to browse files related to the current project.

#pragma once

#include "nau_path_navigation_widget.hpp"
#include "nau_project_browser_file_operations_menu.hpp"
#include "nau_project_browser_file_system_model.hpp"
#include "nau_project_browser_icon_provider.hpp"
#include "nau_project_browser_info_widget.hpp"
#include "nau_project_browser_item_type.hpp"
#include "nau_project_browser_list_view.hpp"
#include "nau_project_browser_table_view.hpp"
#include "nau_project_browser_proxy_models.hpp"
#include "nau_project_browser_styled_delegate.hpp"
#include "nau_project_browser_summary_widget.hpp"
#include "nau_project_browser_tree_view.hpp"
#include "nau_project_browser_view_scale_widget.hpp"
#include "nau_project_browser_item_type.hpp"
#include "nau_sort_type_widget.hpp"
#include "nau_shortcut_hub.hpp"
#include "baseWidgets/nau_widget.hpp"
#include "baseWidgets/nau_widget_utility.hpp"

#include <QFileSystemWatcher>

class NauFilterCheckBox;
class NauProject;


// ** NauProjectBrowser

class NAU_EDITOR_API NauProjectBrowser : public NauWidget
{
    Q_OBJECT

public:
    NauProjectBrowser(NauShortcutHub* shortcutHub, NauWidget* parent);

    bool setProject(const NauProject& project);
    void setCurrentDir(const NauDir& currentDir);
    void setAssetFileFilter(std::string_view assetExtension);
    void setAssetTypeResolvers(std::vector<std::shared_ptr<NauProjectBrowserItemTypeResolverInterface>> itemTypeResolvers);

signals:
    void eventFileDoubleClicked(const QString& path, NauEditorFileType type);
    void eventAddContentClicked(const QString& path);
    void eventImportClicked(const QString& path);

    // Emitted when fs in <sources> directory in project has been changed.
    void eventSourceChanged(const NauDir& projectSourceDir);

private:
    void applyFilter(const QString& filter);
    void setCurrentDirInTree(const NauDir& dir);
    void setCurrentDirInList(const QModelIndex& fsIndex);
    void updateTreeRootIndex();
    void updateTreeViewState();
    void updateAuxiliaryWidgets();
    void updateStateDuringFiltering();
    void updateTableViewColumnVisibilities();
    void handleFsProxyLayoutChanged();
    void handleContentViewDoubleClicked(const QModelIndex& index);
    void handleSortTypeOrOrderChanged(int columnIndex, Qt::SortOrder order);
    void installSourceDirWatcher();
    void handleSourceDirChanged();

    // Returns false and warns the user if any of specified directories contains undeletable items.
    bool checkNoUndeletableContent(const QModelIndexList& indexes);

    static std::vector<NauEditorFileType> filterableItemTypes();

    // Underlying FS model loads the fs tree in on-demand mode.
    void populateFsModel();

    NauWidget* createContentViewBottomContainer();
    NauWidget* createAddContentContainer();

private:
    NauDir m_projectRootDir;
    NauDir m_projectSourceDir;

    // Remember current directory in the project tree, to restore it after user clears a filter.
    QString m_lastTreeDirectory;
    bool m_fsModelPopulated = false;

    std::vector<std::shared_ptr<NauProjectBrowserItemTypeResolverInterface>> m_itemTypeResolvers;
    std::unique_ptr<NauProjectBrowserIconProvider> m_iconProvider;

    std::unique_ptr<NauProjectBrowserFileSystemModel> m_fsModel;
    std::unique_ptr<NauProjectFileSystemProxyModel> m_fsProxy;
    std::unique_ptr<NauProjectTreeProxyModel> m_treeProxy;
    std::unique_ptr<NauProjectContentProxyModel> m_contentViewProxy;

    std::unique_ptr<NauProjectTreeDelegate> m_treeDelegate;
    std::unique_ptr<NauProjectContentViewTableDelegate> m_contentViewTableDelegate;
    std::unique_ptr<NauProjectContentViewTileDelegate> m_contentViewTileDelegate;

    NauProjectBrowserTreeView* m_tree = nullptr;
    NauProjectBrowserListView* m_contentListView = nullptr;
    NauProjectBrowserTableView* m_contentTableView = nullptr;
    NauPathNavigationWidget* m_navigationBar = nullptr;
    NauProjectBrowserSummaryWidget* m_contentViewSummary = nullptr;
    NauProjectBrowserInfoWidget* m_contentViewCurentInfo = nullptr;
    NauProjectBrowserViewScaleWidget* m_contentViewScale = nullptr;
    NauSortTypeWidget* m_sortTypeWidget = nullptr;
    NauLayoutStacked* m_contentViewStackedLayout = nullptr;

    std::shared_ptr<NauProjectBrowserFileOperationsMenu> m_fileOperationMenu;

    std::unique_ptr<QFileSystemWatcher> m_projectSourceWatcher;
};
