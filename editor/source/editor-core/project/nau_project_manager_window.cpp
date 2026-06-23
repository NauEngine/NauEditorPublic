// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_color.hpp"
#include "nau_editor_version.hpp"
#include "nau_log.hpp"
#include "nau_new_project_window.hpp"
#include "nau_project_manager_window.hpp"
#include "nau_project_path.hpp"
#include "nau_recent_project_view.hpp"
#include "nau_settings.hpp"
#include "nau_widget.hpp"
#include "nau/app/nau_qt_app.hpp"
#include "themes/nau_default_theme.hpp"
#include "themes/nau_theme.hpp"

#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>


// ** NauProjectManagerMenuButton

NauProjectManagerMenuButton::NauProjectManagerMenuButton(const QString& name, const QString& icon)
    : NauWidget()
    , m_iconDefault(icon + "-default.png")
    , m_iconHover(icon + "-hover.png")
    , m_iconActive(icon + "-active.png")
{
    auto layout = new NauLayoutHorizontal(this);
    
    // Icon
    m_icon = new QLabel(this);
    m_icon->setPixmap(QPixmap(m_iconDefault).scaledToWidth(16, Qt::SmoothTransformation));
    layout->addWidget(m_icon, 0, Qt::AlignVCenter);
    
    // Name
    m_text = new QLabel(name, this);
    m_text->setFont(Nau::Theme::current().fontProjectManagerMenuButton());
    layout->addSpacing(8);
    layout->addWidget(m_text, 0, Qt::AlignVCenter);
}

void NauProjectManagerMenuButton::setState(State state)
{
    if (state == m_stateCurrent) return;

    m_stateCurrent = state;

    if (m_stateCurrent == Default) {
        m_icon->setPixmap(QPixmap(m_iconDefault).scaledToWidth(16, Qt::SmoothTransformation));
        m_text->setStyleSheet(QString("QLabel { color : %1; }").arg(textColorDefault.hex()));
    } else if (m_stateCurrent == Hover) {
        m_icon->setPixmap(QPixmap(m_iconHover).scaledToWidth(16, Qt::SmoothTransformation));
        m_text->setStyleSheet(QString("QLabel { color : %1; }").arg(textColorHover.hex()));
    } else if (m_stateCurrent == Active) {
        m_icon->setPixmap(QPixmap(m_iconActive).scaledToWidth(16, Qt::SmoothTransformation));
        m_text->setStyleSheet(QString("QLabel { color : %1; }").arg(textColorActive.hex()));
    } else {
        NAU_ASSERT(false, "Unknown button state");
    }

    update();
}

void NauProjectManagerMenuButton::mouseReleaseEvent(QMouseEvent* event)
{
    emit eventPressed();
}

void NauProjectManagerMenuButton::enterEvent(QEnterEvent* event)
{
    if (m_stateCurrent != Active) {
        setState(Hover);
        update();
    }
}

void NauProjectManagerMenuButton::leaveEvent(QEvent* event)
{
    if (m_stateCurrent == Hover) {
        setState(Default);
        update();
    }
}


// ** NauProjectManagerMenu

NauProjectManagerMenu::NauProjectManagerMenu(QWidget* parent)
    : NauWidget(parent)
    , m_layoutActions(nullptr)
{
    setFixedHeight(Height);

    auto layout = new NauLayoutVertical(this);
    auto widgetActions = new NauWidget(this);
    m_layoutActions = new NauLayoutHorizontal(widgetActions);
    layout->addWidget(widgetActions);
    layout->addStretch(1);
}

NauProjectManagerMenuButton* NauProjectManagerMenu::addAction(const QString& name, const QString& icon)
{
    auto button = new NauProjectManagerMenuButton(name, icon);
    if (!m_activeMenu) m_activeMenu = button;

    connect(button, &NauProjectManagerMenuButton::eventPressed, [this, button] {
        m_activeMenu = button;
        for (auto _button : findChildren<NauProjectManagerMenuButton*>()) {
            _button->setState(_button == button ? NauProjectManagerMenuButton::Active
                : NauProjectManagerMenuButton::Default);
        }
        update();
        emit eventButtonPressed(button);
    });

    const auto nButtons = findChildren<NauProjectManagerMenuButton*>().size();
    if (nButtons > 0) m_layoutActions->addSpacing(VSpacing);
    m_layoutActions->addWidget(button);
    update();

    return button;
}

void NauProjectManagerMenu::addStretch()
{
    m_layoutActions->addStretch(1);
}

void NauProjectManagerMenu::setActiveButton(NauProjectManagerMenuButton* button)
{
    NAU_ASSERT(findChildren<NauProjectManagerMenuButton*>().contains(button));
    button->setState(NauProjectManagerMenuButton::Active);
}

void NauProjectManagerMenu::paintEvent(QPaintEvent* event)
{
    NauPainter painter(this);
    
    // Divider
    painter.setPen(NauColor(56, 56, 56));
    painter.drawLine(QPoint(0, height() - 3), QPoint(width(), height() - 3));

    // Selected item
    if (m_activeMenu) {
        const QPoint menuPos = m_activeMenu->pos();
        const QRect menuRect = m_activeMenu->rect();
        painter.fillRect(QRect(menuPos.x(), height() - 3, menuRect.width(), 3), NauColor(255, 126, 41));
    }
}

