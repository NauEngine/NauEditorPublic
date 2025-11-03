// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_filter_widget.hpp"

#include "nau_assert.hpp"
#include "nau_color.hpp"
#include "nau_log.hpp"
#include "themes/nau_theme.hpp"


// ** NauFilterButton

class NauFilterButton : public NauAbstractButton
{
public:
    explicit NauFilterButton(QWidget* parent)
        : NauAbstractButton(parent)
    {
        const QSize fixedSize{ m_filterLeftIcon.size().width() + m_filterRightIcon.size().width(), m_filterLeftIcon.size().height() };
        setFixedSize(fixedSize);
        setObjectName("filterButton");
        setFlat(true);

        const auto& theme = Nau::Theme::current();
        m_palette = theme.paletteFilterWidget();
    }

    void setMenu(NauMenu& menu)
    {
        connect(&menu, &QMenu::aboutToShow, [this]{ setState(NauWidgetState::Pressed); });
        connect(&menu, &QMenu::aboutToHide, [this]{ m_currentState = NauWidgetState::Active; });
        NauAbstractButton::setMenu(&menu);
    }

    struct StateAndCategory
    {
        NauPalette::State state;
        NauPalette::Category category;
    };

    [[nodiscard]] StateAndCategory widgetStateToPaletteState() const noexcept
    {
        switch (m_currentState) {
        case NauWidgetState::Active:
            return { NauPalette::State::Normal, NauPalette::Category::Active };
        case NauWidgetState::Hover:
            return { NauPalette::State::Hovered, NauPalette::Category::Active };
        case NauWidgetState::Pressed:
            return { NauPalette::State::Pressed, NauPalette::Category::Active };
        case NauWidgetState::TabFocused:
            return { NauPalette::State::Selected, NauPalette::Category::Active };
        case NauWidgetState::Disabled:
            return { NauPalette::State::Normal, NauPalette::Category::Disabled };
        }
        NED_ERROR("Unhandled state of the widget.");
        return { NauPalette::State::Normal, NauPalette::Category::Active };
    }

    void setState(NauWidgetState newState) override
    {
        if (m_currentState != NauWidgetState::Pressed) {
            NauAbstractButton::setState(newState);
        }
    }

    void paintEvent(QPaintEvent*) override
    {
        auto [state, category] = widgetStateToPaletteState();
        Nau::paintPixmap(m_filterLeftIcon, m_palette.color(NauPalette::Role::Background, state, category));
        Nau::paintPixmap(m_filterRightIcon, m_palette.color(NauPalette::Role::AlternateBackground, state, category));

        QPainter painter{ this };
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        if (m_currentState == NauWidgetState::TabFocused) {
            constexpr double PEN_WIDTH = 1.0;
            constexpr double RECT_OFFSET = PEN_WIDTH * 0.5;
            constexpr double RECT_RADIUS = 1.5;
            const QSize buttonSize = size();
            const QRectF rect{ RECT_OFFSET, RECT_OFFSET, buttonSize.width() - PEN_WIDTH, buttonSize.height() - PEN_WIDTH };

            QPainterPath path;
            path.addRoundedRect(rect, RECT_RADIUS, RECT_RADIUS);
            QPen pen{ m_palette.color(NauPalette::Role::Border, state, category), PEN_WIDTH, Qt::DashLine };
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(pen);
            painter.drawPath(path);
        }
        painter.drawPixmap(0, 0, m_filterLeftIcon);
        painter.drawPixmap(m_filterLeftIcon.width(), 0, m_filterRightIcon);
    }

private:
    QPixmap m_filterLeftIcon{ ":/UI/icons/filter/filter.svg" };
    QPixmap m_filterRightIcon{ ":/UI/icons/arrow-down.svg" };
    NauPalette m_palette;
};


// ** NauFilterWidgetAction

NauFilterWidgetAction::NauFilterWidgetAction(const NauIcon& icon, const QString& text, bool isChecked, NauWidget* parent)
    : NauWidgetAction(parent)
    , m_checkerActionWidget(new NauActionWidgetChecker(icon, text, parent))
{
    setDefaultWidget(m_checkerActionWidget);
    setObjectName("filterWidgetAction");
}

