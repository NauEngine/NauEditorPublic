// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_thumbnail_manger_impl.hpp"
#include "nau_log.hpp"

#include <format>
#include "magic_enum/magic_enum_flags.hpp"
#include "nau/assets/asset_db.h"
#include "nau/service/service_provider.h"
#include "nau/shared/file_system.h"


// ** NauThumbnailManager

NauThumbnailManager::NauThumbnailManager(const std::filesystem::path& folder, NauProjectBrowserItemTypeResolverInterfacePtr typeResolver)
    : m_typeResolver(typeResolver)
    , m_thumbnailsFolder(folder)
{
    if (!std::filesystem::exists(folder)) {
        std::filesystem::create_directories(folder);
    }

    registerGenerators();
    generateThumbnails();
}

bool NauThumbnailManager::exists(const std::filesystem::path& assetPath) const
{
    return m_thumbnailMap.contains(assetPath);
}


std::pair<bool, std::filesystem::path> NauThumbnailManager::thumbnailForAsset(const std::filesystem::path& assetPath)
{
    if (m_thumbnailMap.contains(assetPath)) {
        return makeGeneratorResult(m_thumbnailMap[assetPath]);
    }

    return makeGeneratorErrorResult();
}

std::pair<bool, std::filesystem::path> NauThumbnailManager::generateThumbnail(const nau::AssetMetaInfoBase& assetMetaInfoBase)
{
    const std::string nausdPath = nau::FileSystemExtensions::resolveToNativePathContentFolder(assetMetaInfoBase.nausdPath.c_str());

    // If thumbnail exists, return registered path
    if (auto result = thumbnailForAsset(std::filesystem::u8path(nausdPath)); result.first) {
        result;
    }

    const NauEditorFileType assetType = m_typeResolver->resolve(nausdPath.c_str()).type;
    // If there is no generator for an asset of this type, return an empty result
    if (!m_generators.contains(assetType)) {
        return makeGeneratorErrorResult();
    }

    auto& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();

    // TODO <Thumbnail>: Rewrite through access to assetDb
    const std::string nausdRelativePath = nau::pathToUtf8(nau::FileSystemExtensions::getRelativeAssetPath(std::filesystem::u8path(nausdPath), false));
    auto sourceRelativePath = assetDb.getSourcePathFromNausdPath(nausdRelativePath.c_str());
    const std::string sourcePath = nau::FileSystemExtensions::resolveToNativePathContentFolder((sourceRelativePath + "." + assetMetaInfoBase.sourceType).c_str());
    const std::filesystem::path destPath = buildThumbnailPath(std::filesystem::u8path(sourcePath));

    // Try generate thumbnail and return result
    if (m_generators[assetType]->generate(destPath, std::filesystem::u8path(sourcePath))) {
        m_thumbnailMap[std::filesystem::u8path(nausdPath)] = destPath;
    }

    return thumbnailForAsset(std::filesystem::u8path(nausdPath));
}

void NauThumbnailManager::registerGenerators()
{
    m_generators[NauEditorFileType::Texture] = std::make_shared<NauTextureThumbnailGenerator>();
}

void NauThumbnailManager::generateThumbnails()
{
    auto& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();

    for (const auto& generator : m_generators) {
        const NauEditorFileType type = generator.first;

        // TODO <Thumbnail>: Rewrite through access to assetDb
        generateThumbnailsForTypedAssets(type, assetDb.findAssetMetaInfoByKind(magic_enum::enum_name(type).data()));
    }
}

void NauThumbnailManager::generateThumbnailsForTypedAssets(NauEditorFileType type, const eastl::vector<nau::AssetMetaInfoBase>& assets)
{
    auto generator = m_generators[type];

    for (const auto& asset : assets) {
        // TODO <Thumbnail>: Rewrite through access to assetDb
        const std::string sourcePath = nau::FileSystemExtensions::resolveToNativePathContentFolder((asset.sourcePath + "." + asset.sourceType).c_str());
        const std::filesystem::path thumbnailPath = buildThumbnailPath(std::filesystem::u8path(sourcePath));

        if (std::filesystem::exists(thumbnailPath)) {
            m_thumbnailMap[std::filesystem::u8path(sourcePath)] = thumbnailPath;
            continue;
        }

        generateThumbnail(asset);
    }
}

std::filesystem::path NauThumbnailManager::buildThumbnailPath(const std::filesystem::path& assetPath)
{
    return m_thumbnailsFolder / std::filesystem::u8path(std::format("{}.{}", nau::pathToUtf8(assetPath.filename()), NauThumbnailGeneratorInterface::thumbnailExtension().data()));
}

std::pair<bool, std::filesystem::path> NauThumbnailManager::makeGeneratorResult(const std::filesystem::path& path)
{
    const bool generatorResult = !path.empty();
    return { generatorResult, path };
}

std::pair<bool, std::filesystem::path> NauThumbnailManager::makeGeneratorErrorResult()
{
    return { false, std::filesystem::path("") };
}
