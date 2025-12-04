// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/inspector/nau_physics_inspector_widgets.hpp"
#include "baseWidgets/nau_buttons.hpp"
#include "baseWidgets/nau_spoiler.hpp"
#include "filter/nau_filter_checkbox.hpp"
#include "nau/physics/nau_phyisics_collision_channel_model.hpp"
#include "nau_log.hpp"
#include "themes/nau_theme.hpp"
 
#include <QCompleter>
#include <QStandardItemModel>


namespace
{
    bool settingsRegResult = NauUsdPropertyFactory::addPropertyWidgetCreator("physics:collisionChannelSettings",
        NauInspectorPhysicsCollisionButton::Create);

    bool channelChooseRegResult = NauUsdPropertyFactory::addPropertyWidgetCreator("physics:collisionChannel",
        NauInspectorPhysicsCollisionSelector::Create);

    bool materialRegResult = NauUsdPropertyFactory::addPropertyWidgetCreator("PhysicsMaterial:material",
        NauInspectorPhysicsMaterialAsset::Create);
}


// ** NauInspectorPhysicsMaterialAsset

NauInspectorPhysicsMaterialAsset::NauInspectorPhysicsMaterialAsset(NauWidget* parent)
    : NauAssetProperty({}, "PhysicsMaterial", /*clearable*/ true, parent)
{
    setLabel(tr("Physical Material"));
}

NauUsdPropertyAbstract* NauInspectorPhysicsMaterialAsset::Create(const std::string&, const std::string&)
{
    return new NauInspectorPhysicsMaterialAsset();
}


// ** NauInspectorPhysicsCollisionButton

NauInspectorPhysicsCollisionButton::NauInspectorPhysicsCollisionButton(NauWidget* parent)
    : NauUsdSingleRowPropertyBase("", parent)
    , m_popupMenu(new NauMenu())
{
    setLabel(tr("Channels Interaction"));

    auto openSettingsButton = new NauMiscButton(this);
    openSettingsButton->setIcon(Nau::Theme::current().iconSettings());
    openSettingsButton->setToolTip(tr("Open channels collision settings"));
    openSettingsButton->setMenu(m_popupMenu);
    openSettingsButton->setFixedSize(16, 16);

    auto actionWidget = new QWidgetAction(this);
    auto popupContainer = new NauFrame;
    popupContainer->setObjectName("collisionChannelPopupWindow");
    auto popupLayout = new NauLayoutVertical(popupContainer);
    popupLayout->setContentsMargins(QMargins());

    auto settingsWidget = new NauPhysicsCollisionSettingsWidget(this);
    popupLayout->addWidget(settingsWidget);

    actionWidget->setDefaultWidget(popupContainer);
    m_popupMenu->addAction(actionWidget);
    m_popupMenu->setObjectName("inspectorCollisionSettingButtonMenu");
    m_popupMenu->setContentsMargins(QMargins());
    m_popupMenu->setContentsMargins(QMargins());
    m_popupMenu->setStyleSheet("background-color: #343434; padding:0px;");

    m_contentLayout->addWidget(openSettingsButton, 0, 2);

    connect(settingsWidget, &NauPhysicsCollisionSettingsWidget::eventCancelRequested, m_popupMenu, &QMenu::close);
    connect(settingsWidget, &NauPhysicsCollisionSettingsWidget::eventSaveRequested, [this] {
        if (Nau::getPhysicsCollisionChannelModel().saveChannels()) {
            Nau::getPhysicsCollisionChannelModel().applySettingsToPhysicsWorld();
        }
    });
}

NauUsdPropertyAbstract* NauInspectorPhysicsCollisionButton::Create(const std::string&, const std::string&)
{
    return new NauInspectorPhysicsCollisionButton();
}

PXR_NS::VtValue NauInspectorPhysicsCollisionButton::getValue()
{
    return {};
}

void NauInspectorPhysicsCollisionButton::setValueInternal(const PXR_NS::VtValue&)
{
}


// ** NauPhysicsCollisionSettingsWidget

const char* const NauPhysicsCollisionSettingsWidget::m_propertyChannelId = "channelId";
const char* const NauPhysicsCollisionSettingsWidget::m_propertyGroupChannelId = "groupChannelId";

NauPhysicsCollisionSettingsWidget::NauPhysicsCollisionSettingsWidget(QWidget* parent)
    : NauFrame(parent)
{
    setupUi();
}

void NauPhysicsCollisionSettingsWidget::keyPressEvent(QKeyEvent *event)
{
    NauFrame::keyPressEvent(event);
    event->accept();
}

void NauPhysicsCollisionSettingsWidget::keyReleaseEvent(QKeyEvent *event)
{
    NauFrame::keyReleaseEvent(event);
    event->accept();
}

