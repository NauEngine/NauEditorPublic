// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/outliner/nau_usd_outliner_client.hpp"
#include "nau/outliner/nau_world_outline_panel.hpp"
#include "nau_log.hpp"
#include "nau_assert.hpp"
#include "nau/prim-factory/nau_usd_prim_factory.hpp"
#include "themes/nau_theme.hpp"

#include "nau/utils/nau_usd_editor_utils.hpp"

#include "pxr/usd/usd/primRange.h"
#include "usd_proxy/usd_prim_proxy.h"


//TODO: Refine the api architecture of client-widget interaction in the future
// ** NauUsdOutlinerClient

NauUsdOutlinerClient::NauUsdOutlinerClient(NauWorldOutlinerWidget* outlinerWidget, NauWorldOutlineTableWidget& outlinerTab, const NauUsdSelectionContainerPtr& selectionContainer)
    : m_outlinerWidget(outlinerWidget)
    , m_selectionContainer(selectionContainer)
    , m_outlinerTab(outlinerTab)
{
}

void NauUsdOutlinerClient::handleNotice(NauUITranslatorProxyNotice const& notice)
{
    auto stage = notice.stage();
    const pxr::SdfPathVector& changedObjects = notice.resyncedPaths();
    for (const auto& object : changedObjects) {
        const pxr::SdfPath primPath = object.GetPrimPath();
        pxr::UsdPrim prim = stage->GetPrimAtPath(primPath);

        // If the object exists, we create it in the outliner; if not - skip it
        if (auto treeWidget = itemFromPath(primPath.GetString()); treeWidget == nullptr) {
            if (prim) {
                addItemFromPrim(prim);
            }
        } else {

            if (prim) {
                updateItemFromPrim(prim);
            } else {
                removeItemsFromPrimPath(primPath);
                // We deleted selected objects
                m_selectionContainer->clear();
            }
        }
    }

    const pxr::SdfPathVector& changedInfo = notice.infoChanges();
    for (const auto& objectInfo : changedInfo) {
        pxr::UsdPrim prim = stage->GetPrimAtPath(objectInfo.GetPrimPath());
        updateItemFromPrim(prim);
    }
}

NauWorldOutlinerWidget* NauUsdOutlinerClient::outlinerWidget()
{
    return m_outlinerWidget;
}

void NauUsdOutlinerClient::deleteItems()
{
    emit eventPrimDeleteRequested();
}

void NauUsdOutlinerClient::renameItems(QTreeWidgetItem* item, const QString& newName)
{
    const std::string primPath = item->text(+NauWorldOutlineTableWidget::Columns::Guid).toUtf8().constData();
    emit eventPrimRenameRequested(primPath, newName.toUtf8().constData());
}

void NauUsdOutlinerClient::copyItems()
{
    m_bufferItems.clear();

    m_bufferItems = m_selectionContainer->selection();
}

void NauUsdOutlinerClient::pasteItems()
{
    emit eventPrimsPasteRequested(m_bufferItems);
}

void NauUsdOutlinerClient::duplicateItems()
{
    emit eventPrimsDuplicateRequested();
}

void NauUsdOutlinerClient::createItem(const std::string& typeName)
{
    emit eventPrimCreateRequested(typeName);
}

void NauUsdOutlinerClient::focusOnItem(QTreeWidgetItem* item)
{
    const std::string primPath = item->text(+NauWorldOutlineTableWidget::Columns::Guid).toUtf8().constData();
    emit eventFocusOnPrim(primPath);
}

void NauUsdOutlinerClient::moveItems(const QModelIndex& destination, const std::vector<QString>& guids)
{
    QTreeWidgetItem* destItem = m_outlinerTab.itemFromIndex(destination);
    const std::string destPath = destItem
        ? destItem->text(+NauWorldOutlineTableWidget::Columns::Guid).toStdString()
        : "/";

    for (const auto& guid : guids) {
        const std::string movingPath = guid.toStdString();
        emit eventPrimReparentRequested(movingPath, destPath);
    }
}

