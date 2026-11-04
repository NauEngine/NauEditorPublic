// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include <QRect>

#include <algorithm>

namespace nau::editor
{
    inline constexpr int CameraPreviewRenderWidth = 320;
    inline constexpr int CameraPreviewRenderHeight = 180;

    inline QRect cameraPreviewRect(int viewportWidth, int viewportHeight)
    {
        // Fit whole 16:9 units into one third of the render window on either axis.
        const int units = std::min({CameraPreviewRenderWidth / 16, viewportWidth / (3 * 16), viewportHeight / (3 * 9)});
        if (units <= 0)
        {
            return {};
        }
        const int width = units * 16;
        const int height = units * 9;
        const int margin = std::min({12, viewportWidth / 16, viewportHeight / 16});
        return {viewportWidth - margin - width, viewportHeight - margin - height, width, height};
    }
}  // namespace nau::editor
