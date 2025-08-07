#include "log.h"
#include "util_functions.h"
#include "FileLogAppender.h"
#include "StdoutLogApeender.h"
#include "ILogAppender.h"
#include "LogXmlConfig.h"

#ifdef ____WINDOWS
#include <io.h>
#else
#include <unistd.h>
#include <sys/fcntl.h>
#endif

#include <utility>
#include <cassert>
#include <algorithm>
#include <chrono>
#include <iostream>
using namespace std::chrono_literals;


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






/******************************* Logger *******************************/
Logger::Logger(const std::string& name, LogLevel level, bool isAsync) //NOLINT
        : m_name(name), m_level(level), m_isStop{false}, m_isAsync(isAsync)
{ }


void Logger::addAppender(const ILogAppender::ptr& appender)
{
    assert(appender != nullptr);

    std::lock_guard lg{m_appenderMutex};
    m_appenders.push_back(appender);
}

void Logger::delAppender(const ILogAppender::ptr& appender)
{
    assert(appender != nullptr);

    std::lock_guard lg{m_appenderMutex};
    for (auto it = m_appenders.begin(); it != m_appenders.end() ; ++it) {
        // 智能指针的==运算符比较的是内部指针指向的地址
        if (*it == appender) {
            m_appenders.erase(it);
        }
    }
}

void Logger::clearAppenders() {
    std::lock_guard lg{m_appenderMutex};

    for(const auto& appender: m_appenders)
    {
        delAppender(appender);
    }
}

void Logger::Log(const LogMessage::ptr& msg) const
{
    assert(msg != nullptr);

    // TICK_START()
    if (m_isStop)
        return;

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

    //! FATAL日志，立即写，然后结束进程
    if(msg->getLevel() == LogLevel::eFATAL) {
        LogSynch(msg);
        std::terminate();
    }


    for (const auto& appender: m_appenders) {
        // push到阻塞队列，进行异步写
        //! 注意异步操作时，智能指针一定得拷贝，不能是智能指针的const &，否则调用者可能已释放智能指针！
        // LoggerManager::Instance().m_blockqueue.push(std::pair{appender, msg});

        //! 多生产者：格式化消息并将其放入缓冲区
        appender->AppendBuffer(msg);
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
    std::shared_lock r_lock{ m_loggerMutex };
    auto it = m_loggers.find(name);
    if (it == m_loggers.end()) {
        return getDefaultLogger();
    }
    return it->second;
}

bool LoggerManager::delLogger(const std::string& name)
{
    std::unique_lock w_lock{ m_loggerMutex };
    auto ret = m_loggers.erase(name);
    return ret != 0;
}


void LoggerManager::ReadConfigs()
{
    if(!config::ConfigManager::GetIsLoaded()) {
        std::cerr << std::format("日志配置项未加载，请先加载日志配置项！\n");
        std::terminate();
    }

    if (m_isConfigLoad)
        return;

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
                    std::cerr << std::format("预料之外的Appender类型！\n");
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

    m_isConfigLoad = true;
    m_isConfigLoad.notify_one();
}

LoggerManager::LoggerManager(): m_isRunning{false} {
    ReadConfigs();
    if(not m_isRunning)
        StartAsyncThread();

    // addListener();
}

LoggerManager::~LoggerManager()
{
    if(m_isRunning)
        StopAsyncThread();
}


void LoggerManager::StartAsyncThread()
{
    m_async_thread = std::thread( [this](){
        std::cout << std::format("异步写线程开启！\n");
        AsyncLogFlushThread();
        std::cout << std::format("异步写线程结束！\n");
    } );

    // 等待异步写线程启动完毕才能返回给使用者使用
    m_async_thread.detach();
    m_isRunning.wait(false);
}

void LoggerManager::AsyncLogFlushThread()
{
    // 如果值仍为false，则继续阻塞
    m_isConfigLoad.wait(false);

    m_isRunning = true;
    m_isRunning.notify_all();
    while (m_isRunning)
    {
        // 等待队列中有元素被push，然后将元素pop至msg中返回
        // std::pair<std::shared_ptr<LogAppender>, LogMessage::ptr> p;
        // decltype(m_blockqueue)::value_type p;
        // m_blockqueue.wait_pop(p);

        //! logger和appender理论上需要加锁以防止增删，但实际增删几乎不会发生
        for (auto& logger : m_loggers | std::views::values) {
            for (auto & appender : logger->m_appenders) {
                appender->FlushBuffer();
            }
        }
    }
}

void LoggerManager::StopAsyncThread()
{
    if(m_isRunning) {
        // 阻止使用者写入新的日志
        for (auto& val : m_loggers | std::views::values) {
            if (const auto & logger = val) {
                logger->Stop();
            }
        }

        // 等待将已有日志写完
        // while(!m_blockqueue.empty()) { /* spinning */ }

        // 发出信号让异步写线程结束
        m_isRunning = false;
        // m_blockqueue.push(std::pair{nullptr, nullptr});
    }
}




// struct LogListener
// {
//     LogListener()
//     {
//         // 更改new_val的内部数据，将
//         config::g_log_config->AddListener(
//                 [](const config::LogXmlConfig &old_config_var, const config::LogXmlConfig &new_config_var) {
//                     for (auto &&new_logger_var: new_config_var.m_loggers) {
//                         // const引用无法更改new_val中的数据，因此根据其名字使用其它方式获得非const对象以更改
//                         // getLogger总会返回，因为若查找不到就会返回一个新建的logger
//                         auto logger = LoggerManager::Instance().getLogger(new_logger_var.m_name);
//                         if (!logger) {
//                             std::terminate();
//                         }
//
//                         // 更新
//                         logger->setLevel(LogLevel::FromString(new_logger_var.m_level));
//                         logger->clearAppenders();
//                         for (const auto &appender_var: new_logger_var.m_appenders) {
//                             ILogAppender::ptr appender;
//                             switch (appender_var.m_type) {
//                                 case config::LogXmlConfig::Logger::Appender::Type::FILE:
//                                     appender.reset(new FileLogAppender{appender_var.m_filepath, appender_var.m_format});
//                                     break;
//                                 case config::LogXmlConfig::Logger::Appender::Type::STDOUT:
//                                     appender.reset(new StdoutLogApeender{appender_var.m_format});
//                                     break;
//                             }
//                             logger->addAppender(appender);
//                         }
//
//                         // 旧的有，但新的没有，于是删除之
//                         for (auto &&old_logger_var: old_config_var.m_loggers) {
//                             if (old_logger_var != new_logger_var) {
//                                 auto del_logger = LoggerManager::Instance().getLogger(old_logger_var.m_name);
//                                 LoggerManager::Instance().delLogger(del_logger->getName());
//                             }
//                         }
//                     }
//                 });
//     }
// };
// void LoggerManager::addListener() {
//     static LogListener s_logger_listener;
// }



}

