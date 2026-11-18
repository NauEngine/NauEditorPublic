// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Property value type of the editor document interface (decision D5): a closed set of types with value semantics.
// Backends convert to and from their own representation (VtValue in the USD backend, RuntimeValue later).

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <variant>


// ** NauDocumentVec2, NauDocumentVec3, NauDocumentVec4
//
// Double precision on purpose: values read from the USD backend must survive a round trip unchanged.

struct NauDocumentVec2
{
    double x = 0.0;
    double y = 0.0;

    bool operator==(const NauDocumentVec2&) const = default;
};

struct NauDocumentVec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    bool operator==(const NauDocumentVec3&) const = default;
};

struct NauDocumentVec4
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double w = 0.0;

    bool operator==(const NauDocumentVec4&) const = default;
};


// ** NauDocumentIVec2, NauDocumentIVec3, NauDocumentIVec4

struct NauDocumentIVec2
{
    int32_t x = 0;
    int32_t y = 0;

    bool operator==(const NauDocumentIVec2&) const = default;
};

struct NauDocumentIVec3
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    bool operator==(const NauDocumentIVec3&) const = default;
};

struct NauDocumentIVec4
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;
    int32_t w = 0;

    bool operator==(const NauDocumentIVec4&) const = default;
};


// ** NauDocumentColor4
//
// Linear RGBA, components usually in [0, 1].

struct NauDocumentColor4
{
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
    double a = 1.0;

    bool operator==(const NauDocumentColor4&) const = default;
};


// ** NauDocumentMat4
//
// Row-major 4x4 matrix with the translation in the last row (row vectors), the same layout as GfMatrix4d.

struct NauDocumentMat4
{
    std::array<double, 16> values = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };

    bool operator==(const NauDocumentMat4&) const = default;
};


// ** NauDocumentAssetRef
//
// Reference to another asset, or to a part of it.
// Stage 1 keeps the strings exactly as the backend stores them, so saved files don't change:
// - asset: "uid:<asset uid>" or a path relative to the document file ("../meshes/cube.usda.nausd").
// - subAsset: the part of the asset, a prim path inside the asset file in the USD backend ("/Root/Cube").
//   Empty when the whole asset is referenced, which is always the case for asset-typed properties.
// After D2/D3 (Stage 2-3) both collapse into one asset uid; sub-meshes get their own uids.

struct NauDocumentAssetRef
{
    std::string asset;
    std::string subAsset;

    bool operator==(const NauDocumentAssetRef&) const = default;
};


// ** NauDocumentValue
//
// Integers and floats are widened (int64_t, double). The property descriptor says how a value is stored,
// and the backend narrows on write. Arrays and nested objects aren't covered yet; today's inspector doesn't show them either.

using NauDocumentValue = std::variant<
    bool,
    int64_t,
    double,
    std::string,
    NauDocumentVec2,
    NauDocumentVec3,
    NauDocumentVec4,
    NauDocumentIVec2,
    NauDocumentIVec3,
    NauDocumentIVec4,
    NauDocumentColor4,
    NauDocumentMat4,
    NauDocumentAssetRef
>;


// ** NauDocumentValueType
//
// How a property is stored, which also tells clients which alternative of NauDocumentValue to expect
// and which widget to show. Mapping from today's USD type names (nau_usd_property_factory.hpp):
//
//   Bool     bool              -> bool
//   Int      int               -> int64_t
//   Float    float             -> double
//   Double   double            -> double
//   String   string            -> std::string
//   Double2  double2, texCoord2d                          -> NauDocumentVec2
//   Double3  double3, point3d, normal3d, vector3d, texCoord3d -> NauDocumentVec3
//   Double4  double4           -> NauDocumentVec4
//   Int2/3/4 int2, int3, int4  -> NauDocumentIVec2/3/4
//   Color4   color4d           -> NauDocumentColor4
//   Matrix4  matrix4d, frame4d -> NauDocumentMat4
//   Asset    asset             -> NauDocumentAssetRef (subAsset empty)
//
// Any other stored type is Unsupported: the property is listed, but has no value at the interface,
// the same as today, where such properties get no widget.

enum class NauDocumentValueType
{
    Unsupported,
    Bool,
    Int,
    Float,
    Double,
    String,
    Double2,
    Double3,
    Double4,
    Int2,
    Int3,
    Int4,
    Color4,
    Matrix4,
    Asset
};
