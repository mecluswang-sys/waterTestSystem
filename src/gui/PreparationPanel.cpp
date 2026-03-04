/**
 * @file PreparationPanel.cpp
 * @brief 测试准备区面板实现（HMI流程图式）
 */

#include "gui/PreparationPanel.h"
#include "DeviceManager.h"
#include "ConfigManager.h"

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
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

namespace WaterTest
{

    namespace
    {
        static void ensureHmiConfigLoadedOnce()
        {
            static bool tried = false;
            if (tried)
                return;
            tried = true;

            const bool ok = ConfigManager::getInstance().loadConfig("config/system.conf");
            qInfo().noquote() << QString("[HMI配置] load config/system.conf %1 (cwd=%2)")
                                     .arg(ok ? "OK" : "FAIL")
                                     .arg(QDir::currentPath());
        }

        static bool hmiDragEnabled()
        {
            // 由配置文件控制：config/system.conf
            // ui.hmi.drag_enabled = true/false
            ensureHmiConfigLoadedOnce();
            return ConfigManager::getInstance().getBool("ui.hmi.drag_enabled", false);
        }

        static bool hmiShowPressureSensors()
        {
            // 由配置文件控制：config/system.conf
            // ui.hmi.show_pressure_sensors = true/false
            ensureHmiConfigLoadedOnce();
            return ConfigManager::getInstance().getBool("ui.hmi.show_pressure_sensors", true);
        }

        static QString hmiThemePreset()
        {
            // 由配置文件控制：config/system.conf
            // ui.hmi.theme = graphite/light/ocean
            // - 若 ui.hmi.theme 为空或为 auto，则跟随 ui.theme
            ensureHmiConfigLoadedOnce();
            auto &cfg = ConfigManager::getInstance();
            const QString hmi = QString::fromStdString(cfg.getString("ui.hmi.theme", "")).trimmed().toLower();
            if (hmi.isEmpty() || hmi == "auto")
            {
                const QString ui = QString::fromStdString(cfg.getString("ui.theme", "graphite")).trimmed().toLower();
                return ui;
            }
            return hmi;
        }

        static QString hmiPositionLogPath()
        {
            // 优先写到“文档”目录：更容易让现场直接找到
            QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
            if (!baseDir.isEmpty())
                baseDir = QDir(baseDir).filePath("WaterTestSystem");
            else
                baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

            if (baseDir.isEmpty())
                baseDir = QDir::currentPath();

            QDir().mkpath(baseDir);
            return QDir(baseDir).filePath("hmi_positions.log");
        }

        static void appendHmiPositionLine(const QString &line)
        {
            const QString path = hmiPositionLogPath();
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                qWarning().noquote() << QString("[HMI坐标] 无法写入文件: %1").arg(path);
                return;
            }
            QTextStream out(&file);
            out << line << '\n';
        }

        static QString formatHmiPositionLine(const QString &tag, const QString &name, const QPointF &pos)
        {
            return QString("%1\t%2\t%3\t%4\t%5")
                .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
                .arg(tag)
                .arg(name)
                .arg(pos.x(), 0, 'f', 0)
                .arg(pos.y(), 0, 'f', 0);
        }

        // ===== UI-design 视觉 token（集中改色：只动这里即可整体生效） =====
        // 说明：支持三套主题，通过 ui.hmi.theme（或 ui.theme）选择。
        // - graphite：保留当前“石墨灰”方案（相当于“把这套保存下”）
        // - light：灰白底方案（便于白天/亮环境查看）
        // - ocean：海蓝青（低眩光深色 + 青色强调）
        static QColor kUiBg;
        static QColor kUiBg2;
        static QColor kUiPanel;
        static QColor kUiBody;
        static QColor kUiBorder;
        static QColor kUiBorderWeak;
        static QColor kUiText;
        static QColor kUiTextMuted;
        static QColor kUiTextDim;
        static QColor kUiGridMinor;
        static QColor kUiGridMajor;

        static QColor kUiCyan;
        static QColor kUiGreen;
        static QColor kUiRed;
        static QColor kUiOrange;
        static QColor kUiPurple;

        // 组件/管道常用辅助色（避免散落硬编码）
        static QColor kUiShadow;
        static QColor kUiInk;
        static QColor kUiMetalDark;
        static QColor kUiMetalMid;
        static QColor kUiWater;
        static QColor kUiPipeOuter;
        static QColor kUiPipeInner;

        struct UiThemeTokens
        {
            QColor bg;
            QColor panel;
            QColor body;
            QColor border;
            QColor borderWeak;
            QColor text;
            QColor textMuted;
            QColor textDim;

            QColor cyan;
            QColor green;
            QColor red;
            QColor orange;
            QColor purple;

            QColor shadow;
            QColor ink;
            QColor metalDark;
            QColor metalMid;
            QColor water;
        };

        static UiThemeTokens makeGraphiteTheme()
        {
            UiThemeTokens t;
            t.bg = QColor("#3a3d40");
            t.panel = QColor("#3f4245");
            t.body = QColor("#474b4f");
            t.border = QColor("#666a6e");
            t.borderWeak = QColor("#45494c");
            t.text = QColor("#d6d9dc");
            t.textMuted = QColor("#a9adb1");
            t.textDim = QColor("#80858a");

            t.cyan = QColor("#4aa7a8");
            t.green = QColor("#59a86a");
            t.red = QColor("#d4605a");
            t.orange = QColor("#d3a34a");
            t.purple = QColor("#8a7ec7");

            t.shadow = QColor(0, 0, 0, 90);
            t.ink = QColor(0, 0, 0, 200);
            t.metalDark = QColor("#444");
            t.metalMid = QColor("#555");
            t.water = QColor("#0af");
            return t;
        }

        static UiThemeTokens makeLightTheme()
        {
            UiThemeTokens t;
            t.bg = QColor("#e6e8ea");
            t.panel = QColor("#f0f2f3");
            t.body = QColor("#2d333aa9");
            t.border = QColor("#90979f");
            t.borderWeak = QColor("#464c52");
            t.text = QColor("#2b2f33");
            t.textMuted = QColor("#4f565d");
            t.textDim = QColor("#6b737b");

            t.cyan = QColor("#1f8a8b");
            t.green = QColor("#2a8f56");
            t.red = QColor("#c24b45");
            t.orange = QColor("#c4842d");
            t.purple = QColor("#6a5fb2");

            t.shadow = QColor(0, 0, 0, 50);
            t.ink = QColor(0, 0, 0, 160);
            t.metalDark = QColor("#7a828a");
            t.metalMid = QColor("#8b9299");
            t.water = QColor("#1677c8");
            return t;
        }

        static UiThemeTokens makeOceanTheme()
        {
            UiThemeTokens t;
            t.bg = QColor("#0e2a33");
            t.panel = QColor("#12323d");
            t.body = QColor("#153a46");
            t.border = QColor("#2c5866");
            t.borderWeak = QColor("#1a3f4b");
            t.text = QColor("#d6f2f1");
            t.textMuted = QColor("#a6c9c8");
            t.textDim = QColor("#7ea5a6");

            t.cyan = QColor("#1aa6a8");
            t.green = QColor("#2fbf8f");
            t.red = QColor("#ff6b6b");
            t.orange = QColor("#f2c14e");
            t.purple = QColor("#7c6bd6");

            t.shadow = QColor(0, 0, 0, 85);
            t.ink = QColor(0, 0, 0, 190);
            t.metalDark = QColor("#244a55");
            t.metalMid = QColor("#2c5866");
            t.water = QColor("#2bbcff");
            return t;
        }

