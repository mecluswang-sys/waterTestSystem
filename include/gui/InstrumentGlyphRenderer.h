#pragma once

#include "ConfigManager.h"
#include "gui/CalibrationMarkers.h"
#include "gui/HmiGlyphTheme.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace WaterTest::GuiGlyph
{
    inline QRectF flowMeterBoundingRect() { return QRectF(-70, -52, 140, 110); }
    inline QPointF flowMeterInletPortLocal() { return QPointF(-60, 0); }
    inline QPointF flowMeterOutletPortLocal() { return QPointF(60, 0); }

    inline void drawFlowMeterGlyph(QPainter *p, const QRectF &boundingRect, const QString &name, double flow, const QString &unit, bool hasAlarm, int emptyPipeAlarm, int excitationAlarm, bool selected, const HmiGlyphTheme &theme)
    {
        if (!p)
            return;

        p->setRenderHint(QPainter::Antialiasing, true);
        if (selected)
        {
            p->setPen(QPen(theme.cyan, 3, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(boundingRect.adjusted(2, 2, -2, -2), 12, 12);
        }

        p->setPen(Qt::NoPen);
        p->setBrush(theme.shadow);
        p->drawRoundedRect(QRectF(-62, -40, 124, 80).translated(3, 4), 14, 14);

        const bool activeAlarm = hasAlarm && (emptyPipeAlarm != 0 || excitationAlarm != 0);
        p->setBrush(theme.panel);
        p->setPen(QPen(activeAlarm ? QColor(255, 90, 90) : theme.border, activeAlarm ? 3 : 2));
        p->drawRoundedRect(QRectF(-60, -38, 120, 76), 14, 14);

        p->setBrush(theme.metalDark);
        p->setPen(QPen(theme.border, 2));
        p->drawRect(QRectF(-74, -6, 14, 12));
        p->drawRect(QRectF(60, -6, 14, 12));

        p->setBrush(theme.body);
        p->setPen(QPen(theme.border, 2));
        p->drawEllipse(QPointF(-18, 0), 18, 18);
        p->setPen(QPen(theme.cyan, 2, Qt::SolidLine, Qt::RoundCap));
        p->drawLine(QPointF(-18, 0), QPointF(-6, -8));

        p->setPen(theme.orange);
        QFont valFont = p->font();
        valFont.setPointSize(12);
        valFont.setBold(true);
        valFont.setFamily("Consolas");
        p->setFont(valFont);
        p->drawText(QRectF(4, -14, 54, 24), Qt::AlignLeft | Qt::AlignVCenter, QString::number(flow, 'f', 2));

        p->setPen(theme.textDim);
        QFont unitFont = p->font();
        unitFont.setPointSize(8);
        unitFont.setBold(false);
        unitFont.setFamily("Consolas");
        p->setFont(unitFont);
        p->drawText(QRectF(4, 6, 54, 16), Qt::AlignLeft | Qt::AlignVCenter, unit);

        if (activeAlarm)
        {
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(255, 90, 90));
            p->drawEllipse(QRectF(46, -28, 8, 8));
            p->setPen(QColor(255, 90, 90));
            p->setFont(unitFont);
            QString alarmText;
            if (emptyPipeAlarm != 0 && excitationAlarm != 0)
                alarmText = "空管/激磁报警";
            else if (emptyPipeAlarm != 0)
                alarmText = "空管报警";
            else
                alarmText = "激磁报警";
            p->drawText(QRectF(4, 20, 88, 14), Qt::AlignLeft | Qt::AlignVCenter, alarmText);
        }

        p->setPen(theme.text);
        QFont f = p->font();
        f.setPointSize(9);
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(-70, 32, 140, 20), Qt::AlignCenter, name);

        p->setPen(QPen(theme.border, 1));
        p->setBrush(theme.cyan);
        p->drawEllipse(flowMeterInletPortLocal(), 4, 4);
        p->drawEllipse(flowMeterOutletPortLocal(), 4, 4);

        if (showCalibrationMarkers())
        {
            // 校准标记：中心点与左右连接点。
            const QPointF center(0.0, 0.0);
            const QPointF leftPort = flowMeterInletPortLocal();
            const QPointF rightPort = flowMeterOutletPortLocal();

            drawCalibrationMarker(p, center, "C", QColor(255, 74, 74), 4.0, 1.8);
            drawCalibrationMarker(p, leftPort, "L", QColor(74, 208, 255), 3.2, 1.6);
            drawCalibrationMarker(p, rightPort, "R", QColor(74, 208, 255), 3.2, 1.6);
        }
    }

    inline QRectF loopBoundingRect() { return QRectF(-70, -34, 140, 84); }
    inline QPointF loopInletPortLocal() { return QPointF(-58, 0); }
    inline QPointF loopOutletPortLocal() { return QPointF(58, 0); }
    inline QPointF loopBottomPortLocal() { return QPointF(0, 34); }

    inline void drawLoopGlyph(QPainter *p, const QRectF &boundingRect, const QString &name, bool selected, const HmiGlyphTheme &theme)
    {
        if (!p)
            return;
        p->setRenderHint(QPainter::Antialiasing, true);
        if (selected)
        {
            p->setPen(QPen(theme.cyan, 3, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(boundingRect.adjusted(2, 2, -2, -2), 12, 12);
        }

        p->setPen(Qt::NoPen);
        p->setBrush(theme.shadow);
        p->drawRoundedRect(QRectF(-60, -26, 120, 62).translated(3, 4), 14, 14);

        p->setBrush(theme.body);
        p->setPen(QPen(theme.border, 2));
        p->drawRoundedRect(QRectF(-60, -26, 120, 62), 14, 14);

        p->setPen(QPen(theme.cyan, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath loop;
        loop.addEllipse(QPointF(-18, 2), 12, 12);
        p->drawPath(loop);
        p->drawLine(QPointF(-6, 2), QPointF(26, 2));
        p->drawEllipse(QPointF(30, 2), 3, 3);

        p->setPen(theme.text);
        QFont f = p->font();
        f.setPointSize(9);
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(-70, 20, 140, 20), Qt::AlignCenter, name);

        p->setPen(QPen(theme.border, 1));
        p->setBrush(theme.cyan);
        p->drawEllipse(loopInletPortLocal(), 4, 4);
        p->drawEllipse(loopOutletPortLocal(), 4, 4);
        p->drawEllipse(loopBottomPortLocal(), 4, 4);
    }
}
