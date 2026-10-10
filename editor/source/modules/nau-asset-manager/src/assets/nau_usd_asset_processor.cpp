// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/assets/nau_usd_asset_processor.hpp"
#include "nau/usd_meta_tools/usd_meta_generator.h"
#include "nau/usd_meta_tools/usd_meta_manager.h"
#include "usd_proxy/usd_prim_proxy.h"

#include "nau_log.hpp"
#include "nau_assert.hpp"

#include "pxr/usd/sdf/listEditorProxy.h"
#include "nau/assets/nau_file_types.hpp"
#include "nau/asset_tools/asset_api.h"
#include "nau/shared/file_system.h"

#include <QApplication>
#include "nau/usd_meta_tools/usd_meta_info.h"

#include <pxr/usd/ar/defineResolver.h>

#include "nau/service/service_provider.h"
#include "nau/assets/asset_db.h"

// ** NauUsdTypesResolver

class NauUsdTypesResolver : public NauProjectBrowserItemTypeResolverInterface
{
public:
    ItemTypeDescription resolve(const QString& assetFilePath, const std::optional<std::string> primPath = std::nullopt) const override
    {
        static const std::unordered_map<std::string, ItemTypeDescription> typesMap = {
            { "mesh", { NauEditorFileType::Model, QStringLiteral("Mesh Asset"), QStringLiteral("Mesh asset file")} },
            { "sound", { NauEditorFileType::RawAudio, QStringLiteral("Sound"), QStringLiteral("Sound asset file")} },
            { "audio-container", { NauEditorFileType::AudioContainer, QStringLiteral("Audio Container Asset"), QStringLiteral("Audio Container file")} },
            { "ui", { NauEditorFileType::UI, QStringLiteral("UI"), QStringLiteral("In game ui asset file")} },
            { "vfx", { NauEditorFileType::VFX, QStringLiteral("VFX Asset"), QStringLiteral("VFX file")} },
            { "font", { NauEditorFileType::Font, QStringLiteral("Font Asset"), QStringLiteral("Font file")} },
            { "material", { NauEditorFileType::Material, QStringLiteral("Material Asset"), QStringLiteral("Material file")} },
            { "shader", { NauEditorFileType::Shader, QStringLiteral("Shader Asset"), QStringLiteral("Shader file")} },
            { "texture", { NauEditorFileType::Texture, QStringLiteral("Texture Asset"), QStringLiteral("Texture file")} },
            { "physics-material", { NauEditorFileType::PhysicsMaterial, QStringLiteral("Physics Material"), QStringLiteral("Physics file")} },
            { "scene", { NauEditorFileType::Scene, QStringLiteral("Scene"), QStringLiteral("Scene file")} },
            { "animation", { NauEditorFileType::Animation, QStringLiteral("Animation Asset"), QStringLiteral("Animation file")} },
            { "input", { NauEditorFileType::Action, QStringLiteral("Input Asset"), QStringLiteral("Input file")} },
        };

        if(!assetFilePath.endsWith(".nausd")) 
        {
            return ItemTypeDescription { NauEditorFileType::Unrecognized, {}, {}};
        }

        nau::UsdMetaInfoArray metaInfoArray = nau::UsdMetaManager::instance().getInfo(assetFilePath.toUtf8().constData());

        if (metaInfoArray.empty()) {
            return ItemTypeDescription { NauEditorFileType::Unrecognized, {}, {}};
        }

        // TODO: Refactor source getting. Get it from UsdMetaInfo
        auto stage = pxr::UsdStage::Open(assetFilePath.toStdString(), pxr::UsdStage::LoadNone);

        if (!stage) {
            return ItemTypeDescription { NauEditorFileType::Unrecognized, {}, {}};
        }

        std::string typeName;
        // TODO Parse tree correctly
        for (const auto& metaInfo : metaInfoArray) {
            findUnderlyingTypeName(metaInfo, typeName);
        }

        auto element = typesMap.find(typeName);
        if (element == typesMap.end()) {
            return ItemTypeDescription { NauEditorFileType::Unrecognized, {}, {}};
        }

        return element->second;
    }

    void findUnderlyingTypeName(const nau::UsdMetaInfo& root, std::string& typeName) const
    {
        /*
        * When we parse metaInfo we have 2 options:
        * - 1 size array that contains undefined (usda)
        * - 2 size array that contains type information. (nausd).
        * We need to find a child that has no children and 
        * the type is different from group and undefined. 
        * This child will store the name of the type. 
        * For optimisation, we can exit after 
        * the first type name found.
        */

        // TODO Parse tree correctly
        if (root.type != "group" && root.type != "undefined") {
            typeName = root.type;
        }

        if (root.type != "group" && root.type != "undefined" && root.children.empty()) {
            typeName = root.type;
        }

        for (const auto& child : root.children) {
            findUnderlyingTypeName(child, typeName);
        }
    }
};


// ** NauUsdAssetProcessor

