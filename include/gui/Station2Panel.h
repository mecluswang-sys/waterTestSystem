/**
 * @file Station2Panel.h
 * @brief 2号操作台面板实现（流程图展示）
 */

#ifndef WATERTEST_GUI_STATION2PANEL_H
#define WATERTEST_GUI_STATION2PANEL_H

#include "Station1Panel.h"

namespace WaterTest
{
    /**
     * @class Station2Panel
     * @brief 2号操作台 - 继承Station1Panel并使用不同的设备编号
     */
    class Station2Panel : public Station1Panel
    {
    public:
        explicit Station2Panel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent = nullptr);
        ~Station2Panel();

    protected:
        // 设备编号常数（2号操作台特定）
        static constexpr uint16_t STATION_ID = 2;
        static constexpr uint16_t PS_OFFSET = 8;    // PS8-11 (instead of PS4-7)
        static constexpr uint16_t FM_ID = 2;        // FM2 (instead of FM1)
        static constexpr uint16_t V1_ID = 3;        // V3 (instead of V1)
        static constexpr uint16_t V2_ID = 4;        // V4 (instead of V2)
        static constexpr uint16_t VREG_ID = 2;      // V调压2 (instead of V调压1)
        static constexpr uint8_t RELAY_V1_IDX = 4;  // 继电器索引 4 (instead of 0)
        static constexpr uint8_t RELAY_V2_IDX = 5;  // 继电器索引 5 (instead of 1)
        static constexpr uint8_t RELAY_PUMP_IDX = 9; // 继电器索引 9 (instead of 8)
        static constexpr uint8_t RELAY_VA2_IDX = 4; // 阀2动作测试 (instead of 1)
        static constexpr uint8_t RELAY_VA3_IDX = 5; // 阀3动作测试 (instead of 2)
        static constexpr uint8_t RELAY_VA4_IDX = 6; // 阀4动作测试 (instead of 3)

    private:
        void onSelfCheck() override;
    };

} // namespace WaterTest

#endif // WATERTEST_GUI_STATION2PANEL_H
