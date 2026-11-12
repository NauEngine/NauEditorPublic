// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/utils/nau_usd_editor_utils.hpp"
#include "nau/math/nau_matrix_math.hpp"
#include "nau_log.hpp"

#include "pxr/usd/sdf/fileFormat.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdGeom/xformCache.h"
#include "pxr/usd/kind/registry.h"
#include "pxr/base/gf/matrix4d.h"
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>

#include <QCoreApplication>
#include <QFile>

#include <format>


// ** NauUsdSceneUtils

void NauUsdSceneUtils::forceUpdatePrimsTransformOpOrder(PXR_NS::UsdStageRefPtr scene)
{
    for (auto primRange : scene->TraverseAll()) {
        auto xformable = PXR_NS::UsdGeomXformable(primRange);
        if (xformable) {
            NauUsdPrimUtils::forceAddTransformOpOrder(xformable);
        }
    }
}

std::string NauUsdSceneUtils::generateUniquePrimPath(PXR_NS::UsdStageRefPtr currentScene, const std::string& parentPath, const std::string& name)
{
    const std::string finalName = !name.empty() ? name : "Prim";
    std::string primName = finalName;

    removeInvalidSymbolsFromPrimName(primName, "::", "__");

    PXR_NS::SdfPath newPath = sdfPathFromParent(parentPath, primName);

    PXR_NS::UsdPrim prim = currentScene->GetPrimAtPath(newPath);
    while (prim.IsValid()) {
        primName = generatePrimName(finalName, primName);
        removeInvalidSymbolsFromPrimName(primName, "::", "__");

        newPath = sdfPathFromParent(parentPath, primName);
        prim = currentScene->GetPrimAtPath(newPath);
    }

    return newPath.GetString();
}

std::string NauUsdSceneUtils::createSceneRelativeAssetPath(const std::string& realScenePath, const std::string& absoluteAssetPath)
{
    const auto rootSceneDirPath = std::filesystem::path(realScenePath).parent_path();
    const auto assetRelativePath = "./" + std::filesystem::relative(absoluteAssetPath, rootSceneDirPath).generic_string();
    return assetRelativePath;
}

std::string NauUsdSceneUtils::generatePrimName(const std::string& typeName, const std::string& primName)
{
    const std::string counterStr = primName.substr(typeName.size(), (primName.size() - typeName.size()));
    const int counter = counterStr.empty() ? 1 : std::stoi(counterStr) + 1;

    return std::format("{}{}", typeName, counter);
}

PXR_NS::SdfPath NauUsdSceneUtils::sdfPathFromParent(const std::string& parentPath, const std::string& primName)
{
    if (parentPath.empty()) {
        return PXR_NS::SdfPath("/" + primName);
    }

    auto parentSdfPath = PXR_NS::SdfPath(parentPath);
    return parentSdfPath.AppendChild(pxr::TfToken(primName));
}

void NauUsdSceneUtils::removeInvalidSymbolsFromPrimName(std::string& primName, const std::string target, const std::string replacement)
{
    size_t pos = 0;

    while ((pos = primName.find(target, pos)) != std::string::npos) {
        primName.replace(pos, target.length(), replacement);
        pos += replacement.length();
    }
}

// ** NauUsdPrimUtils

bool NauUsdPrimUtils::isPrimComponent(pxr::UsdPrim prim)
{
    pxr::TfToken kindToken;
    prim.GetKind(&kindToken);
    return (pxr::KindRegistry::IsComponent(kindToken) || pxr::KindRegistry::IsSubComponent(kindToken));
}

std::string NauUsdPrimUtils::creationTypeName(pxr::UsdPrim prim)
{
    if (pxr::UsdAttribute attr = prim.GetAttribute(pxr::TfToken("componentTypeName"))) {
        std::string componentTypeName;
        if (attr.Get(&componentTypeName) && !componentTypeName.empty()) {
            return componentTypeName;
        }
    }

    return prim.GetTypeName().GetString();
}