NauFilterCheckBox& NauFilterWidgetAction::checkbox() const noexcept
{
    return m_checkerActionWidget->checkBox();
}

QString NauFilterWidgetAction::text() const noexcept
{
    return m_checkerActionWidget->label().text();
}

// ** NauFilterWidget

NauFilterWidget::NauFilterWidget(NauWidget* parent)
    : NauFilterWidget(new NauFlowLayout(Qt::RightToLeft, 1, 5, 1), parent)
{
    auto hLayout = dynamic_cast<NauLayoutHorizontal*>(layout());
    NED_ASSERT(hLayout && "Unexpected layout type");

    hLayout->insertLayout(0, m_layout);
    hLayout->setStretch(0, 1);
}

NauFilterWidget::NauFilterWidget(NauFlowLayout* filterItemOutput, NauWidget* parent)
    : NauWidget(parent)
{
    setObjectName("filterWidget");

    auto* layout = new NauLayoutHorizontal(this);
    m_filterMenu = new NauMenu(tr("Filter Menu"), this);

    auto* filterButton = new NauFilterButton(this);
    filterButton->setMenu(*m_filterMenu);
    m_filterButton = filterButton;

    m_layout = filterItemOutput;
    layout->addWidget(m_filterButton);
}

void NauFilterWidget::addFilterParam(NauFilterWidgetAction* action, bool isChecked)
{
    if (action == nullptr) {
        return;
    }

    m_filterMenu->addAction(action);

    connect(&action->checkbox(), &QCheckBox::stateChanged, this, [this, action](int state) {
        handleFilterStateChange(action, static_cast<Qt::CheckState>(state));
    });

    action->checkbox().setChecked(isChecked);
}

NauFilterWidgetAction* NauFilterWidget::addFilterParam(const NauIcon& icon, const QString& name, bool isChecked)
{
    auto* action = new NauFilterWidgetAction(icon, name, isChecked, this);
    addFilterParam(action, isChecked);
    return action;
}

void NauFilterWidget::handleFilterStateChange(NauFilterWidgetAction* action, Qt::CheckState state)
{
    const QString itemType = action->text();
    auto itItemType = m_filterByItems.find(action);

    if (state == Qt::CheckState::Unchecked) {
        NED_ASSERT(itItemType != m_filterByItems.end());

        layout()->removeWidget(itItemType->second.get());
        m_filterByItems.erase(itItemType);

    } else if (state == Qt::CheckState::PartiallyChecked) {
        NED_ASSERT(itItemType != m_filterByItems.end());

        itItemType->second->setEnable(false);

    } else if (state == Qt::CheckState::Checked) {
        if (itItemType == m_filterByItems.end()) {

            auto* widget = new NauFilterItemWidget(itemType, nullptr);

            // If the layout of fast shutdown widgets is not explicitly set, we do not add them.
            if (m_layout) {
                m_layout->addWidget(0, widget);
            }

            m_filterByItems.emplace(action, widget);

            connect(widget, &NauFilterItemWidget::eventDeleteRequested,
                [action] { action->checkbox().setCheckState(Qt::CheckState::Unchecked); });

            connect(widget, &NauFilterItemWidget::eventToggleActivityRequested, this, [action](bool active) {
                action->checkbox().setCheckState(active ? Qt::CheckState::Checked : Qt::CheckState::PartiallyChecked);
            });

        } else {
            itItemType->second->setEnable(true);
        }
    } else {
        // Shouldn't happened, but still.
        NED_ASSERT(!"Unexpected checkbox state");
    }

    emitChangeFilterSignal();
}

void NauFilterWidget::emitChangeFilterSignal()
{
    QList<NauFilterWidgetAction*> activeFilterItems;
    for (const auto&[action, widget] : m_filterByItems) {
        if (widget->isFilterEnabled()) {
            activeFilterItems << action;
        }
    }

    emit eventChangeFilterRequested(activeFilterItems);
}
