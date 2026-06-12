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
    /**
     * @brief 压力传感器图元的外接矩形。
     *
     * 这个尺寸决定了图元在场景中的占位范围，也会影响选中框、对齐和端口连接时的视觉边界。
     * 当前传感器采用圆形数显风格，所以外接矩形比实际圆盘略大，给标题、数值和底部端口留出空间。
     */
    inline QRectF sensorBoundingRect() { return QRectF(-62, -46, 124, 110); }

    /**
     * @brief 传感器底部锚点的本地坐标。
     *
     * 这个点用于和管道对齐、连接场景中的管线，以及作为视觉上的“接口点”。
     * 由于传感器是上下结构，这里把锚点放在图元下方中间位置。
     */
    inline QPointF sensorAnchorPortLocal() { return QPointF(0, 32); }

    /**
     * @brief 传感器入口端口本地坐标。
     *
     * 目前传感器只有一个等价接口，因此入口和出口共用同一个锚点坐标。
     */
    inline QPointF sensorInletPortLocal() { return sensorAnchorPortLocal(); }

    /**
     * @brief 传感器出口端口本地坐标。
     *
     * 目前传感器只有一个等价接口，因此入口和出口共用同一个锚点坐标。
     */
    inline QPointF sensorOutletPortLocal() { return sensorAnchorPortLocal(); }

    /**
     * @brief 绘制压力传感器图元。
     *
     * @param p 绘制用的 QPainter，不能为空。
     * @param boundingRect 图元外接矩形，用于选中框和整体边界控制。
     * @param name 传感器名称，会显示在图元顶部，例如“压力4”。
     * @param value 当前压力值。绘制时会限制到 [0,100]，作为圆形表盘/数显的显示数据。
     * @param displayDecimals 数值显示的小数位数，用于中间数显。
    * @param unit 单位文本，例如 "kPa"。
     * @param typeColor 传感器类型颜色，用于底座、接口点和强调色。
     * @param selected 是否处于选中状态；true 时会额外绘制虚线选中框。
     * @param drawPorts 是否绘制端口点和底部接口线；当前用于控制图元端口是否可见。
     * @param theme HMI 主题颜色集合，决定背景、边框、文字和阴影风格。
     *
     * 视觉结构说明：
     * 1. 外层圆盘：传感器主体，保留“圆形表盘”的仪表感觉。
     * 2. 中间数显：显示当前压力值，替代原来的指针式视觉。
     * 3. 底部接口：和管道连接的锚点位置。
     * 4. 校准标记：仅在配置开启时显示 C/A 标记，便于调试和对点。
     */
    inline void drawSensorGlyph(
        QPainter *p,
        const QRectF &boundingRect,
        const QString &name,
        double value,
        int displayDecimals,
        const QString &unit,
        const QColor &typeColor,
        bool selected,
        bool drawPorts,
        const HmiGlyphTheme &theme,
        const QString &auxText = QString(),
        const QColor &auxColor = QColor())
    {
        if (!p)
            return;

        p->setRenderHint(QPainter::Antialiasing, true);

        if (selected)
        {
            p->setPen(QPen(theme.cyan, 3, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(boundingRect.adjusted(2, 2, -2, -2), 8, 8);
        }

        // 圆形数显主体的中心点。
        const QPointF gaugeCenter(0.0, -2.0);
        constexpr qreal gaugeR = 30.0;
        const QRectF gaugeRect(gaugeCenter.x() - gaugeR, gaugeCenter.y() - gaugeR, gaugeR * 2.0, gaugeR * 2.0);

        // 外层投影：让圆盘从背景里“浮起来”，增强仪表感。
        p->setPen(Qt::NoPen);
        p->setBrush(theme.shadow);
        p->drawEllipse(gaugeRect.translated(3.0, 4.0));

        // 外圈主体：圆形传感器表盘的外壳。
        p->setBrush(theme.panel);
        p->setPen(QPen(theme.border, 2));
        p->drawEllipse(gaugeRect);

        // 内层主体：比外圈略暗一些，形成层次。
        p->setBrush(theme.body);
        p->setPen(QPen(theme.border, 1.5));
        p->drawEllipse(gaugeRect.adjusted(5, 5, -5, -5));

        // 当前值限制到 0~100，保持与面板内部显示逻辑一致。
        // const double gaugeValue = std::clamp(value, 0.0, 100.0);

        // 外圈刻度弧（0~100）：保留“仪表”外观，但不再配合指针展示。
        p->setPen(QPen(theme.textDim, 1.2, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(Qt::NoBrush);
        p->drawArc(gaugeRect.adjusted(2, 2, -2, -2), 225 * 16, -270 * 16);

        // 外圈细刻度：作为视觉参考，让圆盘看起来像电子数显式表盘。
        p->setPen(QPen(theme.textMuted, 1.0, Qt::SolidLine, Qt::RoundCap));
        for (int tick = 0; tick <= 10; ++tick)
        {
            const double t = static_cast<double>(tick) / 10.0;
            const double deg = 225.0 - t * 270.0;
            const double rad = deg * M_PI / 180.0;
            const bool major = (tick % 5 == 0);
            const qreal inner = major ? 22.0 : 24.0;
            const qreal outer = 29.0;
            const QPointF a(gaugeCenter.x() + std::cos(rad) * inner,
                            gaugeCenter.y() - std::sin(rad) * inner);
            const QPointF b(gaugeCenter.x() + std::cos(rad) * outer,
                            gaugeCenter.y() - std::sin(rad) * outer);
            p->drawLine(a, b);
        }

        // 中心色块：原来用于指针中心，现在保留为电子表盘的视觉基点。
        p->setBrush(typeColor);
        // p->setPen(QPen(theme.ink, 1));
        // p->drawEllipse(gaugeCenter, 2.8, 2.8);

        // 数显底板：把主数值和变化值一起托出来，形成一个统一的信息块。
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(14, 20, 28, 180));
        const qreal valueBoxTop = -16.0;
        const qreal valueBoxHeight = 34.0;
        p->drawRoundedRect(QRectF(-50.0, valueBoxTop, 100.0, valueBoxHeight), 4.0, 4.0);

        // 顶部名称：显示传感器名称及单位，例如“压力4 (kPa)”。
        p->setPen(theme.text);
        QFont tagFont = p->font();
        tagFont.setPointSize(8);
        tagFont.setBold(false);
        tagFont.setFamily("Consolas");
        p->setFont(tagFont);
        const QString nameWithUnit = unit.isEmpty() ? name : QString("%1 (%2)").arg(name, unit);
        p->drawText(QRectF(-58, -43, 116, 12), Qt::AlignCenter, nameWithUnit);

        // 中央主数值：这是压力传感器最关键的显示内容。
        QFont valFont = p->font();
        valFont.setPointSize(16);
        valFont.setBold(true);
        valFont.setFamily("Consolas");
        p->setFont(valFont);
        p->setPen(QColor(245, 248, 255));
        const QRectF valueTextRect(-48.0, -14.0, 96.0, 14.0);
        p->drawText(valueTextRect, Qt::AlignCenter, QString::number(value, 'f', displayDecimals));

        // 第二排：变化值独立显示，避免和主数值挤在同一行。
        const QString auxDisplayText = auxText.isEmpty() ? QString::fromUtf8("↑ -- kPa") : auxText;
        p->setPen(auxColor.isValid() ? auxColor : theme.cyan);
        QFont auxFont = p->font();
        auxFont.setPointSize(8);
        auxFont.setBold(true);
        auxFont.setFamily("Consolas");
        p->setFont(auxFont);
        p->drawText(QRectF(-48.0, 2.0, 96.0, 12.0), Qt::AlignCenter, auxDisplayText);

        // 底部接口线和接口点：用于和管道连接，视觉上对应传感器锚点。
        p->setPen(QPen(theme.border, 1.4, Qt::DashLine, Qt::RoundCap));
        p->drawLine(QPointF(0, gaugeRect.bottom()), QPointF(0, sensorAnchorPortLocal().y() - 3));
        p->setBrush(typeColor);
        p->setPen(QPen(theme.ink, 1));
        p->drawEllipse(QPointF(0, sensorAnchorPortLocal().y()), 2.8, 2.8);

        // 调试校准标记：仅在配置开启时显示，方便现场对点。
        if (showCalibrationMarkers())
        {
            // 校准标记：中心点与底部锚点。
            const QPointF center(0.0, 0.0);
            const QPointF anchorPort = sensorAnchorPortLocal();

            drawCalibrationMarker(p, center, "C", QColor(255, 74, 74), 4.0, 1.8);
            drawCalibrationMarker(p, anchorPort, "A", QColor(74, 208, 255), 3.2, 1.6);
        }

        if (drawPorts)
        {
            // 保留端口着色钩子，便于以后在外部场景里叠加端口可视化。
            p->setPen(QPen(theme.border, 1));
            p->setBrush(theme.cyan);
        }
    }
}
