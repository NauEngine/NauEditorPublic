// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/compiler/nau_source_compiler.hpp"
#include "project/nau_project.hpp"
#include "nau_log.hpp"

#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {
#ifdef Q_OS_WIN
    // Assign the job during CreateProcess, before the child can spawn descendants.
    class BuildProcessJob
    {
    public:
        BuildProcessJob() = default;
        BuildProcessJob(const BuildProcessJob&) = delete;
        BuildProcessJob& operator=(const BuildProcessJob&) = delete;
        ~BuildProcessJob()
        {
            if (m_attributes) DeleteProcThreadAttributeList(m_attributes);
            if (m_job) CloseHandle(m_job);
        }

        bool initialize()
        {
            m_job = CreateJobObjectW(nullptr, nullptr);
            if (!m_job) return false;
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) return false;
            SIZE_T size = 0;
            InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
            m_storage.resize(size);
            auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(m_storage.data());
            if (!InitializeProcThreadAttributeList(attributes, 1, 0, &size)) return false;
            m_attributes = attributes;
            return UpdateProcThreadAttribute(m_attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST,
                &m_job, sizeof(m_job), nullptr, nullptr) != FALSE;
        }

        void configure(QProcess& process)
        {
            process.setCreateProcessArgumentsModifier([this](QProcess::CreateProcessArguments* args) {
                m_startup.StartupInfo = *args->startupInfo;
                m_startup.StartupInfo.cb = sizeof(m_startup);
                m_startup.lpAttributeList = m_attributes;
                args->startupInfo = &m_startup.StartupInfo;
                args->flags |= EXTENDED_STARTUPINFO_PRESENT;
            });
        }

        bool terminate()
        {
            if (!TerminateJobObject(m_job, 1)) return false;
            for (;;) {
                JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info{};
                if (!QueryInformationJobObject(m_job, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr)) return false;
                if (info.ActiveProcesses == 0) return true;
                Sleep(10);
            }
        }

    private:
        HANDLE m_job = nullptr;
        std::vector<unsigned char> m_storage;
        LPPROC_THREAD_ATTRIBUTE_LIST m_attributes = nullptr;
        STARTUPINFOEXW m_startup{};
    };
#endif

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
#ifdef Q_OS_WIN
        BuildProcessJob job;
        if (isCancelled && !job.initialize()) {
            log.append("Failed to create build process job: " + QByteArray::number(GetLastError()));
            config.logFile.write(log.back() + "\r\n");
            return false;
        }
#endif
        QProcess configureProcess;
#ifdef Q_OS_WIN
        if (isCancelled) job.configure(configureProcess);
#endif
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
#ifdef Q_OS_WIN
                if (!job.terminate()) {
                    config.logFile.write("Failed to terminate build process job: " + QByteArray::number(GetLastError()) + "\r\n");
                    return false;
                }
#else
                configureProcess.kill();
#endif
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


