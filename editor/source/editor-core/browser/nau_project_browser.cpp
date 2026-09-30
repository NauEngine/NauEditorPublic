// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_project_browser.hpp"
#include "nau_assert.hpp"
#include "nau_buttons.hpp"
#include "nau_path_navigation_widget.hpp"
#include "filter/nau_filter_widget.hpp"
#include "nau_project_browser_file_operations_menu.hpp"
#include "nau_project_browser_item_type_resolver.hpp"
#include "nau_file_operations.hpp"
#include "nau_log.hpp"

#include "project/nau_project.hpp"
#include "magic_enum/magic_enum.hpp"
#include "magic_enum/magic_enum_utility.hpp"
#include "filter/nau_search_widget.hpp"
#include "themes/nau_theme.hpp"
#include "nau/nau_constants.hpp"

#include <QMessageBox>

#include <filesystem>


// ** NauProjectBrowserFilterWidgetAction

class NauProjectBrowserFilterWidgetAction : public NauFilterWidgetAction
{
public:
    NauProjectBrowserFilterWidgetAction(const NauIcon& icon, NauEditorFileType type, NauWidget* parent)
        : NauFilterWidgetAction(icon, Nau::itemTypeToString(type), false, parent)  // Filters at first startup are disabled
        , m_itemType(type)
    {}

    [[nodiscard]] NauEditorFileType itemType() const noexcept { return m_itemType; }

private:
    NauEditorFileType m_itemType;
};


// ** NauProjectBrowser

