// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/viewport/nau_viewport_utils.hpp"

#include "nau/nau_constants.hpp"

#include "nau/graphics/core_graphics.h"
#include "nau/graphics/core_graphics.h"
#include "nau/math/math.h"
#include "nau/service/service_provider.h"
#include "nau/app/window_manager.h"
#include "nau/platform/windows/app/windows_window.h"
#include "nau/editor-engine/nau_editor_engine_services.hpp"


namespace Nau::Utils
{
    void screenToWorld(const nau::math::vec2& screen, nau::math::vec3& world, nau::math::vec3& worldDirection)
    {
        auto& graphcis = nau::getServiceProvider().get<nau::ICoreGraphics>();
        nau::math::mat4 projectionMatrix = graphcis.getProjMatrix();

        nau::IWindowManager& wndManager = nau::getServiceProvider().get<nau::IWindowManager>();
        auto [screenWidth, screenHeight] = wndManager.getActiveWindow().getSize();

        nau::math::vec2 normalizedScreen;
        normalizedScreen.setX(-(2.f * screen.getX() / screenWidth - 1.f));
        normalizedScreen.setY((2.f * screen.getY() / screenHeight - 1.f));

        nau::math::mat4 camera = Nau::EditorEngine()
            .cameraManager()->activeCamera()->getWorldTransform().getMatrix();

        world = camera.getCol3().getXYZ();

        nau::math::vec4 worldDirection4D = camera.getCol0() * normalizedScreen.getX() / projectionMatrix[0][0] +
            camera.getCol1() * normalizedScreen.getY() / projectionMatrix[1][1] + camera.getCol2();
        worldDirection = -Vectormath::SSE::normalize(worldDirection4D.getXYZ());
    }


    bool worldToScreen(const nau::math::vec3& world, nau::math::vec2& screen)
    {
        auto& graphcis = nau::getServiceProvider().get<nau::ICoreGraphics>();
        nau::math::mat4 projectionMatrix = graphcis.getProjMatrix();

        nau::IWindowManager& wndManager = nau::getServiceProvider().get<nau::IWindowManager>();
        auto [screenWidth, screenHeight] = wndManager.getActiveWindow().getSize();

        nau::math::mat4 camera = Nau::EditorEngine()
            .cameraManager()->activeCamera()->getWorldTransform().getMatrix();

        nau::math::vec4 sp4 = projectionMatrix * (Vectormath::inverse(camera) * nau::math::vec4(world, 1));

        if (sp4.getW() != 0.f) {
            sp4 *= 1.f / sp4.getW();
        }

        screen.setX(((sp4.getX() + 1.0f) * 0.5f) * screenWidth);
        screen.setY(((-sp4.getY() + 1.0f) * 0.5f) * screenHeight);

        return sp4.getX() >= -sp4.getW()
                && sp4.getX() <= sp4.getW()
                && sp4.getY() >= -sp4.getW()
                && sp4.getY() <= sp4.getW()
                && sp4.getZ() >= 0.f
                && sp4.getZ() <= sp4.getW();
    }

    bool isLineSegmentVisible(const nau::math::vec3& worldPoint1, const nau::math::vec3& worldPoint2)
    {
        auto& graphics = nau::getServiceProvider().get<nau::ICoreGraphics>();
        nau::math::mat4 projectionMatrix = graphics.getProjMatrix();
        
        nau::math::mat4 camera = Nau::EditorEngine()
            .cameraManager()->activeCamera()->getWorldTransform().getMatrix();

        nau::math::mat4 viewProjectionMatrix = projectionMatrix * Vectormath::inverse(camera);
        nau::math::vec4 clip1 = viewProjectionMatrix * nau::math::vec4(worldPoint1, 1);
        nau::math::vec4 clip2 = viewProjectionMatrix * nau::math::vec4(worldPoint2, 1);

        if (clip1.getW() != 0.f) {
            clip1 *= 1.f / clip1.getW();
        }
        if (clip2.getW() != 0.f) {
            clip2 *= 1.f / clip2.getW();
        }

        // both points are behind near plane
        if (clip1.getZ() < 0 && clip2.getZ() < 0) {
            return false;
        }
        // both points are beyond far plane
        if (clip1.getZ() > clip1.getW() && clip2.getZ() > clip2.getW()) {
            return false;
        }

        nau::IWindowManager& wndManager = nau::getServiceProvider().get<nau::IWindowManager>();
        auto [screenWidth, screenHeight] = wndManager.getActiveWindow().getSize();

        nau::math::vec2 screen1, screen2;

        screen1.setX(((clip1.getX() + 1.0f) * 0.5f) * screenWidth);
        screen1.setY(((-clip1.getY() + 1.0f) * 0.5f) * screenHeight);
        screen2.setX(((clip2.getX() + 1.0f) * 0.5f) * screenWidth);
        screen2.setY(((-clip2.getY() + 1.0f) * 0.5f) * screenHeight);
        
        auto computeOutcode = [&](const nau::math::vec2& p) -> int {
            int code = 0;
            if (p.getX() < 0) code |= 1;                 // left
            else if (p.getX() > screenWidth) code |= 2;  // right
            if (p.getY() < 0) code |= 4;                 // bottom  
            else if (p.getY() > screenHeight) code |= 8; // top
            return code;
        };
        
        int outcode1 = computeOutcode(screen1);
        int outcode2 = computeOutcode(screen2);

        // either point is in center
        if (outcode1 == 0 || outcode2 == 0) {
            return true;
        }
        // both points share the same side therefore do not intersect the screen
        if ((outcode1 & outcode2) != 0) {
            return false;
        }
        
        return true;
    }
}


