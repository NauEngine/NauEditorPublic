// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Printers of scene snapshots for GoogleTest assertions.

#pragma once

#include "nau_usd_scene_snapshot.hpp"

#include <ostream>


inline void PrintTo(const NauAttributeSnapshot& attribute, std::ostream* os) {
    *os << attribute.value << (attribute.hidden ? " (hidden)" : "");
}

inline void PrintTo(const NauNodeSnapshot& node, std::ostream* os) {
    *os << "{ type: '" << node.typeName << "', displayName: '" << node.displayName << "', kind: '" << node.kind << "'";
    for (const auto& [name, attribute] : node.attributes) {
        *os << ", " << name << " = ";
        PrintTo(attribute, os);
    }
    for (const std::string& reference : node.references) {
        *os << ", ref " << reference;
    }
    *os << " }";
}

inline void PrintTo(const NauSceneSnapshot& scene, std::ostream* os) {
    *os << "\n";
    for (const auto& [path, node] : scene) {
        *os << "  " << path << " ";
        PrintTo(node, os);
        *os << "\n";
    }
}
