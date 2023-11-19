#include "LogXmlConfig.h"

namespace yy::config
{




template<>
class XmlElementTo<LogXmlConfig::Logger::Appender>
{
public:
    LogXmlConfig::Logger::Appender operator()(const XMLElement * xml_appender)
    {
        // 读取appender元素的属性type、logformat、filepath(如果type是file)
        auto type = XmlAttributeTo<std::string>(xml_appender->FindAttribute("type"));
        std::string filepath{};
        if (type == "file") {
            try {
                filepath = XmlAttributeTo<std::string>(xml_appender->FindAttribute("filepath"));
            }
            catch (const std::exception & e) {
                filepath = ("./" + util::get_current_fmt_time("%Y-%m-%d_serverlog_", false)
                        + xml_appender->Parent()->ToElement()->Attribute("m_TypeName") + ".log");
                std::cerr <<e.what() << "，使用默认值" << filepath << std::endl;
            }
        }
        // 读取元素的属性format、time_format、time_use_us
        auto format        = XmlAttributeTo<std::string>(xml_appender->FindAttribute("format"));
        auto time_format   = XmlAttributeTo<std::string>(xml_appender->FindAttribute("time_format"));
        auto time_use_us     = XmlAttributeTo<bool>(xml_appender->FindAttribute("time_use_us"));

        return {type, filepath, format, time_format, time_use_us};
    }
};

template<>
class XmlElementTo<LogXmlConfig::Logger>
{
public:
    LogXmlConfig::Logger operator()(const XMLElement * xml_logger)
    {
        return {
                XmlAttributeTo<std::string>(xml_logger->FindAttribute("m_TypeName")),
                XmlAttributeTo<std::string>(xml_logger->FindAttribute("level")),
                XmlAttributeTo<bool>(xml_logger->FindAttribute("is_async")),
                XmlElementTo<decltype(LogXmlConfig::Logger::m_appenders)>{}(xml_logger)
        };
    }
};

template<>
class XmlElementTo<LogXmlConfig>
 {
 public:
     LogXmlConfig operator()(const XMLElement *xml_log)
     {
         LogXmlConfig logXmlConfig;
         logXmlConfig.load(xml_log);
         return logXmlConfig;
     }
 };



void LogXmlConfig::load(const XMLElement *xml_log)
{
    m_loggers = XmlElementTo<decltype(m_loggers)>{}(xml_log);
}






LogXmlConfig::Logger::Appender::Type LogXmlConfig::Logger::Appender::StringtoType(std::string type_str)
{
    std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::tolower);

    if (type_str == "file"  ) return LogXmlConfig::Logger::Appender::Type::FILE;
    if (type_str == "stdout") return LogXmlConfig::Logger::Appender::Type::STDOUT;
    else throw std::invalid_argument(type_str);
}

[[maybe_unused]] std::string LogXmlConfig::Logger::Appender::TypetoString() const
{
    switch (m_type) {
        case Type::STDOUT:
            return "stdout";
        case Type::FILE:
            return "file";
        default:
            return "";
    }
}



/*声明了ConfigVar<LogXmlConfig>类型的变量，所以对应的成员函数：
如fromXmlElement内的实现所需要的函数或类也要在头文件中有声明，
但是XmlElementTo<LogXmlConfig>在头文件中并无声明，只在LogXmlConfig中有定义，
所以若需要声明ConfigVar<LogXmlConfig>类型的变量，只能在LogXmlConfig.cpp中声明！*/
config::ConfigVar<config::LogXmlConfig>::ptr g_log_config
        = config::ConfigManager::LookUpOrAdd<config::LogXmlConfig>("root.log", {}, "");





}

