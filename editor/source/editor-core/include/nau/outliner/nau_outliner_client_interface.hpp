// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Outliner client interface

#pragma once

#include "nau/nau_editor_config.hpp"
#include "nau/outliner/nau_world_outline_panel.hpp"

#include <vector>
#include <QTreeWidgetItem>


// ** NauOutlinerClientInterface

class NAU_EDITOR_API NauOutlinerClientInterface : public QObject
{
    Q_OBJECT

public:
    virtual ~NauOutlinerClientInterface() = default;

    virtual void rebuildCreationList() = 0;
    virtual void renameItems(QTreeWidgetItem* item, const QString& newName) = 0;
    virtual void deleteItems() = 0;
    virtual void copyItems() = 0;
    virtual void pasteItems() = 0;
    virtual void duplicateItems() = 0;
    virtual void focusOnItem(QTreeWidgetItem* item) = 0;
    virtual void moveItems(const QModelIndex& destination, const std::vector<QString>& guids) = 0;
    virtual void createItem(const std::string& typeName) = 0;  
};