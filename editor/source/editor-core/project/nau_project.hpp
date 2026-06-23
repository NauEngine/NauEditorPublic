// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Contains all information about project

#pragma once

#include "nau_editor_version.hpp"
#include "nau_project_user_settings.hpp"
#include "nau/scene/nau_scene_manager.hpp"
#include "fileAccessor/nau_file_accessor.hpp"

#include <QObject>


// ** NauProjectInfo
//
// Minimal project info used for the project manager

struct NauProjectInfo
{
    NauProjectPath                   path;
    std::optional<NauEditorVersion>  version;
    std::optional<QString>           name;
};


// ** NauProject

class NauProject;
using NauProjectPtr = std::shared_ptr<NauProject>;

class NAU_EDITOR_API NauProject : public QObject
{
    Q_OBJECT

public:
    static NauProjectPtr create(const QString& location, const QString& name, const QString& templateName = "empty");
    static NauProjectPtr load(const NauProjectPath& path);
    static NauProjectInfo info(const NauProjectPath& path);
    
    bool upgradeToCurrentEditorVersion();

    NauProjectPath path() const { return m_path; };
    NauDir dir() const;
    bool isDirty() const { return m_dirty || m_scenes.isDirty(); }

    bool isSourcesDirty() const { return m_sourcesDirty; }
    bool isSourcesCompiled() const;
    void markSourcesDirty();
    void markSourcesClean();
    bool isDllsExists() const;
    bool isDllsOutdated() const;
    void removeDlls() const;
    QString dllPath() const;
    QString compilationLogFileName() const;

    bool isValid() const { return m_path.isValid(); }
    bool save();
    void saveProjectThumbnail(HWND handle);

    void setDisplayName(const QString& name);
    QString displayName() const { return m_displayName; };
    NauEditorVersion version() const { return m_version; };
    QStringList modules() const;

    struct DLLInfo
    {
        QString m_module;
        QString m_dllPath;
        QDateTime m_dllDateTime;
        quint64 m_dllSize;
        bool m_exists;
    };

    void getDLLInfo(std::vector<DLLInfo>& dllInfo);

    NauSceneManager& scenes() { return m_scenes; }
    const NauSceneManager& scenes() const { return m_scenes; }
    std::string currentScenePath() const;

    const QString defaultScriptsFolder() const { return m_path.root().absolutePath() + m_defaultScriptsDir; }
    const QString defaultContentFolder() const { return m_path.root().absolutePath() + "/content/"; }
    const QString defaultSourceFolder() const { return m_path.root().absolutePath() + "/source/"; }
    const QString defaultSourceDirtyMarkerName() const { return defaultSourceFolder() + ".naubuild-dirty"; }
    const QString defaultSourceCleanMarkerName() const { return defaultSourceFolder() + ".naubuild-clean"; }

    // TODO: Store this paths in config
    const QString buildToolPath() const { return QApplication::applicationDirPath() + "/BuildToolCmd.exe"; }
    const QString assetToolPath() const { return QApplication::applicationDirPath() + "/AssetToolCmd.exe"; }

    const QString assetTemplatesFolder() const { return m_path.root().absolutePath() + "/templates/"; }

    NauProjectUserSettings& userSettings() { return m_userSettings; }
    const NauProjectUserSettings& userSettings() const { return m_userSettings; }

    ~NauProject();

private:
    NauProject(const NauProjectPath& path);

    static QStringList scanModules(const QString& inDir);
    static QDateTime scanSources(const QString& inDir);

private:
    const NauProjectPath m_path;
    bool m_dirty = false;
    bool m_sourcesDirty = false;

    QString           m_displayName;
    NauEditorVersion  m_version;
    NauSceneManager   m_scenes;
    QJsonObject       m_configJson;
    QStringList       m_modules;
    QStringList       m_userDlls;
#ifdef QT_NO_DEBUG
    inline static const QString m_configuration = "Release";
#else
    inline static const QString m_configuration = "Debug";
#endif  // QT_NO_DEBUG
    inline static const QString m_defaultBinDir = "/bin/";

    NauProjectUserSettings m_userSettings;

    inline static const QString m_defaultTemplatesDir = "/templates/";

    const QString m_defaultScriptsDir = "/scripts/";
};


// ** NauSceneFileAccessor

class NAU_EDITOR_API NauSceneFileAccessor : public NauFileAccessorInterface
{
public:
    NauSceneFileAccessor(NauProjectPtr project);

    bool init() override { return true; }
    bool openFile(const QString& path) override;

private:
    std::weak_ptr<NauProject> m_project;
};
