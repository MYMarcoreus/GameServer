#pragma once
#include <memory>
#include <sstream>
#include <vector>

namespace yy::Ylog
{

class LogMessage;

class LogFormatter
{
public:
    using ptr = std::shared_ptr<LogFormatter>;
    class IFormatItem
    {
    public:
        virtual ~IFormatItem() = default;
        using ptr = std::shared_ptr<IFormatItem>;

        virtual void format(std::ostream & out, const std::shared_ptr<LogMessage> & msg) = 0;
    };

    ///@param format_pattern 自定义日志格式
    explicit LogFormatter(std::string format_pattern)
        : m_format_pattern(std::move(format_pattern)) { init(); }

    [[nodiscard]] auto GetFormatPattern() const { return m_format_pattern; }

    /// @brief 配置文件中的日志格式能够指定时间项的格式
    void SetTimeFormat(const std::string&  timeFmtPattern = "%Y-%m-%d %H:%M:%S.",  bool need_us = true);

    std::string format(const std::shared_ptr<LogMessage> & msg) const;

private:
    void init();

private:
    std::string                   m_format_pattern; // 支持自定义日志格式
    std::vector<IFormatItem::ptr> m_format_items;   // 解析pattern后，格式化后的日志格式项
};

}

