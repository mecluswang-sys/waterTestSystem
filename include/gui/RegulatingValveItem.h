/**
 * @file RegulatingValveItem.h
 * @brief 电动调压阀图元的独立封装，与电磁阀分开维护。
 */

#pragma once

#include "gui/HmiGlyphTheme.h"
#include "gui/ValveGlyphRenderer.h"

#include <QGraphicsItem>
#include <QPainter>
#include <QSvgRenderer>
#include <QString>

#include <algorithm>
#include <cmath>

namespace WaterTest
{
    class RegulatingValveItem : public QGraphicsItem
    {
    public:
        static QPointF inletPortLocal() { return GuiGlyph::valveInletPortLocal(); }
        static QPointF outletPortLocal() { return GuiGlyph::valveOutletPortLocal(); }

        explicit RegulatingValveItem(const QString &name,
                                     const GuiGlyph::HmiGlyphTheme &theme,
                                     bool open = false,
                                     double degree = 0.0)
            : m_name(name),
              m_theme(theme),
              m_open(open),
              m_degree(degree)
        {
            setCacheMode(DeviceCoordinateCache);
            setFlags(QGraphicsItem::ItemIsSelectable);
        }

        QRectF boundingRect() const override { return QRectF(-90, -60, 180, 120); }

        void setOpen(bool open)
        {
            if (m_open == open)
                return;
            m_open = open;
            update();
        }

        void setDegree(double degree)
        {
            if (qFuzzyCompare(m_degree, degree))
                return;
            m_degree = degree;
            update();
        }

        void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
        {
            static QSvgRenderer openRenderer(QStringLiteral(":/hmi/regulating-valve-open.svg"));
            static QSvgRenderer closedRenderer(QStringLiteral(":/hmi/regulating-valve-close.svg"));
            QSvgRenderer &renderer = m_open ? openRenderer : closedRenderer;
            if (!renderer.isValid())
            {
                GuiGlyph::drawValveGlyph(p, boundingRect(), m_name, m_open, m_degree, isSelected(), m_theme, true, false);
                return;
            }

            p->save();
            renderer.render(p, QRectF(-90, -60, 180, 120));

            const double opening = std::clamp(m_degree, 0.0, 100.0);
            const double ratio = opening / 100.0;
            constexpr qreal gaugeRadius = 19.0;
            const QPointF gaugeCenter(0.0, -36.0);
            const QRectF gaugeRect(gaugeCenter.x() - gaugeRadius,
                                   gaugeCenter.y() - gaugeRadius,
                                   gaugeRadius * 2.0,
                                   gaugeRadius * 2.0);

            p->setPen(QPen(m_theme.textDim, 1.8, Qt::SolidLine, Qt::RoundCap));
            p->setBrush(Qt::NoBrush);
            p->drawArc(gaugeRect, 225 * 16, -270 * 16);

            const QColor accent = m_open ? m_theme.green.lighter(120) : m_theme.orange;
            p->setPen(QPen(accent, 2.4, Qt::SolidLine, Qt::RoundCap));
            p->drawArc(gaugeRect, 225 * 16, static_cast<int>(-270.0 * ratio * 16.0));

            const double angle = (225.0 - ratio * 270.0) * M_PI / 180.0;
            const QPointF tip(gaugeCenter.x() + std::cos(angle) * (gaugeRadius - 1.3),
                              gaugeCenter.y() - std::sin(angle) * (gaugeRadius - 1.3));
            const QPointF leftTip(gaugeCenter.x() + std::cos(angle + 0.10) * (gaugeRadius - 3.0),
                                  gaugeCenter.y() - std::sin(angle + 0.10) * (gaugeRadius - 3.0));
            const QPointF rightTip(gaugeCenter.x() + std::cos(angle - 0.10) * (gaugeRadius - 3.0),
                                   gaugeCenter.y() - std::sin(angle - 0.10) * (gaugeRadius - 3.0));
            QPainterPath pointer;
            pointer.moveTo(gaugeCenter);
            pointer.lineTo(leftTip);
            pointer.lineTo(tip);
            pointer.lineTo(rightTip);
            pointer.closeSubpath();
            p->setBrush(accent);
            p->setPen(Qt::NoPen);
            p->drawPath(pointer);
            p->setPen(Qt::NoPen);
            p->setBrush(accent);
            p->drawEllipse(gaugeCenter, 2.4, 2.4);

            p->setPen(m_theme.textMuted);
            QFont gaugeFont = p->font();
            gaugeFont.setPointSize(7);
            gaugeFont.setBold(true);
            p->setFont(gaugeFont);
            p->drawText(QRectF(-13.0, -12.0, 26.0, 10.0), Qt::AlignCenter,
                        QString::number(opening, 'f', 0));

            if (isSelected())
            {
                p->setPen(QPen(m_theme.cyan, 2, Qt::DashLine));
                p->setBrush(Qt::NoBrush);
                p->drawRect(boundingRect().adjusted(2, 2, -2, -2));
            }

            p->setPen(m_theme.text);
            QFont nameFont = p->font();
            nameFont.setPointSize(9);
            nameFont.setBold(true);
            p->setFont(nameFont);
            p->drawText(QRectF(-70, 34, 140, 18), Qt::AlignCenter, m_name);
            p->restore();
        }

    private:
        QString m_name;
        GuiGlyph::HmiGlyphTheme m_theme;
        bool m_open;
        double m_degree;
    };
}
