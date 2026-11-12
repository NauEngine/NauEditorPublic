// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "browser/nau_file_operations.hpp"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QTemporaryDir>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif


namespace
{
    bool createFile(const QString& path)
    {
        QDir().mkpath(QFileInfo{path}.absolutePath());

        QFile file{path};
        return file.open(QIODevice::WriteOnly) && file.write("nau") == 3;
    }
    // Fake meta file: the first line is a name of the source, the second one is uid.
    bool createMetaFile(const QString& path, const QByteArray& sourceName)
    {
        QDir().mkpath(QFileInfo{path}.absolutePath());

        QFile file{path};
        return file.open(QIODevice::WriteOnly) && file.write(sourceName) == sourceName.size();
    }

    QByteArray readFile(const QString& path)
    {
        QFile file{path};
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    class NauFileOperationsAssetTests : public testing::Test
    {
    protected:
        void SetUp() override
        {
            ASSERT_TRUE(tempDir.isValid());

            NauFileOperations::setAssetMetaOperations({
                [](const QString& metaFilePath) {
                    const QString sourceName = QString::fromUtf8(readFile(metaFilePath).split('\n').front());
                    return sourceName.isEmpty() ? QString()
                        : QFileInfo(QFileInfo{metaFilePath}.absoluteDir(), sourceName).absoluteFilePath();
                },
            });
        }

        void TearDown() override
        {
            NauFileOperations::setAssetMetaOperations({});
        }

        QTemporaryDir tempDir;
    };
}


TEST(NauFileOperationsTests, simpleFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("hello(1).txt");
    
    // Act
    const QString& generatedFileName = NauFileOperations::generateFileName("hello.txt");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, simpleWithComplexExtensionFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("hello(1).grp.bin");
    
    // Act
    const QString& generatedFileName = NauFileOperations::generateFileName("hello.grp.bin");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, emptyFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("(1)");
    
    // Act
    const QString& generatedFileName =  NauFileOperations::generateFileName("");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, strangeFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("((1)");
    
    // Act
    const QString& generatedFileName = NauFileOperations::generateFileName("(");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, simpleWithPathFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("text(1).txt");
    
    // Act
    const QString& generatedFileName =  NauFileOperations::generateFileName("/a/c/b/text.txt");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, withEmptyExtensionFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("hello(1).");
    
    // Act
    const QString& generatedFileName =  NauFileOperations::generateFileName("hello.");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, withNoExtensionFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("hello(1)");
    
    // Act
    const QString& generatedFileName =  NauFileOperations::generateFileName("hello");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, dotFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("(1).");
    
    // Act
    const QString& generatedFileName =  NauFileOperations::generateFileName(".");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, incrementSimpleFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("hello(2).txt");
    
    // Act
    const QString& generatedFileName =  NauFileOperations::generateFileName("hello(1).txt");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, incrementSimpleNoExtensionFileName)
{
    const auto& expectedFileName = QStringLiteral("hello(2)");
    
    // Act
    const QString& generatedFileName =  NauFileOperations::generateFileName("hello(1)");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST(NauFileOperationsTests, incrementComplexFileName)
{
    // Arrange
    const auto& expectedFileName = QStringLiteral("hello(1)(2).txt");
    
    // Act
    const QString& generatedFileName = NauFileOperations::generateFileName("hello(1)(1).txt");

    // Assert
    EXPECT_EQ(generatedFileName, expectedFileName);
}

TEST_F(NauFileOperationsAssetTests, assetSourcePathIsTakenFromMetaFile)
{
    // Arrange
    const QString sourcePath = tempDir.filePath("texture.png");
    const QString metaFilePath = tempDir.filePath("renamed.png.nausd");
    ASSERT_TRUE(createFile(sourcePath));
    ASSERT_TRUE(createFile(tempDir.filePath("renamed.png")));
    ASSERT_TRUE(createMetaFile(metaFilePath, "texture.png"));

    // Act
    const QString result = NauFileOperations::assetSourcePath(metaFilePath);

    // Assert
    EXPECT_EQ(result, sourcePath);
}

TEST_F(NauFileOperationsAssetTests, assetSourcePathOfMetaFileWithoutSource)
{
    // Arrange
    const QString metaFilePath = tempDir.filePath("material.nausd");
    ASSERT_TRUE(createFile(tempDir.filePath("material")));
    ASSERT_TRUE(createMetaFile(metaFilePath, "\nmaterial uid"));

    // Act
    const QString result = NauFileOperations::assetSourcePath(metaFilePath);

    // Assert
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(NauFileOperationsAssetTests, assetSourcePathOfNonMetaFile)
{
    // Arrange
    const QString path = tempDir.filePath("texture.png");
    ASSERT_TRUE(createFile(path));

    // Act
    const QString result = NauFileOperations::assetSourcePath(path);

    // Assert
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(NauFileOperationsAssetTests, assetSourcePathPreservesMissingSource)
{
    const QString meta = tempDir.filePath("texture.png.nausd");
    ASSERT_TRUE(createMetaFile(meta, "texture.png\nold uid"));

    EXPECT_EQ(NauFileOperations::assetSourcePath(meta), tempDir.filePath("texture.png"));
}

TEST(NauFileOperationsTests, moveToTrashNonExistentPath)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    // Act
    const bool result = NauFileOperations::moveToTrash({tempDir.filePath("missing")});

    // Assert
    EXPECT_FALSE(result);
}

TEST(NauFileOperationsTests, moveToTrashDirectoryWithContent)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString dirPath = tempDir.filePath("folder");
    ASSERT_TRUE(createFile(dirPath + "/file.txt"));
    ASSERT_TRUE(createFile(dirPath + "/sub/subsub/file.txt"));

    // Act
    const bool result = NauFileOperations::moveToTrash({dirPath});

    // Assert
    EXPECT_TRUE(result);
    EXPECT_FALSE(QFileInfo::exists(dirPath));
}

TEST(NauFileOperationsTests, copyPathRecursively)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString srcPath = tempDir.filePath("src");
    const QString dstPath = tempDir.filePath("dst");
    ASSERT_TRUE(createFile(srcPath + "/file.txt"));
    ASSERT_TRUE(createFile(srcPath + "/sub/file.txt"));

    // Act
    const bool result = NauFileOperations::copyPathRecursively(srcPath, dstPath);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(QFileInfo::exists(dstPath + "/file.txt"));
    EXPECT_TRUE(QFileInfo::exists(dstPath + "/sub/file.txt"));
    EXPECT_TRUE(QFileInfo::exists(srcPath + "/sub/file.txt"));
}

TEST(NauFileOperationsTests, copyPathRecursivelyNonExistentSource)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    // Act
    const bool result = NauFileOperations::copyPathRecursively(tempDir.filePath("missing"), tempDir.filePath("dst"));

