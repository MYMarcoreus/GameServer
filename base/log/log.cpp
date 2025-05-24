#include "log.h"
#include "util_functions.h"
#include "FileLogAppender.h"
#include "StdoutLogApeender.h"
#include "ILogAppender.h"
#include <algorithm>
#include "LogXmlConfig.h"

#ifdef ____WINDOWS
#include <io.h>
#else
#include <unistd.h>
#include <sys/fcntl.h>
#endif

#include <utility>
#include <cassert>


namespace yy::Ylog {


/******************************* LogLevel *******************************/
std::string LogLevel::ToString() const
{
    switch (m_level) {
        case eUNKNOWN: return "unknown";
        case eTRACE  : return "trace";
        case eDEBUG  : return "debug";
        case eINFO   : return "info";
        case eWARN   : return "warn";
        case eERROR  : return "error";
        case eFATAL  : return "fatal";
        default      : return ""; // should never get here
    }
}

LogLevel LogLevel::FromString(const std::string &level_str)
{
    std::string str{level_str};
    std::ranges::transform(str, str.begin(), ::tolower);

    if (str == "trace") return eTRACE;
    if (str == "debug") return eDEBUG;
    if (str == "info")  return eINFO;
    if (str == "warn")  return eWARN;
    if (str == "error") return eERROR;
    if (str == "fatal") return eFATAL;

    return eUNKNOWN;
}




/******************************* Formatter *******************************/
///@brief 格式化字符串中的普通字符
class PlainFormatItem final : public LogFormatter::IFormatItem
{
public:
    explicit PlainFormatItem(std::string  str) : m_str(std::move(str)) {}
    void format(std::ostream& out, const LogMessage::ptr & msg) override
    { out << m_str; }

private:
    std::string m_str;
};

///@brief 格式化字符串中的%l：日志级别
class LevelFormatItem final : public LogFormatter::IFormatItem
{
public:
    void format(std::ostream& out, const LogMessage::ptr & msg) override
    { out << msg->getLevel().ToString(); }
};

///@brief 格式化字符串中的%i：线程id
class ThreadIDFormatItem final : public LogFormatter::IFormatItem
{
public:
    void format(std::ostream& out, const LogMessage::ptr & msg) override
    { out << msg->getTheradID(); }
};

///@brief 格式化字符串中的%c：日志内容
class ContentFormatItem final : public LogFormatter::IFormatItem
{
public:
    void format(std::ostream& out, const LogMessage::ptr & msg) override
    { out << msg->getContent(); }
};

///@brief 格式化字符串中的%t：日志时间，以产生日志的时间为准
class TimeFormatItem final : public LogFormatter::IFormatItem
{
public:
    ///@param timeFmtPattern 自定义时间格式
    ///@param need_us
    explicit TimeFormatItem(std::string  timeFmtPattern = "%Y-%m-%d %H:%M:%S.", const bool need_us = true)
        : m_time_fmt_pattern(std::move(timeFmtPattern)), m_need_us(need_us) {}

    void format(std::ostream& out, const LogMessage::ptr & msg) override
    { out << util::get_current_fmt_time(m_time_fmt_pattern, m_need_us); }

