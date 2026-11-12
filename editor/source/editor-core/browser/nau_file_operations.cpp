// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_file_operations.hpp"
#include "nau_project_browser_file_system_model.hpp"
#include "nau_widget_utility.hpp"
#include "nau_log.hpp"

#include <QApplication>
#include <QRegularExpression>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QDirIterator>

#include <algorithm>
#include <vector>

#include <utility>


// ** NauFileTransaction
// Remembers done steps to undo them in reverse order if a next one fails.

namespace
{
    class NauFileTransaction
    {
    public:
        // Note that QFile::rename falls back to copy and remove if src and dst are on different volumes.
        bool moveFile(const QString& src, const QString& dst)
        {
            if (!QFile::rename(src, dst)) {
                NED_ERROR("Failed to move {} to {}", src.toUtf8().constData(), dst.toUtf8().constData());
                return false;
            }

            m_steps.push_back({StepType::Moved, src, dst});
            return true;
        }

        bool moveDirectory(const QString& src, const QString& dst)
        {
            const NauDir srcDir(src);
            if (!srcDir.exists()) return false;

            if (!QFileInfo::exists(dst)) {
                if (!NauDir().mkdir(dst)) return false;
                m_steps.push_back({StepType::DirCreated, QString(), dst});
            }

            const NauDir dstDir = dst;
            for (const QString& dirEntry : srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
                if (!moveDirectory(srcDir.absoluteFilePath(dirEntry), dstDir.absoluteFilePath(dirEntry))) return false;
            }

            for (const QString& fileEntry : srcDir.entryList(QDir::Files | QDir::Hidden | QDir::System)) {
                if (!moveFile(srcDir.absoluteFilePath(fileEntry), dstDir.absoluteFilePath(fileEntry))) return false;
            }

            if (!NauDir().rmdir(src)) {
                NED_ERROR("Failed to remove directory {}", src.toUtf8().constData());
                return false;
            }

            m_steps.push_back({StepType::DirRemoved, src, QString()});
            return true;
        }

        bool moveToTrash(const QString& path)
        {
            if (!QFileInfo::exists(path)) {
                NED_WARNING("Unable to move to trash {}: path doesn't exist", path.toUtf8().constData());
                return false;
            }

            // Directories are moved with their content at once. Moving the content item by item
            // creates a separate trash entry for each subdirectory and leaves a partially removed tree on failure.
            // Note that Qt reports success without moving anything if Windows declines the operation,
            // e.g. when the directory is watched by QFileSystemWatcher. So make sure the path has really gone.
            QString pathInTrash;
            if (!QFile::moveToTrash(path, &pathInTrash) || QFileInfo::exists(path)) {
                NED_ERROR("Failed to move to trash {}", path.toUtf8().constData());
                return false;
            }

            m_steps.push_back({StepType::Moved, path, pathInTrash});
            return true;
        }

        void rollback()
        {
            for (auto step = m_steps.crbegin(); step != m_steps.crend(); ++step) {
                bool result = false;
                switch (step->type)
                {
                case StepType::Moved: result = QFile::rename(step->to, step->from); break;
                case StepType::DirCreated: result = NauDir().rmdir(step->to); break;
                case StepType::DirRemoved: result = NauDir().mkdir(step->from); break;
                }

                if (!result) {
                    NED_ERROR("Failed to undo file operation. From: \"{}\", to: \"{}\"",
                        step->from.toUtf8().constData(), step->to.toUtf8().constData());
                }
            }

            m_steps.clear();
        }

    private:
        enum class StepType
        {
            Moved,
            DirCreated,
            DirRemoved,
        };

        struct Step
        {
            StepType type;
            QString from;
            QString to;
        };

        std::vector<Step> m_steps;
    };
}


// ** NauFileOperations

NauFileOperations::AssetMetaOperations NauFileOperations::m_assetMetaOperations;

void NauFileOperations::setAssetMetaOperations(AssetMetaOperations operations)
{
    m_assetMetaOperations = std::move(operations);
}

void NauFileOperations::copyToClipboard(const QModelIndexList& indexes)
{
    auto mimeData = prepareCopyCut(indexes);
    if (!mimeData) return;

    mimeData->setData(cutOperationMimeType(), copyDropEffectData());
    QApplication::clipboard()->setMimeData(mimeData);
}

void NauFileOperations::cutToClipboard(const QModelIndexList& indexes)
{
    auto mimeData = prepareCopyCut(indexes);
    if (!mimeData) return;

    // Asset meta file refers to its source by relative path, so the source has to be moved too.
    // Sources go first: a meta file must not appear in a new place until its source is there.
    QList<QUrl> sourceUrls;
    for (const QUrl& url : mimeData->urls()) {
        const QString sourcePath = assetSourcePath(url.toLocalFile());
        if (!sourcePath.isEmpty() && !sourceUrls.contains(QUrl::fromLocalFile(sourcePath))) {
            sourceUrls.append(QUrl::fromLocalFile(sourcePath));
        }
    }
    mimeData->setUrls(sourceUrls + mimeData->urls());

    mimeData->setData(cutOperationMimeType(), cutDropEffectData());
    QApplication::clipboard()->setMimeData(mimeData);
}

