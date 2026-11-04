// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_asset_manager_impl.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"

#include <algorithm>
#include <format>
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
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/primSpec.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/stringUtils.h>
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
    if (!ensureUniqueUid(assetPath)) {
        NED_ERROR("Failed to ensure unique uid for {}, import aborted", assetPath);
        return;
    }

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

        const auto children = stage->GetPseudoRoot().GetAllChildren();
        if (children.empty()) {
            return false;
        }
        pxr::UsdPrim assetPrim = children.front();

        const pxr::TfToken uidToken("uid");
        pxr::UsdAttribute uidAttr = assetPrim.GetAttribute(uidToken);

        if (!uidAttr) {
            return true;
        }

        std::string currentUid;
        if (!uidAttr.Get(&currentUid)) {
            return true;
        }

        const auto parsedUid = nau::Uid::parseString(currentUid);
        if (!parsedUid) {
            return true;
        }

        // The uid belongs to the asset this file was copied from only if that asset's file still exists.
        // Unknown uid means a brand new asset, a missing original file means the asset was renamed or moved:
        // in both cases the uid must be kept, otherwise references to the asset are lost.
        auto& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();
        const eastl::string registeredNausdPath = assetDb.getNausdPathFromUid(*parsedUid);
        if (registeredNausdPath.empty()) {
            return true;
        }

        const std::filesystem::path registeredAssetPath = nau::FileSystemExtensions::resolveToNativePathContentFolder(registeredNausdPath.c_str());
        std::error_code ec;
        if (!std::filesystem::exists(registeredAssetPath, ec)) {
            // Renaming keeps the file modification time, so the importer would treat the asset as up to date
            // and keep the old path in the asset database. Touch the file to make it reimport under the new path.
            std::filesystem::last_write_time(assetPath, std::filesystem::file_time_type::clock::now(), ec);
            return true;
        }
        if (std::filesystem::equivalent(registeredAssetPath, assetPath, ec)) {
            return true;
        }

        const std::string newUid = nau::toString(nau::Uid::generate());
        if (!uidAttr.Set(newUid)) {
            return false;
        }

        // Renderer batches instances by material name, so a copy with the original prim name is drawn with the original material
        if (typeResolver()->resolve(assetPath.c_str()).type == NauEditorFileType::Material) {
            // Same naming as for a newly created material, plus a uid suffix: a copied folder has the same file names
            std::string stem = std::filesystem::path(assetPath).stem().string();
            std::replace(stem.begin(), stem.end(), '(', '_');
            std::erase(stem, ')');
            const std::string newPrimName = pxr::TfMakeValidIdentifier(std::format("material_{}_{}", stem, newUid.substr(0, 8)));

            pxr::SdfPrimSpecHandle primSpec = stage->GetRootLayer()->GetPrimAtPath(assetPrim.GetPath());
            if (!primSpec || !primSpec->SetName(newPrimName)) {
                NED_WARNING("Failed to rename material prim in {} to {}", assetPath, newPrimName);
            }
        }

        if (!stage->GetRootLayer()->Save()) {
            return false;
        }

        NED_INFO("Replaced duplicate UID in {} with {}", assetPath, newUid);

        return true;

    } catch (const std::exception &e) {
        NED_ERROR("Exception occurred while ensuring unique uid for {}: {}", assetPath, e.what());
        return false;
    } catch (...) {
        NED_ERROR("Unknown exception occurred while ensuring unique uid for {}", assetPath);
        return false;
    }
}
