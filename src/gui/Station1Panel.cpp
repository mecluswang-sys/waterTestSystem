/**
 * @file Station1Panel.cpp
 * @brief 1号操作台面板实现（流程图展示）
 */

#include "gui/Station1Panel.h"
#include "gui/ContainerGlyphRenderer.h"
#include "gui/HmiGlyphTheme.h"
#include "gui/HmiGlyphThemeUtils.h"
#include "gui/InstrumentGlyphRenderer.h"
#include "gui/PipeGlyphRenderer.h"
#include "gui/ProcessValveGlyphRenderer.h"
#include "gui/SensorGlyphRenderer.h"
#include "gui/ValveGlyphRenderer.h"

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
#include <QScrollBar>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QMessageBox>
#include <QStyle>
#include <QDebug>
#include <QDateTime>
#include <QEvent>
#include <QEventLoop>
#include <QMouseEvent>
#include <QtMath>
#include <QDir>
#include <vector>
#include <array>

namespace WaterTest
{
    namespace
    {
        constexpr qreal kDeviceItemScale = 1.45;
        constexpr qreal kAccumulatorItemScale = 1.2;
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

        static bool isM100RelayIndex(uint8_t index)
        {
            return index <= 3;
        }

        // 1号操作台 DQ 输出匹配表
        // index0~3: 关键阀门映射到 M100.0~M100.3；其余沿用 Q0.4-Q1.1
        static const std::array<RelayDef, 10> kStation1Relays{{
            {0,  "电磁阀1",  "M100.0", "valve"},
            {1,  "电磁阀2",  "M100.1", "valve"},
            {2,  "电磁阀3",  "M100.2", "valve"},
            {3,  "电磁阀4",  "M100.3", "valve"},
            {4,  "调压阀电磁阀", "Q0.4", "valve"},
            {5,  "回流阀电磁阀", "Q0.5", "valve"},
            {6,  "备用继电器",  "Q0.6", "valve"},
            {7,  "备用继电器",  "Q0.7", "valve"},
            {8,  "供压泵1启停",  "Q1.0", "pump"},
            {9,  "供压泵2启停",  "Q1.1", "pump"},
        }};

        // 2号操作台继电器映射（独立于1号）
        static const std::array<RelayDef, 6> kStation2Relays{{
            {4, "电磁阀3", "Q0.4", "valve"},
            {5, "电磁阀4", "Q0.5", "valve"},
            {6, "电磁阀5", "Q0.6", "valve"},
            {7, "电磁阀6", "Q0.7", "valve"},
            {8, "预留继电器", "Q1.0", "valve"},
            {9, "供压泵2启停", "Q1.1", "pump"},
        }};

        // 3号操作台继电器映射（独立于1号）
        static const std::array<RelayDef, 5> kStation3Relays{{
            {6,  "电磁阀5", "Q0.6", "valve"},
            {7,  "电磁阀6", "Q0.7", "valve"},
            {8,  "电磁阀7", "Q1.0", "valve"},
            {9,  "电磁阀8", "Q1.1", "valve"},
            {10, "供压泵3启停", "Q1.2", "pump"},
        }};

        static std::vector<RelayDef> relayDefsForStation(int stationNumber)
        {
            if (stationNumber == 2)
                return std::vector<RelayDef>(kStation2Relays.begin(), kStation2Relays.end());
            if (stationNumber == 3)
                return std::vector<RelayDef>(kStation3Relays.begin(), kStation3Relays.end());
            return std::vector<RelayDef>(kStation1Relays.begin(), kStation1Relays.end());
        }

        static const RelayDef *findRelayDefByIndex(const std::vector<RelayDef> &defs, uint8_t index)
        {
            for (const auto &def : defs)
            {
                if (def.index == index)
                    return &def;
            }
            return nullptr;
        }

        struct StationRelayGlyphMap
        {
            uint8_t v1;
            uint8_t v2;
            uint8_t v3;
            uint8_t test;
        };

        static StationRelayGlyphMap relayGlyphMapForStation(int stationNumber)
        {
            if (stationNumber == 2)
                return {4, 5, 6, 7};
            if (stationNumber == 3)
                return {6, 7, 8, 9};
            return {0, 1, 3, 2};
        }

        static QString formatRelayBtnText(const QString &addr, const QString &label, bool on)
        {
            const QString stateText = on ? "● 通/得电" : "○ 断/失电";
            return QString("%1\n%2\n%3")
                .arg(addr, label, stateText);
        }

        static int pressureDisplayDecimals(const PressureSensor &sensor, int fallbackDecimals = 1)
        {
            if (sensor.displayDecimals >= 0 && sensor.displayDecimals <= 6)
                return sensor.displayDecimals;
            return fallbackDecimals;
        }

        constexpr double kKPaPerKgfCm2 = 98.0665;

        static double kPaToKgfCm2(double kpa)
        {
            return kpa / kKPaPerKgfCm2;
        }

        static QString fmtPressure(double kpa, int decimals = 1)
        {
            return QString::number(kPaToKgfCm2(kpa), 'f', decimals);
        }

        static QString fmtKPa(const PressureSensor &sensor, int fallbackDecimals = 1)
        {
            return QString::number(kPaToKgfCm2(sensor.pressure), 'f', pressureDisplayDecimals(sensor, fallbackDecimals)) + " kgf/cm^2";
        }

        static int resolveRemotePressureIndex(uint16_t sensorId, int fallbackIndex)
        {
            auto &cfg = ConfigManager::getInstance();

            const std::string prefix = "station.pressure_sensor." + std::to_string(sensorId) + ".";
            const int configuredIndex = cfg.getInt(prefix + "remote_index", -1);
            if (configuredIndex >= 0 && configuredIndex <= 3)
                return configuredIndex;

            const int modbusAddress = cfg.getInt(prefix + "modbus_address", -1);
            if (modbusAddress >= 1 && modbusAddress <= 4)
                return modbusAddress - 1;

            if (fallbackIndex < 0)
                return 0;
            if (fallbackIndex > 3)
                return 3;
            return fallbackIndex;
        }

        static QString systemModeToText(SystemMode mode)
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
        }

        static QString deviceStatusToText(DeviceStatus status)
        {
            switch (status)
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
        }

        static QString selfCheckCell(bool ok)
        {
            const QString color = ok ? "#3fb950" : "#f85149";
            const QString text = ok ? "OK" : "NG";
            return QString("<span style='color:%1;font-weight:800'>%2</span>").arg(color, text);
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

        static GuiGlyph::HmiGlyphTheme makeGlyphTheme()
        {
            return GuiGlyph::makeHmiGlyphTheme(
                kUiShadow,
                kUiPanel,
                kUiBody,
                kUiBorder,
                kUiBorderWeak,
                kUiCyan,
                kUiText,
                kUiTextDim,
                kUiTextMuted,
                kUiInk,
                kUiGreen,
                kUiRed,
                kUiOrange,
                kUiPurple,
                kUiMetalDark,
                kUiMetalMid,
                QColor());
        }

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

            GuiGlyph::addDynamicPipeToScene(scene,
                                            path,
                                            0.0,
                                            kUiPipeOuter,
                                            kUiPipeInner,
                                            makeGlyphTheme(),
                                            kPipeOuterWidth,
                                            kPipeInnerWidth,
                                            kPipeFlowWidth,
                                            1);

            Q_UNUSED(arrowTip);
            Q_UNUSED(arrowFrom);
        }

        static void addHmiPipeWithArrowCustom(QGraphicsScene *scene,
                                              const QPainterPath &path,
                                              const QPointF &arrowTip,
                                              const QPointF &arrowFrom,
                                              qreal outerWidth,
                                              qreal innerWidth,
                                              qreal flowWidth)
        {
            ensureUiTokensInitialized();

            GuiGlyph::addDynamicPipeToScene(scene,
                                            path,
                                            0.0,
                                            kUiPipeOuter,
                                            kUiPipeInner,
                                            makeGlyphTheme(),
                                            outerWidth,
                                            innerWidth,
                                            flowWidth,
                                            1);

            Q_UNUSED(arrowTip);
            Q_UNUSED(arrowFrom);
        }

        static void addDebugPointMarker(QGraphicsScene *scene,
                                        const QPointF &point,
                                        const QString &label,
                                        const QColor &color = QColor("#ff4d4f"))
        {
            if (!scene)
                return;

            QPen pen(color, 2.0);
            QBrush brush(color);
            auto *dot = scene->addEllipse(point.x() - 5.0, point.y() - 5.0, 10.0, 10.0, pen, brush);
            dot->setZValue(20);
            dot->setData(0, "debug_marker");

            auto *text = scene->addText(label);
            text->setDefaultTextColor(color);
            QFont font = text->font();
            font.setPointSize(9);
            font.setBold(true);
            text->setFont(font);
            text->setPos(point + QPointF(8.0, -20.0));
            text->setZValue(20);
            text->setData(0, "debug_marker");
        }

