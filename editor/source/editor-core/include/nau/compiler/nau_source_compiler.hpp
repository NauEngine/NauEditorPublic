// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include <QApplication>
#include <QString>

class NauProject;

// ** NauSourceCompiler

class NauSourceCompiler
{
public:
    struct NauBuildSettings
    {
        QString configName;
        QString targetDir;
        QString preset;
        bool openAfterBuild;
    };

    virtual bool publishProject(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink) = 0;
    virtual bool compileProjectSource(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink) = 0;
    virtual bool checkBuildTool(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink) = 0;
    virtual bool checkCmakeInPath(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink) = 0;
    virtual bool buildProject(const NauBuildSettings& buildSettings, const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink, const std::function<bool()>& isCancelled = {}) = 0;
};


// ** NauWinDllCompilerCpp

class NauWinDllCompilerCpp : NauSourceCompiler
{
    Q_DECLARE_TR_FUNCTIONS(NauWinDllCompilerCpp)
public:
    bool publishProject(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink) override;
    bool compileProjectSource(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink) override;
    bool checkBuildTool(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink) override;
    bool checkCmakeInPath(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink) override;
    bool buildProject(const NauBuildSettings& buildSettings, const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink, const std::function<bool()>& isCancelled = {}) override;
};