// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_header_view.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"
#include "nau_widget_utility.hpp"
#include "themes/nau_theme.hpp"

#include <QPainter>


// ** NauHeaderView

NauHeaderView::NauHeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView(orientation, parent)
{
    setFixedHeight(32);
    setIconSize(QSize(12, 12));
}

void NauHeaderView::setSectionHideable(int logicalIndex, bool hideable)
{
    if (hideable) {
        m_unhideableSections.erase(logicalIndex);
    } else {
        m_unhideableSections.insert(logicalIndex);
    }
}

void NauHeaderView::paintSection(QPainter* painter, const QRect& rect, int logicalIndex) const
{
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->fillRect(rect, m_brush);
    painter->setPen(m_highlightedColumns.contains(logicalIndex) ? m_brightColor : m_color);

    QRect drawRect = rect - m_contentMargin;
    drawRect.moveCenter(rect.center());

    QRect sortIndicatorRect { QPoint(0, 0), sortIndicatorSize() };
    sortIndicatorRect.moveTopRight(drawRect.topRight());
    drawRect.setRight(sortIndicatorRect.left());

    const auto decorationData = model()->headerData(logicalIndex, orientation(), Qt::DecorationRole);
    if (decorationData.isValid()) {
        const QIcon icon = decorationData.value<QIcon>();

        // Center the picture in the middle of the column
        const int leftPadding = iconSize().width() / 2;
        const int topPadding = iconSize().height() / 2;
        const QRect iconRect{ rect.left() + leftPadding + 6, rect.top() + topPadding + 4
            ,iconSize().width(), iconSize().height()};

        icon.paint(painter, iconRect);
    } else {
        const QVariant displayData = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole);
        if (displayData.isValid()) {
            const QString displayFullText = displayData.toString();
            const QFontMetricsF metrics{painter->font()};
            const QString displayElidedText = metrics.elidedText(displayFullText, Qt::ElideRight, drawRect.width());
            const QSize textSize{static_cast<int>(std::ceil(metrics.horizontalAdvance(displayElidedText))), drawRect.height()};
            const QRect textRect{ drawRect.topLeft(), textSize};

            painter->drawText(textRect, displayElidedText, QTextOption(Qt::AlignLeft | Qt::AlignVCenter));
            sortIndicatorRect.moveLeft(textRect.right() + m_spacing);
        }
    }

    if (isSortIndicatorShown() && sortIndicatorSection() == logicalIndex) {
        if (sortIndicatorOrder() == Qt::SortOrder::AscendingOrder) {
            Nau::Theme::current().iconAscendingSortIndicator().paint(painter, sortIndicatorRect);
        } else {
            Nau::Theme::current().iconDescendingSortIndicator().paint(painter, sortIndicatorRect);
        }
    }

    painter->restore();
}

void NauHeaderView::contextMenuEvent(QContextMenuEvent* event)
{
    NauMenu menu;
    fillContextMenu(menu);

    if (!menu.isEmpty()) {

        menu.exec(event->globalPos());
        event->accept();
    }
}

QSize NauHeaderView::sortIndicatorSize() const
{
    return QSize(16, 16);
}

void NauHeaderView::setBackgroundBrush(const NauBrush& brush)
{
    m_brush = brush;
    update();
}

void NauHeaderView::setForegroundColor(const NauColor& color)
{
    m_color = color;
    update();
}

void NauHeaderView::setForegroundBrightText(const NauColor& color)
{
    m_brightColor = color;
    update();
}

void NauHeaderView::setSpacing(int spacing)
{
    m_spacing = spacing;
    update();
}

void NauHeaderView::setContentMargin(int aleft, int atop, int aright, int abottom)
{
    m_contentMargin = QMargins(aleft, atop, aright, abottom);
    update();
}

void NauHeaderView::setPalette(const NauPalette& palette)
{
    m_palette = palette;

    setBackgroundBrush(m_palette.brush(NauPalette::Role::BackgroundHeader));
    setForegroundColor(m_palette.color(NauPalette::Role::ForegroundHeader));
    setForegroundBrightText(m_palette.color(NauPalette::Role::ForegroundBrightHeader));
}

void NauHeaderView::setColumnHighlighted(int logicalIndex, bool highlighted)
{
    if (highlighted) {
        m_highlightedColumns.insert(logicalIndex);
    } else {
        m_highlightedColumns.erase(logicalIndex);
    }
}

void NauHeaderView::fillContextMenu(NauMenu& menu)
{
    for (int index = 0; index < model()->columnCount(); ++index) {
        QString displayText = model()->headerData(index, orientation()).toString();
        if (displayText.isEmpty()) {
            displayText = model()->headerData(index, orientation(), Qt::ToolTipRole).toString();
        }
        if (displayText.isEmpty()) {
            displayText = tr("Column %1").arg(index);
        }

        auto action = menu.addAction(displayText);
        action->setCheckable(true);
        action->setChecked(!isSectionHidden(index));
        action->setEnabled(!m_unhideableSections.contains(index));
        if (!action->isEnabled()) {
            continue;
        }

        connect(action, &QAction::toggled, this, [this, index](bool checked) {
            setSectionHidden(index, !checked);
            emit eventColumnVisibleToggled(index, !checked);
        });
    }
}
