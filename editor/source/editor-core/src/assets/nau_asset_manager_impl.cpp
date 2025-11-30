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

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/valueTypeName.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/ar/defineResolver.h>


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
    m_projectPath = project.path().root().absolutePath().toUtf8().constData();
    m_assetProcessor->importAsset(m_projectPath, "");
}

void NauAssetManager::importAsset(const std::string& sourcePath)
{
    m_assetProcessor->importAsset(m_projectPath, sourcePath);
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

    const std::string nausdRelativePath = nau::FileSystemExtensions::getRelativeAssetPath(std::filesystem::path(assetPath), false).string();
    auto sourceRelativePath = assetDb.getSourcePathFromNausdPath(nausdRelativePath.c_str()); /// +"." + assetInfo[0].type.c_str();
    std::string sourceAbsolutePath = nau::FileSystemExtensions::resolveToNativePathContentFolder(sourceRelativePath.data());

    if (!std::filesystem::exists(sourceAbsolutePath) || std::filesystem::is_directory(sourceAbsolutePath)) {
        sourceAbsolutePath = std::filesystem::path(assetPath).replace_extension().string();
    }

    if (!std::filesystem::exists(sourceAbsolutePath) || std::filesystem::is_directory(sourceAbsolutePath)) {
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

    const std::string nausdRelativePath = nau::FileSystemExtensions::getRelativeAssetPath(std::filesystem::path(assetPath), true).string();
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
     
     m_assetProcessor->importAsset(m_projectPath, assetPath);
     
     // Load asset again if needed
     state = asset->getLoadState();
     if ((unloadResult == nau::IAssetDescriptor::UnloadResult::UnloadedHasReferences) && (state == nau::IAssetDescriptor::LoadState::None)) {
         asset->load();
     }
}

void NauAssetManager::assetAdded(const std::string& assetPath)
{
    ensureUniqueUid(assetPath);

    m_assetProcessor->importAsset(m_projectPath, assetPath);

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
    m_assetProcessor->importAsset(m_projectPath, sourcePath);
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

bool NauAssetManager::ensureUniqueUid(const std::string& assetPath)
{
    try {
        auto stage = pxr::UsdStage::Open(assetPath, pxr::UsdStage::LoadNone);
        if (!stage) {
            return false;
        }

        pxr::UsdPrim assetPrim = stage->GetPrimAtPath(pxr::SdfPath("/Root"));
        if (!assetPrim) {
            const auto children = stage->GetPseudoRoot().GetAllChildren();
            if (children.empty()) {
                return false;
            }
            assetPrim = children.front();
        }

        const pxr::TfToken uidToken("uid");
        pxr::UsdAttribute uidAttr = assetPrim.GetAttribute(uidToken);

        if (!uidAttr) {
            return true;
        }

        std::string currentUid;
        if (!uidAttr.Get(&currentUid)) {
            return true;
        }

        auto &assetDb = nau::getServiceProvider().get<nau::IAssetDB>();

        try {
            const auto &foundMeta = assetDb.findAssetMetaInfoByUid(*(nau::Uid::parseString(currentUid)));

            eastl::string nausdPathE = assetDb.getNausdPathFromUid(*(nau::Uid::parseString(currentUid)));
            std::string nausdPath = nausdPathE.c_str();

            auto normalizePath = [](const std::string& p) {
                std::string result = std::filesystem::absolute(p).string();
                for (auto& c : result) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
                return result;
            };

            if (normalizePath(nausdPath) == normalizePath(assetPath)) {
                return true;
            }

        } catch (...) {
            return true;
        }

        const std::string newUid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        if (!uidAttr.Set(newUid)) {
            return false;
        }
        if (!stage->GetRootLayer()->Save()) {
            return false;
        }

        NED_INFO("Replaced duplicate UID in {} with {}", assetPath, newUid);

        return true;

    } catch (const std::exception &e) {
        NED_ERROR("Exception occured while ensuring unique uid for {}: {}", assetPath, e.what());
        return false;
    } catch (...) {
        NED_ERROR("Unknown exception occured while ensuring unique uid for {}", assetPath);
        return false;
    }
}
