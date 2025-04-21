#ifndef ____YLOG_H
#define ____YLOG_H

#include "Singleton.h"
#include "util_functions.h"
#include "ConfigManager.h"
#include "ThreadSafeQueue.hpp"
#include "LogXmlConfig.h"
#include "ILogAppender.h"

#include <unordered_map>
#include <utility>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <fstream>  // std::ofstream
#include <sstream>  // std::stringstream
#include <iostream> // std::cout
#include <thread>
#include <cstdarg>
#include <atomic>

#define MAKE_LOG_MESSAGE(level, content) std::make_shared<yy::Ylog::LogMessage>(level, /*yy::util::get_current_fmt_time(),*/ __FILE__, __LINE__, std::this_thread::get_id(), content)

#define GET_LOGGER(loggername) yy::Ylog::LoggerManager::getInstance().getLogger(loggername)

// #define YLOG_LEVEL(____loggername, ____level, ____format, ...)                                  \
// {                                                                                                \
//     if(GET_LOGGER(____loggername)->getLevel() <= ____level)                                       \
//     {                                                                                              \
//         char* ____buf = nullptr;                                                                    \
//         int ____len = ::asprintf(&____buf, ____format, ##__VA_ARGS__);                               \
//         if (____len != -1)                                                                            \
//         {                                                                                              \
//             GET_LOGGER(____loggername)->Log(MAKE_LOG_MESSAGE(____level, std::string(____buf, ____len)));\
//             free(____buf);                                                                               \
//         }                                                                                                 \
//     }                                                                                                      \
// }

