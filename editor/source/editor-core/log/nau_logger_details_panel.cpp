// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_logger_details_panel.hpp"
#include "nau_log_model.hpp"
#include "nau_buttons.hpp"
#include "nau_log_constants.hpp"
#include "themes/nau_theme.hpp"

#include <QClipboard>


// ** NauLoggerDetailsPanel

NauLoggerDetailsPanel::NauLoggerDetailsPanel(NauShortcutHub* shortcutHub, NauWidget* parent)
    : QScrollArea(parent)
{
    createUi(shortcutHub);
}

void NauLoggerDetailsPanel::loadMessageInfo(const QModelIndex& index)
{
    if (!index.isValid()) {
        clearUi();
        return;
    }

    m_titleIcon->setPixmap(index.data(NauLogModel::LevelIconRole).value<QIcon>().pixmap(16, 16));
    m_titleLevel->setText(index.data(NauLogModel::LevelDisplayTextRole).toString());
    m_titleSource->setText(index.data(NauLogModel::SourceRole).toString());
    m_labelLevel->setText(index.data(NauLogModel::LevelDisplayTextRole).toString());
    m_labelLogger->setText(index.data(NauLogModel::SourceRole).toString());
    m_labelDate->setText(index.data(NauLogModel::TimeRole).toDateTime().toString(NauLogConstants::dateTimeDisplayFormat()));
    m_labelMessage->setText(index.data(NauLogModel::MessageRole).toString());
    m_labelDetails->setText(index.data(NauLogModel::LevelDetailsRole).toString());
}

void NauLoggerDetailsPanel::clearUi()
{
    m_titleLevel->setText(QString());
    m_titleSource->setText(QString());
    m_labelLevel->setText(QString());
    m_labelLogger->setText(QString());
    m_labelDate->setText(QString());
    m_labelMessage->setText(QString());
    m_labelDetails->setText(QString());
}

void NauLoggerDetailsPanel::handleCopyMessageTextSelectionRequest()
{
    QApplication::clipboard()->setText(m_labelMessage->selectedText());
}

