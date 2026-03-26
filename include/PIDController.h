/**
 * @file PIDController.h
 * @brief 增量式 PID 控制器（带积分限幅与输出限幅）
 *
 * 用于电动调节阀压力闭环控制：
 *   压力传感器反馈 → PID → AO 输出开度 (0-100%) → 阀门
 *
 * 接线图 AOX-Q-005~010 / SRCU1TA：
 *   AO 命令  → 端子 10-11（4-20mA，对应 Siemens 5530-27648）
 *   AI 反馈  ← 端子 16-17（4-20mA，位置反馈）
 */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

namespace WaterTest
{

    class PIDController
    {
    public:
        /**
         * @param kp           比例增益
         * @param ki           积分增益
         * @param kd           微分增益
         * @param outputMin    输出下限（默认 0.0，对应阀门关闭）
         * @param outputMax    输出上限（默认 100.0，对应阀门全开）
         * @param integralLimit 积分累积绝对值上限（防积分饱和）
         */
        PIDController(double kp = 0.5, double ki = 0.01, double kd = 0.1,
                      double outputMin = 0.0, double outputMax = 100.0,
                      double integralLimit = 1000.0)
            : m_kp(kp), m_ki(ki), m_kd(kd),
              m_outputMin(outputMin), m_outputMax(outputMax),
              m_integralLimit(integralLimit),
              m_prevError(0.0), m_integral(0.0)
        {
        }

        /**
         * @brief 执行一次 PID 计算
         * @param setpoint  目标值（压力 kPa 或其他工程量）
         * @param actual    当前反馈值
         * @return          输出量（0-100%）
         */
        double compute(double setpoint, double actual)
        {
            const double error = setpoint - actual;

            m_integral += error;
            if (m_integral > m_integralLimit)
                m_integral = m_integralLimit;
            if (m_integral < -m_integralLimit)
                m_integral = -m_integralLimit;

            const double derivative = error - m_prevError;
            m_prevError = error;

            double output = m_kp * error + m_ki * m_integral + m_kd * derivative;

            if (output > m_outputMax)
                output = m_outputMax;
            if (output < m_outputMin)
                output = m_outputMin;

            return output;
        }

        /// 重置积分项与微分项（切换模式或急停时调用）
        void reset()
        {
            m_prevError = 0.0;
            m_integral  = 0.0;
        }

        void setGains(double kp, double ki, double kd)
        {
            m_kp = kp;
            m_ki = ki;
            m_kd = kd;
        }

        void setOutputLimits(double min, double max)
        {
            m_outputMin = min;
            m_outputMax = max;
        }

        double getKp() const { return m_kp; }
        double getKi() const { return m_ki; }
        double getKd() const { return m_kd; }

    private:
        double m_kp, m_ki, m_kd;
        double m_outputMin, m_outputMax, m_integralLimit;
        double m_prevError;
        double m_integral;
    };

    // ===== Siemens S7 模拟量转换辅助函数 =====
    // 接线图: AO 命令 4-20mA, AI 反馈 4-20mA
    // Siemens S7-1200 AO 工程量范围（4-20mA 模式）:
    //   4mA  → 5530   (阀门 0% 开度)
    //   20mA → 27648  (阀门 100% 开度)

    /**
     * @brief 开度百分比 (0-100%) → Siemens AO 原始值 (5530-27648)
     */
    inline int percentToAO(float percent)
    {
        if (percent < 0.0f)
            percent = 0.0f;
        if (percent > 100.0f)
            percent = 100.0f;
        return static_cast<int>(percent / 100.0f * 22118.0f + 5530.0f);
    }

    /**
     * @brief Siemens AI 原始值 → 开度百分比 (0-100%)
     * 反馈通道 4-20mA 与命令同源，同样用 5530-27648 范围解析。
     */
    inline float aiToPercent(int value)
    {
        const float v = static_cast<float>(value - 5530);
        const float percent = v / 22118.0f * 100.0f;
        if (percent < 0.0f)
            return 0.0f;
        if (percent > 100.0f)
            return 100.0f;
        return percent;
    }

    /**
     * @brief Siemens AI 原始值 → 压力工程量 (kPa)
     * 4-20mA 线性映射：5530 → minPressure，27648 → maxPressure。
     * @param rawValue   AI 通道原始整数值 (5530-27648)
     * @param minPressure 对应 4mA 的压力下限 (kPa)
     * @param maxPressure 对应 20mA 的压力上限 (kPa)
     */
    inline float aiToPressure(int rawValue, float minPressure, float maxPressure)
    {
        const float pct = aiToPercent(rawValue);                         // 0-100%
        return minPressure + pct / 100.0f * (maxPressure - minPressure); // 线性映射
    }

} // namespace WaterTest

#endif // PID_CONTROLLER_H