NauProjectBrowser::NauProjectBrowser(NauShortcutHub* shortcutHub, NauWidget* parent)
    : NauWidget(parent)
{
    auto layout = new NauLayoutVertical(this);
    layout->setObjectName("ProjectBrowserLayout");
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto topControllerContainer = new NauWidget;
    topControllerContainer->setObjectName("topControllerContainer");
    topControllerContainer->setMinimumHeight(36);

    auto topContainerLayout = new NauLayoutHorizontal(topControllerContainer);
    topContainerLayout->setSpacing(16);
    topContainerLayout->setContentsMargins(16, 8, 16, 8);
    topContainerLayout->setSpacing(16);

    topControllerContainer->setLayout(topContainerLayout);

    auto searchBox = new NauSearchWidget(this);
    searchBox->setObjectName("SearchBox");
    searchBox->setPlaceholderText(tr("Search in assets..."));
    searchBox->setToolTip(
        tr("You can specify filters. Separate parameters by %1. Example: *.exe%1*.png").arg(NauDir::listSeparator()));
    connect(searchBox, &NauLineEdit::textChanged, this, [this, searchBox] {
        applyFilter(searchBox->text());
    });
    connect(searchBox->searchAction(), &QAction::triggered, this, [this, searchBox] {
        applyFilter(searchBox->text());
    });

    auto filterContainer = new NauFlowLayout(Qt::LeftToRight, 0, 4, 4);
    filterContainer->setObjectName("filterItemsContainer");

    auto filterWidget = new NauFilterWidget(filterContainer, this);

    const auto types = filterableItemTypes();
    for (const auto itemType : types) {
        auto filter = new NauProjectBrowserFilterWidgetAction(Nau::Theme::current().iconResourcePlaceholder(), itemType, filterWidget);
        filterWidget->addFilterParam(filter);
    }

    topContainerLayout->addWidget(createAddContentContainer());
    topContainerLayout->addWidget(filterWidget);
    topContainerLayout->addWidget(searchBox);
    topContainerLayout->setStretch(2, 1);

    auto splitter = new NauSplitter();
    splitter->setObjectName("ProjectBrowserSplitter");

    m_fileOperationMenu = std::make_shared<NauProjectBrowserFileOperationsMenu>(shortcutHub);

    m_tree = new NauProjectBrowserTreeView(m_fileOperationMenu, splitter);
    m_tree->setObjectName("NauProjectBrowserTree");
    m_tree->setSelectionMode(NauProjectBrowserTreeView::SingleSelection);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setHeaderHidden(true);
    m_tree->setEditTriggers(NauProjectBrowserTreeView::NoEditTriggers);

    auto topListViewControlBar = new NauWidget;
    topListViewControlBar->setObjectName("topListViewControlBar");

    auto topListViewControlBarLayout = new NauLayoutVertical(topListViewControlBar);

    auto contentViewControlBar = new NauWidget;
    auto contentViewControlBarLayout = new NauLayoutHorizontal(contentViewControlBar);
    contentViewControlBarLayout->setSpacing(8);
    contentViewControlBarLayout->setContentsMargins(16, 8, 16, 8);

    m_navigationBar = new NauPathNavigationWidget(contentViewControlBar);
    m_navigationBar->setObjectName("pathNavigator");

    m_sortTypeWidget = new NauSortTypeWidget(contentViewControlBar);
    m_sortTypeWidget->setObjectName("sortTypeWidget");

    auto contentViewSwitcherButton = new NauPushButton(contentViewControlBar);
    contentViewSwitcherButton->setObjectName("ContentViewModeSwitcher");
    contentViewSwitcherButton->setCheckable(true);

    QIcon icon;
    icon.addPixmap(QPixmap(":/UI/icons/browser/hierarchy-view.png"), QIcon::Mode::Normal, QIcon::State::On);
    icon.addPixmap(QPixmap(":/UI/icons/browser/flat-view.png"), QIcon::Mode::Normal, QIcon::State::Off);
    contentViewSwitcherButton->setIcon(icon);
    contentViewSwitcherButton->setStyleSheet("border:none");

    connect(contentViewSwitcherButton, &NauPushButton::toggled, [this](bool on){
        m_contentViewProxy->setDataStructure(on
            ? NauProjectContentProxyModel::DataStructure::Flat
            : NauProjectContentProxyModel::DataStructure::Hierarchy);

        setCurrentDirInList(m_tree->currentIndex());
        updateTableViewColumnVisibilities();
    });

    contentViewControlBarLayout->addWidget(m_navigationBar);
    contentViewControlBarLayout->addWidget(m_sortTypeWidget);
    contentViewControlBarLayout->addWidget(contentViewSwitcherButton);
    contentViewControlBarLayout->setStretch(0, 1);

    m_contentListView = new NauProjectBrowserListView(m_fileOperationMenu, splitter);
    m_contentTableView = new NauProjectBrowserTableView(m_fileOperationMenu, splitter);

    m_contentViewStackedLayout = new NauLayoutStacked;
    m_contentViewStackedLayout->setObjectName("contentViewStackedLayout");
    m_contentViewStackedLayout->addWidget(m_contentListView);
    m_contentViewStackedLayout->addWidget(m_contentTableView);

    topListViewControlBarLayout->addWidget(contentViewControlBar);
    topListViewControlBarLayout->addLayout(m_contentViewStackedLayout);
    topListViewControlBarLayout->addWidget(createContentViewBottomContainer());
    topListViewControlBarLayout->setStretch(1, 1);
    topListViewControlBarLayout->setSpacing(0);
    topListViewControlBarLayout->setContentsMargins(0, 0, 0, 0);

    splitter->addWidget(m_tree);
    splitter->addWidget(topListViewControlBar);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    
    layout->addWidget(topControllerContainer);
    layout->addLayout(filterContainer);
    layout->addWidget(new NauSpacer);
    layout->addWidget(splitter);
    layout->setStretch(3, 1);

    shortcutHub->addApplicationShortcut(NauShortcutOperation::ProjectBrowserFindAsset,
    [this, searchBox](NauShortcutOperation) {
        searchBox->setFocus(Qt::ShortcutFocusReason);
    });

    connect(m_tree, &NauProjectBrowserTreeView::eventContextMenuRequestedOnEmptySpace, [this]{
        setCurrentDirInTree(m_projectRootDir);
    });

    connect(m_sortTypeWidget, &NauSortTypeWidget::eventSortTypeOrderChanged, [this](NauSortType type, NauSortOrder order) {
        if (!m_contentViewProxy) return;

        std::string_view type_name = magic_enum::enum_name(type);
        std::string_view order_name = magic_enum::enum_name(order);
        NED_DEBUG("ProjectBrowser: set sort type to: {} and order {}", type_name, order_name);

        const auto columnIdx = +NauProjectBrowserFileSystemModel::sortTypeToColumn(type);
        m_contentViewProxy->setSortType(type);
        m_contentViewProxy->sort(columnIdx, Nau::toQtSortOrder(order));
        m_contentTableView->header()->setSortIndicator(columnIdx, Nau::toQtSortOrder(order));
    });

    connect(m_navigationBar, &NauPathNavigationWidget::changeDirectoryRequested, [this](const NauDir& dir) {
        const QString path = m_projectRootDir.absoluteFilePath(dir.path());
        const QModelIndex index = m_fsModel->index(path);
        const QModelIndex treeIndex = m_treeProxy->mapFromSource(m_fsProxy->mapFromSource(index));

        m_tree->selectionModel()->setCurrentIndex(treeIndex, QItemSelectionModel::ClearAndSelect);
    });

    connect(m_contentViewScale, &NauProjectBrowserViewScaleWidget::eventScaleValueChanged, [this](float scale) {
        NED_ASSERT(m_contentViewTileDelegate && m_contentViewTableDelegate);
        m_contentViewTileDelegate->setScale(scale);

        static const int tileViewWidgetIndex = 0;
        static const int tableViewWidgetIndex = 1;
        static const float eps = 0.001f;

        m_contentViewStackedLayout->setCurrentIndex(scale > eps ? tileViewWidgetIndex : tableViewWidgetIndex);
        m_sortTypeWidget->setVisible(m_contentViewStackedLayout->currentIndex() == tileViewWidgetIndex);

        updateTableViewColumnVisibilities();
    });

    // ToDo: ui should be blocked while file operation.
    connect(m_fileOperationMenu.get(), &NauProjectBrowserFileOperationsMenu::eventCopyRequested,
        NauFileOperations::copyToClipboard);

    connect(m_fileOperationMenu.get(), &NauProjectBrowserFileOperationsMenu::eventPasteRequested, [this](const QModelIndex& parent) {
        const auto prevCursor = cursor();
        const auto cursorReverter = qScopeGuard([prevCursor, this]{
            setCursor(prevCursor);
        });

        setCursor(Qt::CursorShape::BusyCursor);
        NauFileOperations::pasteFromClipboard(parent);
    });

    connect(m_fileOperationMenu.get(), &NauProjectBrowserFileOperationsMenu::eventCutRequested,
        NauFileOperations::cutToClipboard);

    connect(m_fileOperationMenu.get(), &NauProjectBrowserFileOperationsMenu::eventDuplicateRequested,
         [this](const QModelIndexList& indexes) {
        const auto prevCursor = cursor();
        const auto cursorReverter = qScopeGuard([prevCursor, this]{
            setCursor(prevCursor);
        });

        setCursor(Qt::CursorShape::BusyCursor);
        NauFileOperations::duplicate(indexes);
    });

    connect(m_fileOperationMenu.get(), &NauProjectBrowserFileOperationsMenu::eventRenameRequested,
        [this] (QAbstractItemView* view, const QModelIndex& index) {
            view->edit(index);
    });

    connect(m_fileOperationMenu.get(), &NauProjectBrowserFileOperationsMenu::eventCreateDirectoryRequested,
        [this] (QAbstractItemView* view, const QModelIndex& index) {
            const NauDir path = index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
            static const auto newFolderTemplate = tr("New Folder");
            const QString absNewFileName = NauFileOperations::generateFileNameIfExists(path.absoluteFilePath(newFolderTemplate));
            const QString relNewFileName = QFileInfo{absNewFileName}.fileName();
            const QModelIndex result = m_fsModel->mkdir(m_fsModel->index(path.absolutePath()), relNewFileName);
            const bool isValidAndNotFilteredOut =  result.isValid() && m_fsProxy->mapFromSource(result).isValid();

            NED_DEBUG("Create folder in: \"{}\" with name \"{}\" is {}", path.absolutePath().toUtf8().constData(),
                relNewFileName.toUtf8().constData(), result.isValid());

            if (isValidAndNotFilteredOut) {
                auto treeIdx = m_treeProxy->mapFromSource(m_fsProxy->mapFromSource(result));
                m_tree->setCurrentIndex(treeIdx);
                m_tree->edit(treeIdx);
            }
    });

    connect(m_fileOperationMenu.get(), &NauProjectBrowserFileOperationsMenu::eventDeleteRequested, 
        [this](QAbstractItemView* view, const QModelIndexList& indexes) {
        QStringList delFileList;

        for (const QModelIndex& index : indexes) {
            delFileList << index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
        }

        if (QMessageBox::question(this, tr("NauEditor"),
                tr("Delete selected item(s)? This action cannot be undone.")) != QMessageBox::StandardButton::Yes) {
            NED_TRACE("User rejected to delete resource(s)");
            return;
        }

        for (const auto& file : delFileList) {
            // Note that it's a bad idea to remove resource via WinAPI
            // Underlying Qt's FS model installs the watchers that can lead to files to be locked on Windows.
            const QModelIndex fsModelIndex = m_fsModel->index(file);
            const QString path = fsModelIndex.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
            const bool result = NauFileOperations::deletePathRecursively(path);
            
            if (!result) {
                NED_ERROR("Failed to move to trash {}", file.toUtf8().constData());
            } else {
                NED_TRACE("Successfully moved to trash {}", file.toUtf8().constData());
            }
        }

        setCurrentDirInList(m_tree->currentIndex());
    });

    connect(m_fileOperationMenu.get(), &NauProjectBrowserFileOperationsMenu::eventViewInShellRequested,
        [this](const QModelIndexList& indexes) {
#ifdef Q_OS_WIN
            for (const auto& index : indexes) {
                const QString path = index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
                NED_TRACE("Open in Explorer requested {} ", path.toUtf8().constData());

                const QFileInfo info{path};
                if (info.isDir()) {
                     QProcess::startDetached("explorer", {QDir::toNativeSeparators(path)});
                } else {
                     QProcess::startDetached("explorer", {"/select,", QDir::toNativeSeparators(path)});
                }
            }
#elif
#endif
    });

    connect(m_fileOperationMenu.get(), &NauProjectBrowserFileOperationsMenu::eventImportAssetRequested, [this] {
        emit eventImportClicked(m_lastTreeDirectory);
    });

    connect(filterWidget, &NauFilterWidget::eventChangeFilterRequested, [this](const QList<NauFilterWidgetAction*>& filters) {
        if (!m_fsProxy) return;

        QList<NauEditorFileType> filterList;
        filterList.reserve(filters.size());
        for (NauFilterWidgetAction* filter: filters) {
            filterList.emplaceBack(static_cast<NauProjectBrowserFilterWidgetAction*>(filter)->itemType());
        }

        m_fsProxy->setUserDefinedTypeFilters(filterList);

        setCurrentDirInList(m_tree->currentIndex());
        updateStateDuringFiltering();
    });
}

