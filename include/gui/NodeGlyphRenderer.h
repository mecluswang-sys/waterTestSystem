#pragma once

#include "gui/HmiGlyphTheme.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace WaterTest::GuiGlyph
{
    inline QRectF teeNodeBoundingRect() { return QRectF(-24, -20, 48, 40); }
    inline QPointF teeNodeInlet1PortLocal() { return QPointF(-18, 8); }
    inline QPointF teeNodeInlet2PortLocal() { return QPointF(-18, -8); }
    inline QPointF teeNodeOutletPortLocal() { return QPointF(18, 0); }

    inline void drawTeeNodeGlyph(QPainter *p, const QRectF &boundingRect, bool selected, const HmiGlyphTheme &theme)
    {
        if (!p)
            return;
        p->setRenderHint(QPainter::Antialiasing, true);
        if (selected)
        {
            p->setPen(QPen(theme.cyan, 2, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(boundingRect.adjusted(1, 1, -1, -1), 6, 6);
        }

        p->setPen(QPen(theme.border, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawLine(QPointF(-14, -8), QPointF(-14, 8));
        p->drawLine(QPointF(-14, 0), QPointF(14, 0));

        p->setPen(QPen(theme.border, 1));
        p->setBrush(theme.cyan);
        p->drawEllipse(teeNodeInlet1PortLocal(), 3.5, 3.5);
        p->drawEllipse(teeNodeInlet2PortLocal(), 3.5, 3.5);
        p->drawEllipse(teeNodeOutletPortLocal(), 3.5, 3.5);
    }

    inline QRectF safetyIndicatorBoundingRect() { return QRectF(-30, -22, 60, 44); }

    inline void drawSafetyIndicatorGlyph(QPainter *p, const QString &name, bool active, const HmiGlyphTheme &theme)
    {
        if (!p)
            return;
        p->setRenderHint(QPainter::Antialiasing, true);

        const QColor status = active ? theme.red : theme.border;

        p->setPen(QPen(theme.border, 2));
        p->setBrush(theme.body);
        QPolygonF tri;
        tri << QPointF(-14, 2) << QPointF(0, 20) << QPointF(14, 2);
        p->drawPolygon(tri);

        p->setBrush(theme.body);
        p->drawRect(QRectF(-5, -20, 10, 10));

        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(theme.textMuted, 1.5));
        QPainterPath spring;
        spring.moveTo(0, -10);
        spring.lineTo(-3, -8);
        spring.lineTo(3, -6);
        spring.lineTo(-3, -4);
        spring.lineTo(3, -2);
        spring.lineTo(-3, 0);
        spring.lineTo(0, 2);
        p->drawPath(spring);
        p->setPen(QPen(theme.border, 2));

        p->setBrush(theme.panel);
        QPolygonF vent;
        vent << QPointF(14, 8) << QPointF(22, 4) << QPointF(22, 12);
        p->drawPolygon(vent);

        p->setBrush(theme.body);
        p->drawRect(QRectF(-3, 20, 6, 12));

        p->setPen(QPen(theme.ink, 1));
        p->setBrush(status);
        p->drawEllipse(QPointF(-22, 10), 3, 3);

        p->setPen(theme.text);
        QFont f = p->font();
        f.setPointSize(8);
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(-18, -34, 36, 14), Qt::AlignCenter, name);
    }
}