void NauPhysicsCollisionSettingsWidget::setupUi()
{
    setObjectName("physicsCollisionSettingsWidget");

    auto layout = new NauLayoutVertical(this);
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);

    layout->addWidget(createHeaderPanel());
    layout->addWidget(createAddPanel());
    layout->addWidget(m_contentScrollArea = createContentScrollArea());
    layout->addWidget(new NauSpacer(Qt::Horizontal, 1, this));
    layout->addWidget(createButtonPanel());
}

void NauPhysicsCollisionSettingsWidget::rebuildMutableUi()
{
    recreateContentArea();
    const auto& channels = Nau::getPhysicsCollisionChannelModel().channels();

    for (const auto& channel : channels) {
        auto spoiler = new NauSimpleSpoiler(QString::fromStdString(channel.name), m_contentScrollArea->widget());
        spoiler->setHeaderFixedHeight(36);
        spoiler->setHeaderContentMargins(QMargins(16, 0, 16, 0));
        spoiler->setHeaderHorizontalSpace(0);
        spoiler->setHeaderPalette(Nau::Theme::current().palettePhysicsChannelSettings());
        spoiler->setContentAreaMargins(QMargins(16, 8, 16, 8));
        spoiler->setContentVerticalSpacing(16);
        spoiler->setTitleEditable(true);
        spoiler->setExpanded(false);
        spoiler->setToolTip(tr("Double click the title to rename"));

        connect(spoiler, &NauSimpleSpoiler::eventRenameRequested, [this, spoiler, channelIdx = channel.channel](const QString& newName) {
            if (Nau::getPhysicsCollisionChannelModel().renameChannel(channelIdx, newName.toStdString())) {
                spoiler->setTitle(newName);
                QList<NauCheckBox*> allCheckboxes = findChildren<NauCheckBox*>();
                for (NauCheckBox* ch : allCheckboxes) {
                    const auto channelId = ch->property(m_propertyChannelId);
                    if (channelId.isValid() &&  channelId.toInt() == channelIdx) {
                        ch->setText(newName);
                    }
                }
            }
        });

        auto allCheckbox = new NauFilterCheckBox(tr("All"), m_contentScrollArea->widget());
        allCheckbox->setTristate(true);
        allCheckbox->setObjectName(QStringLiteral("All_Channel_g=%1").arg(channel.channel));

        spoiler->addWidget(allCheckbox);
        std::vector<NauCheckBox*> channelCheckboxes;

        for (const auto& otherChannel : channels) {
            auto checkbox = new NauCheckBox(QString::fromStdString(otherChannel.name), m_contentScrollArea->widget());
            channelCheckboxes.push_back(checkbox);
            checkbox->setProperty(m_propertyChannelId, otherChannel.channel);
            checkbox->setProperty(m_propertyGroupChannelId, channel.channel);
            checkbox->setObjectName(QStringLiteral("Channel_i=%1_g=%2").arg(otherChannel.channel).arg(channel.channel));
            checkbox->setChecked(Nau::getPhysicsCollisionChannelModel().channelsCollideable(channel.channel, otherChannel.channel));

            spoiler->addWidget(checkbox);
        }

        auto spoilerDeleteButton = spoiler->addHeaderButton();
        spoilerDeleteButton->setEnabled(channel.channel != NauPhysicsCollisionChannelModel::defaultChannel());
        spoilerDeleteButton->setIcon(Nau::Theme::current().iconClose());
        spoilerDeleteButton->setToolTip(spoilerDeleteButton->isEnabled()
            ? tr("Delete this collision channel")
            : tr("This collision channel is default one. It cannot be deleted"));

        connect(spoilerDeleteButton, &NauToogleButton::clicked, [this, name = channel.name, channel = channel.channel, spoiler, allCheckbox, channelCheckboxes] {
            const bool result = Nau::getPhysicsCollisionChannelModel().deleteChannel(channel);
            if (result) {
                auto item =  m_contentScrollArea->layout()->takeAt(m_contentScrollArea->layout()->indexOf(spoiler));
                delete item->widget();
                delete item;

                QList<NauCheckBox*> allCheckboxes = findChildren<NauCheckBox*>();
                for (NauCheckBox* ch : allCheckboxes) {
                    const auto channelId = ch->property(m_propertyChannelId);
                    if (channelId.isValid() &&  channelId.toInt() == channel) {
                        ch->setEnabled(false);
                        ch->hide();
                    }
                }
            }
            NED_DEBUG("Deleted collision channel {} finished with {}", name, result);
        });

        connect(allCheckbox, &NauCheckBox::stateChanged, this, [channelCheckboxes, allCheckbox](int state) {
            const auto currentState = static_cast<Qt::CheckState>(state);
            if (currentState == Qt::CheckState::PartiallyChecked) {
                return;
            }

            for (NauCheckBox* checkbox : channelCheckboxes) {
                if (checkbox->isEnabled())
                checkbox->setCheckState(currentState);
            }
        });

        updateAllCheckbox(channelCheckboxes, allCheckbox);

        for (NauCheckBox* checkbox : channelCheckboxes) {
            connect(checkbox, &NauCheckBox::stateChanged, this, [this, originChannel = channel.channel, checkbox, allCheckbox, channelCheckboxes](int state) {
                const auto thisChannelId = static_cast<NauPhysicsCollisionChannelModel::Channel>(checkbox->property(m_propertyChannelId).toInt());

                Nau::getPhysicsCollisionChannelModel().setChannelsCollideable(originChannel, thisChannelId, state == Qt::Checked);

                QList<NauCheckBox*> allCheckboxes = findChildren<NauCheckBox*>();
                for (NauCheckBox* ch : allCheckboxes) {
                    const auto channelId = ch->property(m_propertyChannelId);
                    const auto groupChannelId = ch->property(m_propertyGroupChannelId);

                    if (ch->isEnabled() && channelId.isValid() && groupChannelId.isValid() &&
                        groupChannelId.toInt() == thisChannelId && channelId.toInt() == originChannel) {
                        ch->setCheckState(static_cast<Qt::CheckState>(state));
                    }
                }

                updateAllCheckbox(channelCheckboxes, allCheckbox);
            });
        }
        m_contentScrollArea->layout()->addWidget(spoiler);
    }

    m_contentScrollArea->layout()->addSpacerItem(new QSpacerItem(10, 1000, QSizePolicy::Minimum, QSizePolicy::Expanding));
    static_cast<NauLayoutVertical*>(layout())->insertWidget(2, m_contentScrollArea);
}

