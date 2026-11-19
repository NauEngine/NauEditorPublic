// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_log.hpp"

#include "pxr/base/plug/registry.h"
#include "pxr/base/plug/plugin.h"

#include <QCoreApplication>
#include <QFile>

#include <gtest/gtest.h>


// ** NauUsdTestEnvironment
// Sets up what the editor sets up before editing a stage: editor logger and the editor's USD plugins (Nau schemas, .nausd format).
// Done here and not in main(), so listing tests (gtest_discover_tests runs it after build) does not need it.

class NauUsdTestEnvironment : public testing::Environment
{
public:
    void SetUp() override
    {
        NauLog::init();

        // Same mechanism as InitializeUsdPlugins() in nau_editor_application.cpp
        const std::string pluginInfoFilePath = QCoreApplication::applicationDirPath().toUtf8().constData() + std::string("/plugins/plugInfo.json");
        ASSERT_TRUE(QFile::exists(pluginInfoFilePath.c_str())) << "USD plugin configuration not found: " << pluginInfoFilePath;

        auto usdPlugins = pxr::PlugRegistry::GetInstance().RegisterPlugins(pluginInfoFilePath);
        ASSERT_FALSE(usdPlugins.empty()) << "No USD plugins registered from " << pluginInfoFilePath;
        for (auto usdPlugin : usdPlugins) {
            EXPECT_TRUE(usdPlugin->Load()) << "Failed to load USD plugin " << usdPlugin->GetName();
        }
    }

    void TearDown() override
    {
        NauLog::close();
    }
};

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);

    // Needed for NauLog and settings; names keep them apart from the real editor's
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName("N-GINN");
    QCoreApplication::setApplicationName("NauEditorUsdTests");

    testing::AddGlobalTestEnvironment(new NauUsdTestEnvironment);
    return RUN_ALL_TESTS();
}
