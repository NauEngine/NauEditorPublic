// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Outliner client USD implementation

#pragma once

#include "nau/nau_usd_asset_editor_common_config.hpp"
#include "nau/outliner/nau_outliner_client_interface.hpp"
#include "nau/ui-translator/nau_usd_scene_ui_translator.hpp"
#include "nau/selection/nau_usd_selection_container.hpp"
#include "nau/nau_usd_scene_config.hpp"

#include <pxr/usd/usd/prim.h>

#include <QObject>


// ** NauUsdOutlinerClient

class NAU_USD_ASSET_EDITOR_COMMON_API NauUsdOutlinerClient : public NauOutlinerClientInterface
                                                           , public NauUITranslatorClientInterface
{
    Q_OBJECT
public:
    NauUsdOutlinerClient(NauWorldOutlinerWidget* outlinerWidget, NauWorldOutlineTableWidget& outlinerTab, const NauUsdSelectionContainerPtr& selectionContainer);

    NauWorldOutlinerWidget* outlinerWidget();

    void handleNotice(NauUITranslatorProxyNotice const& notice) override;

    // TODO: Needed here?
    void deleteItems() override;
    void copyItems() override;
    void pasteItems() override;
    void duplicateItems() override;
    void renameItems(QTreeWidgetItem* item, const QString& newName) override;
    void createItem(const std::string& typeName) override;
    void focusOnItem(QTreeWidgetItem* item) override;
    void moveItems(const QModelIndex& destination, const std::vector<QString>& guids) override;

    void addItemFromPrim(PXR_NS::UsdPrim newPrim);
    void updateItemFromPrim(PXR_NS::UsdPrim changedPrim);

    void removeItemsFromPrimPath(const PXR_NS::SdfPath& removedPath);

    // Use only for when new scene loaded
    void updateItemsFromScene(PXR_NS::UsdStageRefPtr scene);
    void clearItems();

    std::string pathFromModelIndex(const QModelIndex& index) const;    
    QTreeWidgetItem* itemFromPath(const std::string& path);

signals:
    void eventPrimDeleteRequested();
    void eventPrimRenameRequested(const std::string& path, const std::string& newName);
    void eventPrimCreateRequested(const std::string& typeName);

    void eventBillboardCreateRequested(const std::string& typeName, const std::string& path);

    void eventPrimsPasteRequested(const std::vector<PXR_NS::UsdPrim>& primsToPaste);
    void eventPrimsDuplicateRequested();
    void eventPrimReparentRequested(const std::string& srcPath, const std::string& dstPath);

    void eventFocusOnPrim(const std::string& path);

private:
    void updateItemFromPrimInternal(QTreeWidgetItem* item, PXR_NS::UsdPrim prim);
    virtual void rebuildCreationList();

private:
    PXR_NS::UsdStageRefPtr m_currentScene;
    NauUsdSelectionContainerPtr m_selectionContainer;
    std::vector<pxr::UsdPrim> m_bufferItems;

    NauWorldOutlinerWidget* m_outlinerWidget;
    NauWorldOutlineTableWidget& m_outlinerTab;
};
