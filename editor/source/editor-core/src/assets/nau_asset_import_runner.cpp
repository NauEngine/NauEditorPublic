// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_asset_import_runner.hpp"

#include "nau_log.hpp"


// ** NauAssetImportRunner

NauAssetImportRunner::NauAssetImportRunner(const std::filesystem::path& projectPath, const std::filesystem::path& assetToolPath)
    : m_projectDir(projectPath)
    , m_assetToolPath(assetToolPath)
{

}

void NauAssetImportRunner::run(const std::optional<std::filesystem::path>& assetPath)
{
    if (m_importProcess && m_isRunning) {
        NED_WARNING("Import proccess is already running");
        return;
    }

    // TODO: Make a process container to be able to handle multiple import processes
    m_importProcess = std::unique_ptr<NauProcess>();

    QStringList args{
        "import",
        "--project", m_projectDir.string().c_str(),
    };

    if (assetPath) {
        args.push_back("--file");
        args.push_back(assetPath.value().string().c_str());
    }

    // Create asset tool process
    m_importProcess = std::make_unique<NauProcess>();
    m_importProcess->setProgram(m_assetToolPath.string().c_str());
    m_importProcess->setArguments(args);
    const QString assetToolWorkingDirectory = QFileInfo(m_assetToolPath).dir().absolutePath();
    m_importProcess->setWorkingDirectory(assetToolWorkingDirectory);

    // Subscribe to process events
    m_importProcess->connect(m_importProcess.get(), &NauProcess::started, [this] {
        NED_DEBUG("Import process started");
    });

    m_importProcess->connect(m_importProcess.get(), &NauProcess::readyReadStandardOutput, [this] {
        const QByteArray byteArr = m_importProcess->readAllStandardError();
        const QString message = QString::fromLocal8Bit(byteArr);
        if (!message.isNull() && !message.isEmpty()) {
            NED_INFO(message.toUtf8().constData());
        }
    });

    m_importProcess->connect(m_importProcess.get(), &NauProcess::readyReadStandardError, [this] {
        const QByteArray byteArr = m_importProcess->readAllStandardError();
        const QString error = QString::fromLocal8Bit(byteArr);
        if (!error.isNull() && !error.isEmpty()) {
            NED_ERROR(error.toUtf8().constData());
        }
    });

    m_importProcess->connect(m_importProcess.get(), &NauProcess::finished, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        const bool result = exitCode == 0 && (exitStatus == NauProcess::ExitStatus::NormalExit);

        if (result) {
            NED_INFO("Asset import process finished!");
        } else {
            NED_ERROR("Asset import process failed!");
        }
        
        m_isRunning = false;
    });

    // Execute process
    m_isRunning = true;
    m_importProcess->start();
}
