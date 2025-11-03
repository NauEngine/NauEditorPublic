// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/widgets/nau_timeline_content_view.hpp"
#include "nau/widgets/nau_common_timeline_widgets.hpp"
#include "nau/nau_timeline_utils.hpp"

#include "themes/nau_theme.hpp"
#include "nau_utils.hpp"

#include <QTimer>


// ** NauTimelineKeyframe

NauTimelineKeyframe::NauTimelineKeyframe(NauWidget* parent)
    : NauWidget(parent)
    , m_dx(0)
    , m_parent(nullptr)
    , m_propertyIndex(-1)
    , m_timeValue(0.f)
    , m_selected(false)
    , m_selectedOuter(false)
    , m_pressed(false)
{
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    setFixedSize(12, 12);
}

void NauTimelineKeyframe::showChildren(bool flag)
{
    for (NauTimelineKeyframe* child : m_children) {
        child->showChildren(flag);
        flag ? child->show() : child->hide();
    }
}

void NauTimelineKeyframe::setTimeValue(float value) noexcept
{
    m_timeValue = value;
    for (NauTimelineKeyframe* child : m_children) {
        child->setTimeValue(value);
    }
}

void NauTimelineKeyframe::setKeyframeParent(NauTimelineKeyframe* parent) noexcept
{
    m_parent = parent;
}

void NauTimelineKeyframe::eraseChild(NauTimelineKeyframe* child)
{
    auto it = std::find(m_children.begin(), m_children.end(), child);
    m_children.erase(it);
}

void NauTimelineKeyframe::addChild(NauTimelineKeyframe* child)
{
    m_children.emplace_back(child);
}

NauTimelineKeyframe* NauTimelineKeyframe::findChild(int propertyIndex)
{
    for (NauTimelineKeyframe* child : m_children) {
        if (child->propertyIndex() == propertyIndex) {
            return child;
        }
        if (NauTimelineKeyframe* subChild = child->findChild(propertyIndex)) {
            return subChild;
        }
    }
    return nullptr;
}

size_t NauTimelineKeyframe::childrenCount(bool recursive) const noexcept
{
    size_t count = m_children.size();
    if (recursive) {
        for (NauTimelineKeyframe* child : m_children) {
            count += child->childrenCount(recursive);
        }
    }
    return count;
}

size_t NauTimelineKeyframe::visibleChildrenCount(bool recursive) const noexcept
{
    size_t count = 0;
    for (NauTimelineKeyframe* child : m_children) {
        count += child->isVisible();
        if (recursive && child->isVisible()) {
            count += child->childrenCount(recursive);
        }
    }
    return count;
}

void NauTimelineKeyframe::makeSelected() noexcept
{
    if (!isSelected()) {
        NauTimelineKeyframe* parent = m_parent;
        while (parent != nullptr) {
            parent->makeOuterSelected(false);
            parent = parent->m_parent;
        }
        for (NauTimelineKeyframe* child : m_children) {
            child->makeOuterSelected(true);
        }
    }
    m_selected = true;
}

void NauTimelineKeyframe::makeOuterSelected(bool useRecursionForChildren) noexcept
{
    m_selectedOuter = true;
    if (!useRecursionForChildren) {
        return;
    }
    for (NauTimelineKeyframe* child : m_children) {
        child->makeOuterSelected(true);
    }
}

void NauTimelineKeyframe::makeUnselected() noexcept
{
    if (!isSelected()) {
        return;
    }
    m_selected = false;
    m_selectedOuter = false;

    NauTimelineKeyframe* parent = m_parent;
    while (parent != nullptr) {
        parent->makeOuterUnselected(false);
        parent = parent->m_parent;
    }
    for (NauTimelineKeyframe* child : m_children) {
        child->makeOuterUnselected(true);
    }
}

void NauTimelineKeyframe::makeOuterUnselected(bool useRecursionForChildren) noexcept
{
    m_selectedOuter = false;
    if (!useRecursionForChildren) {
        return;
    }
    for (NauTimelineKeyframe* child : m_children) {
        child->makeOuterUnselected(true);
    }
}

void NauTimelineKeyframe::mousePressEvent(QMouseEvent* event)
{
    if (!m_pressed) {
        m_dx = -event->pos().x();
    }
    m_pressed = true;
    makeSelected();
    emit eventKeyframeSelection(this);
    if (event->button() == Qt::MouseButton::RightButton) {
        emit eventShowKeyframeMenu(this);
    }

    NauWidget::mousePressEvent(event);
}

void NauTimelineKeyframe::mouseReleaseEvent(QMouseEvent* event)
{
    m_pressed = false;
    if (m_dx != 0) {
        makeUnselected();
        move(pos().x() + width() / 2, pos().y());
        emit eventPositionChanged(this);
    }
    NauWidget::mouseReleaseEvent(event);
}

void NauTimelineKeyframe::mouseMoveEvent(QMouseEvent* event)
{
    auto [oldX, oldY] = pos();
    const int dx = event->pos().x();
    m_dx += dx;
    move(std::max(64, oldX + dx) - width() / 2, oldY);
    NauWidget::mouseMoveEvent(event);
}

void NauTimelineKeyframe::focusOutEvent(QFocusEvent* event)
{
    makeUnselected();
    emit eventKeyframeSelection(nullptr);
    NauWidget::focusOutEvent(event);
}

void NauTimelineKeyframe::moveEvent(QMoveEvent* event)
{
    if (m_selected) {
        NauTimelineKeyframe* parent = m_parent;
        while (parent != nullptr) {
            parent->move(event->pos().x(), parent->pos().y());
            parent = parent->m_parent;
        }
    }
    if (!event->oldPos().isNull()) {
        const int diffY = (event->pos() - event->oldPos()).y();
        for (NauTimelineKeyframe* child : m_children) {
            child->move(event->pos().x(), child->pos().y() + diffY);
        }
    }
    NauWidget::moveEvent(event);
}