void NauLoggerDetailsPanel::createUi(NauShortcutHub* shortcutHub)
{
    auto scrollFrame = new NauFrame(this);

    scrollFrame->setMinimumWidth(280);
    scrollFrame->setObjectName("logDetailsPanel");

    auto verticalLayout = new NauLayoutVertical(scrollFrame);
    verticalLayout->setObjectName("verticalLayout");
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->setSpacing(0);

    auto headerFrame = new NauFrame(this);
    headerFrame->setObjectName("logDetailsHeaderFrame");
    headerFrame->setFixedHeight(NauLogConstants::detailsHeaderPanelHeight());

    NauPalette headerPalette;
    headerPalette.setBrush(NauPalette::Role::Background,
        Nau::Theme::current().paletteLogger().brush(NauPalette::Role::Background));
    headerFrame->setPalette(headerPalette);

    auto titleLayout = new NauLayoutHorizontal(headerFrame);
    titleLayout->setSpacing(20);
    titleLayout->setObjectName("titleLayout");
    titleLayout->setContentsMargins(16, 16, 16, 16);
    auto titleTextLayout = new NauLayoutHorizontal();
    titleTextLayout->setSpacing(4);
    titleTextLayout->setObjectName("titleTextLayout");

    m_titleIcon = new NauLabel(headerFrame);
    m_titleIcon->setObjectName("titleIcon");
    m_titleIcon->setFixedSize(QSize(16, 16));

    titleTextLayout->addWidget(m_titleIcon);

    m_titleLevel = new NauLabel(headerFrame);
    m_titleLevel->setObjectName("titleLevel");

    titleTextLayout->addWidget(m_titleLevel);

    auto labelFrom = new NauLabel(headerFrame);
    labelFrom->setObjectName("labelFrom");

    titleTextLayout->addWidget(labelFrom);

    m_titleSource = new NauLabel(headerFrame);
    m_titleSource->setObjectName("titleSource");

    titleTextLayout->addWidget(m_titleSource);

    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    titleTextLayout->addItem(horizontalSpacer);
    titleLayout->addLayout(titleTextLayout);

    auto headerControlLayout = new NauLayoutHorizontal();
    headerControlLayout->setSpacing(20);
    headerControlLayout->setObjectName("headerControlLayout");

    auto createButton = [this, headerFrame](const QString& objName, const QString& tooltip, const NauIcon& icon,
        const std::function<void()>& clickHandler) {
        auto button = new NauMiscButton(headerFrame);
        button->setObjectName(objName);
        button->setToolTip(tooltip);
        button->setFixedSize(QSize(16, 16));
        button->setIcon(icon);
        connect(button, &NauToolButton::clicked, clickHandler);
        
        return button;
    };

    headerControlLayout->addWidget(createButton("loggerDetailsCopyMessage", tr("Copy message"), Nau::Theme::current().iconLoggerCopy(), [this] {
        QApplication::clipboard()->setText(m_labelMessage->text());
    }));

    headerControlLayout->addWidget(createButton("loggerDetailsCloseButton", tr("Close details panel"),
        Nau::Theme::current().iconClose(), std::bind(&NauLoggerDetailsPanel::eventCloseRequested, this)));

    titleLayout->addLayout(headerControlLayout);
    titleLayout->setStretch(0, 1);
    verticalLayout->addWidget(headerFrame);

    auto contentFrame = new NauFrame(this);
    contentFrame->setObjectName("logDetailsContentFrame");

    auto contentFrameLayout = new NauLayoutVertical(contentFrame);
    contentFrameLayout->setObjectName("contentFrameLayout");
    auto horizontalLayoutLevel = new NauLayoutHorizontal();
    horizontalLayoutLevel->setSpacing(4);
    horizontalLayoutLevel->setObjectName("horizontalLayoutLevel");
    auto labelLevelTitle = new NauLabel(contentFrame);
    labelLevelTitle->setObjectName("labelLevelTitle");

    horizontalLayoutLevel->addWidget(labelLevelTitle);

    m_labelLevel = new NauLabel(contentFrame);
    m_labelLevel->setObjectName("labelLevel");

    horizontalLayoutLevel->addWidget(m_labelLevel);
    horizontalLayoutLevel->setStretch(1, 1);

    contentFrameLayout->addLayout(horizontalLayoutLevel);

    auto horizontalLayoutLogger = new NauLayoutHorizontal();
    horizontalLayoutLogger->setSpacing(4);
    horizontalLayoutLogger->setObjectName("horizontalLayoutLogger");
    auto labelLoggerTitle = new NauLabel(contentFrame);
    labelLoggerTitle->setObjectName("labelLoggerTitle");

    horizontalLayoutLogger->addWidget(labelLoggerTitle);

    m_labelLogger = new NauLabel(contentFrame);
    m_labelLogger->setObjectName("labelLogger");

    horizontalLayoutLogger->addWidget(m_labelLogger);
    horizontalLayoutLogger->setStretch(1, 1);

    contentFrameLayout->addLayout(horizontalLayoutLogger);

    auto horizontalLayoutDate = new NauLayoutHorizontal();
    horizontalLayoutDate->setSpacing(4);
    horizontalLayoutDate->setObjectName("horizontalLayoutDate");
    auto labelDateTitle = new NauLabel(contentFrame);
    labelDateTitle->setObjectName("labelDateTitle");

    horizontalLayoutDate->addWidget(labelDateTitle);

    m_labelDate = new NauLabel(contentFrame);
    m_labelDate->setObjectName("labelDate");

    horizontalLayoutDate->addWidget(m_labelDate);
    horizontalLayoutDate->setStretch(1, 1);

    contentFrameLayout->addLayout(horizontalLayoutDate);

    auto verticalLayoutMessageContent = new NauLayoutVertical();
    verticalLayoutMessageContent->setObjectName("verticalLayoutMessageContent");
    auto labelMessageTitle = new NauLabel(contentFrame);
    labelMessageTitle->setObjectName("labelMessageTitle");

    verticalLayoutMessageContent->addWidget(labelMessageTitle);

    m_labelMessage = new NauLabel(contentFrame);
    m_labelMessage->setObjectName("labelMessage");
    m_labelMessage->setWordWrap(true);
    m_labelMessage->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_labelMessage->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    connect(m_labelMessage, &NauFrame::customContextMenuRequested, [this, shortcutHub](const QPoint& pos){
        NauMenu menu;

        const auto copyKey = shortcutHub->getAssociatedKeySequence(NauShortcutOperation::LoggerCopyTextSelection);
        menu.addAction(Nau::Theme::current().iconLoggerCopy(), tr("Copy"), copyKey,
            this, &NauLoggerDetailsPanel::handleCopyMessageTextSelectionRequest);

        menu.exec(QCursor::pos());
    });
    shortcutHub->addWidgetShortcut(NauShortcutOperation::LoggerCopyTextSelection, *m_labelMessage,
        std::bind(&NauLoggerDetailsPanel::handleCopyMessageTextSelectionRequest, this));

    verticalLayoutMessageContent->addWidget(m_labelMessage);

    contentFrameLayout->addLayout(verticalLayoutMessageContent);

    auto tagsContainerLayout = new NauLayoutVertical();
    tagsContainerLayout->setObjectName("tagsContainerLayout");
    auto labelTagsTitle = new NauLabel(contentFrame);
    labelTagsTitle->setObjectName("labelTagsTitle");

    tagsContainerLayout->addWidget(labelTagsTitle);

    m_tagsContainer = new NauWidget(contentFrame);
    m_tagsContainer->setObjectName("tagsContainer");

    tagsContainerLayout->addWidget(m_tagsContainer);

    contentFrameLayout->addLayout(tagsContainerLayout);

    auto verticalLayoutMessageLevelDetails = new NauLayoutVertical();
    verticalLayoutMessageLevelDetails->setObjectName("verticalLayoutMessageLevelDetails");
    auto labelDetailsTitle = new NauLabel(contentFrame);
    labelDetailsTitle->setObjectName("labelDetailsTitle");

    verticalLayoutMessageLevelDetails->addWidget(labelDetailsTitle);

    m_labelDetails = new NauLabel(contentFrame);
    m_labelDetails->setObjectName("labelDetails");
    m_labelDetails->setWordWrap(true);

    verticalLayoutMessageLevelDetails->addWidget(m_labelDetails);

    contentFrameLayout->addLayout(verticalLayoutMessageLevelDetails);
    contentFrameLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    verticalLayout->addWidget(contentFrame);

    // These properties used for qss targeting.
    // Here we have to font to each label for we've got the application wide qss applied.
    // It stops propagating the font from parent to child.
    // TODO Should be refactored after usage of qss removed from the editor.
    const auto processLabels = [](const char* propName, std::vector<NauLabel*> labels) {
        for (auto label : labels) {
            label->setProperty(propName, true);
            label->setFont(Nau::Theme::current().fontLogger());
        }
    };

    processLabels("logDetailsPanelValue", {m_titleLevel, m_titleSource, m_labelLevel,
        m_labelLogger, m_labelDate, m_labelMessage, m_labelDetails});

    processLabels("logDetailsPanelName", {labelLevelTitle, labelLoggerTitle, labelDateTitle,
        labelMessageTitle, labelTagsTitle, labelDetailsTitle});

    labelFrom->setText(tr("from"));
    labelLevelTitle->setText(tr("Level:"));
    labelLoggerTitle->setText(tr("Logger:"));
    labelDateTitle->setText(tr("Date:"));
    labelMessageTitle->setText(tr("Message:"));
    labelTagsTitle->setText(tr("Tags:"));
    labelDetailsTitle->setText(tr("Details:"));

    setWidget(scrollFrame);
    setWidgetResizable(true);
    setMinimumWidth(300);
    setObjectName("scrollFrameLoggerDetailsPanel");
}