    // Assert
    EXPECT_FALSE(result);
}

TEST(NauFileOperationsTests, moveToTrashWatchedDirectoryReportsFailure)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString dirPath = tempDir.filePath("watched");
    ASSERT_TRUE(createFile(dirPath + "/sub/file.txt"));

    // Windows doesn't allow moving a watched directory, but Qt reports success anyway.
    QFileSystemWatcher watcher;
    watcher.addPaths({dirPath, dirPath + "/sub"});

    // Act
    const bool result = NauFileOperations::moveToTrash({dirPath});

    // Assert
    EXPECT_EQ(result, !QFileInfo::exists(dirPath));
}

TEST_F(NauFileOperationsAssetTests, assetsSharingSourceFindsOtherMetaFiles)
{
    // Arrange
    const QString firstMetaFilePath = tempDir.filePath("first.nausd");
    const QString secondMetaFilePath = tempDir.filePath("second.nausd");
    ASSERT_TRUE(createFile(tempDir.filePath("texture.png")));
    ASSERT_TRUE(createFile(tempDir.filePath("other.png")));
    ASSERT_TRUE(createMetaFile(firstMetaFilePath, "texture.png"));
    ASSERT_TRUE(createMetaFile(secondMetaFilePath, "texture.png"));
    ASSERT_TRUE(createMetaFile(tempDir.filePath("other.nausd"), "other.png"));

    // Act
    const QStringList result = NauFileOperations::assetsSharingSource(firstMetaFilePath);

    // Assert
    EXPECT_EQ(result, QStringList{secondMetaFilePath});
}

