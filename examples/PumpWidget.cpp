#include "PumpWidget.h"
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QRectF>
#include <QtMath>

PumpWidget::PumpWidget(QWidget *parent)
    : QWidget(parent), m_rotationAngle(0.0f) 
{
    // 创建定时器，每 30 毫秒刷新一次界面，实现丝滑动画
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_rotationAngle -= 2.0f; // 逆时针旋转
        if (m_rotationAngle < -360.0f) m_rotationAngle += 360.0f;
        update(); // 触发 paintEvent 重新绘制
    });
    m_timer->start(30);

    // 设置一个暗色背景，凸显科技蓝色调
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(10, 15, 28));
    setAutoFillBackground(true);
    setPalette(pal);
}

void PumpWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    // 开启抗锯齿，让边缘和曲线非常平滑
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    QPointF center(w / 2.0, h / 2.0);
    qreal radius = qMin(w, h) * 0.25; // 根据窗口大小自动缩放泵的尺寸

    // 定义发光科技蓝颜色
    QColor brightBlue(70, 190, 255);
    QColor darkBlue(20, 50, 90, 150);
    QColor fluidColor(40, 150, 230);

    // --- 1. 绘制背景网格（还原原图暗格） ---
    painter.setPen(QPen(QColor(20, 30, 45), 1));
    for (int x = 0; x < w; x += 40) painter.drawLine(x, 0, x, h);
    for (int y = 0; y < h; y += 40) painter.drawLine(0, y, w, y);

    // --- 2. 绘制管道 ---
    QPen pipePen(brightBlue, 2);
    painter.setPen(pipePen);
    painter.setBrush(Qt::NoBrush);

    qreal pipeW = radius * 1.2;
    qreal pipeH = radius * 0.5;

    // 左下出水管
    QRectF leftPipe(center.x() - radius - pipeW + radius*0.2, center.y() + radius*0.2, pipeW, pipeH);
    painter.drawRect(leftPipe);
    // 管道内流体块
    painter.fillRect(QRectF(leftPipe.x() + 10, leftPipe.y() + 10, 30, pipeH - 20), fluidColor);

    // 右上进水管
    QRectF rightPipe(center.x() + radius - radius*0.2, center.y() - radius*0.2 - pipeH, pipeW, pipeH);
    painter.drawRect(rightPipe);
    // 管道内流体块
    painter.fillRect(QRectF(rightPipe.right() - 40, rightPipe.y() + 10, 30, pipeH - 20), fluidColor);

    // --- 3. 绘制泵外壳 ---
    // 外圈大圆
    painter.drawEllipse(center, radius + 5, radius + 5);
    // 内圈填充半透明深蓝
    painter.setBrush(darkBlue);
    painter.drawEllipse(center, radius, radius);

    // --- 4. 绘制动态旋转的叶片 ---
    painter.save(); // 保存当前画布状态
    painter.translate(center); // 将坐标系原点移到泵中心
    painter.rotate(m_rotationAngle); // 旋转画布

    QPen bladePen(brightBlue, 3, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(bladePen);
    painter.setBrush(Qt::NoBrush);

    int numBlades = 7;
    for (int i = 0; i < numBlades; ++i) {
        painter.save();
        painter.rotate(i * (360.0 / numBlades));

        // 使用 QPainterPath 构建完美的贝塞尔曲线来表达旋涡状叶片
        QPainterPath bladePath;
        QPointF pStart(radius * 0.15, 0);
        QPointF pControl(radius * 0.5, radius * 0.4); // 控制点决定弯曲度
        QPointF pEnd(radius * 0.95 * qCos(qDegreesToRadians(30.0)), radius * 0.95 * qSin(qDegreesToRadians(30.0)));
        
        bladePath.moveTo(pStart);
        bladePath.quadTo(pControl, pEnd); // 绘制二次贝塞尔曲线
        
        painter.drawPath(bladePath);
        painter.restore();
    }
    painter.restore(); // 恢复画布状态

    // --- 5. 绘制中心轴 ---
    painter.setPen(Qt::NoPen);
    painter.setBrush(brightBlue);
    painter.drawEllipse(center, 6, 6);
}