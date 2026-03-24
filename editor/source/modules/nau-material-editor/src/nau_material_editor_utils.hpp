// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Nau material editor utils

#pragma once

#include "pxr/usd/usd/stage.h"

class NauMaterialEditorUtils
{
public:
    NauMaterialEditorUtils() = delete;

    static pxr::UsdStageRefPtr createMaterialPreviewScene();
};




