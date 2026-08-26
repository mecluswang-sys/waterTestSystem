/**
 * @file ElectricValveItem.h
 * @brief 电磁阀图元的独立封装，避免与调压阀图元混用。
 */

#pragma once

#include "gui/HmiGlyphTheme.h"
#include "gui/ValveGlyphRenderer.h"

#include <QGraphicsItem>
#include <QPainter>
#include <QSvgRenderer>
#include <QString>

namespace WaterTest
{
    class ElectricValveItem : public QGraphicsItem
    {
    public:
        static QPointF inletPortLocal() { return GuiGlyph::valveInletPortLocal(); }
        static QPointF outletPortLocal() { return GuiGlyph::valveOutletPortLocal(); }

        explicit ElectricValveItem(const QString &name,
                                   const GuiGlyph::HmiGlyphTheme &theme,
                                   bool open = true,
                                   double degree = 100.0)
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

        const QString &getName() const { return m_name; }

        void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
        {
            static QSvgRenderer openRenderer(QStringLiteral(":/hmi/valve-open.svg"));
            static QSvgRenderer closedRenderer(QStringLiteral(":/hmi/valve-close.svg"));
            QSvgRenderer &renderer = m_open ? openRenderer : closedRenderer;
            if (!renderer.isValid())
            {
                GuiGlyph::drawValveGlyph(p, boundingRect(), m_name, m_open, m_degree, isSelected(), m_theme);
                return;
            }

            p->save();
            renderer.render(p, boundingRect());

            if (isSelected())
            {
                p->setPen(QPen(m_theme.cyan, 2, Qt::DashLine));
                p->setBrush(Qt::NoBrush);
                p->drawRect(boundingRect().adjusted(2, 2, -2, -2));
            }

            p->setPen(m_theme.text);
            QFont font = p->font();
            font.setPointSize(9);
            font.setBold(true);
            p->setFont(font);
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