        // ========== Station1 的图标化拟物设备图元（与 PreparationPanel 同风格） ==========
        class AccumulatorItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return GuiGlyph::accumulatorInletPortLocal(); }
            static QPointF outletPortLocal() { return GuiGlyph::accumulatorOutletPortLocal(); }
            static QPointF returnPortLocal() { return GuiGlyph::accumulatorReturnPortLocal(); }

            explicit AccumulatorItem(const QString &name, bool showReturnPort = true)
                : m_name(name), m_showReturnPort(showReturnPort)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return GuiGlyph::accumulatorBoundingRect(); }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                GuiGlyph::drawAccumulatorGlyph(p, boundingRect(), m_name, isSelected(), makeGlyphTheme(), m_showReturnPort);
            }

        private:
            QString m_name;
            bool m_showReturnPort = true;
        };

        class ValveItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return GuiGlyph::valveInletPortLocal(); }
            static QPointF outletPortLocal() { return GuiGlyph::valveOutletPortLocal(); }

            explicit ValveItem(const QString &name,
                               bool open = true,
                               double degree = 100.0,
                               bool regulatingStyle = false,
                               bool plainRegulatingStyle = false)
                : m_name(name),
                  m_open(open),
                  m_degree(degree),
                  m_regulatingStyle(regulatingStyle),
                  m_plainRegulatingStyle(plainRegulatingStyle)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override
            {
                // 调压阀包含上方仪表与下方文字，使用更大的包围框避免边缘裁切。
                if (m_regulatingStyle)
                    return QRectF(-55, -55, 114, 122);
                return QRectF(-50, -48, 104, 108);
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

            const QString &getName() const { return m_name; }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                GuiGlyph::drawValveGlyph(
                    p,
                    boundingRect(),
                    m_name,
                    m_open,
                    m_degree,
                    isSelected(),
                    makeGlyphTheme(),
                    m_regulatingStyle,
                    m_plainRegulatingStyle);
            }

        private:
            QString m_name;
            bool m_open;
            double m_degree;
            bool m_regulatingStyle = false;
            bool m_plainRegulatingStyle = false;
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
            static QPointF inletPortLocal() { return GuiGlyph::threeWayValveInletPortLocal(); }
            static QPointF outletPortLocal() { return GuiGlyph::threeWayValveOutletPortLocal(); }
            static QPointF branchPortLocal() { return GuiGlyph::threeWayValveBranchPortLocal(); }

            explicit ThreeWayValveItem(const QString &name)
                : m_name(name)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return GuiGlyph::threeWayValveBoundingRect(); }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                GuiGlyph::drawThreeWayValveGlyph(p, boundingRect(), m_name, isSelected(), makeGlyphTheme());
            }

        private:
            QString m_name;
        };

        class SensorItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return GuiGlyph::sensorInletPortLocal(); }
            static QPointF outletPortLocal() { return GuiGlyph::sensorOutletPortLocal(); }

            explicit SensorItem(const QString &name, const QString &unit = "kgf/cm^2", QColor typeColor = QColor())
                : m_name(name), m_value(0.0), m_unit(unit), m_typeColor(typeColor.isValid() ? typeColor : kUiPurple)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return GuiGlyph::sensorBoundingRect(); }

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
                GuiGlyph::drawSensorGlyph(
                    p,
                    boundingRect(),
                    m_name,
                    m_value,
                    m_displayDecimals,
                    m_unit,
                    m_typeColor,
                    isSelected(),
                    true,
                        makeGlyphTheme());
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
            static QPointF inletPortLocal() { return GuiGlyph::flowMeterInletPortLocal(); }
            static QPointF outletPortLocal() { return GuiGlyph::flowMeterOutletPortLocal(); }

            explicit FlowMeterItem(const QString &name)
                : m_name(name), m_flow(0.0), m_unit("L/min"), m_hasAlarm(false), m_emptyPipeAlarm(0), m_excitationAlarm(0)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return GuiGlyph::flowMeterBoundingRect(); }

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
                GuiGlyph::drawFlowMeterGlyph(
                    p,
                    boundingRect(),
                    m_name,
                    m_flow,
                    m_unit,
                    m_hasAlarm,
                    m_emptyPipeAlarm,
                    m_excitationAlarm,
                    isSelected(),
                    makeGlyphTheme());
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
            static QPointF inletPortLocal() { return GuiGlyph::loopInletPortLocal(); }
            static QPointF outletPortLocal() { return GuiGlyph::loopOutletPortLocal(); }
            static QPointF bottomPortLocal() { return GuiGlyph::loopBottomPortLocal(); }

            explicit LoopItem(const QString &name)
                : m_name(name)
            {
                setCacheMode(DeviceCoordinateCache);
                setFlags(QGraphicsItem::ItemIsSelectable);
            }

            QRectF boundingRect() const override { return GuiGlyph::loopBoundingRect(); }

            void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override
            {
                GuiGlyph::drawLoopGlyph(p, boundingRect(), m_name, isSelected(), makeGlyphTheme());
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

    Station1Panel::Station1Panel(std::shared_ptr<DeviceManager> deviceManager,
                                 QWidget *parent,
                                 const PanelConfig &panelConfig)
        : QWidget(parent),
          m_deviceManager(std::move(deviceManager)),
          m_stationClient(nullptr),
          m_selfCheckBtn(nullptr),
          m_startBtn(nullptr),
          m_stopBtn(nullptr),
          m_panelConfig(panelConfig),
          m_view(nullptr),
          m_scene(nullptr),
          m_flowTimer(nullptr),
          m_dataTimer(nullptr),
          m_relayTimer(nullptr),
          m_flowDashOffset(0.0)
    {
        setupUI();
    }

    Station1Panel::~Station1Panel() = default;

    void Station1Panel::setStationClient(std::shared_ptr<StationClient> stationClient)
    {
        m_stationClient = stationClient;
    }

    void Station1Panel::syncVisualStateOnce()
    {
        updateRelayButtons(true);
        updateSensorValues(true);
    }

    void Station1Panel::resizeEvent(QResizeEvent *event)
    {
        QWidget::resizeEvent(event);
        applyAutoFit();
    }

    void Station1Panel::showEvent(QShowEvent *event)
    {
        QWidget::showEvent(event);
        setRealtimeUpdatesEnabled(true);
        // 关键：首次显示时布局刚完成，确保用最终 viewport 尺寸做 fitInView
        QTimer::singleShot(0, this, [this]()
                           { applyAutoFit(); });
    }

    void Station1Panel::hideEvent(QHideEvent *event)
    {
        QWidget::hideEvent(event);
        setRealtimeUpdatesEnabled(false);
    }

    void Station1Panel::setupUI()
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(12, 10, 12, 12);
        layout->setSpacing(8);

        auto *toolbarLayout = new QHBoxLayout();
        toolbarLayout->setContentsMargins(0, 0, 0, 0);
        toolbarLayout->setSpacing(8);
        toolbarLayout->addStretch();

        m_startBtn = new QPushButton("开始", this);
        m_startBtn->setMinimumHeight(34);
        m_startBtn->setProperty("tone", "accent");
        connect(m_startBtn, &QPushButton::clicked, this, [this]() {
            onStartButtonClicked("ui");
        });
        toolbarLayout->addWidget(m_startBtn);

        m_stopBtn = new QPushButton("停止", this);
        m_stopBtn->setMinimumHeight(34);
        m_stopBtn->setProperty("tone", "bad");
        connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
            onStopButtonClicked("ui");
        });
        toolbarLayout->addWidget(m_stopBtn);

        m_selfCheckBtn = new QPushButton("系统自检", this);
        m_selfCheckBtn->setMinimumHeight(34);
        m_selfCheckBtn->setProperty("tone", "accent");
        connect(m_selfCheckBtn, &QPushButton::clicked, this, &Station1Panel::onSelfCheck);
        toolbarLayout->addWidget(m_selfCheckBtn);

        layout->addLayout(toolbarLayout);

        m_view = new QGraphicsView(this);
        m_view->setFrameShape(QFrame::NoFrame);
        m_view->setRenderHint(QPainter::Antialiasing, true);
        m_view->setRenderHint(QPainter::TextAntialiasing, true);
        m_view->setStyleSheet("background: transparent;");
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
        connect(m_dataTimer, &QTimer::timeout, this, [this]() { updateSensorValues(); });
        m_dataTimer->start(200);

        // 继电器状态刷新：1 秒一次（降低无意义的 Q 区轮询频率）
        m_relayTimer = new QTimer(this);
        connect(m_relayTimer, &QTimer::timeout, this, [this]() { updateRelayButtons(); });
        m_relayTimer->start(1000);

        buildScene();
        updateRelayButtons();

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

            const QVariant regIdVar = item->data(6);
            if (regIdVar.isValid())
            {
                const int regulatingValveId = regIdVar.toInt();
                float currentOpening = 0.0f;
                if (m_deviceManager)
                {
                    const auto rv = m_deviceManager->getRegulatingValve(static_cast<uint16_t>(regulatingValveId));
                    currentOpening = qBound(0.0f, rv.openingPercent, 100.0f);
                }

                QDialog dialog(this);
                dialog.setWindowTitle("调压阀开度控制");
                dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

                auto *mainLayout = new QVBoxLayout(&dialog);
                auto *infoLabel = new QLabel(QString("%1\n当前开度: %2%")
                                                 .arg(valveName)
                                                 .arg(QString::number(currentOpening, 'f', 1)),
                                             &dialog);
                mainLayout->addWidget(infoLabel);

                auto *slider = new QSlider(Qt::Horizontal, &dialog);
                slider->setRange(0, 100);
                slider->setValue(qRound(currentOpening));
                mainLayout->addWidget(slider);

                auto *spin = new QDoubleSpinBox(&dialog);
                spin->setRange(0.0, 100.0);
                spin->setDecimals(1);
                spin->setSingleStep(1.0);
                spin->setValue(static_cast<double>(currentOpening));
                mainLayout->addWidget(spin);

                connect(slider, &QSlider::valueChanged, &dialog, [spin](int value) {
                    if (qRound(spin->value()) != value)
                        spin->setValue(static_cast<double>(value));
                });
                connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, [slider](double value) {
                    const int iv = qRound(value);
                    if (slider->value() != iv)
                        slider->setValue(iv);
                });

                auto *buttons = new QHBoxLayout();
                auto *applyBtn = new QPushButton("设定开度", &dialog);
                auto *cancelBtn = new QPushButton("取消", &dialog);
                buttons->addWidget(applyBtn);
                buttons->addWidget(cancelBtn);
                mainLayout->addLayout(buttons);

                connect(applyBtn, &QPushButton::clicked, &dialog, [&, regulatingValveId]() {
                    const bool strictMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
                    if (m_stationClient && strictMode)
                    {
                        QMessageBox::information(this,
                                                 "暂不支持",
                                                 "当前远程模式仅支持开关阀命令，调压阀开度设定请在主控端执行，或关闭 strict_remote_mode 后本地下发。");
                        return;
                    }

                    bool ok = false;
                    if (m_deviceManager)
                    {
                        ok = m_deviceManager->setValveOpeningPercent(
                            static_cast<uint16_t>(regulatingValveId),
                            static_cast<float>(spin->value()));
                    }

                    if (!ok)
                    {
                        QMessageBox::warning(this, "操作失败", "开度设定下发失败，请检查 PLC/主控连接状态。");
                        return;
                    }

                    updateSensorValues(true);
                    dialog.accept();
                });
                connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

                dialog.exec();
                return;
            }

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

        // 初始状态仅当前可见页面保持实时刷新，降低Tab切换负载。
        setRealtimeUpdatesEnabled(isVisible());
    }

    void Station1Panel::setRealtimeUpdatesEnabled(bool enabled)
    {
        if (m_flowTimer)
        {
            if (enabled)
                m_flowTimer->start(50);
            else
                m_flowTimer->stop();
        }

        if (m_dataTimer)
        {
            if (enabled)
                m_dataTimer->start(200);
            else
                m_dataTimer->stop();
        }

        if (m_relayTimer)
        {
            if (enabled)
                m_relayTimer->start(1000);
            else
                m_relayTimer->stop();
        }
    }

    void Station1Panel::buildRelayPanel(QWidget *container)
    {
        const auto relayDefs = relayDefsForStation(m_panelConfig.stationNumber);

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
        uint8_t maxRelayIndex = 0;
        for (const auto &def : relayDefs)
            maxRelayIndex = std::max(maxRelayIndex, def.index);
        m_relayBtns.resize(static_cast<size_t>(maxRelayIndex) + 1, nullptr);

        int visibleBtnCount = 0;

        for (const auto &def : relayDefs)
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

            // 1号台保持 relay0 仅图元控制；2/3号台全部显示。
            if (m_panelConfig.stationNumber == 1 && def.index == 0)
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

    void Station1Panel::onSelfCheck()
    {
        if (!m_deviceManager)
        {
            QMessageBox::warning(this, "系统自检", "设备管理器未初始化（请先连接系统）");
            return;
        }

        const bool pumpManualForLeakTest = ConfigManager::getInstance().getBool("selfcheck.pump_manual_for_leak_test", true);
        const int testDn = ConfigManager::getInstance().getInt("selfcheck.station1.test_dn", 50);

        enum class SelfCheckFlowState {
            IDLE,
            REFRESH,
            STATIC_CHECK,
            STEP1_LINKAGE,
            STEP2_PREPARE,
            STEP3_OBSERVE,
            STEP4_CONCLUSION,
            VALVE_ACTION,
            FINISH,
            FAULT_STOP
        };
        auto stateToText = [](SelfCheckFlowState s) -> const char * {
            switch (s) {
            case SelfCheckFlowState::IDLE: return "IDLE";
            case SelfCheckFlowState::REFRESH: return "REFRESH";
            case SelfCheckFlowState::STATIC_CHECK: return "STATIC_CHECK";
            case SelfCheckFlowState::STEP1_LINKAGE: return "STEP1_LINKAGE";
            case SelfCheckFlowState::STEP2_PREPARE: return "STEP2_PREPARE";
            case SelfCheckFlowState::STEP3_OBSERVE: return "STEP3_OBSERVE";
            case SelfCheckFlowState::STEP4_CONCLUSION: return "STEP4_CONCLUSION";
            case SelfCheckFlowState::VALVE_ACTION: return "VALVE_ACTION";
            case SelfCheckFlowState::FINISH: return "FINISH";
            case SelfCheckFlowState::FAULT_STOP: return "FAULT_STOP";
            }
            return "UNKNOWN";
        };
        SelfCheckFlowState flowState = SelfCheckFlowState::IDLE;
        auto transitionTo = [&](SelfCheckFlowState newState, const QString &reason) {
            if (flowState == newState)
                return;
            qInfo() << "[Station1Panel][SelfCheck] state"
                    << stateToText(flowState) << "->" << stateToText(newState)
                    << "reason=" << reason;
            flowState = newState;
        };

        auto dnBandValue = [&](float small, float medium, float large) -> float {
            if (testDn <= 50)
                return small;
            if (testDn <= 100)
                return medium;
            return large;
        };

        // ===== 创建动态进度弹窗 =====
        enum StepState { PENDING = 0, RUNNING, STEP_OK, STEP_FAIL };
        struct StepItem { QString name; StepState state; QString detail; };
        std::vector<StepItem> steps = {
            {QString::fromUtf8("数据初始化刷新"),         PENDING, QString()},
            {QString::fromUtf8("压力 PS4"),         PENDING, QString()},
            {QString::fromUtf8("压力 PS5"),         PENDING, QString()},
            {QString::fromUtf8("压力 PS6"),         PENDING, QString()},
            {QString::fromUtf8("压力 PS7"),         PENDING, QString()},
            {QString::fromUtf8("流量计 FM1"),             PENDING, QString()},
            {QString::fromUtf8("电磁阀1 (M100.0)"),                PENDING, QString()},
            {QString::fromUtf8("电磁阀2 (M100.1)"),                PENDING, QString()},
            {QString::fromUtf8("电磁阀4 (M100.3)"),                PENDING, QString()},
            {QString::fromUtf8("电动调压阀1"),                     PENDING, QString()},
            {QString::fromUtf8("步骤1：电磁阀1/2/4 三阀联动测试"), PENDING, QString()},
            {QString::fromUtf8("步骤2：气泵/建压准备"),            PENDING, QString()},
            {QString::fromUtf8("步骤3：压力观察 (PS5)"),           PENDING, QString()},
            {QString::fromUtf8("步骤4：泄漏结论"),                 PENDING, QString()},
            {QString::fromUtf8("电磁阀2 动作测试"),                PENDING, QString()},
            {QString::fromUtf8("电磁阀3 动作测试"),                PENDING, QString()},
            {QString::fromUtf8("电磁阀4 动作测试"),                PENDING, QString()},
        };
        const int IDX_REFRESH = 0, IDX_PS4 = 1, IDX_PS5 = 2, IDX_PS6 = 3, IDX_PS7 = 4;
        const int IDX_FM1 = 5, IDX_V1 = 6, IDX_V2 = 7, IDX_V4 = 8, IDX_VREG = 9;
        const int IDX_STEP1 = 10, IDX_STEP2 = 11, IDX_STEP3 = 12, IDX_STEP4 = 13;
        const int IDX_VA2 = 14, IDX_VA3 = 15, IDX_VA4 = 16;

        auto buildHtml = [&]() -> QString {
            QString rows;
            for (const auto &s : steps) {
                QString icon, color;
                switch (s.state) {
                case PENDING:   icon = QString::fromUtf8("○"); color = QStringLiteral("#9da7b3"); break;
                case RUNNING:   icon = QString::fromUtf8("⋯"); color = QStringLiteral("#f0c040"); break;
                case STEP_OK:   icon = QString::fromUtf8("✓"); color = QStringLiteral("#3fb950"); break;
                case STEP_FAIL: icon = QString::fromUtf8("✗"); color = QStringLiteral("#f85149"); break;
                }
                rows += QString(
                    "<tr><td style='padding:5px 10px'>%1</td>"
                    "<td style='padding:5px 10px;text-align:center'>"
                    "<span style='color:%2;font-weight:800'>%3</span></td>"
                    "<td style='padding:5px 10px;color:#c9d6e2'>%4</td></tr>")
                    .arg(s.name.toHtmlEscaped(), color, icon, s.detail.toHtmlEscaped());
            }
            return QStringLiteral(
                "<h3 style='margin:0 0 8px 0'>") + QString::fromUtf8("1号操作台系统自检") +
                QStringLiteral("</h3>"
                "<table style='border-collapse:collapse;border:1px solid #223244;width:100%' border='1'>"
                "<tr style='background:#0f1a24'><th style='padding:6px 10px'>") +
                QString::fromUtf8("检查项目") +
                QStringLiteral("</th><th style='padding:6px 10px;width:60px'>") +
                QString::fromUtf8("状态") +
                QStringLiteral("</th><th style='padding:6px 10px'>") +
                QString::fromUtf8("详情") +
                QStringLiteral("</th></tr>") +
                rows + QStringLiteral("</table>");
        };

        QDialog *liveDlg = new QDialog(this, Qt::Window);
        liveDlg->setWindowTitle(QString::fromUtf8("系统自检进度"));
        liveDlg->setMinimumSize(720, 480);
        liveDlg->setAttribute(Qt::WA_DeleteOnClose);
        auto *dlgLayout = new QVBoxLayout(liveDlg);
        auto *textEdit = new QTextEdit(liveDlg);
        textEdit->setReadOnly(true);
        textEdit->setHtml(buildHtml());
        dlgLayout->addWidget(textEdit, 1);
        auto *closeBtn = new QPushButton(QString::fromUtf8("检测中，请稍候…"), liveDlg);
        closeBtn->setEnabled(false);
        connect(closeBtn, &QPushButton::clicked, liveDlg, &QDialog::close);
        dlgLayout->addWidget(closeBtn);
        liveDlg->setLayout(dlgLayout);
        liveDlg->show();
        QCoreApplication::processEvents();

        auto setStep = [&](int idx, StepState state, const QString &detail) {
            steps[static_cast<size_t>(idx)].state = state;
            steps[static_cast<size_t>(idx)].detail = detail;
            textEdit->setHtml(buildHtml());
            textEdit->verticalScrollBar()->setValue(textEdit->verticalScrollBar()->maximum());
            QCoreApplication::processEvents();
        };

        // 防止检测期间重复触发
        if (m_selfCheckBtn)
            m_selfCheckBtn->setEnabled(false);

        // ===== 数据初始化刷新 =====
        transitionTo(SelfCheckFlowState::REFRESH, QString::fromUtf8("进入数据刷新阶段"));
        setStep(IDX_REFRESH, RUNNING, QString::fromUtf8("正在刷新设备数据…"));
        const bool refreshOk = m_deviceManager->updateAllDevices();
        setStep(IDX_REFRESH, refreshOk ? STEP_OK : STEP_FAIL,
                refreshOk ? QString::fromUtf8("刷新成功") : QString::fromUtf8("刷新失败（可能影响后续结果）"));

        // ===== 传感器状态检查 =====
        transitionTo(SelfCheckFlowState::STATIC_CHECK, QString::fromUtf8("进入静态检查阶段"));
        auto checkPressureSensor = [&](int stepIdx, int sensorId) -> bool {
            setStep(stepIdx, RUNNING, QString::fromUtf8("检测中…"));
            const auto ps = m_deviceManager->getPressureSensor(sensorId);
            const bool ok = (ps.id != 0 && ps.status == DeviceStatus::ONLINE);
            setStep(stepIdx, ok ? STEP_OK : STEP_FAIL,
                    ok ? QString("%1, %2").arg(deviceStatusToText(ps.status)).arg(fmtKPa(ps))
                       : QString::fromUtf8("离线或未配置"));
            return ok;
        };
        const bool p4Ok = checkPressureSensor(IDX_PS4, 4);
        const bool p5Ok = checkPressureSensor(IDX_PS5, 5);
        checkPressureSensor(IDX_PS6, 6);
        checkPressureSensor(IDX_PS7, 7);

        // ===== 流量计状态检查 =====
        {
            setStep(IDX_FM1, RUNNING, QString::fromUtf8("检测中…"));
            const auto fm1 = m_deviceManager->getFlowMeter(1);
            const bool ok = (fm1.id != 0 && fm1.status == DeviceStatus::ONLINE);
            setStep(IDX_FM1, ok ? STEP_OK : STEP_FAIL,
                    ok ? QString::fromUtf8("在线, %1 m\u00b3/h").arg(QString::number(fm1.flowRate, 'f', 3))
                       : QString::fromUtf8("离线或未配置"));
        }

        // ===== 阀门状态检查 =====
        auto valveStatusStr = [](ValveStatus vs) -> QString {
            switch (vs) {
            case ValveStatus::OPEN:    return QString::fromUtf8("开启");
            case ValveStatus::CLOSED:  return QString::fromUtf8("关闭");
            case ValveStatus::OPENING: return QString::fromUtf8("开启中");
            case ValveStatus::CLOSING: return QString::fromUtf8("关闭中");
            default:                   return QString::fromUtf8("故障");
            }
        };
        {
            setStep(IDX_V1, RUNNING, QString::fromUtf8("检测中…"));
            const auto v1 = m_deviceManager->getValve(1);
            const bool ok = (v1.id != 0);
            setStep(IDX_V1, ok ? STEP_OK : STEP_FAIL,
                    ok ? QString::fromUtf8("配置正常, 当前状态: %1").arg(valveStatusStr(v1.status))
                       : QString::fromUtf8("未配置"));
        }
        {
            setStep(IDX_V2, RUNNING, QString::fromUtf8("检测中…"));
            const auto v2 = m_deviceManager->getValve(2);
            const bool ok = (v2.id != 0);
            setStep(IDX_V2, ok ? STEP_OK : STEP_FAIL,
                    ok ? QString::fromUtf8("配置正常, 当前状态: %1").arg(valveStatusStr(v2.status))
                       : QString::fromUtf8("未配置"));
        }
        {
            setStep(IDX_V4, RUNNING, QString::fromUtf8("检测中…"));
            const auto v4 = m_deviceManager->getValve(4);
            const bool v4ok = (v4.id != 0);
            setStep(IDX_V4, v4ok ? STEP_OK : STEP_FAIL,
                    v4ok ? QString::fromUtf8("配置正常, 当前状态: %1").arg(valveStatusStr(v4.status))
                         : QString::fromUtf8("未配置"));
        }
        {
            setStep(IDX_VREG, RUNNING, QString::fromUtf8("检测中…"));
            const auto vreg = m_deviceManager->getRegulatingValve(1);
            const bool ok = (vreg.id != 0 && vreg.deviceStatus == DeviceStatus::ONLINE);
            setStep(IDX_VREG, ok ? STEP_OK : STEP_FAIL,
                      ok ? QString::fromUtf8("在线, 目标: %1 kgf/cm^2, 实际: %2 kgf/cm^2, 开度: %3%")
                             .arg(fmtPressure(vreg.setPressure, 1))
                             .arg(fmtPressure(vreg.actualPressure, 1))
                             .arg(vreg.openingPercent, 0, 'f', 0)
                       : QString::fromUtf8("离线或未配置"));
        }

        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        auto controlRelay = [&](uint8_t index, bool on) -> bool {
            if (m_stationClient && strictRemoteMode) {
                ControlCommand cmd;
                cmd.command_type = 0;
                cmd.index = index;
                cmd.action = on ? 1 : 0;
                return m_stationClient->sendCommand(cmd);
            }
            return m_deviceManager->setRelay(index, on);
        };
        auto controlPump = [&](uint8_t index, bool on) -> bool {
            if (m_stationClient && strictRemoteMode) {
                ControlCommand cmd;
                cmd.command_type = 1;
                cmd.index = index;
                cmd.action = on ? 1 : 0;
                return m_stationClient->sendCommand(cmd);
            }
            return m_deviceManager->controlPump(static_cast<uint16_t>(index + 1), on);
        };
        auto waitMs = [](int delayMs) {
            QEventLoop waitLoop;
            QTimer::singleShot(delayMs, &waitLoop, &QEventLoop::quit);
            waitLoop.exec();
        };

        // ===== 步骤1：电磁阀1/2/4 三阀联动测试 =====
        // 同时打开电磁阀1（M100.0）/电磁阀2（M100.1）/电磁阀4（M100.3），观察 PS4 变化，验证三阀通路能力。
        transitionTo(SelfCheckFlowState::STEP1_LINKAGE, QString::fromUtf8("进入步骤1三阀联动"));
        const float linkagePressureThr = ConfigManager::getInstance().getFloat(
            "selfcheck.valve2_linkage_min_delta_kpa",
            dnBandValue(8.0f, 12.0f, 18.0f));
        const int linkageWaitMs = ConfigManager::getInstance().getInt("selfcheck.valve2_linkage_wait_ms", 3000);
        bool v1WasOn = false;
        bool v2WasOnStep1 = false;
        bool v4WasOn = false;
        const bool v1StateOk      = m_deviceManager->getRelayState(0, v1WasOn);
        const bool v2Step1StateOk = m_deviceManager->getRelayState(1, v2WasOnStep1);
        const bool v4StateOk      = m_deviceManager->getRelayState(3, v4WasOn);
        if (p4Ok && v1StateOk && v2Step1StateOk && v4StateOk) {
            setStep(IDX_STEP1, RUNNING, QString::fromUtf8("正在打开电磁阀1/2/4…"));
            const float ps4Before = m_deviceManager->getPressureSensor(4).pressure;
            const bool openOk = controlRelay(0, true) && controlRelay(1, true) && controlRelay(3, true);
            if (openOk) {
                setStep(IDX_STEP1, RUNNING, QString::fromUtf8("三阀已开，等待 %1 ms 观察 PS4 变化…").arg(linkageWaitMs));
                waitMs(linkageWaitMs);
                m_deviceManager->updateAllDevices();
                const float ps4After = m_deviceManager->getPressureSensor(4).pressure;
                const float delta = std::abs(ps4After - ps4Before);
                const bool linkageOk = (delta >= linkagePressureThr);
                setStep(IDX_STEP1, STEP_OK,
                        linkageOk
                                ? QString::fromUtf8("通路正常（PS4 变化 %1 kgf/cm^2 \u2265 阈值 %2 kgf/cm^2）")
                                    .arg(fmtPressure(delta, 2)).arg(fmtPressure(linkagePressureThr, 2))
                                : QString::fromUtf8("通路可能异常（PS4 变化 %1 kgf/cm^2，阈值 %2 kgf/cm^2）")
                                    .arg(fmtPressure(delta, 2)).arg(fmtPressure(linkagePressureThr, 2)));
            } else {
                setStep(IDX_STEP1, STEP_FAIL, QString::fromUtf8("电磁阀控制失败（1/2/4 之一未响应）"));
            }
            (void)controlRelay(0, v1WasOn);
            (void)controlRelay(1, v2WasOnStep1);
            (void)controlRelay(3, v4WasOn);
        } else {
            setStep(IDX_STEP1, STEP_FAIL, QString::fromUtf8("压力传感器4离线或阀门状态读取失败，已跳过"));
        }

        // ===== 步骤2：气泵/建压准备 =====
        transitionTo(SelfCheckFlowState::STEP2_PREPARE, QString::fromUtf8("进入步骤2建压准备"));
        const int leakBuildWaitMs = pumpManualForLeakTest
            ? 3000
            : ConfigManager::getInstance().getInt("selfcheck.valve2_leak_build_wait_ms", 2500);
        const int leakHoldWaitMs  = ConfigManager::getInstance().getInt("selfcheck.valve2_leak_hold_ms", 3500);
        const float leakBuildMinKpa  = ConfigManager::getInstance().getFloat("selfcheck.valve2_leak_build_min_kpa", 50.0f);
        const float leakP4DropMaxKpa = ConfigManager::getInstance().getFloat(
            "selfcheck.valve2_leak_max_p4_drop_kpa",
            dnBandValue(8.0f, 12.0f, 16.0f));
        const float leakP5RiseMaxKpa = ConfigManager::getInstance().getFloat(
            "selfcheck.valve2_leak_max_p5_rise_kpa",
            dnBandValue(3.0f, 5.0f, 8.0f));
        bool v2WasOn = false;
        bool pump1WasOn = false;
        const bool v2StateOk   = m_deviceManager->getRelayState(1, v2WasOn);
        const bool pumpStateOk = m_deviceManager->getRelayState(8, pump1WasOn);

        setStep(IDX_STEP2, RUNNING, QString::fromUtf8("检查气泵状态…"));
        bool canDoLeakTest = false;
        if (!(p4Ok && p5Ok)) {
            setStep(IDX_STEP2, STEP_FAIL, QString::fromUtf8("压力传感器4/5离线，无法执行泄漏判定"));
        } else if (!(v1StateOk && v2StateOk && pumpStateOk)) {
            setStep(IDX_STEP2, STEP_FAIL, QString::fromUtf8("阀门或泵状态读取失败，无法安全执行"));
        } else {
            setStep(IDX_STEP2, STEP_OK,
                    pumpManualForLeakTest ? QString::fromUtf8("手动泵模式：跳过提醒，继续检测") : QString::fromUtf8("自动泵控制链路正常"));
            canDoLeakTest = true;
        }

        // ===== 步骤3 & 4：压力观察 + 泄漏结论 =====
        if (canDoLeakTest) {
            transitionTo(SelfCheckFlowState::STEP3_OBSERVE, QString::fromUtf8("进入步骤3压力观察"));
            bool prepOk = controlRelay(1, false) && controlRelay(0, true);
            if (!prepOk) {
                setStep(IDX_STEP3, STEP_FAIL, QString::fromUtf8("前置阀门切换失败，已中止"));
                setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("前置失败，未形成结论"));
                transitionTo(SelfCheckFlowState::FAULT_STOP, QString::fromUtf8("步骤3前置失败"));
            } else {
                bool pumpStartedBySelfCheck = false;
                bool skipLeakResult = false;
                if (!pumpManualForLeakTest) {
                    setStep(IDX_STEP3, RUNNING, QString::fromUtf8("正在启动气泵建压…"));
                    if (!controlPump(0, true)) {
                        setStep(IDX_STEP3, STEP_FAIL, QString::fromUtf8("气泵启动失败，无法建压"));
                        setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("建压失败，未形成结论"));
                        skipLeakResult = true;
                    } else {
                        pumpStartedBySelfCheck = true;
                    }
                }
                if (!skipLeakResult) {
                    setStep(IDX_STEP3, RUNNING, QString::fromUtf8("等待建压 %1 ms…").arg(leakBuildWaitMs));
                    waitMs(leakBuildWaitMs);
                    m_deviceManager->updateAllDevices();
                    const auto p4Build = m_deviceManager->getPressureSensor(4);
                    const auto p5Build = m_deviceManager->getPressureSensor(5);
                    const bool buildOk = (p4Build.pressure >= leakBuildMinKpa);
                    if (pumpStartedBySelfCheck)
                        (void)controlPump(0, false);
                    if (!buildOk) {
                        setStep(IDX_STEP3, STEP_FAIL,
                                QString::fromUtf8("建压不足（PS4=%1 kgf/cm^2 < 最小建压 %2 kgf/cm^2）")
                                    .arg(fmtPressure(p4Build.pressure, 1)).arg(fmtPressure(leakBuildMinKpa, 1)));
                        setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("建压不足，无法有效判定泄漏"));
                        transitionTo(SelfCheckFlowState::FAULT_STOP, QString::fromUtf8("步骤3建压不足"));
                    } else {
                        setStep(IDX_STEP3, RUNNING,
                                QString::fromUtf8("PS4=%1 kgf/cm^2，保压 %2 ms 中…")
                                    .arg(fmtPressure(p4Build.pressure, 1)).arg(leakHoldWaitMs));
                        waitMs(leakHoldWaitMs);
                        m_deviceManager->updateAllDevices();
                        const auto p4Hold = m_deviceManager->getPressureSensor(4);
                        const auto p5Hold = m_deviceManager->getPressureSensor(5);
                        const float p4Delta = p4Build.pressure - p4Hold.pressure;
                        const float p4AbsDelta = std::abs(p4Delta);
                        const QString p4Direction = (p4Delta >= 0) ? QString::fromUtf8("升") : QString::fromUtf8("降");
                        const float p5Delta = p5Hold.pressure - p5Build.pressure;
                        const float p5AbsDelta = std::abs(p5Delta);
                        const QString p5Direction = (p5Delta >= 0) ? QString::fromUtf8("升") : QString::fromUtf8("降");
                        setStep(IDX_STEP3, STEP_OK,
                                QString::fromUtf8("已采集：PS4 %1\u2192%2 kgf/cm^2（%3 %4），PS5 %5\u2192%6 kgf/cm^2（%7 %8）")
                                    .arg(fmtPressure(p4Build.pressure, 2))
                                    .arg(fmtPressure(p4Hold.pressure, 2))
                                    .arg(p4Direction)
                                    .arg(fmtPressure(p4AbsDelta, 2))
                                    .arg(fmtPressure(p5Build.pressure, 2))
                                    .arg(fmtPressure(p5Hold.pressure, 2))
                                    .arg(p5Direction)
                                    .arg(fmtPressure(p5AbsDelta, 2)));
                        setStep(IDX_STEP4, RUNNING, QString::fromUtf8("正在判定…"));
                        transitionTo(SelfCheckFlowState::STEP4_CONCLUSION, QString::fromUtf8("进入步骤4泄漏结论"));
                        const bool leakOk = (p4AbsDelta <= leakP4DropMaxKpa) && (p5AbsDelta <= leakP5RiseMaxKpa);
                        setStep(IDX_STEP4, leakOk ? STEP_OK : STEP_FAIL,
                                leakOk
                                    ? QString::fromUtf8("密封正常（PS4变化 %1 kgf/cm^2 \u2264 %2，PS5变化 %3 kgf/cm^2 \u2264 %4）")
                                        .arg(fmtPressure(p4AbsDelta, 2)).arg(fmtPressure(leakP4DropMaxKpa, 2))
                                        .arg(fmtPressure(p5AbsDelta, 2)).arg(fmtPressure(leakP5RiseMaxKpa, 2))
                                    : QString::fromUtf8("疑似泄漏（PS4变化 %1 kgf/cm^2 阈值 %2，PS5变化 %3 kgf/cm^2 阈值 %4）")
                                        .arg(fmtPressure(p4AbsDelta, 2)).arg(fmtPressure(leakP4DropMaxKpa, 2))
                                        .arg(fmtPressure(p5AbsDelta, 2)).arg(fmtPressure(leakP5RiseMaxKpa, 2)));
                        if (!leakOk)
                            transitionTo(SelfCheckFlowState::FAULT_STOP, QString::fromUtf8("泄漏判定失败"));
                    }
                }
            }
            if (!pumpManualForLeakTest)
                (void)controlPump(0, pump1WasOn);
            (void)controlRelay(0, v1WasOn);
            (void)controlRelay(1, v2WasOn);
        } else {
            setStep(IDX_STEP3, STEP_FAIL, QString::fromUtf8("前置条件未满足，已跳过"));
            setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("前置条件未满足，已跳过"));
        }

        // ===== 电磁阀2/3/4 顺序动作测试 =====
        transitionTo(SelfCheckFlowState::VALVE_ACTION, QString::fromUtf8("进入顺序动作测试阶段"));
        const int valveActionPulseMs = ConfigManager::getInstance().getInt("selfcheck.valve_action_pulse_ms", 600);
        struct ValveActionItem { uint8_t index; int stepIdx; };
        const std::array<ValveActionItem, 3> valveActionItems{{
            {1, IDX_VA2}, {2, IDX_VA3}, {3, IDX_VA4}
        }};
        for (const auto &item : valveActionItems) {
            setStep(item.stepIdx, RUNNING, QString::fromUtf8("读取当前状态…"));
            bool current = false;
            if (!m_deviceManager->getRelayState(item.index, current)) {
                setStep(item.stepIdx, STEP_FAIL, QString::fromUtf8("状态读取失败，已跳过"));
                continue;
            }
            setStep(item.stepIdx, RUNNING, QString::fromUtf8("正在开启 %1 ms…").arg(valveActionPulseMs));
            const bool openOk = controlRelay(item.index, true);
            if (openOk)
                waitMs(valveActionPulseMs);
            const bool restoreOk = controlRelay(item.index, current);
            setStep(item.stepIdx, (openOk && restoreOk) ? STEP_OK : STEP_FAIL,
                    (openOk && restoreOk)
                        ? QString::fromUtf8("已开启 %1 ms 并恢复原状态").arg(valveActionPulseMs)
                        : QString::fromUtf8("动作失败（开阀:%1, 恢复:%2）")
                              .arg(openOk ? "OK" : "NG")
                              .arg(restoreOk ? "OK" : "NG"));
        }

        // ===== 完成 =====
        if (m_selfCheckBtn)
            m_selfCheckBtn->setEnabled(true);
        closeBtn->setText(QString::fromUtf8("自检完成，点击关闭"));
        closeBtn->setEnabled(true);
        if (flowState != SelfCheckFlowState::FAULT_STOP)
            transitionTo(SelfCheckFlowState::FINISH, QString::fromUtf8("自检流程完成"));
        QCoreApplication::processEvents();
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

    void Station1Panel::updateRelayButtons(bool force)
    {
        if (!force && !isVisible())
            return;

        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        const bool skipRelayButtonPaint = (m_stationClient && strictRemoteMode);

        if (!m_deviceManager)
            return;

        const auto relays = relayDefsForStation(m_panelConfig.stationNumber);
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        for (size_t i = 0; i < relays.size(); ++i)
        {
            bool on = false;
            const bool ok = m_deviceManager->getRelayState(relays[i].index, on);
            if (!ok)
                continue; // PLC 未连接时跳过，不改变显示

            // 若 M100.0 ~ M100.3 在置位后短时间内被拉回 false，给出可视化提示。
            if (isM100RelayIndex(relays[i].index) && m_expectM100Hold[relays[i].index])
            {
                const qint64 elapsed = nowMs - m_expectM100SetMs[relays[i].index];
                if (on)
                {
                    m_expectM100Hold[relays[i].index] = false;
                }
                else if (elapsed >= 200 && elapsed <= 3000)
                {
                    qWarning() << "[M100][Station1Panel] auto reset detected after set true"
                               << "index=" << relays[i].index
                               << "elapsedMs=" << elapsed;
                    QMessageBox::information(this,
                                             QString("%1 被自动复位").arg(relays[i].addr),
                                             QString("已写入 %1=1，但很快回读到 0。\n")
                                                 .arg(relays[i].addr) +
                                                 QString("这通常表示 PLC 程序中有复位逻辑（如联锁条件不满足或 R 线圈）。"));
                    m_expectM100Hold[relays[i].index] = false;
                }
                else if (elapsed > 3000)
                {
                    m_expectM100Hold[relays[i].index] = false;
                }
            }

            // 图元颜色联动：M100.0 ~ M100.3 对应图元随 relay 状态变化。
            if (isM100RelayIndex(relays[i].index))
                setRelayValveGlyphState(m_scene, relays[i].index, on);

            if (skipRelayButtonPaint)
                continue;

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
        if (isM100RelayIndex(index))
        {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (m_lastM100ToggleMs[index] > 0 && (nowMs - m_lastM100ToggleMs[index]) < 700)
            {
                qWarning() << "[M100][Station1Panel] relay toggle ignored by M100 guard"
                           << "index=" << index
                           << "elapsedMs=" << (nowMs - m_lastM100ToggleMs[index]);
                return;
            }
            m_lastM100ToggleMs[index] = nowMs;
        }

        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        const auto relayDefs = relayDefsForStation(m_panelConfig.stationNumber);
        const RelayDef *def = findRelayDefByIndex(relayDefs, index);
        const QString addr = def ? def->addr : QString("Q?");
        const bool useRemote = (m_stationClient && strictRemoteMode);

        qInfo() << "[M100][Station1Panel] relay click"
            << "src=" << source
            << "index=" << index
            << "addr=" << addr
            << "strictRemoteMode=" << strictRemoteMode
            << "hasStationClient=" << (m_stationClient != nullptr)
            << "useRemote=" << useRemote;

        // 泵输出位暂不开放本地切换，仅做状态观察。
        if (def && def->type == "pump")
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
            if (isM100RelayIndex(index))
                m_expectM100Hold[index] = false;
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
            if (def)
                btn->setText(formatRelayBtnText(def->addr, def->label, target));
            btn->setProperty("dqOn", target);
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
            if (isM100RelayIndex(index))
                setRelayValveGlyphState(m_scene, index, target);
            qInfo() << "[M100][Station1Panel] ui optimistic update"
                    << "index=" << index
                    << "addr=" << (def ? def->addr : QString("Q?"))
                    << "dqOn=" << target;
        }

        if (useRemote)
        {
            if (isM100RelayIndex(index))
                m_expectM100Hold[index] = false;
        }
        else
        {
            if (isM100RelayIndex(index))
            {
                if (target)
                {
                    m_expectM100Hold[index] = true;
                    m_expectM100SetMs[index] = QDateTime::currentMSecsSinceEpoch();
                }
                else
                {
                    m_expectM100Hold[index] = false;
                }
            }
            // 回读校准放到事件循环后执行，避免阻塞当前帧绘制。
            QTimer::singleShot(250, this, [this]() { updateRelayButtons(); });
        }
    }

    bool Station1Panel::controlStartStop(bool start, const char *source)
    {
        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        const bool useRemote = (m_stationClient && strictRemoteMode);

        const int remotePumpIndex = ConfigManager::getInstance().getInt("station1.start_stop.remote_pump_index", 0);
        const int localPumpId = ConfigManager::getInstance().getInt("station1.start_stop.local_pump_id", 1);

        bool ok = false;
        if (useRemote)
        {
            if (!m_stationClient->isConnected())
            {
                qWarning() << "[Station1Panel] start/stop remote rejected: station client disconnected"
                           << "source=" << source << "start=" << start;
                ok = false;
            }
            else
            {
                ControlCommand cmd;
                cmd.command_type = 1; // pump
                cmd.index = static_cast<uint8_t>(std::max(0, remotePumpIndex));
                cmd.action = start ? 1 : 0;
                ok = m_stationClient->sendCommand(cmd);
            }
        }
        else if (m_deviceManager)
        {
            ok = m_deviceManager->controlPump(static_cast<uint16_t>(std::max(1, localPumpId)), start);
        }

        if (!ok)
        {
            QMessageBox::warning(this,
                                 "操作失败",
                                 start ? "开始指令下发失败，请检查 PLC/终端连接状态。"
                                       : "停止指令下发失败，请检查 PLC/终端连接状态。");
        }
        return ok;
    }

    void Station1Panel::onStartButtonClicked(const char *source)
    {
        qInfo() << "[Station1Panel] start requested" << "source=" << source;
        (void)controlStartStop(true, source);
    }

    void Station1Panel::onStopButtonClicked(const char *source)
    {
        qInfo() << "[Station1Panel] stop requested" << "source=" << source;
        (void)controlStartStop(false, source);
    }

    void Station1Panel::pollPhysicalStartStopButtons()
    {
        if (m_panelConfig.stationNumber != 1)
            return;
        if (!m_deviceManager)
            return;
        if (!ConfigManager::getInstance().getBool("station1.start_stop.physical_enable", true))
            return;

        const int startByte = ConfigManager::getInstance().getInt("station1.start_stop.physical_start_m_byte", 101);
        const int startBit = ConfigManager::getInstance().getInt("station1.start_stop.physical_start_m_bit", 0);
        const int stopByte = ConfigManager::getInstance().getInt("station1.start_stop.physical_stop_m_byte", 101);
        const int stopBit = ConfigManager::getInstance().getInt("station1.start_stop.physical_stop_m_bit", 1);
        const int startBitClamped = (startBit < 0) ? 0 : ((startBit > 7) ? 7 : startBit);
        const int stopBitClamped = (stopBit < 0) ? 0 : ((stopBit > 7) ? 7 : stopBit);
        const int startByteSafe = (startByte < 0) ? 0 : startByte;
        const int stopByteSafe = (stopByte < 0) ? 0 : stopByte;

        bool startPressed = false;
        const bool startOk = m_deviceManager->readMerkerState(static_cast<uint16_t>(startByteSafe),
                                       static_cast<uint8_t>(startBitClamped),
                                                               startPressed);
        if (startOk)
        {
            if (startPressed && !m_lastStartPhysicalPressed)
                onStartButtonClicked("physical");
            m_lastStartPhysicalPressed = startPressed;
        }

        bool stopPressed = false;
        const bool stopOk = m_deviceManager->readMerkerState(static_cast<uint16_t>(stopByteSafe),
                                      static_cast<uint8_t>(stopBitClamped),
                                                              stopPressed);
        if (stopOk)
        {
            if (stopPressed && !m_lastStopPhysicalPressed)
                onStopButtonClicked("physical");
            m_lastStopPhysicalPressed = stopPressed;
        }
    }

    void Station1Panel::buildScene()
    {
        const auto relayGlyphMap = relayGlyphMapForStation(m_panelConfig.stationNumber);
        QString labelV1;
        QString labelV2;
        QString labelVReg1;
        QString labelTestValve;
        QString labelVBack1;
        QString labelVReg2;
        int valveIdV1 = 1;
        int valveIdV2 = 2;
        int valveIdTest = 3;
        int valveIdBack = 4;

        switch (m_panelConfig.stationNumber)
        {
        case 1:
            labelV1 = QString::fromUtf8("电磁阀1");
            labelV2 = QString::fromUtf8("电磁阀2");
            labelVReg1 = QString::fromUtf8("电动调压阀1");
            labelTestValve = QString::fromUtf8("电磁阀3（待测试电磁阀）");
            labelVBack1 = QString::fromUtf8("电磁阀4");
            labelVReg2 = QString::fromUtf8("电动调压阀2");
            valveIdV1 = 1;
            valveIdV2 = 2;
            valveIdTest = 3;
            valveIdBack = 4;
            break;
        case 2:
            labelV1 = QString::fromUtf8("电磁阀5");
            labelV2 = QString::fromUtf8("电磁阀6");
            labelVReg1 = QString::fromUtf8("电动调压阀3");
            labelTestValve = QString::fromUtf8("电磁阀7（待测试电磁阀）");
            labelVBack1 = QString::fromUtf8("电磁阀8");
            labelVReg2 = QString::fromUtf8("电动调压阀4");
            valveIdV1 = 5;
            valveIdV2 = 6;
            valveIdTest = 7;
            valveIdBack = 8;
            break;
        case 3:
            labelV1 = QString::fromUtf8("电磁阀9");
            labelV2 = QString::fromUtf8("电磁阀10");
            labelVReg1 = QString::fromUtf8("电动调压阀5");
            labelTestValve = QString::fromUtf8("电磁阀11（待测试电磁阀）");
            labelVBack1 = QString::fromUtf8("电磁阀12");
            labelVReg2 = QString::fromUtf8("电动调压阀6");
            valveIdV1 = 9;
            valveIdV2 = 10;
            valveIdTest = 11;
            valveIdBack = 12;
            break;
        default:
        {
            const int valveNoBase = 1 + (m_panelConfig.stationNumber - 1) * 2;
            labelV1 = QString::fromUtf8("电磁阀%1").arg(valveNoBase);
            labelV2 = QString::fromUtf8("电磁阀%1").arg(valveNoBase + 1);
            labelVReg1 = QString::fromUtf8("电动调压阀%1").arg(m_panelConfig.stationNumber);
            labelTestValve = QString::fromUtf8("电磁阀%1（待测试电磁阀）").arg(valveNoBase + 2);
            labelVBack1 = QString::fromUtf8("电磁阀%1").arg(valveNoBase + 3);
            labelVReg2 = QString::fromUtf8("电动调压阀%1").arg(m_panelConfig.stationNumber + 1);
            valveIdV1 = valveNoBase;
            valveIdV2 = valveNoBase + 1;
            valveIdTest = valveNoBase + 2;
            valveIdBack = valveNoBase + 3;
            break;
        }
        }

        if (!m_scene)
            return;

        m_scene->clear();

        ensureUiTokensInitialized();

        // 先给一个较大的场景范围，后面会按图元边界收紧
        m_scene->setSceneRect(0, 0, 1900, 780);

        // 与“测试准备区”一致：蓝色网格 + 拟物灰背景
        addBlueGridBackground(m_scene, m_scene->sceneRect());

        // 标题
        auto *caption = m_scene->addText(QString("%1号操作台流程 -- DN25").arg(m_panelConfig.stationNumber));
        caption->setDefaultTextColor(kUiText);
        QFont tf = caption->font();
        tf.setPointSize(14);
        tf.setBold(true);
        caption->setFont(tf);
        caption->setPos(8, 8);
        caption->setData(0, "hmi_caption");
        caption->setZValue(5);

        // ==== 图标化设备布局（两排）====
        // 该区块只负责“坐标定义”，尽量把可调参数集中，避免后续叠加偏移难维护。
        const bool simplifiedStation1 = (m_panelConfig.stationNumber == 1);
        const qreal gridStepX = 300;
        const qreal gridBaseX = 10;
        const qreal stationBaseShiftX = 200;
        const qreal yRow1 = 220;
        const qreal yRow2 = 530;
        const qreal valvePortYOffset = 14;
        const qreal row1AfterAccumulatorYOffset = -valvePortYOffset;
        const qreal row2AfterFlowMeterYOffset = -valvePortYOffset;
        const qreal pressureSensorTapYOffset = -28;

        // 列坐标（以“列号”而不是硬编码 x）
        const auto colX = [&](qreal col) { return gridBaseX + stationBaseShiftX + gridStepX * col; };

        // 1号台：直接使用显式坐标（便于人工微调）；2/3号台沿用列坐标规则。
        // 下面几个长度参数里，和你当前关注点最相关的是 fmRightPipeLen：
        // 它控制“流量计右出口出来后，先向右走多远，再向下拐”的那一小段水平距离。
        // 由于当前走线采用三段折线：start -> p1 -> p2 -> end，
        // 其中 p1 是右上拐点，p2 是右下拐点，所以 p1.x()/p2.x() 越大，
        // 你肉眼看到的“右侧转折横向管道”就越长。
        const qreal xAcc = simplifiedStation1 ? 200.0 : colX(0.0);
        const qreal xV3w = colX(1.0);
        const qreal xFmStation1 = 1650.0;
        const qreal fmTopTransitionPipeLen = 90.0;
        const qreal fmRightPipeLen = simplifiedStation1 ? 180.0 : 90.0;
        const qreal station1InletPipeLen = 240.0;
        const qreal station1TopRowShift = simplifiedStation1 ? -160.0 : 0.0;
        const qreal station1V1ShiftX = simplifiedStation1 ? -100.0 : 0.0;
        const qreal valveOutletOffsetX = ValveItem::outletPortLocal().x();
        const qreal valveInletOffsetX = ValveItem::inletPortLocal().x();
        const QPointF flowMeterInletLocal = FlowMeterItem::inletPortLocal();
        const QPointF flowMeterOutletLocal = FlowMeterItem::outletPortLocal();
        const QPointF flowMeterLeftPortLocal =
            (flowMeterInletLocal.x() <= flowMeterOutletLocal.x()) ? flowMeterInletLocal : flowMeterOutletLocal;
        const QPointF flowMeterRightPortLocal =
            (flowMeterInletLocal.x() <= flowMeterOutletLocal.x()) ? flowMeterOutletLocal : flowMeterInletLocal;
        const qreal valveOutletOffsetXScene = valveOutletOffsetX * kDeviceItemScale;
        const qreal flowMeterLeftOffsetXScene = flowMeterLeftPortLocal.x() * kDeviceItemScale;
        const qreal station1VRegAnchorX = xFmStation1 + flowMeterLeftOffsetXScene - valveOutletOffsetXScene - fmTopTransitionPipeLen + station1TopRowShift;
                const qreal station1ValveToValvePipeLen = station1InletPipeLen;
        const qreal xV1 = simplifiedStation1
                      ? (((xAcc + station1TopRowShift) + (station1VRegAnchorX - (xAcc + station1TopRowShift)) / 3.0) + station1V1ShiftX)
                      : colX(2.0);
                const qreal xV2 = simplifiedStation1
                                                            ? (xV1 + station1ValveToValvePipeLen + (valveOutletOffsetX - valveInletOffsetX) * kDeviceItemScale)
                                                            : colX(3.0);
        const qreal xVReg = simplifiedStation1
                                                                ? (xV2 + station1ValveToValvePipeLen + (valveOutletOffsetX - valveInletOffsetX) * kDeviceItemScale)
                                : colX(4.0);
        const uint16_t topRegValveId = static_cast<uint16_t>(std::max(1, m_panelConfig.stationNumber * 2 - 1));
        const uint16_t bottomRegValveId = static_cast<uint16_t>(topRegValveId + 1);
        const qreal xPs3 = (((xAcc + station1TopRowShift) + AccumulatorItem::outletPortLocal().x()) +
                    (xV1 + ValveItem::inletPortLocal().x())) *
                   0.5;
        const qreal xPs1 = (xV1 + xV2) * 0.5;
        const qreal xPs2 = (xV2 + xVReg) * 0.5;

        // 下排与上排对齐：流量计放到右侧竖向落点，待测阀3与电动调压阀1同列。
        const qreal station1FlowMeterShiftX = simplifiedStation1 ? -140.0 : 0.0;
        const qreal xFm = simplifiedStation1 ? (xFmStation1 + station1TopRowShift + station1FlowMeterShiftX) : (xVReg + 260.0);
        const qreal yFm = simplifiedStation1 ? (yRow1 + row1AfterAccumulatorYOffset) : yRow1;
        const qreal flowMeterScale = simplifiedStation1 ? 1.25 : kDeviceItemScale;
        const qreal xTestValve = simplifiedStation1 ? xFm : xVReg;
        const qreal xVBack1 = simplifiedStation1 ? xVReg : xV1;
        const qreal xVBackReg = simplifiedStation1 ? xV2 : xAcc;
        const qreal xLoop = xAcc;
        // 1号台电磁阀5与压力罐同列，回路末端通过折线回接到压力罐。
        const qreal xV5 = simplifiedStation1 ? xV1 : (xAcc + gridStepX * -0.95);
        const qreal xPs8 = simplifiedStation1 ? xPs1 : (xVBackReg + xV5) * 0.5;
        const qreal xPt1 = simplifiedStation1 ? xFm : (xFm + xTestValve) * 0.5;
        const qreal xPt2 = simplifiedStation1 ? xPs2 : (xTestValve + xVBack1) * 0.5;
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

        auto alignItemPortX = [&](QGraphicsItem *item,
                                  const QPointF &itemPortLocal,
                                  const QGraphicsItem *refItem,
                                  const QPointF &refPortLocal)
        {
            if (!item || !refItem)
                return;
            const qreal itemPortX = item->mapToScene(itemPortLocal).x();
            const qreal refPortX = refItem->mapToScene(refPortLocal).x();
            item->setX(item->x() + (refPortX - itemPortX));
        };

        // 第一排（从左到右，列 0~6）
        AccumulatorItem *acc = nullptr;
        if (!simplifiedStation1)
        {
            acc = new AccumulatorItem("压力罐", true);
            place(acc, xAcc, yRow1);
            acc->setScale(kAccumulatorItemScale);
        }

        SensorItem *ps3 = nullptr;
        if (simplifiedStation1)
        {
            ps3 = new SensorItem(QString("压力3"), "kgf/cm^2", kUiPurple);
            ps3->setData(1, 3);
            place(ps3, xPs3, yRow1 + row1AfterAccumulatorYOffset + pressureSensorTapYOffset);
        }

        ThreeWayValveItem *v3w = nullptr;
        if (!simplifiedStation1)
        {
            v3w = new ThreeWayValveItem("电动三通切换阀");
            place(v3w, xV3w, yRow1 + row1AfterAccumulatorYOffset);
        }

        auto *v1 = new ValveItem(labelV1, true, 100.0);
        place(v1, xV1, yRow1 + row1AfterAccumulatorYOffset);
        v1->setData(3, valveIdV1);
        v1->setData(4, relayGlyphMap.v1);
        v1->setFlag(QGraphicsItem::ItemIsSelectable, false);

        // 压力传感器上置：使底部红点与主干管道平齐。
        auto *ps1 = new SensorItem(QString("压力%1").arg(m_panelConfig.pressureSensorIds[0]), "kgf/cm^2", kUiPurple);
        ps1->setData(1, static_cast<int>(m_panelConfig.pressureSensorIds[0]));
        place(ps1, xPs1, yRow1 + row1AfterAccumulatorYOffset + pressureSensorTapYOffset);

        auto *v2 = new ValveItem(labelV2, true, 100.1);
        place(v2, xV2, yRow1 + row1AfterAccumulatorYOffset);
        v2->setData(3, valveIdV2);
        v2->setData(4, relayGlyphMap.v2);
        v2->setFlag(QGraphicsItem::ItemIsSelectable, false);

        auto *ps2 = new SensorItem(QString("压力%1").arg(m_panelConfig.pressureSensorIds[1]), "kgf/cm^2", kUiPurple);
        ps2->setData(1, static_cast<int>(m_panelConfig.pressureSensorIds[1]));
        place(ps2, xPs2, yRow1 + row1AfterAccumulatorYOffset + pressureSensorTapYOffset);

        auto *vReg = new ValveItem(labelVReg1, false, 0.0, true, simplifiedStation1);
        place(vReg, xVReg, yRow1 + row1AfterAccumulatorYOffset);
        vReg->setData(3, 6);
        vReg->setData(6, static_cast<int>(topRegValveId));

        // 第二排（从右到左，列 6~0）
        auto *fm = new FlowMeterItem("流量计");
        place(fm, xFm, yFm);
        fm->setScale(flowMeterScale);
        if (simplifiedStation1)
        {
            const qreal targetMainPipeY = vReg->mapToScene(ValveItem::outletPortLocal()).y();
            const qreal fmLeftPortY = fm->mapToScene(flowMeterLeftPortLocal).y();
            fm->setY(fm->y() + (targetMainPipeY - fmLeftPortY));
        }

        auto *pt1 = new SensorItem(QString("压力%1").arg(m_panelConfig.pressureSensorIds[2]), "kgf/cm^2", kUiOrange);
        pt1->setData(1, static_cast<int>(m_panelConfig.pressureSensorIds[2]));
        pt1->setData(2, "kgf/cm^2");
        place(pt1, xPt1, yRow2 + row2AfterFlowMeterYOffset + pressureSensorTapYOffset);

        auto *testValve = new ValveItem(labelTestValve, false, 100.2);
        place(testValve, xTestValve, yRow2 + row2AfterFlowMeterYOffset);
        testValve->setData(3, valveIdTest);
        testValve->setData(4, relayGlyphMap.test);
        testValve->setFlag(QGraphicsItem::ItemIsSelectable, false);

        auto *pt2 = new SensorItem(QString("压力%1").arg(m_panelConfig.pressureSensorIds[3]), "kgf/cm^2", kUiOrange);
        pt2->setData(1, static_cast<int>(m_panelConfig.pressureSensorIds[3]));
        pt2->setData(2, "kgf/cm^2");
        place(pt2, xPt2, yRow2 + row2AfterFlowMeterYOffset + pressureSensorTapYOffset);

        auto *vBack1 = new ValveItem(labelVBack1, true, 100.3);
        place(vBack1, xVBack1, yRow2 + row2AfterFlowMeterYOffset);
        vBack1->setData(3, valveIdBack);
        vBack1->setData(4, relayGlyphMap.v3);
        vBack1->setFlag(QGraphicsItem::ItemIsSelectable, false);

        auto *vBackReg = new ValveItem(labelVReg2, false, 0.0, true, simplifiedStation1);
        place(vBackReg, xVBackReg, yRow2 + row2AfterFlowMeterYOffset);
        vBackReg->setData(3, 9);
        vBackReg->setData(6, static_cast<int>(bottomRegValveId));

        SensorItem *ps8 = nullptr;
        ValveItem *v5 = nullptr;
        if (simplifiedStation1)
        {
            ps8 = new SensorItem(QString("压力8"), "kgf/cm^2", kUiOrange);
            ps8->setData(1, 8);
            ps8->setData(2, "kgf/cm^2");
            place(ps8, xPs8, yRow2 + row2AfterFlowMeterYOffset + pressureSensorTapYOffset);

            v5 = new ValveItem(QString::fromUtf8("电磁阀5"), true, 100.0);
            place(v5, xV5, yRow2 + row2AfterFlowMeterYOffset);
            v5->setData(3, 5);
            v5->setData(4, 5);
            v5->setFlag(QGraphicsItem::ItemIsSelectable, false);
        }

        LoopItem *loopNode = nullptr;
        if (!simplifiedStation1)
        {
            loopNode = new LoopItem("回路");
            place(loopNode, xLoop, yRow2);
        }

        auto alignSensorAnchorToPipeMid = [&](SensorItem *sensor, const QPointF &pipeStart, const QPointF &pipeEnd)
        {
            if (!sensor)
                return;

            const QPointF mid((pipeStart.x() + pipeEnd.x()) * 0.5,
                              (pipeStart.y() + pipeEnd.y()) * 0.5);
            const QPointF anchorLocal = SensorItem::inletPortLocal();
            sensor->setPos(mid.x() - anchorLocal.x() * kDeviceItemScale,
                           mid.y() - anchorLocal.y() * kDeviceItemScale);
        };

        alignSensorAnchorToPipeMid(ps1,
                                   v1->mapToScene(ValveItem::outletPortLocal()),
                                   v2->mapToScene(ValveItem::inletPortLocal()));
        alignSensorAnchorToPipeMid(ps2,
                                   v2->mapToScene(ValveItem::outletPortLocal()),
                                   vReg->mapToScene(ValveItem::inletPortLocal()));

        if (simplifiedStation1 && ps3)
        {
            const QPointF end = v1->mapToScene(ValveItem::inletPortLocal());
            const QPointF start(end.x() - station1InletPipeLen, end.y());
            alignSensorAnchorToPipeMid(ps3, start, end);
        }

        {
            // 这里不是直接画主管道，而是先用和主管道相同的几何点位去安放 PT1。
            // 这样 PT1 会始终贴着“右侧竖直下落段”附近，不会因为后面改拐点长度而漂到错误位置。
            //
            // 几何含义：
            // fmRight  : 流量计右出口
            // testOut  : 待测阀右侧端口
            // elbowX   : 右侧拐点所在的统一 x 坐标
            // p1       : 上拐点（先向右走到这里）
            // p2       : 下拐点（再沿竖线下落到这里）
            const QPointF fmRight = fm->mapToScene(flowMeterRightPortLocal);
            const QPointF testOut = testValve->mapToScene(ValveItem::outletPortLocal());
            const qreal elbowX = std::max(fmRight.x(), testOut.x()) + fmRightPipeLen;
            const QPointF p1(elbowX, fmRight.y());
            const QPointF p2(elbowX, testOut.y());
            alignSensorAnchorToPipeMid(pt1, p2, testOut);
            if (simplifiedStation1)
                pt1->setX(pt1->x() + 90.0);
        }

        alignSensorAnchorToPipeMid(pt2,
                                   testValve->mapToScene(ValveItem::inletPortLocal()),
                                   vBack1->mapToScene(ValveItem::outletPortLocal()));

        if (simplifiedStation1 && ps8 && v5)
        {
            alignSensorAnchorToPipeMid(ps8,
                                       vBackReg->mapToScene(ValveItem::inletPortLocal()),
                                       v5->mapToScene(ValveItem::outletPortLocal()));

            // 1号台关键图元按端口精确对齐，避免仅按图元中心对齐造成视觉误差。
            alignItemPortX(vBack1, ValveItem::inletPortLocal(), vReg, ValveItem::inletPortLocal());
            alignItemPortX(vBackReg, ValveItem::inletPortLocal(), v2, ValveItem::inletPortLocal());
            alignItemPortX(v5, ValveItem::inletPortLocal(), v1, ValveItem::inletPortLocal());
            alignItemPortX(ps8, SensorItem::inletPortLocal(), ps1, SensorItem::inletPortLocal());
        }

        // ==== 管道连接（四段主流程，按工艺流向编号）====

        // 管路 1：上排供压主线
        // 1号台：左侧来流 -> 电动阀 V1 -> 电动阀 V2 -> 电动调压阀（压力3上置测点）。
        // 2/3号台：蓄能器 -> 电动三通切换阀 -> 电动阀 V1 -> 电动阀 V2 -> 电动调压阀。
        if (simplifiedStation1)
        {
            const QPointF end = v1->mapToScene(ValveItem::inletPortLocal());
            const QPointF start(end.x() - station1InletPipeLen, end.y());
            QPainterPath path(start);
            path.lineTo(end);
            addHmiPipeWithArrow(m_scene, path, end, start);
        }
        else
        {
            connectPorts(m_scene, acc->mapToScene(AccumulatorItem::outletPortLocal()), v3w->mapToScene(ThreeWayValveItem::inletPortLocal()));
            connectPorts(m_scene, v3w->mapToScene(ThreeWayValveItem::outletPortLocal()), v1->mapToScene(ValveItem::inletPortLocal()));
        }
        connectPorts(m_scene, v1->mapToScene(ValveItem::outletPortLocal()), v2->mapToScene(ValveItem::inletPortLocal()));
        connectPorts(m_scene, v2->mapToScene(ValveItem::outletPortLocal()), vReg->mapToScene(ValveItem::inletPortLocal()));

        // 管路 2：上排到下排的跨排过渡线
        // 路径：电动调压阀出口 ->（向右预留）->（垂直下行）-> 流量计入口侧。
        // 描述：该段负责完成上排主线到下排测试回路的“换行”连接；
        //       走线采用“水平 -> 垂直 -> 水平”折线，确保跨排连接无斜线。
        //       其中 +50 为预留水平过渡段长度，用于避免与设备本体过近。
        {
            const QPointF start = vReg->mapToScene(ValveItem::outletPortLocal());
            const QPointF end = fm->mapToScene(flowMeterLeftPortLocal);
            QPainterPath path(start);
            const QPointF p1(start.x() + fmTopTransitionPipeLen, start.y());
            const QPointF p2(p1.x(), end.y());
            path.lineTo(p1);
            path.lineTo(p2);
            path.lineTo(end);
            addHmiPipeWithArrow(m_scene, path, end, p2);
        }

        // 管路 3：下排测试主线
        // 路径：流量计右侧出口 ->（右移）->（下行）->（左移）-> 待测试阀（电磁阀3） -> 回路电动阀（vBack1）。
        //
        // 这段是你现在手动调图时最应该看的几何控制点：
        // start  : 流量计右出口，折线起点。
        // end    : 待测试阀右侧端口，折线终点。
        // elbowX : 右侧“外扩”后的统一 x 坐标；它决定右边那一小段横向净空到底有多长。
        // p1     : 第一拐点，表示从流量计出来后先向右走到哪里。
        // p2     : 第二拐点，表示竖直下落后准备回折向左的位置。
        //
        // 如果你后续还要继续手工调：
        // 1. 想让右侧横管更长，优先改 fmRightPipeLen。
        // 2. 想整体右移整套“流量计 + 待测阀”关系，再看 xFm / xTestValve。
        // 3. p1/p2 现在会画红点，方便你在界面里直接观察拐点是否如预期移动。
        {
            const QPointF start = fm->mapToScene(flowMeterRightPortLocal);
            const QPointF end = testValve->mapToScene(ValveItem::outletPortLocal());
            QPainterPath path(start);
            const qreal elbowX = std::max(start.x(), end.x()) + fmRightPipeLen;
            const QPointF p1(elbowX, start.y());
            const QPointF p2(elbowX, end.y());
            path.lineTo(p1);
            path.lineTo(p2);
            path.lineTo(end);
            addHmiPipeWithArrowCustom(m_scene,
                                      path,
                                      end,
                                      p2,
                                      kPipeOuterWidth + 3.0,
                                      kPipeInnerWidth + 2.0,
                                      kPipeFlowWidth + 1.0);

            // 调图辅助：把右侧两个拐点直接标红，方便现场肉眼确认“长度参数”到底在控制哪两个点。
            if (simplifiedStation1)
            {
                addDebugPointMarker(m_scene, p1, "P1");
                addDebugPointMarker(m_scene, p2, "P2");
            }
        }
        connectPorts(m_scene, testValve->mapToScene(ValveItem::inletPortLocal()), vBack1->mapToScene(ValveItem::outletPortLocal()));

        // 管路 4：下排收口段
        // 1号台：回路电动阀（vBack1） -> 回路电动调压阀（vBackReg） -> 电磁阀5 -> 左侧去向（压力8为上置测点，不串接在主管道内）。
        // 2/3号台：回路电动阀（vBack1） -> 回路电动调压阀（vBackReg） -> 回路节点（loopNode）。
        connectPorts(m_scene, vBack1->mapToScene(ValveItem::inletPortLocal()), vBackReg->mapToScene(ValveItem::outletPortLocal()));
        if (simplifiedStation1)
        {
            connectPorts(m_scene, vBackReg->mapToScene(ValveItem::inletPortLocal()), v5->mapToScene(ValveItem::outletPortLocal()));
            {
                // 电磁阀5左侧出口：直接向左引出到站外去向。
                const QPointF start = v5->mapToScene(ValveItem::inletPortLocal());
                const QPointF end(start.x() - station1InletPipeLen, start.y());
                QPainterPath path(start);
                path.lineTo(end);
                addHmiPipeWithArrow(m_scene, path, end, start);
            }
        }
        else
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
        if (!isVisible())
            return;

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

            pathItem->setVisible(anyPumpRunning);
            if (!anyPumpRunning)
                continue;

            QPen pen = pathItem->pen();
            pen.setDashOffset(m_flowDashOffset);
            pathItem->setPen(pen);
        }
    }

    void Station1Panel::updateSensorValues(bool force)
    {
        if (!force && !isVisible())
            return;

        if (!m_scene)
            return;

        const bool strictRemoteMode = ConfigManager::getInstance().getBool("station.strict_remote_mode", true);
        if (m_stationClient && strictRemoteMode)
        {
            const SensorData net = m_stationClient->getLatestSensorData();
            const auto allItems = m_scene->items();

            const auto &mappedSensorIds = m_panelConfig.pressureSensorIds;
            for (size_t idx = 0; idx < mappedSensorIds.size(); ++idx)
            {
                const int sensorId = static_cast<int>(mappedSensorIds[idx]);
                const int remotePressureIndex = resolveRemotePressureIndex(mappedSensorIds[idx], static_cast<int>(idx));
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

                    sensorItem->setValue(static_cast<double>(net.pressure[remotePressureIndex]));
                    sensorItem->setDisplayDecimals(2);
                }
            }

            if (m_panelConfig.stationNumber == 1)
            {
                {
                    const int sensorId = 3;
                    const int remotePressureIndex = resolveRemotePressureIndex(static_cast<uint16_t>(sensorId), 2);
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

                        sensorItem->setValue(static_cast<double>(net.pressure[remotePressureIndex]));
                        sensorItem->setDisplayDecimals(2);
                    }
                }

                const int sensorId = 8;
                const int remotePressureIndex = resolveRemotePressureIndex(static_cast<uint16_t>(sensorId), 3);
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

                    sensorItem->setValue(static_cast<double>(net.pressure[remotePressureIndex]));
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

            if (m_deviceManager)
            {
                for (auto *it : allItems)
                {
                    if (!it)
                        continue;

                    auto *valveItem = dynamic_cast<ValveItem *>(it);
                    if (!valveItem)
                        continue;

                    const QVariant regVar = it->data(6);
                    if (!regVar.isValid())
                        continue;

                    const int regId = regVar.toInt();
                    if (regId <= 0)
                        continue;

                    const auto rv = m_deviceManager->getRegulatingValve(static_cast<uint16_t>(regId));
                    const double opening = qBound(0.0, static_cast<double>(rv.openingPercent), 100.0);
                    valveItem->setOpen(opening > 0.1);
                    valveItem->setDegree(opening);
                }
            }

            // 物理按钮联动依赖本地 PLC 位读取；若本地可读则沿用同一动作链路。
            pollPhysicalStartStopButtons();

            return;
        }

        if (!m_deviceManager)
            return;

        const auto allItems = m_scene->items();

        for (uint16_t mappedSensorId : m_panelConfig.pressureSensorIds)
        {
            const int sensorId = static_cast<int>(mappedSensorId);
            const auto sensor = m_deviceManager->getPressureSensor(mappedSensorId);
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

        if (m_panelConfig.stationNumber == 1)
        {
            {
                const int sensorId = 3;
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

            const int sensorId = 8;
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

        for (auto *it : allItems)
        {
            if (!it)
                continue;

            auto *valveItem = dynamic_cast<ValveItem *>(it);
            if (!valveItem)
                continue;

            const QVariant regVar = it->data(6);
            if (regVar.isValid())
            {
                const int regId = regVar.toInt();
                if (regId > 0)
                {
                    const auto rv = m_deviceManager->getRegulatingValve(static_cast<uint16_t>(regId));
                    const double opening = qBound(0.0, static_cast<double>(rv.openingPercent), 100.0);
                    valveItem->setOpen(opening > 0.1);
                    valveItem->setDegree(opening);
                }
                continue;
            }

            // 1号操作台的关键电磁阀图元由继电器状态驱动（data(4)），
            // 避免被 getValve 轮询结果覆盖造成“瞬间自动关掉”的假象。
            if (m_panelConfig.stationNumber == 1 && it->data(4).isValid())
                continue;

            const QVariant v = it->data(3);
            if (!v.isValid())
                continue;

            const int valveId = v.toInt();
            if (valveId <= 0)
                continue;

            const auto valve = m_deviceManager->getValve(static_cast<uint16_t>(valveId));
            const bool open = (valve.status == ValveStatus::OPEN || valve.status == ValveStatus::OPENING);
            valveItem->setOpen(open);
            valveItem->setDegree(static_cast<double>(valve.openingDegree));
        }

        const auto flowMeter = m_deviceManager->getFlowMeter(m_panelConfig.flowMeterId);
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

        pollPhysicalStartStopButtons();
    }

} // namespace WaterTest
