/**
 * @file Station3Panel.cpp
 * @brief 3号操作台面板实现（继承Station1Panel，使用不同的设备编号）
 */

#include "gui/Station3Panel.h"
#include "DeviceManager.h"
#include "ConfigManager.h"
#include "StationClient.h"
#include "NetworkProtocol.h"
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QScrollBar>

namespace WaterTest
{
    namespace
    {
        constexpr double kKPaPerKgfCm2 = 98.0665;

        static double kPaToKgfCm2(double kpa)
        {
            return kpa / kKPaPerKgfCm2;
        }

        static QString fmtPressure(double kpa, int decimals = 1)
        {
            return QString::number(kPaToKgfCm2(kpa), 'f', decimals);
        }

        static int pressureDisplayDecimals(const PressureSensor &sensor, int fallbackDecimals = 1)
        {
            if (sensor.displayDecimals >= 0 && sensor.displayDecimals <= 6)
                return sensor.displayDecimals;
            return fallbackDecimals;
        }

        static QString fmtKPa(const PressureSensor &sensor, int fallbackDecimals = 1)
        {
            return QString::number(kPaToKgfCm2(sensor.pressure), 'f', pressureDisplayDecimals(sensor, fallbackDecimals)) + " kgf/cm^2";
        }

        static QString deviceStatusToText(DeviceStatus status)
        {
            switch (status)
            {
            case DeviceStatus::ONLINE:
                return QString::fromUtf8("在线");
            case DeviceStatus::OFFLINE:
                return QString::fromUtf8("离线");
            case DeviceStatus::FAULT:
                return QString::fromUtf8("故障");
            case DeviceStatus::MAINTENANCE:
                return QString::fromUtf8("维护");
            default:
                return QString::fromUtf8("未知");
            }
        }
    }

