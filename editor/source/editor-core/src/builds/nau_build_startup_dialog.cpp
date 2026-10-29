// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_build_startup_dialog.hpp"
#include "nau/compiler/nau_source_compiler.hpp"
#include "themes/nau_theme.hpp"
#include "nau_plus_enum.hpp"

#include "QFileDialog"
#include <QDesktopServices>
#include <QCloseEvent>
#include <QThread>
#include <QUrl>
#include <utility>


// Build logger macro

#define NED_BUILD_INFO(...) NauLog::buildLogger().logMessage(NauEngineLogLevel::Info, __FUNCTION__, __FILE__, static_cast<unsigned>(__LINE__), __VA_ARGS__)
#define NED_BUILD_CRITICAL(...) NauLog::buildLogger().logMessage(NauEngineLogLevel::Critical, __FUNCTION__, __FILE__, static_cast<unsigned>(__LINE__), __VA_ARGS__)


// ** NauBuildStartupDailog

NauBuildStartupDailog::NauBuildStartupDailog(const NauProject& project, NauMainWindow* parent)
    : NauDialog(parent)
    , m_buildToolPath(project.buildToolPath())
    , m_project(project)
    , m_currentBuildState(BuildState::None)
{
    setWindowTitle(tr("Application build"));
    setMinimumSize(560, 480);

    auto mainLayout = new NauLayoutVertical(this);
    mainLayout->setContentsMargins(QMargins(OuterMargin, OuterMargin, OuterMargin, OuterMargin));
    mainLayout->setSpacing(LayoutSpacing);

    // Platforms
    auto platformLayout = new NauLayoutVertical();
    auto platformsTitle = new NauLabel(tr("Platforms"));
    m_platforms = new NauComboBox(this);
    platformLayout->addWidget(platformsTitle);
    platformLayout->addWidget(m_platforms);

    // Configurations
    auto configurationLayout = new NauLayoutVertical();
    auto configurationTitle = new NauLabel(tr("Configuration"));
    m_configuration = new NauComboBox(this);
    configurationLayout->addWidget(configurationTitle);
    configurationLayout->addWidget(m_configuration);

    // Architecture
    auto architectureLayout = new NauLayoutVertical();
    auto architectureTitle = new NauLabel(tr("Architecture"));
    m_architecture = new NauComboBox(this);
    architectureLayout->addWidget(architectureTitle);
    architectureLayout->addWidget(m_architecture);

    // Compression
    auto compressionLayout = new NauLayoutVertical();
    auto compressionTitle = new NauLabel(tr("Compression"));
    m_compression = new NauComboBox(this);
    compressionLayout->addWidget(compressionTitle);
    compressionLayout->addWidget(m_compression);

    // Build directory
    auto buildDirLayout = new NauLayoutVertical;
    buildDirLayout->addWidget(new NauLabel(tr("Build Directory")));

    auto buildDirChooseLayout = new NauLayoutHorizontal;
    m_buildDirLabel = new NauLabel(QString());
    m_buildDirLabel->setWordWrap(true);

    auto chooseDirButton = new NauToolButton();
    const auto buildDirectory = getDefaultBuildDirectory();
    chooseDirButton->setText(buildDirectory);
    setBuildDirectory(buildDirectory);
    connect(chooseDirButton, &NauToolButton::clicked, [this](){
        const QString selectedDir = QFileDialog::getExistingDirectory(this,
            tr("Select directory for your build"), m_buildDir.absolutePath());

        if (!selectedDir.isEmpty() && NauDir().exists(selectedDir)) {
            setBuildDirectory(selectedDir);
        }
    });

    buildDirChooseLayout->addWidget(m_buildDirLabel);
    buildDirChooseLayout->addWidget(chooseDirButton);

    buildDirLayout->addLayout(buildDirChooseLayout);
    // Build settings layout
    auto platformAndConfigLayout = new NauLayoutHorizontal();
    platformAndConfigLayout->addLayout(platformLayout);
    platformAndConfigLayout->addLayout(configurationLayout);
    platformAndConfigLayout->setSpacing(LayoutSpacing);

    auto architectureAndCompressionLayout = new NauLayoutHorizontal();
    architectureAndCompressionLayout->addLayout(architectureLayout);
    architectureAndCompressionLayout->addLayout(compressionLayout);
    architectureAndCompressionLayout->setSpacing(LayoutSpacing);

    // Post build action
    auto postBuildActionLayout = new NauLayoutVertical();
    auto postBuildActionTitle = new NauLabel(tr("After-Build Action"));
    m_postBuildAction = new NauComboBox(this);
    postBuildActionLayout->addWidget(postBuildActionTitle);
    postBuildActionLayout->addWidget(m_postBuildAction);

    // Build buttons
    m_buildButton = new NauPrimaryButton();
    m_buildButton->setText(tr("Build App"));
    m_buildButton->setIcon(Nau::Theme::current().iconPreferences());
    m_buildButton->setFixedHeight(NauAbstractButton::standardHeight());
    m_buildButton->setFixedWidth(160);
    connect(m_buildButton, &NauAbstractButton::clicked, this, &NauBuildStartupDailog::runBuild);

    m_openBuildButton = new NauPrimaryButton();
    m_openBuildButton->setText(tr("Open build folder"));
    m_openBuildButton->setFixedHeight(NauAbstractButton::standardHeight());
    m_openBuildButton->setFixedWidth(160);
    m_openBuildButton->setVisible(true);
    m_openBuildButton->setEnabled(false);
    connect(m_openBuildButton, &NauAbstractButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_buildDir.absolutePath()));
        });

    m_buildStatusLabel = new NauLabel();
    m_buildStatusLabel->setMinimumWidth(180);

    m_buildProgressBar = new NauProgressBar(this);
    m_buildProgressBar->setRange(0, 0);
    m_buildProgressBar->setTextVisible(false);
    m_buildProgressBar->setFixedHeight(8);
    m_buildProgressBar->hide();

    // Bottom buttons row: [status] <stretch> [open] [gap] [build]
    auto buildButtonsLayout = new NauLayoutHorizontal();
    buildButtonsLayout->setSpacing(0);
    buildButtonsLayout->addWidget(m_buildStatusLabel, Qt::AlignLeft | Qt::AlignVCenter);
    buildButtonsLayout->addStretch(1);
    buildButtonsLayout->addWidget(m_openBuildButton, Qt::AlignRight);
    buildButtonsLayout->addSpacing(LayoutSpacing);
    buildButtonsLayout->addWidget(m_buildButton, Qt::AlignRight);

    // Fill main layout
    auto separator = new NauLineWidget(ColorSeparator, SeparatorSize, Qt::Horizontal, this);
    separator->setFixedWidth(width());
    separator->setFixedHeight(SeparatorSize);

    mainLayout->addLayout(platformAndConfigLayout);
    mainLayout->addLayout(architectureAndCompressionLayout);
    mainLayout->addWidget(separator);
    mainLayout->addLayout(buildDirLayout);
    mainLayout->addLayout(postBuildActionLayout);
    mainLayout->addStretch(1);
    mainLayout->addWidget(m_buildProgressBar);
    mainLayout->addLayout(buildButtonsLayout);

    fillSettings();

    if (!m_project.isSourcesCompiled()) {
        m_buildStatusLabel->setText(tr("<img src=\":/UI/icons/compilation/warning-tri-yellow.svg\">Unable to publish this project for its sources out of a date.<br/>"
            "To recompile the sources, please restart the editor"));

        m_buildButton->setEnabled(false);
        m_buildButton->setToolTip(m_buildStatusLabel->text());
    }
}

