// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_camera_preview_layout.hpp"

#include <gtest/gtest.h>

namespace nau::editor
{
    TEST(CameraPreviewLayout, movesWithViewportEvenWhenPreviewSizeIsUnchanged)
    {
        EXPECT_EQ(cameraPreviewRect(1920, 1080), QRect(1588, 888, 320, 180));
        EXPECT_EQ(cameraPreviewRect(1921, 1082), QRect(1589, 890, 320, 180));
    }

    TEST(CameraPreviewLayout, scalesToFitEitherViewportDimension)
    {
        EXPECT_EQ(cameraPreviewRect(480, 600), QRect(308, 498, 160, 90));
        EXPECT_EQ(cameraPreviewRect(800, 270), QRect(628, 168, 160, 90));
    }

    TEST(CameraPreviewLayout, hidesAndRestoresAtMinimumViewportSize)
    {
        EXPECT_TRUE(cameraPreviewRect(47, 27).isEmpty());
        EXPECT_TRUE(cameraPreviewRect(48, 26).isEmpty());
        EXPECT_EQ(cameraPreviewRect(48, 27), QRect(31, 17, 16, 9));
    }
}
