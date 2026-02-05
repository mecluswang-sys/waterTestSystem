/**
 * @file ConfigManager.cpp
 * @brief Configuration Manager Implementation
 */

#include "ConfigManager.h"
#include <fstream>
#include <sstream>

namespace WaterTest
{

    ConfigManager &ConfigManager::getInstance()
    {
        static ConfigManager instance;
        return instance;
    }

    bool ConfigManager::loadConfig(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return false;
        }

        m_config.clear();
        std::string line;

        while (std::getline(file, line))
        {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#' || line[0] == ';')
            {
                continue;
            }

            // Parse key-value pair
            size_t pos = line.find('=');
            if (pos != std::string::npos)
            {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                // Remove spaces
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                m_config[key] = value;
            }
        }

        file.close();
        return true;
    }

    bool ConfigManager::saveConfig(const std::string &filename)
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            return false;
        }

        for (const auto &pair : m_config)
        {
            file << pair.first << " = " << pair.second << "\n";
        }

        file.close();
        return true;
    }

    std::string ConfigManager::getString(const std::string &key, const std::string &defaultValue) const
    {
        auto it = m_config.find(key);
        return (it != m_config.end()) ? it->second : defaultValue;
    }

    int ConfigManager::getInt(const std::string &key, int defaultValue) const
    {
        auto it = m_config.find(key);
        if (it != m_config.end())
        {
            try
            {
                return std::stoi(it->second);
            }
            catch (...)
            {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    float ConfigManager::getFloat(const std::string &key, float defaultValue) const
    {
        auto it = m_config.find(key);
        if (it != m_config.end())
        {
            try
            {
                return std::stof(it->second);
            }
            catch (...)
            {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    bool ConfigManager::getBool(const std::string &key, bool defaultValue) const
    {
        auto it = m_config.find(key);
        if (it != m_config.end())
        {
            std::string value = it->second;
            return (value == "true" || value == "1" || value == "yes");
        }
        return defaultValue;
    }

    void ConfigManager::setString(const std::string &key, const std::string &value)
    {
        m_config[key] = value;
    }

    void ConfigManager::setInt(const std::string &key, int value)
    {
        m_config[key] = std::to_string(value);
    }

    void ConfigManager::setFloat(const std::string &key, float value)
    {
        m_config[key] = std::to_string(value);
    }

    void ConfigManager::setBool(const std::string &key, bool value)
    {
        m_config[key] = value ? "true" : "false";
    }

} // namespace WaterTest