#define YLOG_LEVEL(____loggername, ____level, ____format, ...)                               \
{                                                                                             \
    if (GET_LOGGER(____loggername)->getLevel() <= ____level)                                   \
    {                                                                                           \
        std::string ____logMessage = std::format(____format, ##__VA_ARGS__);                     \
        GET_LOGGER(____loggername)->Log(MAKE_LOG_MESSAGE(____level, ____logMessage));             \
    }                                                                                              \
}






#define YLOG_TRACE(format, ...) YLOG_LEVEL("default", yy::Ylog::LogLevel::eTRACE, format, ##__VA_ARGS__)
#define YLOG_DEBUG(format, ...) YLOG_LEVEL("default", yy::Ylog::LogLevel::eDEBUG, format, ##__VA_ARGS__)
#define YLOG_INFO(format, ...) YLOG_LEVEL("default", yy::Ylog::LogLevel::eINFO , format, ##__VA_ARGS__)
#define YLOG_WARN(format, ...) YLOG_LEVEL("default", yy::Ylog::LogLevel::eWARN , format, ##__VA_ARGS__)
#define YLOG_ERROR(format, ...) YLOG_LEVEL("default", yy::Ylog::LogLevel::eERROR, format, ##__VA_ARGS__)
#define YLOG_FATAL(format, ...) YLOG_LEVEL("default", yy::Ylog::LogLevel::eFATAL, format, ##__VA_ARGS__)

#define CLOSE_YLOG() Ylog::LoggerManager::getInstance().StopAsyncThread();

#define USE_CPP_STREAM true



//! 日志系统需要在配置系统加载后才能开始：应先调用config::ConfigManager::LoadXmlConfigs()后才使用日志系统
namespace yy::Ylog {

class LogLevel
{
public:
    // 级别的值越低，能输出的信息就越多；
    enum Level : int8_t
    {
        eUNKNOWN = 0,
        eTRACE = 1,
        eDEBUG = 2,
        eINFO = 3,
        eWARN = 4,
        eERROR = 5,
        eFATAL = 6
    };

    LogLevel(const LogLevel& log_level) = default;
    LogLevel(const LogLevel::Level& level) : m_level(level) {} // NOLINT(google-explicit-constructor)

    explicit LogLevel(const std::string& level_str) : LogLevel(FromString(level_str)) {}

    LogLevel& operator=(const LogLevel&) = default;

    LogLevel& operator=(const LogLevel::Level& level)
    {
        m_level = level;
        return *this;
    }

    bool operator==(const LogLevel& log_level) { return m_level == log_level.m_level; }
    bool operator==(const LogLevel::Level& level) { return m_level == level; }
    bool operator!=(const LogLevel& log_level) { return m_level != log_level.m_level; }
    bool operator!=(const LogLevel::Level& level) { return m_level != level; }
    bool operator> (const LogLevel& log_level) { return m_level > log_level.m_level; }
    bool operator> (const LogLevel::Level& level) { return m_level > level; }
    bool operator< (const LogLevel& log_level) { return m_level < log_level.m_level; }
    bool operator< (const LogLevel::Level& level) { return m_level < level; }
    bool operator>=(const LogLevel& log_level) { return m_level >= log_level.m_level; }
    bool operator>=(const LogLevel::Level& level) { return m_level >= level; }
    bool operator<=(const LogLevel& log_level) { return m_level <= log_level.m_level; }
    bool operator<=(const LogLevel::Level& level) { return m_level <= level; }

    std::string ToString();

    static LogLevel FromString(const std::string& level_str);

private:
    LogLevel::Level m_level;
};


// 日志消息：包含了一条日志的每个部分的信息
class LogMessage
{
public:
    using ptr = std::shared_ptr<LogMessage>;

public:
    LogMessage(
        LogLevel        level   , /*std::string time,*/
        std::string     filepath, uint32_t    line,
        std::thread::id theradID, std::string content
    ) : m_level(level), /*m_time(std::move(time)),*/
        m_filepath(std::move(filepath)), m_fileline(line),
        m_threadID(theradID), m_content(std::move(content)) {}

    ~LogMessage() = default;

    [[nodiscard]] LogLevel getLevel() const { return m_level; }

    // [[nodiscard]] const std::string& getTime() const { return m_time; }

    [[nodiscard]] const std::string& getFilepath() const { return m_filepath; }

    [[nodiscard]] uint32_t getFileline() const { return m_fileline; }

    [[nodiscard]] std::thread::id getTheradID() const { return m_threadID; }

    [[nodiscard]] const std::string& getContent() const { return m_content; }

private:
    LogLevel        m_level;    // 日志级别
    // std::string     m_time;     // 产生日志信息的时间
    std::string     m_filepath; // 产生日志信息的文件名
    uint32_t        m_fileline; // 产生日志信息的代码所在行号
    std::thread::id m_threadID; // 线程号
    std::string     m_content;  // 具体内容
};



class LogFormatter
{
public:
    using ptr = std::shared_ptr<LogFormatter>;
    class IFormatItem
    {
    public:
        using ptr = std::shared_ptr<IFormatItem>;

        virtual void format(std::ostream & out, const LogMessage::ptr& msg) = 0;
    };

    ///@param format_pattern 自定义日志格式
    explicit LogFormatter(std::string format_pattern)
        : m_format_pattern(std::move(format_pattern)) { init(); }

    [[nodiscard]] auto GetFormatPattern() const { return m_format_pattern; }

    /// @brief 配置文件中的日志格式能够指定时间项的格式
    void SetTimeFormat(const std::string&  timeFmtPattern = "%Y-%m-%d %H:%M:%S.",  bool need_us = true);

    std::string format(const LogMessage::ptr& msg)
    {
        // 遍历每一项，将其转换为最终被输出的字符串
        std::stringstream ss;
        for(const auto & item: m_format_items) {
            item->format(ss, msg);
        }
        return ss.str();
    }
private:
    void init();

private:
    std::string                  m_format_pattern; // 支持自定义日志格式
    std::vector<IFormatItem::ptr> m_format_items;   // 解析pattern后，格式化后的日志格式项
};


class Logger
{
public:
    using ptr = std::shared_ptr<Logger>;
    friend class LoggerManager;

public:
    Logger(const std::string& name, LogLevel level, bool isAsync);

    ~Logger() = default;

    std::string getName() const { return m_name; }

    LogLevel getLevel() const { return m_level; }

    /// @brief 对appender列表内的所有appender执行log
    /// @param msg 一条日志信息，在该函数中可能被输出到不同的地方(file、stdout)
    void Log(const LogMessage::ptr& msg);

    /// @brief 向日志器添加一个日志添加器
    void addAppender(const std::shared_ptr<ILogAppender>& appender);

    /// @brief 从日志器删除一个日志添加器
    void delAppender(const std::shared_ptr<ILogAppender>& appender);

    void clearAppenders();

    /// @brief 支持运行时改变日志器的级别
    void setLevel(LogLevel level) { m_level = level; }

private:
    /// @brief 同步写日志，直接将日志写到文件/标准输出中
    void LogSynch(const LogMessage::ptr& msg);

    /// @brief 异步写日志，其实只是将日志信息push到blockqueue中
    void LogAsync(const LogMessage::ptr& msg);

private:
    std::string                   m_name;      // 日志器名称
    LogLevel                      m_level;     // 日志器级别
    std::vector<std::shared_ptr<ILogAppender>> m_appenders; // 日志添加器
    mutable std::mutex            m_mutex;     // 管理appenders的互斥锁

    /* 异步 */
    bool m_isAsync;    // 是否使用异步
};


/// @brief 日志器不止一个，可由配置文件设置，每一个都可以有自己的名字，
///        由一个单例类LoggerManager来进行管理所有的日志器
class LoggerManager : public Singleton<LoggerManager>
{
    SINGLETON_NECESSITY(LoggerManager)
    friend void Logger::LogAsync(const LogMessage::ptr&);
public:
    /// @brief 读取保存在LogXmlConfig单例对象中的配置信息
    void ReadConfigs();

    /// @brief 获取default日志器，
    /// default日志器为「异步」日志器，输出至「文件」(文件位于"./年-月-日_serverlog.log")和「标准输出」
    Logger::ptr getDefaultLogger();

    /// @brief 获取指定名称的日志器，若不存在则按照default日志器的规格创建一个名为name的日志器
    Logger::ptr getLogger(const std::string& name = "default");

    bool delLogger(const std::string& name)
    {
        auto ret = m_loggers.erase(name);
        return ret != 0;
    }

private:
    LoggerManager();

    //FIXED: 如何让LoggerManager最后析构（在最后一个写日志操作结束后结束），
    //       尤其是在~LinuxServer()执行时或执行后结束？而不是在其之前结束！
    //? 似乎可以使用饿汉单例模式使得LoggerManager在最开始就被构造，从而使得在最后才析构
    //? 如果不使用饿汉式的话，可以在主函数一开始时就写一条日志
    //?（隐式调用LoggerManager::getInstance()从而初始化局部静态对象）
    ~LoggerManager() override;

    /// @brief 异步写日志线程：不断地等待blockqueue中的日志信息，取出并写到文件/标准输出中
    void AsyncLogFlushThread();

    /// @brief 开启异步写日志线程
    void StartAsyncThread();

    // 关闭异步写日志线程：等待异步写线程将日志信息队列都读空并写到文件中
    void StopAsyncThread();

    static void addListener();

private:
    std::unordered_map<std::string, Logger::ptr> m_loggers;
    std::mutex m_mutex;

    /* 所有日志器共用一个阻塞队列，并用m_isRun控制异步写日志线程的运行 */
    yy::util::ThreadSafeQueue<std::pair<std::shared_ptr<ILogAppender>, LogMessage::ptr>> m_blockqueue;
    // 某线程因遇到错误结束程序，为使得detach的线程也能够关闭，故使用原子变量isRun进行同步
    std::atomic<bool> m_isRunning;
    std::thread       m_async_thread;
};



// template <typename... Args>
// void Test(std::string ____loggername, LogLevel ____level, std::string ____format, Args ... args)
// {
//     if (GET_LOGGER(____loggername)->getLevel() <= ____level)
//     {
//         std::string _logMessage = std::format(____format, args...);
//         GET_LOGGER(____loggername)->Log(MAKE_LOG_MESSAGE(____level, _logMessage));
//     }
// }





} // namespace yy::Ylog



#endif // !____YLOG_H