bool NauProjectBrowser::setProject(const NauProject& project)
{
    m_projectRootDir = project.dir();
    m_projectSourceDir = project.defaultSourceFolder();

    // We build following hierarchy of Model-View:
    //
    //   NauProjectBrowserFileSystemModel(m_fsModel: QFileSystemModel for direct acces to FS).
    //   |__NauProjectFileSystemProxyModel(m_fsProxy: a proxy to apply user's filter).
    //   |____NauProjectTreeProxyModel (m_treeProxy: a proxy to show only dirs in the tree view).
    //   |______NauProjectBrowserTreeView (m_tree)
    //   |____NauProjectTreeProxyModel (m_contentViewProxy: a proxy to show only files in the list view).
    //   |______NauProjectBrowserListView (m_list)

    NED_ASSERT(!m_itemTypeResolvers.empty());
    m_iconProvider = std::make_unique<NauProjectBrowserIconProvider>(m_itemTypeResolvers);
    installSourceDirWatcher();

    m_fsModel = std::make_unique<NauProjectBrowserFileSystemModel>(m_projectRootDir, m_itemTypeResolvers);
    m_fsModel->setIconProvider(m_iconProvider.get());
    m_fsModel->setReadOnly(false);

    m_fsProxy =  std::make_unique<NauProjectFileSystemProxyModel>(m_projectRootDir.absolutePath());
    m_fsProxy->setSourceModel(m_fsModel.get());
    m_fsProxy->setAllowedDirs({"\\content\\", "\\source\\"});
    m_fsProxy->setBlackListResourcesWildcard({"\\content\\shaders"});

    m_treeProxy = std::make_unique<NauProjectTreeProxyModel>();
    m_treeProxy->setSourceModel(m_fsProxy.get());

    m_contentViewProxy = std::make_unique<NauProjectContentProxyModel>();
    m_contentViewProxy->setSourceModel(m_fsProxy.get());
    
    m_treeDelegate = std::make_unique<NauProjectTreeDelegate>();
    m_contentViewTableDelegate = std::make_unique<NauProjectContentViewTableDelegate>();
    m_contentViewTileDelegate = std::make_unique<NauProjectContentViewTileDelegate>();

    m_tree->setModel(m_treeProxy.get());

    auto commonSelectionModel = new QItemSelectionModel(m_contentViewProxy.get(), this);

    m_contentListView->setDataAndSelectionModel(*m_contentViewProxy, *commonSelectionModel);
    m_contentListView->setSelectionModel(commonSelectionModel);
    m_contentListView->setItemDelegate(m_contentViewTileDelegate.get());
    m_contentListView->setViewMode(NauProjectBrowserListView::IconMode);
    m_contentListView->setMovement(NauProjectBrowserListView::Free);

    m_contentTableView->setDataAndSelectionModel(*m_contentViewProxy, *commonSelectionModel);
    m_contentTableView->setItemDelegate(m_contentViewTableDelegate.get());

    // TODO These startup values/states should be read from the user settings.
    m_contentViewScale->setScale(0.5);
    m_sortTypeWidget->setSortTypeAndOrder(NauSortType::Name, NauSortOrder::Ascending);

    m_fileOperationMenu->setProjectDir(m_projectRootDir);
    m_tree->setItemDelegate(m_treeDelegate.get());

    // Note that we clear indentation of tree itself, for our delegate can handle this.
    m_tree->setIndentation(0);

    // We need only 1st column(i.e. directory names).
    for (int idx = 1; idx < m_fsModel->columnCount(); ++idx) {
        m_tree->setColumnHidden(idx, true);
    }

    connect(m_contentListView, &NauProjectBrowserListView::doubleClicked, this,
        &NauProjectBrowser::handleContentViewDoubleClicked);

    connect(m_contentTableView, &NauProjectBrowserListView::doubleClicked, this,
        &NauProjectBrowser::handleContentViewDoubleClicked);

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, 
        this, [this](const QModelIndex& treeProxyCurrent, const QModelIndex&) {
        const QModelIndex fsProxyIndex = m_treeProxy->mapToSource(treeProxyCurrent);
        m_lastTreeDirectory = m_fsModel->filePath(m_fsProxy->mapToSource(fsProxyIndex));

        setCurrentDirInList(treeProxyCurrent);

        m_navigationBar->setNavigationChain(m_projectRootDir.relativeFilePath(m_lastTreeDirectory));
        m_contentViewSummary->setRootIndex(m_contentViewProxy->mapFromSource(fsProxyIndex));
    });

    connect(m_contentListView->selectionModel(), &QItemSelectionModel::selectionChanged,
        this, &NauProjectBrowser::updateAuxiliaryWidgets);

    connect(m_fsModel.get(), &NauProjectBrowserFileSystemModel::directoryLoaded, [this](const QString& path) {
        if (NauDir{path} == m_projectRootDir) {
            populateFsModel();

            updateAuxiliaryWidgets();
        }
    });

    connect(m_fsModel.get(), &NauProjectBrowserFileSystemModel::fileRenamed, [this]{
        // Renaming by Qt's fs model does not notify(by layoutChange, or dataChange).
        // So we have to resort the view by ourselves.
        m_contentViewProxy->invalidate();
        m_contentViewProxy->sort(0, m_contentViewProxy->sortOrder());

        updateAuxiliaryWidgets();
    });

    connect(m_contentViewProxy.get(), &NauProjectContentProxyModel::rowsInserted,
        this, &NauProjectBrowser::updateAuxiliaryWidgets);

    connect(m_contentTableView->header(), &QHeaderView::sortIndicatorChanged, 
        this, &NauProjectBrowser::handleSortTypeOrOrderChanged);

    connect(m_fsProxy.get(), &NauProjectFileSystemProxyModel::fileRenamed, [this](const NauDir& path,
        const QString& oldName, const QString& newName) {
        if (path.absoluteFilePath(oldName) == m_lastTreeDirectory) {
            setCurrentDirInTree(path.absoluteFilePath(newName));
            updateAuxiliaryWidgets();
        }
    });

    connect(m_fsProxy.get(), &NauProjectFileSystemProxyModel::layoutChanged,
        this, &NauProjectBrowser::handleFsProxyLayoutChanged);

    updateTreeRootIndex();
    setCurrentDirInTree(m_projectRootDir.absolutePath());
    updateTreeViewState();

    return true;
}

