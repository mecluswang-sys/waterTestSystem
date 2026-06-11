/**
 * @file RegulatingValveItem.h
 * @brief 电动调压阀图元的独立封装，与电磁阀分开维护。
 */

#pragma once

#include "gui/HmiGlyphTheme.h"
#include "gui/ValveGlyphRenderer.h"

#include <QGraphicsItem>
#include <QPainter>
#include <QString>

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

        QRectF boundingRect() const override { return QRectF(-55, -68, 114, 128); }

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
            GuiGlyph::drawValveGlyph(p, boundingRect(), m_name, m_open, m_degree, isSelected(), m_theme, true, false);
        }

    private:
        QString m_name;
        GuiGlyph::HmiGlyphTheme m_theme;
        bool m_open;
        double m_degree;
    };
}
