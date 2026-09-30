// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/prim-factory/nau_usd_prim_factory.hpp"
#include "nau_log.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"

#include "pxr/usd/usdGeom/xformCache.h"


// ** NauUsdPrimCreatorAbstract

pxr::UsdPrim NauUsdPrimCreatorAbstract::createPrim(pxr::UsdStageWeakPtr stage, const pxr::SdfPath& path, const pxr::TfToken& typeName,
    const std::string& displayName, const pxr::GfMatrix4d& initialTransform, bool isComponent)
{
    pxr::UsdPrim prim = createPrimInternal(stage, path, typeName);

    prim.SetDisplayName(displayName);

    auto xformable = PXR_NS::UsdGeomXformable(prim);
    if (xformable) {
        // Block objectchanged event during transform setting when creating object
        PXR_NS::TfNotice::Block block;
        PXR_NS::UsdGeomXformOp transformOp = NauUsdPrimUtils::forceAddTransformOpOrder(xformable);
        transformOp.Set(initialTransform);
    }

    // Set component kind
    if (isComponent) {
        prim.SetKind(pxr::TfToken("component"));
    }

    return prim;
}


// ** NauUsdPrimFactory

NauUsdPrimFactory& NauUsdPrimFactory::instance()
{
    static NauUsdPrimFactory instance;
    return instance;
}

void NauUsdPrimFactory::addCreator(const std::string& primType, NauUsdPrimCreatorAbstractPtr creator, const std::string& displayName)
{
    if (primType.empty()) {
        NED_ERROR("Usd prim factory: trying to add creator for empty type.");
        return;
    }

    if (!creator) {
        NED_ERROR("Usd prim factory: trying to add empty creator.");
        return;
    }

    std::string resultDisplayName = !displayName.empty() ? displayName : primType;
    m_typesDisplayNames[primType] = resultDisplayName;
    m_creators[primType] = creator;

}

pxr::UsdPrim NauUsdPrimFactory::createPrim(pxr::UsdStageWeakPtr stage, const pxr::SdfPath& path, const pxr::TfToken& typeName,
        const std::string& displayName, const pxr::GfMatrix4d& initialTransform, bool isComponent)
{
    NauUsdPrimCreatorAbstractPtr creator = m_creators[typeName.GetString()];

    std::string authoredName = displayName;
    if (isComponent) {
        if (auto it = m_typesDisplayNames.find(typeName.GetString()); it != m_typesDisplayNames.end()) {
            authoredName = it->second;
        }
    }

    return creator->createPrim(stage, path, typeName, authoredName, initialTransform, isComponent);
}

std::vector<std::string> NauUsdPrimFactory::registeredAllPrimCreators() const
{
    std::vector<std::string> types;

    for (const auto& resourceType : m_creators) {
        types.push_back(resourceType.first);
    }

    return types;
}

std::map<std::string, std::string> NauUsdPrimFactory::registeredPrimCreatorsWithDisplayNames(primFilter filter) const
{
    std::map<std::string, std::string> types;
    for (const auto& resourceType : m_typesDisplayNames) {
        if (filter(resourceType.first)) {
            types[resourceType.first] = resourceType.second;
        }
    }

    return types;
}