    std::string getTimeFormat(){ return m_time_fmt_pattern; }
private:
    std::string m_time_fmt_pattern; // 自定义时间格式
    bool        m_need_us{};
};

///@brief 格式化字符串中的%f：产生日志的代码文件
class FilepathFormatItem final : public LogFormatter::IFormatItem
{
public:
    void format(std::ostream& out, const LogMessage::ptr & msg) override
    { out << msg->getFilepath(); }
};

///@brief 格式化字符串中的%L：产生日志的代码所在行数
class FilelineFormatItem final : public LogFormatter::IFormatItem
{
public:
    void format(std::ostream& out, const LogMessage::ptr & msg) override
    { out << msg->getFileline(); }
};

///@brief 格式化字符串中的%n：换行符
class NewlineFormatItem final : public LogFormatter::IFormatItem
{
public:
    void format(std::ostream& out, const LogMessage::ptr & ) override
    { out << '\n'; }
};

///@brief 格式化字符串中的%T：输出Tab
class TabFormatItem final : public LogFormatter::IFormatItem
{
public:
    void format(std::ostream& out, const LogMessage::ptr & ) override
    { out << '\t'; }
};

///@brief 格式化字符串中的%p：输出百分号%
class PercentSignFormatItem final : public LogFormatter::IFormatItem
{
public:
    void format(std::ostream& out, const LogMessage::ptr & ) override
    { out << '%'; }
};


thread_local static std::unordered_map <char, LogFormatter::IFormatItem::ptr> g_format_item_map
{
        {'l', std::make_shared<LevelFormatItem>()      }, // 日志级别
        {'i', std::make_shared<ThreadIDFormatItem>()   }, // 线程id
        {'c', std::make_shared<ContentFormatItem>()    }, // 日志内容
        {'t', std::make_shared<TimeFormatItem>()       }, // 日志时间，以产生日志的时间为准
        {'f', std::make_shared<FilepathFormatItem>()   }, // 产生日志的代码文件
        {'L', std::make_shared<FilelineFormatItem>()   }, // 产生日志的代码所在行数
        {'n', std::make_shared<NewlineFormatItem>()    }, // 换行符
        {'T', std::make_shared<TabFormatItem>()        }, // 输出Tab
        {'p', std::make_shared<PercentSignFormatItem>()}, // 输出百分号%
};


void LogFormatter::init()
{
    m_format_items.clear();


    //解析m_format_pattern，填充m_format_items，需要根据格式符g_format_item_map映射
    enum class LogFormatParseStatus {
        Normal,
        Sign
    };
    auto status = LogFormatParseStatus::Normal;

    for(size_t i = 0 ; i < m_format_pattern.size() ; ++i) {
        switch(status) {
            // 普通字符
            case LogFormatParseStatus::Normal:
            {
                size_t j;
                for(j = i ;j < m_format_pattern.size() ; ++j ) {
                    // 找到占位符，结束当前状态，需要转换至下一状态，下一轮循环i就指向占位符%后的字符
                    if(m_format_pattern[j] == '%') {
                        status = LogFormatParseStatus::Sign;
                        break;
                    }
                }
                // [i, j)都是普通字符，需要作为PlainFormatItem写入
                if(i != j) {
                    m_format_items.push_back( std::make_shared<PlainFormatItem>
                        (m_format_pattern.substr(i, j-i)) );
                    i = j;
                }

                break;
            }
            // 占位符%
            case LogFormatParseStatus::Sign:
            {
                auto it = g_format_item_map.find(m_format_pattern[i]);
                if(it == g_format_item_map.end()) {
                    m_format_items.push_back(std::make_shared<PlainFormatItem>("<error-type>"));
                } else {
                    m_format_items.push_back(it->second);
                }
                status = LogFormatParseStatus::Normal;
                break;
            }
        }
    }
}

void LogFormatter::SetTimeFormat(const std::string& timeFmtPattern, bool need_us)
{
    g_format_item_map['t'] = std::make_shared<TimeFormatItem>(timeFmtPattern, need_us);
    init();
}



/******************************* FileLogAppender *******************************/

// std::string FileLogAppender::format(const LogMessage::ptr& msg)
// {
//     assert(msg != nullptr);
//
// #if USE_CPP_STREAM
//     std::stringstream ss;
//     ss << "[" << msg->getTime() << "]"
//        << "[" << msg->getLevel().toString() << "]"
//        << "[thread:" << msg->getTheradID() << "]"
//        << "[" << msg->getFilename() << ":" << msg->getLine() << "]"
//        << msg->getContent()
//        << "\n";
//     return std::move(ss.str());
// #else
//     // 将std::thread::id类型转换为整型
//     auto _id = msg->getTheradID();
//     auto id = *(std::thread::native_handle_type*)(&_id);
//     std::string msg_str;
//     msg_str += ("[" + msg->getTime() + "]");
//     msg_str += ("[" + msg->getLevel().toString() + "]");
//     msg_str += ("[thread:" + std::to_string(id) + "]");
//     msg_str += msg->getContent();
//     msg_str += ("[" + msg->getFilename() + ":" + std::to_string(msg->getLine()) + "]");
//     msg_str += "\n";
//     return msg_str;
// #endif
// }


/******************************* Logger *******************************/
Logger::Logger(const std::string& name, LogLevel level, bool isAsync) //NOLINT
        : m_name(name), m_level(level), m_isAsync(isAsync)
{ }


void Logger::addAppender(const ILogAppender::ptr& appender)
{
    assert(appender != nullptr);

    std::lock_guard lg{m_mutex};
    m_appenders.push_back(appender);
}

void Logger::delAppender(const ILogAppender::ptr& appender)
{
    assert(appender != nullptr);

    std::lock_guard lg{m_mutex};
    for (auto it = m_appenders.begin(); it != m_appenders.end() ; ++it) {
        // 智能指针的==运算符比较的是内部指针指向的地址
        if (*it == appender) {
            m_appenders.erase(it);
        }
    }
}

void Logger::clearAppenders() {
    for(const auto& appender: m_appenders)
    {
        delAppender(appender);
    }
}

void Logger::Log(const LogMessage::ptr& msg) const
{
    assert(msg != nullptr);


    // TICK_START()

    // 日志器将过滤掉级别比它低的日志
    if (msg->getLevel() < m_level)
        return;

    // 判断是同步模式还是异步模式
    if (m_isAsync) {
        LogAsync(msg);
    } else {
        LogSynch(msg);
    }

    // TICK_END_CALC()
}

void Logger::LogAsync(const LogMessage::ptr& msg) const
{
    assert(msg != nullptr);

    for (const auto& appender: m_appenders) {
        // push到阻塞队列，进行异步写
        //! 注意异步操作时，智能指针一定得拷贝，不能是智能指针的const &，否则调用者可能已释放智能指针！
        LoggerManager::getInstance().m_blockqueue.push(std::pair{appender, msg});
    }
}

void Logger::LogSynch(const LogMessage::ptr& msg) const
{
    assert(msg != nullptr);

    // 对m_appenders只是读，故无需上锁
    for (const auto &appender: m_appenders) {
        // 直接写
        appender->WriteLog(msg);
    }

    //FIXME: 收到FATAL日志时到底该不该结束程序呢？
    if(msg->getLevel() == LogLevel::eFATAL) {
        std::terminate();
    }
}




/******************************* LoggerManager *******************************/
Logger::ptr LoggerManager::getDefaultLogger() {
    return m_loggers["default"];
}

Logger::ptr LoggerManager::getLogger(const std::string& name)
{
    std::lock_guard lg{ m_mutex };
    auto it = m_loggers.find(name);
    if (it == m_loggers.end()) {
        Logger::ptr logger{new Logger(name, getDefaultLogger()->m_level, getDefaultLogger()->m_isAsync)};
        for(const auto & appender: getDefaultLogger()->m_appenders)
        {
            logger->addAppender(appender);
        }
        m_loggers[name] = logger;
        return logger;
    }
    return it->second;
}


void LoggerManager::ReadConfigs()
{
    if(!config::ConfigManager::GetIsLoaded()) {
        std::cerr << "日志配置项未加载，请先加载日志配置项！" << std::endl;
        std::terminate();
    }

    m_loggers.clear();

    for (auto &logger: config::g_log_config->GetValue().m_loggers) {
        m_loggers[logger.m_name] = std::make_shared<Logger>(
                logger.m_name,
                LogLevel{logger.m_level},
                logger.m_is_async
        );

        for (auto &appender: logger.m_appenders) {
            auto & log_type = appender.m_type;
            auto & log_path = appender.m_filepath;
            auto & log_format = appender.m_format;

            ILogAppender::ptr logAppender = nullptr;
            switch (log_type) {
                case config::LogXmlConfig::Logger::Appender::Type::FILE:
                    logAppender = std::make_shared<FileLogAppender>(log_path, log_format);
                    break;
                case config::LogXmlConfig::Logger::Appender::Type::STDOUT:
                    logAppender = std::make_shared<StdoutLogApeender>(log_format);
                    break;
                default:
                    std::cerr << "预料之外的Appender类型！" << std::endl;
                    std::terminate();
            }

            assert(logAppender != nullptr);
            logAppender->SetTimeFormat(appender.m_time_format, appender.m_time_use_us);
            m_loggers[logger.m_name]->addAppender(logAppender );
        }
    }

    auto default_name = "default";
    if(not m_loggers.contains(default_name))
    {
        auto default_level      = LogLevel::eTRACE;
        auto default_use_us      = true;
        auto default_filepath  = ("./" + util::get_current_fmt_time("%Y-%m-%d_serverlog", false) + ".log");
        auto default_logformat = "[%t][%l][%i][%f:%L] %c%n";

        // 保证一定有一个名为"default"的logger
        m_loggers[default_name] = std::make_shared<Logger>(default_name, default_level, default_use_us);
        m_loggers[default_name]->addAppender(std::make_shared<FileLogAppender>(default_filepath, default_logformat));
    }
}

LoggerManager::LoggerManager(): m_isRunning{false} {
    ReadConfigs();
    if(!m_isRunning)
        StartAsyncThread();

    addListener();
}

LoggerManager::~LoggerManager()
{
    if(m_isRunning)
        StopAsyncThread();
}


void LoggerManager::StartAsyncThread()
{
    m_isRunning = true;
    m_async_thread = std::thread( [this](){
        std::cout << "异步写线程开启！" << std::endl;
        AsyncLogFlushThread();
        std::cout << "异步写线程结束！" << std::endl;
    } );
    m_async_thread.detach();
}

void LoggerManager::AsyncLogFlushThread()
{
    while (true)
    {
        // 等待队列中有元素被push，然后将元素pop至msg中返回
        // std::pair<std::shared_ptr<LogAppender>, LogMessage::ptr> p;
        decltype(m_blockqueue)::value_type p;
        m_blockqueue.wait_pop(p);

        // 在异步线程中进行同步写
        if(m_isRunning and p.first != nullptr) {
            assert(p.second != nullptr);

            p.first->WriteLog(p.second);

            //FIXME: 收到FATAL日志时到底该不该结束程序呢？
            if(p.second->getLevel() == LogLevel::eFATAL) {
                std::terminate();
            }
        }
        // 收到结束线程的信号
        else {
            // while(!m_blockqueue.empty()) {
            //     if(p.first != nullptr)
            //         p.first->WriteLog(p.second);
            // }

            break;
        }
    }
}

void LoggerManager::StopAsyncThread()
{
    if(m_isRunning) {
        // 等待将所有日志写完
        while(!m_blockqueue.empty()) { /* spinning */ }

        // 发出信号让异步写线程结束
        m_isRunning = false;
        m_blockqueue.push(std::pair{nullptr, nullptr});
    }
}




struct LogListener
{
    LogListener()
    {
        // 更改new_val的内部数据，将
        config::g_log_config->AddListener(
                [](const config::LogXmlConfig &old_config_var, const config::LogXmlConfig &new_config_var) {
                    for (auto &&new_logger_var: new_config_var.m_loggers) {
                        // const引用无法更改new_val中的数据，因此根据其名字使用其它方式获得非const对象以更改
                        // getLogger总会返回，因为若查找不到就会返回一个新建的logger
                        auto logger = LoggerManager::getInstance().getLogger(new_logger_var.m_name);
                        if (!logger) {
                            std::terminate();
                        }

                        // 更新
                        logger->setLevel(LogLevel::FromString(new_logger_var.m_level));
                        logger->clearAppenders();
                        for (const auto &appender_var: new_logger_var.m_appenders) {
                            ILogAppender::ptr appender;
                            switch (appender_var.m_type) {
                                case config::LogXmlConfig::Logger::Appender::Type::FILE:
                                    appender.reset(new FileLogAppender{appender_var.m_filepath, appender_var.m_format});
                                    break;
                                case config::LogXmlConfig::Logger::Appender::Type::STDOUT:
                                    appender.reset(new StdoutLogApeender{appender_var.m_format});
                                    break;
                            }
                            logger->addAppender(appender);
                        }

                        // 旧的有，但新的没有，于是删除之
                        for (auto &&old_logger_var: old_config_var.m_loggers) {
                            if (old_logger_var != new_logger_var) {
                                auto del_logger = LoggerManager::getInstance().getLogger(old_logger_var.m_name);
                                LoggerManager::getInstance().delLogger(del_logger->getName());
                            }
                        }
                    }
                });
    }
};


void LoggerManager::addListener() {
    static LogListener s_logger_listener;
}



}

