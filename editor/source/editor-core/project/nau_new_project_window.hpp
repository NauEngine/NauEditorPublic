// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Dialog for creating project

#pragma once

#include "nau_widget.hpp"
#include "nau_project.hpp"

#include <QObject>


// ** NauNewProjectWindow

class NAU_EDITOR_API NauNewProjectWindow : public NauDialog
{
    Q_OBJECT

public:
    NauNewProjectWindow(NauDialog* parent);

private:
    void update();
    void createProject();
    void fillTemplates();

signals:
    void eventRequestProjectCreation(const QString& path, const QString& name, const QString& templateName);

private:
    NauLineEdit* m_inputName;
    NauLineEdit* m_inputPath;
    NauComboBox* m_inputTemplateComboBox;
    QPushButton* m_buttonCreate;
};
