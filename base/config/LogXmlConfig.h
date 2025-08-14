#pragma once

#include "ConfigManager.h"


namespace yy::config
{

struct LogXmlConfig
{
    struct Logger
    {
        struct Appender
        {
            enum class Type : int8_t
            {
                STDOUT, // 对应 StdoutLogAppender
                FILE    // 对应 FileLogAppender
            };

            [[nodiscard]] std::string TypetoString() const;

            static Type StringtoType(std::string type_str);

            Appender(const std::string& type_str, std::string path, std::string _logformat,
                     std::string timeFmtPattern,  bool need_us)
                    : m_type(StringtoType(type_str)),
                      m_filepath(std::move(path)), m_format(std::move(_logformat)),
                      m_time_format(std::move(timeFmtPattern)), m_time_use_us(need_us)
            {}

            bool operator==(const Appender& other) const {
                return m_type == other.m_type &&
                       m_filepath == other.m_filepath &&
                       m_format == other.m_format &&
                       m_time_format == other.m_time_format &&
                       m_time_use_us == other.m_time_use_us;
            }

            /* data */
            Type           m_type;
            std::string    m_filepath;
            std::string    m_format;
            std::string    m_time_format;
            bool           m_time_use_us;
        };

        Logger(std::string name, std::string level, bool is_async, const std::vector<Appender> & appenders)
                : m_name(std::move(name)), m_level(std::move(level)),
                m_is_async(is_async), m_appenders(appenders)
        {
            std::transform(m_level.begin(), m_level.end(), m_level.begin(), ::tolower);
        }

        bool operator==(const Logger& other) const {
            return m_name == other.m_name
                   && m_level == other.m_level
                   && other.m_appenders == m_appenders;
        }

        /* data */
        std::string           m_name;     // 日志器名称
        std::string           m_level;    // 日志器级别
        bool                  m_is_async; // 是否启用异步
        std::vector<Appender> m_appenders;
    };

    bool operator==(const LogXmlConfig& other) const {
        return m_loggers == other.m_loggers;
    }

    /* data */
    std::vector<Logger> m_loggers;
};


extern ConfigVar<LogXmlConfig>::ptr g_log_config;



}
