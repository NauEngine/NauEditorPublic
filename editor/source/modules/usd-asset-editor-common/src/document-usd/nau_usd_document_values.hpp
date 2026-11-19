// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Conversion between USD values and the editor document value type (decision D5)

#pragma once

#include "nau/document/nau_document_value.hpp"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/sdf/valueTypeName.h"

#include <optional>


// ** NauUsdDocumentValues

class NauUsdDocumentValues
{
public:
    NauUsdDocumentValues() = delete;

    // The document type of a USD value type, by its type name as the inspector's widget table uses it today
    // ("point3d" -> Double3). Arrays and types without a widget today are Unsupported.
    static NauDocumentValueType valueType(const PXR_NS::SdfValueTypeName& typeName);

    // nullopt if the value doesn't hold what the type stores
    static std::optional<NauDocumentValue> toDocumentValue(const PXR_NS::VtValue& value, NauDocumentValueType type);

    // Narrows to the stored type (int64 -> int, double -> float). nullopt if the value holds the wrong alternative.
    static std::optional<PXR_NS::VtValue> toUsdValue(const NauDocumentValue& value, NauDocumentValueType type);

    static NauDocumentMat4 toDocumentMatrix(const PXR_NS::GfMatrix4d& matrix);
    static PXR_NS::GfMatrix4d toUsdMatrix(const NauDocumentMat4& matrix);
};
