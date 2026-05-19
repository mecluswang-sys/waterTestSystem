/**
 * @file Station3Panel.h
 * @brief 3号操作台面板实现（流程图展示）
 */

#ifndef WATERTEST_GUI_STATION3PANEL_H
#define WATERTEST_GUI_STATION3PANEL_H

#include "Station1Panel.h"

namespace WaterTest
{
    /**
     * @class Station3Panel
     * @brief 3号操作台 - 继承Station1Panel并使用不同的设备编号
     */
    class Station3Panel : public Station1Panel
    {
    public:
        explicit Station3Panel(std::shared_ptr<DeviceManager> deviceManager, QWidget *parent = nullptr);
        ~Station3Panel();

    protected:
        // 设备编号常数（3号操作台特定）
        static constexpr uint16_t STATION_ID = 3;
        static constexpr uint16_t PS_OFFSET = 12;   // PS12-15 (instead of PS4-7)
        static constexpr uint16_t FM_ID = 3;        // FM3 (instead of FM1)
        static constexpr uint16_t V1_ID = 5;        // V5 (instead of V1)
        static constexpr uint16_t V2_ID = 6;        // V6 (instead of V2)
        static constexpr uint16_t VREG_ID = 3;      // V调压3 (instead of V调压1)
        static constexpr uint8_t RELAY_V1_IDX = 6;  // 继电器索引 6 (instead of 0)
        static constexpr uint8_t RELAY_V2_IDX = 7;  // 继电器索引 7 (instead of 1)
        static constexpr uint8_t RELAY_PUMP_IDX = 10; // 继电器索引 10 (instead of 8)
        static constexpr uint8_t RELAY_VA2_IDX = 6; // 阀2动作测试 (instead of 1)
        static constexpr uint8_t RELAY_VA3_IDX = 7; // 阀3动作测试 (instead of 2)
        static constexpr uint8_t RELAY_VA4_IDX = 8; // 阀4动作测试 (instead of 3)

    private:
        void onSelfCheck() override;
    };

} // namespace WaterTest

#endif // WATERTEST_GUI_STATION3PANEL_H
