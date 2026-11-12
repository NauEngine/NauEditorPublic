// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "browser/nau_project_browser_item_type_resolver.hpp"

#include <gtest/gtest.h>


TEST(NauProjectBrowserItemTypeResolverTests, sceneFileIsRecognized)
{
    // Arrange
    const NauProjectBrowserItemTypeResolver resolver;

    // Act
    const auto description = resolver.resolve(QStringLiteral("C:/project/content/scenes/main.nausd_scene"));

    // Assert
    EXPECT_EQ(description.type, NauEditorFileType::Scene);
}
