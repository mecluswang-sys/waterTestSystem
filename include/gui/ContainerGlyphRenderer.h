#pragma once

#include "gui/HmiGlyphTheme.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <algorithm>

namespace WaterTest::GuiGlyph
{
    inline QRectF accumulatorBoundingRect() { return QRectF(-60, -55, 120, 120); }
    inline QPointF accumulatorInletPortLocal() { return QPointF(-50, 0); }
    inline QPointF accumulatorOutletPortLocal() { return QPointF(50, 0); }
    inline QPointF accumulatorReturnPortLocal() { return QPointF(0, 55); }

    inline QRectF tankBoundingRect() { return QRectF(-70, -90, 160, 240); }
    inline QPointF tankInletPortLocal() { return QPointF(-55, -5); }
    inline QPointF tankOutletPortLocal() { return QPointF(55, 50); }

    inline void drawAccumulatorGlyph(
        QPainter *p,
        const QRectF &boundingRect,
        const QString &name,
        bool selected,
        const HmiGlyphTheme &theme,
        bool showReturnPort = true)
    {
        if (!p)
            return;

        p->setRenderHint(QPainter::Antialiasing, true);

        if (selected)
        {
            p->setPen(QPen(theme.cyan, 2.5, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(boundingRect.adjusted(2, 2, -2, -2), 12, 12);
        }

        p->setPen(Qt::NoPen);
        p->setBrush(theme.shadow);
        p->drawRoundedRect(QRectF(-54, -50, 108, 100).translated(3, 4), 18, 18);

        const QRectF body(-50, -26, 100, 52);
        QLinearGradient g(body.topLeft(), body.bottomLeft());
        g.setColorAt(0.0, theme.body.lighter(112));
        g.setColorAt(1.0, theme.body.darker(108));
        p->setBrush(g);
        p->setPen(QPen(theme.border, 2));
        p->drawRoundedRect(body, 26, 26);

        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(theme.borderWeak, 2));
        p->drawArc(QRectF(-30, -18, 60, 36), 30 * 16, 120 * 16);

        p->setBrush(theme.metalDark);
        p->setPen(QPen(theme.border, 2));
        p->drawRect(QRectF(body.left() - 14, -6, 14, 12));
        p->drawRect(QRectF(body.right(), -6, 14, 12));
        if (showReturnPort)
            p->drawRect(QRectF(-6, body.bottom(), 12, 16));

        p->setPen(theme.text);
        QFont f = p->font();
        f.setPointSize(9);
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(-60, 30, 120, 20), Qt::AlignCenter, name);

        p->setPen(QPen(theme.border, 1));
        p->setBrush(theme.cyan);
        p->drawEllipse(accumulatorInletPortLocal(), 4, 4);
        p->drawEllipse(accumulatorOutletPortLocal(), 4, 4);
        if (showReturnPort)
            p->drawEllipse(accumulatorReturnPortLocal(), 4, 4);
    }

    inline void drawTankGlyph(
        QPainter *p,
        const QRectF &boundingRect,
        const QString &name,
        double fillPercent,
        bool filling,
        const QString &detailText,
        bool selected,
        const HmiGlyphTheme &theme)
    {
        if (!p)
            return;

        p->setRenderHint(QPainter::Antialiasing, true);

        if (selected)
        {
            p->setPen(QPen(theme.cyan, 3, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(boundingRect.adjusted(2, 2, -2, -2), 18, 18);
        }

        p->setPen(Qt::NoPen);
        p->setBrush(theme.shadow);
        p->drawRoundedRect(QRectF(-58, -88, 116, 176).translated(3, 4), 18, 18);

        const QRectF roof(-46, -74, 92, 18);
        const QRectF shell(-46, -65, 92, 120);
        const QRectF bottom(-46, 55, 92, 18);

        p->setBrush(theme.body);
        p->setPen(QPen(theme.border, 2));
        p->drawEllipse(roof);
        p->drawEllipse(bottom);

        p->setBrush(theme.panel);
        p->setPen(QPen(theme.border, 2));
        p->drawRect(shell);

        const double level = std::clamp(fillPercent, 0.0, 100.0);
        const double fillH = shell.height() * (level / 100.0);
        const QRectF liquid(shell.left() + 2, shell.bottom() - fillH, shell.width() - 4, fillH);
        p->save();
        p->setClipRect(shell);
        p->setBrush(QColor(theme.cyan.red(), theme.cyan.green(), theme.cyan.blue(), 120));
        p->setPen(Qt::NoPen);
        p->drawRect(liquid);
        p->setPen(QPen(theme.cyan, 2));
        p->drawLine(QPointF(liquid.left(), liquid.top()), QPointF(liquid.right(), liquid.top()));
        p->restore();

        p->setBrush(theme.metalDark);
        p->setPen(QPen(theme.border, 2));
        p->drawRect(QRectF(shell.left() - 18, shell.center().y() - 5, 18, 10));
        p->drawRect(QRectF(shell.right(), shell.bottom() - 10, 18, 10));

        p->setBrush(theme.metalDark);
        p->setPen(QPen(theme.border, 1));
        p->drawRect(QRectF(shell.left() + 10, bottom.bottom() - 2, 8, 20));
        p->drawRect(QRectF(shell.right() - 18, bottom.bottom() - 2, 8, 20));
        p->drawRect(QRectF(shell.left() + 6, bottom.bottom() + 16, shell.width() - 12, 4));

        p->setBrush(theme.metalDark);
        p->setPen(QPen(theme.border, 1.5));
        p->drawEllipse(QRectF(-8, roof.top() + 3, 16, 6));

        p->setBrush(filling ? theme.green : theme.border);
        p->setPen(QPen(theme.ink, 1));
        p->drawEllipse(QPointF(shell.right() + 14, roof.center().y()), 4, 4);

        p->setPen(theme.text);
        QFont f = p->font();
        f.setPointSize(10);
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(-70, 78, 140, 18), Qt::AlignCenter, name);

        QFont f2 = p->font();
        f2.setPointSize(8);
        f2.setBold(false);
        f2.setFamily("Consolas");
        p->setFont(f2);
        p->setPen(theme.textMuted);
        p->drawText(QRectF(-70, 62, 140, 16), Qt::AlignCenter, detailText);

        p->setPen(QPen(theme.border, 1));
        p->setBrush(theme.cyan);
        p->drawEllipse(tankInletPortLocal(), 4, 4);
        p->drawEllipse(tankOutletPortLocal(), 4, 4);
    }
}
