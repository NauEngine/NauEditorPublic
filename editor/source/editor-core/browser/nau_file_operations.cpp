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
#include <QUuid>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/valueTypeName.h>
#include <pxr/base/tf/token.h>


static bool updateUsdMaterialUid(const QString &filePath) {
    try {
        auto stage = pxr::UsdStage::Open(filePath.toStdString());
        if (!stage) {
            return false;
        }

        const auto root = stage->GetPseudoRoot();
        const auto children = root.GetAllChildren();
        if (children.empty()) {
            return false;
        }

        const auto matPrim = children.front();
        const pxr::TfToken uidToken("uid");

        pxr::UsdAttribute attr = matPrim.GetAttribute(uidToken);
        if (!attr) {
            attr = matPrim.CreateAttribute(uidToken, pxr::SdfValueTypeNames->String);
        }

        const std::string newUid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        if (!attr.Set(newUid)) {
            return false;
        }

        stage->GetRootLayer()->Save();
        return true;
    } catch (const std::exception &) {
        return false;
    } catch (...) {
        return false;
    }
}

// ** NauFileOperations

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

    mimeData->setData(cutOperationMimeType(), cutDropEffectData());
    QApplication::clipboard()->setMimeData(mimeData);
}

void NauFileOperations::pasteFromClipboard(const QModelIndex& parent)
{
    auto clipboard = QApplication::clipboard();
    auto mimeData = clipboard->mimeData();
    if (!mimeData || !mimeData->hasUrls()) return;

    const bool cutting = mimeData->data(cutOperationMimeType()) == cutDropEffectData();
    const QString path = parent.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
    const QFileInfo fi{path};
    const NauDir dstPath =  fi.isDir() ? path : fi.absolutePath();

    for (const auto& url : mimeData->urls()) {
        const QString srcPath = url.toLocalFile();
        const bool isDir = QFileInfo{srcPath}.isDir();

        if (isDir && dstPath.absolutePath().startsWith(srcPath)) {
            NED_WARNING("Unable to {} {} to {}. Source and Destination is the same.",
                (cutting ? "move" : "copy"), srcPath.toUtf8().constData(), dstPath.absolutePath().toUtf8().constData());
            continue;
        }

        const QString srcFileName = QFileInfo{srcPath}.fileName();
        const QString dstFileName = !cutting 
            ? generateFileNameIfExists(dstPath.absoluteFilePath(srcFileName))
            : dstPath.absoluteFilePath(srcFileName);

        bool result = false;
        if (cutting) {
            result = isDir ? movePathRecursively(srcPath, dstFileName) : QFile::rename(srcPath, dstFileName);
        } else {
            result = isDir ? copyPathRecursively(srcPath, dstFileName) : QFile::copy(srcPath, dstFileName);
        }

        if (!result) {
            NED_ERROR("Failed to {} {} to {}", (cutting ? "move" : "copy"), srcPath.toUtf8().constData(),
                dstFileName.toUtf8().constData());
        } else {
            NED_TRACE("Successfuly {} {} to {}", (cutting ? "moving" : "coping"), srcPath.toUtf8().constData(),
                dstFileName.toUtf8().constData());

            if (!cutting && !isDir) {
                const QFileInfo fiDst(dstFileName);
                if (fiDst.suffix().toLower() == QStringLiteral("nausd")) {
                    if (updateUsdMaterialUid(dstFileName)) {
                        NED_TRACE("Updated uid for copied material {}", dstFileName.toUtf8().constData());
                    } else {
                        NED_TRACE("No uid update applied for {}", dstFileName.toUtf8().constData());
                    }
                } else {
                    NED_TRACE("Skipped uid update for {} (not .nausd)", dstFileName.toUtf8().constData());
                }
            }
        }
    }

    clipboard->clear();
}

void NauFileOperations::duplicate(const QModelIndexList& indexes)
{
    for (const auto& index : indexes) {
        const QString srcPath = index.data(NauProjectBrowserFileSystemModel::FilePathRole).toString();
        const bool isDir = QFileInfo{srcPath}.isDir();
        const NauDir dstPath = QFileInfo{srcPath}.absoluteDir();
        const QString dstFileName = generateFileNameIfExists(srcPath);

        const bool result = isDir 
            ? copyPathRecursively(srcPath, dstFileName)
            : QFile::copy(srcPath, dstFileName);

        if (!result) {
            NED_ERROR("Failed to duplicate {} to {}", srcPath.toUtf8().constData(), dstFileName.toUtf8().constData());
        } else {
            NED_TRACE("Duplicated {} to {}", srcPath.toUtf8().constData(), dstFileName.toUtf8().constData());

            if (!isDir) {
                const QFileInfo fiDst(dstFileName);
                if (fiDst.suffix().toLower() == QStringLiteral("nausd")) {
                    if (updateUsdMaterialUid(dstFileName)) {
                        NED_TRACE("Updated uid for duplicated file {}", dstFileName.toUtf8().constData());
                    } else {
                        NED_WARNING("No uid update applied for {}", dstFileName.toUtf8().constData());
                    }
                } else {
                    NED_TRACE("Skipped uid update for {} (not .nausd)", dstFileName.toUtf8().constData());
                }
            }
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

        const QFileInfo copiedFI(dstAbsFileName);
        const QString suffix = copiedFI.suffix().toLower();
        if (suffix == QStringLiteral("nausd")) {
            if (updateUsdMaterialUid(dstAbsFileName)) {
                NED_TRACE("Updated uid for copied file {}", dstAbsFileName.toUtf8().constData());
            } else {
                NED_TRACE("No uid update applied for copied file {}", dstAbsFileName.toUtf8().constData());
            }
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

bool NauFileOperations::movePathRecursively(const QString& src, const QString& dst)
{
    NauDir srcDir(src);
    if (!srcDir.exists()) return false;

    NauDir dstDir = dst;
    dstDir.mkpath(dst);

    for (const QString& dirEntry : srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!movePathRecursively(srcDir.absoluteFilePath(dirEntry), dstDir.absoluteFilePath(dirEntry))) return false;
    }

    for (const QString& fileEntry : srcDir.entryList(QDir::Files)) {
        if (!QFile::rename(srcDir.absoluteFilePath(fileEntry), dstDir.absoluteFilePath(fileEntry))) return false;
    }

    return NauDir().rmdir(src);
}

bool NauFileOperations::deletePathRecursively(const QString& path)
{
    const NauDir dir(path);
    for (const QString& dirEntry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!deletePathRecursively(dir.absoluteFilePath(dirEntry))) return false;
    }

    return QFile::moveToTrash(path);
}

