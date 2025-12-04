// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Viewport utils

#pragma once

#include <nau/math/math.h>

#ifdef NAU_BUILD_WATERMARK
#include <eastl/string.h>

#include <QObject>
#include <QVector>
#include <QCryptographicHash>
#include <QPoint>
#include <QRandomGenerator>
#include <QPropertyAnimation>
#endif


namespace Nau::Utils
{
    void screenToWorld(const nau::math::vec2& screen, nau::math::vec3& world, nau::math::vec3& worldDirection);
    bool worldToScreen(const nau::math::vec3& world, nau::math::vec2& screen);
    bool isLineSegmentVisible(const nau::math::vec3& worldPoint1, const nau::math::vec3& worldPoint2);
}


#ifdef NAU_BUILD_WATERMARK

// ** NauWatermarkData

class NauWatermarkData : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QPointF position READ position WRITE setPosition)

public:
    NauWatermarkData(QPointF position) : m_position(position), m_target(position) {}
    NauWatermarkData(const NauWatermarkData& other) : m_position(other.m_position), m_target(other.m_position) {}
    NauWatermarkData& operator=(const NauWatermarkData& other) { m_position = other.m_position; m_target = other.m_target; return *this; }
    
    void update();

    QPointF& position() { return m_position; }
    void setPosition(const QPointF& position) { m_position = position; }

private:
    QPointF m_position;
    QPointF m_target;
};


// ** NauViewportWatermark

class NauViewportWatermark
{
public:
    NauViewportWatermark();

    static void setDevicePixelRatio(float DPR) { devicePixelRatio = DPR; }

private:
    void renderWatermark();

private:
    inline static float devicePixelRatio = 1.0;

private:
    eastl::string m_buildNumberStr;
    const eastl::string m_warningFirstStr = "Warning! This is an early alpha test build,";
    const eastl::string m_warningSecondStr = "that in no way reflects the quality of the final product.";
    QVector<NauWatermarkData> m_watermarks;
};
#endif