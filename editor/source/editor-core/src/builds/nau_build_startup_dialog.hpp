// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Dialog that allows to configure and run the project build.

#pragma once

#include "nau_widget.hpp"
#include "nau_widget_utility.hpp"
#include "baseWidgets/nau_label.hpp"
#include "nau_buttons.hpp"
#include "project/nau_project.hpp"
#include "nau_log.hpp"


// ** NauBuildStartupDailog

class NauBuildStartupDailog : public NauDialog
{
    Q_OBJECT

    enum class BuildState
    {
        None,
        Building,
        Ready,
        Failed
    };

    enum class AfterBuildAction
    {
        None,
        OpenDirectory,
    };

public:
    NauBuildStartupDailog(const NauProject& project, NauMainWindow* parent);

private:
    void runBuild();
    void cancelBuild();
    void setBuildDirectory(const QString& directory);

    void fillSettings();

    void setBuildState(BuildState state);
    
    QString getDefaultBuildDirectory();

private:
    std::unique_ptr<NauProcess> m_buildProcess;
    std::shared_ptr<std::atomic_bool> m_cancelBuildFlag;
    NauDir m_buildDir;
    QString m_buildToolPath;
    BuildState m_currentBuildState;

    const NauProject& m_project;

    // Widgets
    NauComboBox* m_platforms;
    NauComboBox* m_configuration;
    NauComboBox* m_architecture;
    NauComboBox* m_compression;

    NauLabel* m_buildDirLabel;
    NauComboBox* m_postBuildAction;

    NauLabel* m_buildStatusLabel;
    NauProgressBar* m_buildProgressBar;

    NauPrimaryButton* m_buildButton;
    NauPrimaryButton* m_openBuildButton;

    inline static constexpr int OuterMargin = 16;
    inline static constexpr int SeparatorSize = 2;
    inline static constexpr int LayoutSpacing = 16;

    inline static const auto ColorSeparator = NauColor(153, 153, 153);
};