#ifdef NAU_BUILD_WATERMARK

QPointF randomCoordinate()
{
    return QPointF(
        QRandomGenerator::global()->generateDouble(),
        QRandomGenerator::global()->generateDouble()
    );
}

// ** NauWatermarkData

void NauWatermarkData::update()
{
    if (m_position == m_target) {
        m_target = randomCoordinate();
        auto animation = new QPropertyAnimation(this, "position");
        animation->setDuration(10000);
        animation->setStartValue(m_position);
        animation->setEndValue(m_target);
        animation->start();
    }
}


// ** NauViewportWatermark

static const std::unique_ptr<NauViewportWatermark> watermark(new NauViewportWatermark);

NauViewportWatermark::NauViewportWatermark()
    : m_buildNumberStr(QString(QCryptographicHash::hash(QString(NAU_WATERMARK_RECEPIENT_NAME).toUtf8(), QCryptographicHash::Md5).toHex()).toUtf8().data())
{
    // Uncomment to put random positions back:
    // constexpr int nWatermarks = 6;
    // for (int i = 0; i < nWatermarks; ++i) {
    //     m_watermarks.append(NauWatermarkData(randomCoordinate()));
    // }

    // Fixed point positions
    m_watermarks.append(NauWatermarkData({ 0.2, 0.2 }));
    m_watermarks.append(NauWatermarkData({ 0.8, 0.2 }));
    m_watermarks.append(NauWatermarkData({ 0.2, 0.8 }));
    m_watermarks.append(NauWatermarkData({ 0.8, 0.8 }));
    m_watermarks.append(NauWatermarkData({ 0.5, 0.5 }));

    NauWorldDelegates::onRenderUi.addCallback([this]() {
        renderWatermark();
    });
}

void NauViewportWatermark::renderWatermark()
{
    // Draw bottom build str
    {
        StdGuiRender::set_font(0);
        StdGuiRender::set_font_ht(14 * devicePixelRatio);
        StdGuiRender::set_color(COLOR_WHITE);

        eastl::string bottomBuildStr("Build " + m_buildNumberStr);
        const auto BBox = StdGuiRender::get_str_bbox(bottomBuildStr.c_str());
        float xPos = StdGuiRender::real_screen_width() / 2 - BBox.center().x;
        float yPos = StdGuiRender::real_screen_height() - StdGuiRender::real_screen_height() / 8;
        StdGuiRender::draw_strf_to(xPos, yPos, bottomBuildStr.c_str());
    }

    // Draw warning str
    {
        StdGuiRender::set_font_ht(13 * devicePixelRatio);

        const auto BBoxFirst = StdGuiRender::get_str_bbox(m_warningFirstStr.c_str());
        float xPos = StdGuiRender::real_screen_width() / 2 - BBoxFirst.center().x;
        float yPos = StdGuiRender::real_screen_height() - (StdGuiRender::real_screen_height() / 8 - 13 * devicePixelRatio);
        StdGuiRender::draw_strf_to(xPos, yPos, m_warningFirstStr.c_str());

        const auto BBoxSecond = StdGuiRender::get_str_bbox(m_warningSecondStr.c_str());
        xPos = StdGuiRender::real_screen_width() / 2 - BBoxSecond.center().x;
        yPos = StdGuiRender::real_screen_height() - (StdGuiRender::real_screen_height() / 8 - 25 * devicePixelRatio);
        StdGuiRender::draw_strf_to(xPos, yPos, m_warningSecondStr.c_str());
    }

    // Draw build strings
    {
        StdGuiRender::set_font_ht(15 * devicePixelRatio);
        StdGuiRender::set_color(E3DCOLOR(40, 40, 40, 0));

        for (auto& data : m_watermarks) {
            // Uncomment to enable animations:
            // data.update();
            
            // Draw centered
            const auto BBox = StdGuiRender::get_str_bbox(m_buildNumberStr.c_str());
            StdGuiRender::draw_strf_to(
                data.position().x() * StdGuiRender::real_screen_width() - BBox.center().x,
                data.position().y() * StdGuiRender::real_screen_height() + BBox.center().y,
                m_buildNumberStr.c_str()
            );
        }
    }
}
#endif