bool NauFileOperations::pasteFromClipboard(const QModelIndex& parent)
{
    auto clipboard = QApplication::clipboard();
    auto mimeData = clipboard->mimeData();
    if (!mimeData || !mimeData->hasUrls()) return true;

    const bool cutting = mimeData->data(cutOperationMimeType()) == cutDropEffectData();
    const QString path = parent.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
    const QFileInfo fi{path};
    const NauDir dstPath =  fi.isDir() ? path : fi.absolutePath();

    QStringList srcPaths;
    for (const auto& url : mimeData->urls()) {
        srcPaths << url.toLocalFile();
    }

    bool result = true;
    if (cutting) {
        result = movePaths(srcPaths, dstPath.absolutePath());
    } else {
        // Copy keeps its existing behavior; source/meta duplication is handled separately.
        for (const QString& srcPath : srcPaths) {
            const bool isDir = QFileInfo{srcPath}.isDir();
            if (isDir && dstPath.absolutePath().startsWith(srcPath)) {
                result = false;
                continue;
            }

            const QString dstFileName = generateFileNameIfExists(
                dstPath.absoluteFilePath(QFileInfo{srcPath}.fileName()));
            const bool copied = isDir ? copyPathRecursively(srcPath, dstFileName)
                                      : QFile::copy(srcPath, dstFileName);
            if (!copied) {
                NED_ERROR("Failed to copy {} to {}", srcPath.toUtf8().constData(), dstFileName.toUtf8().constData());
                result = false;
            }
        }
    }

    if (result) {
        clipboard->clear();
    }
    return result;
}

void NauFileOperations::duplicate(const QModelIndexList& indexes)
{
    for (const auto& index : indexes) {
        const QString srcPath = index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
        const NauDir dstPath = QFileInfo{srcPath}.absoluteDir();
        const QString dstFileName = generateFileNameIfExists(srcPath);

        const bool result = QFileInfo{srcPath}.isDir() 
            ? copyPathRecursively(srcPath, dstFileName)
            : QFile::copy(srcPath, dstFileName);

        if (!result) {
            NED_ERROR("Failed to duplicate {} to {}", srcPath.toUtf8().constData(), dstFileName.toUtf8().constData());
        } else {
            NED_TRACE("Duplicated {} to {}", srcPath.toUtf8().constData(), dstFileName.toUtf8().constData());
        }
    }
}

QString NauFileOperations::generateFileNameIfExists(const QString& absFilePath)
{
    auto result = absFilePath;

    const QFileInfo info{result};
    const NauDir dir = info.absoluteDir();

    QString newFileName = info.fileName();

    while (QFile::exists(result)) {
        newFileName = generateFileName(newFileName);
        result = dir.absoluteFilePath(newFileName);
    }

    return result;
}

QString NauFileOperations::cutOperationMimeType()
{
#ifndef Q_OS_WIN
#warning Cut operation (i.e. drop effect) from/to OS File Browser is implemented for Win.
#endif // !Q_OS_WIN

    return QStringLiteral("Preferred DropEffect");
}

QByteArray NauFileOperations::copyDropEffectData()
{
    return QByteArray::fromRawData("\x05\x00\x00\x00", 4);
}

QByteArray NauFileOperations::cutDropEffectData()
{
    return QByteArray::fromRawData("\x02\x00\x00\x00", 4);
}

QMimeData* NauFileOperations::prepareCopyCut(const QModelIndexList& indexes)
{
    if (indexes.isEmpty()) return nullptr;

    QList<QUrl> urls;

    for (const auto& index : indexes) {
        const QString path = index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
        urls.append(QUrl::fromLocalFile(path));

        NED_TRACE("Prepare \"{}\" copy/cut through clipboard", path.toUtf8().constData());
    }

    auto mimeData = new QMimeData;
    mimeData->setUrls(urls);

    return mimeData;
}

bool NauFileOperations::copyPathRecursively(const QString& src, const QString& dst, bool overwrite)
{
    const NauDir srcDir(src);
    if (!srcDir.exists()) return false;

    if (!NauDir().mkpath(dst)) return false;
    const NauDir dstDir = dst;

    for (const QString& dirEntry : srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!copyPathRecursively(srcDir.absoluteFilePath(dirEntry), dstDir.absoluteFilePath(dirEntry), overwrite)) return false;
    }

    for (const QString& fileEntry : srcDir.entryList(QDir::Files)) {
        const QString dstAbsFileName = dstDir.absoluteFilePath(fileEntry);

        if (overwrite && NauFile::exists(dstAbsFileName)) {
            NauFile::remove(dstAbsFileName);
        }

        if (!QFile::copy(srcDir.absoluteFilePath(fileEntry), dstAbsFileName)) { 
            return false;
        }
    }

    return true;
}

