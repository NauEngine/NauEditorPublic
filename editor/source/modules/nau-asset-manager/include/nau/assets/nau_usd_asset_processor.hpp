// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Usd asset manager implementation

#pragma once

#include "nau/nau_usd_asset_processor_config.hpp"

#include "nau/assets/nau_asset_manager.hpp"
#include "nau/rtti/rtti_impl.h"

#include <vector>

// ** NauUsdAssetProcessor

class NAU_USD_ASSET_PROCESSOR_API NauUsdAssetProcessor : public NauAssetFileProcessorInterface
{
    NAU_CLASS_(NauUsdAssetProcessor, NauAssetFileProcessorInterface)

public:
    NauUsdAssetProcessor() = default;
    ~NauUsdAssetProcessor() = default;
    void setAssetFileUid(const std::filesystem::path& assetSource) override;
    int importAsset(const std::filesystem::path& project, const std::filesystem::path& assetSource) override;
    std::string sourcePathFromAssetFile(const std::string& assetPath) override;
    std::string assetSourceFile(const std::string& metaFilePath) override;

    std::string_view assetFileFilter() const override;
    std::string_view assetFileExtension() const override;

    bool isAssetMetaFile(const std::string& assetFilePath) override;
    bool isAssetValid(const std::string& assetFilePath) override;

    std::vector<std::string> getAssetPrimsPath(const std::string& assetPath) override;

    std::shared_ptr<NauProjectBrowserItemTypeResolverInterface> createTypeResolver() override;
};
