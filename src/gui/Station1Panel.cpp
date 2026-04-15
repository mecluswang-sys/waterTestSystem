/**
 * @file Station1Panel.cpp
 * @brief 1号操作台面板实现（流程图展示）
 */

#include "gui/Station1Panel.h"

#include "DeviceManager.h"
#include "ConfigManager.h"
#include "StationClient.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsLineItem>
#include <QVBoxLayout>
#include <QPainterPath>
#include <QLinearGradient>
#include <QResizeEvent>
#include <QFont>
#include <QShowEvent>
#include <QTimer>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QMessageBox>
#include <QStyle>
#include <QDebug>
#include <QDateTime>
#include <QEvent>
#include <QMouseEvent>
#include <QtMath>
#include <QDir>
#include <vector>
#include <array>

namespace WaterTest
{
    namespace
    {
        constexpr qreal kDeviceItemScale = 1.25;
        constexpr qreal kPipeOuterWidth = 14.0;
        constexpr qreal kPipeInnerWidth = 9.0;
        constexpr qreal kPipeFlowWidth = 5.0;

        // 说明：Station1Panel 需要与“测试准备区(PreparationPanel)”保持一致的拟物/HMI风格。
        // PreparationPanel 的 HMI 图元类都内联在 cpp 中，无法直接复用；这里复用同一套“主题 token + 网格背景”，
        // 并把节点/连线改成拟物面板与“管道”风格（外圈/内圈）。

        // ===== DQ 继电器定义表（含 M 区特殊映射） =====
        struct RelayDef
        {
            uint8_t index;  // 线性索引：0 特殊映射 M100.0，其余沿用 Q 区线性索引
            QString label;  // 设备名称
            QString addr;   // 显示地址（如 "Q0.0"）
            QString type;   // 类型提示："valve" 或 "pump"
        };

        // 1号操作台 DQ 输出匹配表
        // index0: 电磁阀1 -> M100.0；其余沿用 Q0.1-Q1.1（现场联调项保持不变）
        static const std::array<RelayDef, 10> kStation1Relays{{
            {0,  "电磁阀1",  "M100.0", "valve"},
            {1,  "出水电磁阀",  "Q0.1", "valve"},
            {2,  "排气电磁阀",  "Q0.2", "valve"},
            {3,  "待测阀电磁阀", "Q0.3", "valve"},
            {4,  "调压阀电磁阀", "Q0.4", "valve"},
            {5,  "回流阀电磁阀", "Q0.5", "valve"},
            {6,  "备用继电器",  "Q0.6", "valve"},
            {7,  "备用继电器",  "Q0.7", "valve"},
            {8,  "供压泵1启停",  "Q1.0", "pump"},
            {9,  "供压泵2启停",  "Q1.1", "pump"},
        }};

        static QString formatRelayBtnText(const QString &addr, const QString &label, bool on)
        {
            const QString stateText = on ? "● 通/得电" : "○ 断/失电";
            return QString("%1\n%2\n%3")
                .arg(addr, label, stateText);
        }

        static void ensureHmiConfigLoadedOnce()
        {
            static bool tried = false;
            if (tried)
                return;
            tried = true;
            (void)ConfigManager::getInstance().loadConfig("config/system.conf");
        }

        static QString hmiThemePreset()
        {
            // 与 PreparationPanel 保持一致：ui.hmi.theme 为空或 auto 时跟随 ui.theme
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

        // ===== HMI 视觉 token（与 PreparationPanel 同源） =====
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
        static QColor kUiShadow;
        static QColor kUiInk;
        static QColor kUiMetalDark;
        static QColor kUiMetalMid;
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
            kUiCyan = t.cyan;
            kUiShadow = t.shadow;
            kUiInk = t.ink;
            kUiGreen = t.green;
            kUiRed = t.red;
            kUiOrange = t.orange;
            kUiPurple = t.purple;
            kUiMetalDark = t.metalDark;
            kUiMetalMid = t.metalMid;

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

            applied = normalized;
            return true;
        }

        static void addBlueGridBackground(QGraphicsScene *scene, const QRectF &rect)
        {
            if (!scene)
                return;

            ensureUiTokensInitialized();

            // 清理旧网格
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

            QLinearGradient bg(rect.topLeft(), rect.bottomLeft());
            bg.setColorAt(0.0, kUiBg2);
            bg.setColorAt(1.0, kUiBg);
            scene->setBackgroundBrush(bg);

            const QPen minor(kUiGridMinor, 1);
            const QPen major(kUiGridMajor, 1.5);
            const int step = 100;
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

        static void addHmiPipeWithArrow(QGraphicsScene *scene,
                                        const QPainterPath &path,
                                        const QPointF &arrowTip,
                                        const QPointF &arrowFrom)
        {
            ensureUiTokensInitialized();

            // 画“管道”：外圈 + 内圈（拟物厚重感）
            {
                auto *outer = scene->addPath(path, QPen(kUiPipeOuter, kPipeOuterWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                outer->setZValue(1);
                auto *inner = scene->addPath(path, QPen(kUiPipeInner, kPipeInnerWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                inner->setZValue(2);
            }

            Q_UNUSED(arrowTip);
            Q_UNUSED(arrowFrom);
        }

        // ========== Station1 的图标化拟物设备图元（与 PreparationPanel 同风格） ==========
        class AccumulatorItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return QPointF(-50, 0); }
            static QPointF outletPortLocal() { return QPointF(50, 0); }
            static QPointF returnPortLocal() { return QPointF(0, 55); }

            explicit AccumulatorItem(const QString &name)
                : m_name(name)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return QRectF(-60, -55, 120, 120); }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 2.5, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 12, 12);
                }

                // 轻阴影
                p->setPen(Qt::NoPen);
                p->setBrush(kUiShadow);
                p->drawRoundedRect(QRectF(-54, -50, 108, 100).translated(3, 4), 18, 18);

                // 罐体（横向胶囊）
                const QRectF body(-50, -26, 100, 52);
                QLinearGradient g(body.topLeft(), body.bottomLeft());
                g.setColorAt(0.0, kUiBody.lighter(112));
                g.setColorAt(1.0, kUiBody.darker(108));
                p->setBrush(g);
                p->setPen(QPen(kUiBorder, 2));
                p->drawRoundedRect(body, 26, 26);

                // 内部“气囊”符号
                p->setBrush(Qt::NoBrush);
                p->setPen(QPen(kUiBorderWeak, 2));
                p->drawArc(QRectF(-30, -18, 60, 36), 30 * 16, 120 * 16);

                // 管口
                p->setBrush(kUiMetalDark);
                p->setPen(QPen(kUiBorder, 2));
                p->drawRect(QRectF(body.left() - 14, -6, 14, 12));
                p->drawRect(QRectF(body.right(), -6, 14, 12));
                p->drawRect(QRectF(-6, body.bottom(), 12, 16));

                // 文本
                p->setPen(kUiText);
                QFont f = p->font();
                f.setPointSize(9);
                f.setBold(true);
                p->setFont(f);
                p->drawText(QRectF(-60, 30, 120, 20), Qt::AlignCenter, m_name);

                // 端口触点
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
                p->drawEllipse(returnPortLocal(), 4, 4);
            }

