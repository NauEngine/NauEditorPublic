// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "browser/nau_file_operations.hpp"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QTemporaryDir>


namespace
{
    bool createFile(const QString& path)
    {
        QDir().mkpath(QFileInfo{path}.absolutePath());

        QFile file{path};
        return file.open(QIODevice::WriteOnly) && file.write("nau") == 3;
    }
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

TEST(NauFileOperationsTests, assetSourcePathOfMetaFileWithSource)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString sourcePath = tempDir.filePath("texture.png");
    ASSERT_TRUE(createFile(sourcePath));
    ASSERT_TRUE(createFile(sourcePath + ".nausd"));

    // Act
    const QString& result = NauFileOperations::assetSourcePath(sourcePath + ".nausd");

    // Assert
    EXPECT_EQ(result, sourcePath);
}

TEST(NauFileOperationsTests, assetSourcePathOfMetaFileWithoutSource)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString metaPath = tempDir.filePath("material.nausd");
    ASSERT_TRUE(createFile(metaPath));

    // Act
    const QString& result = NauFileOperations::assetSourcePath(metaPath);

    // Assert
    EXPECT_TRUE(result.isEmpty());
}

TEST(NauFileOperationsTests, assetSourcePathOfNonMetaFile)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    const QString sourcePath = tempDir.filePath("texture.png");
    ASSERT_TRUE(createFile(sourcePath));

    // Act
    const QString& result = NauFileOperations::assetSourcePath(sourcePath);

    // Assert
    EXPECT_TRUE(result.isEmpty());
}

TEST(NauFileOperationsTests, assetSourcePathIgnoresDirectory)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    ASSERT_TRUE(QDir().mkpath(tempDir.filePath("folder")));
    ASSERT_TRUE(createFile(tempDir.filePath("folder.nausd")));

    // Act
    const QString& result = NauFileOperations::assetSourcePath(tempDir.filePath("folder.nausd"));

    // Assert
    EXPECT_TRUE(result.isEmpty());
}

TEST(NauFileOperationsTests, moveToTrashNonExistentPath)
{
    // Arrange
    const QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());

    // Act
    const bool result = NauFileOperations::moveToTrash(tempDir.filePath("missing"));

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
    const bool result = NauFileOperations::moveToTrash(dirPath);

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
    const bool result = NauFileOperations::moveToTrash(dirPath);

    // Assert
    EXPECT_EQ(result, !QFileInfo::exists(dirPath));
}
