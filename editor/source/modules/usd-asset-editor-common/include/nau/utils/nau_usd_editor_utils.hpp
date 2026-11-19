// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Nau editor utils for usd scene & objects

#pragma once

#include "nau/nau_usd_asset_editor_common_config.hpp"

#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/xformable.h"

#include "nau/math/math.h"

#include <vector>

#include <QMatrix3x3>
#include <QGenericMatrix>


// ** NauUsdSceneUtils
// Editor functions for UsdStage

class NAU_USD_ASSET_EDITOR_COMMON_API NauUsdSceneUtils
{
public:
    NauUsdSceneUtils() = delete;

    // Deletes stage prims xFormOpOrder and creates a new one from one operation - transformOp
    // It is necessary because we do not support opOrder, but only work with a transform in the form of a matrix
    static void forceUpdatePrimsTransformOpOrder(PXR_NS::UsdStageRefPtr scene);

    static std::string generateUniquePrimPath(PXR_NS::UsdStageRefPtr currentScene, const std::string& parentPath, const std::string& name);
    static std::string createSceneRelativeAssetPath(const std::string& realScenePath, const std::string& absoluteAssetPath);

private:
    static std::string generatePrimName(const std::string& typeName, const std::string& primName);
    static PXR_NS::SdfPath sdfPathFromParent(const std::string& parentPath, const std::string& primName);
    static void removeInvalidSymbolsFromPrimName(std::string& primName, const std::string target, const std::string replacement);
};


// ** NauUsdPrimUtils
// Editor functions for UsdPrim

class NAU_USD_ASSET_EDITOR_COMMON_API NauUsdPrimUtils
{
public:
    NauUsdPrimUtils() = delete;

    static bool isPrimComponent(pxr::UsdPrim prim);

    // Type name to pass to NauUsdPrimFactory to create a prim like this one.
    // All components are typed NauComponent; the factory knows them by their engine class name (componentTypeName).
    static std::string creationTypeName(pxr::UsdPrim prim);

    // Deletes prim xFormOpOrder and creates a new one from one operation - transformOp
    static PXR_NS::UsdGeomXformOp forceAddTransformOpOrder(PXR_NS::UsdGeomXformable& xformable);

    static pxr::GfMatrix4d localPrimTransform(pxr::UsdPrim prim);
    static pxr::GfMatrix4d worldPrimTransform(pxr::UsdPrim prim);

    static pxr::GfMatrix4d relativeTransform(pxr::UsdPrim prim, const pxr::GfMatrix4d& originalTransform);

    static void setPrimTransform(pxr::UsdPrim prim, const pxr::GfMatrix4d& matrix);
    static void setPrimWorldTransform(pxr::UsdPrim prim, const pxr::GfMatrix4d& matrix);
};


// ** NauUsdEditorUtils
// Editor functions for custom containers with usd data types

class NAU_USD_ASSET_EDITOR_COMMON_API NauUsdEditorUtils
{
public:
    NauUsdEditorUtils() = delete;

    // Receives a list of tricks as input and creates from it a set with their and children’s names
    static void collectPrimsWithChildrensPaths(const std::vector<PXR_NS::UsdPrim>& prims, pxr::SdfPathSet& result);

    // Removes from a list prims that are children of other prims from input list
    static std::vector<PXR_NS::UsdPrim> normalizedPrimsList(const std::vector<PXR_NS::UsdPrim>& prims);

private:
    // Removes prim childs paths from prims paths set
    static void removeChildPrimsFromSet(PXR_NS::UsdPrim prim, pxr::SdfPathSet& normalizedPrimsSet);
};


// ** NauUsdEditorMathUtils

class NAU_USD_ASSET_EDITOR_COMMON_API NauUsdEditorMathUtils
{
public:
    NauUsdEditorMathUtils() = delete;

    static pxr::GfMatrix4d nauMatrixToGfMatrix(const nau::math::mat4& matrix);
    static nau::math::mat4 gfMatrixToNauMatrix(const pxr::GfMatrix4d& matrix);

    static QMatrix3x3 gfMatrixToTrsComposition(const pxr::GfMatrix4d& matrix);
    static pxr::GfMatrix4d trsCompositionToGfMatrix(QMatrix3x3 composition); 
};