        static void applyThemeTokens(const UiThemeTokens &t)
        {
            kUiBg = t.bg;
            kUiBg2 = [&]()
            {
                QColor c = t.bg;
                return c.lighter(t.bg.lightness() > 200 ? 104 : 110);
            }();
            kUiPanel = t.panel;
            kUiBody = t.body;
            kUiBorder = t.border;
            kUiBorderWeak = t.borderWeak;
            kUiText = t.text;
            kUiTextMuted = t.textMuted;
            kUiTextDim = t.textDim;

            // 网格：根据明暗底自动选择“黑/白系”透明线（用 text 派生最稳）
            kUiGridMinor = [&]()
            {
                QColor c = t.text;
                c.setAlpha(t.bg.lightness() > 200 ? 18 : 8);
                return c;
            }();
            kUiGridMajor = [&]()
            {
                QColor c = t.text;
                c.setAlpha(t.bg.lightness() > 200 ? 28 : 14);
                return c;
            }();

            kUiCyan = t.cyan;
            kUiGreen = t.green;
            kUiRed = t.red;
            kUiOrange = t.orange;
            kUiPurple = t.purple;

            kUiShadow = t.shadow;
            kUiInk = t.ink;
            kUiMetalDark = t.metalDark;
            kUiMetalMid = t.metalMid;
            kUiWater = t.water;

            // 管道：外圈略偏边框色，内圈略贴近背景
            kUiPipeOuter = t.border;
            kUiPipeInner = [&]()
            {
                QColor c = t.bg;
                return c.darker(t.bg.lightness() > 200 ? 110 : 125);
            }();
        }

        static bool ensureUiTokensInitialized()
        {
            static QString applied;

            const QString preset = hmiThemePreset();
            QString normalized = "graphite";
            if (preset == "light" || preset == "graywhite" || preset == "greywhite")
                normalized = "light";
            else if (preset == "ocean" || preset == "aqua" || preset == "teal")
                normalized = "ocean";
            if (applied == normalized)
                return false;

            if (normalized == "light")
                applyThemeTokens(makeLightTheme());
            else if (normalized == "ocean")
                applyThemeTokens(makeOceanTheme());
            else
                applyThemeTokens(makeGraphiteTheme());

            qInfo().noquote() << QString("[HMI主题] apply preset=%1 (ui.hmi/ui.theme=%2)").arg(normalized, preset);
            applied = normalized;
            return true;
        }

        static QString fmtKPa(double pa)
        {
            return QString::number(pa / 1e3, 'f', 1) + " kPa";
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
            // 基于现场拖拽回填（2026-02-05）
            // 以这些点位作为“初始位置”，拖拽默认关闭（可在配置打开）。
            QPointF outdoorPoolPos{110, 340};
            QPointF pump1Pos{440, 150};
            QPointF pump2Pos{440, 540};
            QPointF valve1Pos{800, 132};
            QPointF valve2Pos{800, 522};
            QPointF tankPos{1200, 260};
            // 微调：让 Tank(outlet) 与 V3(inlet) 的端口 y 对齐，从而该段管道为“一条直线”
            // Tank outlet: tankPos.y + 1.5 * 50 = 260 + 75 = 335
            // V3 inlet:     valve3Pos.y + 1.5 * 12 = valve3Pos.y + 18 -> 335 => valve3Pos.y = 317
            QPointF valve3Pos{1500, 317};
            QPointF teePos{1770, 450};

            // 压力传感器（卡片显示）
            QPointF ps1Pos{630, 100};
            QPointF ps2Pos{630, 490};
            QPointF ps3Pos{1200, 80};

            QPointF outdoorPool() const { return outdoorPoolPos; }
            QPointF pump1() const { return pump1Pos; }
            QPointF pump2() const { return pump2Pos; }
            QPointF valve1() const { return valve1Pos; }
            QPointF valve2() const { return valve2Pos; }
            QPointF tank() const { return tankPos; }
            QPointF valve3() const { return valve3Pos; }
            QPointF tee() const { return teePos; }

            QPointF ps1() const { return ps1Pos; }
            QPointF ps2() const { return ps2Pos; }
            QPointF ps3() const { return ps3Pos; }
        };

        // ===== 管道走向（手工 waypoint，可选） =====
        // 说明：
        // - 每条管道默认不填 waypoint（QList 为空）时，走 DynamicPipe 的“自动正交路由”。
        // - 如果需要手工固定走向：在下面把对应列表填上“场景坐标点”。
        // - 点的含义：依次经过的拐点（scenePos），最终仍会自动用横竖线连接。
        // - 建议坐标用 10 像素网格（和拖拽吸附一致），例如 (520, 220)。
        struct HmiPipeWaypointsConfig
        {
            // 室外水池 -> P1
            QList<QPointF> poolToP1Scene;
            // 室外水池 -> P2
            QList<QPointF> poolToP2Scene;
            // P1 -> V1
            QList<QPointF> p1ToV1Scene;
            // P2 -> V2
            QList<QPointF> p2ToV2Scene;
            // V1 -> Tank
            QList<QPointF> v1ToTankScene;
            // Tank(outlet) -> V3(inlet)
            QList<QPointF> tankToV3Scene;
            // V3(outlet) -> Tee(inlet2)
            QList<QPointF> v3ToTeeScene;
            // V2(outlet) -> Tee(inlet1)
            QList<QPointF> v2ToTeeScene;
        };

        static QList<QPointF> toStartLocalWaypoints(QGraphicsItem *startItem, const QList<QPointF> &sceneWaypoints)
        {
            if (sceneWaypoints.isEmpty())
                return {};
            QList<QPointF> result;
            result.reserve(sceneWaypoints.size());
            for (const QPointF &wpScene : sceneWaypoints)
            {
                if (startItem)
                    result.append(startItem->mapFromScene(wpScene));
                else
                    result.append(wpScene);
            }
            return result;
        }