NauBuildStartupDailog::~NauBuildStartupDailog()
{
    // The worker owns only snapshots and can finish without GUI event delivery.
    if (m_buildThread) {
        m_cancelBuildFlag->store(true);
        m_buildThread->wait();
        delete m_buildThread;
    }
}

void NauBuildStartupDailog::done(int result)
{
    if (m_buildThread) {
        m_closeResult = result;
        cancelBuild();
        return;
    }

    NauDialog::done(result);
}

void NauBuildStartupDailog::closeEvent(QCloseEvent* event)
{
    if (m_buildThread) {
        done(QDialog::Rejected);
        event->ignore();
        return;
    }

    NauDialog::closeEvent(event);
}

void NauBuildStartupDailog::setBuildState(BuildState state)
{
    m_currentBuildState = state;

    const bool building = state == BuildState::Building;
    const bool ready = state == BuildState::Ready;

    m_buildButton->setEnabled(true);
    m_buildButton->setText(building ? tr("Cancel") : tr("Build App"));
    m_buildButton->setIcon(building
        ? Nau::Theme::current().iconClose()
        : Nau::Theme::current().iconPreferences());

    m_buildProgressBar->setVisible(building);

    m_openBuildButton->setEnabled(ready);

    switch (state) {
    case BuildState::Building:
        m_buildStatusLabel->setText(tr("Building..."));
        break;

    case BuildState::Ready:
        m_buildStatusLabel->setText(tr("Build finished successfully"));
        break;

    case BuildState::Failed:
        m_buildStatusLabel->setText(tr("Build failed. See Console for details"));
        break;

    case BuildState::None:
    default:
        m_buildStatusLabel->clear();
        break;
    }
}

