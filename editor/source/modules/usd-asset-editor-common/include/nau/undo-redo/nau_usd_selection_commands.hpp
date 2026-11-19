// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Provides undo/redo commands for selection.

#pragma once

#include "nau/nau_usd_asset_editor_common_config.hpp"
#include "nau_document_commands.hpp"
#include "nau/selection/nau_usd_selection_container.hpp"


// ** NauCommandSelectionChanged

// Moves to node ids with the selection container (T1.5); until then it needs a USD document.

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandSelectionChanged : public NauAbstractDocumentCommand
{
public:
    NauCommandSelectionChanged(
        NauDocumentPtr document,
        const NauUsdSelectionContainerPtr selectionContainer,
        const NauUsdPrimsSelection& oldSelection,
        const NauUsdPrimsSelection& newSelection
    );

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    std::vector<std::string> collectPaths(const NauUsdPrimsSelection& selection);
    pxr::UsdStageRefPtr stage() const;

private:
    // TODO: Usd weak ptr
    NauUsdSelectionContainerPtr m_selectionContainer;

    const NauUsdPrimsSelection m_oldSelection;
    const NauUsdPrimsSelection m_newSelection;
};