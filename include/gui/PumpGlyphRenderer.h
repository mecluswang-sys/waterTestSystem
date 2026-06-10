#pragma once

#include "gui/HmiGlyphTheme.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QtMath>

namespace WaterTest::GuiGlyph
{
    // =========================================================================
    // 1. 标准 HMI 图元静态坐标定义 (以图元几何中心 (0,0) 为原点)
    // =========================================================================
    
    /**
     * @brief 获取标准水泵图元的包围盒(边界区域)
     * @details 宽度 110 (从 -55 到 55)，高度 140 (从 -70 到 70)。
     * 上下对称的区域有利于在画布(GraphicsScene)中进行旋转和居中对齐。
     */
    inline QRectF pumpBoundingRect() { return QRectF(-55, -70, 110, 140); }

    /**
     * @brief 获取左侧进水口(法兰)的局部物理坐标
     * @return 位于机身最左侧边缘(-55)，中心水平线偏下方(17)的位置
     */
    inline QPointF pumpInletPortLocal() { return QPointF(-45, 17); }

    /**
     * @brief 获取右侧出水口(法兰)的局部物理坐标
     * @return 位于泵头最右侧边缘(55)，中心水平线偏上方(-15)的位置
     */
    inline QPointF pumpOutletPortLocal() { return QPointF(45, -17); }


    // =========================================================================
    // 2. 主工艺图水泵绘制函数 (drawPumpGlyph)
    // =========================================================================
    
    /**
     * @brief 在主工艺流程图上绘制标准水泵图元
     * @param p Qt2D画家指针
     * @param boundingRect 目标包围盒范围 (传入 pumpBoundingRect())
     * @param name 设备名称 (如 "PUMP01")
     * @param running 是否处于运行状态
     * @param frequencyHz 当前变频器运行频率
     * @param selected 该图元当前是否被鼠标选中高亮
     * @param theme HMI全局色彩主题配置
     */
    inline void drawPumpGlyph(
        QPainter *p,
        const QRectF &boundingRect,
        const QString &name,
        bool running,
        double frequencyHz,
        bool selected,
        const HmiGlyphTheme &theme)
    {
        if (!p) return;

        // 开启抗锯齿，确保圆形外壳、法兰和斜角线条边缘平滑无锯齿
        p->setRenderHint(QPainter::Antialiasing, true);

        // --- 步骤 2.1: 绘制选中高亮外框 ---
        if (selected)
        {
            p->setPen(QPen(theme.cyan, 3, Qt::DashLine)); // 青色、3像素宽的工控虚线
            p->setBrush(Qt::NoBrush);                     // 内部不填充颜色
            // 外框向内收缩2像素，并绘制10像素圆角的选中指示框
            p->drawRoundedRect(boundingRect.adjusted(2, 2, -2, -2), 10, 10);
        }

        // 根据运行状态判定指示灯和核心轮毂的渲染色 (运行为绿，停止为边框暗色)
        const QColor statusColor = running ? theme.green : theme.border;

        // --- 步骤 2.2: 绘制整体机身拟物化立体阴影 ---
        p->setPen(Qt::NoPen); // 阴影不需要外边框
        p->setBrush(theme.shadow);
        // 基于基础机身尺寸(-54, -42)向右下偏移(2, 3)像素，制造自然的光影下沉感
        p->drawRoundedRect(QRectF(-54, -42, 108, 140).translated(2, 3), 10, 10);

        // --- 步骤 2.3: 绘制卧式电机机身 (Motor Body) ---
        // 定义左侧卧式电机的矩形区域 (位于中心左侧)
        const QRectF motor(-50, -15, 40, 30);
        p->setBrush(theme.body);             // 填充机身底色
        p->setPen(QPen(theme.border, 2));    // 2像素的标准设备外边框
        p->drawRect(motor);

        // 绘制电机中间的中央控制面板/接线盒外壳
        p->setBrush(theme.panel);
        p->setPen(QPen(theme.border, 1));
        p->drawEllipse(QPointF(motor.left() + 20, motor.center().y()), 6, 15);

        // 绘制联轴器 (连接电机与右侧泵头的机械传动轴)
        p->setBrush(theme.metalMid);
        p->setPen(QPen(theme.border, 1));
        p->drawRect(QRectF(motor.right(), motor.center().y() - 3, 12, 6));

        // --- 步骤 2.4: 绘制右侧泵头轮毂 (Pump Head) ---
        // 绘制泵头主体圆形外壳 (圆心位于 X=22, Y=0，半径18)
        p->setBrush(theme.body);
        p->setPen(QPen(theme.border, 2));
        p->drawEllipse(QPointF(22, 0), 18, 18);

        // 绘制泵头最右侧的金属出水法兰基座
        p->setBrush(theme.metalDark);
        p->setPen(QPen(theme.border, 2));
        p->drawRect(QRectF(40, -4, 12, 8));

        // 绘制电机左上角的微型运行状态指示灯 (LED)
        p->setBrush(statusColor);
        p->setPen(QPen(theme.ink, 1));
        p->drawEllipse(QPointF(motor.left() + 10, motor.top() + 5), 4, 4);

        // 若水泵正在运行，在右侧泵头圆心内部加绘一圈动态水流辅助同心圆
        if (running)
        {
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(theme.cyan, 2));
            p->drawEllipse(QPointF(22, 0), 10, 10);
        }

