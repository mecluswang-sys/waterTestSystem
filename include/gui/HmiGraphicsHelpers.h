#pragma once

#include <QGraphicsItem>
#include <QPointF>
#include <QVariant>
#include <QString>

namespace WaterTest::GuiGlyph
{
    inline QVariant snapToGridItemChange(
        QGraphicsItem::GraphicsItemChange change,
        const QVariant &value,
        QGraphicsScene *scene,
        qreal gridSize = 10.0)
    {
        if (change != QGraphicsItem::ItemPositionChange || !scene)
            return value;

        const QPointF newPos = value.toPointF();
        const qreal xV = qRound(newPos.x() / gridSize) * gridSize;
        const qreal yV = qRound(newPos.y() / gridSize) * gridSize;
        return QPointF(xV, yV);
    }

    inline QString formatMovedItemMessage(const QString &name, const QPointF &pos)
    {
        return QString("[HMI坐标] %1 pos=(%2, %3)")
            .arg(name)
            .arg(pos.x(), 0, 'f', 0)
            .arg(pos.y(), 0, 'f', 0);
    }
}
