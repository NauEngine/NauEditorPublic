// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_assert.hpp"
#include "nau_editor_version.hpp"
#include "nau_log.hpp"
#include "nau_project.hpp"
#include "nau_project_path.hpp"

#include "nau/project_tools/project_api.h"

#include <QApplication>
#include <QJSonDocument>
#include <QJSonObject>
#include <QDir>
#include <QDirIterator>
#include <QScreen>
#include <QGuiApplication>
#include <QRegularExpression>


// ** NauProject

NauProject::NauProject(const NauProjectPath& path)
    : m_path(path)
    , m_scenes(path.root())
    , m_version(NauEditorVersion::invalid())
    , m_userSettings(path, m_scenes)
{
    NED_DEBUG("Project at {} initialized", static_cast<std::string>(path));
}

NauProject::~NauProject()
{
    NED_DEBUG("Closing project: {}", m_displayName);
}

NauProjectPtr NauProject::create(const QString& location, const QString& name, const QString& templateName)
{
    const auto path = NauProjectPath::create(location, name);
    NED_DEBUG("Creating new project at {}", static_cast<std::string>(path));

    auto args = std::make_unique<nau::InitProjectArguments>();
    QDir projectParentDir = path.root();
    projectParentDir.cdUp();
    args->projectPath = projectParentDir.path().toStdString();
    args->toolsPath = qApp->applicationDirPath().toStdString();
    args->templateName = templateName.toStdString();
    args->projectName = name.toStdString();
    args->contentOnly = false;
    args->generateSolutionFile = false;
    args->openIde = false;
    args->cMakePreset = "";

    const bool projectInited = nau::initProject(args.get()) == 0;
    if (!(projectInited && path.isValid())) {
        NED_ERROR("Project creation at {} failed", static_cast<std::string>(path));
        return nullptr;
    }

    // Base properties
    auto project = NauProjectPtr(new NauProject(path));
    project->setDisplayName(name);
    project->m_version = NauEditorVersion::current();
    project->m_scenes.createScene("main");  // Initial main scene

    NauFile projectFile(path);
    projectFile.open(QIODevice::ReadOnly);
    project->m_configJson = QJsonDocument::fromJson(projectFile.readAll()).object();
    project->markSourcesDirty();

    // Save
    const bool saved = project->save();
    NED_ASSERT(saved);

    return project;
}

bool NauProject::upgradeToCurrentEditorVersion()
{
    const NauEditorVersion currentEditorVersion = NauEditorVersion::current();

    NED_DEBUG("Upgrading project {} up to version {}", m_displayName.toUtf8().constData(), currentEditorVersion.asString());

    m_version = currentEditorVersion;
    return save();
}

NauProjectPtr NauProject::load(const NauProjectPath& path)
{
    NED_DEBUG("Loading project at {}", static_cast<std::string>(path));

    auto project = NauProjectPtr(new NauProject(path));
    
    // Load project file
    NauFile projectFile(path);
    if (!projectFile.open(QIODevice::ReadOnly)) {
        NED_ERROR("Failed to load project at {}! Returning empty project.", static_cast<std::string>(path));
        return project;
    }

    // Load project properties
    QJsonObject projectData = QJsonDocument::fromJson(projectFile.readAll()).object();
    if (!NED_PASSTHROUGH_ASSERT(!projectData.isEmpty())) {
        NED_ERROR("Corrupted project (invalid JSON) at {}! Returning empty project.", static_cast<std::string>(path));
        return project;
    }
    project->m_configJson = projectData;
    project->m_displayName = projectData["ProjectName"].toString();  // Setting directly so we don't mark the project dirty
    project->m_version = projectData.contains("EditorVersion")
        ? NauEditorVersion::fromText(projectData["EditorVersion"].toString())
        : NauEditorVersion::invalid();

    // User settings
    if (!project->m_userSettings.load()) {
        NED_WARNING("Failed to load user settings");
    }

    project->m_sourcesDirty = QFile::exists(project->defaultSourceDirtyMarkerName());
    project->m_modules = scanModules(QDir(project->defaultSourceFolder()).absoluteFilePath("game"));//{"MainGameModule"};

    // Scenes
    project->m_scenes.load(projectData);

    NED_DEBUG("Project loaded successfully.");
    return project;
}

NauProjectInfo NauProject::info(const NauProjectPath& path)
{
    // Load project file
    NauFile projectFile(path);
    if (!projectFile.open(QIODevice::ReadOnly)) {
        NED_ERROR("Failed to load project at {}! Returning empty project.", static_cast<std::string>(path));
        return { .path = NauProjectPath::invalid() };
    }

    // Load project properties
    QJsonObject projectData = QJsonDocument::fromJson(projectFile.readAll()).object();
    if (!NED_PASSTHROUGH_ASSERT(!projectData.isEmpty())) {
        NED_ERROR("Corrupted project (invalid JSON) at {}! Returning empty project.", static_cast<std::string>(path));
        return { .path = NauProjectPath::invalid() };
    }

    auto version = projectData.contains("EditorVersion")
        ? NauEditorVersion::fromText(projectData["EditorVersion"].toString())
        : NauEditorVersion::invalid();

    return {
        .path = path,
        .version = version,
        .name = projectData["ProjectName"].toString()
    };
}

