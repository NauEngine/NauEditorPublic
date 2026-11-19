// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/undo-redo/nau_usd_scene_commands.hpp"
#include "nau/document-usd/nau_usd_document.hpp"
#include "nau_log.hpp"

#include "pxr/usd/sdf/reference.h"

#include <optional>


namespace
{
    NauDocumentNodeId nodeFromPath(const NauDocumentPtr& document, const PXR_NS::SdfPath& path)
    {
        const auto* usdDocument = dynamic_cast<const NauUsdDocument*>(document.get());
        if (!usdDocument) {
            NED_ERROR("Usd scene command: not a USD document, path: {}", path.GetString());
            return {};
        }

        return usdDocument->nodeFromPath(path);
    }

    NauDocumentNodeCreateInfo createInfo(const NauDocumentPtr& document, const std::string& path, const std::string& typeName,
        const std::string& displayName, const PXR_NS::GfMatrix4d& initialTransform, bool isComponent)
    {
        // The caller made the name unique, so the document keeps it
        const PXR_NS::SdfPath primPath(path);
        return {
            .parent = nodeFromPath(document, primPath.GetParentPath()),
            .typeName = typeName,
            .kind = isComponent ? NauDocumentNodeKind::Component : NauDocumentNodeKind::Object,
            .nameHint = primPath.GetName(),
            .displayName = displayName,
            .transform = NauUsdDocument::documentMatrix(initialTransform),
        };
    }

    // The gizmo passes the USD attribute of the local transform
    std::string propertyKey(const PXR_NS::TfToken& propName)
    {
        return propName == "xformOp:transform" ? std::string(NauDocumentPropertyKeys::transform) : propName.GetString();
    }

    std::optional<NauDocumentValue> propertyValue(const NauDocumentPtr& document, const PXR_NS::SdfPath& path,
        const PXR_NS::TfToken& propName, const PXR_NS::VtValue& value)
    {
        const auto* usdDocument = dynamic_cast<const NauUsdDocument*>(document.get());
        if (!usdDocument || value.IsEmpty()) {
            return std::nullopt;
        }

        return usdDocument->documentValue(usdDocument->nodeFromPath(path), propertyKey(propName), value);
    }

    std::optional<NauDocumentAssetRef> assetReference(const PXR_NS::VtValue& value)
    {
        if (!value.IsHolding<PXR_NS::SdfReference>()) {
            return std::nullopt;
        }

        return NauUsdDocument::documentAssetReference(value.UncheckedGet<PXR_NS::SdfReference>());
    }
}


// ** NauCommandCreateUsdPrim

NauCommandCreateUsdPrim::NauCommandCreateUsdPrim(
    NauDocumentPtr document,
    const std::string& path,
    const std::string& typeName,
    const std::string& displayName,
    const PXR_NS::GfMatrix4d& initialTransform,
    bool isComponent
    )
    : NauCommandCreateNode(document, createInfo(document, path, typeName, displayName, initialTransform, isComponent))
{
}


// ** NauCommandRemoveUsdPrim

NauCommandRemoveUsdPrim::NauCommandRemoveUsdPrim(NauDocumentPtr document, const std::string& path)
    : NauCommandRemoveNode(document, nodeFromPath(document, PXR_NS::SdfPath(path)))
{
}


// ** NauCommandRenameUsdPrim

NauCommandRenameUsdPrim::NauCommandRenameUsdPrim(NauDocumentPtr document, const std::string& path, const std::string& newName)
    : NauCommandRenameNode(document, nodeFromPath(document, PXR_NS::SdfPath(path)), newName)
{
}


// ** NauCommandReparentUsdPrim

NauCommandReparentUsdPrim::NauCommandReparentUsdPrim(NauDocumentPtr document, const std::string& currentPath, const std::string& newParentPath)
    : NauCommandReparentNode(document, nodeFromPath(document, PXR_NS::SdfPath(currentPath)), nodeFromPath(document, PXR_NS::SdfPath(newParentPath)))
{
}


// ** NauCommandChangeUsdPrimProperty

NauCommandChangeUsdPrimProperty::NauCommandChangeUsdPrimProperty(
    NauDocumentPtr document,
    const PXR_NS::SdfPath& path,
    const PXR_NS::TfToken& propName,
    const PXR_NS::VtValue& newValue
    )
    : NauCommandChangeNodeProperty(document, nodeFromPath(document, path), propertyKey(propName),
        document->property(nodeFromPath(document, path), propertyKey(propName)), propertyValue(document, path, propName, newValue))
{
}

NauCommandChangeUsdPrimProperty::NauCommandChangeUsdPrimProperty(
    NauDocumentPtr document,
    const PXR_NS::SdfPath& path,
    const PXR_NS::TfToken& propName,
    const PXR_NS::VtValue& initialValue,
    const PXR_NS::VtValue& newValue
    )
    : NauCommandChangeNodeProperty(document, nodeFromPath(document, path), propertyKey(propName),
        propertyValue(document, path, propName, initialValue), propertyValue(document, path, propName, newValue))
{
}


// ** NauCommandChangeUsdPrimAssetReference

NauCommandChangeUsdPrimAssetReference::NauCommandChangeUsdPrimAssetReference(NauDocumentPtr document, const PXR_NS::SdfPath& path, const PXR_NS::VtValue& newValue)
    : NauCommandChangeNodeAssetReference(document, nodeFromPath(document, path), std::nullopt, assetReference(newValue))
{
}

NauCommandChangeUsdPrimAssetReference::NauCommandChangeUsdPrimAssetReference(NauDocumentPtr document, const PXR_NS::SdfPath& path, const PXR_NS::VtValue& initialValue, const PXR_NS::VtValue& newValue)
    : NauCommandChangeNodeAssetReference(document, nodeFromPath(document, path), assetReference(initialValue), assetReference(newValue))
{
}
