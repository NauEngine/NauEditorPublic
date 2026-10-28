// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/assets/nau_asset_preview.hpp"
#include "nau/app/nau_editor_services.hpp"


// ** NauAssetPreview

bool NauAssetPreview::hasPreview(const QString& assetPath)
{
    return false;

    // TODO: Get thumbnail manager as singleton or store pointer to manager in this class
    //auto thumbnailManager = Nau::Editor().thumbnailManager();
    //if (!thumbnailManager) {
    //    return false;
    //}

    //const std::filesystem::path stdAssetPath = assetPath.toUtf8().constData();
    //return thumbnailManager->exists(stdAssetPath);
}

QIcon NauAssetPreview::assetPreview(const QString& assetPath)
{
    // TODO: Get thumbnail manager as singleton or store pointer to manager in this class
    auto thumbnailManager = Nau::Editor().thumbnailManager();
    const std::filesystem::path stdAssetPath = assetPath.toStdWString();
    
    return QIcon(QString::fromStdWString(thumbnailManager->thumbnailForAsset(stdAssetPath).second.wstring()));
}