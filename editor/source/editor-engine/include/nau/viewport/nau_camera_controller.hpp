// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Camera controller class

#pragma once

#include <nau/math/math.h>
#include "nau/viewport/nau_viewport_input.hpp"


// ** NauCameraControllerInterface

class NauCameraControllerInterface
{
public:
    virtual ~NauCameraControllerInterface() = default;

    virtual void updateCameraMovement(float deltaTime, const NauViewportInput& input) = 0;
    virtual void changeCameraSpeed(float deltaSpeed) = 0;
    virtual void changeCameraSensitivity(float sensitivity) = 0;
    virtual void focusOn(const nau::math::mat4& matrix, int distanceMeters = 5) = 0;

    virtual bool isCameraActive(const NauViewportInput& input) const = 0;
};

using NauCameraControllerInterfacePtr = std::shared_ptr<NauCameraControllerInterface>;