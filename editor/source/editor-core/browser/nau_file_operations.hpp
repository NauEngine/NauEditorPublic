// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Base file system items operations.

#pragma once

#include "nau/nau_editor_config.hpp"

#include <QModelIndexList>
#include <QStringList>

#include <functional>


// ** NauFileOperations

class NAU_EDITOR_API NauFileOperations
{
public:
    // Meta file format is known to the asset system only, so it provides the operations on meta files.
    struct AssetMetaOperations
    {
        // Returns the declared source path, including a missing source; empty means no source.
        std::function<QString(const QString& metaFilePath)> sourcePath;
    };

    static void setAssetMetaOperations(AssetMetaOperations operations);

    static void copyToClipboard(const QModelIndexList& indexes);
    static void cutToClipboard(const QModelIndexList& indexes);
    // Keeps the clipboard unchanged on failure.
    static bool pasteFromClipboard(const QModelIndex& parent);

    // Moves each item to trash intact; rolls back completed moves on failure.
    static bool moveToTrash(const QStringList& paths);

    // Includes sources unless referenced by unselected assets.
    static bool moveAssetsToTrash(const QStringList& paths);

    // Supports cross-volume moves; rolls back completed steps on failure.
    static bool movePaths(const QStringList& srcPaths, const QString& dstDir);

    // Returns the declared source path, even if missing; empty means no source.
    static QString assetSourcePath(const QString& path);

    // Finds other meta files referencing the same source, excluding ignored paths.
    static QStringList assetsSharingSource(const QString& path, const QStringList& ignored = {});

    static void duplicate(const QModelIndexList& indexes);

    // Returns available file name if specified exists.
    // New file name is generated via generateFileName().
    static QString generateFileNameIfExists(const QString& absFilePath);

    // Copy directory from src to dst with respect to src structure.
    // Set overwrite if directory/file should be replaced if it does exist in dst directory.
    // Returns false on failure on any item's copying.
    static bool copyPathRecursively(const QString& src, const QString& dst, bool overwrite = false);

    // Appends the counter to baseFileName with respect to extension and file name.
    // 'some file.txt' -> some file(1).txt'
    // some file(1).txt' -> some file(2).txt'
    static QString generateFileName(const QString& baseFileName);

private:
    static AssetMetaOperations m_assetMetaOperations;

    static bool isAssetMetaFile(const QString& path);

    static QString cutOperationMimeType();
    static QByteArray copyDropEffectData();
    static QByteArray cutDropEffectData();

    static QMimeData* prepareCopyCut(const QModelIndexList& indexes);
};