void NauPhysicsCollisionSettingsWidget::recreateContentArea()
{
    if (auto item = layout()->takeAt(2)) {
        if (auto widget = item->widget()) {
            delete widget;
        }

        delete item;
    }

    m_contentScrollArea = createContentScrollArea();
}

NauFrame* NauPhysicsCollisionSettingsWidget::createAddPanel()
{
    auto addButton = new NauTertiaryButton(this);
    addButton->setText(tr("Add Channel"));
    addButton->setIcon(Nau::Theme::current().iconAddTertiaryStyle());
    addButton->setFixedHeight(32);

    connect(addButton, &NauTertiaryButton::clicked, [this] {
        Nau::getPhysicsCollisionChannelModel().addChannel();
        rebuildMutableUi();
    });

    auto addPanel = new NauFrame;
    addPanel->setPalette(Nau::Theme::current().palettePhysicsChannelSettings());

    auto addPanelLayout = new NauLayoutHorizontal(addPanel);
    addPanelLayout->addWidget(addButton);
    addPanelLayout->addStretch(1);
    addPanelLayout->setContentsMargins(QMargins());

    return addPanel;
}

NauFrame* NauPhysicsCollisionSettingsWidget::createHeaderPanel()
{
    auto headerFrame = new NauFrame;
    auto headerPalette = Nau::Theme::current().palettePhysicsChannelSettings();
    headerPalette.setBrush(NauPalette::Role::Background, headerPalette.brush(NauPalette::Role::BackgroundHeader));

    headerFrame->setPalette(headerPalette);
    headerFrame->setFixedHeight(40);
    auto headerLayout = new NauLayoutHorizontal(headerFrame);
    auto headerCloseButton = new NauMiscButton(headerFrame);
    headerCloseButton->setIcon(Nau::Theme::current().iconClose());
    connect(headerCloseButton, &NauMiscButton::clicked, 
        this, &NauPhysicsCollisionSettingsWidget::eventCancelRequested);

    headerLayout->addWidget(new NauStaticTextLabel(tr("Physics Channels")), 1);
    headerLayout->addWidget(headerCloseButton);

    return headerFrame;
}

NauFrame* NauPhysicsCollisionSettingsWidget::createButtonPanel()
{
    auto container = new NauFrame;

    auto footerPalette = Nau::Theme::current().palettePhysicsChannelSettings();
    footerPalette.setBrush(NauPalette::Role::Background, footerPalette.brush(NauPalette::Role::BackgroundFooter));

    container->setPalette(footerPalette);
    container->setFixedHeight(60);

    auto layout = new NauLayoutHorizontal(container);
    layout->setSpacing(8);
    layout->setContentsMargins(QMargins());
    layout->addSpacerItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));

    auto cancelButton = new NauSecondaryButton(this);
    cancelButton->setText(tr("Cancel"));
    cancelButton->setIcon(Nau::Theme::current().iconClose());
    cancelButton->setFixedHeight(28);

    auto saveButton = new NauPrimaryButton(this);
    saveButton->setText(tr("Save"));
    saveButton->setIcon(Nau::Theme::current().iconSave());
    saveButton->setFixedHeight(28);

    layout->addWidget(cancelButton, 0, Qt::AlignCenter);
    layout->addWidget(saveButton,  0, Qt::AlignCenter);

    connect(saveButton, &NauPrimaryButton::clicked, this, &NauPhysicsCollisionSettingsWidget::eventSaveRequested);
    connect(cancelButton, &NauSecondaryButton::clicked, this, &NauPhysicsCollisionSettingsWidget::eventCancelRequested);

    return container;
}