bool NauProject::save()
{
    // Prepare data
    QJsonObject root = m_configJson;
    root["ProjectName"] = m_displayName;;
    root["EditorVersion"] = m_version.asQtString();

    // Scenes
    m_scenes.save(root);

    // User settings
    if (!m_userSettings.save()) {
        NED_WARNING("Failed to save user settings");
    }

    // Save
    NauFile result(m_path);
    if (!result.open(QIODevice::WriteOnly)) {
        NED_ERROR("Failed to open project file for saving at {}", static_cast<std::string>(m_path));
        return false;
    }

    result.write(QJsonDocument(root).toJson());
    result.close();
    m_dirty = false;

    auto args = std::make_unique<nau::SaveProjectArguments>();
    args->projectPath = path().root().path().toStdString();
    args->toolsPath = qApp->applicationDirPath().toStdString();
    args->config = QJsonDocument(root).toJson().toStdString();
    args->dontUpgrade = true;
    args->projectName = m_displayName.toStdString();

    NED_ASSERT(nau::saveProject(args.get()) == 0);

    NED_DEBUG("Project saved successfully.");
    return true;
}

void NauProject::saveProjectThumbnail(HWND handle)
{
    auto pixmap = QGuiApplication::primaryScreen()->grabWindow(WId(handle));
    QFile file(m_path.root().path() + "/.editor" + "/project_thumbnail.png");
    file.open(QIODevice::WriteOnly);
    pixmap.save(&file, "PNG");
}


void NauProject::setDisplayName(const QString& name)
{
    m_displayName = name;
    m_dirty = true;
}

QStringList NauProject::modules() const
{
    return m_modules;
}

void NauProject::getDLLInfo(std::vector<NauProject::DLLInfo>& dllInfo)
{
    dllInfo.clear();
    dllInfo.reserve(m_modules.size());
    for (auto& module : m_modules) {
        NauProject::DLLInfo info;
        info.m_module = module;
        info.m_dllPath = dllPath() + module + ".dll";
        info.m_exists = QFile::exists(info.m_dllPath);
        if (info.m_exists) {
            info.m_dllDateTime = QFileInfo(QFile(info.m_dllPath)).lastModified();
            info.m_dllSize = QFileInfo(QFile(info.m_dllPath)).size();
        }
        dllInfo.push_back(info);
    }
}

std::string NauProject::currentScenePath() const
{
    return m_scenes.currentSceneAbsolutePath().toUtf8().data();
}

NauDir NauProject::dir() const
{
    return NauDir(QFileInfo(m_path).absolutePath());
}

bool NauProject::isSourcesCompiled() const
{
    return QFile::exists(defaultSourceCleanMarkerName());
}

QString NauProject::dllPath() const
{
    return m_path.root().absolutePath() + m_defaultBinDir + m_configuration + QDir::separator();
}

QString NauProject::compilationLogFileName() const
{
    return dir().absoluteFilePath("log_compile.txt");
}

bool NauProject::isDllsExists() const
{
    for (auto& module : m_modules) {
        auto fullPath = dllPath() + module + ".dll";
        if (!QFile::exists(fullPath)) {
            return false;
        }
    }
    return true;
}

bool NauProject::isDllsOutdated() const
{
    auto sourcesStamp = scanSources(defaultSourceFolder());
    for (auto& module : m_modules) {
        auto fullPath = dllPath() + module + ".dll";
        if (QFile::exists(fullPath)) {
            QDateTime dllStamp = QFileInfo(QFile(fullPath)).lastModified();
            if (sourcesStamp > dllStamp) {
                auto source = sourcesStamp.toString();
                auto dll = dllStamp.toString();
                return true;
            }
        }
    }
    return false;
}

void NauProject::removeDlls() const
{
    for (auto& module : m_modules) {
        auto fullPath = dllPath() + module + ".dll";
        if (QFile::exists(fullPath)) {
            QFile::remove(fullPath);
        }
    }
}

void NauProject::markSourcesDirty()
{
    QFile::remove(defaultSourceCleanMarkerName());

    QFile file(defaultSourceDirtyMarkerName());
    if (!file.open(QFile::WriteOnly)) {
        NED_ERROR("Failed to mark sources of project dirty:{}", file.errorString());
    }
}

void NauProject::markSourcesClean()
{
    QFile::remove(defaultSourceDirtyMarkerName());
    
    QFile file(defaultSourceCleanMarkerName());
    if (!file.open(QFile::WriteOnly)) {
        NED_ERROR("Failed to mark sources of project clean:{}", file.errorString());
    }
}

QStringList NauProject::scanModules(const QString& inDir)
{
    QDirIterator fsIt(inDir, {"CMakeLists.txt"}, QDir::Files, QDirIterator::Subdirectories);
    const QRegularExpression rx{"TargetName\\s+(\\w+)"};

    QStringList result;
    while (fsIt.hasNext()) {
        QFile cmakeLists{fsIt.next()};
        if (cmakeLists.open(QIODevice::ReadOnly)) {
            QRegularExpressionMatch match = rx.match(cmakeLists.readAll());
            if (match.hasMatch() && match.hasCaptured(1)) {
                result << match.captured(1);
            }
        }
    }

    return result;
}

QDateTime NauProject::scanSources(const QString& inDir)
{
    QDateTime timestamp;
    QDirIterator fsIt(inDir, { "*.*" }, QDir::Files, QDirIterator::Subdirectories);
    while (fsIt.hasNext()) {
        QFile file{ fsIt.next() };
        QFileInfo fileInfo(file);
        if (fileInfo.fileName().startsWith(".")) {
            // Skip auxiliary files
            continue;
        }
        QDateTime last = fileInfo.lastModified();
        if (last > timestamp) {
            timestamp = last;
        }
    }
    return timestamp;
}


// ** NauSceneFileAccessor

NauSceneFileAccessor::NauSceneFileAccessor(NauProjectPtr project)
    : m_project(project)
{
}

bool NauSceneFileAccessor::openFile(const QString& path)
{
    auto& sceneManager = m_project.lock()->scenes();
    const auto sceneInfo = sceneManager.absolutePathToScene(path);
    if (sceneInfo != sceneManager.currentScene()) {
        sceneManager.switchScene(sceneInfo);
    }
    return true;
}