        private:
            QString m_name;
        };

        class ValveItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return QPointF(-40, 12); }
            static QPointF outletPortLocal() { return QPointF(45, 12); }
            static QPointF topPortLocal() { return QPointF(0, -2); }
            static QPointF bottomPortLocal() { return QPointF(0, 26); }

            explicit ValveItem(const QString &name, bool open = true, double degree = 100.0)
                : m_name(name), m_open(open), m_degree(degree)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return QRectF(-50, -40, 104, 100); }

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
                p->setRenderHint(QPainter::Antialiasing, true);

                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 10, 10);
                }

                const QColor borderColor = kUiBorder;
                const QColor discColor = m_open ? kUiGreen : kUiRed;
                const QColor statusColor = discColor;
                const QColor stateTint = m_open ? QColor(56, 189, 120, 52) : QColor(239, 68, 68, 56);
                const QColor stateEdge = m_open ? QColor(34, 197, 94) : QColor(239, 68, 68);

                // 轻阴影
                p->setPen(Qt::NoPen);
                p->setBrush(kUiShadow);
                p->drawRoundedRect(QRectF(-40, -38, 80, 98).translated(2, 3), 10, 10);

                // 状态底色层：开/关时使用高对比色进行整体提示。
                p->setBrush(stateTint);
                p->setPen(QPen(stateEdge, 2));
                p->drawRoundedRect(QRectF(-40, -38, 80, 98), 10, 10);

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
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(statusColor.red(), statusColor.green(), statusColor.blue(), 90));
                p->drawEllipse(QPointF(-11, -30), 6, 6);
                p->setBrush(statusColor);
                p->setPen(QPen(kUiInk, 1));
                p->drawEllipse(QPointF(-11, -30), 4, 4);

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
                p->drawEllipse(QPointF(24, -26), 7, 7);
                // 指针
                const double ang = (rotation - 90.0) * 3.14159 / 180.0;
                p->setPen(QPen(statusColor, 3, Qt::SolidLine, Qt::RoundCap));
                p->drawLine(QPointF(24, -26), QPointF(24 + 6 * std::cos(ang), -26 + 6 * std::sin(ang)));

                // 文本
                p->setPen(kUiText);
                QFont f = p->font();
                f.setPointSize(9);
                f.setBold(true);
                p->setFont(f);
                p->drawText(QRectF(-52, 28, 104, 16), Qt::AlignCenter, m_name);

                QFont f2 = p->font();
                f2.setPointSize(7);
                f2.setBold(true);
                f2.setFamily("Consolas");
                p->setFont(f2);
                p->setPen(Qt::NoPen);
                p->setBrush(stateEdge);
                p->drawRoundedRect(QRectF(-24, 44, 48, 12), 4, 4);
                p->setPen(Qt::white);
                p->drawText(QRectF(-24, 44, 48, 12), Qt::AlignCenter, m_open ? "OPEN" : "CLOSE");

                // 入口/出口触点
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
                p->drawEllipse(topPortLocal(), 4, 4);
                p->drawEllipse(bottomPortLocal(), 4, 4);
            }

        private:
            QString m_name;
            bool m_open;
            double m_degree;
        };

        static void setRelayValveGlyphState(QGraphicsScene *scene, uint8_t relayIndex, bool on)
        {
            if (!scene)
                return;
            const auto items = scene->items();
            for (auto *it : items)
            {
                if (!it)
                    continue;
                const QVariant v = it->data(4);
                if (!v.isValid() || v.toInt() != relayIndex)
                    continue;

                auto *valve = dynamic_cast<ValveItem *>(it);
                if (!valve)
                    continue;

                valve->setOpen(on);
                valve->setDegree(on ? 100.0 : 0.0);
            }
        }

        class ThreeWayValveItem : public QGraphicsItem
        {
        public:
            // 左进，右出，同时带一个“下支路”示意口
            static QPointF inletPortLocal() { return QPointF(-44, 12); }
            static QPointF outletPortLocal() { return QPointF(50, 12); }
            static QPointF branchPortLocal() { return QPointF(0, 42); }

            explicit ThreeWayValveItem(const QString &name)
                : m_name(name)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return QRectF(-56, -52, 120, 122); }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 10, 10);
                }

                // 基于 ValveItem 的菱形阀体 + 下支路
                const QColor borderColor = kUiBorder;
                const QColor discColor = kUiOrange;

                // 轻阴影
                p->setPen(Qt::NoPen);
                p->setBrush(kUiShadow);
                p->drawRoundedRect(QRectF(-44, -40, 92, 104).translated(2, 3), 10, 10);

                // 执行器
                p->setBrush(kUiBody);
                p->setPen(QPen(borderColor, 2));
                p->drawRoundedRect(QRectF(-16, -34, 32, 18), 2, 2);

                p->setPen(kUiTextMuted);
                QFont mf = p->font();
                mf.setPointSize(8);
                mf.setBold(true);
                mf.setFamily("Consolas");
                p->setFont(mf);
                p->drawText(QRectF(-16, -34, 32, 18), Qt::AlignCenter, "3W");

                // 阀体（菱形）
                QPainterPath diamond;
                diamond.moveTo(0, -2);
                diamond.lineTo(28, 12);
                diamond.lineTo(0, 26);
                diamond.lineTo(-28, 12);
                diamond.closeSubpath();
                p->setBrush(kUiBody);
                p->setPen(QPen(borderColor, 2));
                p->drawPath(diamond);

                // 三通符号
                p->setPen(QPen(borderColor, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p->drawLine(QPointF(0, 20), QPointF(0, 40));

                // 阀瓣
                p->setBrush(discColor);
                p->setPen(QPen(kUiInk, 1));
                p->drawEllipse(QPointF(0, 12), 16, 3);

                // 文本
                p->setPen(kUiText);
                QFont f = p->font();
                f.setPointSize(9);
                f.setBold(true);
                p->setFont(f);
                p->drawText(QRectF(-60, 30, 120, 18), Qt::AlignCenter, m_name);

                // 端口触点
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
                p->drawEllipse(branchPortLocal(), 4, 4);
            }

        private:
            QString m_name;
        };

        class SensorItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return QPointF(-52, 14); }
            static QPointF outletPortLocal() { return QPointF(52, 14); }

            explicit SensorItem(const QString &name, const QString &unit = "kPa", QColor typeColor = QColor())
                : m_name(name), m_value(0.0), m_unit(unit), m_typeColor(typeColor.isValid() ? typeColor : kUiPurple)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return QRectF(-62, -46, 124, 110); }

            void setValue(double v)
            {
                if (qFuzzyCompare(m_value, v))
                    return;
                m_value = v;
                update();
            }

            void setDisplayDecimals(int decimals)
            {
                decimals = std::clamp(decimals, 0, 6);
                if (m_displayDecimals == decimals)
                    return;
                m_displayDecimals = decimals;
                update();
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 8, 8);
                }

                const QColor borderColor = kUiBorder;
                const QColor valueColor = m_typeColor;

                const QRectF card(-52, -40, 104, 56);

                // 阴影
                p->setPen(Qt::NoPen);
                p->setBrush(kUiShadow);
                p->drawRect(card.translated(2.5, 3.0));

                // 卡片
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

                // 数值高亮窗：提高对比度，让压力数值在远距离也清晰可读。
                const QRectF valuePanel(card.left() + 5, card.top() + 16, card.width() - 10, 28);
                p->setPen(QPen(valueColor, 1.5));
                p->setBrush(QColor(12, 18, 28, 220));
                p->drawRoundedRect(valuePanel, 4, 4);

                // Value
                QFont valFont = p->font();
                valFont.setPointSize(17);
                valFont.setBold(true);
                valFont.setFamily("Consolas");
                p->setFont(valFont);
                p->setPen(QColor(245, 248, 255));
                const QString v = QString::number(m_value, 'f', m_displayDecimals);
                p->drawText(valuePanel, Qt::AlignCenter, v);

                // Unit
                QFont unitFont = p->font();
                unitFont.setPointSize(8);
                unitFont.setBold(true);
                unitFont.setFamily("Consolas");
                p->setFont(unitFont);
                const QRectF unitPanel(card.right() - 36, card.top() + 45, 30, 10);
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(valueColor.red(), valueColor.green(), valueColor.blue(), 220));
                p->drawRoundedRect(unitPanel, 3, 3);
                p->setPen(Qt::white);
                p->drawText(unitPanel, Qt::AlignCenter, m_unit);

                // 引线（虚线）+ 类型点
                p->setPen(QPen(kUiBorder, 1.5, Qt::DashLine, Qt::RoundCap));
                p->drawLine(QPointF(0, card.bottom()), QPointF(0, card.bottom() + 16));
                p->setBrush(m_typeColor);
                p->setPen(QPen(kUiInk, 1));
                p->drawEllipse(QPointF(0, card.bottom() + 16), 3, 3);

                // 端口触点
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
            }

        private:
            QString m_name;
            double m_value;
            int m_displayDecimals = 2;
            QString m_unit;
            QColor m_typeColor;
        };

        class FlowMeterItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return QPointF(-60, 0); }
            static QPointF outletPortLocal() { return QPointF(60, 0); }

            explicit FlowMeterItem(const QString &name)
                : m_name(name), m_flow(0.0), m_unit("L/min"), m_hasAlarm(false), m_emptyPipeAlarm(0), m_excitationAlarm(0)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return QRectF(-70, -52, 140, 110); }

            void setFlow(double v)
            {
                if (qFuzzyCompare(m_flow, v))
                    return;
                m_flow = v;
                update();
            }

            void setUnit(const QString &unit)
            {
                if (m_unit == unit)
                    return;
                m_unit = unit;
                update();
            }

            void setAlarm(bool hasAlarm)
            {
                if (m_hasAlarm == hasAlarm)
                    return;
                m_hasAlarm = hasAlarm;
                if (!m_hasAlarm)
                {
                    m_emptyPipeAlarm = 0;
                    m_excitationAlarm = 0;
                }
                update();
            }

            void setAlarmDetail(int emptyPipeAlarm, int excitationAlarm)
            {
                if (m_emptyPipeAlarm == emptyPipeAlarm && m_excitationAlarm == excitationAlarm)
                    return;
                m_emptyPipeAlarm = emptyPipeAlarm;
                m_excitationAlarm = excitationAlarm;
                update();
            }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 12, 12);
                }

                // 阴影
                p->setPen(Qt::NoPen);
                p->setBrush(kUiShadow);
                p->drawRoundedRect(QRectF(-62, -40, 124, 80).translated(3, 4), 14, 14);

                // 表体
                p->setBrush(kUiPanel);
                const bool activeAlarm = m_hasAlarm && (m_emptyPipeAlarm != 0 || m_excitationAlarm != 0);
                p->setPen(QPen(activeAlarm ? QColor(255, 90, 90) : kUiBorder, activeAlarm ? 3 : 2));
                p->drawRoundedRect(QRectF(-60, -38, 120, 76), 14, 14);

                // 管口
                p->setBrush(kUiMetalDark);
                p->setPen(QPen(kUiBorder, 2));
                p->drawRect(QRectF(-74, -6, 14, 12));
                p->drawRect(QRectF(60, -6, 14, 12));

                // 仪表盘
                p->setBrush(kUiBody);
                p->setPen(QPen(kUiBorder, 2));
                p->drawEllipse(QPointF(-18, 0), 18, 18);
                // 指针
                p->setPen(QPen(kUiCyan, 2, Qt::SolidLine, Qt::RoundCap));
                p->drawLine(QPointF(-18, 0), QPointF(-18 + 12, -8));

                // 数值
                p->setPen(kUiOrange);
                QFont valFont = p->font();
                valFont.setPointSize(12);
                valFont.setBold(true);
                valFont.setFamily("Consolas");
                p->setFont(valFont);
                p->drawText(QRectF(4, -14, 54, 24), Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_flow, 'f', 2));

                p->setPen(kUiTextDim);
                QFont unitFont = p->font();
                unitFont.setPointSize(8);
                unitFont.setBold(false);
                unitFont.setFamily("Consolas");
                p->setFont(unitFont);
                p->drawText(QRectF(4, 6, 54, 16), Qt::AlignLeft | Qt::AlignVCenter, m_unit);

                if (activeAlarm)
                {
                    p->setPen(Qt::NoPen);
                    p->setBrush(QColor(255, 90, 90));
                    p->drawEllipse(QRectF(46, -28, 8, 8));
                    p->setPen(QColor(255, 90, 90));
                    p->setFont(unitFont);
                    QString alarmText;
                    if (m_emptyPipeAlarm != 0 && m_excitationAlarm != 0)
                        alarmText = "空管/激磁报警";
                    else if (m_emptyPipeAlarm != 0)
                        alarmText = "空管报警";
                    else
                        alarmText = "激磁报警";

                    p->drawText(QRectF(4, 20, 88, 14), Qt::AlignLeft | Qt::AlignVCenter, alarmText);
                }

                // 名称
                p->setPen(kUiText);
                QFont f = p->font();
                f.setPointSize(9);
                f.setBold(true);
                p->setFont(f);
                p->drawText(QRectF(-70, 32, 140, 20), Qt::AlignCenter, m_name);

                // 端口触点
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
            }

        private:
            QString m_name;
            double m_flow;
            QString m_unit;
            bool m_hasAlarm;
            int m_emptyPipeAlarm;
            int m_excitationAlarm;
        };

        class LoopItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return QPointF(-58, 0); }
            static QPointF outletPortLocal() { return QPointF(58, 0); }
            static QPointF bottomPortLocal() { return QPointF(0, 34); }

            explicit LoopItem(const QString &name)
                : m_name(name)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return QRectF(-70, -34, 140, 84); }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                p->setRenderHint(QPainter::Antialiasing, true);

                if (isSelected())
                {
                    p->setPen(QPen(kUiCyan, 3, Qt::DashLine));
                    p->setBrush(Qt::NoBrush);
                    p->drawRoundedRect(boundingRect().adjusted(2, 2, -2, -2), 12, 12);
                }

                // 阴影
                p->setPen(Qt::NoPen);
                p->setBrush(kUiShadow);
                p->drawRoundedRect(QRectF(-60, -26, 120, 62).translated(3, 4), 14, 14);

                // 本体
                p->setBrush(kUiBody);
                p->setPen(QPen(kUiBorder, 2));
                p->drawRoundedRect(QRectF(-60, -26, 120, 62), 14, 14);

                // 回路符号
                p->setPen(QPen(kUiCyan, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                QPainterPath loop;
                loop.addEllipse(QPointF(-18, 2), 12, 12);
                p->drawPath(loop);
                p->drawLine(QPointF(-6, 2), QPointF(26, 2));
                p->drawEllipse(QPointF(30, 2), 3, 3);

                // 文本
                p->setPen(kUiText);
                QFont f = p->font();
                f.setPointSize(9);
                f.setBold(true);
                p->setFont(f);
                p->drawText(QRectF(-70, 20, 140, 20), Qt::AlignCenter, m_name);

                // 端口触点
                p->setPen(QPen(kUiBorder, 1));
                p->setBrush(kUiCyan);
                p->drawEllipse(inletPortLocal(), 4, 4);
                p->drawEllipse(outletPortLocal(), 4, 4);
                p->drawEllipse(bottomPortLocal(), 4, 4);
            }

        private:
            QString m_name;
        };

        static void connectPorts(QGraphicsScene *scene, const QPointF &start, const QPointF &end)
        {
            if (!scene)
                return;

            QPainterPath path(start);
            const qreal dx = end.x() - start.x();
            const qreal dy = end.y() - start.y();

            // 规则1：同排连接（y 接近）走“水平主干 + 末端短竖线”，避免斜线。
            if (qAbs(dy) <= 20.0)
            {
                const QPointF corner(end.x(), start.y());
                path.lineTo(corner);
                path.lineTo(end);
                addHmiPipeWithArrow(scene, path, end, corner);
                return;
            }

            // 规则2：同列连接（x 接近）走“竖直主干 + 末端短横线”。
            if (qAbs(dx) <= 20.0)
            {
                const QPointF corner(start.x(), end.y());
                path.lineTo(corner);
                path.lineTo(end);
                addHmiPipeWithArrow(scene, path, end, corner);
                return;
            }

            // 规则3：跨排连接走中间水平走线，保证横平竖直。
            const qreal midY = (start.y() + end.y()) * 0.5;
            path.lineTo(QPointF(start.x(), midY));
            path.lineTo(QPointF(end.x(), midY));
            path.lineTo(end);

            addHmiPipeWithArrow(scene, path, end, QPointF(end.x(), midY));
        }
    }

    Station1Panel::Station1Panel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent)
        : QWidget(parent),
          m_deviceManager(std::move(deviceManager)),
                    m_stationClient(nullptr),
          m_view(nullptr),
          m_scene(nullptr),
          m_flowTimer(nullptr),
          m_dataTimer(nullptr),
          m_flowDashOffset(0.0)
    {
        setupUI();
    }

    Station1Panel::~Station1Panel() = default;

    void Station1Panel::setStationClient(std::shared_ptr<StationClient> stationClient)
    {
        m_stationClient = stationClient;
    }

    void Station1Panel::resizeEvent(QResizeEvent *event)
    {
        QWidget::resizeEvent(event);
        applyAutoFit();
    }

    void Station1Panel::showEvent(QShowEvent *event)
    {
        QWidget::showEvent(event);
        // 关键：首次显示时布局刚完成，确保用最终 viewport 尺寸做 fitInView
        QTimer::singleShot(0, this, [this]()
                           { applyAutoFit(); });
    }

    void Station1Panel::setupUI()
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_view = new QGraphicsView(this);
        m_view->setFrameShape(QFrame::NoFrame);
        m_view->setRenderHint(QPainter::Antialiasing, true);
        m_view->setRenderHint(QPainter::TextAntialiasing, true);
        m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_view->setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);

        m_scene = new QGraphicsScene(this);
        m_view->setScene(m_scene);
        m_view->viewport()->installEventFilter(this);
        layout->addWidget(m_view, 1);

        m_flowTimer = new QTimer(this);
        connect(m_flowTimer, &QTimer::timeout, this, &Station1Panel::updatePipeFlowAnimation);
        m_flowTimer->start(50);

        m_dataTimer = new QTimer(this);
        connect(m_dataTimer, &QTimer::timeout, this, &Station1Panel::updateSensorValues);
        m_dataTimer->start(200);

        // 继电器状态刷新：1 秒一次（降低无意义的 Q 区轮询频率）
        auto *relayTimer = new QTimer(this);
        connect(relayTimer, &QTimer::timeout, this, &Station1Panel::updateRelayButtons);
        relayTimer->start(1000);

        buildScene();

        // 点击阀门图元后弹出控制面板，避免误触：选中后立即清除选中态
        connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]()
                {
            if (!m_scene)
                return;

            const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);

            const auto selected = m_scene->selectedItems();
            if (selected.isEmpty())
                return;

            auto *item = selected.first();

            const QVariant idVar = item->data(3);
            if (!idVar.isValid())
                return;

            const int valveId = idVar.toInt();
            if (valveId <= 0)
                return;

            m_scene->blockSignals(true);
            m_scene->clearSelection();
            m_scene->blockSignals(false);

            auto *valveItem = dynamic_cast<ValveItem *>(item);
            const QString valveName = valveItem ? valveItem->getName() : QString("阀门 #%1").arg(valveId);

            bool isOpen = false;
            if (m_deviceManager)
            {
                const auto valve = m_deviceManager->getValve(static_cast<uint16_t>(valveId));
                isOpen = (valve.status == ValveStatus::OPEN || valve.status == ValveStatus::OPENING);
            }

            QDialog dialog(this);
            dialog.setWindowTitle("阀门控制");
            dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

            auto *mainLayout = new QVBoxLayout(&dialog);
            auto *infoLabel = new QLabel(QString("%1\n当前状态: %2")
                                             .arg(valveName)
                                             .arg(isOpen ? "开启" : "关闭"),
                                         &dialog);
            mainLayout->addWidget(infoLabel);

            auto *buttons = new QHBoxLayout();
            auto *openBtn = new QPushButton("开阀", &dialog);
            auto *closeBtn = new QPushButton("关阀", &dialog);
            auto *cancelBtn = new QPushButton("取消", &dialog);
            buttons->addWidget(openBtn);
            buttons->addWidget(closeBtn);
            buttons->addWidget(cancelBtn);
            mainLayout->addLayout(buttons);

            connect(openBtn, &QPushButton::clicked, &dialog, [&, valveId]() {
                const bool strictMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
                bool ok = false;
                if (m_stationClient && strictMode)
                {
                    ControlCommand cmd;
                    cmd.command_type = 2;
                    cmd.index = static_cast<uint8_t>(valveId - 1);
                    cmd.action = 1;
                    ok = m_stationClient->sendCommand(cmd);
                }
                else if (m_deviceManager)
                {
                    ok = m_deviceManager->controlValve(static_cast<uint16_t>(valveId), true);
                }

                if (!ok)
                {
                    QMessageBox::warning(this, "操作失败", "开阀命令发送失败，请检查主控连接状态。");
                    return;
                }
                dialog.accept();
            });
            connect(closeBtn, &QPushButton::clicked, &dialog, [&, valveId]() {
                const bool strictMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
                bool ok = false;
                if (m_stationClient && strictMode)
                {
                    ControlCommand cmd;
                    cmd.command_type = 2;
                    cmd.index = static_cast<uint8_t>(valveId - 1);
                    cmd.action = 0;
                    ok = m_stationClient->sendCommand(cmd);
                }
                else if (m_deviceManager)
                {
                    ok = m_deviceManager->controlValve(static_cast<uint16_t>(valveId), false);
                }

                if (!ok)
                {
                    QMessageBox::warning(this, "操作失败", "关阀命令发送失败，请检查主控连接状态。");
                    return;
                }
                dialog.accept();
            });
            connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

            dialog.exec(); });
    }

    void Station1Panel::buildRelayPanel(QWidget *container)
    {
        auto *outerLayout = new QVBoxLayout(container);
        outerLayout->setContentsMargins(4, 2, 4, 4);
        outerLayout->setSpacing(2);

        auto *group = new QGroupBox("继电器输出控制 (DQ)", container);
        group->setMinimumHeight(190);
        group->setMaximumHeight(210);
        auto *grid = new QGridLayout(group);
        grid->setContentsMargins(8, 8, 8, 8);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(8);

        // 辅助 lambda：根据通/断状态更新按钮文字和样式
        auto refreshBtn = [](QPushButton *btn, const QString &addr, const QString &label, bool on)
        {
            btn->setText(formatRelayBtnText(addr, label, on));
            btn->setProperty("dqOn", on);
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
        };

        m_relayBtns.clear();
        m_relayBtns.resize(kStation1Relays.size(), nullptr);

        int visibleBtnCount = 0;

        for (const auto &def : kStation1Relays)
        {
            auto *btn = new QPushButton(group);
            btn->setMinimumSize(116, 78);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            // 泵使用不同的语义色（暖色），阀使用默认
            if (def.type == "pump")
            {
                btn->setProperty("tone", "warn");
                btn->setToolTip("泵输出位需要现场联调，当前界面仅显示状态");
            }
            else
                btn->setProperty("tone", "neutral");

            refreshBtn(btn, def.addr, def.label, false);

            const uint8_t idx = def.index;
                if (idx != 0)
                {
                connect(btn, &QPushButton::clicked, this, [this, idx]()
                    { onRelayBtnClicked(idx, "relay_panel_button"); });
                }

            // index=0（电磁阀1）从下方按钮区移除，改由流程图阀门图元点击控制。
            if (def.index == 0)
            {
                btn->setVisible(false);
            }
            else
            {
                const int row = visibleBtnCount / 5;
                const int col = visibleBtnCount % 5;
                grid->addWidget(btn, row, col);
                ++visibleBtnCount;
            }

            if (idx < m_relayBtns.size())
                m_relayBtns[idx] = btn;
        }

        for (int c = 0; c < 5; ++c)
            grid->setColumnStretch(c, 1);

        outerLayout->addWidget(group);
    }

    bool Station1Panel::eventFilter(QObject *watched, QEvent *event)
    {
        if (m_view && watched == m_view->viewport() && event && event->type() == QEvent::MouseButtonRelease)
        {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton)
            {
                QGraphicsItem *hit = m_view->itemAt(mouseEvent->pos());
                while (hit && !hit->data(4).isValid())
                    hit = hit->parentItem();

                if (hit)
                {
                    const int relayIndex = hit->data(4).toInt();
                    if (relayIndex >= 0)
                    {
                        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                        const bool inGlobalRelayGlyphLock = nowMs < m_relayGlyphLockUntilMs;
                        const bool duplicateRelayGlyphClick =
                            (m_lastRelayGlyphIndex == relayIndex) && (nowMs - m_lastRelayGlyphClickMs < 600);

                        if (inGlobalRelayGlyphLock || duplicateRelayGlyphClick)
                        {
                            qWarning() << "[M100][Station1Panel] relay glyph click ignored by debounce"
                                       << "index=" << relayIndex
                                       << "elapsedMs=" << (nowMs - m_lastRelayGlyphClickMs)
                                       << "lockRemainingMs=" << std::max<qint64>(0, m_relayGlyphLockUntilMs - nowMs);
                            return true;
                        }

                        m_lastRelayGlyphIndex = relayIndex;
                        m_lastRelayGlyphClickMs = nowMs;
                        m_relayGlyphLockUntilMs = nowMs + 700;
                        qInfo() << "[M100][Station1Panel] relay glyph hit"
                                << "index=" << relayIndex
                                << "source=" << static_cast<int>(mouseEvent->source())
                                << "spontaneous=" << mouseEvent->spontaneous()
                                << "localPos=" << mouseEvent->pos();
                        onRelayBtnClicked(static_cast<uint8_t>(relayIndex), "glyph");
                        return true;
                    }
                }
            }
        }

        return QWidget::eventFilter(watched, event);
    }

    void Station1Panel::updateRelayButtons()
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (m_stationClient && strictRemoteMode)
            return;

        if (!m_deviceManager)
            return;

        const auto &relays = kStation1Relays;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        for (size_t i = 0; i < relays.size(); ++i)
        {
            bool on = false;
            const bool ok = m_deviceManager->getRelayState(relays[i].index, on);
            if (!ok)
                continue; // PLC 未连接时跳过，不改变显示

            // 若 M100.0 在置位后短时间内被拉回 false，给出可视化提示。
            if (relays[i].index == 0 && m_expectM100Hold)
            {
                const qint64 elapsed = nowMs - m_expectM100SetMs;
                if (on)
                {
                    m_expectM100Hold = false;
                }
                else if (elapsed >= 200 && elapsed <= 3000)
                {
                    qWarning() << "[M100][Station1Panel] auto reset detected after set true"
                               << "elapsedMs=" << elapsed;
                    QMessageBox::information(this,
                                             "M100.0 被自动复位",
                                             "已写入 M100.0=1，但很快回读到 0。\n"
                                             "这通常表示 PLC 程序中有复位逻辑（如联锁条件不满足或 R 线圈）。");
                    m_expectM100Hold = false;
                }
                else if (elapsed > 3000)
                {
                    m_expectM100Hold = false;
                }
            }

            // 图元颜色联动：电磁阀图元随 relay 状态变化。
            if (relays[i].index == 0)
                setRelayValveGlyphState(m_scene, relays[i].index, on);

            QPushButton *btn = (i < m_relayBtns.size()) ? m_relayBtns[i] : nullptr;
            if (!btn)
                continue;
            const bool current = btn->property("dqOn").toBool();
            if (current == on)
                continue; // 无变化，避免重绘闪烁

            btn->setText(formatRelayBtnText(relays[i].addr, relays[i].label, on));
            btn->setProperty("dqOn", on);
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
        }
    }

    void Station1Panel::onRelayBtnClicked(uint8_t index, const char *source)
    {
        if (index == 0)
        {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (m_lastM100ToggleMs > 0 && (nowMs - m_lastM100ToggleMs) < 700)
            {
                qWarning() << "[M100][Station1Panel] relay toggle ignored by index0 guard"
                           << "elapsedMs=" << (nowMs - m_lastM100ToggleMs);
                return;
            }
            m_lastM100ToggleMs = nowMs;
        }

        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        const QString addr = (index < kStation1Relays.size()) ? kStation1Relays[index].addr : QString("Q?");
        const bool useRemote = (m_stationClient && strictRemoteMode);

        qInfo() << "[M100][Station1Panel] relay click"
            << "src=" << source
            << "index=" << index
            << "addr=" << addr
            << "strictRemoteMode=" << strictRemoteMode
            << "hasStationClient=" << (m_stationClient != nullptr)
            << "useRemote=" << useRemote;

        // 泵输出位(Q1.0/Q1.1)暂不开放本地切换，仅做状态观察。
        if (index == 8 || index == 9)
        {
            const int byteOff = index / 8;
            const int bit = index % 8;
            QMessageBox::information(this,
                                     "现场联调项",
                                     QString("Q%1.%2 为泵控制位，需到现场联调，当前仅支持状态查看。")
                                         .arg(byteOff)
                                         .arg(bit));
            return;
        }

        bool current = false;
        if (useRemote)
        {
            if (!m_stationClient->isConnected())
            {
                qWarning() << "[M100][Station1Panel] remote path selected but StationClient disconnected";
            }
            bool gotCurrent = false;
            if (m_deviceManager)
            {
                gotCurrent = m_deviceManager->getRelayState(index, current);
            }
            if (!gotCurrent && index < m_relayBtns.size() && m_relayBtns[index])
            {
                current = m_relayBtns[index]->property("dqOn").toBool();
            }
        }
        else
        {
            if (!m_deviceManager)
                return;
            m_deviceManager->getRelayState(index, current);
        }
        const bool target = !current;

        qInfo() << "[M100][Station1Panel] relay toggle prepare"
                << "index=" << index
                << "addr=" << addr
                << "current=" << current
                << "target=" << target;

        bool ok = false;
        if (useRemote)
        {
            ControlCommand cmd;
            cmd.command_type = 0;
            cmd.index = index;
            cmd.action = target ? 1 : 0;
            ok = m_stationClient->sendCommand(cmd);
            qInfo() << "[M100][Station1Panel] sendCommand result"
                    << "ok=" << ok
                    << "index=" << cmd.index
                    << "action=" << cmd.action;
        }
        else if (m_deviceManager)
        {
            ok = m_deviceManager->setRelay(index, target);
            qInfo() << "[M100][Station1Panel] local setRelay result"
                    << "ok=" << ok
                    << "index=" << index
                    << "target=" << target;
        }

        if (!ok)
        {
            if (index == 0)
                m_expectM100Hold = false;
            qWarning() << "[M100][Station1Panel] relay toggle failed"
                       << "index=" << index
                       << "addr=" << addr
                       << "target=" << target;
            QMessageBox::warning(this, "操作失败",
                                 QString("切换 %1 失败，请检查 PLC/终端连接状态。").arg(addr));
            return;
        }

        // 先做本地乐观刷新，避免同步回读阻塞导致的视觉延迟。
        if (index < m_relayBtns.size() && m_relayBtns[index])
        {
            auto *btn = m_relayBtns[index];
            const auto &def = kStation1Relays[index];
            btn->setText(formatRelayBtnText(def.addr, def.label, target));
            btn->setProperty("dqOn", target);
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
            if (index == 0)
                setRelayValveGlyphState(m_scene, index, target);
            qInfo() << "[M100][Station1Panel] ui optimistic update"
                    << "index=" << index
                    << "addr=" << def.addr
                    << "dqOn=" << target;
        }

        if (useRemote)
        {
            if (index == 0)
                m_expectM100Hold = false;
        }
        else
        {
            if (index == 0)
            {
                if (target)
                {
                    m_expectM100Hold = true;
                    m_expectM100SetMs = QDateTime::currentMSecsSinceEpoch();
                }
                else
                {
                    m_expectM100Hold = false;
                }
            }
            // 回读校准放到事件循环后执行，避免阻塞当前帧绘制。
            QTimer::singleShot(250, this, [this]() { updateRelayButtons(); });
        }
    }

    void Station1Panel::buildScene()
    {
        if (!m_scene)
            return;

        m_scene->clear();

        ensureUiTokensInitialized();

        // 先给一个较大的场景范围，后面会按图元边界收紧
        m_scene->setSceneRect(0, 0, 1900, 780);

        // 与“测试准备区”一致：蓝色网格 + 拟物灰背景
        addBlueGridBackground(m_scene, m_scene->sceneRect());

        // 标题
        auto *caption = m_scene->addText("1号操作台流程");
        caption->setDefaultTextColor(kUiText);
        QFont tf = caption->font();
        tf.setPointSize(14);
        tf.setBold(true);
        caption->setFont(tf);
        caption->setPos(8, 8);
        caption->setData(0, "hmi_caption");
        caption->setZValue(5);

        // ==== 图标化设备布局（两排蛇形：7 + 7）====
        // 布局原则：第一排按“左 -> 右”排列，第二排按“右 -> 左”回折。
        // 目的：在有限宽度内保持流程连续，并增强工艺流向可读性。
        const qreal step = 320;
        const qreal x0 = 10;
        const qreal leftTwoColsShiftX = 200;
        const qreal yRow1 = 220;
        const qreal yRow2 = 530;
        const qreal valvePortYOffset = 14;
        const qreal row1AfterAccumulatorYOffset = -valvePortYOffset;
        const qreal row2AfterFlowMeterYOffset = -valvePortYOffset;
        const qreal pressureSensorTapYOffset = -28;

        const qreal firstRowAnchorX = x0 + leftTwoColsShiftX;
        const qreal xAcc = firstRowAnchorX + step * 0.0;
        const qreal xV3w = firstRowAnchorX + step * 1.0;
        const qreal xV1 = firstRowAnchorX + step * 2.0;
        const qreal xV2 = firstRowAnchorX + step * 3.0;
        const qreal xVReg = firstRowAnchorX + step * 4.0;
        const qreal xPs1 = (xV1 + xV2) * 0.5;
        const qreal xPs2 = (xV2 + xVReg) * 0.5;

        const qreal xFm = xVReg;
        const qreal xTestValve = xV2;
        const qreal xVBack1 = xV1;
        const qreal xVBackReg = xV3w;
        const qreal xLoop = xAcc;
        const qreal xPt1 = (xFm + xTestValve) * 0.5;
        const qreal xPt2 = (xTestValve + xVBack1) * 0.5;

        auto place = [&](QGraphicsItem *it, qreal cx, qreal cy)
        {
            if (!it)
                return;
            it->setPos(QPointF(cx, cy));
            it->setTransformOriginPoint(it->boundingRect().center());
            it->setScale(kDeviceItemScale);
            it->setZValue(2);
            m_scene->addItem(it);
        };

        // 第一排（从左到右，列 0..6）
        auto *acc = new AccumulatorItem("压力罐");
        place(acc, xAcc, yRow1);

        auto *v3w = new ThreeWayValveItem("电动三通切换阀");
        place(v3w, xV3w, yRow1 + row1AfterAccumulatorYOffset);

        auto *v1 = new ValveItem("电动阀", true, 100.0);
        place(v1, xV1, yRow1 + row1AfterAccumulatorYOffset);
        v1->setData(3, 4);
        v1->setData(4, QVariant());

        // 压力传感器上置：使底部红点与主干管道平齐。
        auto *ps1 = new SensorItem("压力传感器4", "kPa", kUiPurple);
        ps1->setData(1, 4); // 1号操作台映射：4号压力传感器
        place(ps1, xPs1, yRow1 + row1AfterAccumulatorYOffset + pressureSensorTapYOffset);

        auto *v2 = new ValveItem("电磁阀1", true, 100.0);
        place(v2, xV2, yRow1 + row1AfterAccumulatorYOffset);
        v2->setData(3, QVariant()); // 该图元改由 relay index 0 (M100.0) 驱动，不走阀门ID回读
        v2->setData(4, 0);          // 位于压力传感器4和5之间：点击直接切换电磁阀1
        v2->setFlag(QGraphicsItem::ItemIsSelectable, false);

        auto *ps2 = new SensorItem("压力传感器5", "kPa", kUiPurple);
        ps2->setData(1, 5); // 1号操作台映射：5号压力传感器
        place(ps2, xPs2, yRow1 + row1AfterAccumulatorYOffset + pressureSensorTapYOffset);

        auto *vReg = new ValveItem("电动调压阀", true, 65.0);
        place(vReg, xVReg, yRow1 + row1AfterAccumulatorYOffset);
        vReg->setData(3, 6);

        // 第二排（从右到左，列 6..0）
        auto *fm = new FlowMeterItem("流量计");
        place(fm, xFm, yRow2);

        auto *pt1 = new SensorItem("压力传感器6", "kPa", kUiOrange);
        pt1->setData(1, 6); // 1号操作台映射：6号压力传感器
        pt1->setData(2, "kPa");
        place(pt1, xPt1, yRow2 + row2AfterFlowMeterYOffset + pressureSensorTapYOffset);

        auto *testValve = new ValveItem("待测试阀", false, 0.0);
        place(testValve, xTestValve, yRow2 + row2AfterFlowMeterYOffset);
        testValve->setData(3, 7);

        auto *pt2 = new SensorItem("压力传感器7", "kPa", kUiOrange);
        pt2->setData(1, 7); // 1号操作台映射：7号压力传感器
        pt2->setData(2, "kPa");
        place(pt2, xPt2, yRow2 + row2AfterFlowMeterYOffset + pressureSensorTapYOffset);

        auto *vBack1 = new ValveItem("电动阀", true, 100.0);
        place(vBack1, xVBack1, yRow2 + row2AfterFlowMeterYOffset);
        vBack1->setData(3, 8);

        auto *vBackReg = new ValveItem("电动调压阀", true, 75.0);
        place(vBackReg, xVBackReg, yRow2 + row2AfterFlowMeterYOffset);
        vBackReg->setData(3, 9);

        auto *loopNode = new LoopItem("回路");
        place(loopNode, xLoop, yRow2);

        // ==== 管道连接（四段主流程，按工艺流向编号）====

        // 管路 1：上排供压主线
        // 路径：蓄能器 -> 电动三通切换阀 -> 电动阀 V1 -> 电动阀 V2 -> 电动调压阀。
        // 描述：该段用于建立测试介质进入测试段前的供压与切换主通道；
        //       全程沿上排主干布置，通过 connectPorts 保持横平竖直并自动端口对齐。
        connectPorts(m_scene, acc->mapToScene(AccumulatorItem::outletPortLocal()), v3w->mapToScene(ThreeWayValveItem::inletPortLocal()));
        connectPorts(m_scene, v3w->mapToScene(ThreeWayValveItem::outletPortLocal()), v1->mapToScene(ValveItem::inletPortLocal()));
        connectPorts(m_scene, v1->mapToScene(ValveItem::outletPortLocal()), v2->mapToScene(ValveItem::inletPortLocal()));
        connectPorts(m_scene, v2->mapToScene(ValveItem::outletPortLocal()), vReg->mapToScene(ValveItem::inletPortLocal()));

        // 管路 2：上排到下排的跨排过渡线
        // 路径：电动调压阀出口 ->（向右预留）->（垂直下行）-> 流量计出口侧。
        // 描述：该段负责完成上排主线到下排测试回路的“换行”连接；
        //       走线采用“水平 -> 垂直 -> 水平”折线，确保跨排连接无斜线。
        //       其中 +50 为预留水平过渡段长度，用于避免与设备本体过近。
        {
            const QPointF start = vReg->mapToScene(ValveItem::outletPortLocal());
            const QPointF end = fm->mapToScene(FlowMeterItem::outletPortLocal());
            QPainterPath path(start);
            const QPointF p1(start.x() + 100.0, start.y());
            const QPointF p2(p1.x(), end.y());
            path.lineTo(p1);
            path.lineTo(p2);
            path.lineTo(end);
            addHmiPipeWithArrow(m_scene, path, end, p2);
        }

        // 管路 3：下排测试主线
        // 路径：流量计 -> 待测试阀 -> 回路电动阀（vBack1）。
        // 描述：该段构成测试核心路径，覆盖流量计量与被测阀通路；
        //       方向与上排相反（回折方向），但仍使用同一连接规则以保持视觉一致。
        connectPorts(m_scene, fm->mapToScene(FlowMeterItem::inletPortLocal()), testValve->mapToScene(ValveItem::outletPortLocal()));
        connectPorts(m_scene, testValve->mapToScene(ValveItem::inletPortLocal()), vBack1->mapToScene(ValveItem::outletPortLocal()));

        // 管路 4：下排回路收口段
        // 路径：回路电动阀（vBack1） -> 回路电动调压阀（vBackReg） -> 回路节点（loopNode）。
        // 描述：该段用于对测试后介质进行回路调节并导入回路节点，完成流程收口；
        //       走线与管路 3 共享基准高度，便于视觉识别与后续长度微调。
        connectPorts(m_scene, vBack1->mapToScene(ValveItem::inletPortLocal()), vBackReg->mapToScene(ValveItem::outletPortLocal()));
        connectPorts(m_scene, vBackReg->mapToScene(ValveItem::inletPortLocal()), loopNode->mapToScene(LoopItem::outletPortLocal()));

        // 注：按需求不再绘制“回路 -> 蓄能器”的闭环管路。

        // 让 fitInView 在控件完成布局（viewport 有真实尺寸）后执行
        QTimer::singleShot(0, this, [this]()
                           { applyAutoFit(); });

        updateSensorValues();
    }

    void Station1Panel::applyAutoFit()
    {
        if (!m_view || !m_scene)
            return;

        const QRectF r = m_scene->sceneRect();
        if (r.isEmpty())
            return;

        m_view->fitInView(r, Qt::KeepAspectRatio);
    }

    void Station1Panel::updatePipeFlowAnimation()
    {
        if (!m_scene)
            return;

        // 与准备区联动：至少一台供压泵运行时才显示流动动画
        bool anyPumpRunning = false;
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (m_stationClient && strictRemoteMode)
        {
            anyPumpRunning = (m_stationClient->getLatestSensorData().flow_rate > 0.001f);
        }
        else if (m_deviceManager)
        {
            anyPumpRunning = m_deviceManager->getPump(1).isRunning ||
                             m_deviceManager->getPump(2).isRunning;
        }
        if (!anyPumpRunning)
            return;

        m_flowDashOffset -= 1.0;
        if (m_flowDashOffset < -10000.0)
            m_flowDashOffset = 0.0;

        const auto items = m_scene->items();
        for (auto *it : items)
        {
            if (!it || it->data(0).toString() != "hmi_pipe_flow")
                continue;

            auto *pathItem = dynamic_cast<QGraphicsPathItem *>(it);
            if (!pathItem)
                continue;

            QPen pen = pathItem->pen();
            pen.setDashOffset(m_flowDashOffset);
            pathItem->setPen(pen);
        }
    }

    void Station1Panel::updateSensorValues()
    {
        if (!m_scene)
            return;

        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (m_stationClient && strictRemoteMode)
        {
            const SensorData net = m_stationClient->getLatestSensorData();
            const auto allItems = m_scene->items();

            const std::vector<int> mappedSensorIds = {4, 5, 6, 7};
            for (size_t idx = 0; idx < mappedSensorIds.size(); ++idx)
            {
                const int sensorId = mappedSensorIds[idx];
                for (auto *it : allItems)
                {
                    if (!it)
                        continue;
                    const QVariant v = it->data(1);
                    if (!v.isValid() || v.toInt() != sensorId)
                        continue;

                    auto *sensorItem = dynamic_cast<SensorItem *>(it);
                    if (!sensorItem)
                        continue;

                    sensorItem->setValue(static_cast<double>(net.pressure[idx]));
                    sensorItem->setDisplayDecimals(2);
                }
            }

            for (auto *it : allItems)
            {
                if (!it)
                    continue;

                auto *flowItem = dynamic_cast<FlowMeterItem *>(it);
                if (!flowItem)
                    continue;

                flowItem->setFlow(static_cast<double>(net.flow_rate));
                flowItem->setUnit("L/min");
                flowItem->setAlarm(false);
                flowItem->setAlarmDetail(0, 0);
            }

            return;
        }

        if (!m_deviceManager)
            return;

        const auto allItems = m_scene->items();

        const std::vector<int> mappedSensorIds = {4, 5, 6, 7};
        for (int sensorId : mappedSensorIds)
        {
            const auto sensor = m_deviceManager->getPressureSensor(static_cast<uint16_t>(sensorId));
            for (auto *it : allItems)
            {
                if (!it)
                    continue;

                const QVariant v = it->data(1);
                if (!v.isValid() || v.toInt() != sensorId)
                    continue;

                auto *sensorItem = dynamic_cast<SensorItem *>(it);
                if (!sensorItem)
                    continue;

                sensorItem->setValue(static_cast<double>(sensor.pressure));
                sensorItem->setDisplayDecimals(sensor.displayDecimals >= 0 && sensor.displayDecimals <= 6 ? sensor.displayDecimals : 2);
            }
        }

        const std::vector<int> mappedValveIds = {4, 5, 6, 7, 8, 9};
        for (int valveId : mappedValveIds)
        {
            const auto valve = m_deviceManager->getValve(static_cast<uint16_t>(valveId));
            const bool open = (valve.status == ValveStatus::OPEN || valve.status == ValveStatus::OPENING);

            for (auto *it : allItems)
            {
                if (!it)
                    continue;

                const QVariant v = it->data(3);
                if (!v.isValid() || v.toInt() != valveId)
                    continue;

                auto *valveItem = dynamic_cast<ValveItem *>(it);
                if (!valveItem)
                    continue;

                valveItem->setOpen(open);
                valveItem->setDegree(static_cast<double>(valve.openingDegree));
            }
        }

        // 1号操作台流量计显示：当前映射到 DeviceManager 的 flow meter 1
        const auto flowMeter = m_deviceManager->getFlowMeter(1);
        const bool hasFlowAlarm = (flowMeter.emptyPipeAlarm != 0 || flowMeter.excitationAlarm != 0);

        for (auto *it : allItems)
        {
            if (!it)
                continue;

            auto *flowItem = dynamic_cast<FlowMeterItem *>(it);
            if (!flowItem)
                continue;

            flowItem->setFlow(static_cast<double>(flowMeter.flowRate));
            flowItem->setUnit((!flowMeter.unitLabel.empty() && flowMeter.unitLabel != "unknown")
                                  ? QString::fromStdString(flowMeter.unitLabel)
                                  : QString("L/min"));
            flowItem->setAlarm(hasFlowAlarm);
            flowItem->setAlarmDetail(static_cast<int>(flowMeter.emptyPipeAlarm),
                                     static_cast<int>(flowMeter.excitationAlarm));
        }
    }

} // namespace WaterTest
