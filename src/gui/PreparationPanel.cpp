/**
 * @file PreparationPanel.cpp
 * @brief 测试准备区面板实现（HMI流程图式）
 */

#include "gui/PreparationPanel.h"
#include "DeviceManager.h"

#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QFrame>
#include <QGroupBox>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QGraphicsSceneMouseEvent>
#include <QStyle>

namespace WaterTest
{

    namespace
    {
        // ===== UI-design 视觉 token（与 docs/UI-design 统一） =====
        static const QColor kUiBg("#1a1a1a");
        static const QColor kUiPanel("#2a2a2a");
        static const QColor kUiBody("#333");
        static const QColor kUiBorder("#666");
        static const QColor kUiBorderWeak("#444");
        static const QColor kUiText("#c9d1d9");
        static const QColor kUiTextMuted("#999");
        static const QColor kUiTextDim("#6b7280");
        static const QColor kUiCyan("#0af");
        static const QColor kUiGreen("#0f0");
        static const QColor kUiRed("#f00");
        static const QColor kUiOrange("#fa0");
        static const QColor kUiPurple("#a0f");

        static QString fmtMPa(double pa)
        {
            return QString::number(pa / 1e6, 'f', 3) + " MPa";
        }

        constexpr qreal kPumpItemWidth = 110;
        constexpr qreal kValveItemWidth = 104;
        constexpr qreal kSensorItemWidth = 104;
        constexpr qreal kTankItemWidth = 160;
        constexpr qreal kPumpItemHeight = 140;
        constexpr qreal kValveItemHeight = 100;
        constexpr qreal kSensorItemHeight = 104;
        constexpr qreal kTankItemHeight = 240;
        constexpr qreal kPumpScale = 1.5;
        constexpr qreal kValveScale = 1.5;
        constexpr qreal kSensorScale = 1.5;
        constexpr qreal kTankScale = 1.5;

        static void setBadgeTone(QLabel *label, const char *tone)
        {
            if (!label)
                return;
            label->setObjectName("badge");
            label->setProperty("tone", tone);
            label->style()->unpolish(label);
            label->style()->polish(label);
        }

        struct HmiLayoutConfig
        {
            // 以 pumpBase 为起点，其他节点根据行列间距和偏移量自动计算，避免手动搬坐标
            QPointF pumpBase{170, 160};
            qreal pumpRowSpacing = 400;
            qreal valveHorizontalOffset = 250;
            qreal sensorVerticalSpacing = 220;
            qreal sensor3HorizontalOffset = 230;
            qreal sensor3VerticalOffset = 60;
            qreal tankHorizontalDistance = 270;
            qreal tankVerticalOffset = 140;

            qreal pumpToValveLeadOffset = 100;
            qreal pumpToValveTrailOffset = 80;
            qreal valveToSensorVerticalStart = 60;
            qreal valveToSensorVerticalEnd = 40;
            qreal valveToSensor3Horizontal = 60;
            qreal sensor3BranchStartX = 70;
            qreal valveToTankHorizontal = 60;
            qreal tankEntryOffsetX = 100;

            // 室外水池配置
            QPointF outdoorPoolBase{30, 280};   // 紧靠最左侧
            qreal outdoorPoolToPumpOffset = 60; // 室外水池到泵的管路偏移

            QPointF pump1() const { return pumpBase; }
            QPointF pump2() const { return QPointF(pumpBase.x(), pumpBase.y() + pumpRowSpacing); }
            QPointF valve1() const { return QPointF(pump1().x() + valveHorizontalOffset, pump1().y()); }
            QPointF valve2() const { return QPointF(pump2().x() + valveHorizontalOffset, pump2().y()); }
            QPointF sensor1() const { return QPointF(valve1().x(), valve1().y() + sensorVerticalSpacing); }
            QPointF sensor2() const { return QPointF(valve2().x(), valve2().y() + sensorVerticalSpacing); }
            QPointF sensor3() const { return QPointF(valve1().x() + sensor3HorizontalOffset, valve1().y() - sensor3VerticalOffset); }
            QPointF tank() const { return QPointF(sensor3().x() + tankHorizontalDistance, sensor3().y() + tankVerticalOffset); }
            QPointF outdoorPool() const { return outdoorPoolBase; }

            qreal pumpHalfWidth() const { return (kPumpItemWidth * kPumpScale) / 2; }
            qreal valveHalfWidth() const { return (kValveItemWidth * kValveScale) / 2; }
        };

        // ========== 拟物图元（蓝底流程图：泵/阀/传感器/分水罐）==========
        class PumpItem : public QGraphicsItem
        {
        public:
            explicit PumpItem(const QString &name)
                : m_name(name), m_running(false), m_frequencyHz(0.0)
            {
                setCacheMode(DeviceCoordinateCache);
            }

            QRectF boundingRect() const override { return QRectF(-55, -45, 110, 140); }

            static constexpr qreal width() { return 110; }
            static constexpr qreal height() { return 140; }

            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (change == ItemPositionChange && scene())
                {
                    // 网格吸附（10像素网格）
                    QPointF newPos = value.toPointF();
                    qreal gridSize = 10.0;
                    qreal xV = qRound(newPos.x() / gridSize) * gridSize;
                    qreal yV = qRound(newPos.y() / gridSize) * gridSize;
                    return QPointF(xV, yV);
                }
                return QGraphicsItem::itemChange(change, value);
            }
            qreal x() const { return pos().x(); }
            qreal y() const { return pos().y(); }

            void setRunning(bool running)
            {
                if (m_running == running)
                    return;
                m_running = running;
                update();
            }

            void setFrequency(double hz)
            {
                if (qFuzzyCompare(m_frequencyHz, hz))
                    return;
                m_frequencyHz = hz;
                update();
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                // 选中状态高亮边框
                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 10, 10);
                }

                // 对齐 docs/UI-design PumpComponent：扁平深色结构 + #666 描边 + 状态灯
                const QColor statusColor = m_running ? kUiGreen : kUiBorder;
                const QColor borderColor = kUiBorder;

