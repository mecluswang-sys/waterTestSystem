/**
 * @file Station1Panel.cpp
 * @brief 1号操作台面板实现（工艺流程图展示 + 实时控制）
 *
 * 架构概览
 * --------
 * Station1Panel 是一个 QWidget，内嵌 QGraphicsView+QGraphicsScene 展示工艺流程图。
 * 场景中每个设备节点都是自定义 QGraphicsItem 子类（ElectricValveItem / RegulatingValveItem / SensorItem / FlowMeterItem 等），
 * 由 buildScene() 一次性构建，之后通过三条定时器周期刷新数据与动画。
    
    void Station1Panel::triggerSelfCheck()
    {
        onSelfCheck();
    }
 
 *
 * 数据来源
 * --------
 * - 当 StationClient 已连接时，优先走主控远程链路。
 * - 当 StationClient 未连接时，回落到本地 DeviceManager 直连 PLC。
 *
 * 三条定时器
 * ----------
 * - m_flowTimer  (50 ms)   : 更新管道流动虚线的 dashOffset，产生液体流动视觉效果。
 * - m_dataTimer  (100 ms)  : 刷新压力/流量/阀门开度到场景图元。
 *                             阀门图元额外使用短时稳定缓存，抑制 PLC 回读瞬态抖动。
 * - m_relayTimer (1000 ms) : 回读继电器状态并更新按钮颜色（降低 PLC 无谓轮询频率）。
 *
 * GraphicsItem 自定义数据槽（QGraphicsItem::data / setData）
 * ----------------------------------------------------------
 *   data(0) = QString  图层标签，如 "hmi_grid"、"hmi_pipe_outer"
 *   data(1) = int      压力传感器 ID，SensorItem 匹配键
 *   data(3) = int      电磁阀逻辑 ID，ElectricValveItem 匹配键
 *   data(4) = int      继电器 index，将电磁阀图元与继电器状态联动
 *   data(6) = int      调压阀 ID，RegulatingValveItem 专用匹配键
 *
 * M100 Merker 写保护机制
 * ----------------------
 * 1号台前4路电磁阀映射到 PLC M100.0~M100.3，写操作有 700 ms 防抖锁（m_lastM100ToggleMs）
 * 和"预期保持"回读校验（m_expectM100Hold），防止 PLC 联锁复位逻辑导致误报弹窗。
 */

#include "gui/Station1Panel.h"
#include "gui/ContainerGlyphRenderer.h"
#include "gui/HmiGlyphTheme.h"
#include "gui/HmiGlyphThemeUtils.h"
#include "gui/InstrumentGlyphRenderer.h"
#include "gui/PipeGlyphRenderer.h"
#include "gui/ElectricValveItem.h"
#include "gui/RegulatingValveItem.h"
#include "gui/ProcessValveGlyphRenderer.h"
#include "gui/SensorGlyphRenderer.h"
#include "gui/ValveGlyphRenderer.h"

#include "DeviceManager.h"
#include "ConfigManager.h"
#include "StationClient.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
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
#include <QScrollBar>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QTextEdit>
#include <QProgressBar>
#include <QMessageBox>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QThread>
#include <QEventLoop>
#include <QMouseEvent>
#include <QtMath>
#include <vector>
#include <array>
#include <chrono>
#include <thread>

namespace WaterTest
{
    namespace
    {
        // ===== 场景图元缩放比例 =====
        constexpr qreal kDeviceItemScale = 1.45;
        constexpr qreal kAccumulatorItemScale = 1.2;

        // ===== 管道三层渲染宽度（像素） =====
        constexpr qreal kPipeOuterWidth = 14.0;
        constexpr qreal kPipeInnerWidth = 9.0;
        constexpr qreal kPipeFlowWidth = 5.0;

        /**
         * @brief DQ 数字量输出继电器通道元信息。
         */
        struct RelayDef
        {
            uint8_t index;
            QString label;
            QString addr;
            QString type;
        };

        /** @brief 判断 relay index 是否映射到 M100.0~M100.3（Merker 位）。 */
        static bool isM100RelayIndex(uint8_t index)
        {
            return index <= 3;
        }

        /** @brief 判断 relay index 是否映射到 M100.4。 */
        static bool relayUsesM100_4(uint8_t index)
        {
            return index == 5;
        }

        /** @brief 需要同步联动到阀门图元的继电器。 */
        static bool relayNeedsGlyphUpdate(uint8_t index)
        {
            return isM100RelayIndex(index) || relayUsesM100_4(index);
        }

        // 1号操作台 DQ 输出匹配表：仅保留 5 个电磁阀。
        static const std::array<RelayDef, 5> kStation1Relays{{
            {0, "电磁阀1", "M100.0", "valve"},
            {1, "电磁阀2", "M100.1", "valve"},
            {2, "电磁阀3", "M100.2", "valve"},
            {3, "电磁阀4", "M100.3", "valve"},
            {5, "电磁阀5", "M100.4", "valve"},
        }};

        /** @brief 返回 1 号操作台的继电器通道列表。 */
        static std::vector<RelayDef> relayDefsForStation(int stationNumber)
        {
            Q_UNUSED(stationNumber);
            return std::vector<RelayDef>(kStation1Relays.begin(), kStation1Relays.end());
        }

        static QString selfCheckStepText(int stepNo)
        {
            switch (stepNo)
            {
            case 0: return QString::fromUtf8("空闲");
            case 5: return QString::fromUtf8("预检");
            case 10: return QString::fromUtf8("联通性检查");
            case 20: return QString::fromUtf8("建压");
            case 30: return QString::fromUtf8("泄漏判定");
            case 40: return QString::fromUtf8("脉冲测试");
            case 90: return QString::fromUtf8("完成");
            case 99: return QString::fromUtf8("失败");
            default: return QString::fromUtf8("未知(%1)").arg(stepNo);
            }
        }

        static QString selfCheckStatusText(const PlcSelfCheckStatus &sc)
        {
            auto realText = [](float value) { return QString::number(value, 'f', 2); };

            QString text;
            text += QString::fromUtf8("忙碌: %1\n").arg(sc.busy ? QString::fromUtf8("是") : QString::fromUtf8("否"));
            text += QString::fromUtf8("完成: %1\n").arg(sc.done ? QString::fromUtf8("是") : QString::fromUtf8("否"));
            text += QString::fromUtf8("通过: %1\n").arg(sc.passed ? QString::fromUtf8("是") : QString::fromUtf8("否"));
            text += QString::fromUtf8("失败: %1\n").arg(sc.failed ? QString::fromUtf8("是") : QString::fromUtf8("否"));
            text += QString::fromUtf8("步骤: %1 (%2)\n").arg(sc.stepNo).arg(selfCheckStepText(sc.stepNo));
            text += QString::fromUtf8("故障码: 0x%1\n").arg(QString::number(sc.faultCode, 16).rightJustified(4, '0').toUpper());
            text += QString::fromUtf8("PS4 Before/After/Build/Hold: %1 / %2 / %3 / %4\n")
                        .arg(realText(sc.ps4Before), realText(sc.ps4After), realText(sc.ps4Build), realText(sc.ps4Hold));
            text += QString::fromUtf8("PS5 Build/Hold/Delta: %1 / %2 / %3\n")
                        .arg(realText(sc.ps5Build), realText(sc.ps5Hold), realText(sc.ps5Delta));
            text += QString::fromUtf8("PS4 Delta: %1\n").arg(realText(sc.ps4Delta));

            if (sc.failed)
                text += QString::fromUtf8("\n异常: 自检失败，请查看故障码和步骤号。\n");
            else if (sc.passed)
                text += QString::fromUtf8("\n结果: 自检通过。\n");
            else if (sc.done)
                text += QString::fromUtf8("\n结果: 自检结束。\n");
            else if (!sc.online)
                text += QString::fromUtf8("\n提示: 正在等待 PLC 自检状态刷新。\n");

            return text;
        }

        static std::array<uint16_t, 2> pressureSensorsForRelayImpl(uint8_t relayIndex)
        {
            switch (relayIndex)
            {
            case 0: return {3, 4};
            case 1: return {4, 5};
            case 2: return {6, 7};
            case 3: return {7, 8};
            case 5: return {5, 8};
            default: return {0, 0};
            }
        }

        /** @brief 按 index 在列表中查找继电器定义，未找到返回 nullptr。 */
        static const RelayDef *findRelayDefByIndex(const std::vector<RelayDef> &defs, uint8_t index)
        {
            for (const auto &def : defs)
            {
                if (def.index == index)
                    return &def;
            }
            return nullptr;
        }

        /**
         * @brief 各操作台流程图中阀门图元对应的继电器 index 映射。
         */
        struct StationRelayGlyphMap
        {
            uint8_t v1;
            uint8_t v2;
            uint8_t v3;
            uint8_t test;
        };

        static StationRelayGlyphMap relayGlyphMapForStation(int stationNumber)
        {
            Q_UNUSED(stationNumber);
            return {0, 1, 3, 2};
        }

        /** @brief 格式化继电器按钮多行文字：地址 / 名称 / 通断状态。 */
        static QString formatRelayBtnText(const QString &addr, const QString &label, bool on)
        {
            const QString stateText = on ? "● 通/得电" : "○ 断/失电";
            return QString("%1\n%2\n%3").arg(addr, label, stateText);
        }

        /** @brief 读取传感器对象的显示小数位配置，超出 [0,6] 范围则用 fallbackDecimals。 */
        static int pressureDisplayDecimals(const PressureSensor &sensor, int fallbackDecimals = 1)
        {
            if (sensor.displayDecimals >= 0 && sensor.displayDecimals <= 6)
                return sensor.displayDecimals;
            return fallbackDecimals;
        }

        /**
         * @brief 读取传感器对象的显示小数位配置，超出 [0,6] 范围则用 fallbackDecimals。
         */
        // ===== 压力单位换算辅助 =====
        // 内部/网络传输单位：kPa（来自 PLC 原始值）
        // SensorItem::setValue() 直接接收 kPa 原始值，量表 0~100 对应 0~100 kPa。
        static double kPaToKgfCm2(double kpa)
        {
            return kpa;
        }

        /** @brief 将 kPa 格式化为 kPa 字符串，用于日志/Tooltip。 */
        static QString fmtPressure(double kpa, int decimals = 1)
        {
            return QString::number(kpa, 'f', decimals);
        }

        /** @brief 将 PressureSensor 对象压力格式化为带单位的 kPa 字符串（仅用于日志）。 */
        static QString fmtKPa(const PressureSensor &sensor, int fallbackDecimals = 1)
        {
            return QString::number(sensor.pressure, 'f', pressureDisplayDecimals(sensor, fallbackDecimals)) + " kPa";
        }

        /**
         * @brief 解析远程 SensorData::pressure[] 下标。
         *
         * 1号操作台远程压力数组固定为 Pressure3..Pressure8，
         * 因此按 sensorId 直接映射：3->0, 4->1, ..., 8->5。
         */
        static int resolveRemotePressureIndex(uint16_t sensorId, int fallbackIndex)
        {
            Q_UNUSED(fallbackIndex);
            if (sensorId < 3)
                return 0;
            if (sensorId > 8)
                return 5;
            return static_cast<int>(sensorId) - 3;
        }

        /**
         * @brief 本地压力读取按传感器 ID 一一对应，不做额外映射。
         */
        static uint16_t resolveLocalPressureSensorId(uint16_t sensorId)
        {
            return sensorId;
        }

        static uint16_t configuredPressureSensorId(const Station1Panel::PanelConfig &panelConfig, int psNumber)
        {
            const int configIndex = psNumber - 1;
            if (configIndex < 0 || configIndex >= static_cast<int>(panelConfig.pressureSensorIds.size()))
                return 0;
            return panelConfig.pressureSensorIds[static_cast<size_t>(configIndex)];
        }

        static std::vector<int> visiblePressureSensorNumbers(int stationNumber)
        {
            Q_UNUSED(stationNumber);
            return {3, 4, 5, 6, 7, 8};
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

        /**
         * @brief 确保 UI 颜色 token 已按当前主题初始化（仅初始化一次）。
         * 读取 config "ui.hmi.theme"（默认跟随 "ui.theme"），支持 graphite/light/ocean。
         * @return true 表示本次调用发生了主题切换，调用方可据此触发场景重绘。
         */
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

        /**
         * @brief 在场景中绘制点阵网格背景并设置渐变底色。
         * 先清除旧网格（data(0)=="hmi_grid"），再重新绘制次网格（每 100px）和主网格（每 400px）。
         */
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

    namespace
    {

        /**
         * @brief 向场景添加一段默认规格的三层管道（外壁+内壁+流动层）。
         * arrowTip/arrowFrom 保留参数，当前未绘制箭头（Q_UNUSED），流动方向由动画偏移体现。
         */
        static void addHmiPipeWithArrow(QGraphicsScene *scene,
                                        const QPainterPath &path,
                                        const QPointF &arrowTip,
                                        const QPointF &arrowFrom,
                                        int segmentId = -1)
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
                                            1,
                                            QVariant("hmi_pipe_flow"),
                                            segmentId >= 0 ? QVariant(segmentId) : QVariant());

            Q_UNUSED(arrowTip);
            Q_UNUSED(arrowFrom);
        }

        /**
         * @brief 向场景添加自定义规格的三层管道，用于需要更粗管径的主干段（如供压总管）。
         */
        static void addHmiPipeWithArrowCustom(QGraphicsScene *scene,
                                              const QPainterPath &path,
                                              const QPointF &arrowTip,
                                              const QPointF &arrowFrom,
                                              qreal outerWidth,
                                              qreal innerWidth,
                                              qreal flowWidth,
                                              int segmentId = -1)
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
                                            1,
                                            QVariant("hmi_pipe_flow"),
                                            segmentId >= 0 ? QVariant(segmentId) : QVariant());

