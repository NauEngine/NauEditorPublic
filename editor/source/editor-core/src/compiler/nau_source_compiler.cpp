// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/compiler/nau_source_compiler.hpp"
#include "project/nau_project.hpp"
#include "nau_log.hpp"

#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QThread>

namespace {
    struct NauSourceCompilerConfig
    {
        QProcessEnvironment env;
        QString projectRootDir;
        QDir appDir;
        QFile logFile;
        QString configName;
        QString buildTool;
    };


    static void getConfig(const NauProject& project, NauSourceCompilerConfig& config, const QString& logFileName)
    {
        config.appDir = qApp->applicationDirPath();
        config.env = QProcessEnvironment::systemEnvironment();
        config.env.insert(QStringLiteral("NAU_ENGINE_SDK_DIR"), config.appDir.absoluteFilePath("NauEngineSDK"));
        config.projectRootDir = project.dir().absolutePath();
        config.logFile.setFileName(logFileName);
        config.configName =
#ifdef QT_NO_DEBUG
            QStringLiteral("Release");
#else
            QStringLiteral("Debug");
#endif  // QT_NO_DEBUG
        config.buildTool = project.buildToolPath();
    }

    static bool runJob(NauSourceCompilerConfig& config, QByteArrayList& log, const QStringList& args, const std::function<bool()>& isCancelled = {})
    {
        QProcess configureProcess;
        configureProcess.setWorkingDirectory(config.projectRootDir);
        configureProcess.setProcessEnvironment(config.env);

        config.logFile.write("Staring ");
        config.logFile.write(config.buildTool.toLocal8Bit());
        for (auto& arg : args) {
            config.logFile.write(" ");
            config.logFile.write(arg.toLocal8Bit());
        }
        config.logFile.write("\r\n");

        configureProcess.start(config.buildTool, args);

        if (!configureProcess.waitForStarted()) {
            config.logFile.write("Failed to start ");
            config.logFile.write(config.buildTool.toLocal8Bit());
            config.logFile.write("\r\n");
            return false;
        }

        while (configureProcess.state() == QProcess::Running) {
            if (isCancelled && isCancelled()) {
                config.logFile.write("Build cancelled by user\r\n");
                configureProcess.kill();
                configureProcess.waitForFinished(-1);
                break;
            }
            configureProcess.waitForFinished(50);
        }

        auto out = configureProcess.readAllStandardOutput();
        auto errors = configureProcess.readAllStandardError();
        config.logFile.write(out);
        config.logFile.write(errors);

        log.clear();
        log.append(out.split('\n'));
        log.append(errors.split('\n'));
        log.append(QStringLiteral("Build tool finished with %1").arg(configureProcess.exitCode()).toLocal8Bit());

        return configureProcess.exitStatus() == QProcess::NormalExit
            && configureProcess.exitCode() == 0;
    }

    struct FileWrite
    {
        FileWrite(QFile& file) : qfile(file)
        {
            qfile.open(QIODevice::WriteOnly);
            qfile.write("Log started: ");
            writeTime();
            qfile.write("\r\n");
        }
        ~FileWrite()
        {
            qfile.write("Log stopped: ");
            writeTime();
            qfile.write("\r\n");
            qfile.close();
        }
        void writeTime()
        {
            QDateTime date = QDateTime::currentDateTime();
            QString formattedTime = date.toString("dd.MM.yyyy hh:mm:ss");
            QByteArray formattedTimeMsg = formattedTime.toLocal8Bit();
            qfile.write(formattedTimeMsg);
        }
        QFile& qfile;
    };

    void parseLog(const QByteArrayList& logStringsRaw, std::vector<std::string>& logStrings, const std::vector<std::string>& keys)
    {
        logStrings.clear();
        for (auto& strRaw : logStringsRaw) {
            auto str = strRaw.toStdString();
            std::string lower(str);
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
            for (auto& key : keys) {
                if (lower.find(key, 0) != std::string::npos) {
                    std::erase(str, '\r');
                    logStrings.push_back(str);
                    break;
                }
            }
        }
    }
}

