// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_project_browser_item_type_resolver.hpp"
#include "nau/nau_constants.hpp"


// ** NauProjectBrowserItemTypeResolver

NauProjectBrowserItemTypeResolver::ItemTypeDescription NauProjectBrowserItemTypeResolver::resolve(const QString& completeExtension, const std::optional<std::string> primPath) const
{
    static const QMap<QString, ItemTypeDescription> repository = {
        {QStringLiteral("blk"), { NauEditorFileType::Config,
            QStringLiteral("BLK Config"), tr("A configuration file that stores varous settings")}},

        {QStringLiteral("das"), { NauEditorFileType::Script,
            QStringLiteral("daScript"), tr("A game script, which implements a logic of events, interactions, etc")}},

        {QStringLiteral("cpp"), { NauEditorFileType::Script,
            QStringLiteral("CPP Source File"), tr("An implementation of game script on C++")}},

        {QStringLiteral("hpp"), { NauEditorFileType::Script,
            QStringLiteral("CPP Header File"), tr("A header file of game script on C++")}},

        {QStringLiteral("h"), { NauEditorFileType::Script,
            QStringLiteral("CPP Header File"), tr("A header file of game script on C++")}},

        {QStringLiteral("gltf"), { NauEditorFileType::Model,
            QStringLiteral("GL Transmission Format"), tr("A standard file format for three-dimensional scenes and models.")}},

        {QStringLiteral("glb"), { NauEditorFileType::Model,
            QStringLiteral("GL Binary"), tr("A binary file format for three-dimensional scenes and models.")}},

        {QStringLiteral("grp"), { NauEditorFileType::Model,
            QStringLiteral("GRP Model"), tr("An internal format for three-dimensional scenes and models")}},

        {QStringLiteral("dxp"), { NauEditorFileType::Texture,
            QStringLiteral("DXP Texture"), tr("An internal format for textures/materials")}},

        {QStringLiteral("jpg"), { NauEditorFileType::Texture,
            QStringLiteral("JPEG Texture"), tr("JPEG texture for material assets")}},

        {QStringLiteral("tga"), { NauEditorFileType::Texture,
            QStringLiteral("TGA Texture"), tr("TGA texture for material assets")}},

        {QStringLiteral("nmat"), { NauEditorFileType::Material,
            QStringLiteral("NAU Material"), tr("Material asset")}},

        {QStringLiteral("action"), {NauEditorFileType::Action,
            QStringLiteral("NAU Input action"), tr("Input action asset")}},

        {QStringLiteral("audio"), { NauEditorFileType::AudioContainer,
            QStringLiteral("Audio container"), tr("Audio container asset")}},

        {QStringLiteral("wav"), { NauEditorFileType::RawAudio,
            QStringLiteral("Raw Audio"), tr("Raw Audio")}},

        {QStringLiteral("vfx"), {NauEditorFileType::VFX,
            QStringLiteral("NAU vfx"), tr("VFX asset")}},

        {QStringLiteral("vromfs"), { NauEditorFileType::VirtualRomFS,
            QStringLiteral("Virtual ROM File System"), tr("An internal format for file system.")}},

        {QStringLiteral("shdump"), { NauEditorFileType::Shader,
            QStringLiteral("Compiled shader"), tr("A shader is a computer program that calculates the appropriate levels of light, color, etc.")}},

        {QStringLiteral("nauproject"), { NauEditorFileType::Project,
            QStringLiteral("NauEditor Project File"), tr("A project file")}},

        {QStringLiteral("nauscene"), { NauEditorFileType::Scene,
            QStringLiteral("NauEditor Scene"), tr("Scene file")}},

        {QStringLiteral("nausd_scene"), { NauEditorFileType::Scene,
            QStringLiteral("NauEditor Scene"), tr("Scene file")}},

        {QString(NAU_ANIMATION_EXTENSION_NAME), { NauEditorFileType::Animation,
            QStringLiteral("Nau animation"), tr("Animation file")}},

        // to be continued...
    };

    auto extension = completeExtension;
    const auto suffixList = completeExtension.split(".", Qt::SkipEmptyParts);

    if (suffixList.size() > 1) {
        // Some project files ends with .vromfs.bin. "bin" here pretty useless to distingush the type of item.
        // So if extension is complex and last part of it is "bin", we take a preceding one to "bin".
        static const auto binExtension = QStringLiteral("bin");

        extension = suffixList.back().compare(binExtension, Qt::CaseInsensitive) == 0
            ? suffixList[suffixList.size() - 2]
            : suffixList.back();
    }

    auto it = repository.find(extension);
    if (it != repository.end()) return *it;

    return ItemTypeDescription { NauEditorFileType::Unrecognized, {}, {}};
}