                // 轻阴影（选中态暂无，这里做轻量立体感）
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(0, 0, 0, 90));
                p->drawRoundedRect(QRectF(-54, -42, 108, 140).translated(2, 3), 10, 10);

                // 电机主体
                const QRectF motor(-50, -15, 40, 30);
                p->setBrush(kUiBody);
                p->setPen(QPen(borderColor, 2));
                p->drawRect(motor);

                // 电机端盖（椭圆）
                p->setBrush(kUiPanel);
                p->setPen(QPen(borderColor, 1));
                p->drawEllipse(QPointF(motor.left() + 20, motor.center().y()), 6, 15);

                // 轴
                p->setBrush(QColor("#555"));
                p->setPen(QPen(QColor("#777"), 1));
                p->drawRect(QRectF(motor.right(), motor.center().y() - 3, 12, 6));

                // 泵壳（圆形）
                p->setBrush(kUiBody);
                p->setPen(QPen(borderColor, 2));
                p->drawEllipse(QPointF(22, 0), 18, 18);

                // 出口
                p->setBrush(QColor("#444"));
                p->setPen(QPen(borderColor, 2));
                p->drawRect(QRectF(40, -4, 12, 8));

                // 状态灯
                p->setBrush(statusColor);
                p->setPen(QPen(QColor("#000"), 1));
                p->drawEllipse(QPointF(motor.left() + 10, motor.top() + 5), 4, 4);

                // 运行提示环（不做动画，静态 #0af 内圈）
                if (m_running)
                {
                    p->setBrush(Qt::NoBrush);
                    p->setPen(QPen(kUiCyan, 2));
                    p->drawEllipse(QPointF(22, 0), 10, 10);
                }

                // 文本（名称 + 状态/频率）
                p->setPen(kUiText);
                QFont nameFont = p->font();
                nameFont.setPointSize(9);
                nameFont.setBold(true);
                p->setFont(nameFont);
                p->drawText(QRectF(-55, 18, 110, 22), Qt::AlignCenter, m_name);

                QFont statusFont = p->font();
                statusFont.setPointSize(8);
                statusFont.setBold(false);
                statusFont.setFamily("Consolas");
                p->setFont(statusFont);
                p->setPen(kUiTextMuted);
                const QString statusText = m_running ? "RUN" : "STOP";
                p->drawText(QRectF(-55, -34, 110, 18), Qt::AlignCenter, statusText);

                const QString freqText = QString("%1Hz").arg(QString::number(m_frequencyHz, 'f', 1));
                p->setPen(kUiTextDim);
                p->drawText(QRectF(-55, 40, 110, 18), Qt::AlignCenter, freqText);

                const QString voltText = QString("%1V").arg(QString::number(m_voltage, 'f', 1));
                p->setPen(kUiTextDim);
                p->drawText(QRectF(-55, 50, 110, 18), Qt::AlignCenter, voltText);

                const QString ampText = QString("%1A").arg(QString::number(m_amperage, 'f', 1));
                p->setPen(kUiTextDim);
                p->drawText(QRectF(-55, 60, 110, 18), Qt::AlignCenter, ampText);

                const QString powerText = QString("%1W").arg(QString::number(m_power, 'f', 1));
                p->setPen(kUiTextDim);
                p->drawText(QRectF(-55, 70, 110, 18), Qt::AlignCenter, powerText);
            }

        private:
            QString m_name;
            bool m_running;
            double m_frequencyHz;
            double m_voltage;
            double m_amperage;
            double m_power;
        };

        class ValveItem : public QGraphicsItem
        {
        public:
            explicit ValveItem(const QString &name)
                : m_name(name), m_open(false), m_degree(0)
            {
                setCacheMode(DeviceCoordinateCache);
            }

            QRectF boundingRect() const override { return QRectF(-52, -40, 104, 100); }

            static constexpr qreal width() { return 104; }
            static constexpr qreal height() { return 100; }
            qreal x() const { return pos().x(); }
            qreal y() const { return pos().y(); }

            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (change == ItemPositionChange && scene())
                {
                    // 网格吸附（10像素网格）
                    QPointF newPos = value.toPointF();
                    qreal gridSize = 10.0;
                    qreal xV = qRound(newPos.x() / gridSize) * gridSize;
                    qreal yV = qRound(newPos.y() / gridSize) * gridSize;
                    return QPointF(xV, yV);
                }
                return QGraphicsItem::itemChange(change, value);
            }

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
                p->setRenderHint(QPainter::Antialiasing, true);

                // 选中状态高亮边框
                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 10, 10);
                }

                // 对齐 docs/UI-design ValveComponent：菱形阀体 + 执行器 + 位置指示
                const QColor borderColor = kUiBorder;
                const QColor discColor = m_open ? kUiGreen : kUiRed;
                const QColor statusColor = discColor;

                // 轻阴影
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(0, 0, 0, 90));
                p->drawRoundedRect(QRectF(-40, -38, 80, 98).translated(2, 3), 10, 10);

                // 执行器
                p->setBrush(kUiBody);
                p->setPen(QPen(borderColor, 2));
                p->drawRoundedRect(QRectF(-15, -34, 30, 18), 2, 2);

                // 执行器字母
                p->setPen(kUiTextMuted);
                QFont mf = p->font();
                mf.setPointSize(8);
                mf.setBold(true);
                mf.setFamily("Consolas");
                p->setFont(mf);
                p->drawText(QRectF(-15, -34, 30, 18), Qt::AlignCenter, "M");

                // 状态灯
                p->setBrush(statusColor);
                p->setPen(QPen(QColor("#000"), 1));
                p->drawEllipse(QPointF(-11, -30), 3, 3);

                // 阀杆
                p->setBrush(QColor("#555"));
                p->setPen(QPen(kUiBorder, 1));
                p->drawRect(QRectF(-2, -16, 4, 12));

                // 阀体（菱形）
                QPainterPath diamond;
                diamond.moveTo(0, -2);
                diamond.lineTo(26, 12);
                diamond.lineTo(0, 26);
                diamond.lineTo(-26, 12);
                diamond.closeSubpath();
                p->setBrush(kUiBody);
                p->setPen(QPen(borderColor, 2));
                p->drawPath(diamond);

                // 阀瓣（随开度旋转 0~90 度）
                const double rotation = (m_degree / 100.0) * 90.0;
                p->save();
                p->translate(0, 12);
                p->rotate(rotation);
                p->setBrush(discColor);
                p->setPen(QPen(QColor("#000"), 1));
                p->drawEllipse(QPointF(0, 0), 16, 3);
                p->restore();

                // 位置指示圆盘
                p->setBrush(kUiPanel);
                p->setPen(QPen(kUiBorder, 1));
                p->drawEllipse(QPointF(24, -26), 6, 6);
                // 指针
                const double ang = (rotation - 90.0) * 3.14159 / 180.0;
                p->setPen(QPen(statusColor, 2, Qt::SolidLine, Qt::RoundCap));
                p->drawLine(QPointF(24, -26), QPointF(24 + 5 * std::cos(ang), -26 + 5 * std::sin(ang)));

                // 文本
                p->setPen(kUiText);
                QFont f = p->font();
                f.setPointSize(9);
                f.setBold(true);
                p->setFont(f);
                p->drawText(QRectF(-52, 30, 104, 18), Qt::AlignCenter, m_name);

                QFont f2 = p->font();
                f2.setPointSize(8);
                f2.setBold(false);
                f2.setFamily("Consolas");
                p->setFont(f2);
                p->setPen(kUiTextMuted);
                const QString s = QString("POS:%1%").arg(QString::number(m_degree, 'f', 0));
                p->drawText(QRectF(-52, 40, 104, 16), Qt::AlignCenter, s);
            }

        private:
            QString m_name;
            bool m_open;
            double m_degree;
        };

        class SensorItem : public QGraphicsItem
        {
        public:
            explicit SensorItem(const QString &name)
                : m_name(name), m_pressureMPa(0.0)
            {
                setCacheMode(DeviceCoordinateCache);
            }

            // 卡片 + 引线
            QRectF boundingRect() const override { return QRectF(-52, -46, 104, 104); }

            static constexpr qreal width() { return 104; }
            static constexpr qreal height() { return 104; }
            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (change == ItemPositionChange && scene())
                {
                    // 网格吸附（10像素网格）
                    QPointF newPos = value.toPointF();
                    qreal gridSize = 10.0;
                    qreal xV = qRound(newPos.x() / gridSize) * gridSize;
                    qreal yV = qRound(newPos.y() / gridSize) * gridSize;
                    return QPointF(xV, yV);
                }
                return QGraphicsItem::itemChange(change, value);
            }

            qreal x() const { return pos().x(); }
            qreal y() const { return pos().y(); }

            void setPressureMPa(double v)
            {
                if (qFuzzyCompare(m_pressureMPa, v))
                    return;
                m_pressureMPa = v;
                update();
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                // 选中状态高亮边框
                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 8, 8);
                }

                // 对齐 docs/UI-design SensorComponent：卡片式传感器 + 虚线引线 + 类型色圆点
                const QColor typeColor = kUiPurple; // pressure
                const QColor borderColor = kUiBorder;
                const QColor valueColor = typeColor;

                const QRectF card(-45, -40, 90, 56);
                // 卡片底
                p->setPen(QPen(borderColor, 2));
                p->setBrush(kUiPanel);
                p->drawRect(card);

                // Tag
                p->setPen(kUiTextDim);
                QFont tagFont = p->font();
                tagFont.setPointSize(8);
                tagFont.setBold(false);
                tagFont.setFamily("Consolas");
                p->setFont(tagFont);
                p->drawText(QRectF(card.left() + 6, card.top() + 4, card.width() - 12, 12), Qt::AlignLeft | Qt::AlignVCenter, m_name);

                // Value
                QFont valFont = p->font();
                valFont.setPointSize(14);
                valFont.setBold(true);
                valFont.setFamily("Consolas");
                p->setFont(valFont);
                p->setPen(valueColor);
                const QString v = QString::number(m_pressureMPa, 'f', 3);
                p->drawText(QRectF(card.left() + 6, card.top() + 18, card.width() - 12, 22), Qt::AlignLeft | Qt::AlignVCenter, v);

                // Unit
                QFont unitFont = p->font();
                unitFont.setPointSize(8);
                unitFont.setBold(false);
                unitFont.setFamily("Consolas");
                p->setFont(unitFont);
                p->setPen(QColor("#9ca3af"));
                p->drawText(QRectF(card.left() + 6 + 52, card.top() + 24, card.width() - 58, 14), Qt::AlignLeft | Qt::AlignVCenter, "MPa");

                // Status（简化：固定 GOOD）
                p->setBrush(kUiGreen);
                p->setPen(Qt::NoPen);
                p->drawRect(QRectF(card.left() + 6, card.bottom() - 10, 6, 6));
                p->setPen(kUiTextDim);
                p->setFont(unitFont);
                p->drawText(QRectF(card.left() + 16, card.bottom() - 12, card.width() - 22, 10), Qt::AlignLeft | Qt::AlignVCenter, "GOOD");

                // 引线（虚线）
                p->setPen(QPen(kUiBorder, 1.5, Qt::DashLine, Qt::RoundCap));
                p->drawLine(QPointF(0, card.bottom()), QPointF(0, card.bottom() + 16));
                p->setBrush(typeColor);
                p->setPen(QPen(QColor("#000"), 1));
                p->drawEllipse(QPointF(0, card.bottom() + 16), 3, 3);
            }

        private:
            QString m_name;
            double m_pressureMPa;
        };

        class TankItem : public QGraphicsItem
        {
        public:
            explicit TankItem(const QString &name)
                : m_name(name), m_fillPercent(0.0), m_filling(false), m_pressureMPa(0.0)
            {
                setCacheMode(DeviceCoordinateCache);
            }

            QRectF boundingRect() const override { return QRectF(-70, -90, 160, 240); }

            static constexpr qreal width() { return 160; }
            static constexpr qreal height() { return 240; }
            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (change == ItemPositionChange && scene())
                {
                    // 网格吸附（10像素网格）
                    QPointF newPos = value.toPointF();
                    qreal gridSize = 10.0;
                    qreal xV = qRound(newPos.x() / gridSize) * gridSize;
                    qreal yV = qRound(newPos.y() / gridSize) * gridSize;
                    return QPointF(xV, yV);
                }
                return QGraphicsItem::itemChange(change, value);
            }

            qreal x() const { return pos().x(); }
            qreal y() const { return pos().y(); }

            void setFillPercent(double p)
            {
                p = std::clamp(p, 0.0, 100.0);
                if (qFuzzyCompare(m_fillPercent, p))
                    return;
                m_fillPercent = p;
                update();
            }

            void setFilling(bool f)
            {
                if (m_filling == f)
                    return;
                m_filling = f;
                update();
            }

            void setPressureMPa(double v)
            {
                if (qFuzzyCompare(m_pressureMPa, v))
                    return;
                m_pressureMPa = v;
                update();
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                // 选中状态高亮边框
                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 18, 18);
                }

                // 对齐 docs/UI-design TankComponent：圆柱罐体（顶/壳/底）+ 半透明液位
                const QColor borderColor = kUiBorder;
                const QColor levelColor = kUiCyan;

                // 轻阴影
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(0, 0, 0, 90));
                p->drawRoundedRect(QRectF(-58, -88, 116, 176).translated(3, 4), 18, 18);

                // 罐体几何（在既有 boundingRect 内做缩放版）
                const QRectF roof(-46, -74, 92, 18);
                const QRectF shell(-46, -65, 92, 120);
                const QRectF bottom(-46, 55, 92, 18);

                // 顶/底椭圆
                p->setBrush(kUiBody);
                p->setPen(QPen(borderColor, 2));
                p->drawEllipse(roof);
                p->drawEllipse(bottom);

                // 壳
                p->setBrush(kUiPanel);
                p->setPen(QPen(borderColor, 2));
                p->drawRect(shell);

                // 液位（clip 到 shell）
                const double level = std::clamp(m_fillPercent, 0.0, 100.0);
                const double fillH = shell.height() * (level / 100.0);
                QRectF liquid(shell.left() + 2, shell.bottom() - fillH, shell.width() - 4, fillH);
                p->save();
                p->setClipRect(shell);
                p->setBrush(QColor(levelColor.red(), levelColor.green(), levelColor.blue(), 120));
                p->setPen(Qt::NoPen);
                p->drawRect(liquid);
                // 液面
                p->setPen(QPen(levelColor, 2));
                p->drawLine(QPointF(liquid.left(), liquid.top()), QPointF(liquid.right(), liquid.top()));
                p->restore();

                // 入口/出口（简化）
                p->setBrush(QColor("#444"));
                p->setPen(QPen(borderColor, 2));
                p->drawRect(QRectF(shell.left() - 18, shell.center().y() - 5, 18, 10));
                p->drawRect(QRectF(shell.right(), shell.bottom() - 10, 18, 10));

                // 支撑
                p->setBrush(QColor("#444"));
                p->setPen(QPen(kUiBorder, 1));
                p->drawRect(QRectF(shell.left() + 10, bottom.bottom() - 2, 8, 20));
                p->drawRect(QRectF(shell.right() - 18, bottom.bottom() - 2, 8, 20));
                p->drawRect(QRectF(shell.left() + 6, bottom.bottom() + 16, shell.width() - 12, 4));

                // 人孔
                p->setBrush(QColor("#444"));
                p->setPen(QPen(QColor("#777"), 1.5));
                p->drawEllipse(QRectF(-8, roof.top() + 3, 16, 6));

                // 状态灯（加水中绿，否则灰）
                const QColor lamp = m_filling ? kUiGreen : kUiBorder;
                p->setBrush(lamp);
                p->setPen(QPen(QColor("#000"), 1));
                p->drawEllipse(QPointF(shell.right() + 14, roof.center().y()), 4, 4);

                // 文本
                p->setPen(kUiText);
                QFont f = p->font();
                f.setPointSize(10);
                f.setBold(true);
                p->setFont(f);
                p->drawText(QRectF(-70, 78, 140, 18), Qt::AlignCenter, m_name);

                QFont f2 = p->font();
                f2.setPointSize(8);
                f2.setBold(false);
                f2.setFamily("Consolas");
                p->setFont(f2);
                p->setPen(kUiTextMuted);
                p->drawText(QRectF(-70, 62, 140, 16), Qt::AlignCenter,
                            QString("LV:%1%%  PT:%2MPa").arg(QString::number(level, 'f', 0)).arg(QString::number(m_pressureMPa, 'f', 3)));
            }

        private:
            QString m_name;
            double m_fillPercent;
            bool m_filling;
            double m_pressureMPa;
        };

        // ========== 室外水池 ==========
        class OutdoorPoolItem : public QGraphicsItem
        {
        public:
            explicit OutdoorPoolItem(const QString &name)
                : m_name(name), m_waterLevel(80.0)
            {
                setCacheMode(DeviceCoordinateCache);
            }

            QRectF boundingRect() const override { return QRectF(-80, -100, 160, 200); }

            static constexpr qreal width() { return 160; }
            static constexpr qreal height() { return 200; }
            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (change == ItemPositionChange && scene())
                {
                    // 网格吸附（10像素网格）
                    QPointF newPos = value.toPointF();
                    qreal gridSize = 10.0;
                    qreal xV = qRound(newPos.x() / gridSize) * gridSize;
                    qreal yV = qRound(newPos.y() / gridSize) * gridSize;
                    return QPointF(xV, yV);
                }
                return QGraphicsItem::itemChange(change, value);
            }

            void setWaterLevel(double level)
            {
                level = std::clamp(level, 0.0, 100.0);
                if (qFuzzyCompare(m_waterLevel, level))
                    return;
                m_waterLevel = level;
                update();
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                // 选中状态高亮边框
                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 12, 12);
                }

                const QColor borderColor = kUiBorder;
                const QColor waterColor = QColor("#0af");

                // 轻阴影
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(0, 0, 0, 90));
                p->drawRoundedRect(QRectF(-78, -98, 156, 196).translated(3, 4), 12, 12);

                // 水池主体（矩形池体）
                const QRectF poolBody(-70, -90, 140, 160);
                p->setBrush(kUiPanel);
                p->setPen(QPen(borderColor, 3));
                p->drawRoundedRect(poolBody, 8, 8);

                // 水位（半透明蓝色）
                const double level = std::clamp(m_waterLevel, 0.0, 100.0);
                const double fillH = poolBody.height() * (level / 100.0);
                QRectF water(poolBody.left() + 3, poolBody.bottom() - fillH, poolBody.width() - 6, fillH);
                p->save();
                p->setClipRect(poolBody);
                p->setBrush(QColor(waterColor.red(), waterColor.green(), waterColor.blue(), 120));
                p->setPen(Qt::NoPen);
                p->drawRect(water);
                // 水面线
                p->setPen(QPen(waterColor, 2));
                p->drawLine(QPointF(water.left(), water.top()), QPointF(water.right(), water.top()));
                p->restore();

                // 出水口（两个出口在右侧）
                p->setBrush(QColor("#444"));
                p->setPen(QPen(borderColor, 2));
                // 上出口（到P1）
                p->drawRect(QRectF(poolBody.right(), poolBody.top() + 30, 18, 10));
                // 下出口（到P2）
                p->drawRect(QRectF(poolBody.right(), poolBody.bottom() - 40, 18, 10));

                // 底座
                p->setBrush(QColor("#333"));
                p->setPen(QPen(kUiBorder, 2));
                p->drawRect(QRectF(-75, poolBody.bottom(), 150, 8));

                // 文本
                p->setPen(kUiText);
                QFont f = p->font();
                f.setPointSize(10);
                f.setBold(true);
                p->setFont(f);
                p->drawText(QRectF(-80, 82, 160, 18), Qt::AlignCenter, m_name);

                QFont f2 = p->font();
                f2.setPointSize(8);
                f2.setBold(false);
                f2.setFamily("Consolas");
                p->setFont(f2);
                p->setPen(kUiTextMuted);
                p->drawText(QRectF(-80, 68, 160, 16), Qt::AlignCenter,
                            QString("水位:%1%%").arg(QString::number(level, 'f', 0)));
            }

        private:
            QString m_name;
            double m_waterLevel;
        };

        // ========== 动态管道（跟随图元移动） ==========
        class DynamicPipe : public QGraphicsItem
        {
        public:
            DynamicPipe(QGraphicsItem *startItem, QPointF startOffset,
                        QGraphicsItem *endItem, QPointF endOffset,
                        const QList<QPointF> &waypoints = {})
                : m_startItem(startItem), m_startOffset(startOffset),
                  m_endItem(endItem), m_endOffset(endOffset),
                  m_waypoints(waypoints), m_flowing(false)
            {
                setZValue(-1);
            }

            QRectF boundingRect() const override
            {
                QPointF start = getStartPos();
                QPointF end = getEndPos();
                qreal minX = qMin(start.x(), end.x()) - 20;
                qreal minY = qMin(start.y(), end.y()) - 20;
                qreal maxX = qMax(start.x(), end.x()) + 20;
                qreal maxY = qMax(start.y(), end.y()) + 20;

                for (const QPointF &wp : m_waypoints)
                {
                    minX = qMin(minX, wp.x() - 20);
                    minY = qMin(minY, wp.y() - 20);
                    maxX = qMax(maxX, wp.x() + 20);
                    maxY = qMax(maxY, wp.y() + 20);
                }

                return QRectF(minX, minY, maxX - minX, maxY - minY);
            }

            void setFlowing(bool flowing)
            {
                if (m_flowing != flowing)
                {
                    m_flowing = flowing;
                    update();
                }
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                QPointF start = getStartPos();
                QPointF end = getEndPos();

                QPen borderPen(QColor("#666"), 12.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                QPen innerPen(QColor("#2a2a2a"), 8.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

                // 构建路径
                QPainterPath path;
                path.moveTo(start);

                if (m_waypoints.isEmpty())
                {
                    path.lineTo(end);
                }
                else
                {
                    for (const QPointF &wp : m_waypoints)
                    {
                        path.lineTo(wp);
                    }
                    path.lineTo(end);
                }

                // 绘制管道
                p->setPen(borderPen);
                p->drawPath(path);
                p->setPen(innerPen);
                p->drawPath(path);

                // 流动提示
                if (m_flowing)
                {
                    QPen flowPen(QColor("#0af"), 4.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
                    flowPen.setDashPattern({4, 4});
                    p->setPen(flowPen);
                    p->drawPath(path);
                }
            }

        private:
            QGraphicsItem *m_startItem;
            QPointF m_startOffset;
            QGraphicsItem *m_endItem;
            QPointF m_endOffset;
            QList<QPointF> m_waypoints;
            bool m_flowing;

            QPointF getStartPos() const
            {
                if (m_startItem)
                    return m_startItem->pos() + m_startOffset * m_startItem->scale();
                return m_startOffset;
            }

            QPointF getEndPos() const
            {
                if (m_endItem)
                    return m_endItem->pos() + m_endOffset * m_endItem->scale();
                return m_endOffset;
            }
        };

        class SafetyIndicatorItem : public QGraphicsItem
        {
        public:
            explicit SafetyIndicatorItem(const QString &name)
                : m_name(name), m_active(false)
            {
                setCacheMode(DeviceCoordinateCache);
            }

            QRectF boundingRect() const override { return QRectF(-30, -22, 60, 44); }

            void setActive(bool a)
            {
                if (m_active == a)
                    return;
                m_active = a;
                update();
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);
                // 参考 docs/UI-design：未触发灰色，触发红色；深灰本体 + #666 描边
                const QColor status = m_active ? kUiRed : kUiBorder;
                const QColor border = kUiBorder;
                const QColor fill = kUiBody;

                // 简化版安全阀符号（不做动画，但用同一套 token）
                p->setPen(QPen(border, 2));
                p->setBrush(fill);
                // 阀体（三角）
                QPolygonF tri;
                tri << QPointF(-14, 2) << QPointF(0, 20) << QPointF(14, 2);
                p->drawPolygon(tri);

                // 弹簧盒
                p->setBrush(kUiBody);
                p->drawRect(QRectF(-5, -20, 10, 10));
                // 弹簧线（zigzag）
                p->setBrush(Qt::NoBrush);
                p->setPen(QPen(kUiTextMuted, 1.5));
                QPainterPath spring;
                spring.moveTo(0, -10);
                spring.lineTo(-3, -8);
                spring.lineTo(3, -6);
                spring.lineTo(-3, -4);
                spring.lineTo(3, -2);
                spring.lineTo(-3, 0);
                spring.lineTo(0, 2);
                p->drawPath(spring);
                p->setPen(QPen(border, 2));

                // 排放口
                p->setBrush(kUiPanel);
                QPolygonF vent;
                vent << QPointF(14, 8) << QPointF(22, 4) << QPointF(22, 12);
                p->drawPolygon(vent);

                // 入口
                p->setBrush(fill);
                p->drawRect(QRectF(-3, 20, 6, 12));

                // 状态灯
                p->setPen(QPen(QColor("#000"), 1));
                p->setBrush(status);
                p->drawEllipse(QPointF(-22, 10), 3, 3);

                // 名称
                p->setPen(kUiText);
                QFont f = p->font();
                f.setPointSize(8);
                f.setBold(true);
                p->setFont(f);
                p->drawText(QRectF(-18, -34, 36, 14), Qt::AlignCenter, m_name);
            }

        private:
            QString m_name;
            bool m_active;
        };

        static void addBlueGridBackground(QGraphicsScene *scene, const QRectF &rect)
        {
            if (!scene)
                return;

            // 对齐 docs/UI-design 的暗色画布（#1a1a1a / #2a2a2a / #444 / #666）
            QLinearGradient bg(rect.topLeft(), rect.bottomLeft());
            bg.setColorAt(0.0, QColor("#1a1a1a"));
            bg.setColorAt(1.0, QColor("#121212"));
            scene->setBackgroundBrush(bg);

            // 轻量网格线（更克制，避免喧宾夺主）
            const QPen minor(QColor(255, 255, 255, 10), 1);
            const QPen major(QColor(255, 255, 255, 18), 1.5);
            const int step = 25;
            for (int x = 0; x <= rect.width(); x += step)
            {
                const bool isMajor = (x % (step * 4) == 0);
                scene->addLine(rect.left() + x, rect.top(), rect.left() + x, rect.bottom(), isMajor ? major : minor)->setZValue(-10);
            }
            for (int y = 0; y <= rect.height(); y += step)
            {
                const bool isMajor = (y % (step * 4) == 0);
                scene->addLine(rect.left(), rect.top() + y, rect.right(), rect.top() + y, isMajor ? major : minor)->setZValue(-10);
            }
        }
    }

    PreparationPanel::PreparationPanel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent)
        : QWidget(parent),
          m_deviceManager(deviceManager),
          m_updateTimer(nullptr),
          m_clockLabel(nullptr),
          m_view(nullptr),
          m_scene(nullptr),
          m_itemPump1(nullptr),
          m_itemPump2(nullptr),
          m_itemValve1(nullptr),
          m_itemValve2(nullptr),
          m_itemPS1(nullptr),
          m_itemPS2(nullptr),
          m_itemPS3(nullptr),
          m_itemTank(nullptr),
          m_statusBadge(nullptr),
          m_fillingTimeLabel(nullptr),
          m_tankLevelBar(nullptr),
          m_tankLevelText(nullptr),
          m_pumpFrequencySpinBox(nullptr),
          m_targetPressureSpinBox(nullptr),
          m_selfCheckBtn(nullptr),
          m_startFillingBtn(nullptr),
          m_stopFillingBtn(nullptr),
          m_reliefValveBtn(nullptr),
          m_reliefValveCloseBtn(nullptr),
          m_emergencyStopBtn(nullptr),
          m_relay1Check(nullptr),
          m_relay2Check(nullptr),
          m_relay3Check(nullptr),
          m_isFilling(false),
          m_fillingTimeSeconds(0),
          m_pumpFrequency(40.0f),
          m_targetPressure(0.5f),
          m_currentPressure(0.0f)
    {
        setupUI();

        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &PreparationPanel::onUpdateData);
    }

    PreparationPanel::~PreparationPanel()
    {
        stopUpdate();
    }

    void PreparationPanel::resizeEvent(QResizeEvent *event)
    {
        QWidget::resizeEvent(event);
        if (!m_view || !m_scene)
            return;

        // 让拟物流程图自适应可视区域（10寸/不同分辨率更直观）
        const QRectF r = m_scene->itemsBoundingRect().adjusted(-20, -20, 20, 20);
        if (!r.isEmpty())
            m_view->fitInView(r, Qt::KeepAspectRatio);
    }

    void PreparationPanel::setupUI()
    {
        auto *rootLayout = new QVBoxLayout(this);
        // 红框区域（顶部 header：标题/时间/系统自检）先移除，后续再整体调整控制台布局
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(0);

        // ========== 主体：单一大屏（左侧蓝底流程图 + 场景内浮动控制台/属性标签） ==========
        m_view = new QGraphicsView(this);
        m_view->setFrameShape(QFrame::NoFrame);
        m_view->setRenderHint(QPainter::Antialiasing, true);
        m_view->setRenderHint(QPainter::TextAntialiasing, true);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scene = new QGraphicsScene(this);
        m_view->setScene(m_scene);
        buildHmiScene();
        rootLayout->addWidget(m_view, 1);
        updateReliefValveStatus();
    }

    void PreparationPanel::buildHmiScene()
    {
        if (!m_scene)
            return;

        m_scene->clear();
        // 场景范围稍微放大，给中间流程图更多排布空间（view 会 fitInView 到实际图元）
        m_scene->setSceneRect(0, 0, 1900, 880);
        const QRectF rect = m_scene->sceneRect();
        addBlueGridBackground(m_scene, rect);

        // 清空图元指针
        m_itemPump1 = nullptr;
        m_itemPump2 = nullptr;
        m_itemValve1 = nullptr;
        m_itemValve2 = nullptr;
        m_itemPS1 = nullptr;
        m_itemPS2 = nullptr;
        m_itemPS3 = nullptr;
        m_itemTank = nullptr;
        m_itemOutdoorPool = nullptr;

        // 清空动态管道
        m_pipes.clear();

        // 关键节点位置（严格对齐 docs/测试准备区流程图.mmd 的拓扑）
        // A1[变频泵1] -->B1[电动阀1]
        // B1 --> C1[传感器1] --> Z1{安全阀1}
        // B1 --> D1[传感器3] --> E(分水罐)
        // A2[变频泵2] -->B2[电动阀2]
        // B2 --> C2[传感器2] --> Z2{安全阀2}
        // B2 --> E(分水罐)

        const HmiLayoutConfig layout;
        const QPointF p1 = layout.pump1();
        const QPointF v1 = layout.valve1();
        const QPointF ps1 = layout.sensor1();

        const QPointF p2 = layout.pump2();
        const QPointF v2 = layout.valve2();
        const QPointF ps2 = layout.sensor2();

        const QPointF ps3 = layout.sensor3();
        const QPointF tank = layout.tank();
        const QPointF outdoorPool = layout.outdoorPool();

        // 设备图元
        auto *pump1 = new PumpItem("P1 变频泵1");
        pump1->setPos(p1);
        pump1->setZValue(2);
        pump1->setScale(kPumpScale);
        m_scene->addItem(pump1);
        m_itemPump1 = pump1;

        auto *pump2 = new PumpItem("P2 变频泵2");
        pump2->setPos(p2);
        pump2->setZValue(2);
        pump2->setScale(kPumpScale);
        m_scene->addItem(pump2);
        m_itemPump2 = pump2;

        auto *valve1 = new ValveItem("V1 电动阀1");
        valve1->setPos(v1);
        valve1->setZValue(2);
        valve1->setScale(kValveScale);
        m_scene->addItem(valve1);
        m_itemValve1 = valve1;

        auto *valve2 = new ValveItem("V2 电动阀2");
        valve2->setPos(v2);
        valve2->setZValue(2);
        valve2->setScale(kValveScale);
        m_scene->addItem(valve2);
        m_itemValve2 = valve2;

        auto *sensor1 = new SensorItem("PS1");
        sensor1->setPos(ps1);
        sensor1->setZValue(2);
        sensor1->setScale(kSensorScale);
        m_scene->addItem(sensor1);
        m_itemPS1 = sensor1;

        auto *sensor2 = new SensorItem("PS2");
        sensor2->setPos(ps2);
        sensor2->setZValue(2);
        sensor2->setScale(kSensorScale);
        m_scene->addItem(sensor2);
        m_itemPS2 = sensor2;

        auto *sensor3 = new SensorItem("PS3");
        sensor3->setPos(ps3);
        sensor3->setZValue(2);
        sensor3->setScale(kSensorScale);
        m_scene->addItem(sensor3);
        m_itemPS3 = sensor3;

        auto *tankItem = new TankItem("分水罐");
        tankItem->setPos(tank);
        tankItem->setZValue(2);
        tankItem->setScale(kTankScale);
        m_scene->addItem(tankItem);
        m_itemTank = tankItem;

        auto *outdoorPoolItem = new OutdoorPoolItem("室外水池");
        outdoorPoolItem->setPos(outdoorPool);
        outdoorPoolItem->setZValue(2);
        outdoorPoolItem->setScale(kTankScale);
        m_scene->addItem(outdoorPoolItem);
        m_itemOutdoorPool = outdoorPoolItem;
        // ========== 创建动态管道（使用相对偏移量） ==========
        // 室外水池 -> P1
        auto *pipePool1 = new DynamicPipe(
            m_itemOutdoorPool, QPointF(OutdoorPoolItem::width() / 2, -55),
            m_itemPump1, QPointF(-PumpItem::width() / 2 - 60 / kPumpScale, 0),
            {});
        m_scene->addItem(pipePool1);
        m_pipes.append(pipePool1);

        // 室外水池 -> P2
        auto *pipePool2 = new DynamicPipe(
            m_itemOutdoorPool, QPointF(OutdoorPoolItem::width() / 2, 50),
            m_itemPump2, QPointF(-PumpItem::width() / 2 - 60 / kPumpScale, 0),
            {});
        m_scene->addItem(pipePool2);
        m_pipes.append(pipePool2);

        // P1 -> V1
        auto *pipe1V1 = new DynamicPipe(
            m_itemPump1, QPointF(PumpItem::width() / 2 + 100 / kPumpScale, 0),
            m_itemValve1, QPointF(-ValveItem::width() / 2 - 80 / kValveScale, 0),
            {});
        m_scene->addItem(pipe1V1);
        m_pipes.append(pipe1V1);

        // P2 -> V2
        auto *pipe2V2 = new DynamicPipe(
            m_itemPump2, QPointF(PumpItem::width() / 2 + 100 / kPumpScale, 0),
            m_itemValve2, QPointF(-ValveItem::width() / 2 - 80 / kValveScale, 0),
            {});
        m_scene->addItem(pipe2V2);
        m_pipes.append(pipe2V2);

        // V1 -> Tank
        auto *pipeV1Tank = new DynamicPipe(
            m_itemValve1, QPointF(70 / kValveScale, 0),
            m_itemTank, QPointF(-TankItem::width() / 2 - 100 / kTankScale, 0),
            {});
        m_scene->addItem(pipeV1Tank);
        m_pipes.append(pipeV1Tank);

        // V2 -> Tank
        auto *pipeV2Tank = new DynamicPipe(
            m_itemValve2, QPointF(0, 0),
            m_itemTank, QPointF(-TankItem::width() / 2 - 100 / kTankScale, 0),
            {});
        m_scene->addItem(pipeV2Tank);
        m_pipes.append(pipeV2Tank);

        // 启动管道更新定时器
        QTimer *pipeTimer = new QTimer(this);
        connect(pipeTimer, &QTimer::timeout, this, &PreparationPanel::onUpdatePipes);
        pipeTimer->start(50); // 20 FPS更新管道

        // ===== 把“属性显示”挂到左侧节点上（标签随节点移动） =====

        // 安全阀指示（贴近两路支线）
        // auto *sv1 = new SafetyIndicatorItem("SV1");
        // sv1->setPos(QPointF(ps1.x() + 10, ps1.y() + 100));
        // sv1->setZValue(2);
        // m_scene->addItem(sv1);
        // m_itemSafety1 = sv1;

        // auto *sv2 = new SafetyIndicatorItem("SV2");
        // sv2->setPos(QPointF(ps2.x() + 10, ps2.y() - 100));
        // sv2->setZValue(2);
        // m_scene->addItem(sv2);
        // m_itemSafety2 = sv2;

        // 标题（左上）
        auto *caption = m_scene->addText("测试准备区流程（拟物显示）");
        caption->setDefaultTextColor(QColor("#e6edf3"));
        QFont tf = caption->font();
        tf.setPointSize(12);
        tf.setBold(true);
        caption->setFont(tf);
        caption->setPos(18, 12);
        caption->setZValue(5);

        // 初次构建后做一次自适配
        if (m_view)
        {
            const QRectF r = m_scene->itemsBoundingRect().adjusted(-20, -20, 20, 20);
            if (!r.isEmpty())
                m_view->fitInView(r, Qt::KeepAspectRatio);
        }
    }

    void PreparationPanel::updateClock()
    {
        if (!m_clockLabel)
            return;
        m_clockLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
    }

    void PreparationPanel::onUpdatePipes()
    {
        // 强制所有管道重绘以跟随图元移动
        for (auto *pipe : m_pipes)
        {
            if (pipe)
                pipe->update();
        }
    }

    void PreparationPanel::onReliefValveToggled(bool open)
    {
        if (!m_deviceManager)
        {
            if (m_reliefValveBtn)
            {
                m_reliefValveBtn->blockSignals(true);
                m_reliefValveBtn->setChecked(false);
                m_reliefValveBtn->blockSignals(false);
            }
            QMessageBox::warning(this, "泄压阀", "设备管理器未初始化");
            return;
        }

        if (open)
        {
            auto reply = QMessageBox::warning(this, "泄压阀",
                                              "确认打开泄压阀(阀11)？\n\n打开后将释放压力。",
                                              QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No)
            {
                m_reliefValveBtn->blockSignals(true);
                m_reliefValveBtn->setChecked(false);
                m_reliefValveBtn->blockSignals(false);
                return;
            }
        }

        if (!m_deviceManager->controlValve(11, open))
        {
            QMessageBox::warning(this, "泄压阀", open ? "打开泄压阀失败" : "关闭泄压阀失败");
            m_reliefValveBtn->blockSignals(true);
            m_reliefValveBtn->setChecked(!open);
            m_reliefValveBtn->blockSignals(false);
            return;
        }

        updateReliefValveStatus();
    }

    void PreparationPanel::updateReliefValveStatus()
    {
        if (!m_deviceManager)
            return;

        const auto v = m_deviceManager->getValve(11);
        const bool isOpen = (v.status == ValveStatus::OPEN || v.status == ValveStatus::OPENING);

        if (m_reliefValveBtn)
        {
            m_reliefValveBtn->setEnabled(!isOpen);
            m_reliefValveBtn->setText(isOpen ? "泄压阀已开启" : "开启泄压阀(阀11)");
        }

        if (m_reliefValveCloseBtn)
        {
            m_reliefValveCloseBtn->setEnabled(isOpen);
            m_reliefValveCloseBtn->setText(isOpen ? "关闭泄压阀(阀11)" : "泄压阀已关闭");
        }
    }

    void PreparationPanel::onSelfCheck()
    {
        if (!m_deviceManager)
        {
            QMessageBox::warning(this, "系统自检", "设备管理器未初始化（请先连接系统）");
            return;
        }

        const bool hasPlc = m_deviceManager->hasPlcClient();
        const bool plcConnected = m_deviceManager->isPlcConnected();
        const bool collecting = m_deviceManager->isDataCollecting();

        auto modeToText = [](SystemMode mode)
        {
            switch (mode)
            {
            case SystemMode::MANUAL:
                return QString("手动");
            case SystemMode::AUTO:
                return QString("自动");
            case SystemMode::TEST:
                return QString("测试");
            case SystemMode::EMERGENCY:
                return QString("紧急");
            default:
                return QString("未知");
            }
        };

        auto statusToText = [](DeviceStatus s)
        {
            switch (s)
            {
            case DeviceStatus::ONLINE:
                return QString("在线");
            case DeviceStatus::OFFLINE:
                return QString("离线");
            case DeviceStatus::FAULT:
                return QString("故障");
            case DeviceStatus::MAINTENANCE:
                return QString("维护");
            default:
                return QString("未知");
            }
        };

        auto okCell = [](bool ok)
        {
            const QString color = ok ? "#3fb950" : "#f85149";
            const QString text = ok ? "OK" : "NG";
            return QString("<span style='color:%1;font-weight:800'>%2</span>").arg(color, text);
        };

        QString rows;
        auto addRow = [&](const QString &item, bool ok, const QString &detail)
        {
            rows += QString("<tr><td style='padding:6px 10px'>%1</td><td style='padding:6px 10px'>%2</td><td style='padding:6px 10px'>%3</td></tr>")
                        .arg(item.toHtmlEscaped(), okCell(ok), detail.toHtmlEscaped());
        };

        addRow("设备管理器", true, "已创建");
        addRow("PLC客户端", hasPlc, hasPlc ? "已初始化" : "未初始化（请先点击主界面连接）");
        addRow("PLC连接", plcConnected, plcConnected ? "已连接" : "未连接");
        addRow("数据采集线程", collecting, collecting ? "运行中" : "未运行");

        // SV1/SV2 暂时移除

        bool refreshOk = false;
        if (plcConnected)
        {
            refreshOk = m_deviceManager->updateAllDevices();
            addRow("一次数据刷新", refreshOk, refreshOk ? "读取成功" : "读取失败（检查PLC地址/网络）");
        }
        else
        {
            addRow("一次数据刷新", false, "PLC未连接，跳过");
        }

        const auto sys = m_deviceManager->getSystemStatus();
        addRow("系统模式", true, modeToText(sys.mode));
        addRow("系统运行", sys.isRunning, sys.isRunning ? "运行中" : "未运行");

        const auto p1 = m_deviceManager->getPressureSensor(1);
        addRow("压力传感器1", p1.id != 0, QString("%1, %2 MPa").arg(statusToText(p1.status)).arg(p1.pressure / 1e6, 0, 'f', 3));

        const QString html =
            "<h3>系统自检结果</h3>"
            "<table style='border-collapse:collapse;border:1px solid #223244' border='1'>"
            "<tr style='background:#0f1a24'><th style='padding:6px 10px'>项目</th><th style='padding:6px 10px'>结果</th><th style='padding:6px 10px'>说明</th></tr>" +
            rows +
            "</table>"
            "<p style='margin-top:10px;color:#9da7b3'>提示：如 PLC 未连接，请先在主界面点击“连接”。</p>";

        QMessageBox msg(this);
        msg.setWindowTitle("系统自检");
        msg.setTextFormat(Qt::RichText);
        msg.setText(html);
        msg.exec();
    }

    void PreparationPanel::startUpdate(int intervalMs)
    {
        if (m_updateTimer && !m_updateTimer->isActive())
        {
            m_updateTimer->start(intervalMs);
        }
    }

    void PreparationPanel::stopUpdate()
    {
        if (m_updateTimer && m_updateTimer->isActive())
        {
            m_updateTimer->stop();
        }
    }

    void PreparationPanel::onUpdateData()
    {
        if (!m_deviceManager)
        {
            return;
        }

        updatePumpStatus();
        updateValveStatus();
        updateWaterLevel();
        updateRelayStates();
        updateReliefValveStatus();

        // 如果正在加水，增加计时
        if (m_isFilling)
        {
            m_fillingTimeSeconds++;
            m_fillingTimeLabel->setText(QString("加水时间: %1秒").arg(m_fillingTimeSeconds));

            // 检查是否达到目标压力
            if (m_currentPressure >= m_targetPressure)
            {
                onStopFilling();
                QMessageBox::information(this, "完成", "分水罐已加满！压力已达到目标值。");
            }
        }
    }

    void PreparationPanel::updatePumpStatus()
    {
        auto p1 = m_deviceManager->getPump(1);
        auto p2 = m_deviceManager->getPump(2);

        if (auto *item = dynamic_cast<PumpItem *>(m_itemPump1))
        {
            item->setRunning(p1.isRunning);
            item->setFrequency(p1.frequency);
        }
        if (auto *item = dynamic_cast<PumpItem *>(m_itemPump2))
        {
            item->setRunning(p2.isRunning);
            item->setFrequency(p2.frequency);
        }
    }

    void PreparationPanel::updateValveStatus()
    {
        auto valve1 = m_deviceManager->getValve(1); // 进水阀
        auto valve2 = m_deviceManager->getValve(2); // 出水阀

        const bool v1Open = (valve1.status == ValveStatus::OPEN || valve1.status == ValveStatus::OPENING);
        const bool v2Open = (valve2.status == ValveStatus::OPEN || valve2.status == ValveStatus::OPENING);

        if (auto *item = dynamic_cast<ValveItem *>(m_itemValve1))
        {
            item->setOpen(v1Open);
            item->setDegree(valve1.openingDegree);
        }
        if (auto *item = dynamic_cast<ValveItem *>(m_itemValve2))
        {
            item->setOpen(v2Open);
            item->setDegree(valve2.openingDegree);
        }
    }

    void PreparationPanel::updateWaterLevel()
    {
        auto s1 = m_deviceManager->getPressureSensor(1);
        auto s2 = m_deviceManager->getPressureSensor(2);
        auto s3 = m_deviceManager->getPressureSensor(3);

        if (auto *item = dynamic_cast<SensorItem *>(m_itemPS1))
            item->setPressureMPa(s1.pressure / 1e6);
        if (auto *item = dynamic_cast<SensorItem *>(m_itemPS2))
            item->setPressureMPa(s2.pressure / 1e6);
        if (auto *item = dynamic_cast<SensorItem *>(m_itemPS3))
            item->setPressureMPa(s3.pressure / 1e6);

        // 以传感器3作为分水罐压力（对应 docs/测试准备区流程图.mmd）
        m_currentPressure = static_cast<float>(s3.pressure / 1e6);
        // 根据压力计算水位百分比（假设目标压力对应满罐）
        int waterLevel = static_cast<int>((m_targetPressure <= 0.0001f) ? 0.0f : (m_currentPressure / m_targetPressure) * 100.0f);
        if (waterLevel > 100)
            waterLevel = 100;
        if (waterLevel < 0)
            waterLevel = 0;

        if (m_tankLevelBar)
            m_tankLevelBar->setValue(waterLevel);

        if (auto *tankItem = dynamic_cast<TankItem *>(m_itemTank))
        {
            tankItem->setFillPercent(waterLevel);
            tankItem->setFilling(m_isFilling);
            tankItem->setPressureMPa(m_currentPressure);
        }

        // 更新水位文字描述
        QString levelText;
        QString levelColor;
        if (waterLevel < 20)
        {
            levelText = "空罐";
            levelColor = "#ff6666";
        }
        else if (waterLevel < 50)
        {
            levelText = "低水位";
            levelColor = "#ffa726";
        }
        else if (waterLevel < 80)
        {
            levelText = "中水位";
            levelColor = "#ffd54f";
        }
        else if (waterLevel < 95)
        {
            levelText = "高水位";
            levelColor = "#66bb6a";
        }
        else
        {
            levelText = "已满";
            levelColor = "#42a5f5";
        }

        if (m_tankLevelText)
        {
            m_tankLevelText->setText(QString("%1  (%2)").arg(levelText).arg(fmtMPa(s3.pressure)));
            const char *tone = "bad";
            if (waterLevel >= 95)
                tone = "good";
            else if (waterLevel >= 50)
                tone = "warn";
            else if (waterLevel >= 20)
                tone = "warn";
            setBadgeTone(m_tankLevelText, tone);
        }
    }

    void PreparationPanel::updateRelayStates()
    {
        if (!m_deviceManager)
            return;
        if (!m_relay1Check || !m_relay2Check || !m_relay3Check)
            return;

        bool on;
        if (m_deviceManager->getRelayState(0, on))
        {
            m_relay1Check->blockSignals(true);
            m_relay1Check->setChecked(on);
            m_relay1Check->blockSignals(false);
        }
        if (m_deviceManager->getRelayState(1, on))
        {
            m_relay2Check->blockSignals(true);
            m_relay2Check->setChecked(on);
            m_relay2Check->blockSignals(false);
        }
        if (m_deviceManager->getRelayState(2, on))
        {
            m_relay3Check->blockSignals(true);
            m_relay3Check->setChecked(on);
            m_relay3Check->blockSignals(false);
        }
    }

    void PreparationPanel::onStartFilling()
    {
        if (!m_deviceManager)
        {
            QMessageBox::warning(this, "错误", "设备管理器未初始化");
            return;
        }

        // 检查当前水位（使用 updateWaterLevel 中的 m_currentPressure）
        if (m_currentPressure >= m_targetPressure)
        {
            QMessageBox::information(this, "提示", "分水罐已满，无需加水");
            return;
        }

        // 确认操作
        auto reply = QMessageBox::question(this, "确认",
                                           QString("确定开始加水吗？\n\n流程：\n1. 启动变频泵1（频率: %1 Hz）\n2. 打开电动阀1(进水阀)\n3. 监测压力直至达到 %2 MPa")
                                               .arg(m_pumpFrequency, 0, 'f', 1)
                                               .arg(m_targetPressure, 0, 'f', 2),
                                           QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            return;
        }

        m_isFilling = true;
        m_fillingTimeSeconds = 0;

        // 启动变频泵（使用用户设置的频率）
        if (!m_deviceManager->controlPump(1, true))
        {
            QMessageBox::warning(this, "错误", "启动变频泵失败");
            m_isFilling = false;
            return;
        }

        if (!m_deviceManager->setPumpFrequency(1, m_pumpFrequency))
        {
            QMessageBox::warning(this, "警告", "设置泵频率失败");
        }

        // 打开进水阀
        if (!m_deviceManager->controlValve(1, true))
        {
            QMessageBox::warning(this, "错误", "打开进水阀失败");
            m_deviceManager->controlPump(1, false);
            m_isFilling = false;
            return;
        }

        // 更新UI
        m_startFillingBtn->setEnabled(false);
        m_stopFillingBtn->setEnabled(true);
        if (m_pumpFrequencySpinBox)
            m_pumpFrequencySpinBox->setEnabled(false);
        if (m_targetPressureSpinBox)
            m_targetPressureSpinBox->setEnabled(false);
        if (m_statusBadge)
        {
            m_statusBadge->setText("加水中");
            setBadgeTone(m_statusBadge, "good");
        }
    }

    void PreparationPanel::onStopFilling()
    {
        if (!m_deviceManager)
        {
            return;
        }

        m_isFilling = false;

        // 关闭进水阀
        m_deviceManager->controlValve(1, false);

        // 停止变频泵
        m_deviceManager->controlPump(1, false);

        // 更新UI
        m_startFillingBtn->setEnabled(true);
        m_stopFillingBtn->setEnabled(false);
        if (m_pumpFrequencySpinBox)
            m_pumpFrequencySpinBox->setEnabled(true);
        if (m_targetPressureSpinBox)
            m_targetPressureSpinBox->setEnabled(true);
        if (m_statusBadge)
        {
            m_statusBadge->setText("已停止");
            setBadgeTone(m_statusBadge, "info");
        }
    }

    void PreparationPanel::onEmergencyStop()
    {
        if (!m_deviceManager)
        {
            return;
        }

        m_isFilling = false;

        // 紧急停止
        m_deviceManager->emergencyStop();

        // 更新UI
        m_startFillingBtn->setEnabled(true);
        m_stopFillingBtn->setEnabled(false);
        if (m_pumpFrequencySpinBox)
            m_pumpFrequencySpinBox->setEnabled(true);
        if (m_targetPressureSpinBox)
            m_targetPressureSpinBox->setEnabled(true);
        if (m_statusBadge)
        {
            m_statusBadge->setText("紧急停止");
            setBadgeTone(m_statusBadge, "bad");
        }

        QMessageBox::information(this, "完成", "紧急停止已执行");
    }

    QString PreparationPanel::getStatusColor(bool isGood) const
    {
        return isGood ? "green" : "red";
    }

    QString PreparationPanel::getValveColor(bool isOpen) const
    {
        return isOpen ? "green" : "red";
    }

    void PreparationPanel::onPumpFrequencyChanged(double value)
    {
        m_pumpFrequency = static_cast<float>(value);
    }

    void PreparationPanel::onTargetPressureChanged(double value)
    {
        m_targetPressure = static_cast<float>(value);
    }

    void PreparationPanel::onRelay1Toggled(bool on)
    {
        if (!m_deviceManager)
            return;
        if (!m_relay1Check)
            return;
        if (!m_deviceManager->setRelay(0, on))
        {
            QMessageBox::warning(this, "错误", "操作Q0.0失败");
            m_relay1Check->blockSignals(true);
            m_relay1Check->setChecked(!on);
            m_relay1Check->blockSignals(false);
        }
    }

    void PreparationPanel::onRelay2Toggled(bool on)
    {
        if (!m_deviceManager)
            return;
        if (!m_relay2Check)
            return;
        if (!m_deviceManager->setRelay(1, on))
        {
            QMessageBox::warning(this, "错误", "操作Q0.1失败");
            m_relay2Check->blockSignals(true);
            m_relay2Check->setChecked(!on);
            m_relay2Check->blockSignals(false);
        }
    }

    void PreparationPanel::onRelay3Toggled(bool on)
    {
        if (!m_deviceManager)
            return;
        if (!m_relay3Check)
            return;
        if (!m_deviceManager->setRelay(2, on))
        {
            QMessageBox::warning(this, "错误", "操作Q0.2失败");
            m_relay3Check->blockSignals(true);
            m_relay3Check->setChecked(!on);
            m_relay3Check->blockSignals(false);
        }
    }

} // namespace WaterTest
