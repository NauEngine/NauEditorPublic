// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Animation timeline window

#pragma once

#include "nau/nau_timeline_utils.hpp"

#include "baseWidgets/nau_widget.hpp"


class NauTimelinePlayback;
class NauTimelineTrackList;
class NauTimelineParameters;
class NauTimelineContentView;


// ** NauTimelineWindow
// The Timeline window provides dedicated areas for controlling playback, selecting a Timeline instance,
// setting Timeline options, managing tracks, adding markers, and managing clips.

class NauTimelineWindow : public NauWidget
{
    Q_OBJECT

public:
    NauTimelineWindow();

    void setCurrentTime(float time);
    void setClipProperties(NauAnimationPropertyListPtr propertyList);
    void setClipNameList(const NauAnimationNameList& nameList, int currentNameIndex);
    void setCreationAvailable(bool available);
    void stopPlayback();

signals:
    void eventPlayButtonPressed();
    void eventPauseButtonPressed();
    void eventStopButtonPressed();
    void eventCreateControllerButtonPressed();
    void eventManualTimeChanged(float time);
    void eventClipSwitched(int clipIndex);
    void eventTrackAdded(int propertyIndex);
    void eventTrackDeleted(int propertyIndex);
    void eventAddKeyframe(int propertyIndex, float time);
    void eventKeyframeDeleted(int propertyIndex, float time);
    void eventKeyframeChanged(int propertyIndex, float timeOld, float timeNew);
    void eventPropertyChanged(int propertyIndex, const NauAnimationPropertyData& data, float time);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    NauTimelinePlayback* m_playback;
    NauTimelineTrackList* m_trackList;
    NauTimelineParameters* m_parameters;
    NauTimelineContentView* m_contentView;
};

