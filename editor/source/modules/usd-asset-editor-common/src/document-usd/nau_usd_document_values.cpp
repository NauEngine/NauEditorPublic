// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_usd_document_values.hpp"

#include "pxr/base/gf/vec2d.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3i.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/usd/sdf/assetPath.h"

#include <algorithm>
#include <string>
#include <unordered_map>


// ** NauUsdDocumentValues

NauDocumentValueType NauUsdDocumentValues::valueType(const PXR_NS::SdfValueTypeName& typeName)
{
    // Same keys as the widget table in nau_usd_property_factory.hpp
    static const std::unordered_map<std::string, NauDocumentValueType> types = {
        { "bool",       NauDocumentValueType::Bool },
        { "int",        NauDocumentValueType::Int },
        { "float",      NauDocumentValueType::Float },
        { "double",     NauDocumentValueType::Double },
        { "string",     NauDocumentValueType::String },
        { "double2",    NauDocumentValueType::Double2 },
        { "texCoord2d", NauDocumentValueType::Double2 },
        { "double3",    NauDocumentValueType::Double3 },
        { "point3d",    NauDocumentValueType::Double3 },
        { "normal3d",   NauDocumentValueType::Double3 },
        { "vector3d",   NauDocumentValueType::Double3 },
        { "texCoord3d", NauDocumentValueType::Double3 },
        { "double4",    NauDocumentValueType::Double4 },
        { "int2",       NauDocumentValueType::Int2 },
        { "int3",       NauDocumentValueType::Int3 },
        { "int4",       NauDocumentValueType::Int4 },
        { "color4d",    NauDocumentValueType::Color4 },
        { "matrix4d",   NauDocumentValueType::Matrix4 },
        { "frame4d",    NauDocumentValueType::Matrix4 },
        { "asset",      NauDocumentValueType::Asset },
    };

    const auto it = types.find(typeName.GetAsToken().GetString());
    return it != types.end() ? it->second : NauDocumentValueType::Unsupported;
}

std::optional<NauDocumentValue> NauUsdDocumentValues::toDocumentValue(const PXR_NS::VtValue& value, NauDocumentValueType type)
{
    using namespace PXR_NS;

    switch (type) {
    case NauDocumentValueType::Bool:
        if (value.IsHolding<bool>()) {
            return value.UncheckedGet<bool>();
        }
        break;
    case NauDocumentValueType::Int:
        if (value.IsHolding<int>()) {
            return static_cast<int64_t>(value.UncheckedGet<int>());
        }
        break;
    case NauDocumentValueType::Float:
        if (value.IsHolding<float>()) {
            return static_cast<double>(value.UncheckedGet<float>());
        }
        break;
    case NauDocumentValueType::Double:
        if (value.IsHolding<double>()) {
            return value.UncheckedGet<double>();
        }
        break;
    case NauDocumentValueType::String:
        if (value.IsHolding<std::string>()) {
            return value.UncheckedGet<std::string>();
        }
        break;
    case NauDocumentValueType::Double2:
        if (value.IsHolding<GfVec2d>()) {
            const GfVec2d& v = value.UncheckedGet<GfVec2d>();
            return NauDocumentVec2{ v[0], v[1] };
        }
        break;
    case NauDocumentValueType::Double3:
        if (value.IsHolding<GfVec3d>()) {
            const GfVec3d& v = value.UncheckedGet<GfVec3d>();
            return NauDocumentVec3{ v[0], v[1], v[2] };
        }
        break;
    case NauDocumentValueType::Double4:
        if (value.IsHolding<GfVec4d>()) {
            const GfVec4d& v = value.UncheckedGet<GfVec4d>();
            return NauDocumentVec4{ v[0], v[1], v[2], v[3] };
        }
        break;
    case NauDocumentValueType::Int2:
        if (value.IsHolding<GfVec2i>()) {
            const GfVec2i& v = value.UncheckedGet<GfVec2i>();
            return NauDocumentIVec2{ v[0], v[1] };
        }
        break;
    case NauDocumentValueType::Int3:
        if (value.IsHolding<GfVec3i>()) {
            const GfVec3i& v = value.UncheckedGet<GfVec3i>();
            return NauDocumentIVec3{ v[0], v[1], v[2] };
        }
        break;
    case NauDocumentValueType::Int4:
        if (value.IsHolding<GfVec4i>()) {
            const GfVec4i& v = value.UncheckedGet<GfVec4i>();
            return NauDocumentIVec4{ v[0], v[1], v[2], v[3] };
        }
        break;
    case NauDocumentValueType::Color4:
        if (value.IsHolding<GfVec4d>()) {
            const GfVec4d& v = value.UncheckedGet<GfVec4d>();
            return NauDocumentColor4{ v[0], v[1], v[2], v[3] };
        }
        break;
    case NauDocumentValueType::Matrix4:
        if (value.IsHolding<GfMatrix4d>()) {
            return toDocumentMatrix(value.UncheckedGet<GfMatrix4d>());
        }
        break;
    case NauDocumentValueType::Asset:
        if (value.IsHolding<SdfAssetPath>()) {
            return NauDocumentAssetRef{ value.UncheckedGet<SdfAssetPath>().GetAssetPath(), {} };
        }
        break;
    case NauDocumentValueType::Unsupported:
        break;
    }

    return std::nullopt;
}

