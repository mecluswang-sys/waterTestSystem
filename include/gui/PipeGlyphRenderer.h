#pragma once

#include "gui/HmiGlyphTheme.h"

#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QVariant>

namespace WaterTest::GuiGlyph
{
    inline void addDynamicPipeToScene(
        QGraphicsScene *scene,
        const QPainterPath &path,
        qreal flowDashOffset,
        const QColor &pipeOuter,
        const QColor &pipeInner,
        const HmiGlyphTheme &theme,
        qreal outerWidth = 12.0,
        qreal innerWidth = 8.0,
        qreal flowWidth = 4.0,
        int baseZ = 1,
        const QVariant &flowTag = QVariant("hmi_pipe_flow"))
    {
        if (!scene || path.isEmpty())
            return;

        auto *outer = scene->addPath(path, QPen(pipeOuter, outerWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        outer->setZValue(baseZ);

        auto *inner = scene->addPath(path, QPen(pipeInner, innerWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        inner->setZValue(baseZ + 1);

        QPen flowPen(theme.cyan, flowWidth, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        flowPen.setDashPattern({4, 4});
        flowPen.setDashOffset(flowDashOffset);

        auto *flow = scene->addPath(path, flowPen);
        flow->setZValue(baseZ + 2);
        flow->setData(0, flowTag);
        flow->setVisible(false);
    }

    inline void drawDynamicPipeGlyph(
        QPainter *p,
        const QPainterPath &path,
        bool flowing,
        qreal flowDashOffset,
        bool selected,
        bool hovered,
        const QList<QPointF> &waypointScenePositions,
        const QColor &pipeOuter,
        const QColor &pipeInner,
        const HmiGlyphTheme &theme)
    {
        if (!p || path.isEmpty())
            return;

        p->setRenderHint(QPainter::Antialiasing, true);

        QPen borderPen(pipeOuter, 12.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QPen innerPen(pipeInner, 8.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

        p->setPen(borderPen);
        p->drawPath(path);
        p->setPen(innerPen);
        p->drawPath(path);

        if (flowing)
        {
            QPen flowPen(theme.cyan, 4.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
            flowPen.setDashPattern({4, 4});
            flowPen.setDashOffset(flowDashOffset);
            p->setPen(flowPen);
            p->drawPath(path);
        }

        if (selected || hovered)
        {
            p->setPen(QPen(theme.cyan, 2));
            p->setBrush(QColor(theme.cyan.red(), theme.cyan.green(), theme.cyan.blue(), 160));
            for (const QPointF &wpScene : waypointScenePositions)
                p->drawEllipse(wpScene, 6, 6);
        }
    }
}