            Q_UNUSED(arrowTip);
            Q_UNUSED(arrowFrom);
        }

        // ========== Station1 的图标化拟物设备图元（与 PreparationPanel 同风格） ==========
        // 所有 Item 类均内联在匿名 namespace 中，不对外暴露。
        // 渲染逻辑委托给 GuiGlyph 命名空间下对应的 draw*Glyph 函数。

        /**
         * @brief 蓄能器图元。
         * 端口：inletPortLocal()=上端进液口，outletPortLocal()=下端出液口，
         *        returnPortLocal()=侧面回液口（可选，由 showReturnPort 控制）。
         */
        class AccumulatorItem : public QGraphicsItem
        {
        public:
            static QPointF outletPortLocal() { return GuiGlyph::accumulatorOutletPortLocal(); }

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

        /**
         * @brief 遍历场景，将 data(4)==relayIndex 的电磁阀图元设置为开/关状态。
         * 由 updateRelayButtons() 和 onRelayBtnClicked() 在继电器状态变化时调用（乐观更新）。
         */
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

                auto *valve = dynamic_cast<ElectricValveItem *>(it);
                if (!valve)
                    continue;

                valve->setOpen(on);
                valve->setDegree(on ? 100.0 : 0.0);
            }
        }

        /**
         * @brief 三通阀图元（一进两出）。
         * 端口：inletPortLocal()=进口，outletPortLocal()=主出口，branchPortLocal()=分支口。
         */
        class ThreeWayValveItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return GuiGlyph::threeWayValveInletPortLocal(); }
            static QPointF outletPortLocal() { return GuiGlyph::threeWayValveOutletPortLocal(); }

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

        /**
         * @brief 压力传感器图元。
         *
         * data(1) = 传感器 ID，updateSensorValues() 用此键定位图元并推送压力值。
         * setValue() 接收原始 kPa 值，量表 0~100 对应 0~100 kPa。
         */
        class SensorItem : public QGraphicsItem
        {
        public:
            static QPointF inletPortLocal() { return GuiGlyph::sensorInletPortLocal(); }

            explicit SensorItem(const QString &name, const QString &unit = "kPa", QColor typeColor = QColor())
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

            void setAuxText(const QString &text, const QColor &color = QColor())
            {
                if (m_auxText == text && m_auxColor == color)
                    return;
                m_auxText = text;
                m_auxColor = color;
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
                    makeGlyphTheme(),
                    m_auxText,
                    m_auxColor);
            }

        private:
            QString m_name;
            double m_value;
            int m_displayDecimals = 2;
            QString m_unit;
            QColor m_typeColor;
            QString m_auxText;
            QColor m_auxColor;
        };

        /**
         * @brief 流量计图元，显示瞬时流量和报警状态。
         * setAlarmDetail() 接收 emptyPipeAlarm（空管报警）和 excitationAlarm（励磁报警）。
         */
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

        class TickedSlider : public QSlider
        {
        public:
            using QSlider::QSlider;

        protected:
            void paintEvent(QPaintEvent *event) override
            {
                QSlider::paintEvent(event);

                if (orientation() != Qt::Horizontal)
                    return;

                const int minimumValue = minimum();
                const int maximumValue = maximum();
                const int range = maximumValue - minimumValue;
                if (range <= 0)
                    return;

                constexpr int kMinorStep = 50;   // 5.0%
                constexpr int kMajorStep = 100;  // 10.0%

                QStyleOptionSlider opt;
                initStyleOption(&opt);

                const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
                if (!groove.isValid())
                    return;

                QPainter painter(this);
                painter.setRenderHint(QPainter::Antialiasing, false);

                QPen pen(palette().color(QPalette::Text));
                pen.setWidth(1);
                painter.setPen(pen);

                const int tickTop = qMin(height() - 10, groove.bottom() + 6);
                const int shortTickBottom = qMin(height() - 6, tickTop + 4);
                const int longTickBottom = qMin(height() - 4, tickTop + 8);

                for (int value = minimumValue; value <= maximumValue; value += kMinorStep)
                {
                    const qreal progress = static_cast<qreal>(value - minimumValue) / static_cast<qreal>(range);
                    const int x = groove.left() + qRound(progress * groove.width());
                    const bool majorTick = (value == minimumValue) || (value == maximumValue) || (((value - minimumValue) % kMajorStep) == 0);
                    painter.drawLine(QPointF(x, tickTop), QPointF(x, majorTick ? longTickBottom : shortTickBottom));
                }
            }
        };

        /**
         * @brief 回路节点图元（三口：进/出/底部旁通），用于 2/3 号台的回路汇合点。
         */
        class LoopItem : public QGraphicsItem
        {
        public:
            static QPointF outletPortLocal() { return GuiGlyph::loopOutletPortLocal(); }

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

        /**
         * @brief 连接两个设备端口（场景坐标），自动选择横平竖直走线规则：
         *   - 同行（|dy|<=20）：水平主干 + 末端短竖线
         *   - 同列（|dx|<=20）：竖直主干 + 末端短横线
         *   - 跨行跨列：中间水平走线（取起终点 Y 均值）
         */
        static void connectPorts(QGraphicsScene *scene, const QPointF &start, const QPointF &end, int segmentId = -1)
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
                addHmiPipeWithArrow(scene, path, end, corner, segmentId);
                return;
            }

            // 规则2：同列连接（x 接近）走“竖直主干 + 末端短横线”。
            if (qAbs(dx) <= 20.0)
            {
                const QPointF corner(start.x(), end.y());
                path.lineTo(corner);
                path.lineTo(end);
                addHmiPipeWithArrow(scene, path, end, corner, segmentId);
                return;
            }

            // 规则3：跨排连接走中间水平走线，保证横平竖直。
            const qreal midY = (start.y() + end.y()) * 0.5;
            path.lineTo(QPointF(start.x(), midY));
            path.lineTo(QPointF(end.x(), midY));
            path.lineTo(end);

            addHmiPipeWithArrow(scene, path, end, QPointF(end.x(), midY), segmentId);
        }
    }

    } // namespace

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
          m_selfCheckStatusPollTimer(nullptr),
          m_flowDashOffset(0.0)
    {
        setupUI();
    }

    Station1Panel::~Station1Panel() = default;

    void Station1Panel::setStationClient(std::shared_ptr<StationClient> stationClient)
    {
        m_stationClient = stationClient;

        const bool plcConnected = m_deviceManager && m_deviceManager->isPlcConnected();
        if (m_selfCheckBtn)
        {
            m_selfCheckBtn->setEnabled(plcConnected);
            m_selfCheckBtn->setToolTip(plcConnected ? QString() : QString::fromUtf8("请先连接 PLC 后再执行自检"));
        }

        if (m_stationClient)
        {
            QObject::connect(m_stationClient.get(), &StationClient::connected, this, [this]() {
                if (m_selfCheckBtn)
                {
                    const bool plcConnected = m_deviceManager && m_deviceManager->isPlcConnected();
                    m_selfCheckBtn->setEnabled(plcConnected);
                    m_selfCheckBtn->setToolTip(plcConnected ? QString() : QString::fromUtf8("请先连接 PLC 后再执行自检"));
                }
            });
            QObject::connect(m_stationClient.get(), &StationClient::disconnected, this, [this]() {
                if (m_selfCheckBtn)
                {
                    const bool plcConnected = m_deviceManager && m_deviceManager->isPlcConnected();
                    m_selfCheckBtn->setEnabled(plcConnected);
                    m_selfCheckBtn->setToolTip(plcConnected ? QString() : QString::fromUtf8("请先连接 PLC 后再执行自检"));
                }
            });
        }
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

    /**
     * @brief 初始化 UI 布局、QGraphicsView、三条定时器，并构建流程图场景。
     *
     * 布局结构：
     *   QVBoxLayout
     *     QHBoxLayout (工具栏：开始/停止/系统自检按钮)
     *     QGraphicsView (占满剩余空间，填充场景)
     *
     * 关键连接：
     * - m_scene::selectionChanged -> 阀门点击弹出控制对话框
     * - m_flowTimer  -> updatePipeFlowAnimation()
     * - m_dataTimer  -> updateSensorValues()
     * - m_relayTimer -> updateRelayButtons()
     */
    // ===== UI 构建层 =====
    // 这里负责窗口、工具栏、QGraphicsView/QGraphicsScene 和定时器初始化。
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

        // 仅在工具栏追加两个批量按钮，不改变中间工艺图区域的布局。
        auto *openAllBtn = new QPushButton("全部打开", this);
        openAllBtn->setMinimumHeight(34);
        openAllBtn->setProperty("tone", "accent");
        connect(openAllBtn, &QPushButton::clicked, this, [this]() {
            controlAllRelayValves(true, "toolbar_all_open");
        });
        toolbarLayout->addWidget(openAllBtn);

        auto *closeAllBtn = new QPushButton("全部关闭", this);
        closeAllBtn->setMinimumHeight(34);
        closeAllBtn->setProperty("tone", "bad");
        connect(closeAllBtn, &QPushButton::clicked, this, [this]() {
            controlAllRelayValves(false, "toolbar_all_close");
        });
        toolbarLayout->addWidget(closeAllBtn);

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

        // 预留下半部分的测试阶段区：固定高度约 220px，用于展示 5 个阶段及其参数。
        // 上半部分流程图保留为主视觉，下半部分用于阶段化测试信息。
        m_stageOverviewGroup = new QGroupBox("测试阶段概览", this);
        m_stageOverviewGroup->setMinimumHeight(208);
        m_stageOverviewGroup->setMaximumHeight(208);
        m_stageOverviewGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // m_stageOverviewGroup->setStyleSheet(
        //     "QGroupBox {"
        //     " font-weight: 700;"
        //     " color: #21303d;"
        //     " border: 1px solid #d1dae4;"
        //     " border-radius: 14px;"
        //     " margin-top: 14px;"
        //     " background: #f6f8fb;"
        //     " }"
        //     "QGroupBox::title {"
        //     " subcontrol-origin: margin;"
        //     " left: 12px;"
        //     " padding: 0 6px;"
        //     " }"
        // );
        setupStageOverview(m_stageOverviewGroup);

        m_scene = new QGraphicsScene(this);
        m_view->setScene(m_scene);
        m_view->viewport()->installEventFilter(this);
        layout->addWidget(m_view, 1);

        layout->addWidget(m_stageOverviewGroup, 0);

        m_flowTimer = new QTimer(this);
        connect(m_flowTimer, &QTimer::timeout, this, &Station1Panel::updatePipeFlowAnimation);
        m_flowTimer->start(1000);

        m_dataTimer = new QTimer(this);
        connect(m_dataTimer, &QTimer::timeout, this, [this]() { updateSensorValues(); });
        m_dataTimer->start(100);

        // 继电器状态刷新：1 秒一次（降低无意义的 Q 区轮询频率）
        m_relayTimer = new QTimer(this);
        connect(m_relayTimer, &QTimer::timeout, this, [this]() { updateRelayButtons(); });
        m_relayTimer->start(1000);

        // 自检状态轮询：仅在 PLC 自检运行期间启用。
        m_selfCheckStatusPollTimer = new QTimer(this);
        m_selfCheckStatusPollTimer->setInterval(250);
        connect(m_selfCheckStatusPollTimer, &QTimer::timeout, this, [this]() {
            if (!m_deviceManager)
                return;
            m_deviceManager->refreshPlcSelfCheckStatus();
            updateRelayButtons(true);
            updateSensorValues(true);
            const auto sc = m_deviceManager->getPlcSelfCheckStatus();
            if (sc.done || sc.passed || sc.failed)
                stopSelfCheckStatusPolling();
        });

        buildScene();
        updateRelayButtons();
        setActiveStageIndex(-1);

        // 点击阀门图元后弹出控制面板，避免误触：选中后立即清除选中态
        connect(m_scene, &QGraphicsScene::selectionChanged, this, [this]()
                {
            if (!m_scene)
                return;

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

            auto *valveItem = dynamic_cast<ElectricValveItem *>(item);
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
                dialog.resize(420, 280);
                dialog.setMinimumSize(420, 280);
                dialog.setStyleSheet(
                    "QDialog { background: #0e1622; }"
                    "QFrame#regValveCard {"
                    " background: #162231;"
                    " border: 1px solid #2a4056;"
                    " border-radius: 16px;"
                    " }"
                    "QLabel#regValveTitle {"
                    " color: #f2f6fb;"
                    " font-size: 16px;"
                    " font-weight: 700;"
                    " }"
                    "QLabel#regValveHint {"
                    " color: #9eafbf;"
                    " font-size: 12px;"
                    " }"
                    "QLabel#regValveSection {"
                    " color: #d7e2ee;"
                    " font-size: 12px;"
                    " font-weight: 600;"
                    " }"
                    "QLabel#regValveFieldLabel {"
                    " color: #d7e2ee;"
                    " font-size: 13px;"
                    " }"
                    "QDoubleSpinBox {"
                    " color: #eef4ff;"
                    " background: #0f1a27;"
                    " border: 1px solid #33485d;"
                    " border-radius: 10px;"
                    " padding: 4px 10px;"
                    " min-height: 30px;"
                    " }"
                    "QDoubleSpinBox:focus {"
                    " border-color: #4cc3a0;"
                    " }"
                    "QPushButton {"
                    " min-height: 34px;"
                    " min-width: 96px;"
                    " border-radius: 10px;"
                    " padding: 0 16px;"
                    " }"
                    "QPushButton#regValveApply {"
                    " color: #ffffff;"
                    " background: #2e8b57;"
                    " border: 1px solid #2f9f57;"
                    " }"
                    "QPushButton#regValveApply:hover { background: #349763; }"
                    "QPushButton#regValveCancel {"
                    " color: #dbe6f2;"
                    " background: #203046;"
                    " border: 1px solid #31455d;"
                    " }"
                    "QPushButton#regValveCancel:hover { background: #26384f; }");

                auto *rootLayout = new QVBoxLayout(&dialog);
                rootLayout->setContentsMargins(16, 16, 16, 16);
                rootLayout->setSpacing(12);

                auto *titleLabel = new QLabel(valveName, &dialog); // "调压阀开度控制"
                titleLabel->setObjectName("regValveTitle");
                rootLayout->addWidget(titleLabel);

                auto *hintLabel = new QLabel(QString("当前开度: %1%")                                                 
                                                 .arg(QString::number(currentOpening, 'f', 1)),
                                             &dialog);
                hintLabel->setObjectName("regValveHint");
                hintLabel->setWordWrap(true);
                rootLayout->addWidget(hintLabel);

                auto *card = new QFrame(&dialog);
                card->setObjectName("regValveCard");
                auto *cardLayout = new QVBoxLayout(card);
                cardLayout->setContentsMargins(16, 14, 16, 14);
                cardLayout->setSpacing(12);

                // auto *targetHeader = new QLabel("目标开度", card);
                // targetHeader->setObjectName("regValveSection");
                // cardLayout->addWidget(targetHeader);

                auto *targetRow = new QWidget(card);
                auto *targetRowLayout = new QHBoxLayout(targetRow);
                targetRowLayout->setContentsMargins(0, 0, 0, 0);
                // targetRowLayout->setSpacing(12);
                auto *targetLabel = new QLabel("开度数值（%）", targetRow);
                targetLabel->setObjectName("regValveFieldLabel");
                targetLabel->setMinimumWidth(72);
                targetLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
                auto *openingSpinBox = new QDoubleSpinBox(targetRow);
                openingSpinBox->setRange(0.0, 100.0);
                openingSpinBox->setDecimals(1);
                openingSpinBox->setSingleStep(0.5);
                openingSpinBox->setValue(currentOpening);
                // openingSpinBox->setSuffix(" %");
                openingSpinBox->setAlignment(Qt::AlignCenter);
                openingSpinBox->setMinimumWidth(180);
                openingSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                // openingSpinBox->setFixedHeight(20);
                targetRowLayout->addWidget(targetLabel);
                targetRowLayout->addWidget(openingSpinBox, 1);
                targetRowLayout->addStretch();
                cardLayout->addWidget(targetRow);

                // auto *sliderHeader = new QHBoxLayout();
                // auto *sliderLabel = new QLabel("滑块调节", card);
                // sliderLabel->setObjectName("regValveSection");
                // sliderHeader->addWidget(sliderLabel);
                // sliderHeader->addStretch();
                // cardLayout->addLayout(sliderHeader);

                auto *slider = new TickedSlider(Qt::Horizontal, card);
                slider->setRange(0, 1000);
                slider->setSingleStep(50);
                slider->setPageStep(100);
                slider->setValue(qRound(currentOpening * 10.0f));
                slider->setMinimumHeight(56);
                slider->setTickPosition(QSlider::NoTicks);
                slider->setTickInterval(50);
                slider->setStyleSheet(
                    "QSlider::groove:horizontal {"
                    " height: 14px;"
                    " border-radius: 7px;"
                    " background: #314457;"
                    " }"
                    "QSlider::sub-page:horizontal {"
                    " background: #4cc3a0;"
                    " border-radius: 7px;"
                    " }"
                    "QSlider::add-page:horizontal {"
                    " background: #5b6f84;"
                    " border-radius: 7px;"
                    " }"
                    "QSlider::handle:horizontal {"
                    " width: 24px;"
                    " margin: -8px 0;"
                    " border-radius: 12px;"
                    " background: #eaf2ff;"
                    " border: 1px solid #8fa3b8;"
                    " }");
                cardLayout->addWidget(slider);

                auto *buttons = new QHBoxLayout();
                buttons->addStretch();
                buttons->setSpacing(12);
                auto *applyBtn = new QPushButton("设定开度", &dialog);
                applyBtn->setObjectName("regValveApply");
                auto *cancelBtn = new QPushButton("取消", &dialog);
                cancelBtn->setObjectName("regValveCancel");
                buttons->addWidget(applyBtn);
                buttons->addWidget(cancelBtn);
                cardLayout->addLayout(buttons);

                rootLayout->addWidget(card);

                connect(slider, &QSlider::valueChanged, &dialog, [openingSpinBox](int value) {
                    const double targetOpening = static_cast<double>(value) / 10.0;
                    if (!qFuzzyCompare(openingSpinBox->value(), targetOpening))
                    {
                        openingSpinBox->blockSignals(true);
                        openingSpinBox->setValue(targetOpening);
                        openingSpinBox->blockSignals(false);
                    }
                });
                connect(openingSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dialog, [slider](double value) {
                    const int targetValue = qRound(value * 10.0);
                    if (slider->value() != targetValue)
                    {
                        slider->setValue(targetValue);
                    }
                });

                connect(applyBtn, &QPushButton::clicked, &dialog, [&, regulatingValveId]() {
                    if (m_stationClient && m_stationClient->isConnected())
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
                            static_cast<float>(openingSpinBox->value()));
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
                bool ok = false;
                if (m_stationClient && m_stationClient->isConnected())
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
                bool ok = false;
                if (m_stationClient && m_stationClient->isConnected())
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

    void Station1Panel::setupStageOverview(QWidget *parent)
    {
        auto *mainLayout = new QVBoxLayout(parent);
        mainLayout->setContentsMargins(10, 8, 10, 10);
        mainLayout->setSpacing(8);

        auto *grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(0);

        const std::array<QString, 5> stageNames{{
            QString::fromUtf8("开阀测试"),
            QString::fromUtf8("低压内泄露"),
            QString::fromUtf8("高压泄露"),
            QString::fromUtf8("流量测试"),
            QString::fromUtf8("高压外泄漏")
        }};

        const std::array<std::array<QString, 3>, 5> stageRowLabels{{
            std::array<QString, 3>{{QString::fromUtf8("调压阀等待"), QString::fromUtf8("前置阀等待"), QString::fromUtf8("PS7压差")}},
            std::array<QString, 3>{{QString::fromUtf8("开阀时间"), QString::fromUtf8("关阀时间"), QString::fromUtf8("压力升高")}},
            std::array<QString, 3>{{QString::fromUtf8("阶段进度"), QString::fromUtf8("PS6/PS4监控"), QString::fromUtf8("泄露判定")}},
            std::array<QString, 3>{{QString::fromUtf8("阀门预开"), QString::fromUtf8("调压阀稳定"), QString::fromUtf8("阶段结果")}},
            std::array<QString, 3>{{QString::fromUtf8("压力升降"), QString::fromUtf8("测试次数"), QString::fromUtf8("泄露值")}}
        }};

        const std::array<std::array<QString, 3>, 5> stageRowValues{{
            std::array<QString, 3>{{QString::fromUtf8("--"), QString::fromUtf8("--"), QString::fromUtf8("--")}},
            std::array<QString, 3>{{QString::fromUtf8("--"), QString::fromUtf8("--"), QString::fromUtf8("--")}},
            std::array<QString, 3>{{QString::fromUtf8("--"), QString::fromUtf8("--"), QString::fromUtf8("--")}},
            std::array<QString, 3>{{QString::fromUtf8("--"), QString::fromUtf8("--"), QString::fromUtf8("--")}},
            std::array<QString, 3>{{QString::fromUtf8("--"), QString::fromUtf8("--"), QString::fromUtf8("--")}}
        }};

        auto makeRow = [](QFrame *parentFrame, const QString &labelText, const QString &valueText, bool active) {
            auto *rowFrame = new QFrame(parentFrame);
            rowFrame->setObjectName("stageRow");
            rowFrame->setFixedHeight(30);
            rowFrame->setStyleSheet(QString(
                                          "QFrame#stageRow {"
                                          " border: 1px solid %1;"
                                          " border-radius: 8px;"
                                          " background: #fcfdff;"
                                          " }")
                                          .arg(active ? QStringLiteral("#c7e4d0") : QStringLiteral("#e3e9ef")));

            auto *rowLayout = new QHBoxLayout(rowFrame);
            rowLayout->setContentsMargins(10, 0, 10, 0);
            rowLayout->setSpacing(6);

            auto *label = new QLabel(labelText, rowFrame);
            label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            label->setStyleSheet(QString(
                                     "QLabel {"
                                     " color: %1;"
                                     " font-size: 11px;"
                                     " font-weight: 600;"
                                     " background-color: #fcfdff;"
                                     " }")
                                     .arg(active ? QStringLiteral("#24463a") : QStringLiteral("#5c6d7d")));

            auto *value = new QLabel(valueText, rowFrame);
            value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            value->setStyleSheet(QString(
                                     "QLabel {"
                                     " color: %1;"
                                     " font-size: 11px;"
                                     " font-weight: 700;"
                                     " background-color: #fcfdff;"
                                     " }")
                                     .arg(active ? QStringLiteral("#2b8a4b") : QStringLiteral("#263441")));

            rowLayout->addWidget(label, 1);
            rowLayout->addWidget(value, 1);

            return std::pair<QFrame *, QLabel *>{rowFrame, value};
        };

        for (int i = 0; i < 5; ++i)
        {
            auto *card = new QFrame(parent);
            card->setObjectName("stageCard");
            card->setFrameShape(QFrame::NoFrame);
            card->setFrameShadow(QFrame::Plain);
            card->setMinimumHeight(170);
            card->setStyleSheet(
                "QFrame#stageCard {"
                " border: 1px solid #dbe3ea;"
                " border-radius: 16px;"
                " background: #fcfdff;"
                " }");

            auto *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(10, 8, 10, 10);
            cardLayout->setSpacing(6);

            const bool isActiveStage = (i == m_activeStageIndex);
            const QString headerColor = isActiveStage ? QStringLiteral("#2f9f57") : QStringLiteral("#33495c");

            auto *statusBar = new QFrame(card);
            statusBar->setFixedHeight(4);
            statusBar->setStyleSheet(QStringLiteral("QFrame { background: %1; border: none; border-radius: 2px; }").arg(headerColor));

            auto *nameLabel = new QLabel(stageNames[static_cast<size_t>(i)], card);
            nameLabel->setObjectName("stageName");
            nameLabel->setFixedHeight(28);
            nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            nameLabel->setStyleSheet(QString(
                                         "QLabel#stageName {"
                                         " color: #f8fbff;"
                                         " font-weight: 700;"
                                         " font-size: 12px;"
                                         " background: %1;"
                                         " border-radius: 8px;"
                                         " padding-left: 10px;"
                                         " }")
                                         .arg(headerColor));

            auto *bodyFrame = new QFrame(card);
            bodyFrame->setObjectName("stageBody");
            bodyFrame->setFrameShape(QFrame::NoFrame);
            bodyFrame->setFrameShadow(QFrame::Plain);
            bodyFrame->setStyleSheet("QFrame#stageBody { border: 0px; background: #fcfdff; }");

            auto *bodyLayout = new QVBoxLayout(bodyFrame);
            bodyLayout->setContentsMargins(2, 0, 2, 0);
            bodyLayout->setSpacing(4);

            auto *paramFrame = new QFrame(bodyFrame);
            auto *paramLayout = new QVBoxLayout(paramFrame);
            paramLayout->setContentsMargins(0, 0, 0, 0);
            paramLayout->setSpacing(4);

            for (int row = 0; row < 3; ++row)
            {
                const auto rowPair = makeRow(paramFrame,
                                             stageRowLabels[static_cast<size_t>(i)][static_cast<size_t>(row)],
                                             stageRowValues[static_cast<size_t>(i)][static_cast<size_t>(row)],
                                             isActiveStage);
                paramLayout->addWidget(rowPair.first);
                m_stageRowLabelLabels[static_cast<size_t>(i)][static_cast<size_t>(row)] = rowPair.first->findChild<QLabel *>();
                m_stageRowFrames[static_cast<size_t>(i)][static_cast<size_t>(row)] = rowPair.first;
                m_stageValueLabels[static_cast<size_t>(i)][static_cast<size_t>(row)] = rowPair.second;
            }

            paramLayout->addStretch(1);
            bodyLayout->addWidget(paramFrame);

            cardLayout->addWidget(statusBar);
            cardLayout->addWidget(nameLabel);
            cardLayout->addWidget(bodyFrame, 1);

            m_stageCardFrames[static_cast<size_t>(i)] = card;
            m_stageNameLabels[static_cast<size_t>(i)] = nameLabel;
            m_stageParamLabels[static_cast<size_t>(i)] = m_stageValueLabels[static_cast<size_t>(i)][0];

            grid->addWidget(card, 0, i);
        }

        for (int col = 0; col < 5; ++col)
            grid->setColumnStretch(col, 1);

        mainLayout->addLayout(grid);
    }

    void Station1Panel::setActiveStageIndex(int stageIndex)
    {
        if (stageIndex >= static_cast<int>(m_stageNameLabels.size()))
            return;

        m_activeStageIndex = stageIndex;

        for (int i = 0; i < static_cast<int>(m_stageNameLabels.size()); ++i)
        {
            auto *card = m_stageCardFrames[static_cast<size_t>(i)];
            auto *nameLabel = m_stageNameLabels[static_cast<size_t>(i)];
            if (!card || !nameLabel)
                continue;

            const bool isActive = (i == m_activeStageIndex);
            const QString headerColor = isActive ? QStringLiteral("#2ea84f") : QStringLiteral("#2d3c4a");

            card->setStyleSheet(QString(
                                    "QFrame#stageCard {"
                                    " border: %1px solid %2;"
                                    " border-radius: 16px;"
                                    " background: %3;"
                                    " }")
                                    .arg(isActive ? 2 : 1)
                                    .arg(isActive ? QStringLiteral("#2f9f57") : QStringLiteral("#dbe3ea"))
                                    .arg(isActive ? QStringLiteral("#eefaf2") : QStringLiteral("#fcfdff")));

            nameLabel->setStyleSheet(QString(
                                         "QLabel#stageName {"
                                         " color: #f7fbff;"
                                         " font-weight: 700;"
                                         " font-size: 12px;"
                                         " background: %1;"
                                         " border-radius: 8px;"
                                         " padding-left: 10px;"
                                         " }")
                                         .arg(headerColor));

            for (int row = 0; row < 3; ++row)
            {
                auto *rowFrame = m_stageRowFrames[static_cast<size_t>(i)][static_cast<size_t>(row)];
                auto *rowLabel = m_stageRowLabelLabels[static_cast<size_t>(i)][static_cast<size_t>(row)];
                auto *valueLabel = m_stageValueLabels[static_cast<size_t>(i)][static_cast<size_t>(row)];
                if (!rowFrame || !rowLabel || !valueLabel)
                    continue;

                const QString rowBg = isActive ? QStringLiteral("#f2fbf5") : QStringLiteral("#fcfdff");
                rowFrame->setStyleSheet(QString(
                                            "QFrame#stageRow {"
                                            " border: 1px solid %1;"
                                            " border-radius: 8px;"
                                            " background: #fcfdff;"
                                            " }")
                                            .arg(isActive ? QStringLiteral("#c7e4d0") : QStringLiteral("#e3e9ef")));

                rowLabel->setStyleSheet(QString(
                                            "QLabel {"
                                            " color: %1;"
                                            " font-size: 11px;"
                                            " font-weight: 600;"
                                            " background-color: %2;"
                                            " }")
                                            .arg(isActive ? QStringLiteral("#24463a") : QStringLiteral("#5c6d7d"))
                                            .arg(rowBg));

                valueLabel->setStyleSheet(QString(
                                              "QLabel {"
                                              " color: %1;"
                                              " font-size: 11px;"
                                              " font-weight: 700;"
                                              " background-color: %2;"
                                              " }")
                                              .arg(isActive ? QStringLiteral("#2b8a4b") : QStringLiteral("#263441"))
                                              .arg(rowBg));
            }
        }
    }

    void Station1Panel::setStageOverviewState(int stageIndex, const QString &title, const QString &detail, bool ok)
    {
        if (stageIndex < 0 || stageIndex >= static_cast<int>(m_stageNameLabels.size()))
            return;

        setActiveStageIndex(stageIndex);

        auto *nameLabel = m_stageNameLabels[static_cast<size_t>(stageIndex)];
        if (nameLabel)
        {
            const QString headerColor = ok ? QStringLiteral("#2ea84f") : QStringLiteral("#c24b45");
            nameLabel->setText(title.isEmpty() ? nameLabel->text() : title);
            nameLabel->setStyleSheet(QString(
                                         "QLabel#stageName {"
                                         " color: #f7fbff;"
                                         " font-weight: 700;"
                                         " font-size: 12px;"
                                         " background: %1;"
                                         " border-radius: 8px;"
                                         " padding-left: 10px;"
                                         " }")
                                         .arg(headerColor));
        }

        auto *valueLabel = m_stageValueLabels[static_cast<size_t>(stageIndex)][0];
        if (valueLabel)
            valueLabel->setText(detail);
    }

    void Station1Panel::appendStageOverviewIssue(int stageIndex, const QString &issueText)
    {
        if (stageIndex < 0 || stageIndex >= static_cast<int>(m_stageNameLabels.size()))
            return;

        auto *valueLabel = m_stageValueLabels[static_cast<size_t>(stageIndex)][2];
        if (!valueLabel)
            return;

        const QString current = valueLabel->text().trimmed();
        if (current.isEmpty() || current == "--")
            valueLabel->setText(issueText);
        else
            valueLabel->setText(current + QStringLiteral("\n") + issueText);

        valueLabel->setStyleSheet(QString(
                                      "QLabel {"
                                      " color: #c24b45;"
                                      " font-size: 11px;"
                                      " font-weight: 700;"
                                      " background-color: #fcfdff;"
                                      " }")
                                      );
    }

    void Station1Panel::setRealtimeUpdatesEnabled(bool enabled)
    {
        if (m_flowTimer)
        {
            if (enabled)
                m_flowTimer->start(1000);
            else
                m_flowTimer->stop();
        }

        if (m_dataTimer)
        {
            if (enabled)
                m_dataTimer->start(100);
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

    void Station1Panel::startSelfCheckStatusPolling()
    {
        if (!m_selfCheckStatusPollTimer || !m_deviceManager)
            return;

        m_deviceManager->refreshPlcSelfCheckStatus();
        if (!m_selfCheckStatusPollTimer->isActive())
            m_selfCheckStatusPollTimer->start();
    }

    void Station1Panel::stopSelfCheckStatusPolling()
    {
        if (m_selfCheckStatusPollTimer && m_selfCheckStatusPollTimer->isActive())
            m_selfCheckStatusPollTimer->stop();
    }

    bool Station1Panel::readPressureValueForDisplay(uint16_t configuredSensorId, size_t fallbackIndex, double &pressureKpa) const
    {
        if (!m_deviceManager)
            return false;

        if (m_stationClient && m_stationClient->isConnected())
        {
            if (m_deviceManager->isPlcConnected())
            {
                const auto sensor = m_deviceManager->getPressureSensor(resolveLocalPressureSensorId(configuredSensorId));
                if (sensor.id != 0)
                {
                    pressureKpa = static_cast<double>(sensor.pressure);
                    return true;
                }
            }

            const SensorData net = m_stationClient->getLatestSensorData();
            const int remotePressureIndex = std::clamp(resolveRemotePressureIndex(configuredSensorId, static_cast<int>(fallbackIndex)), 0, 5);
            pressureKpa = static_cast<double>(net.pressure[remotePressureIndex]);
            return true;
        }

        if (!m_deviceManager->isPlcConnected())
            return false;

        if (!m_deviceManager)
            return false;

        const auto sensor = m_deviceManager->getPressureSensor(resolveLocalPressureSensorId(configuredSensorId));
        if (sensor.id == 0)
            return false;

        pressureKpa = static_cast<double>(sensor.pressure);
        return true;
    }

    std::array<uint16_t, 2> Station1Panel::pressureSensorsForRelay(uint8_t relayIndex)
    {
        return pressureSensorsForRelayImpl(relayIndex);
    }

    QString Station1Panel::pressureCloseDeltaText(uint8_t relayIndex, uint16_t sensorId, double currentPressureKpa, int decimals, QColor *color) const
    {
        if (relayIndex >= m_pressureCloseBaselineValid.size())
            return QString();

        const auto sensors = pressureSensorsForRelay(relayIndex);
        int slot = -1;
        if (sensorId == sensors[0])
            slot = 0;
        else if (sensorId == sensors[1])
            slot = 1;

        if (slot < 0 || !m_pressureCloseBaselineValid[relayIndex][static_cast<size_t>(slot)])
            return QString();

        const double delta = currentPressureKpa - m_pressureCloseBaseline[relayIndex][static_cast<size_t>(slot)];
        if (color)
            *color = (delta >= 0.0) ? QColor("#2f9f57") : QColor("#cf5a46");

        const QString arrow = (delta >= 0.0) ? QString::fromUtf8("↑") : QString::fromUtf8("↓");
        return QString::fromUtf8("%1 %2 kPa").arg(arrow, QString::number(std::abs(delta), 'f', decimals));
    }

    void Station1Panel::capturePressureCloseBaseline(uint8_t relayIndex)
    {
        if (relayIndex >= m_pressureCloseBaseline.size())
            return;

        const auto sensors = pressureSensorsForRelay(relayIndex);
        for (size_t slot = 0; slot < sensors.size(); ++slot)
        {
            const uint16_t configuredSensorId = sensors[slot];
            if (configuredSensorId == 0 || configuredSensorId >= m_pressureCloseBaseline.size())
                continue;

            double pressureKpa = 0.0;
            if (!readPressureValueForDisplay(configuredSensorId, slot, pressureKpa))
                continue;

            m_pressureCloseBaseline[relayIndex][slot] = pressureKpa;
            m_pressureCloseBaselineValid[relayIndex][slot] = true;
        }
    }

    void Station1Panel::clearPressureCloseBaseline(uint8_t relayIndex)
    {
        if (relayIndex >= m_pressureCloseBaseline.size())
            return;
        m_pressureCloseBaseline[relayIndex].fill(0.0);
        m_pressureCloseBaselineValid[relayIndex].fill(false);
    }

    void Station1Panel::clearAllPressureCloseBaselines()
    {
        for (size_t i = 0; i < m_pressureCloseBaseline.size(); ++i)
            clearPressureCloseBaseline(static_cast<uint8_t>(i));
    }

    /**
     * @brief 在给定容器 Widget 内构建"继电器输出控制 (DQ)"面板。
     *
     * 按钮网格：最多 5 列，按 visibleBtnCount 顺序排列。
     * 1号台的 relay0（电磁阀1）隐藏按钮，由流程图图元直接控制。
     * 泵类继电器按钮（tone=warn）仅展示状态，不响应点击切换。
     * 构建后将按钮指针存入 m_relayBtns[index]，供 updateRelayButtons() 更新文字和颜色。
     */
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

    bool Station1Panel::controlRelayState(uint8_t index, bool on, const char *source, bool scheduleReconcile)
    {
        if (!m_deviceManager || !m_deviceManager->isPlcConnected())
        {
            qWarning() << "[M100][Station1Panel] relay request rejected because PLC is disconnected"
                       << "src=" << source
                       << "index=" << index
                       << "target=" << on;
            return false;
        }

        const QString sourceText = QString::fromUtf8(source ? source : "");
        if (!sourceText.startsWith("selfcheck"))
        {
            const auto sc = m_deviceManager->getPlcSelfCheckStatus();
            if (sc.busy)
            {
                qWarning() << "[SelfCheck][Station1Panel] relay control blocked while self-check busy"
                           << "src=" << source
                           << "index=" << index
                           << "target=" << on;
                return false;
            }
        }

        if (isM100RelayIndex(index))
        {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (m_lastM100ToggleMs[index] > 0 && (nowMs - m_lastM100ToggleMs[index]) < 700)
            {
                qWarning() << "[M100][Station1Panel] relay toggle ignored by M100 guard"
                           << "index=" << index
                           << "elapsedMs=" << (nowMs - m_lastM100ToggleMs[index]);
                return false;
            }
            m_lastM100ToggleMs[index] = nowMs;
        }

        const auto relayDefs = relayDefsForStation(m_panelConfig.stationNumber);
        const RelayDef *def = findRelayDefByIndex(relayDefs, index);
        const QString addr = def ? def->addr : QString("Q?");
        const bool useRemote = (m_stationClient && m_stationClient->isConnected());

        qInfo() << "[M100][Station1Panel] relay state request"
                << "src=" << source
                << "index=" << index
                << "addr=" << addr
                << "target=" << on
                << "hasStationClient=" << (m_stationClient != nullptr)
                << "useRemote=" << useRemote;

        if (def && def->type == "pump")
        {
            const QString sourceText = QString::fromUtf8(source ? source : "");
            if (sourceText.startsWith("relay_panel_all_"))
                return true;

            const int byteOff = index / 8;
            const int bit = index % 8;
            QMessageBox::information(this,
                                     "现场联调项",
                                     QString("Q%1.%2 为泵控制位，需到现场联调，当前仅支持状态查看。")
                                         .arg(byteOff)
                                         .arg(bit));
            return false;
        }

        bool ok = false;
        if (useRemote)
        {
            if (!m_stationClient->isConnected())
            {
                qWarning() << "[M100][Station1Panel] remote path selected but StationClient disconnected";
            }

            ControlCommand cmd;
            cmd.command_type = 0;
            cmd.index = index;
            cmd.action = on ? 1 : 0;
            ok = m_stationClient->sendCommand(cmd);
            qInfo() << "[M100][Station1Panel] sendCommand result"
                    << "ok=" << ok
                    << "index=" << cmd.index
                    << "action=" << cmd.action;
        }
        else if (m_deviceManager)
        {
            ok = m_deviceManager->setRelay(index, on);
            qInfo() << "[M100][Station1Panel] local setRelay result"
                    << "ok=" << ok
                    << "index=" << index
                    << "target=" << on;
        }

        if (!ok)
        {
            if (isM100RelayIndex(index))
                m_expectM100Hold[index] = false;
            qWarning() << "[M100][Station1Panel] relay toggle failed"
                       << "index=" << index
                       << "addr=" << addr
                       << "target=" << on;
            return false;
        }

        if (on)
            clearPressureCloseBaseline(index);
        else
            capturePressureCloseBaseline(index);

        if (index < m_relayBtns.size() && m_relayBtns[index])
        {
            auto *btn = m_relayBtns[index];
            if (def)
                btn->setText(formatRelayBtnText(def->addr, def->label, on));
            btn->setProperty("dqOn", on);
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
            qInfo() << "[M100][Station1Panel] ui optimistic update"
                    << "index=" << index
                    << "addr=" << (def ? def->addr : QString("Q?"))
                    << "dqOn=" << on;
        }

        if (relayNeedsGlyphUpdate(index))
            setRelayValveGlyphState(m_scene, index, on);

        if (useRemote)
        {
            if (isM100RelayIndex(index))
                m_expectM100Hold[index] = false;
        }
        else if (scheduleReconcile)
        {
            if (isM100RelayIndex(index))
            {
                if (on)
                {
                    m_expectM100Hold[index] = true;
                    m_expectM100SetMs[index] = QDateTime::currentMSecsSinceEpoch();
                }
                else
                {
                    m_expectM100Hold[index] = false;
                }
            }

            QTimer::singleShot(250, this, [this]() { updateRelayButtons(); });
        }

        return true;
    }

    bool Station1Panel::controlRegulatingValveState(uint16_t id, bool open, const char *source, bool scheduleReconcile)
    {
        if (!m_deviceManager || !m_deviceManager->isPlcConnected())
        {
            qWarning() << "[Valve][Station1Panel] regulating valve request rejected because PLC is disconnected"
                       << "src=" << source
                       << "id=" << id
                       << "target=" << open;
            return false;
        }

        const QString sourceText = QString::fromUtf8(source ? source : "");
        if (!sourceText.startsWith("selfcheck"))
        {
            const auto sc = m_deviceManager->getPlcSelfCheckStatus();
            if (sc.busy)
            {
                qWarning() << "[SelfCheck][Station1Panel] regulating valve control blocked while self-check busy"
                           << "src=" << source
                           << "id=" << id
                           << "targetOpen=" << open;
                return false;
            }
        }

        const bool useRemote = (m_stationClient && m_stationClient->isConnected());
        const float targetPercent = open ? 100.0f : 0.0f;

        qInfo() << "[Valve][Station1Panel] regulating valve request"
                << "src=" << source
                << "id=" << id
                << "targetPercent=" << targetPercent
                << "hasStationClient=" << (m_stationClient != nullptr)
                << "useRemote=" << useRemote;

        bool ok = false;
        if (useRemote)
        {
            if (!m_stationClient->isConnected())
            {
                qWarning() << "[Valve][Station1Panel] remote path selected but StationClient disconnected";
            }

            ControlCommand cmd;
            cmd.command_type = 2;
            cmd.index = static_cast<uint8_t>(id - 1);
            cmd.action = open ? 1 : 0;
            ok = m_stationClient->sendCommand(cmd);
            qInfo() << "[Valve][Station1Panel] sendCommand result"
                    << "ok=" << ok
                    << "index=" << cmd.index
                    << "action=" << cmd.action;
        }
        else if (m_deviceManager)
        {
            ok = m_deviceManager->setValveOpeningPercent(id, targetPercent);
            qInfo() << "[Valve][Station1Panel] local setValveOpeningPercent result"
                    << "ok=" << ok
                    << "id=" << id
                    << "targetPercent=" << targetPercent;
        }

        if (!ok)
            return false;

        if (m_scene)
        {
            auto *valveItem = (id < m_regulatingValveItems.size()) ? dynamic_cast<RegulatingValveItem *>(m_regulatingValveItems[id]) : nullptr;
            if (valveItem)
            {
                valveItem->setOpen(open);
                valveItem->setDegree(targetPercent);
            }
        }

        if (scheduleReconcile && !useRemote)
            QTimer::singleShot(250, this, [this]() { updateSensorValues(); });

        return true;
    }

    void Station1Panel::controlAllRelayValves(bool open, const char *source)
    {
        struct BatchStep
        {
            enum class Kind
            {
                Relay,
                RegulatingValve
            };

            Kind kind;
            uint16_t id;
            QString name;
        };

        const std::vector<BatchStep> openSteps = {
            {BatchStep::Kind::Relay, 0, QString::fromUtf8("电磁阀1")},
            {BatchStep::Kind::Relay, 1, QString::fromUtf8("电磁阀2")},
            {BatchStep::Kind::RegulatingValve, 1, QString::fromUtf8("电动调压阀1")},
            {BatchStep::Kind::Relay, 2, QString::fromUtf8("待测电磁阀")},
            {BatchStep::Kind::Relay, 3, QString::fromUtf8("电磁阀4")},
            {BatchStep::Kind::RegulatingValve, 2, QString::fromUtf8("电动调压阀2")},
            {BatchStep::Kind::Relay, 5, QString::fromUtf8("电磁阀5")},
        };

        std::vector<BatchStep> steps = open ? openSteps : std::vector<BatchStep>(openSteps.rbegin(), openSteps.rend());

        auto state = std::make_shared<std::pair<size_t, bool>>(0, false);
        auto runner = std::make_shared<std::function<void()>>();
        *runner = [this, state, runner, steps = std::move(steps), open, source]() mutable {
            if (state->first >= steps.size())
            {
                updateRelayButtons(true);
                updateSensorValues(true);
                if (state->second)
                {
                    QMessageBox::warning(this,
                                         "操作失败",
                                         open ? "部分阀门未能全部打开，请检查 PLC / 连接状态。"
                                              : "部分阀门未能全部关闭，请检查 PLC / 连接状态。");
                }
                return;
            }

            const BatchStep &step = steps[state->first];
            bool ok = false;
            if (step.kind == BatchStep::Kind::Relay)
            {
                ok = controlRelayState(static_cast<uint8_t>(step.id), open, source, false);
            }
            else
            {
                ok = controlRegulatingValveState(step.id, open, source, false);
            }

            state->second = state->second || !ok;
            ++state->first;

            if (state->first < steps.size())
            {
                QTimer::singleShot(800, this, [runner]() { (*runner)(); });
            }
            else
            {
                updateRelayButtons(true);
                updateSensorValues(true);
                if (state->second)
                {
                    QMessageBox::warning(this,
                                         "操作失败",
                                         open ? "部分阀门未能全部打开，请检查 PLC / 连接状态。"
                                              : "部分阀门未能全部关闭，请检查 PLC / 连接状态。");
                }
            }
        };

        (*runner)();
    }

    namespace
    {
        struct AutoLeakTestConfig
        {
            QString name;
            int buildWaitMs;
            int holdWaitMs;
            float minBuildKpa;
            float p4DropMaxKpa;
            float p5RiseMaxKpa;
            bool closeTestValveBeforeBuild;
            int upstreamPressureSensorNumber = 4;
            int downstreamPressureSensorNumber = 5;
        };

        static bool runRegulatingValveOpenAction(DeviceManager *deviceManager,
                                                 StationClient *stationClient,
                                                 uint16_t valveId,
                                                 const char *source,
                                                 int timeoutMs,
                                                 double targetOpeningPercent = 100.0)
        {
            if (!deviceManager || !deviceManager->isPlcConnected())
                return false;

            const bool useRemote = (stationClient && stationClient->isConnected());

            bool ok = false;
            if (useRemote)
            {
                ControlCommand cmd;
                cmd.command_type = 2;
                cmd.index = static_cast<uint8_t>(valveId - 1);
                cmd.action = 1;
                ok = stationClient->sendCommand(cmd);
            }
            else
            {
                ok = deviceManager->setValveOpeningPercent(valveId, static_cast<float>(targetOpeningPercent));
            }

            if (!ok)
                return false;

            const qint64 deadlineMs = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
            while (QDateTime::currentMSecsSinceEpoch() <= deadlineMs)
            {
                deviceManager->updateAllDevices();
                const auto valve = deviceManager->getRegulatingValve(valveId);
                if (valve.id != 0 && valve.deviceStatus == DeviceStatus::ONLINE && valve.openingPercent >= 95.0f)
                {
                    qInfo() << "[Valve][Station1Panel][AutoOpen] confirmed"
                            << "src=" << source
                            << "id=" << valveId
                            << "openingPercent=" << valve.openingPercent;
                    return true;
                }

                QCoreApplication::processEvents();
                QThread::msleep(100);
            }

            const auto valve = deviceManager->getRegulatingValve(valveId);
            qWarning() << "[Valve][Station1Panel][AutoOpen] confirm timeout"
                       << "src=" << source
                       << "id=" << valveId
                       << "openingPercent=" << valve.openingPercent
                       << "deviceStatus=" << static_cast<int>(valve.deviceStatus);
            return false;
        }

        static bool runHighPressureOpenSequence(DeviceManager *deviceManager,
                                                StationClient *stationClient,
                                                const char *source,
                                                QString *failureReason = nullptr)
        {
            if (!deviceManager || !deviceManager->isPlcConnected())
            {
                if (failureReason)
                    *failureReason = QString::fromUtf8("PLC 未连接或设备管理器不可用");
                return false;
            }

            // 统一封装阀门控制：主控在线就走远程，否则走本地 DeviceManager。
            auto controlRelay = [deviceManager, stationClient](uint8_t index, bool on) -> bool {
                constexpr int kRelayActionGapMs = 180;
                if (stationClient && stationClient->isConnected()) {
                    ControlCommand cmd;
                    cmd.command_type = 0;
                    cmd.index = index;
                    cmd.action = on ? 1 : 0;
                    const bool ok = stationClient->sendCommand(cmd);
                    if (ok)
                        QThread::msleep(kRelayActionGapMs);
                    return ok;
                }
                const bool ok = deviceManager->setRelay(index, on);
                if (ok)
                    QThread::msleep(kRelayActionGapMs);
                return ok;
            };
            // 等待指定毫秒数，保持当前线程的事件循环可继续处理界面刷新。
            auto waitMs = [](int delayMs) {
                QEventLoop waitLoop;
                QTimer::singleShot(delayMs, &waitLoop, &QEventLoop::quit);
                waitLoop.exec();
            };
            // 读取待测阀前后压力：PS6 对应前段，PS7 对应后段。
            // 与界面读数保持一致：优先本地映射读取，远程模式下本地缺失则回退到网络快照。
            auto readPressurePair = [deviceManager, stationClient]() -> std::pair<PressureSensor, PressureSensor> {
                const uint16_t localId6 = resolveLocalPressureSensorId(6);
                const uint16_t localId7 = resolveLocalPressureSensorId(7);

                deviceManager->updateAllDevices();
                PressureSensor p6 = deviceManager->getPressureSensor(localId6);
                PressureSensor p7 = deviceManager->getPressureSensor(localId7);

                if (p6.id != 0 && p7.id != 0)
                    return {p6, p7};

                if (stationClient && stationClient->isConnected())
                {
                    const SensorData net = stationClient->getLatestSensorData();
                    const int idx6 = std::clamp(resolveRemotePressureIndex(6, 2), 0, 5);
                    const int idx7 = std::clamp(resolveRemotePressureIndex(7, 3), 0, 5);

                    if (p6.id == 0)
                    {
                        p6.id = 6;
                        p6.status = DeviceStatus::ONLINE;
                        p6.pressure = static_cast<float>(net.pressure[idx6]);
                    }
                    if (p7.id == 0)
                    {
                        p7.id = 7;
                        p7.status = DeviceStatus::ONLINE;
                        p7.pressure = static_cast<float>(net.pressure[idx7]);
                    }
                }

                return {p6, p7};
            };

            // 高压开阀的关键参数：建压超时、保压等待、目标压力和后端排空阈值。
            const int regValveId = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_open_regulating_valve_id", 2);
            const float regTargetOpening = ConfigManager::getInstance().getFloat("selfcheck.station1.high_pressure_open_target_opening", 100.0f);
            const int regOpenTimeoutMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_open_reg_open_timeout_ms", 2500);
            const int buildTimeoutMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_open_build_timeout_ms", 12000);
            const int holdWaitMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_open_hold_wait_ms", 2000);
            const float targetBuildKpa = ConfigManager::getInstance().getFloat("selfcheck.station1.high_pressure_open_target_kpa", 300.0f);
            const auto &cfg = ConfigManager::getInstance();
            const float p7DrainKpa = [&cfg]() {
                const float newKeyValue = cfg.getFloat("selfcheck.station1.high_pressure_open_p7_drain_kpa", std::numeric_limits<float>::quiet_NaN());
                if (!std::isnan(newKeyValue))
                    return newKeyValue;
                return cfg.getFloat("selfcheck.station1.high_pressure_open_p5_drain_kpa", 30.0f);
            }();
            // 先全开，再等待一段时间后关闭待测阀3，贴合工艺流程中的“先充液、再分离”。
            const int preCloseWaitMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_open_preclose_wait_ms", 3000);

            qInfo() << "[Valve][Station1Panel][HighPressureOpen] start" << "src=" << source;

            // 第一步：先把调压阀开度设到 100%，保证高压建压通道可用。
            qInfo() << "[Valve][Station1Panel][HighPressureOpen] step=1 open regulating valve"
                    << "id=" << regValveId
                    << "targetOpening=" << regTargetOpening;
            if (!runRegulatingValveOpenAction(deviceManager,
                                              stationClient,
                                              static_cast<uint16_t>(std::max(1, regValveId)),
                                              "autoLowPressureInternalLeakStage.reg",
                                              regOpenTimeoutMs,
                                              regTargetOpening))
            {
                qWarning() << "[Valve][Station1Panel][HighPressureOpen] step=1 failed to set regulating valve";
                if (failureReason)
                    *failureReason = QString::fromUtf8("电动调压阀开度设定失败：目标 %1%，阀 ID=%2")
                                         .arg(QString::number(regTargetOpening, 'f', 1))
                                         .arg(regValveId);
                return false;
            }

            // 第二步：把与高压建压有关的全部电磁阀打开，形成可充液通路。
            qInfo() << "[Valve][Station1Panel][HighPressureOpen] step=2 open relays"
                    << "r0=ON" << "r1=ON" << "r3=ON" << "r5=ON";
            if (!controlRelay(0, true) || !controlRelay(1, true) || !controlRelay(3, true) || !controlRelay(5, true))
            {
                qWarning() << "[Valve][Station1Panel][HighPressureOpen] step=2 failed to open pre-relays";
                if (failureReason)
                    *failureReason = QString::fromUtf8("前置阀门打开失败：电磁阀1/2/4/5 之一未响应");
                return false;
            }

            // 第三步：保持 3 秒，等待管路内流体稳定到位。
            waitMs(preCloseWaitMs);

            // 第四步：关闭待测阀 3，把待测区与主通路切开，进入后续建压/保压观察阶段。
            qInfo() << "[Valve][Station1Panel][HighPressureOpen] step=4 close test valve" << "r2=OFF";
            if (!controlRelay(2, false))
            {
                qWarning() << "[Valve][Station1Panel][HighPressureOpen] step=4 failed to close test valve";
                if (failureReason)
                    *failureReason = QString::fromUtf8("待测阀3 关闭失败：电磁阀3 未响应关闭命令");
                return false;
            }

            // 第五步：轮询压力，直到前端压力达到目标值且后端压力满足排空要求，才认为高压开阀步骤完成。
            const qint64 deadlineMs = QDateTime::currentMSecsSinceEpoch() + buildTimeoutMs;
            PressureSensor p6Build;
            PressureSensor p7Build;
            while (true)
            {
                const auto pair = readPressurePair();
                p6Build = pair.first;
                p7Build = pair.second;
                const bool p6Ready = (p6Build.pressure >= targetBuildKpa);
                const bool p7Ready = (p7Build.pressure <= p7DrainKpa);
                qInfo() << "[Valve][Station1Panel][HighPressureOpen] building"
                        << "p6=" << p6Build.pressure
                        << "p7=" << p7Build.pressure
                        << "targetP6>=" << targetBuildKpa
                        << "drainP7<=" << p7DrainKpa
                        << "p6Ready=" << p6Ready
                        << "p7Ready=" << p7Ready;

                if (p6Ready && p7Ready)
                    break;
                if (QDateTime::currentMSecsSinceEpoch() >= deadlineMs)
                {
                    qWarning() << "[Valve][Station1Panel][HighPressureOpen] build timeout"
                               << "p6=" << p6Build.pressure
                               << "p7=" << p7Build.pressure
                               << "targetP6>=" << targetBuildKpa
                               << "drainP7<=" << p7DrainKpa
                               << "p6Ready=" << p6Ready
                               << "p7Ready=" << p7Ready;
                    if (failureReason)
                    {
                        if (!p6Ready && !p7Ready)
                        {
                            *failureReason = QString::fromUtf8("建压超时：PS6=%1 kPa 未达到目标 %2 kPa，PS7=%3 kPa 未低于阈值 %4 kPa")
                                                 .arg(QString::number(p6Build.pressure, 'f', 1))
                                                 .arg(QString::number(targetBuildKpa, 'f', 1))
                                                 .arg(QString::number(p7Build.pressure, 'f', 1))
                                                 .arg(QString::number(p7DrainKpa, 'f', 1));
                        }
                        else if (!p6Ready)
                        {
                            *failureReason = QString::fromUtf8("建压超时：PS6=%1 kPa 未达到目标 %2 kPa")
                                                 .arg(QString::number(p6Build.pressure, 'f', 1))
                                                 .arg(QString::number(targetBuildKpa, 'f', 1));
                        }
                        else
                        {
                            *failureReason = QString::fromUtf8("建压超时：PS7=%1 kPa 未低于阈值 %2 kPa")
                                                 .arg(QString::number(p7Build.pressure, 'f', 1))
                                                 .arg(QString::number(p7DrainKpa, 'f', 1));
                        }
                    }
                    return false;
                }
                QCoreApplication::processEvents();
                QThread::msleep(100);
            }

            (void)controlRelay(0, false);
            (void)controlRelay(1, false);
            (void)controlRelay(3, false);
                // 第五步：关闭前段相关阀门后保压一段时间，获取最终稳定读数。
            waitMs(holdWaitMs);

                const auto pHoldPair = readPressurePair();
            qInfo() << "[Valve][Station1Panel][HighPressureOpen] hold"
                    << "p6=" << pHoldPair.first.pressure
                    << "p7=" << pHoldPair.second.pressure;
            return true;
        }

        static bool runStation1AutoLeakTest(DeviceManager *deviceManager,
                                            StationClient *stationClient,
                                            const AutoLeakTestConfig &config)
        {
            if (!deviceManager || !deviceManager->isPlcConnected())
                return false;

            auto controlRelay = [deviceManager, stationClient](uint8_t index, bool on) -> bool {
                if (stationClient && stationClient->isConnected()) {
                    ControlCommand cmd;
                    cmd.command_type = 0;
                    cmd.index = index;
                    cmd.action = on ? 1 : 0;
                    return stationClient->sendCommand(cmd);
                }
                return deviceManager->setRelay(index, on);
            };
            auto controlPump = [deviceManager, stationClient](uint8_t index, bool on) -> bool {
                if (stationClient && stationClient->isConnected()) {
                    ControlCommand cmd;
                    cmd.command_type = 1;
                    cmd.index = index;
                    cmd.action = on ? 1 : 0;
                    return stationClient->sendCommand(cmd);
                }
                return deviceManager->controlPump(static_cast<uint16_t>(index + 1), on);
            };
            auto waitMs = [](int delayMs) {
                QEventLoop waitLoop;
                QTimer::singleShot(delayMs, &waitLoop, &QEventLoop::quit);
                waitLoop.exec();
            };
            auto readPressurePair = [deviceManager, stationClient, &config]() -> std::pair<PressureSensor, PressureSensor> {
                const int upstreamPsNumber = std::max(1, config.upstreamPressureSensorNumber);
                const int downstreamPsNumber = std::max(1, config.downstreamPressureSensorNumber);
                const uint16_t upstreamLocalId = resolveLocalPressureSensorId(upstreamPsNumber);
                const uint16_t downstreamLocalId = resolveLocalPressureSensorId(downstreamPsNumber);

                deviceManager->updateAllDevices();
                PressureSensor upstream = deviceManager->getPressureSensor(upstreamLocalId);
                PressureSensor downstream = deviceManager->getPressureSensor(downstreamLocalId);

                if (upstream.id != 0 && downstream.id != 0)
                    return {upstream, downstream};

                if (stationClient && stationClient->isConnected())
                {
                    const SensorData net = stationClient->getLatestSensorData();
                    const int upstreamFallback = std::max(0, upstreamPsNumber - 4);
                    const int downstreamFallback = std::max(0, downstreamPsNumber - 4);
                    const int upstreamIndex = std::clamp(resolveRemotePressureIndex(upstreamPsNumber, upstreamFallback), 0, 5);
                    const int downstreamIndex = std::clamp(resolveRemotePressureIndex(downstreamPsNumber, downstreamFallback), 0, 5);

                    if (upstream.id == 0)
                    {
                        upstream.id = static_cast<uint16_t>(upstreamPsNumber);
                        upstream.status = DeviceStatus::ONLINE;
                        upstream.pressure = static_cast<float>(net.pressure[upstreamIndex]);
                    }
                    if (downstream.id == 0)
                    {
                        downstream.id = static_cast<uint16_t>(downstreamPsNumber);
                        downstream.status = DeviceStatus::ONLINE;
                        downstream.pressure = static_cast<float>(net.pressure[downstreamIndex]);
                    }
                }

                return {upstream, downstream};
            };

            const auto pairBefore = readPressurePair();
            const auto p4Before = pairBefore.first;
            const auto p5Before = pairBefore.second;

            bool pumpStartedBySelfCheck = false;
            if (config.closeTestValveBeforeBuild && !controlRelay(2, false))
                return false;

            if (!controlPump(0, true))
                return false;
            pumpStartedBySelfCheck = true;

            const qint64 deadlineMs = QDateTime::currentMSecsSinceEpoch() + config.buildWaitMs;
            PressureSensor p4Build = p4Before;
            PressureSensor p5Build = p5Before;
            while (true)
            {
                const auto pair = readPressurePair();
                p4Build = pair.first;
                p5Build = pair.second;
                if (p4Build.pressure >= config.minBuildKpa)
                    break;
                if (QDateTime::currentMSecsSinceEpoch() >= deadlineMs)
                    break;
                QCoreApplication::processEvents();
                QThread::msleep(100);
            }

            if (pumpStartedBySelfCheck)
                (void)controlPump(0, false);

            if (p4Build.pressure < config.minBuildKpa)
                return false;

            (void)controlRelay(0, false);
            (void)controlRelay(1, false);
            (void)controlRelay(3, false);
            waitMs(config.holdWaitMs);

            const auto pHoldPair = readPressurePair();
            const auto p4Hold = pHoldPair.first;
            const auto p5Hold = pHoldPair.second;
            const float p4Delta = p4Build.pressure - p4Hold.pressure;
            const float p5Delta = p5Hold.pressure - p5Build.pressure;
            const bool externalLeak = (p4Delta >= config.p4DropMaxKpa) && !(p5Delta >= config.p5RiseMaxKpa);
            const bool internalLeak = (p5Delta >= config.p5RiseMaxKpa) && (p4Delta > 0.0f);

                const QString upstreamTag = QString::fromUtf8("PS%1").arg(std::max(1, config.upstreamPressureSensorNumber));
                const QString downstreamTag = QString::fromUtf8("PS%1").arg(std::max(1, config.downstreamPressureSensorNumber));
                qInfo() << "[Station1Panel][AutoLeakTest]" << config.name
                    << (upstreamTag + "Before=") << p4Before.pressure
                    << (downstreamTag + "Before=") << p5Before.pressure
                    << (upstreamTag + "Build=") << p4Build.pressure
                    << (downstreamTag + "Build=") << p5Build.pressure
                    << (upstreamTag + "Hold=") << p4Hold.pressure
                    << (downstreamTag + "Hold=") << p5Hold.pressure
                    << "p4Delta=" << p4Delta
                    << "p5Delta=" << p5Delta
                    << "externalLeak=" << externalLeak
                    << "internalLeak=" << internalLeak;

            return !(externalLeak || internalLeak);
        }
    }

    bool Station1Panel::autoLowPressureOpenValve()
    {
        // 开阀测试流程：调压阀1设到 100% -> 等 1s -> 开电磁阀1 -> 等 1s -> 开待测阀3，并记录 PS7 压差。
        setStageOverviewState(0, QString::fromUtf8("开阀测试"), QString::fromUtf8("执行中…"), true);

        const float targetOpening = ConfigManager::getInstance().getFloat("selfcheck.station1.low_pressure_open_target_opening", 100.0f);
        const int regSettleMs = ConfigManager::getInstance().getInt("selfcheck.station1.low_pressure_open_reg_settle_ms", 1000);
        const int preOpenWaitMs = ConfigManager::getInstance().getInt("selfcheck.station1.low_pressure_open_preopen_wait_ms", 1000);
        const int monitorWaitMs = ConfigManager::getInstance().getInt("selfcheck.station1.low_pressure_open_monitor_wait_ms", 2000);

        auto setStage0Value = [this](int row, const QString &text) {
            if (row < 0 || row >= 3)
                return;
            auto *label = m_stageValueLabels[0][static_cast<size_t>(row)];
            if (!label)
                return;
            label->setText(text);
        };

        auto waitMs = [](int delayMs) {
            if (delayMs <= 0)
                return;
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < delayMs)
            {
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 16);
                QThread::msleep(5);
            }
        };

        auto readPressureByPsNumber = [this](int psNumber, double &pressureKpa) -> bool {
            const uint16_t configuredSensorId = configuredPressureSensorId(m_panelConfig, psNumber);
            if (configuredSensorId == 0)
                return false;
            const size_t fallbackIndex = static_cast<size_t>(std::max(0, psNumber - 3));
            return readPressureValueForDisplay(configuredSensorId, fallbackIndex, pressureKpa);
        };

        bool ok = true;
        QString failureReason;

        // 第一步：调压阀1开度设置到 100%
        if (!m_deviceManager || !m_deviceManager->isPlcConnected())
        {
            ok = false;
            failureReason = QString::fromUtf8("PLC 未连接，无法执行开阀测试流程");
        }
        else if (!m_deviceManager->setValveOpeningPercent(1, targetOpening))
        {
            ok = false;
            failureReason = QString::fromUtf8("电动调压阀1开度设定失败（目标 %1%）").arg(QString::number(targetOpening, 'f', 1));
        }

        if (ok)
        {
            setStage0Value(0, QString::fromUtf8("%1 s").arg(QString::number(static_cast<double>(regSettleMs) / 1000.0, 'f', 1)));
            waitMs(regSettleMs);
        }

        // 第二步：1秒后打开电磁阀1
        if (ok && !controlRelayState(0, true, "autoLowPressureOpenValve", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("电磁阀1 打开失败");
        }
        if (ok)
        {
            setStage0Value(1, QString::fromUtf8("%1 s").arg(QString::number(static_cast<double>(preOpenWaitMs) / 1000.0, 'f', 1)));
            waitMs(preOpenWaitMs);
        }

        // 第三步：1秒后打开待测阀，并记录 PS7 压差
        double p7Before = 0.0;
        double p7After = 0.0;

        if (ok)
        {
            if (!readPressureByPsNumber(7, p7Before))
            {
                ok = false;
                failureReason = QString::fromUtf8("读取 PS7 初始压力失败");
            }
        }

        if (ok && !controlRelayState(2, true, "autoLowPressureOpenValve", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("待测阀 打开失败");
        }

        if (ok)
        {
            waitMs(monitorWaitMs);
            if (!readPressureByPsNumber(7, p7After))
            {
                ok = false;
                failureReason = QString::fromUtf8("读取 PS7 变化压力失败");
            }
        }

        // if (ok && !controlRelayState(2, false, "autoLowPressureOpenValve", false))
        // {
        //     ok = false;
        //     failureReason = QString::fromUtf8("待测阀 关闭失败");
        // }

        // // 等待阀门稳定（保持UI刷新，但屏蔽输入重入）
        // if (ok)
        // {
        //     waitMs(2000);  // 关闭后稳定延时
        // }

        // // 系统复位：关闭电磁阀1
        // if (ok && !controlRelayState(0, false, "autoLowPressureOpenValve", false))
        // {
        //     ok = false;
        //     failureReason = QString::fromUtf8("电磁阀1 关闭失败");
        // }

        // // 等待继电器稳定（保持UI刷新，但屏蔽输入重入）
        // if (ok)
        // {
        //     waitMs(300);  // 继电器稳定延时
        // }

        // // 系统复位：关闭调压阀
        // if (ok && m_deviceManager && !m_deviceManager->setValveOpeningPercent(1, 0.0f))
        // {
        //     ok = false;
        //     failureReason = QString::fromUtf8("调压阀1 关闭失败");
        // }

        if (ok)
        {
            const double p7Delta = p7After - p7Before;
            
            // 判断 p7Delta 是否超过阈值：成功条件
            const double p7DeltaThreshold = 100.0;
            if (p7Delta < p7DeltaThreshold)
            {
                ok = false;
                failureReason = QString::fromUtf8("PS7 压差不足（%1 kPa < %2 kPa），开阀测试失败")
                    .arg(QString::number(p7Delta, 'f', 1))
                    .arg(QString::number(p7DeltaThreshold, 'f', 1));
            }
            
            setStage0Value(2,
                           QString::fromUtf8("PS7 %1→%2 (Δ%3) kPa")
                               .arg(QString::number(p7Before, 'f', 1))
                               .arg(QString::number(p7After, 'f', 1))
                               .arg(QString::number(p7Delta, 'f', 1)));
        }

        setStageOverviewState(0, QString::fromUtf8("开阀测试"), ok ? QString::fromUtf8("完成") : QString::fromUtf8("失败"), ok);
        if (!ok)
            appendStageOverviewIssue(0,
                                     failureReason.isEmpty()
                                         ? QString::fromUtf8("开阀测试失败，请检查 PLC 和调压阀状态")
                                         : QString::fromUtf8("开阀测试失败：%1").arg(failureReason));
        return ok;
    }

    bool Station1Panel::autoLowPressureInternalLeakStage()
    {
        // 低压内泄漏测试流程：调压阀1到 8% -> 等待稳定 -> 关闭电磁阀1 -> 100ms后关闭待测阀 -> 100ms后关闭电磁阀2 -> 2秒后关闭电磁阀4 -> 监控10秒 -> 判断PS6<PS4-10kPa
        setStageOverviewState(1, QString::fromUtf8("低压内泄漏"), QString::fromUtf8("执行中…"), true);
        const float targetOpening = ConfigManager::getInstance().getFloat("selfcheck.station1.low_pressure_internal_leak_target_opening", 8.0f);
        const int regSettleMs = ConfigManager::getInstance().getInt("selfcheck.station1.low_pressure_internal_leak_reg_settle_ms", 8000);
        // 阶段间隔参数（当前默认值分别为 400ms/1500ms）。
        const int postRegDelay100Ms = 400;
        const int postValveDelay100Ms = 1500;
        // 末端压力排空等待与监控窗口。
        const int endPressureDrainWaitMs = 2000;
        const int monitorWaitMs = 10000;
        // 判定阈值：要求 PS6 < PS4 - 10kPa。
        const float pressureDiffThresholdKpa = 10.0f;

        auto setStage1Value = [this](int row, const QString &text) {
            if (row < 0 || row >= 3)
                return;
            auto *label = m_stageValueLabels[1][static_cast<size_t>(row)];
            if (!label)
                return;
            label->setText(text);
        };

        auto waitMs = [](int delayMs) {
            if (delayMs <= 0)
                return;
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < delayMs)
            {
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 16);
                QThread::msleep(5);
            }
        };

        auto readPressureByPsNumber = [this](int psNumber, double &pressureKpa) -> bool {
            const uint16_t configuredSensorId = configuredPressureSensorId(m_panelConfig, psNumber);
            if (configuredSensorId == 0)
                return false;
            const size_t fallbackIndex = static_cast<size_t>(std::max(0, psNumber - 3));
            return readPressureValueForDisplay(configuredSensorId, fallbackIndex, pressureKpa);
        };

        bool ok = true;
        QString failureReason;

        // 步骤1：确认 PLC 在线，并将调压阀1开度设为目标值（默认 8%）。
        if (!m_deviceManager || !m_deviceManager->isPlcConnected())
        {
            ok = false;
            failureReason = QString::fromUtf8("PLC 未连接，无法执行低压内泄漏流程");
        }
        else if (!m_deviceManager->setValveOpeningPercent(1, targetOpening))
        {
            ok = false;
            failureReason = QString::fromUtf8("电动调压阀1开度设定失败（目标 %1%）").arg(QString::number(targetOpening, 'f', 1));
        }

        // 步骤2：等待调压阀稳定（默认 8 秒），避免后续动作受瞬态影响。
        if (ok)
        {
            setStage1Value(0, QString::fromUtf8("调压阀稳定 %1 s").arg(QString::number(static_cast<double>(regSettleMs) / 1000.0, 'f', 1)));
            waitMs(regSettleMs);
        }

        // 步骤3：关闭电磁阀1（继电器 index=0）。
        if (ok && !controlRelayState(0, false, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("电磁阀1 关闭失败");
        }

        // 步骤4：间隔后关闭待测阀（继电器 index=2）。
        if (ok)
        {
            waitMs(postRegDelay100Ms);
            if (!controlRelayState(2, false, "autoHighPressureLeakStage", false))
            {
                ok = false;
                failureReason = QString::fromUtf8("待测阀 关闭失败");
            }
        }

        // 步骤5：再次间隔后关闭电磁阀2（继电器 index=1）。
        if (ok)
        {
            waitMs(postValveDelay100Ms);
            if (!controlRelayState(1, false, "autoHighPressureLeakStage", false))
            {
                ok = false;
                failureReason = QString::fromUtf8("电磁阀2 关闭失败");
            }
        }

        // 步骤6：等待末端压力排空（默认 2 秒）后关闭电磁阀4（继电器 index=3）。
        if (ok)
        {
            waitMs(endPressureDrainWaitMs);
            if (!controlRelayState(3, false, "autoHighPressureLeakStage", false))
            {
                ok = false;
                failureReason = QString::fromUtf8("电磁阀4 关闭失败");
            }
        }

        // 步骤7：记录监控窗口起始时刻的 PS6/PS4 压力。
        double ps6Initial = 0.0;
        double ps4Initial = 0.0;
        double ps6Final = 0.0;
        double ps4Final = 0.0;

        if (ok)
        {
            if (!readPressureByPsNumber(6, ps6Initial) || !readPressureByPsNumber(4, ps4Initial))
            {
                ok = false;
                failureReason = QString::fromUtf8("读取 PS6/PS4 初始压力失败");
            }
        }

        // 步骤8：进入监控窗口（默认 10 秒），采集结束时刻压力。
        if (ok)
        {
            setStage1Value(1, QString::fromUtf8("监控压力变化 %1 s").arg(QString::number(static_cast<double>(monitorWaitMs) / 1000.0, 'f', 1)));
            waitMs(monitorWaitMs);
            if (!readPressureByPsNumber(6, ps6Final) || !readPressureByPsNumber(4, ps4Final))
            {
                ok = false;
                failureReason = QString::fromUtf8("读取 PS6/PS4 最终压力失败");
            }
        }

        // 步骤9：压差判定。成功条件为 PS6 - PS4 < -10kPa，即 PS6 < PS4 - 10kPa。
        if (ok)
        {
            const double pressureDiff = ps6Final - ps4Final;
            const bool testPassed = pressureDiff < (-pressureDiffThresholdKpa);
            
            setStage1Value(2,
                           QString::fromUtf8("PS6: %1→%2 kPa; PS4: %3→%4 kPa; Δ(PS6-PS4)=%5 kPa [%6]")
                               .arg(QString::number(ps6Initial, 'f', 1))
                               .arg(QString::number(ps6Final, 'f', 1))
                               .arg(QString::number(ps4Initial, 'f', 1))
                               .arg(QString::number(ps4Final, 'f', 1))
                               .arg(QString::number(pressureDiff, 'f', 1))
                               .arg(testPassed ? QString::fromUtf8("通过") : QString::fromUtf8("失败")));

            if (!testPassed)
            {
                ok = false;
                failureReason = QString::fromUtf8("压力差异检查失败：PS6(%1 kPa) 应小于 PS4(%2 kPa) - 10 kPa (实际差值：%3 kPa)")
                    .arg(QString::number(ps6Final, 'f', 1))
                    .arg(QString::number(ps4Final, 'f', 1))
                    .arg(QString::number(pressureDiff, 'f', 1));
            }
        }

        setStageOverviewState(1, QString::fromUtf8("低压内泄漏"), ok ? QString::fromUtf8("完成") : QString::fromUtf8("失败"), ok);
        if (!ok)
            appendStageOverviewIssue(1,
                                     failureReason.isEmpty()
                                         ? QString::fromUtf8("低压内泄漏测试失败，请检查 PLC 和调压阀状态")
                                         : QString::fromUtf8("低压内泄漏测试失败：%1").arg(failureReason));
        return ok;
    }

    bool Station1Panel::autoHighPressureLeakStage()
    {
        // 第3阶段：高压泄露测试（同时判定高压外泄漏与高压内泄漏）。
        setStageOverviewState(2, QString::fromUtf8("高压泄露"), QString::fromUtf8("执行中…"), true);

        const float regOpenTarget = ConfigManager::getInstance().getFloat("selfcheck.station1.high_pressure_leak_stage_reg_opening", 100.0f);
        const int regOpenSettleMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_leak_stage_reg_settle_ms", 8000);
        const int closeV4WaitMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_leak_stage_close_v4_wait_ms", 5000);
        const int closeV1WaitMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_leak_stage_close_v1_wait_ms", 1000);
        const int openV4HoldMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_leak_stage_open_v4_hold_ms", 3000);
        const int p7MonitorMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_leak_stage_p7_monitor_ms", 3000);
        const int sampleIntervalMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_leak_stage_sample_ms", 100);
        // 这里暂时先调整到150kPa
        const float ps6DropThresholdKpa = ConfigManager::getInstance().getFloat("selfcheck.station1.high_pressure_leak_stage_ps6_drop_kpa", 150.0f);
        const float ps7RiseThresholdKpa = ConfigManager::getInstance().getFloat("selfcheck.station1.high_pressure_leak_stage_ps7_rise_kpa", 10.0f);

        auto setStage2Value = [this](int row, const QString &text) {
            if (row < 0 || row >= 3)
                return;
            auto *label = m_stageValueLabels[2][static_cast<size_t>(row)];
            if (!label)
                return;
            label->setText(text);
        };

        auto waitMs = [](int delayMs) {
            if (delayMs <= 0)
                return;
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < delayMs)
            {
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 16);
                QThread::msleep(5);
            }
        };

        auto readPressureByPsNumber = [this](int psNumber, double &pressureKpa) -> bool {
            const uint16_t configuredSensorId = configuredPressureSensorId(m_panelConfig, psNumber);
            if (configuredSensorId == 0)
                return false;
            const size_t fallbackIndex = static_cast<size_t>(std::max(0, psNumber - 3));
            return readPressureValueForDisplay(configuredSensorId, fallbackIndex, pressureKpa);
        };

        bool ok = true;
        QString failureReason;

        // 预先打开所有阀门，确保高压泄露测试流程可以顺利进行。
        if (ok && !controlRelayState(0, true, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("失败：电磁阀1 打开失败");
        }
        if (ok && !controlRelayState(1, true, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("失败：电磁阀2 打开失败");
        }
        if (ok && !controlRelayState(2, true, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("失败：待测阀 打开失败");
        }
        if (ok && !controlRelayState(3, true, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("失败：电磁阀4 打开失败");
        }
        // 1) 设置电动调压阀1开度到100%，延迟8秒。
        if (!m_deviceManager || !m_deviceManager->isPlcConnected())
        {
            ok = false;
            failureReason = QString::fromUtf8("PLC 未连接，无法执行高压泄露流程");
        }
        else if (!m_deviceManager->setValveOpeningPercent(1, regOpenTarget))
        {
            ok = false;
            failureReason = QString::fromUtf8("电动调压阀1开度设定失败（目标 %1%）").arg(QString::number(regOpenTarget, 'f', 1));
        }

        if (ok)
        {
            setStage2Value(0, QString::fromUtf8("步骤1：调压阀1=%1%，等待 %2 s")
                                  .arg(QString::number(regOpenTarget, 'f', 1))
                                  .arg(QString::number(static_cast<double>(regOpenSettleMs) / 1000.0, 'f', 1)));
            waitMs(regOpenSettleMs);
        }

        // 2) 关闭待测阀（电磁阀3），延迟5秒。
        if (ok && !controlRelayState(2, false, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("步骤2失败：待测阀 关闭失败");
        }
        if (ok)
            waitMs(closeV4WaitMs);

        // 3) 关闭电磁阀1，延迟1秒。
        if (ok && !controlRelayState(0, false, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("步骤3失败：电磁阀1 关闭失败");
        }
        if (ok)
            waitMs(closeV1WaitMs);

        // 4) 关闭电磁阀2，调压阀1开度置0%，关闭待测阀；从本步骤开始监控 PS6/PS4。
        double ps6Start = 0.0;
        double ps4Start = 0.0;
        double ps6Min = 0.0;
        double ps4Latest = 0.0;

        auto samplePs64 = [&]() -> bool {
            double ps6Now = 0.0;
            double ps4Now = 0.0;
            if (!readPressureByPsNumber(6, ps6Now) || !readPressureByPsNumber(4, ps4Now))
                return false;
            ps6Min = qMin(ps6Min, ps6Now);
            ps4Latest = ps4Now;
            return true;
        };

        if (ok && !controlRelayState(1, false, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("步骤4失败：电磁阀2 关闭失败");
        }
        if (ok && m_deviceManager && !m_deviceManager->setValveOpeningPercent(1, 0.0f))
        {
            ok = false;
            failureReason = QString::fromUtf8("步骤4失败：电动调压阀1 置0%失败");
        }
        
        // 4) 关闭电磁阀4，延迟1秒。建立空压区域
        if (ok && !controlRelayState(3, false, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("步骤4失败：电磁阀4 关闭失败");
        }

        // 步骤4动作完成后，延迟 1 秒再记录 PS6/PS4 起始值。
        if (ok)
        {
            waitMs(2000);
            if (!readPressureByPsNumber(6, ps6Start) || !readPressureByPsNumber(4, ps4Start))
            {
                ok = false;
                failureReason = QString::fromUtf8("步骤4失败：读取 PS6/PS4 初始压力失败");
            }
            else
            {
                ps6Min = ps6Start;
                ps4Latest = ps4Start;
            }
        }

        if (ok)
        {
            setStage2Value(0, QString::fromUtf8("步骤4完成：已关闭阀1/2/3并将调压阀1置0%，开始监控PS6/PS4"));
            if (!samplePs64())
            {
                ok = false;
                failureReason = QString::fromUtf8("步骤4失败：监控过程中读取 PS6/PS4 失败");
            }
        }

        // 5) 打开电磁阀4；3秒后关闭电磁阀4。
        if (ok && !controlRelayState(3, true, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("步骤5失败：电磁阀4 打开失败");
        }

        if (ok)
        {
            QElapsedTimer monitorTimer;
            monitorTimer.start();
            while (monitorTimer.elapsed() < openV4HoldMs)
            {
                if (!samplePs64())
                {
                    ok = false;
                    failureReason = QString::fromUtf8("步骤5失败：读取 PS6/PS4 监控值失败");
                    break;
                }
                waitMs(sampleIntervalMs);
            }
        }

        if (ok && !controlRelayState(3, false, "autoHighPressureLeakStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("步骤5失败：电磁阀4 关闭失败");
        }

        // 6) 步骤4起监控PS6/PS4，PS6降低超过10kPa判定外泄漏；步骤5关闭电磁阀4后监控PS7，PS7升高超过10kPa判定内泄漏。
        double ps7Start = 0.0;
        double ps7Max = 0.0;
        if (ok)
        {
            if (!readPressureByPsNumber(7, ps7Start))
            {
                ok = false;
                failureReason = QString::fromUtf8("步骤6失败：读取 PS7 初始压力失败");
            }
            else
            {
                ps7Max = ps7Start;
                QElapsedTimer ps7Timer;
                ps7Timer.start();
                while (ps7Timer.elapsed() < p7MonitorMs)
                {
                    double ps7Now = 0.0;
                    if (!readPressureByPsNumber(7, ps7Now))
                    {
                        ok = false;
                        failureReason = QString::fromUtf8("步骤6失败：读取 PS7 监控值失败");
                        break;
                    }
                    ps7Max = qMax(ps7Max, ps7Now);
                    if (!samplePs64())
                    {
                        ok = false;
                        failureReason = QString::fromUtf8("步骤6失败：读取 PS6/PS4 监控值失败");
                        break;
                    }
                    waitMs(sampleIntervalMs);
                }
            }
        }

        bool highPressureExternalLeak = false;
        bool highPressureInternalLeak = false;
        double ps6Drop = 0.0;
        double ps7Rise = 0.0;

        if (ok)
        {
            ps6Drop = ps6Start - ps6Min;
            ps7Rise = ps7Max - ps7Start;
            highPressureExternalLeak = (ps6Drop > ps6DropThresholdKpa);
            highPressureInternalLeak = (ps7Rise > ps7RiseThresholdKpa);

            setStage2Value(1,
                           QString::fromUtf8("PS6 %1→%2 (降%3) kPa；PS4 %4→%5 kPa")
                               .arg(QString::number(ps6Start, 'f', 1))
                               .arg(QString::number(ps6Min, 'f', 1))
                               .arg(QString::number(ps6Drop, 'f', 1))
                               .arg(QString::number(ps4Start, 'f', 1))
                               .arg(QString::number(ps4Latest, 'f', 1)));

            QStringList leakTags;
            if (highPressureExternalLeak)
                leakTags << QString::fromUtf8("高压外泄漏");
            if (highPressureInternalLeak)
                leakTags << QString::fromUtf8("高压内泄露");
            if (leakTags.isEmpty())
                leakTags << QString::fromUtf8("未检出泄露");

            setStage2Value(2,
                           QString::fromUtf8("PS7 %1→%2 (升%3) kPa；判定：%4")
                               .arg(QString::number(ps7Start, 'f', 1))
                               .arg(QString::number(ps7Max, 'f', 1))
                               .arg(QString::number(ps7Rise, 'f', 1))
                               .arg(leakTags.join(QString::fromUtf8("，"))));

            if (highPressureExternalLeak || highPressureInternalLeak)
            {
                ok = false;
                failureReason = QString::fromUtf8("高压泄露判定异常：PS6降幅=%1 kPa（阈值>%2），PS7升幅=%3 kPa（阈值>%4）")
                                    .arg(QString::number(ps6Drop, 'f', 1))
                                    .arg(QString::number(ps6DropThresholdKpa, 'f', 1))
                                    .arg(QString::number(ps7Rise, 'f', 1))
                                    .arg(QString::number(ps7RiseThresholdKpa, 'f', 1));
            }
        }

        setStageOverviewState(2, QString::fromUtf8("高压泄露"), ok ? QString::fromUtf8("完成") : QString::fromUtf8("失败"), ok);
        if (!ok)
            appendStageOverviewIssue(2,
                                     failureReason.isEmpty()
                                         ? QString::fromUtf8("高压泄露测试失败，请检查阀门动作与压力传感器状态")
                                         : QString::fromUtf8("高压泄露测试失败：%1").arg(failureReason));
        return ok;
    }

    bool Station1Panel::autoFlowTestStage()
    {
        // 第4阶段：流量测试前置。预开电磁阀1/2/3/4，调压阀1设到34%，稳定8秒。
        setStageOverviewState(3, QString::fromUtf8("流量测试"), QString::fromUtf8("执行中…"), true);

        const float flowRegOpening = ConfigManager::getInstance().getFloat("selfcheck.station1.flow_test_reg_opening", 34.0f);
        const int flowRegSettleMs = ConfigManager::getInstance().getInt("selfcheck.station1.flow_test_reg_settle_ms", 8000);

        auto setStage3Value = [this](int row, const QString &text) {
            if (row < 0 || row >= 3)
                return;
            auto *label = m_stageValueLabels[3][static_cast<size_t>(row)];
            if (!label)
                return;
            label->setText(text);
        };

        auto waitMs = [](int delayMs) {
            if (delayMs <= 0)
                return;
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < delayMs)
            {
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 16);
                QThread::msleep(5);
            }
        };

        auto readPressureByPsNumber = [this](int psNumber, double &pressureKpa) -> bool {
            const uint16_t configuredSensorId = configuredPressureSensorId(m_panelConfig, psNumber);
            if (configuredSensorId == 0)
                return false;
            const size_t fallbackIndex = static_cast<size_t>(std::max(0, psNumber - 3));
            return readPressureValueForDisplay(configuredSensorId, fallbackIndex, pressureKpa);
        };

        bool ok = true;
        QString failureReason;

        if (!m_deviceManager || !m_deviceManager->isPlcConnected())
        {
            ok = false;
            failureReason = QString::fromUtf8("PLC 未连接，无法执行流量测试阶段");
        }

        // 预先打开电磁阀1/2/3/4。
        if (ok && !controlRelayState(0, true, "autoFlowTestStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("电磁阀1 打开失败");
        }
        if (ok && !controlRelayState(1, true, "autoFlowTestStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("电磁阀2 打开失败");
        }
        if (ok && !controlRelayState(2, true, "autoFlowTestStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("电磁阀3（待测阀）打开失败");
        }
        if (ok && !controlRelayState(3, true, "autoFlowTestStage", false))
        {
            ok = false;
            failureReason = QString::fromUtf8("电磁阀4 打开失败");
        }

        if (ok)
        {
            setStage3Value(0, QString::fromUtf8("电磁阀1/2/3/4 已全部打开"));
        }

        // 调整电动调压阀1开度到34%，稳定8秒。
        if (ok && m_deviceManager && !m_deviceManager->setValveOpeningPercent(1, flowRegOpening))
        {
            ok = false;
            failureReason = QString::fromUtf8("电动调压阀1开度设定失败（目标 %1%）").arg(QString::number(flowRegOpening, 'f', 1));
        }

        if (ok)
        {
            setStage3Value(1, QString::fromUtf8("调压阀1=%1%，稳定 %2 s")
                                  .arg(QString::number(flowRegOpening, 'f', 1))
                                  .arg(QString::number(static_cast<double>(flowRegSettleMs) / 1000.0, 'f', 1)));
            waitMs(flowRegSettleMs);

            // 流量系数计算：Kv = 10Q / sqrt(deltaP)
            // Q：流量计实测值（m^3/h）；deltaP：待测阀前后压差（PS6-PS7，kPa）。
            bool kvCalculated = false;
            const auto fm1 = m_deviceManager->getFlowMeter(1);
            const double flowQ = static_cast<double>(fm1.flowRate);

            double p6 = 0.0;
            double p7 = 0.0;
            if (!readPressureByPsNumber(6, p6) || !readPressureByPsNumber(7, p7))
            {
                ok = false;
                failureReason = QString::fromUtf8("流量测试失败：读取 PS6/PS7 压力失败，无法计算 Kv");
            }
            else
            {
                const double deltaP = p6 - p7;
                if (deltaP <= 0.0)
                {
                    ok = false;
                    failureReason = QString::fromUtf8("流量测试失败：deltaP=%1 kPa（需 > 0），无法计算 Kv")
                                        .arg(QString::number(deltaP, 'f', 3));
                }
                else
                {
                    const double kv = (10.0 * flowQ) / qSqrt(deltaP);
                    setStage3Value(2,
                                   QString::fromUtf8("Q=%1 m^3/h, deltaP=%2 kPa, Kv=%3")
                                       .arg(QString::number(flowQ, 'f', 3))
                                       .arg(QString::number(deltaP, 'f', 3))
                                       .arg(QString::number(kv, 'f', 3)));
                    kvCalculated = true;
                }
            }

            if (ok && !kvCalculated)
                setStage3Value(2, QString::fromUtf8("流量测试前置条件已就绪"));
        }

        setStageOverviewState(3, QString::fromUtf8("流量测试"), ok ? QString::fromUtf8("完成") : QString::fromUtf8("失败"), ok);
        if (!ok)
            appendStageOverviewIssue(3,
                                     failureReason.isEmpty()
                                         ? QString::fromUtf8("流量测试前置失败，请检查阀门与调压阀状态")
                                         : QString::fromUtf8("流量测试前置失败：%1").arg(failureReason));
        return ok;
    }

    bool Station1Panel::autoHighPressureExternalLeak()
    {
        // 高压外泄漏：与高压内泄露共用流程，但后端上升阈值更严格，用于区分外泄漏。
        const int buildWaitMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_external_leak_build_wait_ms", 3500);
        const int holdWaitMs = ConfigManager::getInstance().getInt("selfcheck.station1.high_pressure_external_leak_hold_ms", 3500);
        const float minBuildKpa = ConfigManager::getInstance().getFloat("selfcheck.station1.high_pressure_external_leak_min_kpa", 50.0f);
        const float p4DropMaxKpa = ConfigManager::getInstance().getFloat("selfcheck.station1.high_pressure_external_leak_p4_drop_kpa", 8.0f);
        const float p5RiseMaxKpa = ConfigManager::getInstance().getFloat("selfcheck.station1.high_pressure_external_leak_p5_rise_kpa", 1.0f);

        setStageOverviewState(4,
                              QString::fromUtf8("高压外泄漏"),
                              QString::fromUtf8("执行中… 建压等待 %1 ms，保压 %2 ms，目标 %3 kPa")
                                  .arg(buildWaitMs)
                                  .arg(holdWaitMs)
                                  .arg(QString::number(minBuildKpa, 'f', 1)),
                              true);
        const bool ok = runStation1AutoLeakTest(m_deviceManager.get(),
                                       m_stationClient.get(),
                                       {QString::fromUtf8("高压外泄漏"),
                                        buildWaitMs,
                                        holdWaitMs,
                                        minBuildKpa,
                                        p4DropMaxKpa,
                                        p5RiseMaxKpa,
                                        true,
                                        6,
                                        7});
        setStageOverviewState(4, QString::fromUtf8("高压外泄漏"), ok ? QString::fromUtf8("完成") : QString::fromUtf8("失败"), ok);
        if (!ok)
            appendStageOverviewIssue(4,
                                     QString::fromUtf8("高压外泄漏判定失败，请检查 PS6/PS7 压力变化，阈值：PS6 降幅 %1 kPa，PS7 升幅 %2 kPa")
                                         .arg(QString::number(p4DropMaxKpa, 'f', 1))
                                         .arg(QString::number(p5RiseMaxKpa, 'f', 1)));
        return ok;
    }

    void Station1Panel::triggerSelfCheck()
    {
        onSelfCheck();
    }

    void Station1Panel::onSelfCheck()
    {
        if (!m_deviceManager)
        {
            QMessageBox::warning(this, "系统自检", "设备管理器未初始化（请先连接系统）");
            return;
        }

        const bool preferPlcSide = ConfigManager::getInstance().getBool("selfcheck.station1.use_plc_side", true);
        if (preferPlcSide)
        {
            auto &cfg = ConfigManager::getInstance();
            const bool enableOptional = cfg.getBool("selfcheck.plc.cmd.enable_optional", true);
            const bool preOpenRegValves = cfg.getBool("selfcheck.station1.preopen_regulating_valves", true);
            const float reg1Target = cfg.getFloat("selfcheck.station1.regulating_valve1_opening_percent", 100.0f);
            const float reg2Target = cfg.getFloat("selfcheck.station1.regulating_valve2_opening_percent", 100.0f);
            const int regSettleMs = cfg.getInt("selfcheck.station1.regulating_valve_settle_ms", 1200);
            bool ok = false;
            const bool plcConnected = (m_deviceManager && m_deviceManager->isPlcConnected());

            if (plcConnected)
            {
                m_deviceManager->refreshPlcSelfCheckStatus();
                const auto prevSc = m_deviceManager->getPlcSelfCheckStatus();
                if (prevSc.busy)
                {
                    QMessageBox::information(this,
                                             "系统自检",
                                             QString::fromUtf8("PLC 自检仍在运行中（Step=%1），请等待结束后再启动。")
                                                 .arg(prevSc.stepNo));
                    return;
                }

                if (prevSc.done || prevSc.passed || prevSc.failed)
                {
                    const bool resetOk = m_deviceManager->resetPlcSelfCheck();
                    if (!resetOk)
                    {
                        QMessageBox::warning(this,
                                             "系统自检",
                                             QString::fromUtf8("二次启动前自动复位失败，请检查 Reset 命令映射与PLC连接。"));
                        return;
                    }
                    QThread::msleep(80);
                }

                if (preOpenRegValves)
                {
                    const bool reg1Ok = m_deviceManager->setValveOpeningPercent(1, reg1Target);
                    const bool reg2Ok = m_deviceManager->setValveOpeningPercent(2, reg2Target);
                    if (!(reg1Ok && reg2Ok))
                    {
                        QMessageBox::warning(this,
                                             "系统自检",
                                             QString::fromUtf8("自检前调压阀预开失败：Reg1=%1%2，Reg2=%3%4\n请检查调压阀映射与PLC连接。")
                                                 .arg(reg1Ok ? QString::fromUtf8("成功") : QString::fromUtf8("失败"))
                                                 .arg(reg1Ok ? QString() : QString::fromUtf8("（阀1）"))
                                                 .arg(reg2Ok ? QString::fromUtf8("成功") : QString::fromUtf8("失败"))
                                                 .arg(reg2Ok ? QString() : QString::fromUtf8("（阀2）")));
                        return;
                    }

                    if (regSettleMs > 0)
                    {
                        QElapsedTimer regSettleTimer;
                        regSettleTimer.start();
                        while (regSettleTimer.elapsed() < regSettleMs)
                        {
                            m_deviceManager->updateAllDevices();
                            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 16);
                            QThread::msleep(20);
                        }
                    }

                    qInfo() << "[SelfCheck][Station1Panel] pre-open regulating valves done"
                            << "reg1Target=" << reg1Target
                            << "reg2Target=" << reg2Target
                            << "settleMs=" << regSettleMs;
                }

                const bool enableOk = m_deviceManager->setPlcSelfCheckEnable(true);
                bool startOk = false;

                if (enableOk || enableOptional)
                {
                    startOk = m_deviceManager->startPlcSelfCheck();
                }

                ok = (enableOk || enableOptional) && startOk;
                if (!ok)
                {
                    const QString plcErr = QString::fromStdString(m_deviceManager->getPlcLastError()).trimmed();
                    const int dbCmd = cfg.getInt("selfcheck.plc.db_cmd", 20);
                    const int enByte = cfg.getInt("selfcheck.plc.cmd.enable.byte_offset", 0);
                    const int enBit = cfg.getInt("selfcheck.plc.cmd.enable.bit_offset", 0);
                    const int stByte = cfg.getInt("selfcheck.plc.cmd.start.byte_offset", 0);
                    const int stBit = cfg.getInt("selfcheck.plc.cmd.start.bit_offset", 1);

                    QString failedStep;
                    if (!enableOk && !enableOptional)
                        failedStep = QString::fromUtf8("Enable 写入失败（严格模式）");
                    else if (!enableOk && enableOptional)
                        failedStep = QString::fromUtf8("Enable 写入失败（已按可选模式继续）但 Start 仍失败");
                    else
                        failedStep = QString::fromUtf8("Start 脉冲写入失败");

                    QMessageBox::warning(this,
                                             "系统自检",
                                             plcErr.isEmpty()
                                                 ? QString::fromUtf8("PLC 自检命令写入失败：") + failedStep +
                                                       QString::fromUtf8("\n当前映射：DB%1, Enable=DBX%2.%3, Start=DBX%4.%5")
                                                           .arg(dbCmd)
                                                           .arg(enByte).arg(enBit)
                                                           .arg(stByte).arg(stBit) +
                                                       QString::fromUtf8("\n请检查 Enable/Trigger 映射、外部可访问性以及 PLC 连接状态。")
                                                 : QString::fromUtf8("PLC 自检命令写入失败：") + plcErr +
                                                       QString::fromUtf8("\n失败步骤：") + failedStep +
                                                       QString::fromUtf8("\n当前映射：DB%1, Enable=DBX%2.%3, Start=DBX%4.%5")
                                                           .arg(dbCmd)
                                                           .arg(enByte).arg(enBit)
                                                           .arg(stByte).arg(stBit) +
                                                       QString::fromUtf8("\n请检查 Enable/Trigger 映射、外部可访问性以及 PLC 连接状态。"));
                    return;
                }
            }
            else
            {
                QMessageBox::warning(this, "系统自检", "PLC 未连接，无法下发自检命令。请先检查 PLC 连接状态。");
                return;
            }

            startSelfCheckStatusPolling();

            QDialog statusDialog(this);
            statusDialog.setWindowTitle(QString::fromUtf8("系统自检状态跟踪"));
            statusDialog.setWindowModality(Qt::WindowModal);
            statusDialog.resize(760, 540);
            statusDialog.setStyleSheet(
                "QDialog { background: #edf2f7; }"
                "QFrame#scCard {"
                " background: #ffffff;"
                " border: 1px solid #cfd9e3;"
                " border-radius: 14px;"
                " }"
                "QLabel#scTitle {"
                " color: #12324a;"
                " font-size: 20px;"
                " font-weight: 700;"
                " }"
                "QLabel#scSubtitle {"
                " color: #4d667d;"
                " font-size: 12px;"
                " }"
                "QLabel#scBadge {"
                " color: #ffffff;"
                " background: #2d8f74;"
                " border-radius: 12px;"
                " font-size: 12px;"
                " font-weight: 700;"
                " padding: 4px 10px;"
                " }"
                "QLabel#scMetricCard {"
                " background: #f7fafc;"
                " border: 1px solid #d4e0eb;"
                " border-radius: 10px;"
                " color: #223f56;"
                " font-size: 12px;"
                " padding: 6px 10px;"
                " }"
                "QLabel#scHint {"
                " color: #486177;"
                " font-size: 12px;"
                " }"
                "QTextEdit#scSummary {"
                " background: #f5f8fb;"
                " border: 1px solid #d5e1ec;"
                " border-radius: 10px;"
                " color: #1b3246;"
                " font-family: Consolas, 'Microsoft YaHei UI';"
                " font-size: 12px;"
                " }"
                "QProgressBar#scProgress {"
                " height: 14px;"
                " border: 1px solid #c7d6e4;"
                " border-radius: 7px;"
                " background: #e7eef5;"
                " text-align: center;"
                " color: #234158;"
                " font-size: 11px;"
                " }"
                "QProgressBar#scProgress::chunk {"
                " border-radius: 6px;"
                " background: #2d8f74;"
                " }"
                "QPushButton#scCloseBtn {"
                " min-width: 116px;"
                " min-height: 34px;"
                " border-radius: 10px;"
                " color: #ffffff;"
                " background: #2a6f97;"
                " border: 1px solid #255f83;"
                " font-weight: 700;"
                " }"
                "QPushButton#scCloseBtn:hover { background: #327da8; }"
            );

            auto *layout = new QVBoxLayout(&statusDialog);
            layout->setContentsMargins(14, 14, 14, 14);
            layout->setSpacing(10);

            auto *headerCard = new QFrame(&statusDialog);
            headerCard->setObjectName("scCard");
            auto *headerLayout = new QVBoxLayout(headerCard);
            headerLayout->setContentsMargins(14, 12, 14, 12);
            headerLayout->setSpacing(8);

            auto *titleRow = new QHBoxLayout();
            auto *titleLabel = new QLabel(QString::fromUtf8("PLC 自检实时跟踪"), headerCard);
            titleLabel->setObjectName("scTitle");
            auto *statusBadge = new QLabel(QString::fromUtf8("运行中"), headerCard);
            statusBadge->setObjectName("scBadge");
            titleRow->addWidget(titleLabel);
            titleRow->addStretch();
            titleRow->addWidget(statusBadge);

            auto *subtitleLabel = new QLabel(QString::fromUtf8("自检过程中自动刷新步骤、故障码与压力快照。"), headerCard);
            subtitleLabel->setObjectName("scSubtitle");
            subtitleLabel->setWordWrap(true);

            auto *progressBar = new QProgressBar(headerCard);
            progressBar->setObjectName("scProgress");
            progressBar->setRange(0, 100);
            progressBar->setValue(0);
            progressBar->setFormat(QString::fromUtf8("准备中 %p%"));

            auto *metricRow = new QHBoxLayout();
            metricRow->setSpacing(8);
            auto *metricRuntime = new QLabel(QString::fromUtf8("运行时长\n0 ms"), headerCard);
            metricRuntime->setObjectName("scMetricCard");
            auto *metricStep = new QLabel(QString::fromUtf8("当前步骤\n0 (空闲)"), headerCard);
            metricStep->setObjectName("scMetricCard");
            auto *metricFault = new QLabel(QString::fromUtf8("故障码\n0x0000"), headerCard);
            metricFault->setObjectName("scMetricCard");
            auto *metricResult = new QLabel(QString::fromUtf8("结果\n等待"), headerCard);
            metricResult->setObjectName("scMetricCard");
            metricRow->addWidget(metricRuntime, 1);
            metricRow->addWidget(metricStep, 1);
            metricRow->addWidget(metricFault, 1);
            metricRow->addWidget(metricResult, 1);

            headerLayout->addLayout(titleRow);
            headerLayout->addWidget(subtitleLabel);
            headerLayout->addWidget(progressBar);
            headerLayout->addLayout(metricRow);

            auto *summaryEdit = new QTextEdit(&statusDialog);
            summaryEdit->setObjectName("scSummary");
            summaryEdit->setReadOnly(true);
            summaryEdit->setMinimumHeight(280);

            auto *hintLabel = new QLabel(QString::fromUtf8("提示：窗口可保持打开；自检到达终态后会自动清零 Enable。"), &statusDialog);
            hintLabel->setObjectName("scHint");
            hintLabel->setWordWrap(true);

            auto *closeBtn = new QPushButton(QString::fromUtf8("关闭"), &statusDialog);
            closeBtn->setObjectName("scCloseBtn");
            auto *buttonRow = new QHBoxLayout();
            buttonRow->addStretch();
            buttonRow->addWidget(closeBtn);

            layout->addWidget(headerCard);
            layout->addWidget(summaryEdit, 1);
            layout->addWidget(hintLabel);
            layout->addLayout(buttonRow);

            QObject::connect(closeBtn, &QPushButton::clicked, &statusDialog, &QDialog::accept);

            QElapsedTimer elapsedTimer;
            elapsedTimer.start();
            auto refreshStatus = [this, summaryEdit, titleLabel, subtitleLabel, closeBtn, elapsedTimer, statusBadge, progressBar, metricRuntime, metricStep, metricFault, metricResult]() mutable
            {
                auto stepProgress = [](int stepNo, bool done, bool passed, bool failed) -> int {
                    if (failed)
                        return 100;
                    if (passed || done || stepNo >= 90)
                        return 100;
                    if (stepNo <= 0)
                        return 0;
                    if (stepNo <= 5)
                        return 10;
                    if (stepNo <= 10)
                        return 28;
                    if (stepNo <= 20)
                        return 48;
                    if (stepNo <= 30)
                        return 72;
                    if (stepNo <= 40)
                        return 92;
                    return 96;
                };

                if (!m_deviceManager)
                {
                    summaryEdit->setPlainText(QString::fromUtf8("设备管理器不可用，无法读取自检状态。"));
                    titleLabel->setText(QString::fromUtf8("PLC 自检状态读取失败"));
                    subtitleLabel->setText(QString::fromUtf8("请检查设备管理器初始化状态与PLC链路。"));
                    statusBadge->setText(QString::fromUtf8("读取失败"));
                    statusBadge->setStyleSheet(QStringLiteral("QLabel#scBadge { color:#ffffff; background:#c24b45; border-radius:12px; font-size:12px; font-weight:700; padding:4px 10px; }"));
                    progressBar->setValue(0);
                    progressBar->setFormat(QString::fromUtf8("状态不可用 %p%"));
                    metricRuntime->setText(QString::fromUtf8("运行时长\n--"));
                    metricStep->setText(QString::fromUtf8("当前步骤\n--"));
                    metricFault->setText(QString::fromUtf8("故障码\n--"));
                    metricResult->setText(QString::fromUtf8("结果\n读取失败"));
                    return;
                }

                // 自检跟踪窗口期间同步刷新 1 号操作台图元，让阀门开关动作可视化。
                updateRelayButtons(true);
                updateSensorValues(true);

                const auto sc = m_deviceManager->getPlcSelfCheckStatus();
                QString text = selfCheckStatusText(sc);
                const qint64 elapsedMs = elapsedTimer.elapsed();
                text.prepend(QString::fromUtf8("运行时长: %1 ms\n\n").arg(elapsedMs));
                summaryEdit->setPlainText(text);

                const int progress = stepProgress(sc.stepNo, sc.done, sc.passed, sc.failed);
                progressBar->setValue(progress);
                progressBar->setFormat(QString::fromUtf8("流程进度 %1%" ).arg(progress));
                metricRuntime->setText(QString::fromUtf8("运行时长\n%1 ms").arg(elapsedMs));
                metricStep->setText(QString::fromUtf8("当前步骤\n%1 (%2)").arg(sc.stepNo).arg(selfCheckStepText(sc.stepNo)));
                metricFault->setText(QString::fromUtf8("故障码\n0x%1").arg(QString::number(sc.faultCode, 16).rightJustified(4, '0').toUpper()));

                if (sc.failed)
                {
                    titleLabel->setText(QString::fromUtf8("系统自检状态跟踪 - 失败"));
                    subtitleLabel->setText(QString::fromUtf8("检测到故障，建议记录故障码并回查步骤判据。"));
                    statusBadge->setText(QString::fromUtf8("失败"));
                    statusBadge->setStyleSheet(QStringLiteral("QLabel#scBadge { color:#ffffff; background:#bb3e3e; border-radius:12px; font-size:12px; font-weight:700; padding:4px 10px; }"));
                    progressBar->setStyleSheet(QStringLiteral("QProgressBar#scProgress { height:14px; border:1px solid #e1c9c9; border-radius:7px; background:#f5e7e7; text-align:center; color:#6c2a2a; font-size:11px; } QProgressBar#scProgress::chunk { border-radius:6px; background:#bb3e3e; }"));
                    metricResult->setText(QString::fromUtf8("结果\n失败"));
                    closeBtn->setText(QString::fromUtf8("关闭"));
                }
                else if (sc.passed)
                {
                    titleLabel->setText(QString::fromUtf8("系统自检状态跟踪 - 通过"));
                    subtitleLabel->setText(QString::fromUtf8("所有阶段判据满足，系统自检通过。"));
                    statusBadge->setText(QString::fromUtf8("通过"));
                    statusBadge->setStyleSheet(QStringLiteral("QLabel#scBadge { color:#ffffff; background:#2d8f74; border-radius:12px; font-size:12px; font-weight:700; padding:4px 10px; }"));
                    progressBar->setStyleSheet(QStringLiteral("QProgressBar#scProgress { height:14px; border:1px solid #c7d6e4; border-radius:7px; background:#e7eef5; text-align:center; color:#234158; font-size:11px; } QProgressBar#scProgress::chunk { border-radius:6px; background:#2d8f74; }"));
                    metricResult->setText(QString::fromUtf8("结果\n通过"));
                    closeBtn->setText(QString::fromUtf8("关闭"));
                }
                else if (sc.done)
                {
                    titleLabel->setText(QString::fromUtf8("系统自检状态跟踪 - 已结束"));
                    subtitleLabel->setText(QString::fromUtf8("流程已结束，请确认最终结果与现场状态。"));
                    statusBadge->setText(QString::fromUtf8("已结束"));
                    statusBadge->setStyleSheet(QStringLiteral("QLabel#scBadge { color:#ffffff; background:#3f7193; border-radius:12px; font-size:12px; font-weight:700; padding:4px 10px; }"));
                    progressBar->setStyleSheet(QStringLiteral("QProgressBar#scProgress { height:14px; border:1px solid #c7d6e4; border-radius:7px; background:#e7eef5; text-align:center; color:#234158; font-size:11px; } QProgressBar#scProgress::chunk { border-radius:6px; background:#3f7193; }"));
                    metricResult->setText(QString::fromUtf8("结果\n结束"));
                    closeBtn->setText(QString::fromUtf8("关闭"));
                }
                else if (sc.busy)
                {
                    titleLabel->setText(QString::fromUtf8("系统自检状态跟踪 - 运行中"));
                    subtitleLabel->setText(QString::fromUtf8("正在执行自检流程，图元状态与压力数据实时更新。"));
                    statusBadge->setText(QString::fromUtf8("运行中"));
                    statusBadge->setStyleSheet(QStringLiteral("QLabel#scBadge { color:#ffffff; background:#c07a2a; border-radius:12px; font-size:12px; font-weight:700; padding:4px 10px; }"));
                    progressBar->setStyleSheet(QStringLiteral("QProgressBar#scProgress { height:14px; border:1px solid #d7cfbf; border-radius:7px; background:#f4eee2; text-align:center; color:#5a4121; font-size:11px; } QProgressBar#scProgress::chunk { border-radius:6px; background:#c07a2a; }"));
                    metricResult->setText(QString::fromUtf8("结果\n进行中"));
                    closeBtn->setText(QString::fromUtf8("关闭"));
                }
                else
                {
                    titleLabel->setText(QString::fromUtf8("系统自检状态跟踪 - 等待启动"));
                    subtitleLabel->setText(QString::fromUtf8("命令已下发，等待PLC进入自检状态。"));
                    statusBadge->setText(QString::fromUtf8("等待"));
                    statusBadge->setStyleSheet(QStringLiteral("QLabel#scBadge { color:#ffffff; background:#5d7388; border-radius:12px; font-size:12px; font-weight:700; padding:4px 10px; }"));
                    progressBar->setStyleSheet(QStringLiteral("QProgressBar#scProgress { height:14px; border:1px solid #c7d6e4; border-radius:7px; background:#e7eef5; text-align:center; color:#234158; font-size:11px; } QProgressBar#scProgress::chunk { border-radius:6px; background:#5d7388; }"));
                    metricResult->setText(QString::fromUtf8("结果\n等待"));
                }
            };

            QTimer statusTimer(&statusDialog);
            QObject::connect(&statusTimer, &QTimer::timeout, &statusDialog, refreshStatus);
            statusTimer.start(300);
            refreshStatus();

            statusDialog.exec();
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
                     ok ? QString::fromUtf8("在线, 当前开度: %1%")
                         .arg(vreg.openingPercent, 0, 'f', 0)
                       : QString::fromUtf8("离线或未配置"));
        }

        auto controlRelay = [&](uint8_t index, bool on) -> bool {
            constexpr int kRelayActionGapMs = 180;
            if (m_stationClient && m_stationClient->isConnected()) {
                ControlCommand cmd;
                cmd.command_type = 0;
                cmd.index = index;
                cmd.action = on ? 1 : 0;
                const bool ok = m_stationClient->sendCommand(cmd);
                if (ok)
                    QThread::msleep(kRelayActionGapMs);
                return ok;
            }
            const bool ok = m_deviceManager->setRelay(index, on);
            if (ok)
                QThread::msleep(kRelayActionGapMs);
            return ok;
        };
        auto controlPump = [&](uint8_t index, bool on) -> bool {
            if (m_stationClient && m_stationClient->isConnected()) {
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
                                ? QString::fromUtf8("通路正常（PS4 变化 %1 kPa \u2265 阈值 %2 kPa）")
                                    .arg(fmtPressure(delta, 2)).arg(fmtPressure(linkagePressureThr, 2))
                                : QString::fromUtf8("通路可能异常（PS4 变化 %1 kPa，阈值 %2 kPa）")
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

        // ===== 步骤3 & 4：高压泄漏测试（前后压力对比） =====
        if (canDoLeakTest) {
            transitionTo(SelfCheckFlowState::STEP3_OBSERVE, QString::fromUtf8("进入步骤3高压泄漏测试"));

            // 先关闭待测阀（电磁阀3），让待测区前后两段进入可观测状态。
            bool prepOk = controlRelay(2, false);
            if (!prepOk) {
                setStep(IDX_STEP3, STEP_FAIL, QString::fromUtf8("待测阀关闭失败，已中止"));
                setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("前置失败，未形成结论"));
                transitionTo(SelfCheckFlowState::FAULT_STOP, QString::fromUtf8("待测阀关闭失败"));
            } else {
                bool pumpStartedBySelfCheck = false;
                bool skipLeakResult = false;
                if (!pumpManualForLeakTest) {
                    setStep(IDX_STEP3, RUNNING, QString::fromUtf8("正在启动气泵并升压…"));
                    if (!controlPump(0, true)) {
                        setStep(IDX_STEP3, STEP_FAIL, QString::fromUtf8("气泵启动失败，无法建压"));
                        setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("建压失败，未形成结论"));
                        skipLeakResult = true;
                    } else {
                        pumpStartedBySelfCheck = true;
                    }
                }
                if (!skipLeakResult) {
                    setStep(IDX_STEP3, RUNNING, QString::fromUtf8("等待待测阀前压力达到 %1 kPa…").arg(fmtPressure(leakBuildMinKpa, 1)));

                    auto readPressurePair = [&]() -> std::pair<PressureSensor, PressureSensor> {
                        m_deviceManager->updateAllDevices();
                        return {m_deviceManager->getPressureSensor(4), m_deviceManager->getPressureSensor(5)};
                    };

                    PressureSensor p4Build;
                    PressureSensor p5Build;
                    {
                        const qint64 deadlineMs = QDateTime::currentMSecsSinceEpoch() + leakBuildWaitMs;
                        while (true) {
                            const auto pair = readPressurePair();
                            p4Build = pair.first;
                            p5Build = pair.second;
                            if (p4Build.pressure >= leakBuildMinKpa)
                                break;
                            if (QDateTime::currentMSecsSinceEpoch() >= deadlineMs)
                                break;
                            QCoreApplication::processEvents();
                            QThread::msleep(100);
                        }
                    }

                    const bool buildOk = (p4Build.pressure >= leakBuildMinKpa);
                    if (pumpStartedBySelfCheck)
                        (void)controlPump(0, false);
                    if (!buildOk) {
                        setStep(IDX_STEP3, STEP_FAIL,
                                QString::fromUtf8("建压不足（PS4=%1 kPa < 最小建压 %2 kPa）")
                                    .arg(fmtPressure(p4Build.pressure, 1)).arg(fmtPressure(leakBuildMinKpa, 1)));
                        setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("建压不足，无法有效判定泄漏"));
                        transitionTo(SelfCheckFlowState::FAULT_STOP, QString::fromUtf8("步骤3建压不足"));
                    } else {
                        setStep(IDX_STEP3, RUNNING,
                                QString::fromUtf8("PS4达到 %1 kPa，依次关闭电磁阀1/2/4 并保压 %2 ms…")
                                    .arg(fmtPressure(p4Build.pressure, 1)).arg(leakHoldWaitMs));
                        (void)controlRelay(0, false);
                        (void)controlRelay(1, false);
                        (void)controlRelay(3, false);
                        waitMs(leakHoldWaitMs);
                        const auto pHoldPair = readPressurePair();
                        const auto p4Hold = pHoldPair.first;
                        const auto p5Hold = pHoldPair.second;
                        const float p4Delta = p4Build.pressure - p4Hold.pressure;
                        const float p4AbsDelta = std::abs(p4Delta);
                        const QString p4Direction = (p4Delta >= 0) ? QString::fromUtf8("升") : QString::fromUtf8("降");
                        const float p5Delta = p5Hold.pressure - p5Build.pressure;
                        const float p5AbsDelta = std::abs(p5Delta);
                        const QString p5Direction = (p5Delta >= 0) ? QString::fromUtf8("升") : QString::fromUtf8("降");
                        setStep(IDX_STEP3, STEP_OK,
                                QString::fromUtf8("已采集：PS4 %1\u2192%2 kPa（%3 %4），PS5 %5\u2192%6 kPa（%7 %8）")
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
                        const bool p4DropExceeds = (p4Delta >= leakP4DropMaxKpa);
                        const bool p5RiseExceeds = (p5Delta >= leakP5RiseMaxKpa);
                        const bool externalLeak = p4DropExceeds && !p5RiseExceeds;
                        const bool internalLeak = p5RiseExceeds && (p4Delta > 0.0f);
                        const bool leakOk = !externalLeak && !internalLeak;
                        setStep(IDX_STEP4, leakOk ? STEP_OK : STEP_FAIL,
                                leakOk
                                    ? QString::fromUtf8("密封正常（PS4下降 %1 kPa，PS5上升 %2 kPa）")
                                        .arg(fmtPressure(p4Delta, 2)).arg(fmtPressure(p5Delta, 2))
                                    : externalLeak
                                        ? QString::fromUtf8("外泄漏：PS4下降 %1 kPa > 阈值 %2 kPa，PS5无明显上升")
                                            .arg(fmtPressure(p4Delta, 2)).arg(fmtPressure(leakP4DropMaxKpa, 2))
                                        : QString::fromUtf8("内泄漏：PS5上升 %1 kPa > 阈值 %2 kPa，且 PS4 同步下降")
                                            .arg(fmtPressure(p5Delta, 2)).arg(fmtPressure(leakP5RiseMaxKpa, 2)));
                        if (externalLeak || internalLeak)
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
    /**
     * @brief 拦截 QGraphicsView::viewport 的鼠标释放事件，实现流程图阀门图元点击控制。
     *
     * 点击流程：
     *   1. 命中检测：从鼠标位置向上遍历 parentItem 链，找到有 data(4) 的图元
     *   2. 防抖：同一 relay index 600 ms 内的重复点击被忽略；全局锁 700 ms
     *   3. 通过 onRelayBtnClicked() 执行实际开关逻辑
     *
     * 全局锁（m_relayGlyphLockUntilMs）防止多个图元同时被点击或动画帧触发二次命中。
     */
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

    /**
     * @brief 从 DeviceManager 回读所有继电器状态，刷新按钮文字/颜色和阀门图元。
     *
     * @param force true=强制刷新（即使面板不可见），false=仅在 isVisible() 时执行
     *
     * 执行逻辑：
     * 1. 读取每个 relay 的当前通断状态（getRelayState）
     * 2. 若是 M100 通道，检查"预期保持"是否被 PLC 联锁复位（弹窗提示）
     * 3. M100.0~M100.3 的图元颜色通过 setRelayValveGlyphState 联动
     * 4. strict_remote_mode=true 时跳过按钮文字刷新（避免误覆盖远程状态）
     * 5. 只在状态实际变化时重绘按钮，避免闪烁
     */
    void Station1Panel::updateRelayButtons(bool force)
    {
        if (!force && !isVisible())
            return;

        const bool skipRelayButtonPaint = (m_stationClient && m_stationClient->isConnected());

        if (!m_deviceManager)
            return;
        if (m_autoSequenceRunning)
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

            // 图元颜色联动：M100.0 ~ M100.3 以及电磁阀5(M100.4) 对应图元随 relay 状态变化。
            if (relayNeedsGlyphUpdate(relays[i].index))
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

    /**
     * @brief 处理继电器切换请求（来自按钮面板或流程图图元点击）。
     *
     * @param index  继电器线性索引（对应 RelayDef::index）
     * @param source 调用来源标识（"relay_panel_button" / "glyph"），仅用于调试日志
     *
     * 执行流程：
     * 1. M100 通道防抖：同一路 700 ms 内重复操作被丢弃（m_lastM100ToggleMs）
     * 2. 泵类通道（type=="pump"）拦截并弹出"现场联调"提示，不执行切换
     * 3. 读取当前状态（getRelayState 或按钮缓存），取反得到目标状态
     * 4. 发送控制命令：
     *    - 远程模式（m_stationClient && strict_remote_mode）：通过 StationClient::sendCommand
     *    - 本地模式：通过 DeviceManager::setRelay
     * 5. 成功后做"乐观更新"：立即刷新按钮和图元，避免等待下一个 relayTimer 周期
     * 6. 本地模式下 250 ms 后执行 updateRelayButtons() 校准真实状态
     */
    void Station1Panel::onRelayBtnClicked(uint8_t index, const char *source)
    {
        if (m_deviceManager)
        {
            const auto sc = m_deviceManager->getPlcSelfCheckStatus();
            if (sc.busy)
            {
                QMessageBox::information(this,
                                         "自检运行中",
                                         "PLC 系统自检正在运行，已临时锁定手动阀门控制，避免与自检输出仲裁冲突。\n"
                                         "请等待自检结束后再进行手动操作。");
                return;
            }
        }

        if (relayUsesM100_4(index))
        {
            qInfo() << "[M100][Station1Panel] relay mapped to M100.4"
                    << "index=" << index;
        }

        bool current = false;
        if (m_deviceManager)
            m_deviceManager->getRelayState(index, current);

        qInfo() << "[M100][Station1Panel] relay toggle prepare"
                << "index=" << index
                << "current=" << current
                << "target=" << (!current);

        if (!controlRelayState(index, !current, source))
        {
            const auto relayDefs = relayDefsForStation(m_panelConfig.stationNumber);
            const RelayDef *def = findRelayDefByIndex(relayDefs, index);
            const QString addr = def ? def->addr : QString("Q?");
            if (isM100RelayIndex(index))
            {
                QMessageBox::warning(this, "操作失败",
                                     QString("切换 %1 失败，请检查基础控制 DB 是否关闭优化访问、并允许绝对地址读写。").arg(addr));
            }
            else
            {
                QMessageBox::warning(this, "操作失败",
                                     QString("切换 %1 失败，请检查 PLC/终端连接状态。").arg(addr));
            }
        }
    }

    bool Station1Panel::controlStartStop(bool start, const char *source)
    {
        if (!m_deviceManager || !m_deviceManager->isPlcConnected())
        {
            qWarning() << "[StartStop][Station1Panel] request rejected because PLC is disconnected"
                       << "src=" << source
                       << "start=" << start;
            return false;
        }

        Q_UNUSED(start);
        Q_UNUSED(source);

        // 泵启停当前暂停，不下发任何控制命令。
        // 这条路径保留接口，等后续重新接回泵控制逻辑时再恢复。
        return true;
    }

    void Station1Panel::onStartButtonClicked(const char *source)
    {
        qInfo() << "[Station1Panel] start requested" << "source=" << source;

        const QString sourceText = QString::fromUtf8(source ? source : "");
        if (sourceText == "ui")
        {
            if (m_autoSequenceRunning)
                return;
            if (!m_deviceManager || !m_deviceManager->isPlcConnected())
            {
                QMessageBox::warning(this, "操作失败", "PLC 未连接，无法执行开始流程。请先连接 PLC。");
                return;
            }

            const std::array<std::pair<const char *, bool (Station1Panel::*)()>, 4> actions{{
                {"开阀测试", &Station1Panel::autoLowPressureOpenValve},
                {"低压内泄漏", &Station1Panel::autoLowPressureInternalLeakStage},
                {"高压泄露", &Station1Panel::autoHighPressureLeakStage},
                {"流量测试", &Station1Panel::autoFlowTestStage},
            }};

            m_autoSequenceRunning = true;
            if (m_startBtn)
                m_startBtn->setEnabled(false);
            if (m_stopBtn)
                m_stopBtn->setEnabled(false);

            for (int i = 0; i < static_cast<int>(actions.size()); ++i)
            {
                const auto &[name, action] = actions[static_cast<size_t>(i)];
                setStageOverviewState(i,
                                      QString::fromUtf8(name),
                                      QString::fromUtf8("等待执行"),
                                      true);
            }

            bool allOk = true;
            for (int i = 0; i < static_cast<int>(actions.size()); ++i)
            {
                const auto &[name, action] = actions[static_cast<size_t>(i)];
                qInfo() << "[Station1Panel] start auto action begin" << name;
                setActiveStageIndex(i);
                setStageOverviewState(i,
                                      QString::fromUtf8(name),
                                      QString::fromUtf8("执行中…"),
                                      true);
                const bool ok = (this->*action)();
                if (!ok)
                {
                    qWarning() << "[Station1Panel] auto action failed" << name;
                    allOk = false;
                    break;
                }
            }

            if (m_startBtn)
                m_startBtn->setEnabled(true);
            if (m_stopBtn)
                m_stopBtn->setEnabled(true);
            m_autoSequenceRunning = false;

            // 不弹窗阻塞，直接显示结果在阶段概览中，避免事件循环重入导致崩溃
            qInfo() << "[Station1Panel] auto sequence finished" << "allOk=" << allOk;
            return;
        }

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
        m_flowPipeItems.clear();
        m_pressureSensorItems.fill(nullptr);
        m_valveItems.fill(nullptr);
        m_regulatingValveItems.fill(nullptr);
        m_flowMeterItem = nullptr;

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

        Q_UNUSED(m_panelConfig);
        labelV1 = QString::fromUtf8("电磁阀1");
        labelV2 = QString::fromUtf8("电磁阀2");
        labelVReg1 = QString::fromUtf8("电动调压阀1");
        labelTestValve = QString::fromUtf8("待测电磁阀");
        labelVBack1 = QString::fromUtf8("电磁阀4");
        labelVReg2 = QString::fromUtf8("电动调压阀2");
        valveIdV1 = 1;
        valveIdV2 = 2;
        valveIdTest = 3;
        valveIdBack = 4;

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

        // ==== 图标化设备布局（单套简化布局）====
        // 1号台现在只保留这一套布局。
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

        // 1号台固定坐标：只保留这一套简化布局。
        const qreal xAcc = 200.0;
        const qreal xFmStation1 = 1650.0;
        const qreal fmTopTransitionPipeLen = 90.0;
        const qreal fmRightPipeLen = 180.0;
        const qreal station1InletPipeLen = 240.0;
        const qreal station1TopRowShift = -160.0;
        const qreal station1V1ShiftX = -100.0;
        const qreal valveOutletOffsetX = ElectricValveItem::outletPortLocal().x();
        const qreal valveInletOffsetX = ElectricValveItem::inletPortLocal().x();
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
        const qreal xV1 = ((xAcc + station1TopRowShift) + (station1VRegAnchorX - (xAcc + station1TopRowShift)) / 3.0) + station1V1ShiftX;
        const qreal xV2 = xV1 + station1ValveToValvePipeLen + (valveOutletOffsetX - valveInletOffsetX) * kDeviceItemScale;
        const qreal xVReg = xV2 + station1ValveToValvePipeLen + (valveOutletOffsetX - valveInletOffsetX) * kDeviceItemScale;
        const uint16_t topRegValveId = static_cast<uint16_t>(std::max(1, m_panelConfig.stationNumber * 2 - 1));
        const uint16_t bottomRegValveId = static_cast<uint16_t>(topRegValveId + 1);
        const qreal xPs1 = (xV1 + xV2) * 0.5;
        const qreal xPs2 = (xV2 + xVReg) * 0.5;

        // 下排与上排对齐：流量计放到右侧竖向落点，待测阀3与电动调压阀1同列。
        const qreal station1FlowMeterShiftX = -140.0;
        const qreal xFm = xFmStation1 + station1TopRowShift + station1FlowMeterShiftX;
        const qreal yFm = yRow1 + row1AfterAccumulatorYOffset;
        const qreal flowMeterScale = 1.25;
        const qreal xTestValve = xFm;
        const qreal xVBack1 = xVReg;
        const qreal xVBackReg = xV2;
        const qreal xV5 = xV1;
        const qreal xPs8 = xPs1;
        const qreal xPt1 = xFm;
        const qreal xPt2 = xPs2;
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

        auto addDeferred = [&](QGraphicsItem *it)
        {
            if (!it)
                return;
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

        // 第一排（从左到右）
        auto *ps3 = new SensorItem(QString::fromUtf8("压力3"), "kPa", kUiPurple);
        ps3->setData(1, static_cast<int>(configuredPressureSensorId(m_panelConfig, 3)));
        addDeferred(ps3);
        if (const uint16_t sensorId = configuredPressureSensorId(m_panelConfig, 3); sensorId < m_pressureSensorItems.size())
            m_pressureSensorItems[sensorId] = ps3;

        auto *v1 = new ElectricValveItem(labelV1, makeGlyphTheme(), true, 100.0);
        place(v1, xV1, yRow1 + row1AfterAccumulatorYOffset);
        v1->setData(3, valveIdV1);
        v1->setData(4, relayGlyphMap.v1);
        v1->setFlag(QGraphicsItem::ItemIsSelectable, false);
        if (valveIdV1 < static_cast<int>(m_valveItems.size()))
            m_valveItems[static_cast<size_t>(valveIdV1)] = v1;

        // 压力传感器上置：使底部红点与主干管道平齐。
        auto *ps4 = new SensorItem(QString::fromUtf8("压力4"), "kPa", kUiPurple);
        ps4->setData(1, static_cast<int>(configuredPressureSensorId(m_panelConfig, 4)));
        addDeferred(ps4);
        if (const uint16_t sensorId = configuredPressureSensorId(m_panelConfig, 4); sensorId < m_pressureSensorItems.size())
            m_pressureSensorItems[sensorId] = ps4;

        auto *v2 = new ElectricValveItem(labelV2, makeGlyphTheme(), true, 100.1);
        place(v2, xV2, yRow1 + row1AfterAccumulatorYOffset);
        v2->setData(3, valveIdV2);
        v2->setData(4, relayGlyphMap.v2);
        v2->setFlag(QGraphicsItem::ItemIsSelectable, false);
        if (valveIdV2 < static_cast<int>(m_valveItems.size()))
            m_valveItems[static_cast<size_t>(valveIdV2)] = v2;

        auto *ps5 = new SensorItem(QString::fromUtf8("压力5"), "kPa", kUiPurple);
        ps5->setData(1, static_cast<int>(configuredPressureSensorId(m_panelConfig, 5)));
        addDeferred(ps5);
        if (const uint16_t sensorId = configuredPressureSensorId(m_panelConfig, 5); sensorId < m_pressureSensorItems.size())
            m_pressureSensorItems[sensorId] = ps5;

        auto *vReg = new RegulatingValveItem(labelVReg1, makeGlyphTheme(), false, 0.0);
        place(vReg, xVReg, yRow1 + row1AfterAccumulatorYOffset);
        vReg->setData(3, 6);
        vReg->setData(6, static_cast<int>(topRegValveId));
        if (topRegValveId < m_regulatingValveItems.size())
            m_regulatingValveItems[topRegValveId] = vReg;

        

        // 第二排（从右到左，列 6~0）
        auto *fm = new FlowMeterItem("流量计");
        place(fm, xFm, yFm);
        fm->setScale(flowMeterScale);
        m_flowMeterItem = fm;
        const qreal targetMainPipeY = vReg->mapToScene(RegulatingValveItem::outletPortLocal()).y();
        const qreal fmLeftPortY = fm->mapToScene(flowMeterLeftPortLocal).y();
        fm->setY(fm->y() + (targetMainPipeY - fmLeftPortY));

        auto *ps6 = new SensorItem(QString::fromUtf8("压力6"), "kPa", kUiOrange);
        ps6->setData(1, static_cast<int>(configuredPressureSensorId(m_panelConfig, 6)));
        ps6->setData(2, "kPa");
        addDeferred(ps6);
        if (const uint16_t sensorId = configuredPressureSensorId(m_panelConfig, 6); sensorId < m_pressureSensorItems.size())
            m_pressureSensorItems[sensorId] = ps6;

        auto *testValve = new ElectricValveItem(labelTestValve, makeGlyphTheme(), false, 100.2);
        place(testValve, xTestValve, yRow2 + row2AfterFlowMeterYOffset);
        testValve->setData(3, valveIdTest);
        testValve->setData(4, relayGlyphMap.test);
        testValve->setFlag(QGraphicsItem::ItemIsSelectable, false);
        if (valveIdTest < static_cast<int>(m_valveItems.size()))
            m_valveItems[static_cast<size_t>(valveIdTest)] = testValve;

        auto *ps7 = new SensorItem(QString::fromUtf8("压力7"), "kPa", kUiOrange);
        ps7->setData(1, static_cast<int>(configuredPressureSensorId(m_panelConfig, 7)));
        ps7->setData(2, "kPa");
        addDeferred(ps7);
        if (const uint16_t sensorId = configuredPressureSensorId(m_panelConfig, 7); sensorId < m_pressureSensorItems.size())
            m_pressureSensorItems[sensorId] = ps7;

        auto *vBack1 = new ElectricValveItem(labelVBack1, makeGlyphTheme(), true, 100.3);
        place(vBack1, xVBack1, yRow2 + row2AfterFlowMeterYOffset);
        vBack1->setData(3, valveIdBack);
        vBack1->setData(4, relayGlyphMap.v3);
        vBack1->setFlag(QGraphicsItem::ItemIsSelectable, false);
        if (valveIdBack < static_cast<int>(m_valveItems.size()))
            m_valveItems[static_cast<size_t>(valveIdBack)] = vBack1;

        auto *vBackReg = new RegulatingValveItem(labelVReg2, makeGlyphTheme(), false, 0.0);
        place(vBackReg, xVBackReg, yRow2 + row2AfterFlowMeterYOffset);
        vBackReg->setData(3, 9);
        vBackReg->setData(6, static_cast<int>(bottomRegValveId));
        if (bottomRegValveId < m_regulatingValveItems.size())
            m_regulatingValveItems[bottomRegValveId] = vBackReg;

        auto *ps8 = new SensorItem(QString("压力8"), "kPa", kUiOrange);
        ps8->setData(1, 8);
        ps8->setData(2, "kPa");
        addDeferred(ps8);
        m_pressureSensorItems[8] = ps8;

        auto *v5 = new ElectricValveItem(QString::fromUtf8("电磁阀5"), makeGlyphTheme(), true, 100.0);
        place(v5, xV5, yRow2 + row2AfterFlowMeterYOffset);
        v5->setData(3, 5);
        v5->setData(4, 5);
        v5->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_valveItems[5] = v5;

        auto alignSensorAnchorToPipeMid = [&](SensorItem *sensor, const QPointF &pipeStart, const QPointF &pipeEnd)
        {
            if (!sensor)
                return;

            const QPointF mid((pipeStart.x() + pipeEnd.x()) * 0.5,
                              (pipeStart.y() + pipeEnd.y()) * 0.5);
            const QPointF anchorLocal = SensorItem::inletPortLocal();
            const QPointF anchorScene = sensor->mapToScene(anchorLocal);
            const QPointF delta = mid - anchorScene;
            sensor->setPos(sensor->pos() + delta);
        };

        alignSensorAnchorToPipeMid(ps4,
                                   v1->mapToScene(ElectricValveItem::outletPortLocal()),
                                   v2->mapToScene(ElectricValveItem::inletPortLocal()));
        alignSensorAnchorToPipeMid(ps5,
                                   v2->mapToScene(ElectricValveItem::outletPortLocal()),
                                   vReg->mapToScene(RegulatingValveItem::inletPortLocal()));

        {
            // ps6 放在 p2 与电磁阀右侧接口之间的管道中点，避免随着管线长度变化漂移。
            //
            // 几何含义：
            // fmRight        : 流量计右出口
            // valveRightPort : 电磁阀右侧端口
            // elbowX         : 右侧拐点所在的统一 x 坐标
            // p1       : 上拐点（先向右走到这里）
            // p2       : 下拐点（再沿竖线下落到这里）
            const QPointF fmRight = fm->mapToScene(flowMeterRightPortLocal);
            const QPointF valveRightPort = testValve->mapToScene(ElectricValveItem::outletPortLocal());
            const qreal elbowX = std::max(fmRight.x(), valveRightPort.x()) + fmRightPipeLen;
            const QPointF p1(elbowX, fmRight.y());
            const QPointF p2(elbowX, valveRightPort.y());
            alignSensorAnchorToPipeMid(ps6, p2, valveRightPort);
        }

        alignSensorAnchorToPipeMid(ps7,
                                   testValve->mapToScene(ElectricValveItem::inletPortLocal()),
                                   vBack1->mapToScene(ElectricValveItem::outletPortLocal()));

        alignSensorAnchorToPipeMid(ps8,
                                   vBackReg->mapToScene(RegulatingValveItem::inletPortLocal()),
                                   v5->mapToScene(ElectricValveItem::outletPortLocal()));

        // 1号台关键图元按端口精确对齐，避免仅按图元中心对齐造成视觉误差。
        alignItemPortX(vBack1, ElectricValveItem::inletPortLocal(), vReg, RegulatingValveItem::inletPortLocal());
        alignItemPortX(vBackReg, RegulatingValveItem::inletPortLocal(), v2, ElectricValveItem::inletPortLocal());
        alignItemPortX(v5, ElectricValveItem::inletPortLocal(), v1, ElectricValveItem::inletPortLocal());
        alignItemPortX(ps8, SensorItem::inletPortLocal(), ps4, SensorItem::inletPortLocal());

        // ==== 管道连接（四段主流程，按工艺流向编号）====

        // 管路 1：上排供压主线（单布局）
        const QPointF end = v1->mapToScene(ElectricValveItem::inletPortLocal());
        const QPointF start(end.x() - station1InletPipeLen, end.y());
        QPainterPath path(start);
        path.lineTo(end);
        addHmiPipeWithArrow(m_scene, path, end, start, 1);
        alignSensorAnchorToPipeMid(ps3, start, end);
        connectPorts(m_scene, v1->mapToScene(ElectricValveItem::outletPortLocal()), v2->mapToScene(ElectricValveItem::inletPortLocal()), 2);
        connectPorts(m_scene, v2->mapToScene(ElectricValveItem::outletPortLocal()), vReg->mapToScene(RegulatingValveItem::inletPortLocal()), 3);

        // 管路 2：上排到下排的跨排过渡线
        // 路径：电动调压阀出口 ->（向右预留）->（垂直下行）-> 流量计入口侧。
        // 描述：该段负责完成上排主线到下排测试回路的“换行”连接；
        //       走线采用“水平 -> 垂直 -> 水平”折线，确保跨排连接无斜线。
        //       其中 +50 为预留水平过渡段长度，用于避免与设备本体过近。
        {
            const QPointF start = vReg->mapToScene(RegulatingValveItem::outletPortLocal());
            const QPointF end = fm->mapToScene(flowMeterLeftPortLocal);
            QPainterPath path(start);
            const QPointF p1(start.x() + fmTopTransitionPipeLen, start.y());
            const QPointF p2(p1.x(), end.y());
            path.lineTo(p1);
            path.lineTo(p2);
            path.lineTo(end);
            addHmiPipeWithArrow(m_scene, path, end, p2, 4);
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
            const QPointF end = testValve->mapToScene(ElectricValveItem::outletPortLocal());
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
                                      kPipeFlowWidth + 1.0,
                                      5);

        }
        connectPorts(m_scene, testValve->mapToScene(ElectricValveItem::inletPortLocal()), vBack1->mapToScene(ElectricValveItem::outletPortLocal()), 6);

        // 管路 4：下排收口段
        // 1号台：回路电动阀（vBack1） -> 回路电动调压阀（vBackReg） -> 电磁阀5 -> 左侧去向（压力8为上置测点，不串接在主管道内）。
        connectPorts(m_scene, vBack1->mapToScene(ElectricValveItem::inletPortLocal()), vBackReg->mapToScene(RegulatingValveItem::outletPortLocal()), 7);
        connectPorts(m_scene, vBackReg->mapToScene(RegulatingValveItem::inletPortLocal()), v5->mapToScene(ElectricValveItem::outletPortLocal()), 8);
        // 电磁阀5左侧出口：直接向左引出到站外去向。
        {
            const QPointF start = v5->mapToScene(ElectricValveItem::inletPortLocal());
            const QPointF end(start.x() - station1InletPipeLen, start.y());
            QPainterPath path(start);
            path.lineTo(end);
            addHmiPipeWithArrow(m_scene, path, end, start, 9);
        }

        // 注：按需求不再绘制“回路 -> 蓄能器”的闭环管路。

        m_flowPipeItems.reserve(m_scene->items().size());
        const auto allSceneItems = m_scene->items();
        for (auto *it : allSceneItems)
        {
            if (!it || it->data(0).toString() != "hmi_pipe_flow")
                continue;

            if (auto *pathItem = dynamic_cast<QGraphicsPathItem *>(it))
            {
                pathItem->setVisible(true);
                m_flowPipeItems.push_back(pathItem);
            }
        }

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

    void Station1Panel::updatePipeFlowVisibility()
    {
        if (!m_scene || !m_deviceManager)
            return;

        auto relayOpen = [&](uint8_t relayIndex) -> bool {
            bool on = false;
            if (m_deviceManager->getRelayState(relayIndex, on))
                return on;
            return false;
        };

        auto regulatingValveOpen = [&](uint16_t regId) -> bool {
            const auto rv = m_deviceManager->getRegulatingValve(regId);
            return rv.openingPercent > 0.1f;
        };

        const bool v1Open = relayOpen(0);
        const bool v2Open = relayOpen(1);
        const bool testValveOpen = relayOpen(2);
        const bool vBack1Open = relayOpen(3);
        const bool v5Open = relayOpen(5);
        const bool vRegOpen = regulatingValveOpen(1);
        const bool vBackRegOpen = regulatingValveOpen(2);

        auto segmentFlowVisible = [&](int segmentId) -> bool {
            switch (segmentId)
            {
            case 1:
                return true;
            case 2:
                return v1Open;
            case 3:
                return v1Open && v2Open;
            case 4:
                return v1Open && v2Open && vRegOpen;
            case 5:
                return v1Open && v2Open && vRegOpen;
            case 6:
                return v1Open && v2Open && vRegOpen && testValveOpen;
            case 7:
                return v1Open && v2Open && vRegOpen && testValveOpen && vBack1Open;
            case 8:
                return v1Open && v2Open && vRegOpen && testValveOpen && vBack1Open && vBackRegOpen;
            case 9:
                return v1Open && v2Open && vRegOpen && testValveOpen && vBack1Open && vBackRegOpen && v5Open;
            default:
                return false;
            }
        };

        for (auto *pathItem : m_flowPipeItems)
        {
            if (!pathItem)
                continue;

            const int segmentId = pathItem->data(1).isValid() ? pathItem->data(1).toInt() : -1;
            const bool visible = segmentFlowVisible(segmentId);
            if (pathItem->isVisible() != visible)
                pathItem->setVisible(visible);
        }
    }

    /**
     * @brief 更新管道流动动画（50 ms 定时器驱动）。
     *
     * 这里只推进虚线偏移，具体显隐由 updatePipeFlowVisibility() 按阀门状态决定。
     */
    void Station1Panel::updatePipeFlowAnimation()
    {
        if (!isVisible())
            return;

        if (!m_scene)
            return;

        if (m_flowPipeItems.empty())
            return;

        m_flowDashOffset += 5.0;
        if (m_flowDashOffset < -10000.0)
            m_flowDashOffset = 0.0;

        for (auto *pathItem : m_flowPipeItems)
        {
            if (!pathItem)
                continue;

            QPen pen = pathItem->pen();
            pen.setDashOffset(m_flowDashOffset);
            pathItem->setPen(pen);
        }
    }

    /**
     * @brief 刷新场景中所有传感器/阀门/流量计图元的显示值（200 ms 定时器驱动）。
     *
     * @param force true=强制刷新（即使面板不可见），false=不可见时跳过
     *
    * 远程分支（m_stationClient && strict_remote_mode=true）：
    *   1. 遍历当前页面实际显示的 PS 编号，并按“PS n -> pressureSensorIds[n-1]”取配置值。
    *   2. 每个传感器优先从本地 DeviceManager 通过 resolveLocalPressureSensorId() 读取；
    *      本地无数据则退回远程 SensorData::pressure[resolveRemotePressureIndex()] 。
     *   3. 从远程 SensorData::flow_rate 刷新流量计。
     *   4. 从本地 DeviceManager::getRegulatingValve() 刷新电动调压阀开度。
     *
     * 本地分支（strict_remote_mode=false 或无 StationClient）：
    *   1. 遍历当前页面实际显示的 PS 编号，并按“PS n -> pressureSensorIds[n-1]”读取本地压力传感器。
     *   2. 阀门状态：data(6) 存在则读调压阀开度；data(4) 存在则由继电器状态驱动（跳过）；
     *      其余读 DeviceManager::getValve() 状态。
     *   3. 从 DeviceManager::getFlowMeter() 刷新流量计和报警状态。
     *
     * 注意：setValue() 直接传入原始 kPa 值（SensorGlyphRenderer 量表 0~100 对应 0~100 kPa）。
     */
    void Station1Panel::updateSensorValues(bool force)
    {
        if (!force && !isVisible())
            return;

        if (!m_scene)
            return;

        auto updateSensorItemById = [&](uint16_t sensorId, double pressureKpa, int decimals, const QString &auxText = QString(), const QColor &auxColor = QColor()) {
            if (sensorId >= m_pressureSensorItems.size())
                return;
            auto *sensorItem = dynamic_cast<SensorItem *>(m_pressureSensorItems[sensorId]);
            if (!sensorItem)
                return;
            sensorItem->setValue(pressureKpa);
            sensorItem->setDisplayDecimals(decimals);
            sensorItem->setAuxText(auxText, auxColor);
        };

        auto relayForPressureSensor = [](uint16_t sensorId) -> int {
            switch (sensorId)
            {
            case 3: return 0;
            case 4: return 1;
            case 5: return 1;
            case 6: return 2;
            case 7: return 3;
            case 8: return 5;
            default: return -1;
            }
        };

        auto updateRegulatingValveById = [&](uint16_t regId, double openingPercent) {
            if (regId >= m_regulatingValveItems.size())
                return;
            auto *valveItem = dynamic_cast<RegulatingValveItem *>(m_regulatingValveItems[regId]);
            if (!valveItem)
                return;
            valveItem->setOpen(openingPercent > 0.1);
            valveItem->setDegree(openingPercent);
        };

        auto updateFlowMeterItem = [&](double flowRate, const QString &unit, bool hasAlarm, int emptyPipeAlarm, int excitationAlarm) {
            auto *flowItem = dynamic_cast<FlowMeterItem *>(m_flowMeterItem);
            if (!flowItem)
                return;
            flowItem->setFlow(flowRate);
            flowItem->setUnit(unit);
            flowItem->setAlarm(hasAlarm);
            flowItem->setAlarmDetail(emptyPipeAlarm, excitationAlarm);
        };

        if (m_stationClient && m_stationClient->isConnected())
        {
            const SensorData net = m_stationClient->getLatestSensorData();
            const auto visiblePsNumbers = visiblePressureSensorNumbers(m_panelConfig.stationNumber);
            for (size_t idx = 0; idx < visiblePsNumbers.size(); ++idx)
            {
                const int psNumber = visiblePsNumbers[idx];
                const uint16_t configuredSensorId = configuredPressureSensorId(m_panelConfig, psNumber);
                if (configuredSensorId == 0)
                    continue;

                const int sensorId = static_cast<int>(configuredSensorId);
                double pressureKpa = 0.0;
                if (!readPressureValueForDisplay(configuredSensorId, idx, pressureKpa))
                    continue;

                QColor deltaColor;
                const int relayIndex = relayForPressureSensor(configuredSensorId);
                const QString deltaText = (relayIndex >= 0)
                    ? pressureCloseDeltaText(static_cast<uint8_t>(relayIndex), configuredSensorId, pressureKpa, 1, &deltaColor)
                    : QString();
                updateSensorItemById(static_cast<uint16_t>(sensorId), pressureKpa, 1, deltaText, deltaColor);
            }

            updateFlowMeterItem(static_cast<double>(net.flow_rate), QString("L/min"), false, 0, 0);

            if (m_deviceManager)
            {
                const auto topRv = m_deviceManager->getRegulatingValve(1);
                updateRegulatingValveById(1, qBound(0.0, static_cast<double>(topRv.openingPercent), 100.0));
                const auto bottomRv = m_deviceManager->getRegulatingValve(2);
                updateRegulatingValveById(2, qBound(0.0, static_cast<double>(bottomRv.openingPercent), 100.0));
            }

            updatePipeFlowVisibility();

            // 物理按钮联动依赖本地 PLC 位读取；若本地可读则沿用同一动作链路。
            pollPhysicalStartStopButtons();

            return;
        }

        if (!m_deviceManager)
            return;

        const auto visiblePsNumbers = visiblePressureSensorNumbers(m_panelConfig.stationNumber);
        for (size_t idx = 0; idx < visiblePsNumbers.size(); ++idx)
        {
            const int psNumber = visiblePsNumbers[idx];
            const uint16_t configuredSensorId = configuredPressureSensorId(m_panelConfig, psNumber);
            if (configuredSensorId == 0)
                continue;

            const int sensorId = static_cast<int>(configuredSensorId);
            double pressureKpa = 0.0;
            if (!readPressureValueForDisplay(configuredSensorId, idx, pressureKpa))
                continue;

            QColor deltaColor;
            const int relayIndex = relayForPressureSensor(configuredSensorId);
            const QString deltaText = (relayIndex >= 0)
                ? pressureCloseDeltaText(static_cast<uint8_t>(relayIndex), configuredSensorId, pressureKpa, 1, &deltaColor)
                : QString();
            updateSensorItemById(static_cast<uint16_t>(sensorId), pressureKpa, 1, deltaText, deltaColor);
        }

        const auto topRv = m_deviceManager->getRegulatingValve(1);
        updateRegulatingValveById(1, qBound(0.0, static_cast<double>(topRv.openingPercent), 100.0));
        const auto bottomRv = m_deviceManager->getRegulatingValve(2);
        updateRegulatingValveById(2, qBound(0.0, static_cast<double>(bottomRv.openingPercent), 100.0));

        updatePipeFlowVisibility();

        const auto flowMeter = m_deviceManager->getFlowMeter(m_panelConfig.flowMeterId);
        const bool hasFlowAlarm = (flowMeter.emptyPipeAlarm != 0 || flowMeter.excitationAlarm != 0);
        updateFlowMeterItem(static_cast<double>(flowMeter.flowRate),
                            (!flowMeter.unitLabel.empty() && flowMeter.unitLabel != "unknown")
                                ? QString::fromStdString(flowMeter.unitLabel)
                                : QString("L/min"),
                            hasFlowAlarm,
                            static_cast<int>(flowMeter.emptyPipeAlarm),
                            static_cast<int>(flowMeter.excitationAlarm));

        pollPhysicalStartStopButtons();
    }

} // namespace WaterTest
