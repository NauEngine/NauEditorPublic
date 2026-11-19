// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Opening documents and registering node types, without pxr. For editors that use only NauDocumentInterface.

#pragma once

#include "nau/nau_usd_asset_editor_common_config.hpp"
#include "nau/document/nau_document_interface.hpp"

#include <string>


namespace Nau
{
    // Process-wide node type registry. In Stage 1 it registers creators with NauUsdPrimFactory. Types registered with
    // the factory directly are listed too, with their type and display name only.
    NAU_USD_ASSET_EDITOR_COMMON_API NauDocumentTypeRegistryInterface& DocumentTypeRegistry();

    // See NauUsdDocument::open, createNew and createInMemory. Return nullptr on failure.
    NAU_USD_ASSET_EDITOR_COMMON_API NauDocumentPtr OpenUsdDocument(const std::string& path);
    NAU_USD_ASSET_EDITOR_COMMON_API NauDocumentPtr CreateUsdDocument(const std::string& path);
    NAU_USD_ASSET_EDITOR_COMMON_API NauDocumentPtr CreateInMemoryUsdDocument();
}
