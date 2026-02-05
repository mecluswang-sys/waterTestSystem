/**
 * @file ConfigManager.h
 * @brief 配置管理器
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <map>

namespace WaterTest
{

    class ConfigManager
    {
    public:
        static ConfigManager &getInstance();

        bool loadConfig(const std::string &filename);
        bool saveConfig(const std::string &filename);

        std::string getString(const std::string &key, const std::string &defaultValue = "") const;
        int getInt(const std::string &key, int defaultValue = 0) const;
        float getFloat(const std::string &key, float defaultValue = 0.0f) const;
        bool getBool(const std::string &key, bool defaultValue = false) const;

        void setString(const std::string &key, const std::string &value);
        void setInt(const std::string &key, int value);
        void setFloat(const std::string &key, float value);
        void setBool(const std::string &key, bool value);

    private:
        ConfigManager() = default;
        std::map<std::string, std::string> m_config;
    };

} // namespace WaterTest

#endif // CONFIG_MANAGER_H