    Station3Panel::Station3Panel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent)
        : Station1Panel(std::move(deviceManager),
                        parent,
                        Station1Panel::PanelConfig{3, {12, 13, 14, 15}, 3})
    {
        // Station3Panel 使用相同的UI逻辑，但在onSelfCheck中使用不同的设备编号
    }

    Station3Panel::~Station3Panel() = default;

    void Station3Panel::onSelfCheck()
    {
        if (!m_deviceManager)
        {
            QMessageBox::warning(this, QString::fromUtf8("系统自检"), QString::fromUtf8("设备管理器未初始化（请先连接系统）"));
            return;
        }

        const bool pumpManualForLeakTest = ConfigManager::getInstance().getBool("selfcheck.pump_manual_for_leak_test", true);

        // ===== 创建动态进度弹窗 =====
        enum StepState { PENDING = 0, RUNNING, STEP_OK, STEP_FAIL };
        struct StepItem { QString name; StepState state; QString detail; };
        std::vector<StepItem> steps = {
            {QString::fromUtf8("数据初始化刷新"),         PENDING, QString()},
            {QString::fromUtf8("压力传感器 PS12"),        PENDING, QString()},
            {QString::fromUtf8("压力传感器 PS13"),        PENDING, QString()},
            {QString::fromUtf8("压力传感器 PS14"),        PENDING, QString()},
            {QString::fromUtf8("压力传感器 PS15"),        PENDING, QString()},
            {QString::fromUtf8("流量计 FM3"),             PENDING, QString()},
            {QString::fromUtf8("电磁阀5 (Q0.6)"),         PENDING, QString()},
            {QString::fromUtf8("电磁阀6 (Q0.7)"),         PENDING, QString()},
            {QString::fromUtf8("电动调压阀5"),            PENDING, QString()},
            {QString::fromUtf8("步骤1：电磁阀5控制测试"), PENDING, QString()},
            {QString::fromUtf8("步骤2：气泵/建压准备"),   PENDING, QString()},
            {QString::fromUtf8("步骤3：压力观察 (PS13)"), PENDING, QString()},
            {QString::fromUtf8("步骤4：泄漏结论"),        PENDING, QString()},
            {QString::fromUtf8("电磁阀6 动作测试"),       PENDING, QString()},
            {QString::fromUtf8("电磁阀7 动作测试"),       PENDING, QString()},
            {QString::fromUtf8("电磁阀8 动作测试"),       PENDING, QString()},
        };
        const int IDX_REFRESH = 0, IDX_PS12 = 1, IDX_PS13 = 2, IDX_PS14 = 3, IDX_PS15 = 4;
        const int IDX_FM3 = 5, IDX_V5 = 6, IDX_V6 = 7, IDX_VREG = 8;
        const int IDX_STEP1 = 9, IDX_STEP2 = 10, IDX_STEP3 = 11, IDX_STEP4 = 12;
        const int IDX_VA6 = 13, IDX_VA7 = 14, IDX_VA8 = 15;

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
                "<h3 style='margin:0 0 8px 0'>") + QString::fromUtf8("3号操作台系统自检") +
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

        if (m_selfCheckBtn)
            m_selfCheckBtn->setEnabled(false);

        // ===== 数据初始化刷新 =====
        setStep(IDX_REFRESH, RUNNING, QString::fromUtf8("正在刷新设备数据…"));
        const bool refreshOk = m_deviceManager->updateAllDevices();
        setStep(IDX_REFRESH, refreshOk ? STEP_OK : STEP_FAIL,
                refreshOk ? QString::fromUtf8("刷新成功") : QString::fromUtf8("刷新失败（可能影响后续结果）"));

        // ===== 传感器状态检查 =====
        auto checkPressureSensor = [&](int stepIdx, int sensorId) -> bool {
            setStep(stepIdx, RUNNING, QString::fromUtf8("检测中…"));
            const auto ps = m_deviceManager->getPressureSensor(sensorId);
            const bool ok = (ps.id != 0 && ps.status == DeviceStatus::ONLINE);
            setStep(stepIdx, ok ? STEP_OK : STEP_FAIL,
                    ok ? QString("%1, %2").arg(deviceStatusToText(ps.status)).arg(fmtKPa(ps))
                       : QString::fromUtf8("离线或未配置"));
            return ok;
        };
        const bool p12Ok = checkPressureSensor(IDX_PS12, 12);
        const bool p13Ok = checkPressureSensor(IDX_PS13, 13);
        checkPressureSensor(IDX_PS14, 14);
        checkPressureSensor(IDX_PS15, 15);

        // ===== 流量计状态检查 =====
        {
            setStep(IDX_FM3, RUNNING, QString::fromUtf8("检测中…"));
            const auto fm3 = m_deviceManager->getFlowMeter(FM_ID);
            const bool ok = (fm3.id != 0 && fm3.status == DeviceStatus::ONLINE);
            setStep(IDX_FM3, ok ? STEP_OK : STEP_FAIL,
                    ok ? QString::fromUtf8("在线, %1 m\u00b3/h").arg(QString::number(fm3.flowRate, 'f', 3))
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
            setStep(IDX_V5, RUNNING, QString::fromUtf8("检测中…"));
            const auto v5 = m_deviceManager->getValve(V1_ID);
            const bool ok = (v5.id != 0);
            setStep(IDX_V5, ok ? STEP_OK : STEP_FAIL,
                    ok ? QString::fromUtf8("配置正常, 当前状态: %1").arg(valveStatusStr(v5.status))
                       : QString::fromUtf8("未配置"));
        }
        {
            setStep(IDX_V6, RUNNING, QString::fromUtf8("检测中…"));
            const auto v6 = m_deviceManager->getValve(V2_ID);
            const bool ok = (v6.id != 0);
            setStep(IDX_V6, ok ? STEP_OK : STEP_FAIL,
                    ok ? QString::fromUtf8("配置正常, 当前状态: %1").arg(valveStatusStr(v6.status))
                       : QString::fromUtf8("未配置"));
        }
        {
            setStep(IDX_VREG, RUNNING, QString::fromUtf8("检测中…"));
            const auto vreg = m_deviceManager->getRegulatingValve(VREG_ID);
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

        // ===== 步骤1：电磁阀5控制 & 联动测试 =====
        const float linkagePressureThr = ConfigManager::getInstance().getFloat("selfcheck.valve2_linkage_min_delta_kpa", 10.0f);
        const int linkageWaitMs = ConfigManager::getInstance().getInt("selfcheck.valve2_linkage_wait_ms", 3000);
        bool v5WasOn = false;
        const bool v5StateOk = m_deviceManager->getRelayState(RELAY_V1_IDX, v5WasOn);
        if (p12Ok && v5StateOk) {
            setStep(IDX_STEP1, RUNNING, QString::fromUtf8("正在打开电磁阀5…"));
            const float ps12Before = m_deviceManager->getPressureSensor(12).pressure;
            if (controlRelay(RELAY_V1_IDX, true)) {
                setStep(IDX_STEP1, RUNNING, QString::fromUtf8("电磁阀5已开，等待 %1 ms 观察 PS12 变化…").arg(linkageWaitMs));
                waitMs(linkageWaitMs);
                m_deviceManager->updateAllDevices();
                const float ps12After = m_deviceManager->getPressureSensor(12).pressure;
                const float delta = std::abs(ps12After - ps12Before);
                const bool linkageOk = (delta >= linkagePressureThr);
                setStep(IDX_STEP1, STEP_OK,
                        linkageOk
                                ? QString::fromUtf8("通路正常（PS12 变化 %1 kgf/cm^2 \u2265 阈值 %2 kgf/cm^2）")
                                    .arg(fmtPressure(delta, 2)).arg(fmtPressure(linkagePressureThr, 2))
                                : QString::fromUtf8("通路可能异常（PS12 变化 %1 kgf/cm^2，阈值 %2 kgf/cm^2）")
                                    .arg(fmtPressure(delta, 2)).arg(fmtPressure(linkagePressureThr, 2)));
            } else {
                setStep(IDX_STEP1, STEP_FAIL, QString::fromUtf8("电磁阀5控制失败"));
            }
            (void)controlRelay(RELAY_V1_IDX, v5WasOn);
        } else {
            setStep(IDX_STEP1, STEP_FAIL, QString::fromUtf8("压力传感器12离线或电磁阀5状态读取失败，已跳过"));
        }

        // ===== 步骤2：气泵/建压准备 =====
        const int leakBuildWaitMs = pumpManualForLeakTest
            ? 3000
            : ConfigManager::getInstance().getInt("selfcheck.valve2_leak_build_wait_ms", 2500);
        const int leakHoldWaitMs  = ConfigManager::getInstance().getInt("selfcheck.valve2_leak_hold_ms", 3500);
        const float leakBuildMinKpa  = ConfigManager::getInstance().getFloat("selfcheck.valve2_leak_build_min_kpa", 50.0f);
        const float leakP12DropMaxKpa = ConfigManager::getInstance().getFloat("selfcheck.valve2_leak_max_p4_drop_kpa", 12.0f);
        const float leakP13RiseMaxKpa = ConfigManager::getInstance().getFloat("selfcheck.valve2_leak_max_p5_rise_kpa", 5.0f);
        bool v6WasOn = false;
        bool pump3WasOn = false;
        const bool v6StateOk   = m_deviceManager->getRelayState(RELAY_V2_IDX, v6WasOn);
        const bool pumpStateOk = m_deviceManager->getRelayState(RELAY_PUMP_IDX, pump3WasOn);

        setStep(IDX_STEP2, RUNNING, QString::fromUtf8("检查气泵状态…"));
        bool canDoLeakTest = false;
        if (!(p12Ok && p13Ok)) {
            setStep(IDX_STEP2, STEP_FAIL, QString::fromUtf8("压力传感器12/13离线，无法执行泄漏判定"));
        } else if (!(v5StateOk && v6StateOk && pumpStateOk)) {
            setStep(IDX_STEP2, STEP_FAIL, QString::fromUtf8("阀门或泵状态读取失败，无法安全执行"));
        } else {
            setStep(IDX_STEP2, STEP_OK,
                    pumpManualForLeakTest ? QString::fromUtf8("手动泵模式：跳过提醒，继续检测") : QString::fromUtf8("自动泵控制链路正常"));
            canDoLeakTest = true;
        }

        // ===== 步骤3 & 4：压力观察 + 泄漏结论 =====
        if (canDoLeakTest) {
            bool prepOk = controlRelay(RELAY_V2_IDX, false) && controlRelay(RELAY_V1_IDX, true);
            if (!prepOk) {
                setStep(IDX_STEP3, STEP_FAIL, QString::fromUtf8("前置阀门切换失败，已中止"));
                setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("前置失败，未形成结论"));
            } else {
                bool pumpStartedBySelfCheck = false;
                bool skipLeakResult = false;
                if (!pumpManualForLeakTest) {
                    setStep(IDX_STEP3, RUNNING, QString::fromUtf8("正在启动气泵建压…"));
                    if (!controlPump(2, true)) {
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
                    const auto p12Build = m_deviceManager->getPressureSensor(12);
                    const auto p13Build = m_deviceManager->getPressureSensor(13);
                    const bool buildOk = (p12Build.pressure >= leakBuildMinKpa);
                    if (pumpStartedBySelfCheck)
                        (void)controlPump(2, false);
                    if (!buildOk) {
                        setStep(IDX_STEP3, STEP_FAIL,
                                QString::fromUtf8("建压不足（PS12=%1 kgf/cm^2 < 最小建压 %2 kgf/cm^2）")
                                    .arg(fmtPressure(p12Build.pressure, 1)).arg(fmtPressure(leakBuildMinKpa, 1)));
                        setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("建压不足，无法有效判定泄漏"));
                    } else {
                        setStep(IDX_STEP3, RUNNING,
                                QString::fromUtf8("PS12=%1 kgf/cm^2，保压 %2 ms 中…")
                                    .arg(fmtPressure(p12Build.pressure, 1)).arg(leakHoldWaitMs));
                        waitMs(leakHoldWaitMs);
                        m_deviceManager->updateAllDevices();
                        const auto p12Hold = m_deviceManager->getPressureSensor(12);
                        const auto p13Hold = m_deviceManager->getPressureSensor(13);
                        const float p12Drop = p12Build.pressure - p12Hold.pressure;
                        const float p13Rise = p13Hold.pressure - p13Build.pressure;
                        setStep(IDX_STEP3, STEP_OK,
                                QString::fromUtf8("已采集：PS12 %1\u2192%2 kgf/cm^2（降 %3），PS13 %4\u2192%5 kgf/cm^2（升 %6）")
                                    .arg(fmtPressure(p12Build.pressure, 2))
                                    .arg(fmtPressure(p12Hold.pressure, 2))
                                    .arg(fmtPressure(p12Drop, 2))
                                    .arg(fmtPressure(p13Build.pressure, 2))
                                    .arg(fmtPressure(p13Hold.pressure, 2))
                                    .arg(fmtPressure(p13Rise, 2)));
                        setStep(IDX_STEP4, RUNNING, QString::fromUtf8("正在判定…"));
                        const bool leakOk = (p12Drop <= leakP12DropMaxKpa) && (p13Rise <= leakP13RiseMaxKpa);
                        setStep(IDX_STEP4, leakOk ? STEP_OK : STEP_FAIL,
                                leakOk
                                    ? QString::fromUtf8("密封正常（PS12压降 %1 kgf/cm^2 \u2264 %2，PS13上升 %3 kgf/cm^2 \u2264 %4）")
                                        .arg(fmtPressure(p12Drop, 2)).arg(fmtPressure(leakP12DropMaxKpa, 2))
                                        .arg(fmtPressure(p13Rise, 2)).arg(fmtPressure(leakP13RiseMaxKpa, 2))
                                    : QString::fromUtf8("疑似泄漏（PS12压降 %1 kgf/cm^2 阈值 %2，PS13上升 %3 kgf/cm^2 阈值 %4）")
                                        .arg(fmtPressure(p12Drop, 2)).arg(fmtPressure(leakP12DropMaxKpa, 2))
                                        .arg(fmtPressure(p13Rise, 2)).arg(fmtPressure(leakP13RiseMaxKpa, 2)));
                    }
                }
            }
            if (!pumpManualForLeakTest)
                (void)controlPump(2, pump3WasOn);
            (void)controlRelay(RELAY_V1_IDX, v5WasOn);
            (void)controlRelay(RELAY_V2_IDX, v6WasOn);
        } else {
            setStep(IDX_STEP3, STEP_FAIL, QString::fromUtf8("前置条件未满足，已跳过"));
            setStep(IDX_STEP4, STEP_FAIL, QString::fromUtf8("前置条件未满足，已跳过"));
        }

        // ===== 电磁阀6/7/8 顺序动作测试 =====
        const int valveActionPulseMs = ConfigManager::getInstance().getInt("selfcheck.valve_action_pulse_ms", 600);
        struct ValveActionItem { uint8_t index; int stepIdx; };
        const std::array<ValveActionItem, 3> valveActionItems{{
            {RELAY_VA2_IDX, IDX_VA6}, {RELAY_VA3_IDX, IDX_VA7}, {RELAY_VA4_IDX, IDX_VA8}
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

        if (m_selfCheckBtn)
            m_selfCheckBtn->setEnabled(true);
        closeBtn->setText(QString::fromUtf8("自检完成，点击关闭"));
        closeBtn->setEnabled(true);
        QCoreApplication::processEvents();
    }

} // namespace WaterTest
