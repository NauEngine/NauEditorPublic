// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_gui_outliner_client.hpp"

#include "nau/prim-factory/nau_usd_prim_factory.hpp"


//TODO: Refine the api architecture of client-widget interaction in the future
// ** NauUsdOutlinerClient

NauGuiOutlinerClient::NauGuiOutlinerClient(NauWorldOutlinerWidget* outlinerWidget, NauWorldOutlineTableWidget& outlinerTab, const NauUsdSelectionContainerPtr& selectionContainer)
    : NauUsdOutlinerClient(outlinerWidget, outlinerTab, selectionContainer)
{
}

void NauGuiOutlinerClient::rebuildCreationList() {
    std::vector<std::pair<std::string, std::string>> creationList;
    auto types = NauUsdPrimFactory::instance().registeredPrimCreatorsWithDisplayNames([](const std::string& value){
         return value.find("NauGui") != std::string::npos;
    });
    for (const auto& [typeName, displayName] : types) {
        creationList.emplace_back(typeName, displayName);
    }

    if (outlinerWidget()) outlinerWidget()->getHeaderWidget().creationList()->initTypesList(creationList);
}
