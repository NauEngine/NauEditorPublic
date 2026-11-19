// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_usd_scene_snapshot.hpp"

#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/primSpec.h"
#include "pxr/usd/sdf/reference.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/vt/array.h"

#include <cmath>
#include <format>


namespace
{
    // Round away floating point noise from transform arithmetic, and fold -0 into 0
    std::string numberString(double value)
    {
        const double rounded = std::round(value * 1e6) / 1e6 + 0.0;
        return std::format("{}", rounded);
    }

    std::string valueString(const PXR_NS::VtValue& value)
    {
        if (value.IsHolding<PXR_NS::GfMatrix4d>()) {
            return NauUsdSnapshotUtils::matrixString(value.UncheckedGet<PXR_NS::GfMatrix4d>());
        }
        if (value.IsHolding<float>()) {
            return numberString(value.UncheckedGet<float>());
        }
        if (value.IsHolding<double>()) {
            return numberString(value.UncheckedGet<double>());
        }
        if (value.IsHolding<std::string>()) {
            return value.UncheckedGet<std::string>();
        }
        if (value.IsHolding<PXR_NS::VtTokenArray>()) {
            std::vector<std::string> tokens;
            for (const PXR_NS::TfToken& token : value.UncheckedGet<PXR_NS::VtTokenArray>()) {
                tokens.push_back(token.GetString());
            }
            return "[" + PXR_NS::TfStringJoin(tokens, ", ") + "]";
        }
        return PXR_NS::TfStringify(value);
    }

    std::string referenceString(const PXR_NS::SdfReference& reference)
    {
        return reference.GetAssetPath() + "<" + reference.GetPrimPath().GetString() + ">";
    }
}


// ** NauUsdSnapshotUtils

NauSceneSnapshot NauUsdSnapshotUtils::snapshotStage(const PXR_NS::UsdStageRefPtr& stage)
{
    NauSceneSnapshot scene;

    for (const PXR_NS::UsdPrim& prim : PXR_NS::UsdPrimRange::AllPrims(stage->GetPseudoRoot())) {
        if (prim.IsPseudoRoot()) {
            continue;
        }

        NauNodeSnapshot node;
        node.typeName = prim.GetTypeName().GetString();
        node.displayName = prim.GetDisplayName();

        PXR_NS::TfToken kind;
        prim.GetKind(&kind);
        node.kind = kind.GetString();

        for (const PXR_NS::UsdAttribute& attribute : prim.GetAuthoredAttributes()) {
            PXR_NS::VtValue value;
            attribute.Get(&value);
            node.attributes[attribute.GetName().GetString()] = { valueString(value), attribute.IsHidden() };
        }

        if (PXR_NS::SdfPrimSpecHandle spec = stage->GetRootLayer()->GetPrimAtPath(prim.GetPath())) {
            const PXR_NS::SdfReferencesProxy referenceList = spec->GetReferenceList();
            for (const auto& items : { referenceList.GetExplicitItems(), referenceList.GetPrependedItems(), referenceList.GetAppendedItems() }) {
                for (const PXR_NS::SdfReference& reference : items) {
                    node.references.push_back(referenceString(reference));
                }
            }
        }

        scene[prim.GetPath().GetString()] = node;
    }

    return scene;
}

std::string NauUsdSnapshotUtils::matrixString(const PXR_NS::GfMatrix4d& matrix)
{
    std::vector<std::string> rows;
    for (int row = 0; row < 4; ++row) {
        std::vector<std::string> cells;
        for (int column = 0; column < 4; ++column) {
            cells.push_back(numberString(matrix[row][column]));
        }
        rows.push_back("[" + PXR_NS::TfStringJoin(cells, ", ") + "]");
    }
    return "[" + PXR_NS::TfStringJoin(rows, ", ") + "]";
}

std::string NauUsdSnapshotUtils::translationString(double x, double y, double z)
{
    PXR_NS::GfMatrix4d matrix(1.0);
    matrix.SetTranslateOnly(PXR_NS::GfVec3d(x, y, z));
    return matrixString(matrix);
}