void NauProjectBrowser::setCurrentDir(const NauDir& currentDir)
{
    if (m_lastTreeDirectory != currentDir.absolutePath()) {
        setCurrentDirInTree(currentDir);
    }
}

void NauProjectBrowser::setAssetFileFilter(std::string_view assetExtension)
{
    m_fsProxy->setWhiteListResourcesWildcard({ QString(assetExtension.data()), "*.cpp", "*.hpp", "*.h" });
}

void NauProjectBrowser::setAssetTypeResolvers(std::vector<std::shared_ptr<NauProjectBrowserItemTypeResolverInterface>> itemTypeResolvers)
{
    m_itemTypeResolvers = std::move(itemTypeResolvers);
}

void NauProjectBrowser::applyFilter(const QString& filter)
{
    if (!m_fsProxy) return;

    m_fsProxy->setUserDefinedFilters(filter.split(NauDir::listSeparator(), Qt::SkipEmptyParts));
    setCurrentDirInList(m_tree->currentIndex());
    updateStateDuringFiltering();
}

void NauProjectBrowser::setCurrentDirInTree(const NauDir& dir)
{
    const auto fsIndex = m_fsModel->index(dir.absolutePath());
    const auto index = m_treeProxy->mapFromSource(m_fsProxy->mapFromSource(fsIndex));
    m_tree->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
}

