#pragma once

#include "gui/HmiGlyphTheme.h"

#include <QFont>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace WaterTest::GuiGlyph
{
    inline QRectF threeWayValveBoundingRect() { return QRectF(-56, -52, 120, 122); }
    inline QPointF threeWayValveInletPortLocal() { return QPointF(-44, 12); }
    inline QPointF threeWayValveOutletPortLocal() { return QPointF(50, 12); }
    inline QPointF threeWayValveBranchPortLocal() { return QPointF(0, 42); }

    inline void drawThreeWayValveGlyph(QPainter *p, const QRectF &boundingRect, const QString &name, bool selected, const HmiGlyphTheme &theme)
    {
        if (!p)
            return;

        p->setRenderHint(QPainter::Antialiasing, true);
        if (selected)
        {
            p->setPen(QPen(theme.cyan, 3, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(boundingRect.adjusted(2, 2, -2, -2), 10, 10);
        }

        const QColor iconColor = theme.orange;
        const QColor stateTint(iconColor.red(), iconColor.green(), iconColor.blue(), 26);

        p->setPen(Qt::NoPen);
        p->setBrush(theme.shadow);
        p->drawRoundedRect(QRectF(-44, -40, 92, 104).translated(2, 3), 10, 10);

        p->setBrush(stateTint);
        p->setPen(QPen(iconColor, 2));
        p->drawRoundedRect(QRectF(-44, -40, 92, 104), 10, 10);

        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(iconColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawRect(QRectF(-10, -22, 20, 16));
        p->drawLine(QPointF(0, -6), QPointF(0, 18));

        constexpr qreal wingDepth = 24.0;
        constexpr qreal wingHalfSide = 13.86;
        QPolygonF leftWing;
        leftWing << QPointF(-wingDepth, 18 - wingHalfSide) << QPointF(0, 18) << QPointF(-wingDepth, 18 + wingHalfSide);
        QPolygonF rightWing;
        rightWing << QPointF(wingDepth, 18 - wingHalfSide) << QPointF(0, 18) << QPointF(wingDepth, 18 + wingHalfSide);
        p->drawPolygon(leftWing);
        p->drawPolygon(rightWing);

        p->setBrush(iconColor);
        p->drawEllipse(QPointF(0, 18), 2.5, 2.5);
        p->drawLine(QPointF(0, 18), QPointF(0, 40));

        p->setPen(theme.textMuted);
        QFont actuatorFont = p->font();
        actuatorFont.setPointSize(7);
        actuatorFont.setBold(true);
        actuatorFont.setFamily("Consolas");
        p->setFont(actuatorFont);
        p->drawText(QRectF(-10, -22, 20, 16), Qt::AlignCenter, "3W");

        p->setPen(theme.text);
        QFont f = p->font();
        f.setPointSize(9);
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(-60, 30, 120, 18), Qt::AlignCenter, name);

        p->setPen(QPen(theme.border, 1));
        p->setBrush(theme.cyan);
        p->drawEllipse(threeWayValveInletPortLocal(), 4, 4);
        p->drawEllipse(threeWayValveOutletPortLocal(), 4, 4);
        p->drawEllipse(threeWayValveBranchPortLocal(), 4, 4);
    }
}
