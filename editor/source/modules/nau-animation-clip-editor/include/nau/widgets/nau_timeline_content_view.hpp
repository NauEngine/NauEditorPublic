// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Animation clip content view widget

#pragma once

#include "baseWidgets/nau_static_text_label.hpp"
#include "baseWidgets/nau_widget.hpp"
#include "nau/nau_timeline_utils.hpp"

class NauTimelineKeyframePool;
class NauPrimaryButton;


// ** NauTimelineKeyframe

class NauTimelineKeyframe : public NauWidget
{
    Q_OBJECT

public:
    NauTimelineKeyframe(NauWidget* parent);

    NauTimelineKeyframe(const NauTimelineKeyframe&) {};
    NauTimelineKeyframe(NauTimelineKeyframe&&) noexcept {};

    void showChildren(bool flag);
    void setTimeValue(float value) noexcept;
    void setKeyframeParent(NauTimelineKeyframe* parent) noexcept;
    void eraseChild(NauTimelineKeyframe* child);
    void addChild(NauTimelineKeyframe* child);

    [[nodiscard]]
    NauTimelineKeyframe* findChild(int propertyIndex);
    [[nodiscard]]
    NauTimelineKeyframe* keyframeParent() const noexcept { return m_parent; }
    [[nodiscard]]
    float propertyIndex() const noexcept { return m_propertyIndex; }
    [[nodiscard]]
    float timeValue() const noexcept { return m_timeValue; }
    [[nodiscard]]
    bool isSelected() const noexcept { return m_selected || m_selectedOuter; }
    [[nodiscard]]
    size_t childrenCount(bool recursive) const noexcept;
    [[nodiscard]]
    size_t visibleChildrenCount(bool recursive) const noexcept;

signals:
    void eventKeyframeSelection(NauTimelineKeyframe* keyframe);
    void eventShowKeyframeMenu(NauTimelineKeyframe* keyframe);
    void eventPositionChanged(NauTimelineKeyframe* keyframe);

protected:
    void makeSelected() noexcept;
    void makeOuterSelected(bool useRecursionForChildren) noexcept;
    void makeUnselected() noexcept;
    void makeOuterUnselected(bool useRecursionForChildren) noexcept;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    friend class NauTimelineKeyframePool;

    NauTimelineKeyframe* m_parent;
    std::vector<NauTimelineKeyframe*> m_children;

    int m_dx;
    int m_propertyIndex;
    float m_timeValue;
    bool m_selected;
    bool m_selectedOuter;
    bool m_pressed;
};


// ** NauTimelineKeyframePool

class NauTimelineKeyframePool
{
public:
    NauTimelineKeyframePool();
    ~NauTimelineKeyframePool() noexcept;

    void resize(size_t count);
    [[nodiscard]]
    NauTimelineKeyframe* create(NauWidget* owner, NauTimelineKeyframe* parent, int propertyIndex, float time);
    void free(NauTimelineKeyframe* keyframe);

private:
    enum {
        DEFAULT_POOL_SIZE = 1024
    };

    using Pool = std::vector<NauTimelineKeyframe>;
    
    std::vector<Pool> m_keyframePools;
    std::vector<NauTimelineKeyframe*> m_freeKeyframes;
    size_t m_capacity = 0;
};


class NauTimelineScrollBar;


enum class NauTimelineKeyStepReason
{
    Begin,
    Previous,
    Current,
    Next,
    End
};


// ** NauTimelineContentView
// Use the Content view to add, position, and manage the clips and markers on each track in the Timeline asset or Timeline instance.
// The selected Edit mode determines how clips and markers interact with each other when you add, position, trim, resize, or delete them.

class NauTimelineContentView : public NauWidget
{
    Q_OBJECT

public:
    NauTimelineContentView(NauWidget* parent);

    void setKeyframes(NauAnimationPropertyListPtr propertiesListPtr);

    [[nodiscard]]
    float timeValue(NauTimelineKeyStepReason reason);
    [[nodiscard]]
    float currentTime() const noexcept { return m_currentTime; }
    void setCurrentTime(float time) noexcept;
    void setCreationAvailable(bool available);
    void setKeyframesExpanded(int propertyIndex, bool flag);
    void resetZoom();

signals:
    void eventClipCreated();
    void eventCurrentTimeChanged(float time, bool isManual);
    void eventKeyframeDeleted(int propertyIndex, float time);
    void eventKeyframeChanged(int propertyIndex, float timeOld, float timeNew);

protected:
    enum BaseConstants
    {
        KEYFRAME_ROOT_OFFSET = 8,
        KEYFRAME_OFFSET = 10,
        HEADER_HEIGHT = 100,
        TIMELINE_HEIGHT = 48,
        TRACK_CELL_SIZE = 32,
        SAFE_AREA_WIDTH = TRACK_CELL_SIZE * 2,
        TIMELINE_SECTION_SIZE = 5,
        SECTION_SMALL_HEIGHT = 8,
        STEP_REDUCTION = 1,
        SECTION_DECREASE_COUNT = TRACK_CELL_SIZE / 2 / STEP_REDUCTION,
        TIMELINE_SCROLL_OFFSET = 6,
    };

    struct StepMeasures
    {
        float stepTime;
        int   stepWidth;
    };

    [[nodiscard]]
    static float computeStepTime(int decreaseLevel, float baseStepTime) noexcept;
    [[nodiscard]]
    static int computeStepWidth(int decreaseLevel) noexcept;
    [[nodiscard]]
    static float computeDecreaseLevelMultiplier(int decreseLevel) noexcept;

    [[nodiscard]]
    StepMeasures computeConstants() const noexcept;

    void setCurrentTimeInternal(float time, bool isManual);
    void updateTimelineScroller(int decreaseLevelOld, bool useAspect) noexcept;
    void setSelectedKeyframe(NauTimelineKeyframe* keyframe) noexcept;
    void processKeyframeMove(NauTimelineKeyframe* keyframe) noexcept;
    void updateKeyframePosition(NauTimelineKeyframe* keyframe) noexcept;
    void updateKeyframesPositions() noexcept;
    void updateTimeline(int decreaseLevelOld);
    void showKeyframeMenu(NauTimelineKeyframe* keyframe);

    void updateCurrentTime(float currentCursorPositionX) noexcept;
    void createKeyframeFromProperties();
    void deleteKeyframe();
    void clearKeyframes();

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    struct PropertyData
    {
        std::vector<NauTimelineKeyframe*> keyframeList;
        bool expandedFlag = false;
    };

    NauPrimaryButton* m_createClipButton;
    NauStaticTextLabel* m_createClipText;
    NauMenu* m_keyframeMenu;
    QList<QLine> m_drawingLines;
    QList<QString> m_drawingText;
    std::map<float, NauTimelineKeyframe*> m_rootKeyframes;
    std::vector<PropertyData> m_propertyDataList;
    NauTimelineKeyframe* m_selectedKeyframe;
    NauTimelineKeyframe* m_keyframeForMenu;

    NauTimelineScrollBar* m_timelineScroller;
    NauTimelineScrollBar* m_trackScroller;
    NauWidget* m_createClipContainer;

    NauAnimationPropertyListPtr m_propertiesListPtr;

    NauTimelineKeyframePool m_keyframePool;
    std::unique_ptr<QTimer> m_timer;

    uint32_t m_decreaseLevel;
    float m_baseStepTime;
    float m_animationStart;
    float m_animationEnd;
    float m_currentTime;
    bool m_stepEnabled;
    bool m_timelinePressed;
};