PXR_NS::UsdGeomXformOp NauUsdPrimUtils::forceAddTransformOpOrder(PXR_NS::UsdGeomXformable& xformable)
{
    PXR_NS::UsdGeomXformCache cache;
    bool resetsStack = false;

    const PXR_NS::GfMatrix4d primTransform = cache.GetLocalTransformation(xformable.GetPrim(), &resetsStack);

    // Clear current op order
    xformable.ClearXformOpOrder();
    PXR_NS::UsdGeomXformOp transformOp = xformable.GetTransformOp();
    if (!transformOp)
        transformOp = xformable.AddTransformOp(PXR_NS::UsdGeomXformOp::PrecisionDouble);

    if (transformOp)
    {
        transformOp.Set(primTransform);
        return transformOp;
    }
    else
    {
        NED_ERROR("Trying to apply transform to object without transform!");
        return {};
    }
}

pxr::GfMatrix4d NauUsdPrimUtils::relativeTransform(pxr::UsdPrim prim, const pxr::GfMatrix4d& originalTransform)
{
    pxr::UsdGeomXformCache cache;
    return originalTransform * cache.GetLocalToWorldTransform(prim).GetInverse();
}

pxr::GfMatrix4d NauUsdPrimUtils::localPrimTransform(pxr::UsdPrim prim)
{
    PXR_NS::UsdGeomXformCache cache;
    bool resetsStack = false;

    return cache.GetLocalTransformation(prim, &resetsStack);
}

pxr::GfMatrix4d NauUsdPrimUtils::worldPrimTransform(pxr::UsdPrim prim)
{
    PXR_NS::UsdGeomXformCache cache;
    return cache.GetLocalToWorldTransform(prim);
}

void NauUsdPrimUtils::setPrimTransform(pxr::UsdPrim prim, const pxr::GfMatrix4d& matrix)
{
    auto xformable = PXR_NS::UsdGeomXformable(prim);
    if (!xformable) {
        NED_ERROR("Trying to apply transform to object without transform!");
        return;
    }

    auto transformOp = xformable.GetTransformOp();
    if (!transformOp)
    {
        xformable.AddTransformOp(PXR_NS::UsdGeomXformOp::PrecisionDouble);
        transformOp = xformable.GetTransformOp();
    }
    transformOp.Set(matrix);
}

void NauUsdPrimUtils::setPrimWorldTransform(pxr::UsdPrim prim, const pxr::GfMatrix4d& matrix)
{
    auto xformable = PXR_NS::UsdGeomXformable(prim);
    if (!xformable) {
        NED_ERROR("Trying to apply transform to object without transform!");
        return;
    }

    pxr::GfMatrix4d result = matrix;
    auto parentPrim = prim.GetParent();
    if (parentPrim && parentPrim.IsValid()) {
        pxr::UsdGeomXformCache cache;
        result = matrix * cache.GetLocalToWorldTransform(parentPrim).GetInverse();
    }

    auto transformOp = xformable.GetTransformOp();
    if(!transformOp) 
    {
        xformable.AddTransformOp(PXR_NS::UsdGeomXformOp::PrecisionDouble);
        transformOp = xformable.GetTransformOp();
    }

    transformOp.Set(result);
}


// ** NauUsdEditorUtils

void NauUsdEditorUtils::collectPrimsWithChildrensPaths(const std::vector<PXR_NS::UsdPrim>& prims, pxr::SdfPathSet& result)
{
    for (const pxr::UsdPrim& prim : prims) {
        result.insert(prim.GetPath());

        std::vector<PXR_NS::UsdPrim> childPrims;
        for (auto primChild : prim.GetAllChildren()) {
            childPrims.push_back(primChild);
        }
        collectPrimsWithChildrensPaths(childPrims, result);
    }
}

std::vector<PXR_NS::UsdPrim> NauUsdEditorUtils::normalizedPrimsList(const std::vector<PXR_NS::UsdPrim>& prims)
{
    if (prims.empty()) {
        return prims;
    }

    pxr::SdfPathSet normalizedPrimPathsSet;
    for (auto prim : prims) {
        normalizedPrimPathsSet.insert(prim.GetPath());
    }

    for (auto prim : prims) {
        removeChildPrimsFromSet(prim, normalizedPrimPathsSet);
    }
    
    std::vector<PXR_NS::UsdPrim> result;
    for (auto prim : prims) {
        // If found prim in a normalized list, then add this prim to the result list
        if (normalizedPrimPathsSet.contains(prim.GetPath())) {
            result.push_back(prim);
        }
    }

    return result;
}

