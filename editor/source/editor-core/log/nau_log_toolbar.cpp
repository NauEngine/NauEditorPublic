// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_log_toolbar.hpp"
#include "nau_buttons.hpp"
#include "nau_log_constants.hpp"
#include "nau_plus_enum.hpp"
#include "themes/nau_theme.hpp"

#include <QComboBox>
#include <QCompleter>
#include <QLayout>
#include <QRegularExpression>
#include <QSettings>
#include <QStringListModel>


// ** NauLoggerFilterWidgetAction

class NauLoggerFilterWidgetAction : public NauFilterWidgetAction
{
public:
    NauLoggerFilterWidgetAction(const NauIcon& icon, const QString& name, NauLogLevel level, QWidget* parent)
        : NauFilterWidgetAction(icon, name, parent)
        , m_level(level)
    {}

    NauLogLevel level() const
    { 
        return m_level;
    }

private:
    NauLogLevel m_level;
};


// ** NauLogToolBar

NauLogToolBar::NauLogToolBar(QWidget* parent)
    : NauWidget(parent)
    , m_menu(new NauMenu())
    , m_searchWidget(new NauSearchWidget(this))
    , m_completerData(new QStringListModel(this))
    , m_completer(new QCompleter(m_completerData, this))
{
    auto topLayout = new NauLayoutVertical(this);
    auto upperLayout = new NauLayoutHorizontal;
    auto bottomLayout = new NauFlowLayout(Qt::LeftToRight, 1, 5, 1);
    topLayout->setSpacing(4);
    topLayout->setContentsMargins(4, 4, 4, 4);
    topLayout->addLayout(upperLayout);
    topLayout->addLayout(bottomLayout);

    auto leftLayout = new NauLayoutHorizontal();
    leftLayout->setContentsMargins(16, 8, 16, 8);

    m_filterWidget = new NauFilterWidget(bottomLayout, this);
    m_filterWidget->setObjectName("loggerFilterWidget");

    const auto iconLogs = Nau::Theme::current().iconLogs();

    for (const auto& [level, text] : NauLogConstants::levelByNameMap()) {
        const auto levelIdx = +level;

        auto filter = new NauLoggerFilterWidgetAction(
            (levelIdx >= 0 && levelIdx < iconLogs.size() ? iconLogs[levelIdx] : NauIcon()),
            text, level, m_filterWidget);

        m_filterWidget->addFilterParam(filter);
    }
    connect(m_filterWidget, &NauFilterWidget::eventChangeFilterRequested, [this](const QList<NauFilterWidgetAction*>& filters) {
        std::vector<NauLogLevel> preferredLevels;
        for (NauFilterWidgetAction* filter : filters) {
            preferredLevels.push_back(static_cast<NauLoggerFilterWidgetAction*>(filter)->level());
        }

        emit eventFilterByLevelChangeRequested(preferredLevels);
    });

    auto settingsButton = new NauTertiaryButton;
    settingsButton->setObjectName("loggerPanelSettingsButton");
    settingsButton->setIcon(Nau::Theme::current().iconLoggerSettings());
    settingsButton->setText(tr("Settings"));
    settingsButton->setMenu(m_menu);
    settingsButton->setFixedHeight(NauLogConstants::settingsButtonHeight());

    upperLayout->addWidget(m_filterWidget);
    upperLayout->addWidget(m_searchWidget, 1);
    upperLayout->addWidget(settingsButton);

    auto saveButton = m_menu->addAction(Nau::Theme::current().iconSave(), tr("Save As..."),
        this, &NauLogToolBar::eventSaveLogAsRequested);
    saveButton->setCheckable(true);
    saveButton->setObjectName("saveAsLogAction");

    m_menu->addSeparator();

    m_caseAction = m_menu->addAction(tr("Filter case sensitive"));
    m_caseAction->setCheckable(true);
    m_caseAction->setObjectName("caseSensitiveAction");

    m_regexAction = m_menu->addAction(tr("Filter regular expression"));
    m_regexAction->setCheckable(true);
    m_regexAction->setObjectName("regexAction");

    m_menu->addSeparator();

    m_clearOutputAction = m_menu->addAction(Nau::Theme::current().iconClean(), tr("Clear Log"));
    m_clearOutputAction->setObjectName("clearHistoryAction");

    m_clearHistoryAction = m_menu->addAction(tr("Clear Filter History"));
    m_clearHistoryAction->setObjectName("clearHistoryAction");

    m_menu->addSeparator();
 
    m_autoScrollAction = m_menu->addAction(tr("Auto Scroll"), this, &NauLogToolBar::eventAutoScrollPolicyChanged);
    m_autoScrollAction->setCheckable(true);
    m_autoScrollAction->setChecked(true);

    m_detailsPanelVisibilityAction = m_menu->addAction(Nau::Theme::current().iconLoggerDetailsToggle(),
        tr("Show Details"), this, &NauLogToolBar::eventToggleDetailVisibilityRequested);
    m_detailsPanelVisibilityAction->setObjectName("loggerDetailsToggle");
    m_detailsPanelVisibilityAction->setCheckable(true);

    m_searchWidget->setPlaceholderText(tr("Search..."));

    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_searchWidget->setCompleter(m_completer);

    connect(m_searchWidget, &QLineEdit::textChanged, this, &NauLogToolBar::emitFilterStateChanged);
    connect(m_searchWidget, &QLineEdit::editingFinished, this, &NauLogToolBar::handleFilterEditFinished);

    connect(m_caseAction, &QAction::toggled, this, &NauLogToolBar::emitFilterStateChanged);
    connect(m_regexAction, &QAction::toggled, this, &NauLogToolBar::emitFilterStateChanged);

    connect(m_clearHistoryAction, &QAction::triggered, this, &NauLogToolBar::clearCompleterHistory);
    connect(m_clearOutputAction, &QAction::triggered, this, &NauLogToolBar::eventClearOutputRequested);

    loadCompleterHistory();
}

