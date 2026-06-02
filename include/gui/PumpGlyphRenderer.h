#pragma once

#include "gui/HmiGlyphTheme.h"

#include <QFont>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace WaterTest::GuiGlyph
{
    inline QRectF pumpBoundingRect() { return QRectF(-55, -45, 110, 140); }
    inline QPointF pumpInletPortLocal() { return QPointF(-55, 0); }
    inline QPointF pumpOutletPortLocal() { return QPointF(55, 0); }

    inline void drawPumpGlyph(
        QPainter *p,
        const QRectF &boundingRect,
        const QString &name,
        bool running,
        double frequencyHz,
        bool selected,
        const HmiGlyphTheme &theme)
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

        const QColor statusColor = running ? theme.green : theme.border;

        p->setPen(Qt::NoPen);
        p->setBrush(theme.shadow);
        p->drawRoundedRect(QRectF(-54, -42, 108, 140).translated(2, 3), 10, 10);

        const QRectF motor(-50, -15, 40, 30);
        p->setBrush(theme.body);
        p->setPen(QPen(theme.border, 2));
        p->drawRect(motor);

        p->setBrush(theme.panel);
        p->setPen(QPen(theme.border, 1));
        p->drawEllipse(QPointF(motor.left() + 20, motor.center().y()), 6, 15);

        p->setBrush(theme.metalMid);
        p->setPen(QPen(theme.border, 1));
        p->drawRect(QRectF(motor.right(), motor.center().y() - 3, 12, 6));

        p->setBrush(theme.body);
        p->setPen(QPen(theme.border, 2));
        p->drawEllipse(QPointF(22, 0), 18, 18);

        p->setBrush(theme.metalDark);
        p->setPen(QPen(theme.border, 2));
        p->drawRect(QRectF(40, -4, 12, 8));

        p->setBrush(statusColor);
        p->setPen(QPen(theme.ink, 1));
        p->drawEllipse(QPointF(motor.left() + 10, motor.top() + 5), 4, 4);

        if (running)
        {
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(theme.cyan, 2));
            p->drawEllipse(QPointF(22, 0), 10, 10);
        }

        p->setPen(theme.text);
        QFont nameFont = p->font();
        nameFont.setPointSize(9);
        nameFont.setBold(true);
        p->setFont(nameFont);
        p->drawText(QRectF(-55, 18, 110, 22), Qt::AlignCenter, name);

        QFont statusFont = p->font();
        statusFont.setPointSize(8);
        statusFont.setBold(false);
        statusFont.setFamily("Consolas");
        p->setFont(statusFont);
        p->setPen(theme.textMuted);
        p->drawText(QRectF(-55, -34, 110, 18), Qt::AlignCenter, running ? "RUN" : "STOP");

        p->setPen(theme.textDim);
        p->drawText(QRectF(-55, 40, 110, 18), Qt::AlignCenter, QString("%1Hz").arg(QString::number(frequencyHz, 'f', 1)));

        p->setPen(QPen(theme.border, 1));
        p->setBrush(theme.cyan);
        p->drawEllipse(pumpInletPortLocal(), 4, 4);
        p->drawEllipse(pumpOutletPortLocal(), 4, 4);
    }
}