void NauTimelineKeyframe::paintEvent(QPaintEvent* event)
{
    const NauPalette palette = Nau::Theme::current().paletteTimelineKeyframe();
    const NauPalette::State state = isSelected() ? NauPalette::Selected : NauPalette::Normal;

    const int halfWidth = width() / 2;
    const int halfHeight = height() / 2;
    const int quarterWidth = halfWidth / 2;
    const int quarterHeight = halfHeight / 2;

    QPainterPath pathOuter;
    pathOuter.moveTo(0, halfHeight);
    pathOuter.lineTo(halfWidth, 0);
    pathOuter.lineTo(width(), halfHeight);
    pathOuter.lineTo(halfWidth, height());
    pathOuter.lineTo(0, halfHeight);

    QPainterPath pathInner;
    pathInner.moveTo(quarterWidth, halfHeight);
    pathInner.lineTo(halfWidth, quarterHeight);
    pathInner.lineTo(halfWidth + quarterWidth, halfHeight);
    pathInner.lineTo(halfWidth, halfHeight + quarterHeight);
    pathInner.lineTo(quarterWidth, halfHeight);

    QPainter painter{ this };
    painter.fillPath(pathOuter, palette.color(NauPalette::Role::Background, state));
    painter.fillPath(pathInner, palette.color(NauPalette::Role::Foreground, state));
}


// ** NauTimelineKeyframePool

NauTimelineKeyframePool::NauTimelineKeyframePool() = default;

NauTimelineKeyframePool::~NauTimelineKeyframePool() noexcept = default;

void NauTimelineKeyframePool::resize(size_t count)
{
    if (m_capacity < count) {
        auto& pool = m_keyframePools.emplace_back();
        pool.reserve(std::max<size_t>(DEFAULT_POOL_SIZE, count - m_capacity));
        m_capacity += pool.capacity();
    }
}

NauTimelineKeyframe* NauTimelineKeyframePool::create(NauWidget* owner, NauTimelineKeyframe* parent, int propertyIndex, float time)
{
    NauTimelineKeyframe* keyframe = nullptr;
    if (m_freeKeyframes.empty()) {
        for (auto& pool : m_keyframePools) {
            if (pool.size() < pool.capacity()) {
                keyframe = &pool.emplace_back(owner);
                break;
            }
        }
    } else {
        keyframe = m_freeKeyframes.back();
        m_freeKeyframes.pop_back();
    }
    if (keyframe != nullptr) {
        keyframe->setParent(owner);
        keyframe->m_timeValue = time;
        keyframe->m_propertyIndex = propertyIndex;
        keyframe->show();
        if (parent != nullptr) {
            parent->m_children.emplace_back(keyframe);
            keyframe->m_parent = parent;
            keyframe->move(parent->pos());
        }
    }
    return keyframe;
}

void NauTimelineKeyframePool::free(NauTimelineKeyframe* keyframe)
{
    if (keyframe == nullptr) {
        return;
    }
    m_freeKeyframes.emplace_back(keyframe);

    for (NauTimelineKeyframe* child : keyframe->m_children) {
        free(child);
    }
    keyframe->m_children.clear();
    keyframe->m_parent = nullptr;
    keyframe->hide();
}


// ** NauTimelineContentView