NauLogToolBar::FilteringSettings NauLogToolBar::filteringSettings() const
{
    return { m_searchWidget->text(),
             m_regexAction->isChecked(),
             m_caseAction->isChecked() };
}

void NauLogToolBar::checkInputValidity()
{
    const FilteringSettings settings = filteringSettings();

    if (!settings.isRegularExpression) {
        // everything is ok, the input text is valid
        m_filterWidget->setToolTip(QString());
        return;
    }

    const QRegularExpression regex{settings.text};
    if (regex.isValid()) {
        m_filterWidget->setToolTip(QString());
        return;
    }

    m_filterWidget->setToolTip(regex.errorString());
}

void NauLogToolBar::clearCompleterHistory()
{
    QStringListModel* model = static_cast<QStringListModel*>(m_completerData);
    model->setStringList({});
    saveCompleterHistory();
}

QAction* NauLogToolBar::detailsPanelVisibilityAction() const
{
    return m_detailsPanelVisibilityAction;
}

bool NauLogToolBar::autoScrollEnabled() const
{
    return m_autoScrollAction->isChecked();
}

void NauLogToolBar::setAutoScrollEnabled(bool enabled)
{
    m_autoScrollAction->setChecked(enabled);
}

void NauLogToolBar::loadCompleterHistory()
{
    QStringListModel* model = static_cast<QStringListModel*>(m_completerData);
    model->setStringList(
        QSettings("./log_filter_history", QSettings::NativeFormat)
        .value("completerHistory")
        .toStringList()
    );
}

void NauLogToolBar::saveCompleterHistory()
{
    QStringListModel* model = static_cast<QStringListModel*>(m_completerData);
    QSettings("./log_filter_history", QSettings::NativeFormat)
        .setValue("completerHistory", model->stringList());
}

void NauLogToolBar::emitFilterStateChanged()
{
    eventFilterChanged(m_searchWidget->text(), m_regexAction->isChecked(), m_caseAction->isChecked());
    checkInputValidity();
}

void NauLogToolBar::handleFilterEditFinished()
{
    if (QStringListModel* const model = static_cast<QStringListModel*>(m_completerData)) {
        const QString text = m_searchWidget->text();
        if (text.isEmpty() || model->stringList().contains(text)) {
            return;
        }

        if (model->insertRow(model->rowCount())) {
            model->setData(model->index(model->rowCount() - 1, 0), text);
        }
        saveCompleterHistory();
    }
}
