// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_main_menu.hpp"
#include "nau/app/nau_qt_app.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"


// ** NauMainMenu

NauMainMenu::NauMainMenu(NauShortcutHub* shortcutHub, NauWidget* parent)
    : NauMenuBar(parent)
    , m_shortcutHub(std::move(shortcutHub))
{
    buildProjectSection();
    buildSceneSection();
    buildEditSection();
    buildLanguageSection();
    buildViewSection();
    buildHelpSection();
}

NauMenu* NauMainMenu::getMenu(MenuItem item) const
{
    auto it = m_menuByItem.find(item);
    if (it == m_menuByItem.end())
        return nullptr;

    return it->second;
}

void NauMainMenu::setEditorLanguage(const QString& lang)
{
    auto itLangAction =  m_langActionByLang.find(lang);
    if (itLangAction != m_langActionByLang.end()) {
        itLangAction->second->setChecked(true);
        return;
    }

    NED_ERROR("Can't find language {} in main menu", lang);
}

void NauMainMenu::buildProjectSection()
{
    auto menu = new NauMenu(tr("Project"), this);
    menu->addAction(tr("Save Project"), this, &NauMainMenu::saveProjectRequested);
    #if defined(Q_OS_WIN)
    menu->addAction(tr("Show in Explorer"), this, &NauMainMenu::eventReveal);
    #elif defined(Q_OS_MAC)
    menu->addAction(tr("Reveal in Finder"), this, &NauMainMenu::eventReveal);
    #else
    #error Not implemented on this platform
    #endif
    addMenu(menu);
    m_menuByItem.insert({ MenuItem::Project, menu });
}

void NauMainMenu::buildSceneSection()
{
    auto menu = new NauMenu(tr("Scene"), this);

    auto actionNew = menu->addAction(tr("New Scene"), 
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::NewScene), this, &NauMainMenu::eventNewScene);
    m_shortcutHub->addApplicationShortcut(NauShortcutOperation::NewScene, std::bind(&QAction::trigger, actionNew));

    auto actionLoad = menu->addAction(tr("Open Scene"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::OpenScene), this, &NauMainMenu::eventLoadScene);
    m_shortcutHub->addApplicationShortcut(NauShortcutOperation::OpenScene, std::bind(&QAction::trigger, actionLoad));

    auto actionSave = menu->addAction(tr("Save Scene"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::SaveScene), this, &NauMainMenu::eventSaveScene);
    m_shortcutHub->addApplicationShortcut(NauShortcutOperation::SaveScene, std::bind(&QAction::trigger, actionSave));

    auto menuRecent = menu->addMenu(tr("Recent Scenes"));
    connect(menuRecent, &QMenu::aboutToShow, [this, menuRecent] {
        emit eventRecentScenes(menuRecent);
    });
    addMenu(menu);
    m_menuByItem.insert({ MenuItem::Scene, menu });
}

void NauMainMenu::buildEditSection()
{
    auto menu = new NauMenu(tr("Edit"), this);
    auto actionUndo = menu->addAction(tr("Undo"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::Undo), this, &NauMainMenu::eventUndo);
    m_shortcutHub->addApplicationShortcut(NauShortcutOperation::Undo, std::bind(&QAction::trigger, actionUndo));

    auto actionRedo = menu->addAction(tr("Redo"),
        m_shortcutHub->getAssociatedKeySequence(NauShortcutOperation::Redo), this, &NauMainMenu::eventRedo);
    m_shortcutHub->addApplicationShortcut(NauShortcutOperation::Redo, std::bind(&QAction::trigger, actionRedo));

    addMenu(menu);
    m_menuByItem.insert({ MenuItem::Edit, menu });
}

void NauMainMenu::buildViewSection()
{
    auto menu = new NauMenu(tr("Window"), this);
    addMenu(menu);

    m_menuByItem.insert({ MenuItem::View, menu });
}

void NauMainMenu::buildHelpSection()
{
    auto menu = new NauMenu(tr("Help"), this);
    menu->addAction(tr("About %1").arg(NauApp::name()), this, &NauMainMenu::aboutDialogRequested);
    menu->addAction(tr("Feedback"), this, &NauMainMenu::feedbackDialogRequested);
    addMenu(menu);

    m_menuByItem.insert({ MenuItem::Help, menu });
}

void NauMainMenu::buildLanguageSection()
{
    auto targetMenu = getMenu(MenuItem::Edit);
    NED_ASSERT(targetMenu);

    targetMenu->addSeparator();
    auto menu = targetMenu->addMenu(tr("Editor Language"));

    static const std::vector<std::pair<QString, QString>> languages = {
        { QStringLiteral("Русский"), QStringLiteral("ru")},
        { QStringLiteral("English"), QStringLiteral("en")},
    };

    for (const auto&[title, lang] : languages) {
        auto langAction = menu->addAction(title);
        
        connect(langAction, &QAction::triggered, [this, lang, langAction] (bool on) {
            if (on) {
                langAction->setChecked(false);
                emit eventSwitchLanguageRequested(lang);
            }
        });
        langAction->setCheckable(true);
        m_langActionByLang.insert({lang, langAction});
    }
}