NauTimelineContentView::NauTimelineContentView(NauWidget* parent)
    : NauWidget(parent)
    , m_keyframeMenu(new NauMenu(this))
    , m_selectedKeyframe(nullptr)
    , m_timelineScroller(new NauTimelineScrollBar(this, Qt::Horizontal))
    , m_trackScroller(new NauTimelineScrollBar(this, Qt::Vertical))
    , m_createClipContainer(new NauWidget(this))
    , m_timer(std::make_unique<QTimer>(this))
    , m_decreaseLevel(96)
    , m_baseStepTime(1.f / 60.f)
    , m_animationStart(0.f)
    , m_animationEnd(0.f)
    , m_currentTime(0.f)
    , m_stepEnabled(false)
    , m_timelinePressed(false)
{
    {
        constexpr QSize BUTTON_SIZE{ 74, 28 };
        constexpr QSize BUTTON_ROUND{ BUTTON_SIZE.height() / 2, BUTTON_SIZE.height() / 2 };

        auto* createClipButton = new NauPrimaryButton(this);
        createClipButton->setText(QObject::tr("Create"));
        createClipButton->setContentsMargins(16, 6, 16, 6);
        createClipButton->setFixedHeight(BUTTON_SIZE.height());
        createClipButton->setMinimumWidth(BUTTON_SIZE.width());
        createClipButton->setRound(BUTTON_ROUND);

        connect(createClipButton, &QAbstractButton::pressed, this, &NauTimelineContentView::eventClipCreated);

        auto* text = new NauStaticTextLabel(QObject::tr("To begin animating GameObject, create an animation clip."), m_createClipContainer);

        auto* layout = new NauLayoutVertical;
        layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));
        layout->addWidget(text);
        layout->setAlignment(text, Qt::AlignHCenter);
        layout->addItem(new QSpacerItem(0, 16, QSizePolicy::Fixed, QSizePolicy::Fixed));
        layout->addWidget(createClipButton);
        layout->setAlignment(createClipButton, Qt::AlignHCenter);
        layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));

        m_createClipContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_createClipContainer->setLayout(layout);
    }
    setLayout(new NauLayoutVertical);
    layout()->addItem(new QSpacerItem(0, 36, QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout()->addWidget(m_createClipContainer);

    m_timelineScroller->setUseRange(true);
    m_trackScroller->hide();
    // TODO: Fix scroller
    /*connect(m_timelineScroller, &NauTimelineScrollBar::eventScrollerChanged, [this]() {
        const float multiplier = 1.f / m_timelineScroller->viewLength();
        int exponent = static_cast<int>(std::floor(std::log2f(multiplier))) - 1;
        const float timelineLength = static_cast<float>(width()) * multiplier;
        const int decreaseLevelOld = m_decreaseLevel;
        for (; exponent >= 0; --exponent) {
            for (int step = SECTION_DECREASE_COUNT - 1; step >= 0; --step) {
                constexpr float stepReductionValue = static_cast<float>(STEP_REDUCTION) / static_cast<float>(SECTION_DECREASE_COUNT);
                if (std::powf(2.f, exponent) * (1.f + step) * stepReductionValue <= timelineLength) {
                    m_decreaseLevel = exponent * SECTION_DECREASE_COUNT + step;
                    updateTimeline(decreaseLevelOld);
                    return;
                }
            }
        }
        resetZoom();
        updateTimeline(decreaseLevelOld);
    });*/
    connect(m_timelineScroller, &NauTimelineScrollBar::eventScrollerChangeFinished, [this]() {
        const float animationLength = static_cast<float>(nau::CalculateTimelineSizeInPixels(m_animationEnd, m_baseStepTime, TRACK_CELL_SIZE));
        const float timelineLength = static_cast<float>(width()) / m_timelineScroller->viewLength();
        m_timelineScroller->shrinkScroller(animationLength / timelineLength);
        updateKeyframesPositions();
        update();
    });
    auto* action = m_keyframeMenu->addAction(tr("Delete"));
    connect(action, &QAction::triggered, this, &NauTimelineContentView::deleteKeyframe);
    connect(m_timer.get(), &QTimer::timeout, this, &NauTimelineContentView::createKeyframeFromProperties);
    resetZoom();
    updateTimeline(m_decreaseLevel);
}

void NauTimelineContentView::setKeyframes(NauAnimationPropertyListPtr propertiesListPtr)
{
    if (propertiesListPtr->refilled()) {
        clearKeyframes();
    }

    m_propertiesListPtr = std::move(propertiesListPtr);
    m_propertiesListPtr->empty() ? m_createClipContainer->show() : m_createClipContainer->hide();
    m_propertyDataList.resize(m_propertiesListPtr->size());

    m_animationStart = m_propertiesListPtr->startAnimation();
    m_animationEnd = m_propertiesListPtr->endAnimation();
    m_baseStepTime = m_propertiesListPtr->frameDuration();

    size_t keyframesCount = 0;
    m_propertiesListPtr->forEach([this, &keyframesCount](const NauAnimationProperty& property) {
        if (!property.selected()) {
            return;
        }
        ++keyframesCount;
        for (double time : property.timeSamples()) {
            if (property.type() == NauAnimationTrackDataType::Vec3) {
                keyframesCount += 4;
            } else if (property.type() == NauAnimationTrackDataType::Quat) {
                keyframesCount += 5;
            } else {
                ++keyframesCount;
            }
        }
    });
    m_keyframePool.resize(keyframesCount);
    m_timer->start();
    update();
}

float NauTimelineContentView::timeValue(NauTimelineKeyStepReason reason)
{
    if (m_rootKeyframes.empty()) {
        return m_currentTime;
    }
    switch (reason) {
        case NauTimelineKeyStepReason::Begin: {
            return m_rootKeyframes.begin()->first;
        }
        case NauTimelineKeyStepReason::Previous: {
            auto it = m_rootKeyframes.lower_bound(m_currentTime);
            if (it == m_rootKeyframes.end()) {
                return m_rootKeyframes.rbegin()->first;
            }
            return it == m_rootKeyframes.begin() ? m_currentTime : (--it)->first;
        }
        case NauTimelineKeyStepReason::Current: {
            return m_currentTime;
        }
        case NauTimelineKeyStepReason::Next: {
            auto it = m_rootKeyframes.upper_bound(m_currentTime);
            return it == m_rootKeyframes.end() ? m_currentTime : it->first;
        }
        case NauTimelineKeyStepReason::End: {
            return m_rootKeyframes.rbegin()->first;
        }
    }
    return -1.f;
}

void NauTimelineContentView::setCurrentTime(float time) noexcept
{
    setCurrentTimeInternal(time, false);
}

void NauTimelineContentView::setKeyframesExpanded(int propertyIndex, bool flag)
{
    NauAnimationProperty* property = m_propertiesListPtr->propertyByIndex(propertyIndex);
    if (m_propertyDataList[propertyIndex].expandedFlag == flag) {
        return;
    }
    m_propertyDataList[propertyIndex].expandedFlag = flag;
    // todo: make abstract
    size_t childrenCount = 0;
    if (property->type() == NauAnimationTrackDataType::Vec3) {
        childrenCount = 3;
    } else if (property->type() == NauAnimationTrackDataType::Quat) {
        childrenCount = 4;
    }

    auto& keyframeList = m_propertyDataList[propertyIndex].keyframeList;
    if (!flag && !keyframeList.empty()) {
        childrenCount = keyframeList.front()->visibleChildrenCount(true);
    }
    for (NauTimelineKeyframe* keyframe: keyframeList) {
        keyframe->showChildren(flag);
    }
    if (flag && !keyframeList.empty()) {
        childrenCount = keyframeList.front()->visibleChildrenCount(true);
    }

    const int offsetY = (flag ? 1 : -1) * TRACK_CELL_SIZE * static_cast<int>(childrenCount);
    for (int index = propertyIndex + 1, count = m_propertyDataList.size(); index < count;  ++index) {
        for (NauTimelineKeyframe* keyframe : m_propertyDataList[index].keyframeList) {
            const QPoint position = keyframe->pos();
            keyframe->move(position.x(), position.y() + offsetY);
        }
    }
}

void NauTimelineContentView::resetZoom()
{
    m_baseStepTime = 1.f / 60.f;
    m_decreaseLevel = 112u;
    if (m_propertiesListPtr) {
        m_baseStepTime = m_propertiesListPtr->frameDuration();
        const float time = m_propertiesListPtr->endAnimation();
        while (((3.f * computeStepTime(m_decreaseLevel, m_baseStepTime) * static_cast<float>(TIMELINE_SECTION_SIZE)) < time) && (m_decreaseLevel < 160)) {
            m_decreaseLevel += SECTION_DECREASE_COUNT;
        }
    }
}

float NauTimelineContentView::computeStepTime(int decreaseLevel, float baseStepTime) noexcept
{
    return baseStepTime * std::powf(2.f, static_cast<float>(decreaseLevel / SECTION_DECREASE_COUNT));
}

int NauTimelineContentView::computeStepWidth(int decreaseLevel) noexcept
{
    return TRACK_CELL_SIZE - static_cast<int>(decreaseLevel % SECTION_DECREASE_COUNT) * STEP_REDUCTION;
}

float NauTimelineContentView::computeDecreaseLevelMultiplier(int decreseLevel) noexcept
{
    constexpr float stepReductionValue = static_cast<float>(STEP_REDUCTION) / static_cast<float>(SECTION_DECREASE_COUNT);

    const float pow = std::powf(2.f, static_cast<float>(decreseLevel / SECTION_DECREASE_COUNT));
    const float step = 1.f + static_cast<int>(decreseLevel % SECTION_DECREASE_COUNT) * stepReductionValue;

    return pow * step;
}

NauTimelineContentView::StepMeasures NauTimelineContentView::computeConstants() const noexcept
{
    const float stepTime = computeStepTime(m_decreaseLevel, m_baseStepTime);
    const int   stepWidth = computeStepWidth(m_decreaseLevel);
    return { stepTime, stepWidth };
}

void NauTimelineContentView::setCurrentTimeInternal(float time, bool isManual)
{
    if (m_currentTime != time) {
        m_currentTime = time;
        emit eventCurrentTimeChanged(time, isManual);
        update();
    }
}

void NauTimelineContentView::updateTimelineScroller(int decreaseLevelOld, bool useAspect) noexcept
{
    const float animationLength = static_cast<float>(nau::CalculateTimelineSizeInPixels(m_animationEnd, m_baseStepTime, TRACK_CELL_SIZE));
    const float multiplierOld = computeDecreaseLevelMultiplier(decreaseLevelOld);
    const float multiplierNew = computeDecreaseLevelMultiplier(m_decreaseLevel);
    const float multiplierAspect = multiplierNew / multiplierOld;

    const float position = m_timelineScroller->position();
    const float widgetWidth = static_cast<float>(width());
    const float timelineLengthOld = std::round(std::max(widgetWidth * multiplierOld, widgetWidth / m_timelineScroller->viewLength()));
    const float timelineLengthNew = std::round(timelineLengthOld * multiplierAspect);
    const float timelineLengthInv = 1.f / std::max(timelineLengthOld, timelineLengthNew);

    float aspect = 0.f;
    if (useAspect) {
        const float stepWidthOld = static_cast<float>(computeStepWidth(decreaseLevelOld));
        const float maxStepCount = std::max(widgetWidth / stepWidthOld, 1.f);
        const float newSafeAreaWidth = static_cast<float>(SAFE_AREA_WIDTH) - std::round(maxStepCount * position * stepWidthOld);
        float positionX = static_cast<float>(mapFromGlobal(QCursor::pos()).x());
        if (newSafeAreaWidth > 0.f) {
            const float scale = widgetWidth / (widgetWidth - newSafeAreaWidth);
            positionX = std::max(0.f, positionX - newSafeAreaWidth) * scale;
        }
        aspect = positionX / widgetWidth;
    }
    m_timelineScroller->setViewLength(widgetWidth * timelineLengthInv);
    m_timelineScroller->setPosition(position + (1.f - multiplierAspect) * aspect);

    m_timelineScroller->shrinkScroller(animationLength * timelineLengthInv);
}

void NauTimelineContentView::setSelectedKeyframe(NauTimelineKeyframe* keyframe) noexcept
{
    m_selectedKeyframe = keyframe;
}

void NauTimelineContentView::processKeyframeMove(NauTimelineKeyframe* keyframe) noexcept
{
    if (m_selectedKeyframe == nullptr) {
        return;
    }

    const auto [timeStep, trackWidth] = computeConstants();
    const float widgetWidth = static_cast<float>(width());
    const float timelineLength = std::round(widgetWidth / m_timelineScroller->viewLength());
    const float multiplier = computeDecreaseLevelMultiplier(m_decreaseLevel);
    const int timelinePosition = static_cast<int>(std::round(timelineLength * m_timelineScroller->position() / multiplier)) - SAFE_AREA_WIDTH;
    const int safeArea = std::max(-timelinePosition, 0);
    const float timeOffset = nau::CalculateTimeFromPosition(std::max(0, timelinePosition - safeArea), timeStep, trackWidth);

    const float mouseOnContentArea = std::clamp<float>(static_cast<float>(keyframe->pos().x() - safeArea), 0.f, widgetWidth);
    const float newTime = timeOffset + timeStep * (mouseOnContentArea / static_cast<float>(trackWidth));
    const float oldTime = m_selectedKeyframe->timeValue();

    if (newTime == m_selectedKeyframe->timeValue()) {
        return;
    }

    int propertyIndex = m_selectedKeyframe->propertyIndex();
    NauTimelineKeyframe* moveKeyframe = m_selectedKeyframe->keyframeParent();

    moveKeyframe = moveKeyframe && (moveKeyframe->propertyIndex() == propertyIndex) ? moveKeyframe : m_selectedKeyframe;
    if (NauTimelineKeyframe* parent = moveKeyframe->keyframeParent(); (parent != nullptr) && (parent->propertyIndex() == -1) && (parent->childrenCount(false) == 1)) {
        auto it = m_rootKeyframes.find(oldTime);
        if (m_rootKeyframes.contains(newTime)) {
            m_keyframePool.free(moveKeyframe);
            m_rootKeyframes.erase(oldTime);
        } else {
            moveKeyframe = parent;
        }
    } else if (moveKeyframe->keyframeParent()) {
        moveKeyframe->keyframeParent()->eraseChild(moveKeyframe);
    }
    moveKeyframe->setTimeValue(newTime);
    auto keyframeIt = m_rootKeyframes.find(newTime);
    if ((keyframeIt == m_rootKeyframes.end()) && (moveKeyframe->propertyIndex() != -1)) {
        constexpr int ROOT_OFFSET_Y = HEADER_HEIGHT - KEYFRAME_ROOT_OFFSET;
        keyframeIt = m_rootKeyframes.emplace(newTime, m_keyframePool.create(this, nullptr, -1, newTime)).first;
        NauTimelineKeyframe* rootKeyframe = keyframeIt->second;
        rootKeyframe->move(moveKeyframe->pos().x(), ROOT_OFFSET_Y - keyframe->height());
        rootKeyframe->addChild(moveKeyframe);
        moveKeyframe->setKeyframeParent(rootKeyframe);
        connect(rootKeyframe, &NauTimelineKeyframe::eventShowKeyframeMenu, this, &NauTimelineContentView::showKeyframeMenu);
        connect(rootKeyframe, &NauTimelineKeyframe::eventPositionChanged, this, &NauTimelineContentView::processKeyframeMove);
        connect(rootKeyframe, &NauTimelineKeyframe::eventKeyframeSelection, this, &NauTimelineContentView::setSelectedKeyframe);
    } else if (keyframeIt != m_rootKeyframes.end()) {
        if (keyframeIt->second->findChild(propertyIndex)) {
            m_keyframePool.free(moveKeyframe);
        } else {
            keyframeIt->second->addChild(moveKeyframe);
        }
    } else if (moveKeyframe->propertyIndex() == -1) {
        m_rootKeyframes.erase(oldTime);
        m_rootKeyframes.emplace(newTime, moveKeyframe);
    }

    m_selectedKeyframe = nullptr;

    emit eventKeyframeChanged(propertyIndex, oldTime, newTime);
}

void NauTimelineContentView::updateKeyframePosition(NauTimelineKeyframe* keyframe) noexcept
{
    const auto [timeStep, trackWidth] = computeConstants();

    const float widgetWidth = static_cast<float>(width());
    const float timelineLength = std::round(widgetWidth / m_timelineScroller->viewLength());
    const float multiplier = computeDecreaseLevelMultiplier(m_decreaseLevel);
    const int timelinePosition = static_cast<int>(std::round(timelineLength * m_timelineScroller->position() / multiplier)) - SAFE_AREA_WIDTH;
    const int safeArea = std::max(-timelinePosition, 0);
    const float timeOffset = nau::CalculateTimeFromPosition(std::max(0, timelinePosition - safeArea), timeStep, trackWidth);

    const int x = safeArea + nau::CalculateTimelineSizeInPixels(keyframe->timeValue() - timeOffset, timeStep, trackWidth) - keyframe->width() / 2;
    keyframe->move(x, keyframe->pos().y());
}

void NauTimelineContentView::updateKeyframesPositions() noexcept
{
    for (auto&& [time, keyframe] : m_rootKeyframes) {
        updateKeyframePosition(keyframe);
    }
}

void NauTimelineContentView::updateTimeline(int decreaseLevelOld)
{
    if (decreaseLevelOld != m_decreaseLevel) {
        updateTimelineScroller(decreaseLevelOld, true);
    }
    updateKeyframesPositions();
    update();
}

void NauTimelineContentView::showKeyframeMenu(NauTimelineKeyframe* keyframe)
{
    const auto parentWidgetPosition = keyframe->mapToGlobal(QPointF(0, 0)).toPoint();
    const auto correctWidgetPosition = Nau::Utils::Widget::fitWidgetIntoScreen(m_keyframeMenu->sizeHint(), parentWidgetPosition);
    m_keyframeMenu->popup(correctWidgetPosition);
    m_keyframeForMenu = keyframe;
}

void NauTimelineContentView::updateCurrentTime(float currentCursorPositionX) noexcept
{
    if (m_timelinePressed) {
        const auto [timeStep, trackWidth] = computeConstants();

        const float widgetWidth = static_cast<float>(width());
        const float timelineLength = std::round(widgetWidth / m_timelineScroller->viewLength());
        const float multiplier = computeDecreaseLevelMultiplier(m_decreaseLevel);
        const int timelinePosition = static_cast<int>(std::round(timelineLength * m_timelineScroller->position() / multiplier)) - SAFE_AREA_WIDTH;
        const int safeArea = std::max(-timelinePosition, 0);
        const float timeOffset = nau::CalculateTimeFromPosition(std::max(0, timelinePosition - safeArea), timeStep, trackWidth);

        const float mouseOnContentArea = std::clamp<float>(currentCursorPositionX - static_cast<float>(safeArea), 0.f, width());
        const float newTime = timeOffset + timeStep * (mouseOnContentArea / static_cast<float>(trackWidth));
        setCurrentTimeInternal(newTime, true);
        update();
    }
}

void NauTimelineContentView::createKeyframeFromProperties()
{
    constexpr int ROOT_OFFSET_Y = HEADER_HEIGHT - KEYFRAME_ROOT_OFFSET;
    constexpr int MAX_ITERATION_COUNT = 10;
    int propertyIndex = -1;
    int propertyOffsetY = ROOT_OFFSET_Y - NauTimelineKeyframe{ this }.height();
    int iteration = 0;
    const float timeModifier = m_propertiesListPtr->skelList().empty() ? 1.f : m_baseStepTime;
    m_propertiesListPtr->forEach([this, &propertyIndex, &propertyOffsetY, &iteration, timeModifier](const NauAnimationProperty& property) {
        ++propertyIndex;
        auto& propertyData = m_propertyDataList[propertyIndex];
        if (!property.selected() || (iteration > MAX_ITERATION_COUNT)) {
            return;
        }
        // TODO: Make abstract
        int componentCount = 0;
        if (property.type() == NauAnimationTrackDataType::Vec3) {
            componentCount = 3;
        } else if (property.type() == NauAnimationTrackDataType::Quat) {
            componentCount = 4;
        }
        const bool expanded = m_propertyDataList[propertyIndex].expandedFlag;
        auto& propertyKeyframes = propertyData.keyframeList;
        if (propertyKeyframes.empty()) {
            propertyKeyframes.reserve(property.timeSamples().size());
        }
        propertyOffsetY += TRACK_CELL_SIZE;
        for (double sample : property.timeSamples()) {
            if (iteration > MAX_ITERATION_COUNT) {
                break;
            }
            const float time = static_cast<float>(sample) * timeModifier;
            auto keyframeIt = m_rootKeyframes.find(time);
            if (keyframeIt == m_rootKeyframes.end()) {
                NauTimelineKeyframe* keyframe = m_keyframePool.create(this, nullptr, -1, time);
                if (keyframe == nullptr) { break; }
                keyframeIt = m_rootKeyframes.emplace(time, keyframe).first;
                keyframe->move(0, ROOT_OFFSET_Y - keyframe->height());
                connect(keyframe, &NauTimelineKeyframe::eventShowKeyframeMenu, this, &NauTimelineContentView::showKeyframeMenu);
                connect(keyframe, &NauTimelineKeyframe::eventPositionChanged, this, &NauTimelineContentView::processKeyframeMove);
                connect(keyframe, &NauTimelineKeyframe::eventKeyframeSelection, this, &NauTimelineContentView::setSelectedKeyframe);
            }
            auto it = std::find_if(propertyKeyframes.begin(), propertyKeyframes.end(), [time](NauTimelineKeyframe* keyframe) {
                return keyframe->timeValue() == time;
            });
            if (it != propertyKeyframes.end()) {
                const QPoint position = (*it)->pos();
                (*it)->move(position.x(), propertyOffsetY);
                continue;
            }
            NauTimelineKeyframe* rootKeyframe = keyframeIt->second;
            NauTimelineKeyframe* propertyKeyframe = m_keyframePool.create(this, rootKeyframe, propertyIndex, time);
            if (propertyKeyframe == nullptr) { break; }
            connect(propertyKeyframe, &NauTimelineKeyframe::eventShowKeyframeMenu, this, &NauTimelineContentView::showKeyframeMenu);
            connect(propertyKeyframe, &NauTimelineKeyframe::eventPositionChanged, this, &NauTimelineContentView::processKeyframeMove);
            connect(propertyKeyframe, &NauTimelineKeyframe::eventKeyframeSelection, this, &NauTimelineContentView::setSelectedKeyframe);
            const int propertyOffsetX = rootKeyframe->pos().x();
            propertyKeyframe->move(propertyOffsetX, propertyOffsetY);
            {
                NauTimelineKeyframe* subKeyframe;
                for (int index = 1; index <= componentCount; ++index) {
                    subKeyframe = m_keyframePool.create(this, propertyKeyframe, propertyIndex, time);
                    if (subKeyframe == nullptr) { break; }
                    subKeyframe->move(propertyOffsetX, propertyOffsetY + TRACK_CELL_SIZE * index);
                    connect(subKeyframe, &NauTimelineKeyframe::eventShowKeyframeMenu, this, &NauTimelineContentView::showKeyframeMenu);
                    connect(subKeyframe, &NauTimelineKeyframe::eventPositionChanged, this, &NauTimelineContentView::processKeyframeMove);
                    connect(subKeyframe, &NauTimelineKeyframe::eventKeyframeSelection, this, &NauTimelineContentView::setSelectedKeyframe);
                }
            }
            propertyKeyframe->showChildren(expanded);
            propertyKeyframes.emplace_back(propertyKeyframe);
            ++iteration;
        }
        propertyOffsetY += componentCount * static_cast<int>(expanded) * TRACK_CELL_SIZE;
    });
    if (iteration <= MAX_ITERATION_COUNT) {
        m_timer->stop();
    }

    updateKeyframesPositions();
    update();
}

void NauTimelineContentView::deleteKeyframe()
{
    if (m_keyframeForMenu == nullptr) {
        return;
    }
    const float time = m_keyframeForMenu->timeValue();
    int propertyIndex = m_keyframeForMenu->propertyIndex();

    NauTimelineKeyframe* freeKeyframe = m_keyframeForMenu->keyframeParent();
    freeKeyframe = freeKeyframe && (freeKeyframe->propertyIndex() == propertyIndex) ? freeKeyframe : m_keyframeForMenu;
    if (NauTimelineKeyframe* parent = freeKeyframe->keyframeParent(); (parent != nullptr) && (parent->propertyIndex() == -1) && (parent->childrenCount(false) == 1)) {
        freeKeyframe = parent;
    }
    m_keyframePool.free(freeKeyframe);
    emit eventKeyframeDeleted(propertyIndex, time);
}

void NauTimelineContentView::clearKeyframes()
{
    for (auto&& [time, keyframe]: m_rootKeyframes) {
        m_keyframePool.free(keyframe);
    }
    m_rootKeyframes.clear();
    m_propertyDataList.clear();
}

void NauTimelineContentView::mousePressEvent(QMouseEvent* event)
{
    const auto [x, y] = event->position();
    m_timelinePressed = y < static_cast<float>(TIMELINE_HEIGHT);
    updateCurrentTime(x);
    NauWidget::mousePressEvent(event);
}

void NauTimelineContentView::mouseReleaseEvent(QMouseEvent* event)
{
    m_timelinePressed = false;
    update();
    NauWidget::mouseReleaseEvent(event);
}

void NauTimelineContentView::mouseMoveEvent(QMouseEvent* event)
{
    const auto [x, _] = event->position();
    updateCurrentTime(x);
    NauWidget::mouseMoveEvent(event);
}

void NauTimelineContentView::wheelEvent(QWheelEvent* event)
{
    if ((m_createClipContainer == nullptr) || !m_createClipContainer->isVisible()) {
        constexpr float MAX_MULTIPLIER = std::numeric_limits<unsigned short>::max();
        const int diff = event->angleDelta().y();
        if (diff > 0 && m_decreaseLevel > 0) {
            m_decreaseLevel -= 1;
            updateTimeline(m_decreaseLevel + 1);
        }
        else if (diff < 0 && (computeDecreaseLevelMultiplier(m_decreaseLevel) < MAX_MULTIPLIER)) {
            m_decreaseLevel += 1;
            updateTimeline(m_decreaseLevel - 1);
        }
    }
    NauWidget::wheelEvent(event);
}

void NauTimelineContentView::resizeEvent(QResizeEvent* event)
{
    const QSize size = event->size();
    if (size.isValid()) {
        const float animationLength = static_cast<float>(nau::CalculateTimelineSizeInPixels(m_animationEnd, m_baseStepTime, TRACK_CELL_SIZE));
        const float scrollerLength = static_cast<float>(width());
        const float timelineLength = scrollerLength / m_timelineScroller->viewLength();
        const float viewLength = std::min(1.f, size.width() / std::max(animationLength, timelineLength));
        m_timelineScroller->setViewLength(viewLength);
        m_timelineScroller->shrinkScroller(animationLength / timelineLength);
        m_timelineScroller->setLength(size.width());
        m_timelineScroller->move(0, size.height() - TIMELINE_SCROLL_OFFSET - m_timelineScroller->height());
        m_trackScroller->setLength(size.height() - HEADER_HEIGHT);
        m_trackScroller->move(size.width() - TIMELINE_SCROLL_OFFSET - m_trackScroller->width(), HEADER_HEIGHT);
    }
    NauWidget::resizeEvent(event);
}

void NauTimelineContentView::paintEvent(QPaintEvent* event)
{
    const bool primSelected = !m_createClipContainer->isVisible();
    const auto&& [width, height] = size();
    const auto [timeStep, trackWidth] = computeConstants();

    const NauPalette palette = Nau::Theme::current().paletteTimelineContentView();
    const NauPalette palettePointer = Nau::Theme::current().paletteTimelineFramePointer();

    const float timelineLength = std::round(static_cast<float>(width) / m_timelineScroller->viewLength());
    const float multiplier = computeDecreaseLevelMultiplier(m_decreaseLevel);
    const int timelinePosition = static_cast<int>(std::round(timelineLength * m_timelineScroller->position() / multiplier)) - SAFE_AREA_WIDTH;
    const int safeArea = std::abs(std::min(0, timelinePosition));
    const int timelineOffsetSec = safeArea > 0 ? safeArea : (-(timelinePosition) % (trackWidth * TIMELINE_SECTION_SIZE));
    const int timelineOffsetSub = safeArea > 0 ? safeArea : (timelineOffsetSec % trackWidth);
    const float timeOffset = nau::CalculateTimeFromPosition(std::max(0, timelinePosition), timeStep, trackWidth);

    const int MAX_TRACK_LINE_COUNT = (height - HEADER_HEIGHT) / TRACK_CELL_SIZE;
    const int MAX_TIMELINE_SECTION_COUNT = (width / trackWidth) + TIMELINE_SECTION_SIZE;

    m_drawingLines.clear();
    m_drawingLines.reserve(MAX_TRACK_LINE_COUNT + MAX_TIMELINE_SECTION_COUNT);
    m_drawingText.clear();
    m_drawingText.reserve(1 + (MAX_TIMELINE_SECTION_COUNT / TIMELINE_SECTION_SIZE));

    QPainter painter{ this };
    painter.fillRect(0, 0, width, TIMELINE_HEIGHT, palette.color(NauPalette::Role::BackgroundFooter));
    painter.fillRect(0, TIMELINE_HEIGHT, width, HEADER_HEIGHT - TIMELINE_HEIGHT, QColor(20, 20, 20));
    painter.fillRect(0, HEADER_HEIGHT, width, height - HEADER_HEIGHT, palette.color(NauPalette::Role::BackgroundFooter));
    painter.setPen(palette.color(NauPalette::Role::Background));

    if (primSelected) {
        for (int trackIndex = 0; trackIndex < MAX_TRACK_LINE_COUNT; ++trackIndex) {
            const int y = (trackIndex + 1) * TRACK_CELL_SIZE + HEADER_HEIGHT;
            m_drawingLines.emplace_back(0, y, width, y);
        }
        for (int sectionIndex = 0; sectionIndex < MAX_TIMELINE_SECTION_COUNT; ++sectionIndex) {
            const int x = sectionIndex * trackWidth + timelineOffsetSub;
            m_drawingLines.emplace_back(x, HEADER_HEIGHT, x, height);
        }
        painter.drawLines(m_drawingLines);

        m_drawingLines.clear();
        for (int sectionIndex = 0; sectionIndex < MAX_TIMELINE_SECTION_COUNT; sectionIndex += TIMELINE_SECTION_SIZE) {
            const int x = sectionIndex * trackWidth + timelineOffsetSec;
            m_drawingLines.emplace_back(x, HEADER_HEIGHT, x, height);
        }
        painter.setPen(palette.color(NauPalette::Role::AlternateBackground));
        painter.drawLines(m_drawingLines);
    }
    if (primSelected) {
        QRect animationZone{
            safeArea + 1 + nau::CalculateTimelineSizeInPixels(m_animationStart - timeOffset, timeStep, trackWidth),
            0,
            nau::CalculateTimelineSizeInPixels(m_animationEnd - m_animationStart, timeStep, trackWidth),
            TIMELINE_HEIGHT
        };
        NauColor color = palette.color(NauPalette::Role::Foreground);
        painter.fillRect(animationZone, color);
        animationZone.setTop(HEADER_HEIGHT);
        animationZone.setBottom(height);
        color.setAlpha(38);
        painter.fillRect(animationZone, color);
    }
    { // Draw timeline subsections
        m_drawingLines.clear();
        for (int sectionIndex = 0; sectionIndex < MAX_TIMELINE_SECTION_COUNT; ++sectionIndex) {
            int x = timelineOffsetSec + sectionIndex * trackWidth * TIMELINE_SECTION_SIZE + trackWidth;
            for (int subSectionIndex = 0; subSectionIndex < TIMELINE_SECTION_SIZE - 1; ++subSectionIndex, x += trackWidth) {
                m_drawingLines.emplace_back(x, TIMELINE_HEIGHT, x, TIMELINE_HEIGHT - SECTION_SMALL_HEIGHT * (1 + (subSectionIndex & 1)));
            }
        }
        NauColor color = palette.color(NauPalette::Role::BackgroundHeader);
        color.setAlpha(127);
        painter.setPen({ color, 2. }); // timeline subsection color
        painter.drawLines(m_drawingLines);
    }
    { // Draw timeline text and main sections
        constexpr int TIME_TEXT_OFFSET_Y = -2;
        m_drawingLines.clear();
        for (int sectionIndex = 0; sectionIndex < MAX_TIMELINE_SECTION_COUNT; sectionIndex += TIMELINE_SECTION_SIZE) {
            const int x = timelineOffsetSec + sectionIndex * trackWidth;
            m_drawingLines.emplace_back(x, 0, x, TIMELINE_HEIGHT);

            const float time = timeStep * static_cast<float>(TIMELINE_SECTION_SIZE) * (timelinePosition / (trackWidth * TIMELINE_SECTION_SIZE) + m_drawingText.length());
            m_drawingText.emplace_back(std::move(nau::CalculateTimeToString(time, false)));
        }
        painter.setPen({ palette.color(NauPalette::Role::BackgroundHeader), 2. }); // timeline main section color
        painter.drawLines(m_drawingLines);
        painter.setPen(palette.color(NauPalette::Role::Text));
        for (int index = 0; index < m_drawingText.length(); ++index) {
            const int sectionOffset = m_drawingLines[index].p1().x();
            painter.drawText(sectionOffset + 6, 31 + TIME_TEXT_OFFSET_Y, m_drawingText[index]);
        }
    }
    // Draw timeline highlight
    if (m_selectedKeyframe != nullptr) {
        constexpr int keyframeHighlight = 18;
        constexpr int keyframeHighlightHalf = keyframeHighlight / 2;
        const int x = safeArea + 1 + nau::CalculateTimelineSizeInPixels(m_selectedKeyframe->timeValue() - timeOffset, timeStep, trackWidth);
        painter.setPen({ palette.color(NauPalette::Role::ForegroundBrightHeader), keyframeHighlight });
        painter.drawLine(x, keyframeHighlightHalf, x, TIMELINE_HEIGHT - keyframeHighlightHalf);
    }
    if (primSelected) { // Draw current frame
        const NauPalette::State state = m_timelinePressed ? NauPalette::Selected : NauPalette::Normal;

        const int x = safeArea + nau::CalculateTimelineSizeInPixels(m_currentTime - timeOffset, timeStep, trackWidth);
        painter.setPen({ palettePointer.color(NauPalette::Role::Background, state), 2. });
        painter.drawLine(x, 0, x, height);

        constexpr QSize TIME_BUBBLE_SIZE{ 80, 32 };
        constexpr int TIME_BUBBLE_ROUND = 8;
        constexpr int TIME_TEXT_OFFSET_Y = -2;
        const QRect rect{ QPoint{ x - TIME_BUBBLE_SIZE.width() / 2, (TIMELINE_HEIGHT - TIME_BUBBLE_SIZE.height()) / 2 }, TIME_BUBBLE_SIZE };
        QPainterPath path;
        path.addRoundedRect(rect, TIME_BUBBLE_ROUND, TIME_BUBBLE_ROUND);
        painter.fillPath(path, palettePointer.color(NauPalette::Role::Background, state));

        const QString timeString = nau::CalculateTimeToString(m_currentTime, true);
        const QSize pixels = QFontMetrics(painter.font()).size(0, timeString);
        const QSize textPosition = QSize{ rect.left(), rect.bottom() } - QSize{ pixels.width() - TIME_BUBBLE_SIZE.width(), TIME_BUBBLE_SIZE.height() - pixels.height() } / 2;
        painter.setPen(Qt::white);
        painter.drawText(textPosition.width(), textPosition.height() + TIME_TEXT_OFFSET_Y, timeString);
    }
}
