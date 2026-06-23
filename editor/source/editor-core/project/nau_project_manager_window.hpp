// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Project manager dialog for creating/loading projects

#pragma once

#include "nau_project.hpp"
#include "nau_static_text_label.hpp"

#include <QObject>


// ** NauProjectManagerMenuButton

class NauProjectManagerMenuButton : public NauWidget
{
    Q_OBJECT

    friend class NauProjectManagerMenu;

    enum State {
        Default = 0,
        Hover,
        Active
    };

private:
    NauProjectManagerMenuButton(const QString& name, const QString& icon);

signals:
    void eventPressed();

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void setState(State state);

private:
    State m_stateCurrent = Default;
    
    QLabel* m_icon;
    QLabel* m_text;

    const QPixmap m_iconDefault;
    const QPixmap m_iconHover;
    const QPixmap m_iconActive;

    inline static constexpr auto textColorDefault  = NauColor(136, 136, 136);
    inline static constexpr auto textColorHover    = NauColor(255, 255, 255);
    inline static constexpr auto textColorActive   = NauColor(255, 255, 255);

};



// ** NauProjectManagerMenu

class NAU_EDITOR_API NauProjectManagerMenu : public NauWidget
{
    Q_OBJECT

public:
    NauProjectManagerMenu(QWidget* parent);

    NauProjectManagerMenuButton* addAction(const QString& name, const QString& icon);
    
    void addStretch();
    void setActiveButton(NauProjectManagerMenuButton* button);

protected:
    void paintEvent(QPaintEvent* event) override;

signals:
    void eventButtonPressed(NauProjectManagerMenuButton* button);

private:
    inline static constexpr int Height    = 27;
    inline static constexpr int VSpacing  = 24;

private:
    NauLayoutHorizontal* m_layoutActions;
    NauProjectManagerMenuButton* m_activeMenu = nullptr;
};


// ** NauProjectManagerWindow

class NAU_EDITOR_API NauProjectManagerWindow : public NauDialog
{
    Q_OBJECT

public:
    NauProjectManagerWindow(NauMainWindow* parent);

signals:
    void eventLoadProject(NauProjectPtr path);

private slots:
    void handleNewProject();
    void handleLoadProject();
    void handleProjectClicked(const NauProjectInfo& projectInfo);

private:
    void createAndLoadProject(const QString& path, const QString& name, const QString& templateName);
    void loadProject(const NauProjectInfo& projectInfo, bool needsAnUpgrade = false);

private:
    inline static constexpr int Margin   = 32;
    inline static constexpr int HSpacing = 14;
};
