// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/widgets/nau_timeline_window.hpp"
#include "nau/widgets/nau_timeline_content_view.hpp"
#include "nau/widgets/nau_timeline_parameters.hpp"
#include "nau/widgets/nau_timeline_playback.hpp"
#include "nau/widgets/nau_timeline_track_list.hpp"


// ** NauTimelineWindow

NauTimelineWindow::NauTimelineWindow()
    : NauWidget(nullptr)
    , m_playback(new NauTimelinePlayback(this))
    , m_trackList(new NauTimelineTrackList(this))
    , m_parameters(new NauTimelineParameters(this))
    , m_contentView(new NauTimelineContentView(this))
{
    constexpr int MAX_WIDGET_WIDTH = 9999;

    auto* containerLayout = new NauLayoutVertical;
    containerLayout->setSpacing(1);
    containerLayout->addWidget(m_playback);
    containerLayout->addSpacing(3);
    containerLayout->addWidget(m_trackList);
    containerLayout->addWidget(m_parameters);

    const int minWidth = std::max(m_playback->minimumWidth(), m_parameters->minimumWidth());
    auto* container = new NauWidget(this);
    container->setLayout(containerLayout);
    container->setMinimumWidth(minWidth);

    auto* splitter = new NauSplitter(this);
    splitter->setStyleSheet("background-color:#00000000");
    splitter->setChildrenCollapsible(false);
    splitter->setOrientation(Qt::Horizontal);
    splitter->addWidget(container);
    splitter->addWidget(m_contentView);
    splitter->setSizes({ container->minimumWidth(), MAX_WIDGET_WIDTH });

    setLayout(new NauLayoutHorizontal);
    layout()->addWidget(splitter);

    connect(&m_playback->timelinePlayButton(),  &QAbstractButton::pressed, this, &NauTimelineWindow::eventPlayButtonPressed);
    connect(&m_playback->timelinePauseButton(), &QAbstractButton::pressed, this, &NauTimelineWindow::eventPauseButtonPressed);
    connect(&m_playback->timelineStopButton(),  &QAbstractButton::pressed, this, &NauTimelineWindow::eventStopButtonPressed);
    connect(&m_playback->timelineStartButton(), &QAbstractButton::pressed, [this] {
        const float time = m_contentView->timeValue(NauTimelineKeyStepReason::Begin);
        emit eventManualTimeChanged(time);
    });
    connect(&m_playback->timelineEndButton(),   &QAbstractButton::pressed, [this] {
        const float time = m_contentView->timeValue(NauTimelineKeyStepReason::End);
        emit eventManualTimeChanged(time);
    });
    connect(&m_playback->previousKeyButton(), &QAbstractButton::pressed, [this] {
        const float time = m_contentView->timeValue(NauTimelineKeyStepReason::Previous);
        emit eventManualTimeChanged(time);
    });
    connect(&m_playback->nextKeyButton(), &QAbstractButton::pressed, [this] {
        const float time = m_contentView->timeValue(NauTimelineKeyStepReason::Next);
        emit eventManualTimeChanged(time);
    });

    connect(m_trackList, &NauTimelineTrackList::eventItemExpanded, m_contentView, &NauTimelineContentView::setKeyframesExpanded);
    connect(m_trackList, &NauTimelineTrackList::eventDeleteProperty, this, &NauTimelineWindow::eventTrackDeleted);
    connect(m_trackList, &NauTimelineTrackList::eventAddKeyframe, [this](int propertyIndex) {
        emit eventAddKeyframe(propertyIndex, m_contentView->currentTime());
    });
    connect(m_trackList, &NauTimelineTrackList::eventPropertyChanged, [this](int propertyIndex, const NauAnimationPropertyData& data) {
        emit eventPropertyChanged(propertyIndex, data, m_contentView->currentTime());
    });

    connect(m_contentView, &NauTimelineContentView::eventKeyframeChanged, this, &NauTimelineWindow::eventKeyframeChanged);
    connect(m_contentView, &NauTimelineContentView::eventKeyframeDeleted, this, &NauTimelineWindow::eventKeyframeDeleted);
    connect(m_contentView, &NauTimelineContentView::eventClipCreated, this, &NauTimelineWindow::eventCreateControllerButtonPressed);
    connect(m_contentView, &NauTimelineContentView::eventCurrentTimeChanged, [this](float time, bool isManual) {
        m_playback->setCurrentTime(time);
        m_trackList->setCurrentTime(time);
        if (isManual) {
            emit eventManualTimeChanged(time);
        }
    });

    connect(&m_trackList->headerWidget(), &NauTimelineTrackListHeader::eventClipSwitched, this, &NauTimelineWindow::eventClipSwitched);
    connect(&m_trackList->headerWidget(), &NauTimelineTrackListHeader::eventAddedProperty, this, &NauTimelineWindow::eventTrackAdded);
}

void NauTimelineWindow::setCurrentTime(float time)
{
    m_contentView->setCurrentTime(time);
}

void NauTimelineWindow::setClipProperties(NauAnimationPropertyListPtr propertyList)
{
    m_playback->reset();
    m_contentView->setKeyframes(propertyList);
    m_trackList->updateTrackList(std::move(propertyList), m_contentView->currentTime());
}

void NauTimelineWindow::setClipNameList(const NauAnimationNameList& nameList, int currentNameIndex)
{
    m_playback->reset();
    m_trackList->headerWidget().setClipNameList(nameList, currentNameIndex);
    m_contentView->resetZoom();
}

void NauTimelineWindow::setCreationAvailable(bool available)
{
    m_contentView->setCreationAvailable(available);
}

void NauTimelineWindow::stopPlayback()
{
    m_playback->timelineStopButton().pressed();
}

void NauTimelineWindow::paintEvent(QPaintEvent* event)
{
    QPainter painter{ this };
    painter.fillRect(QRectF(0, 0, width(), height()), QColorConstants::Black);
}