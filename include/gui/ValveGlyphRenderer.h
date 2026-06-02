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
    inline QPointF valveInletPortLocal() { return QPointF(-33, 0); }
    inline QPointF valveOutletPortLocal() { return QPointF(33, 0); }

    inline void drawValveGlyph(
        QPainter *p,
        const QRectF &boundingRect,
        const QString &name,
        bool open,
        double degree,
        bool selected,
        const HmiGlyphTheme &theme,
        bool regulatingStyle = false,
        bool plainRegulatingStyle = false)
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

        const QColor iconColor = open ? QColor(32, 84, 170) : QColor(88, 105, 132);
        const QColor frameColor = open ? theme.green : theme.red;
        const QColor stateTint = open ? QColor(32, 84, 170, 32) : QColor(88, 105, 132, 24);
        const bool isRegulatingValve = regulatingStyle || name.contains(QString::fromUtf8("调压阀"));

        if (isRegulatingValve && !plainRegulatingStyle)
        {
            const QRectF bodyRect(-46, -39, 94, 99);
            p->setPen(Qt::NoPen);
            p->setBrush(theme.shadow);
            p->drawRoundedRect(bodyRect.translated(2, 3), 9, 9);

            p->setBrush(stateTint);
            p->setPen(QPen(frameColor, 2));
            p->drawRoundedRect(bodyRect, 9, 9);
        }

        if (isRegulatingValve)
        {
            constexpr qreal triDepth = 27.0;
            constexpr qreal triHalfHeight = 15.0;
            // 调压阀口点与 C 点保持同一 y 轴基线（y=0），避免接口视觉错位。
            const QPointF center(0.0, 0.0);

            // 阀体：左右对顶三角 + 中轴。
            QPolygonF leftTriangle;
            leftTriangle << QPointF(center.x() - triDepth, center.y() - triHalfHeight)
                         << center
                         << QPointF(center.x() - triDepth, center.y() + triHalfHeight);
            QPolygonF rightTriangle;
            rightTriangle << QPointF(center.x() + triDepth, center.y() - triHalfHeight)
                          << center
                          << QPointF(center.x() + triDepth, center.y() + triHalfHeight);

            p->setPen(QPen(iconColor, 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(Qt::NoBrush);
            p->drawPolygon(leftTriangle);
            p->drawPolygon(rightTriangle);
            p->setBrush(frameColor);
            p->drawEllipse(center, 3.8, 3.8);

            // 开度反馈：0~100 映射到 270°扇形仪表（上方）。
            const double opening = std::clamp(degree, 0.0, 100.0);
            const double ratio = opening / 100.0;
            constexpr qreal gaugeR = 19.0;
            constexpr qreal tickOuter = 19.0;
            constexpr qreal tickMinorInner = 15.0;
            constexpr qreal tickMajorInner = 12.6;
            const QPointF gaugeCenter(center.x(), center.y() - 31.0);
            const QRectF gaugeRect(gaugeCenter.x() - gaugeR,
                                   gaugeCenter.y() - gaugeR,
                                   gaugeR * 2.0,
                                   gaugeR * 2.0);

            // 背景刻度弧
            p->setPen(QPen(theme.textDim, 1.8, Qt::SolidLine, Qt::RoundCap));
            p->setBrush(Qt::NoBrush);
            p->drawArc(gaugeRect, 225 * 16, -270 * 16);

            // 进度弧（从0到当前开度）
            const QColor accent = open ? QColor(56, 170, 255) : QColor(170, 132, 92);
            p->setPen(QPen(accent, 2.4, Qt::SolidLine, Qt::RoundCap));
            p->drawArc(gaugeRect, 225 * 16, static_cast<int>(-270.0 * ratio * 16.0));

            // 刻度线：每10一个小刻度，每50一个主刻度
            p->setPen(QPen(theme.textMuted, 1.2, Qt::SolidLine, Qt::RoundCap));
            for (int v = 0; v <= 100; v += 10)
            {
                const double t = static_cast<double>(v) / 100.0;
                const double aDeg = 225.0 - t * 270.0;
                const double aRad = aDeg * M_PI / 180.0;
                const bool major = (v % 50 == 0);
                const qreal inner = major ? tickMajorInner : tickMinorInner;
                const QPointF pA(gaugeCenter.x() + std::cos(aRad) * inner,
                                 gaugeCenter.y() - std::sin(aRad) * inner);
                const QPointF pB(gaugeCenter.x() + std::cos(aRad) * tickOuter,
                                 gaugeCenter.y() - std::sin(aRad) * tickOuter);
                p->drawLine(pA, pB);
            }

            // 指针
            const double thetaDeg = 225.0 - ratio * 270.0;
            const double thetaRad = thetaDeg * M_PI / 180.0;
            const QPointF tip(gaugeCenter.x() + std::cos(thetaRad) * (gaugeR - 1.3),
                              gaugeCenter.y() - std::sin(thetaRad) * (gaugeR - 1.3));
            p->setPen(QPen(accent, 2.6, Qt::SolidLine, Qt::RoundCap));
            p->drawLine(gaugeCenter, tip);
            p->setPen(Qt::NoPen);
            p->setBrush(theme.body.lighter(open ? 132 : 118));
            p->drawEllipse(gaugeCenter, 2.4, 2.4);

            // 开度数值
            p->setPen(theme.textMuted);
            QFont gaugeFont = p->font();
            gaugeFont.setPointSize(7);
            gaugeFont.setBold(true);
            p->setFont(gaugeFont);
            p->drawText(QRectF(gaugeCenter.x() - 13.0, gaugeCenter.y() + gaugeR - 1.0, 26.0, 10.0),
                        Qt::AlignCenter,
                        QString::number(opening, 'f', 0));
        }
        else
        {
            // 电磁阀以两翼交汇点作为图元中心点。
            const QPointF center(0.0, 0.0);
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(iconColor, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->drawRect(QRectF(center.x() - 14.0, center.y() - 42.0, 28.0, 21.0));

            // Stem starts from actuator bottom edge; keep a slightly shorter visual length.
            p->drawLine(QPointF(center.x(), center.y() - 21.0), QPointF(center.x(), center.y() + 1.5));

            constexpr qreal wingDepth = 31.0;
            constexpr qreal wingHalfSide = 17.4;

            p->setPen(QPen(iconColor, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(Qt::NoBrush);
            p->drawLine(QPointF(center.x() - wingDepth, center.y() - wingHalfSide), center);
            p->drawLine(center, QPointF(center.x() - wingDepth, center.y() + wingHalfSide));
            p->drawLine(QPointF(center.x() - wingDepth, center.y() - wingHalfSide),
                        QPointF(center.x() - wingDepth, center.y() + wingHalfSide));
            p->drawLine(QPointF(center.x() + wingDepth, center.y() - wingHalfSide), center);
            p->drawLine(center, QPointF(center.x() + wingDepth, center.y() + wingHalfSide));
            p->drawLine(QPointF(center.x() + wingDepth, center.y() - wingHalfSide),
                        QPointF(center.x() + wingDepth, center.y() + wingHalfSide));
        }
        p->setPen(theme.text);
        QFont f = p->font();
        f.setPointSize(9);
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(-52, 34, 104, 20), Qt::AlignCenter, name);

        if (showCalibrationMarkers())
        {
            const QPointF center(0.0, 0.0);
            const QPointF leftPort = valveInletPortLocal();
            const QPointF rightPort = valveOutletPortLocal();

            drawCalibrationMarker(p, center, "C", QColor(255, 74, 74), 4.0, 1.8);
            drawCalibrationMarker(p, leftPort, "L", QColor(74, 208, 255), 3.2, 1.6);
            drawCalibrationMarker(p, rightPort, "R", QColor(74, 208, 255), 3.2, 1.6);
        }

    }
}