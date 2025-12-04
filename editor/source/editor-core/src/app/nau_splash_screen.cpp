// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/app/nau_splash_screen.hpp"
#include "baseWidgets/nau_static_text_label.hpp"
#include "themes/nau_theme.hpp"
#include "nau_assert.hpp"
#include "nau_editor_version.hpp"


// ** NauEditorSplashScreen

NauEditorSplashScreen::NauEditorSplashScreen(const QString& projectName, int stepCount)
    : NauFrame()
{
    setFixedSize(860, 520);
    setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint);
    setPalette(Nau::Theme::current().paletteSplash());
    setObjectName("StartupSplashScreen");

    auto layout = new NauLayoutVertical(this);
    layout->setContentsMargins(QMargins(0, 0, 0, 0));
    layout->setSpacing(0);

    auto logoLabel = new NauLabel(this);
    logoLabel->setPixmap(QPixmap(":/UI/icons/splash.png"));
    logoLabel->setFixedHeight(400);

    m_progressBar = new NauProgressBar(this);
    m_progressBar->setMinimum(std::min(0, stepCount));
    m_progressBar->setMaximum(std::max(0, stepCount));
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(8);

    auto footerLayout = new NauLayoutGrid();
    footerLayout->setContentsMargins(40, 24, 40, 24);
    footerLayout->setSpacing(0);

    auto headerLabel = new NauLabel(this);
    headerLabel->setObjectName("StartupHeaderVersion");
    headerLabel->setText(QStringLiteral("Nau Engine Editor %1").arg(NauEditorVersion::current().asQtString()));
    headerLabel->setFont(Nau::Theme::current().fontSplashProjectName());
    
    // We don't add this widget to layout to have it free positioning.
    auto headerRect = headerLabel->geometry();
    headerRect.setSize(headerLabel->sizeHint());
    headerRect.moveTopRight(QPoint(width() - footerLayout->contentsMargins().right(), footerLayout->contentsMargins().top()));
    headerLabel->setGeometry(headerRect);

    m_messageLabel = new NauLabel(this);
    m_messageLabel->setObjectName("SplashMessageLabel");
    m_messageLabel->setFont(Nau::Theme::current().fontSplashMessage());

    auto projectNameLabel = new NauLabel(this);
    projectNameLabel->setText(projectName);
    projectNameLabel->setObjectName("SplashMessageProjectLabel");
    projectNameLabel->setFont(Nau::Theme::current().fontSplashProjectName());

    auto copyRightLabel = new NauLabel(tr("Nau Engine %1 2023-2025\nAll rights reserved").arg(u8"\u00A9"), this);
    copyRightLabel->setObjectName("SplashCopyright");
    copyRightLabel->setAlignment(Qt::AlignRight);
    copyRightLabel->setFont(Nau::Theme::current().fontSplashCopyright());

    footerLayout->addWidget(m_messageLabel, 0, 0, 1, 2, Qt::AlignLeft);
    footerLayout->addWidget(projectNameLabel, 1, 0, Qt::AlignLeft);
    footerLayout->addWidget(copyRightLabel, 1, 1, Qt::AlignRight);

    layout->addWidget(logoLabel);
    layout->addWidget(m_progressBar);
    layout->addLayout(footerLayout);
}

void NauEditorSplashScreen::showMessage(const QString& message)
{
    m_messageLabel->setText(getProgressPercentage() + message);

    NauFrame::repaint();
    QCoreApplication::processEvents();
}

void NauEditorSplashScreen::advance(const QString& message, int step)
{
    NED_ASSERT(m_progressBar->value() + step <= m_progressBar->maximum());

    m_progressBar->setValue(m_progressBar->value() + step);
    showMessage(message);
}

QString NauEditorSplashScreen::getProgressPercentage() const
{
    const auto& min = m_progressBar->minimum();
    const auto& max = m_progressBar->maximum();
    const auto& val = m_progressBar->value();

    if (val != min && max != min) {
        const int progress = 100 * static_cast<float>(val) / (max - min);
        return QStringLiteral("%1% - ").arg(std::clamp(progress, 0, 100));
    }

    return QString();
}