void NauUsdOutlinerClient::addItemFromPrim(PXR_NS::UsdPrim newPrim)
{
    if (NauUsdPrimUtils::isPrimComponent(newPrim)) {
        return;
    }

    QSignalBlocker blocker(m_outlinerTab);

    auto item = new QTreeWidgetItem;
    updateItemFromPrimInternal(item, newPrim);

    PXR_NS::UsdPrim parentPrim = newPrim.GetParent();

    auto type = newPrim.GetTypeName().GetString();
    auto primPath = newPrim.GetPath().GetString();

    if (parentPrim.IsValid()) {
        if (QTreeWidgetItem* parentTreeItem = itemFromPath(parentPrim.GetPath().GetString()); parentTreeItem) {
            parentTreeItem->addChild(item);
            m_outlinerTab.expandItem(parentTreeItem);

            auto proxyPrim = UsdProxy::UsdProxyPrim(newPrim);
            auto componentType = proxyPrim.getProperty(pxr::TfToken("componentTypeName"));

            if (componentType && componentType->isValid()) {
                PXR_NS::VtValue value;
                componentType->getValue(&value);

                auto engineTypeName = value.Get<std::string>();

                // TODO: Dirty hack
                // Fix in future updates
                if (engineTypeName == "nau::scene::EnvironmentComponent") {
                    m_outlinerWidget->updateCreationList(engineTypeName, true);
                }
            }

            emit eventBillboardCreateRequested(type, primPath);
            return;
        }
    }

    m_outlinerTab.addTopLevelItem(item);

    auto proxyPrim = UsdProxy::UsdProxyPrim(newPrim);
    auto componentType = proxyPrim.getProperty(pxr::TfToken("componentTypeName"));

    if (componentType && componentType->isValid()) {
        PXR_NS::VtValue value;
        componentType->getValue(&value);

        auto engineTypeName = value.Get<std::string>();

        // TODO: Dirty hack
        // Fix in future updates
        if (engineTypeName == "nau::scene::EnvironmentComponent") {
            m_outlinerWidget->updateCreationList(engineTypeName, true);
        }
    }

    emit eventBillboardCreateRequested(type, primPath);
}

void NauUsdOutlinerClient::updateItemFromPrim(PXR_NS::UsdPrim prim)
{
    QSignalBlocker blocker(m_outlinerTab);

    const std::string primPath = prim.GetPath().GetString();
   
    if (QTreeWidgetItem* treeItem = itemFromPath(primPath); treeItem) {
        updateItemFromPrimInternal(treeItem, prim);
    }
}

void NauUsdOutlinerClient::removeItemsFromPrimPath(const PXR_NS::SdfPath& removedPath)
{
    QSignalBlocker blocker(m_outlinerTab);

    if (QTreeWidgetItem* treeItem = itemFromPath(removedPath.GetString()); treeItem) {
        const QModelIndex modelIndex = m_outlinerTab.indexFromItem(treeItem);

        QModelIndex parentModelIndex;
        if (QTreeWidgetItem* treeItemParent = treeItem->parent(); treeItemParent) {
            parentModelIndex = m_outlinerTab.indexFromItem(treeItemParent);
        }

        m_outlinerTab.model()->removeRow(modelIndex.row(), parentModelIndex);
    }
}