bool NauWinDllCompilerCpp::publishProject(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink)
{
    stageSink(tr("Publish project scripts"));

    NauSourceCompilerConfig config;
    getConfig(project, config, project.dir().absoluteFilePath("log_publish.txt"));
    FileWrite log(config.logFile);
    QByteArrayList logStringsRaw;
    if (runJob(config, logStringsRaw, {
        "build",
        "--project", config.projectRootDir,
        "--targetDir",config.projectRootDir,
        "--preset", "win_vs2022_x64_dll",
        "--config=" + config.configName
        })) {
        stageSink(tr("Publishing done successfully"));
    }
    else {
        stageSink(tr("Publishing FAILED"));
        parseLog(logStringsRaw, logStrings, { "failed","critical","fatal","error" });
        return false;
    }
    return true;
}

bool NauWinDllCompilerCpp::compileProjectSource(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink)
{
    stageSink(tr("Compiling project scripts"));
    
    NauSourceCompilerConfig config;
    getConfig(project, config, project.dir().absoluteFilePath("log_compile.txt"));

    FileWrite log(config.logFile);
    QByteArrayList logStringsRaw;
    if (runJob(config, logStringsRaw, {
        "compile",
        "--project", config.projectRootDir,
        "--preset", "win_vs2022_x64_dll",
        "--config=" + config.configName
        })) {
        stageSink(tr("Compiling done successfully"));
    } else {
        stageSink(tr("Compiling FAILED"));
        parseLog(logStringsRaw, logStrings, { "failed","critical","fatal","error"});
        return false;
    }
    return true;
}

bool NauWinDllCompilerCpp::checkBuildTool(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink)
{
    NauSourceCompilerConfig config;
    getConfig(project, config, project.dir().absoluteFilePath("log_chkbuildtool.txt"));
    FileWrite log(config.logFile);
    QByteArrayList logStringsRaw;

    if (runJob(config, logStringsRaw, { "--version" })) {
        stageSink(tr("BuildTool check success"));
        parseLog(logStringsRaw, logStrings, { "." });
    }
    else {
        stageSink(tr("BuildTool check FAILED"));
        return false;
    }
    return true;
}

bool NauWinDllCompilerCpp::checkCmakeInPath(const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink)
{
    NauSourceCompilerConfig config;
    getConfig(project, config, project.dir().absoluteFilePath("log_chkcmake.txt"));
    config.buildTool = "cmake";
    FileWrite log(config.logFile);
    QByteArrayList logStringsRaw;

    if (runJob(config, logStringsRaw, { "--version" })) {
        stageSink(tr("Cmake check success"));
        parseLog(logStringsRaw, logStrings, { "version" });
    }
    else {
        stageSink(tr("Cmake check FAILED"));
        return false;
    }
    return true;

}

bool NauWinDllCompilerCpp::buildProject(const NauSourceCompiler::NauBuildSettings& buildSettings, const NauProject& project, std::vector<std::string>& logStrings, std::function<void(const QString&)> stageSink, const std::function<bool()>& isCancelled)
{
    stageSink(tr("Building project scripts"));

    NauSourceCompilerConfig config;
    getConfig(project, config, project.dir().absoluteFilePath("log_buildproject.txt"));

    FileWrite log(config.logFile);
    QByteArrayList logStringsRaw;
    bool result = false;
    QStringList args = {
        "build",
        "--project", config.projectRootDir,
        "--targetDir", buildSettings.targetDir,
        "--preset", buildSettings.preset,
        "--config=" + buildSettings.configName,
        "--skipSourcesCompilation",
        "--skipAssetsCompilation",
        "--postBuildCopy"
    };

    if (buildSettings.openAfterBuild) {
        args.push_back("--openAfterBuild");
    }

    if (runJob(config, logStringsRaw, args, isCancelled)) {
        stageSink(tr("Publishing done successfully"));
        result = true;
    }
    else {
        stageSink(tr("Publishing FAILED"));
    }

    parseLog(logStringsRaw, logStrings, { "failed","critical","fatal","error" });
    return result;
}


