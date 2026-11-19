// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Backend-neutral description of an edited scene, used by the undo/redo characterization tests.
// Expected states are written as plain data, so the same test bodies can later run against the editor document interface.

#pragma once

#include "pxr/usd/usd/stage.h"
#include "pxr/base/gf/matrix4d.h"

#include <map>
#include <string>
#include <vector>


// ** NauAttributeSnapshot

struct NauAttributeSnapshot
{
    std::string value;
    bool hidden = false;

    bool operator==(const NauAttributeSnapshot&) const = default;
};


// ** NauNodeSnapshot

struct NauNodeSnapshot
{
    std::string typeName;
    std::string displayName;
    std::string kind;

    // Authored attributes only, by name
    std::map<std::string, NauAttributeSnapshot> attributes;

    // References authored on the root layer, as "assetPath<primPath>", in list order
    std::vector<std::string> references;

    bool operator==(const NauNodeSnapshot&) const = default;
};


// ** NauSceneSnapshot
// Nodes by path, pseudo root excluded

using NauSceneSnapshot = std::map<std::string, NauNodeSnapshot>;


// ** NauUsdSnapshotUtils

class NauUsdSnapshotUtils
{
public:
    NauUsdSnapshotUtils() = delete;

    static NauSceneSnapshot snapshotStage(const PXR_NS::UsdStageRefPtr& stage);

    // Value formatting shared by the stage snapshot and the expected values in tests
    static std::string matrixString(const PXR_NS::GfMatrix4d& matrix);
    static std::string translationString(double x, double y, double z);
};
