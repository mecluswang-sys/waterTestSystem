#pragma once

#include "gui/HmiGlyphTheme.h"

#include <QFont>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <algorithm>

namespace WaterTest::GuiGlyph
{
    inline QRectF outdoorPoolBoundingRect() { return QRectF(-80, -100, 160, 200); }
    inline QPointF outdoorPoolInletPortLocal() { return QPointF(80, -55); }
    inline QPointF outdoorPoolOutletPortLocal() { return QPointF(80, 35); }

    inline void drawOutdoorPoolGlyph(QPainter *p, const QRectF &boundingRect, const QString &name, double waterLevel, bool selected, const HmiGlyphTheme &theme)
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
        p->drawRoundedRect(QRectF(-78, -98, 156, 196).translated(3, 4), 12, 12);

        const QRectF poolBody(-70, -90, 140, 160);
        p->setBrush(theme.panel);
        p->setPen(QPen(theme.border, 3));
        p->drawRoundedRect(poolBody, 8, 8);

        const double level = std::clamp(waterLevel, 0.0, 100.0);
        const double fillH = poolBody.height() * (level / 100.0);
        const QRectF water(poolBody.left() + 3, poolBody.bottom() - fillH, poolBody.width() - 6, fillH);
        p->save();
        p->setClipRect(poolBody);
        p->setBrush(QColor(theme.water.red(), theme.water.green(), theme.water.blue(), 120));
        p->setPen(Qt::NoPen);
        p->drawRect(water);
        p->setPen(QPen(theme.water, 2));
        p->drawLine(QPointF(water.left(), water.top()), QPointF(water.right(), water.top()));
        p->restore();

        p->setBrush(theme.metalDark);
        p->setPen(QPen(theme.border, 2));
        p->drawRect(QRectF(poolBody.right(), poolBody.top() + 30, 18, 10));
        p->drawRect(QRectF(poolBody.right(), poolBody.bottom() - 40, 18, 10));

        p->setBrush(theme.metalMid);
        p->setPen(QPen(theme.border, 2));
        p->drawRect(QRectF(-75, poolBody.bottom(), 150, 8));

        p->setPen(theme.text);
        QFont f = p->font();
        f.setPointSize(10);
        f.setBold(true);
        p->setFont(f);
        p->drawText(QRectF(-80, 82, 160, 18), Qt::AlignCenter, name);

        QFont f2 = p->font();
        f2.setPointSize(8);
        f2.setBold(false);
        f2.setFamily("Consolas");
        p->setFont(f2);
        p->setPen(theme.textMuted);
        p->drawText(QRectF(-80, 68, 160, 16), Qt::AlignCenter, QString("水位:%1%").arg(QString::number(level, 'f', 0)));

        p->setPen(QPen(theme.border, 1));
        p->setBrush(theme.cyan);
        p->drawEllipse(outdoorPoolInletPortLocal(), 4, 4);
        p->drawEllipse(outdoorPoolOutletPortLocal(), 4, 4);
    }
}