void NauProjectBrowser::setCurrentDirInList(const QModelIndex& fsIndex)
{
    const QModelIndex fsProxyIndex = m_treeProxy->mapToSource(fsIndex);
    
    m_contentViewProxy->setCurrentRootIndex(fsProxyIndex);

    m_contentListView->setRootIndex(m_contentViewProxy->mapFromSource(fsProxyIndex));
    m_contentTableView->setRootIndex(m_contentViewProxy->mapFromSource(fsProxyIndex));
    m_contentTableView->selectionModel()->clear();

    m_fileOperationMenu->updateViewSelectionData(m_contentListView);
    m_fileOperationMenu->updateViewSelectionData(m_contentTableView);

    updateAuxiliaryWidgets();
}

void NauProjectBrowser::updateTreeRootIndex()
{
    NauDir dir = m_projectRootDir;

    // We want to populate a tree with dirs of a project with project dir as a root.
    // Project_Dir(m_projectRootDir)
    //  |_game
    //  |_..
    // 
    // If set the project dir itself as root index we will get a list of project dir with no root:
    //  game
    //  .. 
    // So we set a parent of project as a root, and hide all siblings of project dirs by a proxy model.
    dir.cdUp();

    const QModelIndex rootIndex = m_fsModel->setRootPath(dir.absolutePath());
    const QModelIndex proxyRootIndex = m_fsProxy->mapFromSource(rootIndex);

    m_tree->setRootIndex(m_treeProxy->mapFromSource(proxyRootIndex));
    m_fileOperationMenu->updateViewSelectionData(m_tree);
}