TEST_F(NauFileOperationsAssetTests, assetsSharingSourceSkipsIgnoredMetaFiles)
{
    // Arrange
    const QString firstMetaFilePath = tempDir.filePath("first.nausd");
    const QString secondMetaFilePath = tempDir.filePath("second.nausd");
    ASSERT_TRUE(createFile(tempDir.filePath("texture.png")));
    ASSERT_TRUE(createMetaFile(firstMetaFilePath, "texture.png"));
    ASSERT_TRUE(createMetaFile(secondMetaFilePath, "texture.png"));

    // Act
    const QStringList result = NauFileOperations::assetsSharingSource(firstMetaFilePath, {firstMetaFilePath, secondMetaFilePath});

    // Assert
    EXPECT_TRUE(result.isEmpty());
}

TEST(NauFileOperationsTests, moveToTrashRestoresItemsOnFailure)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString filePath = tempDir.filePath("file.txt");
    const QString dirPath = tempDir.filePath("folder");
    ASSERT_TRUE(createFile(filePath));
    ASSERT_TRUE(createFile(dirPath + "/sub/file.txt"));

    // Act
    const bool result = NauFileOperations::moveToTrash({filePath, dirPath, tempDir.filePath("missing")});

    // Assert
    EXPECT_FALSE(result);
    EXPECT_TRUE(QFileInfo::exists(filePath));
    EXPECT_TRUE(QFileInfo::exists(dirPath + "/sub/file.txt"));
}

TEST(NauFileOperationsTests, movePathsMovesFilesAndDirectories)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString filePath = tempDir.filePath("file.txt");
    const QString dirPath = tempDir.filePath("folder");
    const QString dstPath = tempDir.filePath("dst");
    ASSERT_TRUE(createFile(filePath));
    ASSERT_TRUE(createFile(dirPath + "/sub/file.txt"));
    ASSERT_TRUE(QDir().mkpath(dstPath));

    // Act
    const bool result = NauFileOperations::movePaths({filePath, dirPath}, dstPath);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(QFileInfo::exists(dstPath + "/file.txt"));
    EXPECT_TRUE(QFileInfo::exists(dstPath + "/folder/sub/file.txt"));
    EXPECT_FALSE(QFileInfo::exists(filePath));
    EXPECT_FALSE(QFileInfo::exists(dirPath));
}

TEST(NauFileOperationsTests, movePathsReturnsItemsBackOnFailure)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString firstPath = tempDir.filePath("first.txt");
    const QString secondPath = tempDir.filePath("second.txt");
    const QString dstPath = tempDir.filePath("dst");
    ASSERT_TRUE(createFile(firstPath));
    ASSERT_TRUE(createFile(secondPath));
    ASSERT_TRUE(createMetaFile(dstPath + "/second.txt", "occupied"));

    // Act
    const bool result = NauFileOperations::movePaths({firstPath, secondPath}, dstPath);

    // Assert
    EXPECT_FALSE(result);
    EXPECT_TRUE(QFileInfo::exists(firstPath));
    EXPECT_TRUE(QFileInfo::exists(secondPath));
    EXPECT_FALSE(QFileInfo::exists(dstPath + "/first.txt"));
    EXPECT_EQ(readFile(dstPath + "/second.txt"), QByteArray("occupied"));
}

TEST(NauFileOperationsTests, movePathsReturnsDirectoryBackOnFailure)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString dirPath = tempDir.filePath("folder");
    const QString dstPath = tempDir.filePath("dst");
    ASSERT_TRUE(createFile(dirPath + "/sub/moved.txt"));
    ASSERT_TRUE(createFile(dirPath + "/occupied.txt"));
    ASSERT_TRUE(createFile(dstPath + "/folder/occupied.txt"));

    // Act
    const bool result = NauFileOperations::movePaths({dirPath}, dstPath);

    // Assert
    EXPECT_FALSE(result);
    EXPECT_TRUE(QFileInfo::exists(dirPath + "/sub/moved.txt"));
    EXPECT_TRUE(QFileInfo::exists(dirPath + "/occupied.txt"));
    EXPECT_FALSE(QFileInfo::exists(dstPath + "/folder/sub"));
}

TEST(NauFileOperationsTests, movePathsToSameDirectoryChangesNothing)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString filePath = tempDir.filePath("file.txt");
    ASSERT_TRUE(createFile(filePath));

    // Act
    const bool result = NauFileOperations::movePaths({filePath}, tempDir.path());

    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(QFileInfo::exists(filePath));
}

TEST(NauFileOperationsTests, movePathsToOwnSubdirectoryFails)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString dirPath = tempDir.filePath("folder");
    ASSERT_TRUE(createFile(dirPath + "/sub/file.txt"));

    // Act
    const bool result = NauFileOperations::movePaths({dirPath}, dirPath + "/sub");

    // Assert
    EXPECT_FALSE(result);
    EXPECT_TRUE(QFileInfo::exists(dirPath + "/sub/file.txt"));
}

