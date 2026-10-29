// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Asset manager implementation

#pragma once

#include "nau/assets/nau_asset_manager.hpp"
#include "nau/assets/nau_asset_watcher.hpp"
#include "nau_asset_import_runner.hpp"

#include "project/nau_project.hpp"

#include "nau/assets/asset_db.h"

#include <map>


// ** NauAssetManager

class NauAssetManager : public NauAssetManagerInterface
{
    using WatcherClientsList = std::vector<NauAssetManagerClientInterface*>;
    using WatcherClientsTypedMap = std::map<NauEditorFileType, WatcherClientsList>;

public:
    NauAssetManager(NauAssetFileProcessorInterface* assetProcessor);
    ~NauAssetManager() = default;

    void initialize(const NauProject& project) override;

    void importAsset(const std::string& sourcePath) override;

    std::shared_ptr<NauProjectBrowserItemTypeResolverInterface> typeResolver() override;

    std::string sourcePathFromAsset(const std::string& assetPath) override;

    std::string_view assetFileFilter() const override;
    std::string_view assetFileExtension() const override;

    void addClient(const NauAssetTypesList& types, NauAssetManagerClientInterface* client) override;

private:
    bool isAssetMetaFile(const std::string& assetFilePath);
    bool isAssetValid(const std::string& assetFilePath);

    void initializeWatcher(const std::string& contentDir);

    void onAssetAdded(const std::string& assetPath);
    void onAssetRemoved(const std::string& assetPath);
    void onAssetChanged(const std::string& assetPath);

    void assetAdded(const std::string& assetPath);
    void assetRemoved(const std::string& assetPath);

    void sourceAdded(const std::string& sourcePath);
    void sourceRemoved(const std::string& sourcePath);

    bool ensureUniqueUid(const std::string& assetPath);

private:
    nau::IAssetDB& m_assetDb;
    NauAssetFileProcessorInterface* m_assetProcessor;

    std::shared_ptr<NauProjectBrowserItemTypeResolverInterface> m_typeResolver;

    WatcherClientsTypedMap m_watcherClients;
    std::unique_ptr<NauAssetWatcher> m_assetWatcher;

    std::filesystem::path m_projectPath;
};