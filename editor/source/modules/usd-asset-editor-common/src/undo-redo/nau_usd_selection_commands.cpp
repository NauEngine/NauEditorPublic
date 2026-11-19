// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/undo-redo/nau_usd_selection_commands.hpp"
#include "nau/document-usd/nau_usd_document.hpp"
#include "nau_log.hpp"


// ** NauCommandSelectionChanged

NauCommandSelectionChanged::NauCommandSelectionChanged(
    NauDocumentPtr document,
    const NauUsdSelectionContainerPtr selectionContainer,
    const NauUsdPrimsSelection& oldSelection,
    const NauUsdPrimsSelection& newSelection
    )
    : NauAbstractDocumentCommand(std::move(document))
    , m_selectionContainer(selectionContainer)
    , m_oldSelection(oldSelection)
    , m_newSelection(newSelection)
{
}

void NauCommandSelectionChanged::execute()
{
    m_selectionContainer->updateFromPaths(stage(), collectPaths(m_newSelection));
    NED_DEBUG("Execute prim selection command");
}

void NauCommandSelectionChanged::undo()
{
    m_selectionContainer->updateFromPaths(stage(), collectPaths(m_oldSelection));
    NED_DEBUG("Undo prim selection command");
}

NauCommandDescriptor NauCommandSelectionChanged::description() const
{
    return {
        .id = id,
        .name = "Selection changing",
        .objectId = "EditorSelection"
    };
}

std::vector<std::string> NauCommandSelectionChanged::collectPaths(const NauUsdPrimsSelection& selection)
{
    std::vector<std::string> selectedPaths;
    for (const pxr::UsdPrim& prim : selection) {
        selectedPaths.push_back(prim.GetPath().GetString());
    }

    return selectedPaths;
}

pxr::UsdStageRefPtr NauCommandSelectionChanged::stage() const
{
    const auto* usdDocument = dynamic_cast<const NauUsdDocument*>(m_document.get());
    return usdDocument ? usdDocument->stage() : pxr::UsdStageRefPtr();
}