void NauUsdEditorUtils::removeChildPrimsFromSet(PXR_NS::UsdPrim prim, pxr::SdfPathSet& normalizedPrimsSet)
{
    for (auto child : prim.GetAllChildren()) {
        if (normalizedPrimsSet.contains(child.GetPath())) {
            normalizedPrimsSet.erase(child.GetPath());
        }
        removeChildPrimsFromSet(child, normalizedPrimsSet);
    }
}


// ** NauUsdEditorMathUtils

pxr::GfMatrix4d NauUsdEditorMathUtils::nauMatrixToGfMatrix(const nau::math::mat4& matrix)
{
    pxr::GfMatrix4d resultMatrix;

    for (int i = 0; i < 4; ++i) {
        const nau::math::vec4 matrixRow = matrix.getRow(i);       
        pxr::GfVec4d resultRow(matrixRow.getX(), matrixRow.getY(), matrixRow.getZ(), matrixRow.getW());

        resultMatrix.SetColumn(i,resultRow);
    }
    
    return resultMatrix;
}

nau::math::mat4 NauUsdEditorMathUtils::gfMatrixToNauMatrix(const pxr::GfMatrix4d& matrix)
{
    nau::math::mat4 resultMatrix;

    for (int i = 0; i < 4; ++i) {
        const auto matrixColumn = matrix.GetColumn(i);
        nau::math::vec4 resultRow(matrixColumn.data()[0], matrixColumn.data()[1], matrixColumn.data()[2], matrixColumn.data()[3]);

        resultMatrix.setRow(i, resultRow);
    }

    return resultMatrix;
}

QMatrix3x3 NauUsdEditorMathUtils::gfMatrixToTrsComposition(const pxr::GfMatrix4d& matrix)
{
    auto translation = matrix.ExtractTranslation();
    auto r = matrix.ExtractRotationMatrix().GetOrthonormalized();
    auto rotation = matrix.DecomposeRotation(r.GetColumn(0), r.GetColumn(1), r.GetColumn(2));
    auto scale = pxr::GfVec3d(matrix.GetRow3(0).GetLength(), matrix.GetRow3(1).GetLength(), matrix.GetRow3(2).GetLength());

    QMatrix3x3 trsComposition;

    //// Set transform
    trsComposition.data()[0] = translation.data()[0];
    trsComposition.data()[1] = translation.data()[1];
    trsComposition.data()[2] = translation.data()[2];

    //// Set rotation
    trsComposition.data()[3] = rotation.data()[0];
    trsComposition.data()[4] = rotation.data()[1];
    trsComposition.data()[5] = rotation.data()[2];

    //// Set scale
    trsComposition.data()[6] = scale.data()[0];
    trsComposition.data()[7] = scale.data()[1];
    trsComposition.data()[8] = scale.data()[2];

    return trsComposition;
}

pxr::GfMatrix4d NauUsdEditorMathUtils::trsCompositionToGfMatrix(QMatrix3x3 composition)
{
    // TODO: Convert composition to GfMatrix directly
    QMatrix4x3 qMatrix = NauMathMatrixUtils::TRSCompositionToMatrix(composition);

    constexpr int MAX_ROWS = 3;
    constexpr int MAX_COLUMNS = 4;

    constexpr int MAX_RESULT_ROW = 4;

    const float* matrixData = qMatrix.data();
    pxr::GfMatrix4d result;
    result.SetZero();

    for (int column = 0; column < MAX_COLUMNS; ++column) {
        for (int row = 0; row < MAX_ROWS; ++row) {
            result.data()[column * MAX_RESULT_ROW + row] = matrixData[column * MAX_ROWS + row];
        }
    }
    result.data()[(4 * 4) - 1] = 1.f;

    return result;
}