QString NauFileOperations::generateFileName(const QString& fileName)
{
    const QFileInfo info {fileName} ;
    static const auto dotSeparator = QStringLiteral(".");

    QString extension = info.completeSuffix().isEmpty() ? QString() : dotSeparator + info.completeSuffix();
    if (fileName.endsWith(dotSeparator)) {
        extension = dotSeparator;
    }

    const NauDir dir = info.absoluteDir();
    QString baseFileName = info.baseName();

    static const QRegularExpression rx{"^(.*?)\\((\\d*?)\\)$"};
    int counter = 1;

    const auto match = rx.match(baseFileName);

    // if a name has format 'abcd(number)', we increment number, that is contained in it.
    // 1st matched group is a base file name, 2nd is a counter.
    // e.g.: 'test(5)' -> {1:'test', 2:5}
    if (match.hasMatch() && match.hasCaptured(1) && match.hasCaptured(2)) {
        baseFileName = match.captured(1);
        counter = match.captured(2).toInt() + 1;
    }

    static const auto templateStr = QStringLiteral("%1(%2)%3");
    return templateStr.arg(baseFileName).arg(counter).arg(extension);
}

bool NauFileOperations::movePaths(const QStringList& srcPaths, const QString& dstDir)
{
    const NauDir dstPath = dstDir;
    NauFileTransaction transaction;

    for (const QString& srcPath : srcPaths) {
        const QFileInfo srcInfo{srcPath};
        const QString dstFileName = dstPath.absoluteFilePath(srcInfo.fileName());

        // Item is already there.
        if (srcInfo == QFileInfo{dstFileName}) continue;

        bool result = false;
        if (!srcInfo.isDir()) {
            result = transaction.moveFile(srcPath, dstFileName);
        } else if (dstPath.absolutePath().startsWith(srcInfo.absoluteFilePath() + QLatin1Char('/'))) {
            NED_WARNING("Unable to move {} to its own subdirectory {}", srcPath.toUtf8().constData(),
                dstPath.absolutePath().toUtf8().constData());
        } else {
            result = transaction.moveDirectory(srcPath, dstFileName);
        }

        if (!result) {
            transaction.rollback();
            return false;
        }

        NED_TRACE("Successfully moved {} to {}", srcPath.toUtf8().constData(), dstFileName.toUtf8().constData());
    }

    return true;
}

bool NauFileOperations::moveToTrash(const QStringList& paths)
{
    NauFileTransaction transaction;

    for (const QString& path : paths) {
        if (!transaction.moveToTrash(path)) {
            transaction.rollback();
            return false;
        }
    }

    return true;
}

bool NauFileOperations::moveAssetsToTrash(const QStringList& paths)
{
    QStringList sources;
    for (const QString& path : paths) {
        const QString source = assetSourcePath(path);
        if (!source.isEmpty() && QFileInfo{source}.isFile()
            && assetsSharingSource(path, paths).isEmpty() && !sources.contains(source)) {
            sources << source;
        }
    }

    // Remove all sources before their meta files, so the importer cannot recreate the meta files.
    QStringList pathsToTrash = sources + paths;
    pathsToTrash.removeDuplicates();
    return moveToTrash(pathsToTrash);
}

QString NauFileOperations::assetSourcePath(const QString& path)
{
    if (!isAssetMetaFile(path) || !m_assetMetaOperations.sourcePath) return QString();

    return m_assetMetaOperations.sourcePath(path);
}

bool NauFileOperations::isAssetMetaFile(const QString& path)
{
    return path.endsWith(QStringLiteral(".nausd"), Qt::CaseInsensitive);
}

QStringList NauFileOperations::assetsSharingSource(const QString& path, const QStringList& ignored)
{
    const QString sourcePath = assetSourcePath(path);
    if (sourcePath.isEmpty()) return {};

    const auto isSameFile = [](const QString& first, const QString& second) {
        return QFileInfo{first} == QFileInfo{second};
    };

    QStringList result;

    // Source path in a meta file is relative and generated next to it, so looking for other meta files in its directory.
    const NauDir sourceDir = QFileInfo{sourcePath}.absoluteDir();
    for (const QString& fileEntry : sourceDir.entryList({QStringLiteral("*.nausd")}, QDir::Files)) {
        const QString metaFilePath = sourceDir.absoluteFilePath(fileEntry);

        const bool skipped = isSameFile(metaFilePath, path) || std::any_of(ignored.begin(), ignored.end(),
            [&](const QString& ignoredPath) { return isSameFile(metaFilePath, ignoredPath); });

        if (!skipped && isSameFile(assetSourcePath(metaFilePath), sourcePath)) {
            result << metaFilePath;
        }
    }

    return result;
}
