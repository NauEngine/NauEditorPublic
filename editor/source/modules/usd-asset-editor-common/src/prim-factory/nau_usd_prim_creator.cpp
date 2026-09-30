// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/prim-factory/nau_usd_prim_creator.hpp"

#include "pxr/usd/usd/references.h"
#include "pxr/usd/usd/attribute.h"


// ** NauDefaultUsdPrimCreator

pxr::UsdPrim NauDefaultUsdPrimCreator::createPrimInternal(pxr::UsdStageWeakPtr stage, const pxr::SdfPath& path, const pxr::TfToken& typeName)
{
    return stage->DefinePrim(pxr::SdfPath(path), pxr::TfToken(typeName));
}


// ** NauResourceUsdPrimCreator

NauResourceUsdPrimCreator::NauResourceUsdPrimCreator(const std::string& defaultAssetPath, const pxr::SdfPath& primPath)
    : m_defaultAssetPath(defaultAssetPath)
    , m_primPath(primPath)
{

}

pxr::UsdPrim NauResourceUsdPrimCreator::createPrimInternal(pxr::UsdStageWeakPtr stage, const pxr::SdfPath& path, const pxr::TfToken& typeName)
{
    pxr::UsdPrim newPrim = stage->DefinePrim(pxr::SdfPath(path));
    pxr::UsdReferences references = newPrim.GetReferences();

    PXR_NS::SdfReference ref;
    ref.SetAssetPath(m_defaultAssetPath);
    ref.SetPrimPath(m_primPath);

    references.AddReference(ref);

    newPrim.Load();

    return newPrim;
}

NauUsdPrimComponentCreator::NauUsdPrimComponentCreator(const std::string& typeName)
    : m_typeName(typeName)
{
}

pxr::UsdPrim NauUsdPrimComponentCreator::createPrimInternal(pxr::UsdStageWeakPtr stage, const pxr::SdfPath& path, const pxr::TfToken& typeName)
{
    pxr::UsdPrim newPrim = stage->DefinePrim(pxr::SdfPath(path));
    if (pxr::UsdAttribute attr = newPrim.CreateAttribute(pxr::TfToken("componentTypeName"), pxr::SdfValueTypeNames->String)) {
        attr.Set(m_typeName);
        attr.SetHidden(true);
    }
    // Warning. SetTypeName MUST be after attribute creation.
    newPrim.SetTypeName(pxr::TfToken("NauComponent"));

    return newPrim;
}