void NauUsdOutlinerClient::updateItemFromPrimInternal(QTreeWidgetItem* item, pxr::UsdPrim prim)
{
    const std::string displayName = prim.GetDisplayName().empty() ? prim.GetPath().GetName() : prim.GetDisplayName();
    const std::string typeName = prim.GetTypeName().GetString();
    const std::string fullPath = prim.GetPrimPath().GetString();

    // TODO: put back once implemented
    // item->setIcon(+NauWorldOutlineTableWidget::Columns::Visibility, *m_outlinerTab.visibilityIcon());
    item->setIcon(+NauWorldOutlineTableWidget::Columns::Disabled, *m_outlinerTab.availabilityIcon());
    item->setIcon(+NauWorldOutlineTableWidget::Columns::Name, Nau::Theme::current().iconResourcePlaceholder());
    item->setText(+NauWorldOutlineTableWidget::Columns::Name, displayName.c_str());
    item->setText(+NauWorldOutlineTableWidget::Columns::Type, typeName.c_str());
    // item->setText(+NauWorldOutlineTableWidget::Columns::Modified, "01.01.1970");
    // item->setText(+NauWorldOutlineTableWidget::Columns::Tags, {});
    // item->setText(+NauWorldOutlineTableWidget::Columns::Layer, "Main layer: 0");
    item->setText(+NauWorldOutlineTableWidget::Columns::Guid, fullPath.c_str());

    item->setFont(+NauWorldOutlineTableWidget::Columns::Name, Nau::Theme::current().fontWorldOutlineSemibold());
    item->setFont(+NauWorldOutlineTableWidget::Columns::Type, Nau::Theme::current().fontWorldOutlineRegular());
    // item->setFont(+NauWorldOutlineTableWidget::Columns::Modified, Nau::Theme::current().fontWorldOutlineRegular());
    // item->setFont(+NauWorldOutlineTableWidget::Columns::Tags, Nau::Theme::current().fontWorldOutlineRegular());
    // item->setFont(+NauWorldOutlineTableWidget::Columns::Layer, Nau::Theme::current().fontWorldOutlineRegular());
    item->setFont(+NauWorldOutlineTableWidget::Columns::Guid, Nau::Theme::current().fontWorldOutlineRegular());

    auto itemFlags = item->flags();
    itemFlags.setFlag(Qt::ItemIsEditable, true);
    itemFlags.setFlag(Qt::ItemIsDragEnabled, true);
    itemFlags.setFlag(Qt::ItemIsSelectable, true);

    item->setFlags(itemFlags);
}

void NauUsdOutlinerClient::rebuildCreationList() {
    std::vector<std::pair<std::string, std::string>> creationList;
    auto const &factory = NauUsdPrimFactory::instance();

    // Basic objects
    auto basicFilter = [](const std::string &type) {
        return type == "Xform" || type == "NauAssetMesh" || type == "NauAssetVFX";
    };
    auto types = factory.registeredPrimCreatorsWithDisplayNames(basicFilter);
    for (const auto &[typeName, displayName]: types) {
        creationList.emplace_back(typeName, displayName);
    }

    //Audio
    creationList.emplace_back("", "");
    auto audioFilter = [](const std::string &type) {
        return type == "AudioEmitter";
    };
    types = factory.registeredPrimCreatorsWithDisplayNames(audioFilter);
    for (const auto &[typeName, displayName]: types) {
        creationList.emplace_back(typeName, displayName);
    }

    //Lightning
    creationList.emplace_back("", "");
    auto lightningFilter = [](const std::string &type) {
        return  type.find("nau::scene::") != std::string::npos &&
                type.find("Light") != std::string::npos ||
                type.find("light") != std::string::npos ||
                type.find("Environment") != std::string::npos;
    };
    types = factory.registeredPrimCreatorsWithDisplayNames(lightningFilter);
    for (const auto &[typeName, displayName]: types) {
        creationList.emplace_back(typeName, displayName);
    }

    //Camera
    creationList.emplace_back("", "");
    auto cameraFilter = [](const std::string &type) {
        return type.find("Camera") != std::string::npos;
    };
    types = factory.registeredPrimCreatorsWithDisplayNames(cameraFilter);
    for (const auto &[typeName, displayName]: types) {
        creationList.emplace_back(typeName, displayName);
    }

   if (m_outlinerWidget) m_outlinerWidget->getHeaderWidget().creationList()->initTypesList(creationList);
}

QTreeWidgetItem* NauUsdOutlinerClient::itemFromPath(const std::string& path)
{
    auto itemsList = m_outlinerTab.findItems(path.c_str(), Qt::MatchRecursive, +NauWorldOutlineTableWidget::Columns::Guid);

    if (itemsList.count() > 1) {
        NED_ERROR("Found more then one objects with path {}.", path);
        return nullptr;
    }


    return !itemsList.isEmpty() ? itemsList.at(0) : nullptr;
}


void NauUsdOutlinerClient::updateItemsFromScene(pxr::UsdStageRefPtr scene)
{
    m_outlinerTab.clear();

    for (auto primRange : scene->TraverseAll()) {
        addItemFromPrim(primRange);
    }
}

void NauUsdOutlinerClient::clearItems()
{
    m_outlinerTab.clear();
}

std::string NauUsdOutlinerClient::pathFromModelIndex(const QModelIndex& index) const
{
    return index.data(+Qt::DisplayRole).toString().toUtf8().constData();
}