TEST_F(NauFileOperationsAssetTests, trashSelectionKeepsSourceUsedByUnselectedAsset)
{
    const QString source = tempDir.filePath("texture.png");
    const QString first = tempDir.filePath("texture.png.nausd");
    const QString second = tempDir.filePath("copy.png.nausd");
    ASSERT_TRUE(createFile(source));
    ASSERT_TRUE(createMetaFile(first, "texture.png\nfirst uid"));
    ASSERT_TRUE(createMetaFile(second, "texture.png\nsecond uid"));

    EXPECT_TRUE(NauFileOperations::moveAssetsToTrash({first}));
    EXPECT_FALSE(QFileInfo::exists(first));
    EXPECT_TRUE(QFileInfo::exists(source));
    EXPECT_EQ(readFile(second), QByteArray("texture.png\nsecond uid"));
}

TEST_F(NauFileOperationsAssetTests, trashSelectionRemovesSharedSourceOnce)
{
    const QString source = tempDir.filePath("texture.png");
    const QString first = tempDir.filePath("texture.png.nausd");
    const QString second = tempDir.filePath("copy.png.nausd");
    ASSERT_TRUE(createFile(source));
    ASSERT_TRUE(createMetaFile(first, "texture.png\nfirst uid"));
    ASSERT_TRUE(createMetaFile(second, "texture.png\nsecond uid"));

    EXPECT_TRUE(NauFileOperations::moveAssetsToTrash({first, second}));
    EXPECT_FALSE(QFileInfo::exists(source));
    EXPECT_FALSE(QFileInfo::exists(first));
    EXPECT_FALSE(QFileInfo::exists(second));
}

TEST_F(NauFileOperationsAssetTests, trashSelectionRestoresSharedSourceWhenLaterItemFails)
{
    const QString source = tempDir.filePath("texture.png");
    const QString first = tempDir.filePath("texture.png.nausd");
    const QString second = tempDir.filePath("copy.png.nausd");
    ASSERT_TRUE(createFile(source));
    ASSERT_TRUE(createMetaFile(first, "texture.png\nfirst uid"));
    ASSERT_TRUE(createMetaFile(second, "texture.png\nsecond uid"));

    // The last item disappeared after selection. Earlier deletions must be rolled back.
    ASSERT_TRUE(QFile::remove(second));
    EXPECT_FALSE(NauFileOperations::moveAssetsToTrash({first, second}));
    EXPECT_EQ(readFile(source), QByteArray("nau"));
    EXPECT_EQ(readFile(first), QByteArray("texture.png\nfirst uid"));
}

TEST_F(NauFileOperationsAssetTests, trashSelectionAllowsRemovingMetaWithMissingSource)
{
    const QString meta = tempDir.filePath("texture.png.nausd");
    ASSERT_TRUE(createMetaFile(meta, "texture.png\nold uid"));

    EXPECT_TRUE(NauFileOperations::moveAssetsToTrash({meta}));
    EXPECT_FALSE(QFileInfo::exists(meta));
}

#ifdef Q_OS_WIN
TEST_F(NauFileOperationsAssetTests, trashSelectionRestoresSharedSourceWhenSecondMetaIsLocked)
{
    const QString source = tempDir.filePath("texture.png");
    const QString first = tempDir.filePath("texture.png.nausd");
    const QString second = tempDir.filePath("copy.png.nausd");
    ASSERT_TRUE(createFile(source));
    ASSERT_TRUE(createMetaFile(first, "texture.png\nfirst uid"));
    ASSERT_TRUE(createMetaFile(second, "texture.png\nsecond uid"));

    const HANDLE lock = CreateFileW(second.toStdWString().c_str(), GENERIC_READ,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(lock, INVALID_HANDLE_VALUE);
    const bool result = NauFileOperations::moveAssetsToTrash({first, second});
    CloseHandle(lock);

    EXPECT_FALSE(result);
    EXPECT_EQ(readFile(source), QByteArray("nau"));
    EXPECT_EQ(readFile(first), QByteArray("texture.png\nfirst uid"));
    EXPECT_EQ(readFile(second), QByteArray("texture.png\nsecond uid"));
}
#endif