NauScrollWidgetVertical* NauPhysicsCollisionSettingsWidget::createContentScrollArea()
{
    auto contentScrollArea = new NauScrollWidgetVertical;
    contentScrollArea->setObjectName("channelsScrollArea");
    contentScrollArea->layout()->setSpacing(0);
    contentScrollArea->layout()->setContentsMargins(QMargins());
    contentScrollArea->setMinimumSize(360, 500);

    return contentScrollArea;
}

void NauPhysicsCollisionSettingsWidget::updateAllCheckbox(std::vector<NauCheckBox*> channelCheckboxes, NauCheckBox* allCheckBox)
{
    std::optional<Qt::CheckState> prevState;
    for (NauCheckBox* checkbox : channelCheckboxes) {
        const Qt::CheckState checkboxState = checkbox->checkState();

        if (prevState.value_or(checkboxState) != checkboxState) {
            allCheckBox->setCheckState(Qt::PartiallyChecked);
            return;
        }

        prevState = checkboxState;
    }

    allCheckBox->setCheckState(prevState.value_or(Qt::Unchecked));
}

void NauPhysicsCollisionSettingsWidget::showEvent(QShowEvent* event)
{
    Nau::getPhysicsCollisionChannelModel().loadChannels();
    rebuildMutableUi();
}


//** NauInspectorPhysicsCollisionSelector

NauInspectorPhysicsCollisionSelector::NauInspectorPhysicsCollisionSelector(NauWidget* parent)
    : NauUsdSingleRowPropertyBase({}, parent)
{
    setObjectName("inspectorPhysicsCollisionSelector");
    setLabel(tr("Channel"));

    m_selector = new NauPhysicsChannelComboBox; 
    m_contentLayout->addWidget(m_selector, 0, 2, 0, 4);

    connect(m_selector, &NauComboBox::currentIndexChanged, this, &NauUsdSingleRowPropertyBase::eventValueChanged);
}

NauUsdPropertyAbstract* NauInspectorPhysicsCollisionSelector::Create(const std::string&, const std::string&)
{
    return new NauInspectorPhysicsCollisionSelector();
}

PXR_NS::VtValue NauInspectorPhysicsCollisionSelector::getValue()
{
    const auto currentValue = m_selector->currentData().toInt();
    if (currentValue == -1) {
        return PXR_NS::VtValue(NauPhysicsCollisionChannelModel::defaultChannel());
    }

    return PXR_NS::VtValue(currentValue);
}

void NauInspectorPhysicsCollisionSelector::setValueInternal(const PXR_NS::VtValue& value)
{
    const int channel = value.Get<int>();
    m_selector->setCurrentIndex(m_selector->findData(channel));
}


NauPhysicsChannelComboBox::NauPhysicsChannelComboBox(NauWidget* parent)
    : NauComboBox(parent)
{
    setObjectName("physicsChannelComboBox");
    setStyleSheet("background-color:#222222");
    setFixedHeight(32);
    setEditable(true);
    fillItems();

}

void NauPhysicsChannelComboBox::showPopup()
{
    fillItems();

    NauComboBox::showPopup();
}

void NauPhysicsChannelComboBox::fillItems()
{
    const QVariant currentChannelData = currentData();

    setModel(new QStandardItemModel());

    const auto channels = Nau::getPhysicsCollisionChannelModel().channels();
    for (const auto& channel : channels)
    {
        addItem(QString::fromStdString(channel.name), channel.channel);
    }

    auto completer = new QCompleter(model(), this);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setModelSorting(QCompleter::UnsortedModel);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    setCompleter(completer);

    int prevChannelIndex = findData(currentChannelData);
    if (prevChannelIndex == -1) {
        prevChannelIndex = findData(static_cast<int>(Nau::getPhysicsCollisionChannelModel().defaultChannel()));
        if (prevChannelIndex == -1) {
            NED_ERROR("Physics collision channels does not contain a default channel? ");
        }
    }
    setCurrentIndex(prevChannelIndex);

    connect(model(), &QAbstractItemModel::rowsInserted, [this](const QModelIndex &parent, int first, int last) {
        for (int row = first; row <= last; ++row) {
            const QModelIndex index = model()->index(first, 0, parent);
            Nau::getPhysicsCollisionChannelModel().addChannel(index.data().toString().toStdString());
            Nau::getPhysicsCollisionChannelModel().saveChannels();
        }
    });
}