void NauUsdAssetProcessor::setAssetFileUid(const std::filesystem::path& assetSource)
{
    auto stage = pxr::UsdStage::Open(nau::pathToUtf8(assetSource));
    if (!stage) {
        return;
    }

    auto root = stage->GetPseudoRoot();
    auto children = root.GetAllChildren();
    if (children.empty()) {
        return;
    }

    // TODO: Now we can build only from one NauMaterialPipline
    auto materialPrim = children.front();

    UsdProxy::UsdProxyPrim proxyPrim(materialPrim);
    auto proxyProp = proxyPrim.getProperty(pxr::TfToken("uid"));
    if (proxyProp) {
        auto uid = nau::Uid::generate();
        proxyProp->setValue(pxr::VtValue(nau::toString(uid)));
    }

    stage->Save();
}

int NauUsdAssetProcessor::importAsset(const std::filesystem::path& project, const std::filesystem::path& assetSource)
{
    auto args = std::make_unique<nau::ImportAssetsArguments>();

    args->projectPath = nau::pathToUtf8(project);
    if (!assetSource.empty()) {
        args->assetPath = nau::pathToUtf8(assetSource);
    }

    // TODO: remove this code when compiler sequence will be in right order.
    int result = 1;
    for (int i = 0; i < 4 && result != 0; ++i)
    {
        result = nau::importAssets(args.get());
        nau::getServiceProvider().get<nau::IAssetDB>().reloadAssetDB("assets_database/database.db");
    }

    return result;
}

std::string NauUsdAssetProcessor::sourcePathFromAssetFile(const std::string& assetPath)
{
    // TODO: Refactor source getting. Get it from UsdMetaInfo
    auto stage = pxr::UsdStage::Open(assetPath);
    if (!stage) {
        return std::string();
    }

    auto rootPrim = stage->GetPrimAtPath(pxr::SdfPath("/Root"));
    if (!rootPrim) {
        return std::string();
    }

    auto proxyPrim = UsdProxy::UsdProxyPrim(rootPrim);

    auto pathProperty = proxyPrim.getProperty(pxr::TfToken("path"));

    if (pathProperty) {
        pxr::VtValue val;
        pathProperty->getValue(&val);

        if (val.IsHolding<pxr::SdfAssetPath>()) {
            return val.Get<pxr::SdfAssetPath>().GetResolvedPath();
        }
    }

    auto uidProperty = proxyPrim.getProperty(pxr::TfToken("uid"));
    if (uidProperty)
    {
        pxr::VtValue val;
        uidProperty->getValue(&val);

        if (val.IsHolding<std::string>())
        {
            std::string stringUID = val.Get<std::string>();
            auto uid = nau::Uid::parseString(stringUID);
            auto& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();
            // Returns struct from DB (using UID)
            const auto& assetMetaInfo = assetDb.findAssetMetaInfoByUid(*uid);
            // Returns the path to the content folder (no more)
            const auto& assetFolderPath = PXR_NS::ArGetResolver().Resolve("uid:" + stringUID);

            return assetFolderPath.GetPathString() + "\\" + std::string(assetMetaInfo.sourcePath.c_str());
        }
    }

    return std::string();
}

std::string_view NauUsdAssetProcessor::assetFileFilter() const
{
    static constexpr std::string_view assetExtensionFilter = "*.nausd";
    return assetExtensionFilter;
}

std::string_view NauUsdAssetProcessor::assetFileExtension() const
{
    static constexpr std::string_view assetExtension = ".nausd";
    return assetExtension;
}

bool NauUsdAssetProcessor::isAssetMetaFile(const std::string& assetFilePath)
{
    // TODO: Check asset file extension (.nausd)
    return isAssetValid(assetFilePath);
}

bool NauUsdAssetProcessor::isAssetValid(const std::string& assetFilePath)
{
    nau::UsdMetaInfoArray metaInfoArray = nau::UsdMetaManager::instance().getInfo(assetFilePath);

    if (metaInfoArray.empty()) {
        return false;
    }

    // Find at least one valid asset
    // An invalid asset could potentially be loaded
    // If we ignore at least one valid asset here, 
    // we cannot add assets to a resource widget of that type 
    // (vfx, mesh, ui e t.c.).
    bool isValid = false;
    for (const nau::UsdMetaInfo& meta : metaInfoArray) {
        if (meta.isValid) {
            isValid = true;
        }
    }
    return isValid;
}

std::vector<std::string> NauUsdAssetProcessor::getAssetPrimsPath(const std::string& assetPath)
{
    std::vector<std::string> subAssetsPath;

    nau::UsdMetaInfoArray metaInfoArray = nau::UsdMetaManager::instance().getInfo(assetPath);

    if (metaInfoArray.empty()) {
        return subAssetsPath;
    }

    std::function<void(nau::UsdMetaInfoArray&)> getAllSubAssetPaths;

    getAllSubAssetPaths = [&subAssetsPath, &getAllSubAssetPaths](nau::UsdMetaInfoArray& metaInfoArray){
        for (nau::UsdMetaInfo& meta : metaInfoArray) {
            if (meta.isValid) {
                subAssetsPath.push_back(meta.metaSourcePath);
                getAllSubAssetPaths(meta.children);
            }
        }
    };

    getAllSubAssetPaths(metaInfoArray);
    return subAssetsPath;
}

std::shared_ptr<NauProjectBrowserItemTypeResolverInterface> NauUsdAssetProcessor::createTypeResolver()
{
    return std::make_shared<NauUsdTypesResolver>();
}
