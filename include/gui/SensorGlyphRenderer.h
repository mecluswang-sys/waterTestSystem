#pragma once

#include "gui/CalibrationMarkers.h"
#include "gui/HmiGlyphTheme.h"

#include <algorithm>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <cmath>

namespace WaterTest::GuiGlyph
{
    inline QRectF sensorBoundingRect() { return QRectF(-62, -46, 124, 110); }
    inline QPointF sensorAnchorPortLocal() { return QPointF(0, 32); }
    inline QPointF sensorInletPortLocal() { return sensorAnchorPortLocal(); }
    inline QPointF sensorOutletPortLocal() { return sensorAnchorPortLocal(); }

    inline void drawSensorGlyph(
        QPainter *p,
        const QRectF &boundingRect,
        const QString &name,
        double value,
        int displayDecimals,
        const QString &unit,
        const QColor &typeColor,
        bool selected,
        bool drawPorts,
        const HmiGlyphTheme &theme)
    {
        if (!p)
            return;

        p->setRenderHint(QPainter::Antialiasing, true);

        if (selected)
        {
            p->setPen(QPen(theme.cyan, 3, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(boundingRect.adjusted(2, 2, -2, -2), 8, 8);
        }

        const QPointF gaugeCenter(0.0, -2.0);
        constexpr qreal gaugeR = 30.0;
        const QRectF gaugeRect(gaugeCenter.x() - gaugeR, gaugeCenter.y() - gaugeR, gaugeR * 2.0, gaugeR * 2.0);

        p->setPen(Qt::NoPen);
        p->setBrush(theme.shadow);
        p->drawEllipse(gaugeRect.translated(3.0, 4.0));

        p->setBrush(theme.panel);
        p->setPen(QPen(theme.border, 2));
        p->drawEllipse(gaugeRect);

        p->setBrush(theme.body);
        p->setPen(QPen(theme.border, 1.5));
        p->drawEllipse(gaugeRect.adjusted(5, 5, -5, -5));

        // 外圈刻度弧（0~100）
        const double gaugeValue = std::clamp(value, 0.0, 100.0);
        const double ratio = gaugeValue / 100.0;
        p->setPen(QPen(theme.textDim, 1.2, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(Qt::NoBrush);
        p->drawArc(gaugeRect.adjusted(2, 2, -2, -2), 225 * 16, -270 * 16);

        p->setPen(QPen(theme.textMuted, 1.0, Qt::SolidLine, Qt::RoundCap));
        for (int tick = 0; tick <= 10; ++tick)
        {
            const double t = static_cast<double>(tick) / 10.0;
            const double deg = 225.0 - t * 270.0;
            const double rad = deg * M_PI / 180.0;
            const bool major = (tick % 5 == 0);
            const qreal inner = major ? 22.0 : 24.0;
            const qreal outer = 29.0;
            const QPointF a(gaugeCenter.x() + std::cos(rad) * inner,
                            gaugeCenter.y() - std::sin(rad) * inner);
            const QPointF b(gaugeCenter.x() + std::cos(rad) * outer,
                            gaugeCenter.y() - std::sin(rad) * outer);
            p->drawLine(a, b);
        }

        const double needleDeg = 225.0 - ratio * 270.0;
        const double needleRad = needleDeg * M_PI / 180.0;
        const QPointF needleTip(gaugeCenter.x() + std::cos(needleRad) * 23.0,
                                gaugeCenter.y() - std::sin(needleRad) * 23.0);
        p->setPen(QPen(typeColor, 2.4, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(gaugeCenter, needleTip);
        p->setBrush(typeColor);
        p->setPen(QPen(theme.ink, 1));
        p->drawEllipse(gaugeCenter, 2.8, 2.8);

        p->setPen(theme.text);
        QFont tagFont = p->font();
        tagFont.setPointSize(8);
        tagFont.setBold(false);
        tagFont.setFamily("Consolas");
        p->setFont(tagFont);
        const QString nameWithUnit = unit.isEmpty() ? name : QString("%1 (%2)").arg(name, unit);
        p->drawText(QRectF(-58, -43, 116, 12), Qt::AlignCenter, nameWithUnit);

        QFont valFont = p->font();
        valFont.setPointSize(13);
        valFont.setBold(true);
        valFont.setFamily("Consolas");
        p->setFont(valFont);
        p->setPen(QColor(245, 248, 255));
        p->drawText(QRectF(-28, -10, 56, 18), Qt::AlignCenter, QString::number(gaugeValue, 'f', displayDecimals));

        QFont unitFont = p->font();
        unitFont.setPointSize(7);
        unitFont.setBold(false);
        unitFont.setFamily("Consolas");
        p->setFont(unitFont);
        p->setPen(theme.textDim);
        p->drawText(QRectF(-34, 8, 68, 10), Qt::AlignCenter, unit);

        p->setPen(QPen(theme.border, 1.4, Qt::DashLine, Qt::RoundCap));
        p->drawLine(QPointF(0, gaugeRect.bottom()), QPointF(0, sensorAnchorPortLocal().y() - 3));
        p->setBrush(typeColor);
        p->setPen(QPen(theme.ink, 1));
        p->drawEllipse(QPointF(0, sensorAnchorPortLocal().y()), 2.8, 2.8);

        if (showCalibrationMarkers())
        {
            // 校准标记：中心点与底部锚点。
            const QPointF center(0.0, 0.0);
            const QPointF anchorPort = sensorAnchorPortLocal();

            drawCalibrationMarker(p, center, "C", QColor(255, 74, 74), 4.0, 1.8);
            drawCalibrationMarker(p, anchorPort, "A", QColor(74, 208, 255), 3.2, 1.6);
        }

        if (drawPorts)
        {
            p->setPen(QPen(theme.border, 1));
            p->setBrush(theme.cyan);
        }
    }
}