        // ========== 拟物图元（蓝底流程图：泵/阀/传感器/分水罐）==========
        class PumpItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return QPointF(-55, 0); }
            static QPointF outletPortLocal() { return QPointF(55, 0); }

            explicit PumpItem(const QString &name)
                : m_name(name), m_running(false), m_frequencyHz(0.0)
            {
                setCacheMode(DeviceCoordinateCache);
                QGraphicsItem::GraphicsItemFlags flags = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges;
                if (hmiDragEnabled())
                    flags |= QGraphicsItem::ItemIsMovable;
                setFlags(flags);
            }

            QRectF boundingRect() const override { return QRectF(-55, -45, 110, 140); }

            static constexpr qreal width() { return 110; }
            static constexpr qreal height() { return 140; }

            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (!hmiDragEnabled())
                    return QGraphicsItem::itemChange(change, value);
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

            void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
            {
                QGraphicsItem::mouseReleaseEvent(event);
                if (!hmiDragEnabled())
                    return;
                qInfo().noquote() << QString("[HMI坐标] %1 pos=(%2, %3)")
                                         .arg(m_name)
                                         .arg(pos().x(), 0, 'f', 0)
                                         .arg(pos().y(), 0, 'f', 0);
                appendHmiPositionLine(formatHmiPositionLine("MOVE", m_name, pos()));
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
                p->setBrush(kUiShadow);
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
                p->setBrush(kUiMetalMid);
                p->setPen(QPen(kUiBorder, 1));
                p->drawRect(QRectF(motor.right(), motor.center().y() - 3, 12, 6));

                // 泵壳（圆形）
                p->setBrush(kUiBody);
                p->setPen(QPen(borderColor, 2));
                p->drawEllipse(QPointF(22, 0), 18, 18);

                // 出口
                p->setBrush(kUiMetalDark);
                p->setPen(QPen(borderColor, 2));
                p->drawRect(QRectF(40, -4, 12, 8));

                // 状态灯
                p->setBrush(statusColor);
                p->setPen(QPen(kUiInk, 1));
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

                // 入口/出口触点（用于管道吸附）
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
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
            static QPointF inletPortLocal() { return QPointF(-40, 12); }
            static QPointF outletPortLocal() { return QPointF(45, 12); }

            explicit ValveItem(const QString &name)
                : m_name(name), m_open(false), m_degree(0)
            {
                setCacheMode(DeviceCoordinateCache);
                QGraphicsItem::GraphicsItemFlags flags = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges;
                if (hmiDragEnabled())
                    flags |= QGraphicsItem::ItemIsMovable;
                setFlags(flags);
            }

            QRectF boundingRect() const override { return QRectF(-50, -40, 104, 100); }

            static constexpr qreal width() { return 104; }
            static constexpr qreal height() { return 100; }
            qreal x() const { return pos().x(); }
            qreal y() const { return pos().y(); }

            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (!hmiDragEnabled())
                    return QGraphicsItem::itemChange(change, value);
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

            void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
            {
                QGraphicsItem::mouseReleaseEvent(event);
                if (!hmiDragEnabled())
                    return;
                qInfo().noquote() << QString("[HMI坐标] %1 pos=(%2, %3)")
                                         .arg(m_name)
                                         .arg(pos().x(), 0, 'f', 0)
                                         .arg(pos().y(), 0, 'f', 0);
                appendHmiPositionLine(formatHmiPositionLine("MOVE", m_name, pos()));
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
                p->setBrush(kUiShadow);
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
                p->setPen(QPen(kUiInk, 1));
                p->drawEllipse(QPointF(-11, -30), 3, 3);

                // 阀杆
                p->setBrush(kUiMetalMid);
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
                p->setPen(QPen(kUiInk, 1));
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

                // 入口/出口触点（用于管道吸附）
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
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
                QGraphicsItem::GraphicsItemFlags flags = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges;
                if (hmiDragEnabled())
                    flags |= QGraphicsItem::ItemIsMovable;
                setFlags(flags);
            }

            // 卡片 + 引线
            QRectF boundingRect() const override { return QRectF(-52, -46, 104, 104); }

            static constexpr qreal width() { return 104; }
            static constexpr qreal height() { return 104; }
            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (!hmiDragEnabled())
                    return QGraphicsItem::itemChange(change, value);
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

            void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
            {
                QGraphicsItem::mouseReleaseEvent(event);
                if (!hmiDragEnabled())
                    return;
                qInfo().noquote() << QString("[HMI坐标] %1 pos=(%2, %3)")
                                         .arg(m_name)
                                         .arg(pos().x(), 0, 'f', 0)
                                         .arg(pos().y(), 0, 'f', 0);
                appendHmiPositionLine(formatHmiPositionLine("MOVE", m_name, pos()));
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
                p->setPen(kUiTextDim);
                p->drawText(QRectF(card.left() + 6 + 52, card.top() + 24, card.width() - 58, 14), Qt::AlignLeft | Qt::AlignVCenter, "kPa");

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
                p->setPen(QPen(kUiInk, 1));
                p->drawEllipse(QPointF(0, card.bottom() + 16), 3, 3);
            }

        private:
            QString m_name;
            double m_pressureMPa;
        };

        class TankItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return QPointF(-55, -5); }
            static QPointF outletPortLocal() { return QPointF(55, 50); }

            explicit TankItem(const QString &name)
                : m_name(name), m_fillPercent(0.0), m_filling(false), m_pressureMPa(0.0)
            {
                setCacheMode(DeviceCoordinateCache);
                QGraphicsItem::GraphicsItemFlags flags = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges;
                if (hmiDragEnabled())
                    flags |= QGraphicsItem::ItemIsMovable;
                setFlags(flags);
            }

            QRectF boundingRect() const override { return QRectF(-70, -90, 160, 240); }

            static constexpr qreal width() { return 160; }
            static constexpr qreal height() { return 240; }
            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (!hmiDragEnabled())
                    return QGraphicsItem::itemChange(change, value);
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

            void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
            {
                QGraphicsItem::mouseReleaseEvent(event);
                if (!hmiDragEnabled())
                    return;
                qInfo().noquote() << QString("[HMI坐标] %1 pos=(%2, %3)")
                                         .arg(m_name)
                                         .arg(pos().x(), 0, 'f', 0)
                                         .arg(pos().y(), 0, 'f', 0);
                appendHmiPositionLine(formatHmiPositionLine("MOVE", m_name, pos()));
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
                p->setBrush(kUiShadow);
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
                p->setBrush(kUiMetalDark);
                p->setPen(QPen(borderColor, 2));
                p->drawRect(QRectF(shell.left() - 18, shell.center().y() - 5, 18, 10));
                p->drawRect(QRectF(shell.right(), shell.bottom() - 10, 18, 10));

                // 支撑
                p->setBrush(kUiMetalDark);
                p->setPen(QPen(kUiBorder, 1));
                p->drawRect(QRectF(shell.left() + 10, bottom.bottom() - 2, 8, 20));
                p->drawRect(QRectF(shell.right() - 18, bottom.bottom() - 2, 8, 20));
                p->drawRect(QRectF(shell.left() + 6, bottom.bottom() + 16, shell.width() - 12, 4));

                // 人孔
                p->setBrush(kUiMetalDark);
                p->setPen(QPen(kUiBorder, 1.5));
                p->drawEllipse(QRectF(-8, roof.top() + 3, 16, 6));

                // 状态灯（加水中绿，否则灰）
                const QColor lamp = m_filling ? kUiGreen : kUiBorder;
                p->setBrush(lamp);
                p->setPen(QPen(kUiInk, 1));
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
                            QString("LV:%1%%  PT:%2kPa").arg(QString::number(level, 'f', 0)).arg(QString::number(m_pressureMPa, 'f', 1)));

                // 入口/出口触点（用于管道吸附）
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
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
            // 1) 上面入口 2) 下面出口
            static QPointF inletPortLocal() { return QPointF(80, -55); }
            static QPointF outletPortLocal() { return QPointF(80, 35); }

            explicit OutdoorPoolItem(const QString &name)
                : m_name(name), m_waterLevel(80.0)
            {
                setCacheMode(DeviceCoordinateCache);
                QGraphicsItem::GraphicsItemFlags flags = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges;
                if (hmiDragEnabled())
                    flags |= QGraphicsItem::ItemIsMovable;
                setFlags(flags);
            }

            QRectF boundingRect() const override { return QRectF(-80, -100, 160, 200); }

            static constexpr qreal width() { return 160; }
            static constexpr qreal height() { return 200; }
            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (!hmiDragEnabled())
                    return QGraphicsItem::itemChange(change, value);
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

            void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
            {
                QGraphicsItem::mouseReleaseEvent(event);
                if (!hmiDragEnabled())
                    return;
                qInfo().noquote() << QString("[HMI坐标] %1 pos=(%2, %3)")
                                         .arg(m_name)
                                         .arg(pos().x(), 0, 'f', 0)
                                         .arg(pos().y(), 0, 'f', 0);
                appendHmiPositionLine(formatHmiPositionLine("MOVE", m_name, pos()));
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
                const QColor waterColor = kUiWater;

                // 轻阴影
                p->setPen(Qt::NoPen);
                p->setBrush(kUiShadow);
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
                p->setBrush(kUiMetalDark);
                p->setPen(QPen(borderColor, 2));
                // 上出口（到P1）
                p->drawRect(QRectF(poolBody.right(), poolBody.top() + 30, 18, 10));
                // 下出口（到P2）
                p->drawRect(QRectF(poolBody.right(), poolBody.bottom() - 40, 18, 10));

                // 底座
                p->setBrush(kUiMetalMid);
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

                // 入口/出口触点（用于管道吸附）
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
            }

        private:
            QString m_name;
            double m_waterLevel;
        };

        // ========== 三通节点（用于汇合/分支） ==========
        class TeeNodeItem : public QGraphicsItem
        {
        public:
            // 上下触点对调：让 inlet1 / inlet2 的上下位置反一下
            static QPointF inlet1PortLocal() { return QPointF(-18, 8); }
            static QPointF inlet2PortLocal() { return QPointF(-18, -8); }
            static QPointF outletPortLocal() { return QPointF(18, 0); }

            explicit TeeNodeItem(const QString &name)
                : m_name(name)
            {
                setCacheMode(DeviceCoordinateCache);
                QGraphicsItem::GraphicsItemFlags flags = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges;
                if (hmiDragEnabled())
                    flags |= QGraphicsItem::ItemIsMovable;
                setFlags(flags);
            }

            QRectF boundingRect() const override { return QRectF(-24, -20, 48, 40); }

            QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
            {
                if (!hmiDragEnabled())
                    return QGraphicsItem::itemChange(change, value);
                if (change == ItemPositionChange && scene())
                {
                    QPointF newPos = value.toPointF();
                    qreal gridSize = 10.0;
                    qreal xV = qRound(newPos.x() / gridSize) * gridSize;
                    qreal yV = qRound(newPos.y() / gridSize) * gridSize;
                    return QPointF(xV, yV);
                }
                return QGraphicsItem::itemChange(change, value);
            }

            void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
            {
                QGraphicsItem::mouseReleaseEvent(event);
                if (!hmiDragEnabled())
                    return;
                qInfo().noquote() << QString("[HMI坐标] %1 pos=(%2, %3)")
                                         .arg(m_name)
                                         .arg(pos().x(), 0, 'f', 0)
                                         .arg(pos().y(), 0, 'f', 0);
                appendHmiPositionLine(formatHmiPositionLine("MOVE", m_name, pos()));
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 2, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(1, 1, -1, -1), 6, 6);
                }

                // T 形节点（深灰本体 + #666 描边）
                p->setPen(QPen(kUiBorder, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p->drawLine(QPointF(-14, -8), QPointF(-14, 8));
                p->drawLine(QPointF(-14, 0), QPointF(14, 0));

                // 端口触点
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inlet1PortLocal(), 3.5, 3.5);
                p->drawEllipse(inlet2PortLocal(), 3.5, 3.5);
                p->drawEllipse(outletPortLocal(), 3.5, 3.5);
            }

        private:
            QString m_name;
        };

        // ========== 动态管道（跟随图元移动） ==========
        class DynamicPipe : public QGraphicsItem
        {
        public:
            enum class PortDir
            {
                Auto,
                Horizontal,
                Vertical
            };

            DynamicPipe(QGraphicsItem *startItem, QPointF startOffset,
                        QGraphicsItem *endItem, QPointF endOffset,
                        const QList<QPointF> &waypoints = {},
                        PortDir startDir = PortDir::Auto,
                        PortDir endDir = PortDir::Auto)
                : m_startItem(startItem), m_startOffset(startOffset),
                  m_endItem(endItem), m_endOffset(endOffset),
                  m_waypointsStartLocal(waypoints), m_flowing(false),
                  m_flowDashOffset(0.0),
                  m_hovered(false), m_dragWaypointIndex(-1),
                  m_startDir(startDir), m_endDir(endDir)
            {
                setZValue(-1);
                if (hmiDragEnabled())
                {
                    setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
                    setAcceptHoverEvents(true);
                }
                else
                {
                    setFlags({});
                    setAcceptHoverEvents(false);
                }
                m_cachedRect = computeBoundingRect();
            }

            QRectF boundingRect() const override
            {
                return m_cachedRect;
            }

            QPainterPath shape() const override
            {
                // 让命中区域贴合管道，而不是用巨大的 boundingRect
                const QPainterPath path = buildPath();
                if (path.isEmpty())
                    return {};

                QPainterPathStroker stroker;
                stroker.setWidth(20.0);
                stroker.setCapStyle(Qt::RoundCap);
                stroker.setJoinStyle(Qt::RoundJoin);
                return stroker.createStroke(path);
            }

            void refreshGeometry()
            {
                const QRectF next = computeBoundingRect();
                if (next == m_cachedRect)
                    return;
                prepareGeometryChange();
                m_cachedRect = next;
            }

            void setFlowing(bool flowing)
            {
                if (m_flowing != flowing)
                {
                    m_flowing = flowing;
                    update();
                }
            }

            void advanceFlowAnimation(qreal delta)
            {
                if (!m_flowing)
                    return;
                m_flowDashOffset += delta;
                if (m_flowDashOffset > 10000.0)
                    m_flowDashOffset = 0.0;
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                const QPainterPath path = buildPath();
                if (path.isEmpty())
                    return;

                QPen borderPen(kUiPipeOuter, 12.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
                QPen innerPen(kUiPipeInner, 8.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);

                // 绘制管道
                p->setPen(borderPen);
                p->drawPath(path);
                p->setPen(innerPen);
                p->drawPath(path);

                // 流动提示
                if (m_flowing)
                {
                    QPen flowPen(kUiCyan, 4.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
                    flowPen.setDashPattern({4, 4});
                    flowPen.setDashOffset(m_flowDashOffset);
                    p->setPen(flowPen);
                    p->drawPath(path);
                }

                // 选中/悬浮时显示可拖拽拐点（管道拖拽不好用时，用这个改走线）
                if (isSelected() || m_hovered)
                {
                    p->setPen(QPen(kUiCyan, 2));
                    p->setBrush(QColor(kUiCyan.red(), kUiCyan.green(), kUiCyan.blue(), 160));
                    for (const QPointF &wpScene : waypointScenePositions())
                        p->drawEllipse(wpScene, 6, 6);
                }
            }

        protected:
            void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override
            {
                QGraphicsItem::hoverEnterEvent(event);
                m_hovered = true;
                update();
            }

            void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override
            {
                QGraphicsItem::hoverLeaveEvent(event);
                m_hovered = false;
                update();
            }

            void mousePressEvent(QGraphicsSceneMouseEvent *event) override
            {
                if (!hmiDragEnabled())
                {
                    QGraphicsItem::mousePressEvent(event);
                    return;
                }
                if (event->button() != Qt::LeftButton)
                {
                    QGraphicsItem::mousePressEvent(event);
                    return;
                }

                setSelected(true);

                // 1) 先尝试点到已有拐点 2) 没有则创建一个拐点
                const QPointF clickScene = event->scenePos();
                const auto wps = waypointScenePositions();
                int bestIdx = -1;
                qreal bestD2 = 0.0;
                for (int i = 0; i < wps.size(); ++i)
                {
                    const QPointF d = wps[i] - clickScene;
                    const qreal d2 = d.x() * d.x() + d.y() * d.y();
                    if (bestIdx < 0 || d2 < bestD2)
                    {
                        bestIdx = i;
                        bestD2 = d2;
                    }
                }

                constexpr qreal kPickRadius = 14.0;
                if (bestIdx >= 0 && bestD2 <= kPickRadius * kPickRadius)
                {
                    m_dragWaypointIndex = bestIdx;
                }
                else
                {
                    ensureDefaultWaypoint(clickScene);
                    m_dragWaypointIndex = 0;
                }

                event->accept();
            }

            void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
            {
                if (!hmiDragEnabled())
                {
                    QGraphicsItem::mouseMoveEvent(event);
                    return;
                }
                if (m_dragWaypointIndex < 0)
                {
                    QGraphicsItem::mouseMoveEvent(event);
                    return;
                }

                const QPointF snapped = snapToGrid(event->scenePos(), 10.0);
                setWaypointScenePos(m_dragWaypointIndex, snapped);
                refreshGeometry();
                update();
                event->accept();
            }

            void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
            {
                QGraphicsItem::mouseReleaseEvent(event);
                m_dragWaypointIndex = -1;
            }

        private:
            QGraphicsItem *m_startItem;
            QPointF m_startOffset;
            QGraphicsItem *m_endItem;
            QPointF m_endOffset;
            // waypoints 按“起点图元的本地坐标”存储：拖拽设备时管路更稳定（拐点会跟着起点走）
            QList<QPointF> m_waypointsStartLocal;
            bool m_flowing;
            qreal m_flowDashOffset;
            QRectF m_cachedRect;
            bool m_hovered;
            int m_dragWaypointIndex;
            PortDir m_startDir;
            PortDir m_endDir;

            static QPointF snapToGrid(const QPointF &scenePos, qreal gridSize)
            {
                if (gridSize <= 0.0)
                    return scenePos;
                const qreal xV = qRound(scenePos.x() / gridSize) * gridSize;
                const qreal yV = qRound(scenePos.y() / gridSize) * gridSize;
                return QPointF(xV, yV);
            }

            void ensureDefaultWaypoint(const QPointF &scenePos)
            {
                if (!m_waypointsStartLocal.isEmpty())
                    return;
                setWaypointScenePos(0, scenePos);
            }

            QList<QPointF> waypointScenePositions() const
            {
                QList<QPointF> result;
                result.reserve(m_waypointsStartLocal.size());
                for (const QPointF &wpLocal : m_waypointsStartLocal)
                {
                    if (m_startItem)
                        result.append(m_startItem->mapToScene(wpLocal));
                    else
                        result.append(wpLocal);
                }
                return result;
            }

            void setWaypointScenePos(int index, const QPointF &scenePos)
            {
                if (index < 0)
                    return;

                while (m_waypointsStartLocal.size() <= index)
                    m_waypointsStartLocal.append(QPointF());

                if (m_startItem)
                    m_waypointsStartLocal[index] = m_startItem->mapFromScene(scenePos);
                else
                    m_waypointsStartLocal[index] = scenePos;
            }

            QRectF computeBoundingRect() const
            {
                const QList<QPointF> points = buildOrthogonalPolyline();
                if (points.isEmpty())
                    return QRectF();

                qreal minX = points[0].x();
                qreal minY = points[0].y();
                qreal maxX = points[0].x();
                qreal maxY = points[0].y();

                for (const QPointF &pt : points)
                {
                    minX = qMin(minX, pt.x());
                    minY = qMin(minY, pt.y());
                    maxX = qMax(maxX, pt.x());
                    maxY = qMax(maxY, pt.y());
                }

                constexpr qreal kMargin = 30.0;
                return QRectF(minX - kMargin, minY - kMargin, (maxX - minX) + 2 * kMargin, (maxY - minY) + 2 * kMargin);
            }

            static void appendOrthogonalSegment(QList<QPointF> &out, const QPointF &from, const QPointF &to)
            {
                if (out.isEmpty() || out.last() != from)
                    out.append(from);

                // 已经是水平/垂直
                if (qFuzzyCompare(from.x(), to.x()) || qFuzzyCompare(from.y(), to.y()))
                {
                    out.append(to);
                    return;
                }

                // 默认：先水平再垂直（不画斜线）
                out.append(QPointF(to.x(), from.y()));
                out.append(to);
            }

            static qreal snapScalar(qreal v, qreal gridSize)
            {
                if (gridSize <= 0.0)
                    return v;
                return qRound(v / gridSize) * gridSize;
            }

            static void appendRoutedSegment(QList<QPointF> &out,
                                            const QPointF &from,
                                            const QPointF &to,
                                            PortDir fromDir,
                                            PortDir toDir)
            {
                // 已经是水平/垂直，直接连
                if (qFuzzyCompare(from.x(), to.x()) || qFuzzyCompare(from.y(), to.y()))
                {
                    if (out.isEmpty() || out.last() != from)
                        out.append(from);
                    out.append(to);
                    return;
                }

                // 规则：左右口 -> 水平出线；上下口 -> 垂直出线。
                // 需要时允许“两次拐点”，以同时满足起点/终点口的方向。
                const bool fromH = (fromDir == PortDir::Horizontal);
                const bool fromV = (fromDir == PortDir::Vertical);
                const bool toH = (toDir == PortDir::Horizontal);
                const bool toV = (toDir == PortDir::Vertical);

                // 默认回退（兼容 Auto）：先水平再垂直
                auto fallback = [&]()
                { appendOrthogonalSegment(out, from, to); };

                // 只指定了起点方向/终点方向之一时：用一个拐点保证“离开端口”的方向
                if ((fromDir == PortDir::Auto) && (toDir == PortDir::Auto))
                {
                    fallback();
                    return;
                }

                if (fromH && toV)
                {
                    // 起点水平，终点垂直：一拐点即可
                    if (out.isEmpty() || out.last() != from)
                        out.append(from);
                    out.append(QPointF(to.x(), from.y()));
                    out.append(to);
                    return;
                }
                if (fromV && toH)
                {
                    if (out.isEmpty() || out.last() != from)
                        out.append(from);
                    out.append(QPointF(from.x(), to.y()));
                    out.append(to);
                    return;
                }

                if (fromH && toH)
                {
                    // 两端都水平：两拐点，保证最后一段也水平进入终点口
                    const qreal midX = snapScalar((from.x() + to.x()) / 2.0, 10.0);
                    if (out.isEmpty() || out.last() != from)
                        out.append(from);
                    out.append(QPointF(midX, from.y()));
                    out.append(QPointF(midX, to.y()));
                    out.append(to);
                    return;
                }

                if (fromV && toV)
                {
                    // 两端都垂直：两拐点，保证最后一段也垂直进入终点口
                    const qreal midY = snapScalar((from.y() + to.y()) / 2.0, 10.0);
                    if (out.isEmpty() || out.last() != from)
                        out.append(from);
                    out.append(QPointF(from.x(), midY));
                    out.append(QPointF(to.x(), midY));
                    out.append(to);
                    return;
                }

                // 其余组合（含 Auto）：尽量满足已指定的一侧
                if (fromH)
                {
                    if (out.isEmpty() || out.last() != from)
                        out.append(from);
                    out.append(QPointF(to.x(), from.y()));
                    out.append(to);
                    return;
                }
                if (fromV)
                {
                    if (out.isEmpty() || out.last() != from)
                        out.append(from);
                    out.append(QPointF(from.x(), to.y()));
                    out.append(to);
                    return;
                }

                fallback();
            }

            QList<QPointF> buildOrthogonalPolyline() const
            {
                QList<QPointF> result;
                const QPointF start = getStartPos();
                const QPointF end = getEndPos();

                if (m_waypointsStartLocal.isEmpty())
                {
                    appendRoutedSegment(result, start, end, m_startDir, m_endDir);
                    return result;
                }

                QPointF cur = start;
                for (const QPointF &wpLocal : m_waypointsStartLocal)
                {
                    const QPointF wp = m_startItem ? m_startItem->mapToScene(wpLocal) : wpLocal;
                    appendOrthogonalSegment(result, cur, wp);
                    cur = wp;
                }
                appendOrthogonalSegment(result, cur, end);
                return result;
            }

            QPainterPath buildPath() const
            {
                const QList<QPointF> points = buildOrthogonalPolyline();
                if (points.size() < 2)
                    return {};

                QPainterPath path;
                path.moveTo(points[0]);
                for (int i = 1; i < points.size(); ++i)
                    path.lineTo(points[i]);
                return path;
            }

            QPointF getStartPos() const
            {
                if (m_startItem)
                    return m_startItem->mapToScene(m_startOffset);
                return m_startOffset;
            }

            QPointF getEndPos() const
            {
                if (m_endItem)
                    return m_endItem->mapToScene(m_endOffset);
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
                p->setPen(QPen(kUiInk, 1));
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

            ensureUiTokensInitialized();

            // 清理旧网格（避免重复叠加）
            const auto items = scene->items();
            for (auto *it : items)
            {
                if (!it)
                    continue;
                if (it->data(0).toString() == "hmi_grid")
                {
                    scene->removeItem(it);
                    delete it;
                }
            }

            // 工业屏灰背景：仅使用 token，避免背景色与主题脱节
            QLinearGradient bg(rect.topLeft(), rect.bottomLeft());
            bg.setColorAt(0.0, kUiBg2);
            bg.setColorAt(1.0, kUiBg);
            scene->setBackgroundBrush(bg);

            // 轻量网格线（深底上用浅白透明线）
            const QPen minor(kUiGridMinor, 1);
            const QPen major(kUiGridMajor, 1.5);
            const int step = 25;
            for (int x = 0; x <= rect.width(); x += step)
            {
                const bool isMajor = (x % (step * 4) == 0);
                auto *line = scene->addLine(rect.left() + x, rect.top(), rect.left() + x, rect.bottom(), isMajor ? major : minor);
                line->setZValue(-10);
                line->setData(0, "hmi_grid");
            }
            for (int y = 0; y <= rect.height(); y += step)
            {
                const bool isMajor = (y % (step * 4) == 0);
                auto *line = scene->addLine(rect.left(), rect.top() + y, rect.right(), rect.top() + y, isMajor ? major : minor);
                line->setZValue(-10);
                line->setData(0, "hmi_grid");
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
          m_actionBarOverlay(nullptr),
          m_selfCheckBtn(nullptr),
          m_startFillingBtn(nullptr),
          m_drainBtn(nullptr),
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
          m_targetPressure(500.0f),
          m_currentPressure(0.0f)
    {
        ensureUiTokensInitialized();
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

        updateActionBarOverlayGeometry();
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
        m_view->setDragMode(QGraphicsView::RubberBandDrag);
        m_view->setInteractive(true);
        m_scene = new QGraphicsScene(this);
        m_view->setScene(m_scene);
        buildHmiScene();
        rootLayout->addWidget(m_view, 1);

        // ========== 操作按钮栏：叠加在流程区内部靠下（不占用外部布局） ==========
        m_actionBarOverlay = new QWidget(m_view->viewport());
        // 不在 overlay 上设置局部 styleSheet，避免影响应用级 QSS（tone/size）对按钮的匹配
        m_actionBarOverlay->setAutoFillBackground(false);
        m_actionBarOverlay->setAttribute(Qt::WA_NoSystemBackground, true);
        m_actionBarOverlay->setAttribute(Qt::WA_TranslucentBackground, true);

        auto *actionLayout = new QHBoxLayout(m_actionBarOverlay);
        actionLayout->setContentsMargins(30, 10, 12, 50);
        actionLayout->setSpacing(10);

        m_selfCheckBtn = new QPushButton("系统自检", m_actionBarOverlay);
        m_selfCheckBtn->setProperty("tone", "info");
        m_selfCheckBtn->setProperty("size", "lg");

        m_startFillingBtn = new QPushButton("加水", m_actionBarOverlay);
        m_startFillingBtn->setProperty("tone", "good");
        m_startFillingBtn->setProperty("size", "lg");

        m_drainBtn = new QPushButton("放水", m_actionBarOverlay);
        m_drainBtn->setProperty("tone", "warn");
        m_drainBtn->setProperty("size", "lg");

        m_stopFillingBtn = new QPushButton("停止", m_actionBarOverlay);
        m_stopFillingBtn->setProperty("tone", "neutral");
        m_stopFillingBtn->setProperty("size", "lg");
        m_stopFillingBtn->setEnabled(false);

        // 动态属性（tone/size）有时需要显式 polish 才能立即触发 QSS 重算
        auto repolish = [](QWidget *w)
        {
            if (!w)
                return;
            if (auto *s = w->style())
            {
                s->unpolish(w);
                s->polish(w);
            }
            w->update();
        };
        repolish(m_selfCheckBtn);
        repolish(m_startFillingBtn);
        repolish(m_drainBtn);
        repolish(m_stopFillingBtn);

        connect(m_selfCheckBtn, &QPushButton::clicked, this, &PreparationPanel::onSelfCheck);
        connect(m_startFillingBtn, &QPushButton::clicked, this, &PreparationPanel::onStartFilling);
        connect(m_drainBtn, &QPushButton::clicked, this, &PreparationPanel::onDrainWater);
        connect(m_stopFillingBtn, &QPushButton::clicked, this, &PreparationPanel::onStopAll);

        actionLayout->addWidget(m_selfCheckBtn);
        actionLayout->addWidget(m_startFillingBtn);
        actionLayout->addWidget(m_drainBtn);
        actionLayout->addWidget(m_stopFillingBtn);
        actionLayout->addStretch(1);

        updateActionBarOverlayGeometry();
        updateReliefValveStatus();
    }

    void PreparationPanel::updateActionBarOverlayGeometry()
    {
        if (!m_view || !m_actionBarOverlay)
            return;

        QWidget *vp = m_view->viewport();
        if (!vp)
            return;

        const int margin = 10;
        const int h = m_actionBarOverlay->sizeHint().height();
        const int barH = (h > 0) ? h : 56;
        const int w = vp->width();
        const int y = std::max(0, vp->height() - barH - margin);

        m_actionBarOverlay->setGeometry(0, y, w, barH + margin);
        m_actionBarOverlay->raise();
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
        const QPointF p2 = layout.pump2();
        const QPointF v2 = layout.valve2();
        const QPointF tank = layout.tank();
        const QPointF outdoorPool = layout.outdoorPool();
        const QPointF v3 = layout.valve3();
        const QPointF tee = layout.tee();
        const QPointF ps1 = layout.ps1();
        const QPointF ps2 = layout.ps2();
        const QPointF ps3 = layout.ps3();

        // 手工管道走向：如需调整，把对应 QList 填上 waypoint（场景坐标）即可。
        // 注意：这里不要用 const，因为需要给成员赋值。
        HmiPipeWaypointsConfig pipeWps;
        pipeWps.poolToP1Scene = {QPointF(280, 340), QPointF(280, 150)};
        pipeWps.p1ToV1Scene = {QPointF(540, 150), QPointF(660, 150)};
        pipeWps.poolToP2Scene = {QPointF(280, 440), QPointF(280, 540)};
        pipeWps.p2ToV2Scene = {QPointF(540, 540), QPointF(660, 540)};
        pipeWps.v2ToTeeScene = {QPointF(900, 540), QPointF(1660, 459)};
        // pipeWps.v2ToTeeScene = {QPointF(540, 540), QPointF(660, 540)};

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

        auto *valve3 = new ValveItem("V3 电动阀3(流量监控)");
        valve3->setPos(v3);
        valve3->setZValue(2);
        valve3->setScale(kValveScale);
        m_scene->addItem(valve3);

        auto *teeNode = new TeeNodeItem("三通节点");
        teeNode->setPos(tee);
        teeNode->setZValue(2);
        m_scene->addItem(teeNode);

        // 压力传感器图元：可通过配置控制是否显示
        if (hmiShowPressureSensors())
        {
            auto *s1Item = new SensorItem("PS1 压力1");
            s1Item->setPos(ps1);
            s1Item->setZValue(2);
            s1Item->setScale(kSensorScale);
            m_scene->addItem(s1Item);
            m_itemPS1 = s1Item;

            auto *s2Item = new SensorItem("PS2 压力2");
            s2Item->setPos(ps2);
            s2Item->setZValue(2);
            s2Item->setScale(kSensorScale);
            m_scene->addItem(s2Item);
            m_itemPS2 = s2Item;

            auto *s3Item = new SensorItem("PS3 压力3");
            s3Item->setPos(ps3);
            s3Item->setZValue(2);
            s3Item->setScale(kSensorScale);
            m_scene->addItem(s3Item);
            m_itemPS3 = s3Item;
        }
        else
        {
            m_itemPS1 = nullptr;
            m_itemPS2 = nullptr;
            m_itemPS3 = nullptr;
        }

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

        // 初始布局坐标打印（便于现场拖拽微调后回填配置）
        {
            appendHmiPositionLine(QString("---- %1 ----").arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
            appendHmiPositionLine(QString("LOG_PATH\t%1").arg(hmiPositionLogPath()));

            auto logPos = [](const QString &name, const QGraphicsItem *item)
            {
                if (!item)
                    return;
                const QPointF p = item->pos();

                qInfo().noquote() << QString("[HMI初始坐标] %1 pos=(%2, %3)")
                                         .arg(name)
                                         .arg(p.x(), 0, 'f', 0)
                                         .arg(p.y(), 0, 'f', 0);

                appendHmiPositionLine(formatHmiPositionLine("INIT", name, p));
            };

            logPos("室外水池", outdoorPoolItem);
            logPos("P1 变频泵1", pump1);
            logPos("P2 变频泵2", pump2);
            logPos("V1 电动阀1", valve1);
            logPos("V2 电动阀2", valve2);
            logPos("V3 电动阀3(流量监控)", valve3);
            logPos("分水罐", tankItem);
            logPos("三通节点", teeNode);

            if (hmiShowPressureSensors())
            {
                logPos("PS1 压力1", m_itemPS1);
                logPos("PS2 压力2", m_itemPS2);
                logPos("PS3 压力3", m_itemPS3);
            }
        }
        // ========== 创建动态管道（使用相对偏移量） ==========
        // 室外水池 -> P1
        auto *pipePool1 = new DynamicPipe(
            m_itemOutdoorPool, OutdoorPoolItem::outletPortLocal(),
            m_itemPump1, PumpItem::inletPortLocal(),
            toStartLocalWaypoints(m_itemOutdoorPool, pipeWps.poolToP1Scene),
            DynamicPipe::PortDir::Vertical,
            DynamicPipe::PortDir::Horizontal);
        m_scene->addItem(pipePool1);
        m_pipes.append(pipePool1);

        // 室外水池 -> P2
        auto *pipePool2 = new DynamicPipe(
            m_itemOutdoorPool, OutdoorPoolItem::outletPortLocal(),
            m_itemPump2, PumpItem::inletPortLocal(),
            toStartLocalWaypoints(m_itemOutdoorPool, pipeWps.poolToP2Scene),
            DynamicPipe::PortDir::Vertical,
            DynamicPipe::PortDir::Horizontal);
        m_scene->addItem(pipePool2);
        m_pipes.append(pipePool2);

        // P1 -> V1
        auto *pipe1V1 = new DynamicPipe(
            m_itemPump1, PumpItem::outletPortLocal(),
            m_itemValve1, ValveItem::inletPortLocal(),
            toStartLocalWaypoints(m_itemPump1, pipeWps.p1ToV1Scene),
            DynamicPipe::PortDir::Horizontal,
            DynamicPipe::PortDir::Horizontal);
        m_scene->addItem(pipe1V1);
        m_pipes.append(pipe1V1);

        // P2 -> V2
        auto *pipe2V2 = new DynamicPipe(
            m_itemPump2, PumpItem::outletPortLocal(),
            m_itemValve2, ValveItem::inletPortLocal(),
            toStartLocalWaypoints(m_itemPump2, pipeWps.p2ToV2Scene),
            DynamicPipe::PortDir::Horizontal,
            DynamicPipe::PortDir::Horizontal);
        m_scene->addItem(pipe2V2);
        m_pipes.append(pipe2V2);

        // V1 -> Tank
        auto *pipeV1Tank = new DynamicPipe(
            m_itemValve1, ValveItem::outletPortLocal(),
            m_itemTank, TankItem::inletPortLocal(),
            toStartLocalWaypoints(m_itemValve1, pipeWps.v1ToTankScene),
            DynamicPipe::PortDir::Horizontal,
            DynamicPipe::PortDir::Horizontal);
        m_scene->addItem(pipeV1Tank);
        m_pipes.append(pipeV1Tank);

        // Tank(outlet) -> V3(inlet)
        auto *pipeTankV3 = new DynamicPipe(
            m_itemTank, TankItem::outletPortLocal(),
            valve3, ValveItem::inletPortLocal(),
            toStartLocalWaypoints(m_itemTank, pipeWps.tankToV3Scene),
            DynamicPipe::PortDir::Horizontal,
            DynamicPipe::PortDir::Horizontal);
        m_scene->addItem(pipeTankV3);
        m_pipes.append(pipeTankV3);

        // V3(outlet) -> Tee(inlet2)
        auto *pipeV3Tee = new DynamicPipe(
            valve3, ValveItem::outletPortLocal(),
            teeNode, TeeNodeItem::inlet2PortLocal(),
            toStartLocalWaypoints(valve3, pipeWps.v3ToTeeScene),
            DynamicPipe::PortDir::Horizontal,
            DynamicPipe::PortDir::Horizontal);
        m_scene->addItem(pipeV3Tee);
        m_pipes.append(pipeV3Tee);

        // V2(outlet) -> Tee(inlet1)（不再接分水罐入口）
        auto *pipeV2Tee = new DynamicPipe(
            m_itemValve2, ValveItem::outletPortLocal(),
            teeNode, TeeNodeItem::inlet1PortLocal(),
            toStartLocalWaypoints(m_itemValve2, pipeWps.v2ToTeeScene),
            DynamicPipe::PortDir::Horizontal,
            DynamicPipe::PortDir::Horizontal);
        m_scene->addItem(pipeV2Tee);
        m_pipes.append(pipeV2Tee);

        // 打开管道动画（流动虚线）
        for (auto *pipe : m_pipes)
        {
            if (auto *dp = dynamic_cast<DynamicPipe *>(pipe))
                dp->setFlowing(true);
        }

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
        auto *caption = m_scene->addText("测试准备区流程");
        caption->setDefaultTextColor(kUiText);
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
        // 主题可能在配置加载后才可读：这里做一次轻量检查，变更时刷新背景/网格
        if (ensureUiTokensInitialized())
        {
            if (m_scene)
            {
                addBlueGridBackground(m_scene, m_scene->sceneRect());

                // 强制刷新场景内所有图元（含缓存图元），让主题立即体现在设备/文字/图标上
                const auto items = m_scene->items();
                for (auto *it : items)
                {
                    if (!it)
                        continue;
                    // grid 线条已在 addBlueGridBackground 中重建
                    if (it->data(0).toString() == "hmi_grid")
                        continue;

                    if (auto *text = dynamic_cast<QGraphicsTextItem *>(it))
                        text->setDefaultTextColor(kUiText);

                    it->update();
                }
                m_scene->update();
            }
        }

        // 强制所有管道重绘以跟随图元移动（同时刷新几何，避免 boundingRect 变化导致裁剪）
        for (auto *pipe : m_pipes)
        {
            if (!pipe)
                continue;

            if (auto *dp = dynamic_cast<DynamicPipe *>(pipe))
            {
                dp->advanceFlowAnimation(-1.0);
                dp->refreshGeometry();
            }
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
        addRow("压力传感器1", p1.id != 0, QString("%1, %2 kPa").arg(statusToText(p1.status)).arg(p1.pressure / 1e3, 0, 'f', 1));

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
            item->setPressureMPa(s1.pressure / 1e3);
        if (auto *item = dynamic_cast<SensorItem *>(m_itemPS2))
            item->setPressureMPa(s2.pressure / 1e3);
        if (auto *item = dynamic_cast<SensorItem *>(m_itemPS3))
            item->setPressureMPa(s3.pressure / 1e3);

        // 以传感器3作为分水罐压力（对应 docs/测试准备区流程图.mmd）
        m_currentPressure = static_cast<float>(s3.pressure / 1e3);
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
            m_tankLevelText->setText(QString("%1  (%2)").arg(levelText).arg(fmtKPa(s3.pressure)));
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
                                           QString("确定开始加水吗？\n\n流程：\n1. 启动变频泵1（频率: %1 Hz）\n2. 打开电动阀1(进水阀)\n3. 监测压力直至达到 %2 kPa")
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
        if (m_drainBtn)
            m_drainBtn->setEnabled(false);
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
        onStopAll();
    }

    void PreparationPanel::onDrainWater()
    {
        if (!m_deviceManager)
        {
            QMessageBox::warning(this, "错误", "设备管理器未初始化");
            return;
        }

        // 确认操作
        auto reply = QMessageBox::question(this, "确认",
                                           "确定开始放水吗？\n\n提示：将关闭进水并打开出水阀(阀2)。",
                                           QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No)
            return;

        // 停止加水状态（避免冲突）
        m_isFilling = false;
        m_deviceManager->controlValve(1, false);
        m_deviceManager->controlPump(1, false);

        // 打开出水阀
        if (!m_deviceManager->controlValve(2, true))
        {
            QMessageBox::warning(this, "错误", "打开出水阀失败");
            return;
        }

        // 更新UI
        if (m_startFillingBtn)
            m_startFillingBtn->setEnabled(false);
        if (m_drainBtn)
            m_drainBtn->setEnabled(false);
        if (m_stopFillingBtn)
            m_stopFillingBtn->setEnabled(true);
        if (m_pumpFrequencySpinBox)
            m_pumpFrequencySpinBox->setEnabled(false);
        if (m_targetPressureSpinBox)
            m_targetPressureSpinBox->setEnabled(false);
        if (m_statusBadge)
        {
            m_statusBadge->setText("放水中");
            setBadgeTone(m_statusBadge, "warn");
        }
    }

    void PreparationPanel::onStopAll()
    {
        if (!m_deviceManager)
            return;

        m_isFilling = false;

        // 关闭阀门与停止泵（仅停止准备区相关动作）
        m_deviceManager->controlValve(1, false);
        m_deviceManager->controlValve(2, false);
        m_deviceManager->controlPump(1, false);

        // 更新UI
        if (m_startFillingBtn)
            m_startFillingBtn->setEnabled(true);
        if (m_drainBtn)
            m_drainBtn->setEnabled(true);
        if (m_stopFillingBtn)
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
