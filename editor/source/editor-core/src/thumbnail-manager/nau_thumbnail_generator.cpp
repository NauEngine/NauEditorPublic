// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_thumbnail_generator.hpp"
#include <QImage>


// ** NauThumbnailGeneratorInterface

std::string_view NauThumbnailGeneratorInterface::thumbnailExtension()
{
    static constexpr std::string_view thumbnailExtension = "png";
    return thumbnailExtension;
}


// ** NauTextureThumbnailGenerator

bool NauTextureThumbnailGenerator::generate(const std::filesystem::path& destPath, const std::filesystem::path& source)
{
    QImage image(QString::fromStdWString(source.wstring()));
    // TODO: scale texture or use source as thumbnail and remove generator for textures
    return image.save(QString::fromStdWString(destPath.wstring()), NauThumbnailGeneratorInterface::thumbnailExtension().data());
}