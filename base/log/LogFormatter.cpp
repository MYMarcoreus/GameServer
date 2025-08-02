#include "LogFormatter.h"
#include "log.h"

namespace yy::Ylog
{

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
    { out << util::make_format_time(msg->getTime(), m_time_fmt_pattern, m_need_us); }

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



}
