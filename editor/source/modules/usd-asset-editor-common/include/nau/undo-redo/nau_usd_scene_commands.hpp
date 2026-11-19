// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Path-based constructors of the document commands (nau_document_commands.hpp), for call sites that don't use node ids
// yet (USD migration Stage 1). They need a USD document. Remove each one when its last call site moves to node ids.

#pragma once

#include "nau/nau_usd_asset_editor_common_config.hpp"
#include "nau/undo-redo/nau_document_commands.hpp"

#include "pxr/usd/usd/stage.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/base/gf/matrix4d.h"

#include "usd_proxy/usd_proxy.h"

#include <string>


// ** NauCommandCreateUsdPrim

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandCreateUsdPrim : public NauCommandCreateNode
{
public:
    // The prim gets exactly this path; its parent must exist
    NauCommandCreateUsdPrim(
        NauDocumentPtr document,
        const std::string& path,
        const std::string& typeName,
        const std::string& displayName,
        const PXR_NS::GfMatrix4d& initialTransform,
        bool isComponent
    );
};


// ** NauCommandRemoveUsdPrim

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandRemoveUsdPrim : public NauCommandRemoveNode
{
public:
    NauCommandRemoveUsdPrim(NauDocumentPtr document, const std::string& path);
};


// ** NauCommandRenameUsdPrim

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandRenameUsdPrim : public NauCommandRenameNode
{
public:
    NauCommandRenameUsdPrim(NauDocumentPtr document, const std::string& path, const std::string& newName);
};


// ** NauCommandReparentUsdPrim

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandReparentUsdPrim : public NauCommandReparentNode
{
public:
    NauCommandReparentUsdPrim(NauDocumentPtr document, const std::string& currentPath, const std::string& newParentPath);
};


// ** NauCommandChangeUsdPrimProperty

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandChangeUsdPrimProperty : public NauCommandChangeNodeProperty
{
public:
    NauCommandChangeUsdPrimProperty(NauDocumentPtr document, const PXR_NS::SdfPath& path, const PXR_NS::TfToken& propName, const PXR_NS::VtValue& newValue);
    NauCommandChangeUsdPrimProperty(NauDocumentPtr document, const PXR_NS::SdfPath& path, const PXR_NS::TfToken& propName, const PXR_NS::VtValue& initialValue, const PXR_NS::VtValue& newValue);
};


// ** NauCommandChangeUsdPrimAssetReference

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandChangeUsdPrimAssetReference : public NauCommandChangeNodeAssetReference
{
public:
    // The values hold an SdfReference
    NauCommandChangeUsdPrimAssetReference(NauDocumentPtr document, const PXR_NS::SdfPath& path, const PXR_NS::VtValue& newValue);
    NauCommandChangeUsdPrimAssetReference(NauDocumentPtr document, const PXR_NS::SdfPath& path, const PXR_NS::VtValue& initialValue, const PXR_NS::VtValue& newValue);
};
