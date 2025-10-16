// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Outliner client for UI-USD implementation

#pragma once

#include "nau/outliner/nau_usd_outliner_client.hpp"
#include "../nau_gui_editor_config.hpp"


// ** NauUsdOutlinerClient

class NAU_GUI_EDITOR_API NauGuiOutlinerClient : public NauUsdOutlinerClient
{
    Q_OBJECT

public:
    NauGuiOutlinerClient(NauWorldOutlinerWidget* outlinerWidget, NauWorldOutlineTableWidget& outlinerTab, const NauUsdSelectionContainerPtr& selectionContainer);
private:
    void rebuildCreationList() override;
};
