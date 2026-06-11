#pragma once

#include "gui/HmiGlyphTheme.h"
#include "gui/ValveGlyphRenderer.h"

#include <QGraphicsItem>
#include <QPainter>
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

        QRectF boundingRect() const override { return QRectF(-50, -68, 104, 128); }

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
            GuiGlyph::drawValveGlyph(p, boundingRect(), m_name, m_open, m_degree, isSelected(), m_theme);
        }

    private:
        QString m_name;
        GuiGlyph::HmiGlyphTheme m_theme;
        bool m_open;
        double m_degree;
    };
}
