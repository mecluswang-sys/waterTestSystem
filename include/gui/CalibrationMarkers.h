#pragma once

#include "ConfigManager.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPointF>
#include <QString>

namespace WaterTest::GuiGlyph
{
    inline bool showCalibrationMarkers()
    {
        static bool initialized = false;
        static bool enabled = false;
        if (!initialized)
        {
            initialized = true;
            enabled = ConfigManager::getInstance().getBool("ui.hmi.show_calibration_markers", true);
        }
        return enabled;
    }

    inline void drawCalibrationMarker(QPainter *p,
                                      const QPointF &pt,
                                      const QString &label,
                                      const QColor &color,
                                      qreal crossHalf = 4.0,
                                      qreal dotRadius = 1.8)
    {
        if (!p)
            return;

        p->setPen(QPen(color, 1.0, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(Qt::NoBrush);
        p->drawLine(QPointF(pt.x() - crossHalf, pt.y()), QPointF(pt.x() + crossHalf, pt.y()));
        p->drawLine(QPointF(pt.x(), pt.y() - crossHalf), QPointF(pt.x(), pt.y() + crossHalf));

        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawEllipse(pt, dotRadius, dotRadius);

        QFont markerFont = p->font();
        markerFont.setPointSize(6);
        markerFont.setBold(true);
        p->setFont(markerFont);
        p->setPen(color);
        p->drawText(QRectF(pt.x() + 3.0, pt.y() - 9.0, 10.0, 10.0), Qt::AlignLeft | Qt::AlignVCenter, label);
    }
}