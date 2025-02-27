// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// All constants necessary for the editor will be stored here 

#pragma once


// ** Basic Constants

constexpr float NAU_POSITION_MAX_LIMITATION = (1e+4f - 1); 
constexpr float NAU_POSITION_MIN_LIMITATION = (-1e+4f + 1);  

constexpr float NAU_SCALE_MAX_LIMITATION = 1e+4f - 1; 
constexpr float NAU_SCALE_MIN_LIMITATION = 1e-5f;  // The current limit is set based on the container size

constexpr const char* NAU_INPUT_ACTION_EXTENSION_NAME = "ninputaction";
constexpr const char* NAU_INPUT_ACTION_FILE_EXTENSION = ".ninputaction";
constexpr const char* NAU_INPUT_ACTION_FILE_FILTER = "*.ninputaction";

constexpr const char* NAU_ANIMATION_EXTENSION_NAME = "usda";
constexpr const char* NAU_ANIMATION_FILE_EXTENSION = ".usda";
constexpr const char* NAU_ANIMATION_FILE_FILTER = "*.usda";

constexpr const char* NAU_VFX_EXTENSION_NAME = "nvfx";
constexpr const char* NAU_VFX_FILE_EXTENSION = ".nvfx";
constexpr const char* NAU_VFX_FILE_FILTER = "*.nvfx";


// ** UI Constants

constexpr int NAU_WIDGET_DECIMAL_PRECISION = 2;  // At the moment, for real numbers, we use the standard float (32 bits).
                                                 // Thus we can display only 6-7 significant numbers (including decimal numbers) without loss of precision.
                                                 // The world constraint takes 4 significant numbers,
                                                 // so we can allocate 2 more for decimal places to make sure that there will be no problems with accuracy.


// ** Viewport Camera Constants

constexpr float CAMERA_MAX_SPEED = 20.0f;
constexpr float CAMERA_MIN_SPEED = 0.05f;

constexpr float CAMERA_MAX_SENSITIVITY = 0.1f;
constexpr float CAMERA_MIN_SENSITIVITY = 0.0f;
constexpr float CAMERA_SENSITIVITY_MULTIPLIER = 0.1f;

constexpr int CAMERA_MAX_FOV = 120;
constexpr int CAMERA_MIN_FOV = 5;
constexpr int CAMERA_DEFAULT_FOV = 60;

constexpr float ACCELERATION_START_THRESHOLD = 1.5f;
constexpr float ACCELERATION_DURATION = 1.5f;
constexpr float EASING_DURATION = 0.15f;

constexpr float SHIFT_BOOST_POWER = 2.0f;


// ** Nau ECS component names
//    these names are configured in the files:
//        projects/base/templates/baseproject.entities.blk
//        projects/baseProject/prog/gameBase/content/baseProject/gamedata/templates/baseproject.entities.blk

namespace NauEcsComponentNames
{
    constexpr const char* TRANSFORM = "transform";
    constexpr const char* MATERIAL = "material";
    constexpr const char* RI_EXTRA = "ri_extra";
    constexpr const char* RI_EXTRA_NAME = "ri_extra__name";
}