QString NauBuildStartupDailog::getDefaultBuildDirectory()
{
    return m_project.path().root().absoluteFilePath("publish");
}

void NauBuildStartupDailog::fillSettings()
{
    // TODO: Get build settings from engine

    m_platforms->addItems({ "Windows desktop" });
    m_architecture->addItems({ "win_vs2022_x64_dll", "win_vs2022_x64" });
#ifdef QT_NO_DEBUG
    m_configuration->addItems({ "Release" , "Debug" });
#else
    m_configuration->addItems({ "Debug", "Release" });
#endif  // QT_NO_DEBUG
    m_compression->addItems({ "None" });

    m_postBuildAction->addItem(tr("None"), +AfterBuildAction::None);
    m_postBuildAction->addItem(tr("Open After Build"), +AfterBuildAction::OpenDirectory);
    m_postBuildAction->setCurrentIndex(m_postBuildAction->findData(+AfterBuildAction::None));
}

void NauBuildStartupDailog::runBuild()
{
    if (m_currentBuildState == BuildState::Building) {
        cancelBuild();
        return;
    }

    NauSourceCompiler::NauBuildSettings settings;
    settings.configName = m_configuration->currentText();
    settings.preset = m_architecture->currentText();
    settings.targetDir = NauDir::toNativeSeparators(m_buildDir.absolutePath());
    settings.openAfterBuild =
        m_postBuildAction->currentData().toInt() == +AfterBuildAction::OpenDirectory;

    auto cancelFlag = std::make_shared<std::atomic_bool>(false);
    m_cancelBuildFlag = cancelFlag;

    setBuildState(BuildState::Building);

    const QString projectRootDir = m_project.dir().absolutePath();
    const QString buildTool = m_buildToolPath;
    struct BuildResult
    {
        bool success = false;
        std::vector<std::string> logStrings;
    };
    auto result = std::make_shared<BuildResult>();

    m_buildThread = QThread::create([settings, projectRootDir, buildTool, cancelFlag, result] {
        NauWinDllCompilerCpp compiler;
        result->success = compiler.buildProject(
            settings, projectRootDir, buildTool, result->logStrings,
            [](const QString&) {},
            [cancelFlag] { return cancelFlag->load(); });
    });
    m_buildThread->setParent(this);

    connect(m_buildThread, &QThread::finished, this, [this, result, cancelFlag] {
        // finished may precede thread-local cleanup; join before destruction.
        m_buildThread->wait();
        delete std::exchange(m_buildThread, nullptr);
        m_cancelBuildFlag.reset();

        if (m_closeResult) {
            const int closeResult = *m_closeResult;
            m_closeResult.reset();
            NauDialog::done(closeResult);
            return;
        }

        if (cancelFlag->load()) {
            setBuildState(BuildState::None);
            m_buildStatusLabel->setText(tr("Build cancelled"));
            return;
        }
        for (const auto& str : result->logStrings) {
            NED_BUILD_CRITICAL(str);
        }
        if (result->success) {
            NED_BUILD_INFO("Build project success");
            setBuildState(BuildState::Ready);
        } else {
            NED_BUILD_CRITICAL("Build project failed!");
            setBuildState(BuildState::Failed);
        }
    }, Qt::QueuedConnection);
    m_buildThread->start();
}

void NauBuildStartupDailog::cancelBuild()
{
    if (m_currentBuildState != BuildState::Building || !m_cancelBuildFlag) {
        return;
    }

    m_cancelBuildFlag->store(true);
    m_buildButton->setEnabled(false);
    m_buildStatusLabel->setText(tr("Cancelling build..."));
}

void NauBuildStartupDailog::setBuildDirectory(const QString& directory)
{
    m_buildDirLabel->setText(directory);
    m_buildDir = directory;

    if (!m_buildDir.exists()) {
        m_buildDir.mkpath(".");
    }
}