        // --- 步骤 2.5: 静态文本绘制 (名称/运行状态/变频Hz) ---
        // [性能优化提示] 提取公共Font变量在原地复用，彻底断开多余的 QFont 内存深度拷贝
        QFont font = p->font();
        
        // 1. 绘制设备名称 (正上方区域)
        font.setPointSize(9);
        font.setBold(true);
        p->setFont(font);
        p->setPen(theme.text);
        p->drawText(QRectF(-55, 18, 110, 22), Qt::AlignCenter, name);

        // 2. 绘制运行状态字样 ("RUN" 或 "STOP")
        font.setPointSize(8);
        font.setBold(false);
        font.setFamily("Consolas"); // 状态和数字切换为工业常用的等宽 Consolas 字体
        p->setFont(font);
        p->setPen(theme.textMuted);
        p->drawText(QRectF(-55, 30, 110, 18), Qt::AlignCenter, running ? "RUN" : "STOP");

        // 3. 绘制实时变频频率值 (保留1位小数，如 "45.5Hz")
        p->setPen(theme.textDim);
        p->drawText(QRectF(-55, 40, 110, 18), Qt::AlignCenter, QString("%1Hz").arg(QString::number(frequencyHz, 'f', 1)));

        // --- 步骤 2.6: 绘制物理管道对接点 (Ports Flange) ---
        // 在进出水口的局部绝对坐标点上，绘制标准的青色吸附小圆圈，方便管线拓扑抓取
        p->setPen(QPen(theme.border, 1));
        p->setBrush(theme.cyan);
        p->drawEllipse(pumpInletPortLocal(), 4, 4);  // 绘制左进水口
        p->drawEllipse(pumpOutletPortLocal(), 4, 4); // 绘制右出水口
    }


    // =========================================================================
    // 3. 弹出对话框/控制面板水泵预览函数 (drawPumpPreviewGlyph - 带动感叶片旋转)
    // =========================================================================
    
    /**
     * @brief 在控制弹窗或预览面板中绘制自适应大小、带高频旋转叶片的水泵仪表盘
     * @note 尺寸完全基于外部传入的 boundingRect 动态按比例计算，支持视口无级缩放
     */
    inline void drawPumpPreviewGlyph(
        QPainter *p,
        const QRectF &boundingRect,
        const QString &name,
        bool running,
        double frequencyHz,
        bool selected,
        const HmiGlyphTheme &theme)
    {
        if (!p) return;

        p->setRenderHint(QPainter::Antialiasing, true);

        // --- 步骤 3.1: 选中高亮边框渲染 ---
        if (selected)
        {
            p->setPen(QPen(theme.cyan, 3, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(boundingRect.adjusted(2, 2, -2, -2), 12, 12);
        }

        // --- 步骤 3.2: 基础比例因子与几何中心计算 (实现自适应的核心) ---
        const qreal w = boundingRect.width();
        const qreal h = boundingRect.height();
        const QPointF center = boundingRect.center();          // 获取传入视口的几何中点
        const qreal radius = qMin(w, h) * 0.25;                // 泵体圆形外壳的半径设定为视口最短边的 25%
        const QColor brightBlue(70, 190, 255);                 // 科技流体亮蓝色
        const QColor darkBlue(20, 50, 90, 150);                // 泵体内腔深邃暗蓝半透明色
        const QColor fluidColor(40, 150, 230);                 // 流动液体色块

        // --- 步骤 3.3: 绘制科幻风格 HMI 背景网格线 (步长 40 像素) ---
        p->setPen(QPen(QColor(20, 30, 45, 25), 1)); // 极度淡雅的内衬网格线
        for (qreal x = boundingRect.left(); x < boundingRect.right(); x += 40.0)
            p->drawLine(QPointF(x, boundingRect.top()), QPointF(x, boundingRect.bottom()));
        for (qreal y = boundingRect.top(); y < boundingRect.bottom(); y += 40.0)
            p->drawLine(QPointF(boundingRect.left(), y), QPointF(boundingRect.right(), y));

        // 根据核心半径动态推导进出水管的物理尺寸
        const qreal pipeW = radius * 1.0; // 管道向外横向延伸的长度
        const qreal pipeH = radius * 0.25; // 管道的垂直外径粗细

        p->setPen(QPen(brightBlue, 2));
        p->setBrush(Qt::NoBrush);

        // --- 步骤 3.4: 绘制动态错位管路 (左侧偏下进水，右侧偏上出水) ---
        QRectF leftPipe(center.x() - radius - pipeW + radius * 0.5, center.y() + radius * 0.5, pipeW, pipeH);
        QRectF rightPipe(center.x() + radius - radius * 0.5, center.y() - radius * 0.5 - pipeH, pipeW, pipeH);
        
        p->drawRect(leftPipe);  // 绘制左管道外框
        p->drawRect(rightPipe); // 绘制右管道外框

        // [Bug已修复] 基于当前管道尺寸动态计算液体内衬，确保无论如何缩放，水流绝不越界或错位
        qreal padX = pipeW * 0.25;
        qreal padY = pipeH * 0.25;
        p->fillRect(leftPipe.adjusted(padX, padY, -padX, -padY), fluidColor);
        p->fillRect(rightPipe.adjusted(padX, padY, -padX, -padY), fluidColor);

        // --- 步骤 3.5: 绘制大泵体圆盘内腔 ---
        p->drawEllipse(center, radius + 5, radius + 5); // 外壳装饰金属圈
        p->setBrush(darkBlue);
        p->drawEllipse(center, radius, radius);         // 叶片旋转工作室暗色背景

        // --- 步骤 3.6: 绘制高速旋转的 7 叶片转子 ---
        p->save();              // 核心坐标系隔离：保存当前的平移与缩放矩阵
        p->translate(center);   // 将坐标系原点临死挪移到水泵正中心
        if (running)
        {
            // 基于硬件变频 Hz 频率乘以动力系数(8.0)计算当前旋转角速度
            // 使用 fmod 将角度死死压制在 [0, 360) 范围内，彻底阻断数值长期运行导致浮点数溢出卡死
            const qreal angle = -fmod(frequencyHz * 8.0, 360.0);
            p->rotate(angle);   // 旋转画布坐标系
        }

        // 设置叶片画笔：圆头线条端点(RoundCap)能让转子曲线末梢看起来非常圆润细腻
        p->setPen(QPen(brightBlue, 3, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(Qt::NoBrush);

        // [性能重构点] 将贝塞尔曲线的创建挪移到 for 循环外部。利用单路 Path 原地 rotate 复用，
        // 彻底砍断了高频动画时 QPainterPath 在循环体内部频繁申请/释放物理内存的灾难性掉帧开销。
        QPainterPath bladePath;
        const QPointF pStart(radius * 0.15, 0);                 // 叶片根部起点
        const QPointF pControl(radius * 0.5, radius * 0.4);     // 二阶贝塞尔曲线控制点(控制曲率)
        const QPointF pEnd(radius * 0.95 * qCos(qDegreesToRadians(30.0)), radius * 0.95 * qSin(qDegreesToRadians(30.0))); // 转子末梢落点
        bladePath.moveTo(pStart);
        bladePath.quadTo(pControl, pEnd); // 灌注曲线形状

        const int numBlades = 7;                            // 规定 7 叶片工业标准叶轮
        const qreal angleStep = 360.0 / numBlades;          // 计算每片叶片彼此之间的发散夹角
        for (int i = 0; i < numBlades; ++i)
        {
            p->drawPath(bladePath);                         // 绘制这一枚叶片
            p->rotate(angleStep);                           // 坐标系原地累加旋转，高效准备绘制下一片
        }
        p->restore(); // 完美恢复坐标系，退回到以面板左上角为(0,0)的基础画布状态

        // 绘制转子物理中心金属轴心销顶
        p->setPen(Qt::NoPen);
        p->setBrush(brightBlue);
        p->drawEllipse(center, 6, 6);

        // --- 步骤 3.7: 弹出组件文本信息渲染 (自适应视口相对边界) ---
        QFont font = p->font();
        
        // 1. 绘制顶部设备铭牌
        font.setPointSize(9);
        font.setBold(true);
        p->setFont(font);
        p->setPen(theme.text);
        p->drawText(QRectF(boundingRect.left(), boundingRect.top() + 18, boundingRect.width(), 22), Qt::AlignCenter, name);

        // 2. 绘制中下部大字运行状态显示
        font.setPointSize(8);
        font.setBold(false);
        font.setFamily("Consolas");
        p->setFont(font);
        p->setPen(theme.textMuted);
        p->drawText(QRectF(boundingRect.left(), boundingRect.bottom() - 38, boundingRect.width(), 18), Qt::AlignCenter, running ? "RUN" : "STOP");

        // 3. 绘制底部大字精确变频器反馈频率
        p->setPen(theme.textDim);
        p->drawText(QRectF(boundingRect.left(), boundingRect.bottom() - 24, boundingRect.width(), 18), Qt::AlignCenter, QString("%1Hz").arg(QString::number(frequencyHz, 'f', 1)));
    }
}