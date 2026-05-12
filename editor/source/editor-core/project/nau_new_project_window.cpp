// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_assert.hpp"
#include "nau_label.hpp"
#include "nau_new_project_window.hpp"
#include "nau_settings.hpp"
#include "nau_widget_utility.hpp"
#include "nau/app/nau_qt_app.hpp"

#include <QFileDialog>
#include <QMessageBox>


// ** NauNewProjectWindow

NauNewProjectWindow::NauNewProjectWindow(NauDialog* parent)
    : NauDialog(parent)
    , m_inputName(new NauLineEdit(this))
    , m_inputPath(new NauLineEdit(this))
    , m_buttonCreate(new QPushButton(tr("Create"), this))
    , m_inputTemplateComboBox(new NauComboBox(this))
{
    setWindowTitle(tr("Create New Project"));
    setFixedSize(400, 220);
    setStyleSheet(QString("background-color: %1").arg(NauColor(19, 21, 22).hex()));

    // Project name
    auto layout = new NauLayoutVertical(this);
    layout->addWidget(new NauLabel(tr("Project Name:")));

    QRegularExpression rx("^[A-Za-z0-9_-]{1,80}$");
    m_inputName->setValidator(new QRegularExpressionValidator(rx, this));
    layout->addWidget(m_inputName);

    // Project path
    auto labelPath = new NauLabel(tr("Project Path:"));
    layout->addWidget(labelPath);

    auto widgetPath = new NauWidget(this);
    auto layoutPath = new NauLayoutHorizontal(widgetPath);
    layout->addWidget(widgetPath);
    
    m_inputPath->setText(NauSettings::recentProjectDirectory().absolutePath());
    m_inputPath->setReadOnly(true);
    layoutPath->addWidget(m_inputPath);

    auto buttonPath = new QPushButton(tr("Browse..."));
    layoutPath->addWidget(buttonPath);

    // Change path
    connect(buttonPath, &QPushButton::clicked, [this] {
        const QString path = QFileDialog::getExistingDirectory(this, tr("Select path"), m_inputPath->text());
        if (!path.isEmpty()) {
            m_inputPath->setText(path);
        }
    });

    // Templates
    fillTemplates();
    auto labelTemplate = new NauLabel(tr("Template:"));
    layout->addWidget(labelTemplate);
    layout->addWidget(m_inputTemplateComboBox);

    // Create project
    m_buttonCreate->setEnabled(false);
    layout->addWidget(m_buttonCreate);
    connect(m_buttonCreate, &QPushButton::clicked, this, &NauNewProjectWindow::createProject);

    // Enable create button
    connect(m_inputName, &NauLineEdit::textChanged, this, &NauNewProjectWindow::update);
    connect(m_inputPath, &NauLineEdit::textChanged, this, &NauNewProjectWindow::update);
}

void NauNewProjectWindow::update()
{
    m_buttonCreate->setEnabled(!m_inputPath->text().isEmpty() && !m_inputName->text().isEmpty());
    NauDialog::update();
}

void NauNewProjectWindow::createProject()
{
    setEnabled(false);
    const auto name = m_inputName->text();
    const auto path = m_inputPath->text();
    const auto templateName = m_inputTemplateComboBox->currentText();
    NED_ASSERT(!name.isEmpty());
    NED_ASSERT(!path.isEmpty() && NauDir(path).exists());
    NED_ASSERT(!templateName.isEmpty());

    // Check if already exists
    if (NauProjectPath::exists(path, name))
    {
        QMessageBox::warning(this, NauApp::name(), tr("Project %1 at %2 already exists!").arg(name).arg(path), QMessageBox::Ok);
        setEnabled(true);
        return;
    }

    // All good - request to create a new project
    emit eventRequestProjectCreation(path, name, templateName);
    accept();
}
void NauNewProjectWindow::fillTemplates()
{
    QDir nauDir = QDir::current();
    nauDir.cd("project_templates");
    const QStringList templateNames = nauDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    m_inputTemplateComboBox->addItems(templateNames);
    m_inputTemplateComboBox->setCurrentIndex(0);
}