void NauProjectBrowser::updateTreeViewState()
{
    // Expand path to the current(bottom) to the top(root).
    QModelIndex index = m_tree->selectionModel()->currentIndex();
    while (index.isValid()) {
        m_tree->expand(index);
        index = index.parent();
    }
}

void NauProjectBrowser::updateAuxiliaryWidgets()
{
    const auto selected = m_contentListView->selectionModel()->selectedIndexes();
    QModelIndexList selection;

    for (const auto& index : selected) {
        if (index.column() == 0) {
            selection << index;
        }
    }

    m_contentViewSummary->setRootIndex(m_contentListView->rootIndex());
    m_contentViewCurentInfo->setRootIndex(m_contentListView->currentIndex());

    m_contentViewSummary->onSelectionChange(selection);
    m_contentViewCurentInfo->onSelectionChange(selection);
}

void NauProjectBrowser::updateStateDuringFiltering()
{
    updateTreeRootIndex();
    setCurrentDirInTree(m_fsProxy->hasUserDefinedFilters() || m_lastTreeDirectory.isEmpty()
        ? m_projectRootDir.absolutePath()
        : m_lastTreeDirectory);

    updateTableViewColumnVisibilities();
}

void NauProjectBrowser::updateTableViewColumnVisibilities()
{
    const bool relativePathVisible = m_fsProxy->hasUserDefinedFilters() ||
        m_contentViewProxy->dataStructure() == NauProjectContentProxyModel::DataStructure::Flat;

    m_contentTableView->setColumnHidden(+NauProjectBrowserFileSystemModel::Column::RelativePath, !relativePathVisible);
}