// ** NauProjectManagerWindow

NauProjectManagerWindow::NauProjectManagerWindow(NauMainWindow* parent)
    : NauDialog(parent)
{
    NED_DEBUG("Showing project manager");

    setFixedSize(600, 395);
    setWindowTitle(NauApp::name() + " " + NauEditorVersion::current().asQtString());
    setStyleSheet(QString("background-color: %1").arg(NauColor(19, 21, 22).hex()));

    auto layout = new NauLayoutVertical(this);
    layout->setContentsMargins(Margin, Margin, Margin, 0);

    // Menu
    auto menu = new NauProjectManagerMenu(this);
    auto actionProjects = menu->addAction(tr("MY PROJECTS"), ":/project/icons/project/project");
    auto actionCreate = menu->addAction(tr("NEW PROJECT"), ":/project/icons/project/new");
    menu->addStretch();
    auto actionLoad = menu->addAction(tr("OPEN"), ":/project/icons/project/load");
    menu->setActiveButton(actionProjects);
    layout->addWidget(menu);

    connect(menu, &NauProjectManagerMenu::eventButtonPressed, 
        [this, actionCreate, actionLoad](NauProjectManagerMenuButton* button) {
            if (button == actionCreate) {
                handleNewProject();
            } else if (button == actionLoad) {
                handleLoadProject();
            }
    });

    // Recent projects
    auto centralWidget = new NauWidget(this);
    auto centralLayout = new NauLayoutVertical(centralWidget);
    layout->addSpacing(HSpacing);
    layout->addWidget(centralWidget);
    auto scrollWidget = new NauScrollWidgetVertical(centralWidget);
    const NauProjectPathList projectPaths = NauSettings::recentProjectPaths(); 

    NED_DEBUG("Loading recent projects...");

    for (auto& path : projectPaths) {
        auto view = new NauRecentProjectView(NauProject::info(path), centralWidget);
        
        // Open
        connect(view, &NauRecentProjectView::eventClicked, this, &NauProjectManagerWindow::handleProjectClicked);

        // Remove
        connect(view, &NauRecentProjectView::eventClear, [view, path] {
            NED_DEBUG("Removing recent project at {}", static_cast<std::string>(path));
            view->deleteLater();
            NauSettings::tryAndRemoveRecentProjectPath(path);
        });
        scrollWidget->addWidget(view);
    }

    // Open recent
    centralLayout->addWidget(scrollWidget);
}

void NauProjectManagerWindow::handleNewProject()
{
    auto newProjectWindow = new NauNewProjectWindow(this);
    connect(newProjectWindow, &NauNewProjectWindow::eventRequestProjectCreation, this, &NauProjectManagerWindow::createAndLoadProject);
    newProjectWindow->showModal();
}

void NauProjectManagerWindow::handleLoadProject()
{
    const QString projectsDir = NauSettings::recentProjectDirectory().absolutePath();
    const QString projectPath = QFileDialog::getOpenFileName(this, tr("Open project"), projectsDir, tr("Nau project (*.nauproject)"));
    if (projectPath.isEmpty()) {
        return;
    }
    loadProject({ .path = NauProjectPath(projectPath.toUtf8().data()) });
}

void NauProjectManagerWindow::handleProjectClicked(const NauProjectInfo& projectInfo)
{
    NED_DEBUG("Project {}, at \"{}]\" clicked", projectInfo.name.value(), static_cast<std::string>(projectInfo.path));

    bool needsAnUpgrade = false;

    const auto current = NauEditorVersion::current();
    if (!projectInfo.version->isValid() || current < projectInfo.version) {   // Unsupported project version
        
        QMessageBox::warning(this, tr("Unsupported project version"),
            tr("Your current %1 version %2 is lower than the project version %3. Please update %1 or switch to a different version.")
            .arg(NauApp::name()).arg(current.asQtString()).arg(projectInfo.version->asQtString()));
        return;

    } else if (current > projectInfo.version) {   // Outdated project version

        const auto result = QMessageBox::warning(this, tr("Confirm version upgrade"),
            tr("This project was created in %1 version %2. Upgrade the project to be able to open it in %1 %3?")
            .arg(NauApp::name())
            .arg(projectInfo.version->asQtString())
            .arg(current.asQtString()), QMessageBox::Ok, QMessageBox::Cancel);
        
        if (result == QMessageBox::Ok) {
            needsAnUpgrade = true;
        } else {
            NED_DEBUG("User refused to upgrade the project");
            return;
        }
    }

    loadProject(projectInfo, needsAnUpgrade);
}

void NauProjectManagerWindow::createAndLoadProject(const QString& path, const QString& name, const QString& templateName)
{
    auto project = NauProject::create(path, name, templateName);
    NauSettings::setRecentProjectDirectory(path);

    loadProject({ .path = project->path() });
}

void NauProjectManagerWindow::loadProject(const NauProjectInfo& projectInfo, bool needsAnUpgrade)
{
    auto project = NauProject::load(projectInfo.path);
    if (needsAnUpgrade) {
        project->upgradeToCurrentEditorVersion();
    }

    setDisabled(true);
    emit eventLoadProject(project);
    accept();

    NED_DEBUG("Closing projects...");
    for (auto* view : findChildren<NauRecentProjectView*>()) {
        view->deleteLater();
    }
}