std::optional<PXR_NS::VtValue> NauUsdDocumentValues::toUsdValue(const NauDocumentValue& value, NauDocumentValueType type)
{
    using namespace PXR_NS;

    switch (type) {
    case NauDocumentValueType::Bool:
        if (const auto* v = std::get_if<bool>(&value)) {
            return VtValue(*v);
        }
        break;
    case NauDocumentValueType::Int:
        if (const auto* v = std::get_if<int64_t>(&value)) {
            return VtValue(static_cast<int>(*v));
        }
        break;
    case NauDocumentValueType::Float:
        if (const auto* v = std::get_if<double>(&value)) {
            return VtValue(static_cast<float>(*v));
        }
        break;
    case NauDocumentValueType::Double:
        if (const auto* v = std::get_if<double>(&value)) {
            return VtValue(*v);
        }
        break;
    case NauDocumentValueType::String:
        if (const auto* v = std::get_if<std::string>(&value)) {
            return VtValue(*v);
        }
        break;
    case NauDocumentValueType::Double2:
        if (const auto* v = std::get_if<NauDocumentVec2>(&value)) {
            return VtValue(GfVec2d(v->x, v->y));
        }
        break;
    case NauDocumentValueType::Double3:
        if (const auto* v = std::get_if<NauDocumentVec3>(&value)) {
            return VtValue(GfVec3d(v->x, v->y, v->z));
        }
        break;
    case NauDocumentValueType::Double4:
        if (const auto* v = std::get_if<NauDocumentVec4>(&value)) {
            return VtValue(GfVec4d(v->x, v->y, v->z, v->w));
        }
        break;
    case NauDocumentValueType::Int2:
        if (const auto* v = std::get_if<NauDocumentIVec2>(&value)) {
            return VtValue(GfVec2i(v->x, v->y));
        }
        break;
    case NauDocumentValueType::Int3:
        if (const auto* v = std::get_if<NauDocumentIVec3>(&value)) {
            return VtValue(GfVec3i(v->x, v->y, v->z));
        }
        break;
    case NauDocumentValueType::Int4:
        if (const auto* v = std::get_if<NauDocumentIVec4>(&value)) {
            return VtValue(GfVec4i(v->x, v->y, v->z, v->w));
        }
        break;
    case NauDocumentValueType::Color4:
        if (const auto* v = std::get_if<NauDocumentColor4>(&value)) {
            return VtValue(GfVec4d(v->r, v->g, v->b, v->a));
        }
        break;
    case NauDocumentValueType::Matrix4:
        if (const auto* v = std::get_if<NauDocumentMat4>(&value)) {
            return VtValue(toUsdMatrix(*v));
        }
        break;
    case NauDocumentValueType::Asset:
        if (const auto* v = std::get_if<NauDocumentAssetRef>(&value)) {
            return VtValue(SdfAssetPath(v->asset));
        }
        break;
    case NauDocumentValueType::Unsupported:
        break;
    }

    return std::nullopt;
}

NauDocumentMat4 NauUsdDocumentValues::toDocumentMatrix(const PXR_NS::GfMatrix4d& matrix)
{
    NauDocumentMat4 result;
    std::copy(matrix.data(), matrix.data() + 16, result.values.begin());
    return result;
}

PXR_NS::GfMatrix4d NauUsdDocumentValues::toUsdMatrix(const NauDocumentMat4& matrix)
{
    PXR_NS::GfMatrix4d result;
    std::copy(matrix.values.begin(), matrix.values.end(), result.data());
    return result;
}
