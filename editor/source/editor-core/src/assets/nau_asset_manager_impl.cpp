// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_asset_manager_impl.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"

#include <unordered_set>
#include "nau/service/service_provider.h"
#include "nau/shared/file_system.h"
#include "nau/usd_meta_tools/usd_meta_manager.h"
#include "nau/assets/asset_descriptor.h"
#include "nau/assets/asset_manager.h"
#include "nau/assets/asset_path.h"


// ** NauAssetManager

NauAssetManager::NauAssetManager(NauAssetFileProcessorInterface* assetProcessor)
    : m_assetProcessor(assetProcessor)
    , m_typeResolver(assetProcessor->createTypeResolver())
    , m_assetDb(nau::getServiceProvider().get<nau::IAssetDB>())
{
}

void NauAssetManager::initialize(const NauProject& project)
{
    m_assetDb.reloadAssetDB("assets_database/database.db");

    const std::string contentDir = project.defaultContentFolder().toUtf8().constData();
    initializeWatcher(contentDir);
    //

    // Generate asset files for sources
    m_projectPath = project.path().root().absolutePath().toStdWString();
    m_assetProcessor->importAsset(m_projectPath, "");
}

void NauAssetManager::importAsset(const std::string& sourcePath)
{
    m_assetProcessor->importAsset(m_projectPath, std::filesystem::u8path(sourcePath));
}

std::shared_ptr<NauProjectBrowserItemTypeResolverInterface> NauAssetManager::typeResolver()
{
    return m_typeResolver;
}

std::string NauAssetManager::sourcePathFromAsset(const std::string& assetPath)
{
    auto& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();

    auto assetInfo = nau::UsdMetaManager::instance().getInfo(assetPath);
    if (!assetInfo.empty() && assetInfo[0].type == "material") // todo add method IsMetaAsAsset()->bool
        return assetPath;

    const std::string nausdRelativePath = nau::pathToUtf8(nau::FileSystemExtensions::getRelativeAssetPath(std::filesystem::u8path(assetPath), false));
    auto sourceRelativePath = assetDb.getSourcePathFromNausdPath(nausdRelativePath.c_str()); /// +"." + assetInfo[0].type.c_str();
    std::string sourceAbsolutePath = nau::FileSystemExtensions::resolveToNativePathContentFolder(sourceRelativePath.data());

    if (!std::filesystem::exists(std::filesystem::u8path(sourceAbsolutePath)) || std::filesystem::is_directory(std::filesystem::u8path(sourceAbsolutePath))) {
        sourceAbsolutePath = nau::pathToUtf8(std::filesystem::u8path(assetPath).replace_extension());
    }

    if (!std::filesystem::exists(std::filesystem::u8path(sourceAbsolutePath)) || std::filesystem::is_directory(std::filesystem::u8path(sourceAbsolutePath))) {
        NED_ERROR("Source file does not exist!");
        return std::string();
    }

    return sourceAbsolutePath;
}

std::string_view NauAssetManager::assetFileFilter() const
{
    return m_assetProcessor->assetFileFilter();
}

std::string_view NauAssetManager::assetFileExtension() const
{
    return m_assetProcessor->assetFileExtension();
}

void NauAssetManager::initializeWatcher(const std::string& contentDir)
{
    m_assetWatcher = std::make_unique<NauAssetWatcher>(contentDir.c_str());

    m_assetWatcher->connect(m_assetWatcher.get(), &NauAssetWatcher::eventAssetAdded, [this](const std::string& assetPath) {
        onAssetAdded(assetPath);
    });    
    m_assetWatcher->connect(m_assetWatcher.get(), &NauAssetWatcher::eventAssetRemoved, [this](const std::string& assetPath) {
        onAssetRemoved(assetPath);
    });
    m_assetWatcher->connect(m_assetWatcher.get(), &NauAssetWatcher::eventAssetChanged, [this](const std::string& assetPath) {
        onAssetChanged(assetPath);
    });
}

void NauAssetManager::addClient(const NauAssetTypesList& types, NauAssetManagerClientInterface* client)
{
    for (NauEditorFileType assetType : types) {
        m_watcherClients[assetType].push_back(client);
    }
}

void NauAssetManager::onAssetAdded(const std::string& assetPath)
{
    if (isAssetMetaFile(assetPath)) {
        assetAdded(assetPath);
    } else {
        sourceAdded(assetPath);
    }
}

void NauAssetManager::onAssetRemoved(const std::string& assetPath)
{
    if (isAssetMetaFile(assetPath)) {
        assetRemoved(assetPath);
    } else {
        sourceRemoved(assetPath);
    }
}

void NauAssetManager::onAssetChanged(const std::string& assetPath)
{
    if (isAssetMetaFile(assetPath)) {
        return;
    }

    const std::string nausdRelativePath = nau::pathToUtf8(nau::FileSystemExtensions::getRelativeAssetPath(std::filesystem::u8path(assetPath), true));
    auto uid = m_assetDb.getUidFromSourcePath(nausdRelativePath.c_str());
    if (!uid) {
        return;
    }

     nau::IAssetDescriptor::Ptr asset = nau::getServiceProvider().get<nau::IAssetManager>().openAsset(nau::AssetPath(("uid:" + nau::toString(uid)).c_str()));
     nau::IAssetDescriptor::LoadState state = asset->getLoadState();
     
     nau::IAssetDescriptor::UnloadResult unloadResult = nau::IAssetDescriptor::UnloadResult::Unloaded;
     if (state == nau::IAssetDescriptor::LoadState::Ready) {
         unloadResult = asset->unload();
     }
     
     m_assetProcessor->importAsset(m_projectPath, std::filesystem::u8path(assetPath));
     
     // Load asset again if needed
     state = asset->getLoadState();
     if ((unloadResult == nau::IAssetDescriptor::UnloadResult::UnloadedHasReferences) && (state == nau::IAssetDescriptor::LoadState::None)) {
         asset->load();
     }
}

void NauAssetManager::assetAdded(const std::string& assetPath)
{
    m_assetProcessor->importAsset(m_projectPath, std::filesystem::u8path(assetPath));

    m_assetDb.reloadAssetDB("assets_database/database.db");
    const auto sourcePath = sourcePathFromAsset(assetPath);
    const auto kind = typeResolver()->resolve(assetPath.c_str()).type;
    for (auto watcher : m_watcherClients[kind]) {
        watcher->handleSourceAdded(sourcePath);
    }
}

void NauAssetManager::assetRemoved(const std::string& assetPath)
{
    m_assetDb.reloadAssetDB("assets_database/database.db");
}

void NauAssetManager::sourceAdded(const std::string& sourcePath)
{
    m_assetProcessor->importAsset(m_projectPath, std::filesystem::u8path(sourcePath));
}

void NauAssetManager::sourceRemoved(const std::string& sourcePath)
{
}

bool NauAssetManager::isAssetMetaFile(const std::string& assetFilePath)
{
    return m_assetProcessor->isAssetMetaFile(assetFilePath);;
}

bool NauAssetManager::isAssetValid(const std::string& assetFilePath)
{
    return m_assetProcessor->isAssetValid(assetFilePath);
}
