// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Asset manager interface

#pragma once

#include "nau/nau_editor_config.hpp"
#include "nau/assets/nau_asset_manager_client.hpp"

#include "nau/assets/nau_file_types.hpp"
#include "browser/nau_project_browser_item_type.hpp"
#include "project/nau_project.hpp"

#include "nau/rtti/rtti_object.h"

#include <string>
#include <filesystem>


// ** NauAssetFileProcessorInterface
// Interface for asset file processor. Need to be implement for specific asset meta files

class NAU_EDITOR_API NauAssetFileProcessorInterface : virtual public nau::IRefCounted
{
    NAU_INTERFACE(NauAssetFileProcessorInterface, nau::IRefCounted)

public:
    virtual void setAssetFileUid(const std::filesystem::path& assetSource) = 0;
    virtual int importAsset(const std::filesystem::path& project, const std::filesystem::path& assetSource) = 0;

    // Reads asset meta file and gets source path info from it
    virtual std::string sourcePathFromAssetFile(const std::string& assetPath) = 0;

    // Returns absolute path of the file that specified meta file refers to as its source.
    // Returns the declared path even if the source is missing; empty means no source (e.g. materials).
    virtual std::string assetSourceFile(const std::string& metaFilePath) = 0;

    virtual std::string_view assetFileFilter() const = 0;
    virtual std::string_view assetFileExtension() const = 0;

    virtual bool isAssetMetaFile(const std::string& assetFilePath) = 0;
    virtual bool isAssetValid(const std::string& assetFilePath) = 0;

    virtual std::vector<std::string> getAssetPrimsPath(const std::string& assetPath) = 0;

    virtual std::shared_ptr<NauProjectBrowserItemTypeResolverInterface> createTypeResolver() = 0;
};


// ** NauAssetManagerInterface

using NauAssetTypesList = std::vector<NauEditorFileType>;

class NAU_EDITOR_API NauAssetManagerInterface
{
public:
    virtual ~NauAssetManagerInterface() = default;

    virtual void initialize(const NauProject& project) = 0;

    virtual void importAsset(const std::string& sourcePath) = 0;

    virtual std::shared_ptr<NauProjectBrowserItemTypeResolverInterface> typeResolver() = 0;

    virtual std::string sourcePathFromAsset(const std::string& assetPath) = 0;

    virtual std::string_view assetFileFilter() const = 0;
    virtual std::string_view assetFileExtension() const = 0;

    virtual void addClient(const NauAssetTypesList& types, NauAssetManagerClientInterface* client) = 0;
};

using NauAssetManagerInterfacePtr = std::shared_ptr<NauAssetManagerInterface>;