void NauProjectBrowser::handleFsProxyLayoutChanged()
{
    if (m_contentViewProxy->dataStructure() == NauProjectContentProxyModel::DataStructure::Flat)
    {
        setCurrentDirInList(m_tree->currentIndex());
        updateStateDuringFiltering();
    }
}

void NauProjectBrowser::handleContentViewDoubleClicked(const QModelIndex& index)
{
    const QString path = index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
    if (QFileInfo{path}.isDir()) {
        setCurrentDirInTree(path);
        return;
    }

    const auto type = static_cast<NauEditorFileType>(index.data(NauProjectBrowserFileSystemModel::FileItemTypeRole).toInt());
    emit eventFileDoubleClicked(path, type);
}

void NauProjectBrowser::handleSortTypeOrOrderChanged(int columnIndex, Qt::SortOrder order)
{
    const auto column = static_cast<NauProjectBrowserFileSystemModel::Column>(columnIndex);
    const auto currentSortRole = NauProjectBrowserFileSystemModel::columnToSortType(column);
    const auto currentSortOrder = Nau::fromQtSortOrder(order);

    m_sortTypeWidget->handleSortTypeAndOrder(currentSortRole, currentSortOrder);
}

void NauProjectBrowser::installSourceDirWatcher()
{
    m_projectSourceWatcher = std::make_unique<QFileSystemWatcher>();

    QDirIterator fsIt(m_projectSourceDir.absolutePath(),
        QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    while (fsIt.hasNext()) {
        const QString path = fsIt.next();
        m_projectSourceWatcher->addPath(path);
    }

    connect(m_projectSourceWatcher.get(), &QFileSystemWatcher::directoryChanged, this, &NauProjectBrowser::handleSourceDirChanged);
    connect(m_projectSourceWatcher.get(), &QFileSystemWatcher::fileChanged, this, &NauProjectBrowser::handleSourceDirChanged);
}

void NauProjectBrowser::handleSourceDirChanged()
{
    m_projectSourceWatcher.reset();
    emit eventSourceChanged(m_projectSourceDir);

    installSourceDirWatcher();
}

std::vector<NauEditorFileType> NauProjectBrowser::filterableItemTypes()
{
    static const auto filterableItemTypesRepo = std::vector<NauEditorFileType>{
            NauEditorFileType::Unrecognized,   NauEditorFileType::EngineCore, NauEditorFileType::Project,
            NauEditorFileType::Config,         NauEditorFileType::Texture,    NauEditorFileType::Material,
            NauEditorFileType::Model,          NauEditorFileType::Shader,     NauEditorFileType::Script,
            NauEditorFileType::VirtualRomFS,   NauEditorFileType::Scene,      NauEditorFileType::RawAudio,
            NauEditorFileType::AudioContainer, NauEditorFileType::UI,         NauEditorFileType::VFX,
            NauEditorFileType::Font,           NauEditorFileType::PhysicsMaterial, NauEditorFileType::Action
    };

    return filterableItemTypesRepo;
}

void NauProjectBrowser::populateFsModel()
{
    if (m_fsModelPopulated) return;
    m_fsModelPopulated = true;

    NED_TRACE("Force populating FS Model");

    // TODO: 
    // 1. Loading routine should be in separate thread.
    // 2. Show progress/waiting widget in this thread.
    const auto reverter = qScopeGuard([this]{ setEnabled(true); });
    setEnabled(false);

    const std::filesystem::path projectDir = m_projectRootDir.absolutePath().toUtf8().constData();

    for (const std::filesystem::directory_entry& entry : 
        std::filesystem::recursive_directory_iterator(projectDir)) {
        const QModelIndex index = m_fsModel->index(QString::fromUtf8(entry.path().string()));

        while (m_fsModel->canFetchMore(index))
            m_fsModel->fetchMore(index);
    }

    setCurrentDirInTree(m_projectRootDir.absoluteFilePath("content"));
    updateAuxiliaryWidgets();
}

NauWidget* NauProjectBrowser::createContentViewBottomContainer()
{
    auto container = new NauWidget();
    container->setObjectName("contentViewBottomContainer");

    auto layout = new NauLayoutHorizontal(container);
    m_contentViewScale = new NauProjectBrowserViewScaleWidget(container);
    m_contentViewScale->setObjectName("contentViewScale");
    m_contentViewScale->setFixedHeight(16);

    m_contentViewSummary = new NauProjectBrowserSummaryWidget(container);
    m_contentViewSummary->setObjectName("contentViewSummary");
    m_contentViewSummary->setFixedHeight(16);

    m_contentViewCurentInfo = new NauProjectBrowserInfoWidget(container);
    m_contentViewCurentInfo->setObjectName("contentViewCurentInfo");
    m_contentViewCurentInfo->setFixedHeight(16);

    layout->addWidget(m_contentViewCurentInfo, 1);
    layout->addWidget(m_contentViewSummary);
    layout->addWidget(m_contentViewScale);

    layout->setSpacing(8);
    layout->setContentsMargins(16, 8, 16, 8);
    return container;
}

NauWidget* NauProjectBrowser::createAddContentContainer()
{
    auto container = new NauWidget();
    container->setObjectName("addContentContainer");

    auto layout = new NauLayoutHorizontal(container);

    // Add content button
    auto addContentButton = new NauTertiaryButton(this);
    addContentButton->setObjectName("AddContentButton");
    addContentButton->setIcon(Nau::Theme::current().iconAddTertiaryStyle());
    addContentButton->setText(tr("Add"));
    addContentButton->setToolTip(tr("Create a new content in current content folder"));
    addContentButton->setFixedHeight(32);


    connect(addContentButton, &NauToolButton::clicked, this, [this]() {
        emit eventAddContentClicked(m_lastTreeDirectory);
    });

    // Import button
    auto importButton = new NauTertiaryButton(this);
    importButton->setObjectName("ImportButton");
    importButton->setIcon(Nau::Theme::current().iconImportAssetHeader());
    importButton->setText(tr("Import"));
    importButton->setToolTip(tr("Import asset to current content folder"));
    importButton->setFixedHeight(32);


    connect(importButton, &NauToolButton::clicked, this, [this]() {
        emit eventImportClicked(m_lastTreeDirectory);
    });

    layout->setAlignment(Qt::AlignLeft);

    layout->addWidget(addContentButton);
    layout->addWidget(importButton);

    return container;
}
